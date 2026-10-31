#include "mica.h"

#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum
{
    V_NULL,
    V_NUM,
    V_BOOL,
    V_STR,
    V_OBJ
} ValueKind;
typedef struct Obj Obj;
typedef struct Expr Expr;
typedef struct
{
    ValueKind

        kind;
    union
    {
        double n;
        int b;
        char *s;
        Obj *o;
    } as;
} Value;
typedef enum
{
    UNRESOLVED,
    RESOLVING,
    RESOLVED,
    ERROR
} NodeState;
typedef enum
{
    E_NUM,
    E_BOOL,
    E_STR,
    E_TEMPLATE,
    E_OBJ,
    E_REF,
    E_BINARY,
    E_COND,
    E_CALL
} ExprKind;
typedef enum
{
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_NEG,
    OP_NOT,
    OP_EQ,
    OP_NE,
    OP_LT,
    OP_LE,
    OP_GT,
    OP_GE,
    OP_AND,
    OP_OR
} Op;

typedef struct
{
    char *name;
    Expr *value;
} Field;
typedef struct
{
    int expression;
    char *text;
    Expr *expr;
} StringPart;
struct Obj
{
    Field *fields;
    size_t count, cap;
    Obj *parent;
    const char *source_open;
    const char *source_close;
};
struct Expr
{
    ExprKind kind;
    NodeState state;
    Value cache;
    int cache_owned;
    Obj *scope;
    const char *source_start;
    const char *source_end;
    union
    {
        double number;
        int boolean;
        char *string;
        Obj *object;
        struct
        {
            StringPart *items;
            size_t count;
        } template_;
        struct
        {
            unsigned dots;
            char **segments;
            size_t count;
        } ref;
        struct
        {
            Op op;
            Expr *left, *right;
        } binary;
        struct
        {
            Expr *condition, *yes, *no;
        } cond;
        struct
        {
            char *name;
            Expr **args;
            size_t count;
        } call;
    } as;
};
struct MicaContext
{
    Obj *root;
    char *source;
    size_t source_len;
    char error[256];
};

static void free_expr(Expr *e);
static void *xmalloc(size_t n)
{
    void *p = calloc(1, n ? n : 1);
    if (!p)
    {
        fputs("mica: out of memory\n", stderr);
        abort();
    }
    return p;
}
static char *xstrndup(const char *s, size_t n)
{
    char *r = xmalloc(n + 1);
    memcpy(r, s, n);
    return r;
}
static char *xstrdup(const char *s) { return xstrndup(s, strlen(s)); }
static Expr *new_expr(ExprKind k)
{
    Expr *e = xmalloc(sizeof *e);
    e->kind = k;
    e->state = UNRESOLVED;
    return e;
}
static Obj *new_obj(void) { return xmalloc(sizeof(Obj)); }
static void obj_add(Obj *o, char *name, Expr *e)
{
    size_t i;
    /* A later declaration is an intentional local override. */
    for (i = 0; i < o->count; i++)
    {
        if (!strcmp(o->fields[i].name, name))
        {
            free(name);
            free_expr(o->fields[i].value);
            o->fields[i].value = e;
            return;
        }
    }
    if (o->count == o->cap)
    {
        o->cap = o->cap ? o->cap * 2 : 8;
        o->fields = realloc(o->fields, o->cap * sizeof *o->fields);
        if (!o->fields)
            abort();
    }
    o->fields[o->count++] = (Field){name, e};
}

/* Lexer. Newlines are real tokens because they terminate assignments. */
typedef enum
{
    T_EOF,
    T_NL,
    T_ID,
    T_NUM,
    T_STR,
    T_RAW,
    T_EQ,
    T_LB,
    T_RB,
    T_LP,
    T_RP,
    T_DOT,
    T_PLUS,
    T_MINUS,
    T_STAR,
    T_SLASH,
    T_Q,
    T_COLON,
    T_COMMA,
    T_SEMI,
    T_BANG,
    T_EQEQ,
    T_NE,
    T_LT,
    T_LE,
    T_GT,
    T_GE,
    T_AND,
    T_OR
} TokKind;
typedef struct
{
    TokKind kind;
    const char *start;
    const char *end;
    size_t len;
    double number;
    char *text;
} Token;
typedef struct
{
    const char *src, *at;
    const char *last_end;
    Token tok;
    char error[160];
} Lexer;

static void lex_fail(Lexer *l, const char *msg)
{
    if (!l->error[0])
        snprintf(l->error, sizeof l->error, "%s", msg);
}
static void lex_next(Lexer *l)
{
    l->last_end = l->tok.end;
    free(l->tok.text);
    l->tok = (Token){0};
    while (*l->at == ' ' || *l->at == '\t' || *l->at == '\r')
        l->at++;
    if (*l->at == '/' && l->at[1] == '/')
    {
        while (*l->at && *l->at != '\n')
            l->at++;
    }
    if (*l->at == '\n')
    {
        l->tok.start = l->at;
        l->tok.kind = T_NL;
        l->at++;
        l->tok.end = l->at;
        return;
    }
    const char *s = l->at;
    char c = *l->at++;
    l->tok.start = s;
    l->tok.end = l->at;
    l->tok.len = 1;
    if (!c)
    {
        l->tok.kind = T_EOF;
        l->tok.end = s;
        return;
    }
    if (isalpha((unsigned char)c) || c == '_')
    {
        while (isalnum((unsigned char)*l->at) || *l->at == '_')
            l->at++;
        l->tok.kind = T_ID;
        l->tok.len = (size_t)(l->at - s);
        l->tok.end = l->at;
        return;
    }
    if (isdigit((unsigned char)c) || (c == '.' && isdigit((unsigned char)*l->at)))
    {
        char *end;
        l->tok.number = strtod(s, &end);
        l->at = end;
        l->tok.kind = T_NUM;
        l->tok.len = (size_t)(end - s);
        l->tok.end = l->at;
        return;
    }
    if (c == '"')
    {
        size_t cap = 32, n = 0;
        char *out = xmalloc(cap);
        while (*l->at && *l->at != '"')
        {
            char q = *l->at++;
            if (q == '\\' && *l->at)
            {
                q = *l->at++;
                if (q == 'n')
                    q = '\n';
                else if (q == 't')
                    q = '\t';
            }
            if (n + 2 > cap)
            {
                cap *= 2;
                out = realloc(out, cap);
                if (!out)
                    abort();
            }
            out[n++] = q;
        }
        if (*l->at != '"')
            lex_fail(l, "unterminated string");
        else
            l->at++;
        out[n] = 0;
        l->tok.kind = T_STR;
        l->tok.text = out;
        l->tok.end = l->at;
        return;
    }
    if (c == '\'' && l->at[0] == '\'' && l->at[1] == '\'')
    {
        l->at += 2;
        const char *begin = l->at;
        const char *end = strstr(begin, "'''");
        if (!end)
        {
            lex_fail(l, "unterminated raw string");
            end = begin + strlen(begin);
            l->at = end;
        }
        else
            l->at = end + 3;
        l->tok.kind = T_RAW;
        l->tok.text = xstrndup(begin, (size_t)(end - begin));
        l->tok.end = l->at;
        return;
    }
    if (c == '=' && *l->at == '=')
    {
        l->at++;
        l->tok.kind = T_EQEQ;
        l->tok.len = 2;
        l->tok.end = l->at;
        return;
    }
    if (c == '!' && *l->at == '=')
    {
        l->at++;
        l->tok.kind = T_NE;
        l->tok.len = 2;
        l->tok.end = l->at;
        return;
    }
    if (c == '<' && *l->at == '=')
    {
        l->at++;
        l->tok.kind = T_LE;
        l->tok.len = 2;
        l->tok.end = l->at;
        return;
    }
    if (c == '>' && *l->at == '=')
    {
        l->at++;
        l->tok.kind = T_GE;
        l->tok.len = 2;
        l->tok.end = l->at;
        return;
    }
    if (c == '&' && *l->at == '&')
    {
        l->at++;
        l->tok.kind = T_AND;
        l->tok.len = 2;
        l->tok.end = l->at;
        return;
    }
    if (c == '|' && *l->at == '|')
    {
        l->at++;
        l->tok.kind = T_OR;
        l->tok.len = 2;
        l->tok.end = l->at;
        return;
    }
    switch (c)
    {
    case '=':
        l->tok.kind = T_EQ;
        break;
    case '{':
        l->tok.kind = T_LB;
        break;
    case '}':
        l->tok.kind = T_RB;
        break;
    case '(':
        l->tok.kind = T_LP;
        break;
    case ')':
        l->tok.kind = T_RP;
        break;
    case '.':
        l->tok.kind = T_DOT;
        break;
    case '+':
        l->tok.kind = T_PLUS;
        break;
    case '-':
        l->tok.kind = T_MINUS;
        break;
    case '*':
        l->tok.kind = T_STAR;
        break;
    case '/':
        l->tok.kind = T_SLASH;
        break;
    case '?':
        l->tok.kind = T_Q;
        break;
    case ':':
        l->tok.kind = T_COLON;
        break;
    case ',':
        l->tok.kind = T_COMMA;
        break;
    case ';':
        l->tok.kind = T_SEMI;
        break;
    case '!':
        l->tok.kind = T_BANG;
        break;
    case '<':
        l->tok.kind = T_LT;
        break;
    case '>':
        l->tok.kind = T_GT;
        break;
    default:
        lex_fail(l, "unexpected character");
        l->tok.kind = T_EOF;
    }
}

typedef struct
{
    Lexer lex;
    char error[256];
} Parser;
static void parse_fail(Parser *p, const char *fmt, ...)
{
    if (p->error[0])
        return;
    va_list a;
    va_start(a, fmt);
    vsnprintf(p->error, sizeof p->error, fmt, a);
    va_end(a);
}
static int accept(Parser *p, TokKind t)
{
    if (p->lex.tok.kind != t)
        return 0;
    lex_next(&p->lex);
    return 1;
}
static int expect(Parser *p, TokKind t, const char *what)
{
    if (accept(p, t))
        return 1;
    parse_fail(p, "expected %s", what);
    return 0;
}
static Expr *parse_expr(Parser *p);
static void skip_terms(Parser *p)
{
    while (p->lex.tok.kind == T_NL || p->lex.tok.kind == T_SEMI || p->lex.tok.kind == T_COMMA)
        lex_next(&p->lex);
}
static void bind_expr(Expr *e, Obj *scope);
static void bind_obj(Obj *o, Obj *parent)
{
    size_t i;
    o->parent = parent;
    for (i = 0; i < o->count; i++)
        bind_expr(o->fields[i].value, o);
}

static Expr *parse_inline(const char *s, size_t n)
{
    Parser p = {0};
    p.lex.src = p.lex.at = xstrndup(s, n);
    lex_next(&p.lex);
    Expr *e = parse_expr(&p);
    if (!p.error[0] && p.lex.tok.kind != T_EOF)
        parse_fail(&p, "invalid interpolation expression");
    free((char *)p.lex.src);
    free(p.lex.tok.text);
    if (p.error[0])
    { /* malformed interpolation becomes a harmless literal empty expression */
        return NULL;
    }
    return e;
}
static Expr *parse_string(Parser *p, int raw)
{
    char *s = p->lex.tok.text;
    p->lex.tok.text = NULL;
    lex_next(&p->lex);
    if (raw || !strstr(s, "${"))
    {
        Expr *e = new_expr(E_STR);
        e->as.string = s;
        return e;
    }
    Expr *e = new_expr(E_TEMPLATE);
    size_t cap = 0, count = 0;
    const char *cur = s, *mark = s;
    while ((cur = strstr(cur, "${")))
    {
        const char *close = strchr(cur + 2, '}');
        if (!close)
        {
            parse_fail(p, "unterminated interpolation");
            break;
        }
        if (count == cap)
        {
            cap = cap ? cap * 2 : 4;
            e->as.template_.items = realloc(e->as.template_.items, cap * sizeof(StringPart));
            if (!e->as.template_.items)
                abort();
        }
        e->as.template_.items[count++] = (StringPart){0, xstrndup(mark, (size_t)(cur - mark)), NULL};
        Expr *sub = parse_inline(cur + 2, (size_t)(close - (cur + 2)));
        if (!sub)
            parse_fail(p, "invalid interpolation");
        if (count == cap)
        {
            cap *= 2;
            e->as.template_.items = realloc(e->as.template_.items, cap * sizeof(StringPart));
            if (!e->as.template_.items)
                abort();
        }
        e->as.template_.items[count++] = (StringPart){1, NULL, sub};
        cur = close + 1;
        mark = cur;
    }
    if (count == cap)
    {
        cap = cap ? cap * 2 : 2;
        e->as.template_.items = realloc(e->as.template_.items, cap * sizeof(StringPart));
        if (!e->as.template_.items)
            abort();
    }
    e->as.template_.items[count++] = (StringPart){0, xstrdup(mark), NULL};
    e->as.template_.count = count;
    free(s);
    return e;
}
static Expr *parse_object(Parser *p)
{
    Obj *o = new_obj();
    o->source_open = p->lex.tok.start;
    expect(p, T_LB, "'{'");
    skip_terms(p);
    while (!p->error[0] && p->lex.tok.kind != T_RB && p->lex.tok.kind != T_EOF)
    {
        if (p->lex.tok.kind != T_ID)
        {
            parse_fail(p, "expected object field name");
            break;
        }
        char *name = xstrndup(p->lex.tok.start, p->lex.tok.len);
        lex_next(&p->lex);
        expect(p, T_EQ, "'='");
        Expr *v = parse_expr(p);
        if (!v)
        {
            free(name);
            break;
        }
        obj_add(o, name, v);
        if (p->lex.tok.kind != T_RB && p->lex.tok.kind != T_EOF &&
            p->lex.tok.kind != T_NL && p->lex.tok.kind != T_SEMI &&
            p->lex.tok.kind != T_COMMA)
        {
            parse_fail(p, "expected end of field");
        }
        skip_terms(p);
    }
    if (p->lex.tok.kind == T_RB)
        o->source_close = p->lex.tok.start;
    expect(p, T_RB, "'}'");
    Expr *e = new_expr(E_OBJ);
    e->as.object = o;
    return e;
}
static Expr *parse_primary(Parser *p)
{
    Token t = p->lex.tok;
    if (t.kind == T_NUM)
    {
        Expr *e = new_expr(E_NUM);
        e->as.number = t.number;
        lex_next(&p->lex);
        return e;
    }
    if (t.kind == T_STR)
        return parse_string(p, 0);
    if (t.kind == T_RAW)
        return parse_string(p, 1);
    if (t.kind == T_LB)
        return parse_object(p);
    if (t.kind == T_LP)
    {
        lex_next(&p->lex);
        Expr *e = parse_expr(p);
        expect(p, T_RP, "')'");
        return e;
    }
    unsigned dots = 0;
    while (p->lex.tok.kind == T_DOT)
    {
        dots++;
        lex_next(&p->lex);
    }
    if (p->lex.tok.kind == T_ID)
    {
        char *id = xstrndup(p->lex.tok.start, p->lex.tok.len);
        lex_next(&p->lex);
        if (!dots && (!strcmp(id, "true") || !strcmp(id, "false")))
        {
            Expr *e = new_expr(E_BOOL);
            e->as.boolean = !strcmp(id, "true");
            free(id);
            return e;
        }
        if (!dots && p->lex.tok.kind == T_LP)
        {
            Expr *e = new_expr(E_CALL);
            e->as.call.name = id;
            lex_next(&p->lex);
            skip_terms(p);
            while (p->lex.tok.kind != T_RP && p->lex.tok.kind != T_EOF)
            {
                e->as.call.args = realloc(e->as.call.args, (e->as.call.count + 1) * sizeof(Expr *));
                e->as.call.args[e->as.call.count++] = parse_expr(p);
                if (!accept(p, T_COMMA))
                    break;
            }
            expect(p, T_RP, "')'");
            return e;
        }
        Expr *e = new_expr(E_REF);
        e->as.ref.dots = dots;
        e->as.ref.segments = xmalloc(sizeof(char *));
        e->as.ref.segments[0] = id;
        e->as.ref.count = 1;
        while (accept(p, T_DOT))
        {
            if (p->lex.tok.kind != T_ID)
            {
                parse_fail(p, "expected name after '.'");
                break;
            }
            e->as.ref.segments = realloc(e->as.ref.segments, (e->as.ref.count + 1) * sizeof(char *));
            e->as.ref.segments[e->as.ref.count++] = xstrndup(p->lex.tok.start, p->lex.tok.len);
            lex_next(&p->lex);
        }
        return e;
    }
    parse_fail(p, "expected expression");
    return NULL;
}
static Expr *parse_unary(Parser *p)
{
    if (accept(p, T_MINUS))
    {
        Expr *e = new_expr(E_BINARY);
        e->as.binary.op = OP_NEG;
        e->as.binary.left = parse_unary(p);
        return e;
    }
    if (accept(p, T_BANG))
    {
        Expr *e = new_expr(E_BINARY);
        e->as.binary.op = OP_NOT;
        e->as.binary.left = parse_unary(p);
        return e;
    }
    return parse_primary(p);
}
static Expr *parse_product(Parser *p)
{
    Expr *e = parse_unary(p);
    while (e && (p->lex.tok.kind == T_STAR || p->lex.tok.kind == T_SLASH))
    {
        TokKind t = p->lex.tok.kind;
        lex_next(&p->lex);
        Expr *n = new_expr(E_BINARY);
        n->as.binary.op = t == T_STAR ? OP_MUL : OP_DIV;
        n->as.binary.left = e;
        n->as.binary.right = parse_unary(p);
        e = n;
    }
    return e;
}
static Expr *parse_sum(Parser *p)
{
    Expr *e = parse_product(p);
    while (e && (p->lex.tok.kind == T_PLUS || p->lex.tok.kind == T_MINUS))
    {
        TokKind t = p->lex.tok.kind;
        lex_next(&p->lex);
        Expr *n = new_expr(E_BINARY);
        n->as.binary.op = t == T_PLUS ? OP_ADD : OP_SUB;
        n->as.binary.left = e;
        n->as.binary.right = parse_product(p);
        e = n;
    }
    return e;
}
static Expr *parse_compare(Parser *p)
{
    Expr *e = parse_sum(p);
    while (e && (p->lex.tok.kind == T_LT || p->lex.tok.kind == T_LE ||
                 p->lex.tok.kind == T_GT || p->lex.tok.kind == T_GE))
    {
        TokKind t = p->lex.tok.kind;
        Expr *n = new_expr(E_BINARY);
        lex_next(&p->lex);
        n->as.binary.op = t == T_LT ? OP_LT : t == T_LE ? OP_LE
                                          : t == T_GT   ? OP_GT
                                                        : OP_GE;
        n->as.binary.left = e;
        n->as.binary.right = parse_sum(p);
        e = n;
    }
    return e;
}
static Expr *parse_equality(Parser *p)
{
    Expr *e = parse_compare(p);
    while (e && (p->lex.tok.kind == T_EQEQ || p->lex.tok.kind == T_NE))
    {
        TokKind t = p->lex.tok.kind;
        Expr *n = new_expr(E_BINARY);
        lex_next(&p->lex);
        n->as.binary.op = t == T_EQEQ ? OP_EQ : OP_NE;
        n->as.binary.left = e;
        n->as.binary.right = parse_compare(p);
        e = n;
    }
    return e;
}
static Expr *parse_and(Parser *p)
{
    Expr *e = parse_equality(p);
    while (e && accept(p, T_AND))
    {
        Expr *n = new_expr(E_BINARY);
        n->as.binary.op = OP_AND;
        n->as.binary.left = e;
        n->as.binary.right = parse_equality(p);
        e = n;
    }
    return e;
}
static Expr *parse_or(Parser *p)
{
    Expr *e = parse_and(p);
    while (e && accept(p, T_OR))
    {
        Expr *n = new_expr(E_BINARY);
        n->as.binary.op = OP_OR;
        n->as.binary.left = e;
        n->as.binary.right = parse_and(p);
        e = n;
    }
    return e;
}
static Expr *parse_expr(Parser *p)
{
    const char *start = p->lex.tok.start;
    Expr *e = parse_or(p);
    if (e && accept(p, T_Q))
    {
        Expr *n = new_expr(E_COND);
        n->as.cond.condition = e;
        n->as.cond.yes = parse_expr(p);
        expect(p, T_COLON, "':'");
        n->as.cond.no = parse_expr(p);
        e = n;
    }
    if (e)
    {
        e->source_start = start;
        e->source_end = p->lex.last_end;
    }
    return e;
}

static Expr *clone_expr(const Expr *src);
static Obj *clone_obj(const Obj *src)
{
    Obj *o = new_obj();
    size_t i;
    for (i = 0; i < src->count; i++)
        obj_add(o, xstrdup(src->fields[i].name), clone_expr(src->fields[i].value));
    return o;
}
static Expr *clone_expr(const Expr *s)
{
    Expr *e = new_expr(s->kind);
    size_t i;
    switch (s->kind)
    {
    case E_NUM:
        e->as.number = s->as.number;
        break;
    case E_BOOL:
        e->as.boolean = s->as.boolean;
        break;
    case E_STR:
        e->as.string = xstrdup(s->as.string);
        break;
    case E_OBJ:
        e->as.object = clone_obj(s->as.object);
        break;
    case E_TEMPLATE:
        e->as.template_.count = s->as.template_.count;
        e->as.template_.items = xmalloc(e->as.template_.count * sizeof(StringPart));
        for (i = 0; i < e->as.template_.count; i++)
        {
            e->as.template_.items[i].expression = s->as.template_.items[i].expression;
            e->as.template_.items[i].text = s->as.template_.items[i].text ? xstrdup(s->as.template_.items[i].text) : NULL;
            e->as.template_.items[i].expr = s->as.template_.items[i].expr ? clone_expr(s->as.template_.items[i].expr) : NULL;
        }
        break;
    case E_REF:
        e->as.ref.dots = s->as.ref.dots;
        e->as.ref.count = s->as.ref.count;
        e->as.ref.segments = xmalloc(e->as.ref.count * sizeof(char *));
        for (i = 0; i < e->as.ref.count; i++)
            e->as.ref.segments[i] = xstrdup(s->as.ref.segments[i]);
        break;
    case E_BINARY:
        e->as.binary.op = s->as.binary.op;
        e->as.binary.left = clone_expr(s->as.binary.left);
        e->as.binary.right = s->as.binary.right ? clone_expr(s->as.binary.right) : NULL;
        break;
    case E_COND:
        e->as.cond.condition = clone_expr(s->as.cond.condition);
        e->as.cond.yes = clone_expr(s->as.cond.yes);
        e->as.cond.no = clone_expr(s->as.cond.no);
        break;
    case E_CALL:
        e->as.call.name = xstrdup(s->as.call.name);
        e->as.call.count = s->as.call.count;
        e->as.call.args = xmalloc(e->as.call.count * sizeof(Expr *));
        for (i = 0; i < e->as.call.count; i++)
            e->as.call.args[i] = clone_expr(s->as.call.args[i]);
        break;
    }
    return e;
}
static void bind_expr(Expr *e, Obj *scope)
{
    size_t i;
    e->scope = scope;
    switch (e->kind)
    {
    case E_OBJ:
        bind_obj(e->as.object, scope);
        break;
    case E_TEMPLATE:
        for (i = 0; i < e->as.template_.count; i++)
            if (e->as.template_.items[i].expr)
                bind_expr(e->as.template_.items[i].expr, scope);
        break;
    case E_BINARY:
        bind_expr(e->as.binary.left, scope);
        if (e->as.binary.right)
            bind_expr(e->as.binary.right, scope);
        break;
    case E_COND:
        bind_expr(e->as.cond.condition, scope);
        bind_expr(e->as.cond.yes, scope);
        bind_expr(e->as.cond.no, scope);
        break;
    case E_CALL:
        for (i = 0; i < e->as.call.count; i++)
            bind_expr(e->as.call.args[i], scope);
        break;
    default:
        break;
    }
}

static Field *field_direct_n(Obj *o, const char *name, size_t name_len)
{
    size_t i;
    if (!o)
        return NULL;
    for (i = 0; i < o->count; i++)
        if (strlen(o->fields[i].name) == name_len &&
            !memcmp(o->fields[i].name, name, name_len))
            return &o->fields[i];
    return NULL;
}
static Field *field_direct(Obj *o, const char *name)
{
    return field_direct_n(o, name, strlen(name));
}
static Field *field_lexical(Obj *o, const char *name)
{
    for (; o; o = o->parent)
    {
        Field *f = field_direct(o, name);
        if (f)
            return f;
    }
    return NULL;
}
static int eval(Expr *e, Value fallback, Value *out);
static int to_num(Value v, double *n)
{
    if (v.kind == V_NUM)
    {
        *n = v.as.n;
        return 1;
    }
    if (v.kind == V_BOOL)
    {
        *n = v.as.b;
        return 1;
    }
    return 0;
}
static int truthy(Value v)
{
    if (v.kind == V_BOOL)
        return v.as.b;
    if (v.kind == V_NUM)
        return v.as.n != 0;
    return v.kind == V_STR && v.as.s[0];
}
static char *value_text(Value v)
{
    char b[64];
    if (v.kind == V_STR)
        return xstrdup(v.as.s);
    if (v.kind == V_BOOL)
        return xstrdup(v.as.b ? "true" : "false");
    if (v.kind == V_NUM)
    {
        snprintf(b, sizeof b, "%.15g", v.as.n);
        return xstrdup(b);
    }
    return xstrdup("");
}
/* Defined before the evaluator; replacement helper keeps object merge ownership correct. */
static Obj *merged(const Obj *a, const Obj *b, Obj *parent)
{
    Obj *r = clone_obj(a);
    size_t i, j;
    for (i = 0; i < b->count; i++)
    {
        for (j = 0; j < r->count; j++)
            if (!strcmp(r->fields[j].name, b->fields[i].name))
                break;
        if (j < r->count)
        {
            free(r->fields[j].name);
            free_expr(r->fields[j].value);
            r->fields[j].name = xstrdup(b->fields[i].name);
            r->fields[j].value = clone_expr(b->fields[i].value);
        }
        else
            obj_add(r, xstrdup(b->fields[i].name), clone_expr(b->fields[i].value));
    }
    bind_obj(r, parent);
    return r;
}
static int eval_ref(Expr *e, Value fallback, Value *out)
{
    Obj *o = e->scope;
    Field *f = NULL;
    size_t i;
    if (e->as.ref.dots)
    {
        for (i = 1; i < e->as.ref.dots && o; i++)
            o = o->parent;
        f = field_direct(o, e->as.ref.segments[0]);
    }
    else
        f = field_lexical(o, e->as.ref.segments[0]);
    if (!f)
        return 0;
    Value v;
    if (!eval(f->value, fallback, &v))
        return 0;
    for (i = 1; i < e->as.ref.count; i++)
    {
        if (v.kind != V_OBJ)
            return 0;
        f = field_direct(v.as.o, e->as.ref.segments[i]);
        if (!f)
            return 0;
        if (!eval(f->value, fallback, &v))
            return 0;
    }
    *out = v;
    return 1;
}
static int eval(Expr *e, Value fallback, Value *out)
{
    if (!e)
        return 0;
    if (e->state == RESOLVED)
    {
        *out = e->cache;
        return 1;
    }
    if (e->state == ERROR)
        return 0;
    if (e->state == RESOLVING)
    {
        fprintf(stderr, "mica: cyclic dependency detected\n");
        *out = fallback;
        return 1;
    }
    e->state = RESOLVING;
    Value v = {V_NULL};
    int ok = 1;
    size_t i;
    switch (e->kind)
    {
    case E_NUM:
        v = (Value){V_NUM, .as.n = e->as.number};
        break;
    case E_BOOL:
        v = (Value){V_BOOL, .as.b = e->as.boolean};
        break;
    case E_STR:
        v = (Value){V_STR, .as.s = e->as.string};
        break;
    case E_OBJ:
        v = (Value){V_OBJ, .as.o = e->as.object};
        break;
    case E_REF:
        ok = eval_ref(e, fallback, &v);
        break;
    case E_COND:
    {
        Value c;
        ok = eval(e->as.cond.condition, fallback, &c);
        if (ok)
            ok = eval(truthy(c) ? e->as.cond.yes : e->as.cond.no, fallback, &v);
        break;
    }
    case E_TEMPLATE:
    {
        size_t n = 1, used = 0;
        char *s = xmalloc(n);
        s[0] = 0;
        for (i = 0; i < e->as.template_.count && ok; i++)
        {
            char *part;
            if (e->as.template_.items[i].expression)
            {
                Value x;
                ok = eval(e->as.template_.items[i].expr, fallback, &x);
                part = ok ? value_text(x) : NULL;
            }
            else
                part = xstrdup(e->as.template_.items[i].text);
            if (ok)
            {
                size_t z = strlen(part);
                if (used + z + 1 > n)
                {
                    n = used + z + 1;
                    s = realloc(s, n);
                    if (!s)
                        abort();
                }
                memcpy(s + used, part, z + 1);
                used += z;
            }
            free(part);
        }
        if (ok)
        {
            v = (Value){V_STR, .as.s = s};
            e->cache_owned = 1;
        }
        else
            free(s);
        break;
    }
    case E_BINARY:
    {
        Value a, b;
        double x, y;
        int cmp;
        ok = eval(e->as.binary.left, fallback, &a);
        if (!ok)
            break;
        if (e->as.binary.op == OP_NEG)
        {
            ok = to_num(a, &x);
            if (ok)
                v = (Value){V_NUM, .as.n = -x};
            break;
        }
        if (e->as.binary.op == OP_NOT)
        {
            v = (Value){V_BOOL, .as.b = !truthy(a)};
            break;
        }
        /* These operators intentionally avoid evaluating an unnecessary RHS. */
        if (e->as.binary.op == OP_AND && !truthy(a))
        {
            v = (Value){V_BOOL, .as.b = 0};
            break;
        }
        if (e->as.binary.op == OP_OR && truthy(a))
        {
            v = (Value){V_BOOL, .as.b = 1};
            break;
        }
        ok = eval(e->as.binary.right, fallback, &b);
        if (!ok)
            break;
        if (e->as.binary.op == OP_AND || e->as.binary.op == OP_OR)
        {
            v = (Value){V_BOOL, .as.b = truthy(b)};
            break;
        }
        if (e->as.binary.op == OP_ADD && a.kind == V_OBJ && b.kind == V_OBJ)
        {
            /* Preserve the base object's lexical parent (e.g. engine for engine.window). */
            v = (Value){V_OBJ, .as.o = merged(a.as.o, b.as.o, a.as.o->parent)};
            e->cache_owned = 1;
            break;
        }
        if (e->as.binary.op == OP_ADD && a.kind == V_STR && b.kind == V_STR)
        {
            size_t z = strlen(a.as.s) + strlen(b.as.s) + 1;
            char *s = xmalloc(z);
            snprintf(s, z, "%s%s", a.as.s, b.as.s);
            v = (Value){V_STR, .as.s = s};
            e->cache_owned = 1;
            break;
        }
        if ((e->as.binary.op == OP_EQ || e->as.binary.op == OP_NE) &&
            a.kind == V_STR && b.kind == V_STR)
        {
            cmp = strcmp(a.as.s, b.as.s) == 0;
            v = (Value){V_BOOL, .as.b = e->as.binary.op == OP_EQ ? cmp : !cmp};
            break;
        }
        ok = to_num(a, &x) && to_num(b, &y);
        if (!ok)
            break;
        if (e->as.binary.op == OP_DIV && y == 0)
        {
            ok = 0;
            break;
        }
        switch (e->as.binary.op)
        {
        case OP_ADD:
            v = (Value){V_NUM, .as.n = x + y};
            break;
        case OP_SUB:
            v = (Value){V_NUM, .as.n = x - y};
            break;
        case OP_MUL:
            v = (Value){V_NUM, .as.n = x * y};
            break;
        case OP_DIV:
            v = (Value){V_NUM, .as.n = x / y};
            break;
        case OP_EQ:
            v = (Value){V_BOOL, .as.b = x == y};
            break;
        case OP_NE:
            v = (Value){V_BOOL, .as.b = x != y};
            break;
        case OP_LT:
            v = (Value){V_BOOL, .as.b = x < y};
            break;
        case OP_LE:
            v = (Value){V_BOOL, .as.b = x <= y};
            break;
        case OP_GT:
            v = (Value){V_BOOL, .as.b = x > y};
            break;
        case OP_GE:
            v = (Value){V_BOOL, .as.b = x >= y};
            break;
        default:
            ok = 0;
            break;
        }
        break;
    }
    case E_CALL:
    {
        /* Keep all invalid-call paths defined before returning a fallback. */
        double x = 0.0, lo = 0.0, hi = 0.0, result = 0.0;
        if (!strcmp(e->as.call.name, "clamp") && e->as.call.count == 3)
        {
            Value q;
            ok = eval(e->as.call.args[0], fallback, &q) && to_num(q, &x) &&
                 eval(e->as.call.args[1], fallback, &q) && to_num(q, &lo) &&
                 eval(e->as.call.args[2], fallback, &q) && to_num(q, &hi);
            if (ok)
            {
                if (lo > hi)
                {
                    double t = lo;
                    lo = hi;
                    hi = t;
                }
                v = (Value){V_NUM, .as.n = x < lo ? lo : x > hi ? hi
                                                                : x};
            }
        }
        else if ((!strcmp(e->as.call.name, "avg") ||
                  !strcmp(e->as.call.name, "min") ||
                  !strcmp(e->as.call.name, "max")) &&
                 e->as.call.count)
        {
            for (i = 0; i < e->as.call.count && ok; i++)
            {
                Value q;
                ok = eval(e->as.call.args[i], fallback, &q) && to_num(q, &x);
                if (i == 0)
                    result = x;
                else if (!strcmp(e->as.call.name, "avg"))
                    result += x;
                else if (!strcmp(e->as.call.name, "min") && x < result)
                    result = x;
                else if (!strcmp(e->as.call.name, "max") && x > result)
                    result = x;
            }
            if (ok)
            {
                if (!strcmp(e->as.call.name, "avg"))
                    result /= (double)e->as.call.count;
                v = (Value){V_NUM, .as.n = result};
            }
        }
        else if (!strcmp(e->as.call.name, "abs") && e->as.call.count == 1)
        {
            Value q;
            ok = eval(e->as.call.args[0], fallback, &q) && to_num(q, &x);
            if (ok)
                v = (Value){V_NUM, .as.n = x < 0 ? -x : x};
        }
        else
        {
            ok = 0;
        }
        break;
    }
    }
    if (!ok)
    {
        e->state = ERROR;
        fprintf(stderr, "mica: evaluation error\n");
        return 0;
    }
    e->state = RESOLVED;
    e->cache = v;
    *out = v;
    return 1;
}

static void free_obj(Obj *o)
{
    size_t i;
    if (!o)
        return;
    for (i = 0; i < o->count; i++)
    {
        free(o->fields[i].name);
        free_expr(o->fields[i].value);
    }
    free(o->fields);
    free(o);
}
static void free_expr(Expr *e)
{
    size_t i;
    if (!e)
        return;
    switch (e->kind)
    {
    case E_STR:
        free(e->as.string);
        break;
    case E_TEMPLATE:
        for (i = 0; i < e->as.template_.count; i++)
        {
            free(e->as.template_.items[i].text);
            free_expr(e->as.template_.items[i].expr);
        }
        free(e->as.template_.items);
        break;
    case E_OBJ:
        free_obj(e->as.object);
        break;
    case E_REF:
        for (i = 0; i < e->as.ref.count; i++)
            free(e->as.ref.segments[i]);
        free(e->as.ref.segments);
        break;
    case E_BINARY:
        free_expr(e->as.binary.left);
        free_expr(e->as.binary.right);
        break;
    case E_COND:
        free_expr(e->as.cond.condition);
        free_expr(e->as.cond.yes);
        free_expr(e->as.cond.no);
        break;
    case E_CALL:
        free(e->as.call.name);
        for (i = 0; i < e->as.call.count; i++)
            free_expr(e->as.call.args[i]);
        free(e->as.call.args);
        break;
    default:
        break;
    }
    if (e->cache_owned)
    {
        if (e->cache.kind == V_STR)
            free(e->cache.as.s);
        else if (e->cache.kind == V_OBJ)
            free_obj(e->cache.as.o);
    }
    free(e);
}

MicaContext *mica_parse(const char *source)
{
    MicaContext *ctx = xmalloc(sizeof *ctx);
    ctx->root = new_obj();
    ctx->source = xstrdup(source ? source : "");
    ctx->source_len = strlen(ctx->source);
    Parser p = {0};
    p.lex.src = p.lex.at = ctx->source;
    lex_next(&p.lex);
    skip_terms(&p);
    while (!p.error[0] && p.lex.tok.kind != T_EOF)
    {
        if (p.lex.tok.kind != T_ID)
        {
            parse_fail(&p, "expected top-level name");
            break;
        }
        char *name = xstrndup(p.lex.tok.start, p.lex.tok.len);
        lex_next(&p.lex);
        expect(&p, T_EQ, "'='");
        Expr *v = parse_expr(&p);
        if (v)
            obj_add(ctx->root, name, v);
        else
            free(name);
        if (p.lex.tok.kind != T_EOF && p.lex.tok.kind != T_NL && p.lex.tok.kind != T_SEMI && p.lex.tok.kind != T_COMMA)
            parse_fail(&p, "expected end of assignment");
        skip_terms(&p);
    }
    if (p.lex.error[0] && !p.error[0])
        snprintf(p.error, sizeof p.error, "%s", p.lex.error);
    free(p.lex.tok.text);
    if (p.error[0])
        snprintf(ctx->error, sizeof ctx->error, "%s", p.error);
    bind_obj(ctx->root, NULL);
    return ctx;
}
void mica_destroy(MicaContext *ctx)
{
    if (ctx)
    {
        free_obj(ctx->root);
        free(ctx->source);
        free(ctx);
    }
}
const char *mica_last_error(const MicaContext *ctx) { return ctx && ctx->error[0] ? ctx->error : NULL; }
static int path_value(MicaContext *ctx, const char *path, Value fallback, Value *out)
{
    const char *segment;
    Obj *o;
    Field *f;
    if (!ctx || ctx->error[0] || !path || !*path)
        return 0;
    o = ctx->root;
    segment = path;
    for (;;)
    {
        const char *end = segment;
        while (*end && *end != '.')
            end++;

        /* Empty components make a root-relative path malformed. */
        if (end == segment)
            return 0;

        f = field_direct_n(o, segment, (size_t)(end - segment));
        if (!f)
            return 0;
        if (!eval(f->value, fallback, out))
            return 0;

        if (!*end)
            return 1;

        segment = end + 1;
        if (!*segment || out->kind != V_OBJ)
        {
            return 0;
        }
        o = out->as.o;
    }
}
double mica_get_float(MicaContext *ctx, const char *path, double fallback)
{
    Value v = {V_NUM, .as.n = fallback};
    double result = fallback;
    return path_value(ctx, path, v, &v) && to_num(v, &result) ? result : fallback;
}
int mica_get_bool(MicaContext *ctx, const char *path, int fallback)
{
    Value v = {V_BOOL, .as.b = !!fallback};
    return path_value(ctx, path, v, &v) ? truthy(v) : fallback;
}
const char *mica_get_string(MicaContext *ctx, const char *path, const char *fallback)
{
    Value v = {V_STR, .as.s = (char *)(fallback ? fallback : "")};
    return path_value(ctx, path, v, &v) && v.kind == V_STR ? v.as.s : fallback;
}

typedef enum
{
    SET_FLOAT,
    SET_BOOL,
    SET_STRING
} SetType;

typedef struct
{
    char *data;
    size_t len;
    size_t cap;
} TextBuffer;

static void text_append_n(TextBuffer *text, const char *value, size_t len)
{
    size_t required = text->len + len + 1;
    if (required > text->cap)
    {
        size_t cap = text->cap ? text->cap : 64;
        while (cap < required)
            cap *= 2;
        text->data = realloc(text->data, cap);
        if (!text->data)
            abort();
        text->cap = cap;
    }
    memcpy(text->data + text->len, value, len);
    text->len += len;
    text->data[text->len] = '\0';
}
static void text_append(TextBuffer *text, const char *value)
{
    text_append_n(text, value, strlen(value));
}
static void text_append_indent(TextBuffer *text, const char *base, size_t base_len, size_t depth)
{
    size_t i;
    text_append_n(text, base, base_len);
    for (i = 0; i < depth; i++)
        text_append(text, "    ");
}
static int path_part(const char *path, const char **name, size_t *name_len, const char **next)
{
    const char *end;
    if (!path || !*path || !(isalpha((unsigned char)*path) || *path == '_'))
        return 0;
    end = path + 1;
    while (isalnum((unsigned char)*end) || *end == '_')
        end++;
    if (*end && *end != '.')
        return 0;
    *name = path;
    *name_len = (size_t)(end - path);
    *next = *end ? end + 1 : end;
    return !*end || **next;
}
static int valid_path(const char *path)
{
    const char *name, *next;
    size_t name_len;
    do
    {
        if (!path_part(path, &name, &name_len, &next))
            return 0;
        path = next;
    } while (*path);
    return 1;
}
static void append_quoted_string(TextBuffer *text, const char *value)
{
    const char *at;
    text_append(text, "\"");
    for (at = value; *at; at++)
    {
        switch (*at)
        {
        case '\\': text_append(text, "\\\\"); break;
        case '\"': text_append(text, "\\\""); break;
        case '\n': text_append(text, "\\n"); break;
        case '\t': text_append(text, "\\t"); break;
        case '\r': text_append(text, "\\r"); break;
        default: text_append_n(text, at, 1); break;
        }
    }
    text_append(text, "\"");
}
static char *literal_float(double value)
{
    char buffer[64];
    if (!isfinite(value))
        return NULL;
    snprintf(buffer, sizeof buffer, "%.17g", value);
    return xstrdup(buffer);
}
static char *literal_bool(int value)
{
    return xstrdup(value ? "true" : "false");
}
static char *literal_string(const Expr *old, const char *value)
{
    TextBuffer text = {0};
    int raw = old && old->source_end - old->source_start >= 6 &&
              !memcmp(old->source_start, "'''", 3) &&
              !memcmp(old->source_end - 3, "'''", 3) &&
              !strstr(value, "'''");
    if (raw)
    {
        text_append(&text, "'''");
        text_append(&text, value);
        text_append(&text, "'''");
    }
    else
    {
        append_quoted_string(&text, value);
    }
    return text.data;
}
static MicaSetResult replace_source(MicaContext **ctx_ptr, const char *start,
                                    const char *end, const char *replacement)
{
    MicaContext *ctx = *ctx_ptr;
    size_t start_at, end_at, replacement_len;
    char *next_source;
    MicaContext *next;
    if (start < ctx->source || end < start || end > ctx->source + ctx->source_len)
        return MICA_SET_REPARSE_ERROR;
    start_at = (size_t)(start - ctx->source);
    end_at = (size_t)(end - ctx->source);
    replacement_len = strlen(replacement);
    next_source = xmalloc(start_at + replacement_len + (ctx->source_len - end_at) + 1);
    memcpy(next_source, ctx->source, start_at);
    memcpy(next_source + start_at, replacement, replacement_len);
    memcpy(next_source + start_at + replacement_len, ctx->source + end_at,
           ctx->source_len - end_at + 1);
    next = mica_parse(next_source);
    free(next_source);
    if (mica_last_error(next))
    {
        mica_destroy(next);
        return MICA_SET_REPARSE_ERROR;
    }
    mica_destroy(ctx);
    *ctx_ptr = next;
    return MICA_SET_OK;
}
static void append_nested_path(TextBuffer *text, const char *path,
                               const char *base, size_t base_len, size_t depth,
                               const char *literal, const char *newline)
{
    const char *name, *next;
    size_t name_len;
    (void)path_part(path, &name, &name_len, &next);
    text_append_indent(text, base, base_len, depth);
    text_append_n(text, name, name_len);
    text_append(text, " = ");
    if (!*next)
    {
        text_append(text, literal);
        return;
    }
    text_append(text, "{");
    text_append(text, newline);
    append_nested_path(text, next, base, base_len, depth + 1, literal, newline);
    text_append(text, newline);
    text_append_indent(text, base, base_len, depth);
    text_append(text, "}");
}
static void object_base_indent(const MicaContext *ctx, const Obj *object,
                               const char **base, size_t *base_len)
{
    const char *line = object->source_open;
    const char *at;
    while (line > ctx->source && line[-1] != '\n')
        line--;
    at = line;
    while (*at == ' ' || *at == '\t')
        at++;
    *base = line;
    *base_len = (size_t)(at - line);
}
static MicaSetResult append_missing_path(MicaContext **ctx_ptr, Obj *object,
                                         const char *missing_path, const char *literal)
{
    MicaContext *ctx = *ctx_ptr;
    TextBuffer insertion = {0};
    const char *newline = strstr(ctx->source, "\r\n") ? "\r\n" : "\n";
    if (object == ctx->root)
    {
        if (ctx->source_len && ctx->source[ctx->source_len - 1] != '\n' &&
            ctx->source[ctx->source_len - 1] != '\r')
            text_append(&insertion, newline);
        append_nested_path(&insertion, missing_path, "", 0, 0, literal, newline);
        return replace_source(ctx_ptr, ctx->source + ctx->source_len,
                              ctx->source + ctx->source_len, insertion.data);
    }
    else
    {
        const char *base;
        const char *start;
        size_t base_len;
        if (!object->source_open || !object->source_close)
            return MICA_SET_REPARSE_ERROR;
        object_base_indent(ctx, object, &base, &base_len);
        start = object->source_close;
        while (start > ctx->source &&
               (start[-1] == ' ' || start[-1] == '\t' ||
                start[-1] == '\r' || start[-1] == '\n'))
            start--;
        text_append(&insertion, newline);
        append_nested_path(&insertion, missing_path, base, base_len, 1, literal, newline);
        text_append(&insertion, newline);
        text_append_n(&insertion, base, base_len);
        return replace_source(ctx_ptr, start, object->source_close, insertion.data);
    }
}
static MicaSetResult set_literal(MicaContext **ctx_ptr, const char *path,
                                 const char *literal, SetType type, int force)
{
    MicaContext *ctx;
    Obj *object;
    const char *part;
    if (!ctx_ptr || !(ctx = *ctx_ptr) || !literal)
        return MICA_SET_INVALID_ARGUMENT;
    if (!valid_path(path))
        return MICA_SET_INVALID_PATH;
    if (ctx->error[0])
        return MICA_SET_REPARSE_ERROR;
    object = ctx->root;
    part = path;
    for (;;)
    {
        const char *name, *next;
        size_t name_len;
        Field *field;
        (void)path_part(part, &name, &name_len, &next);
        field = field_direct_n(object, name, name_len);
        if (!field)
            return append_missing_path(ctx_ptr, object, part, literal);
        if (!*next)
        {
            Expr *old = field->value;
            int matching_literal = (type == SET_FLOAT && old->kind == E_NUM) ||
                                   (type == SET_BOOL && old->kind == E_BOOL) ||
                                   (type == SET_STRING && old->kind == E_STR);
            if (!matching_literal && !force)
            {
                if (old->kind == E_NUM || old->kind == E_BOOL || old->kind == E_STR)
                    return MICA_SET_TYPE_MISMATCH;
                return MICA_SET_IS_EXPRESSION;
            }
            if (!old->source_start || !old->source_end)
                return MICA_SET_REPARSE_ERROR;
            return replace_source(ctx_ptr, old->source_start, old->source_end, literal);
        }
        if (field->value->kind != E_OBJ)
            return MICA_SET_INTERMEDIATE_NOT_OBJECT;
        object = field->value->as.object;
        part = next;
    }
}
static Expr *direct_value_at_path(MicaContext *ctx, const char *path)
{
    Obj *object;
    if (!ctx || !valid_path(path))
        return NULL;
    object = ctx->root;
    for (;;)
    {
        const char *name, *next;
        size_t name_len;
        Field *field;
        (void)path_part(path, &name, &name_len, &next);
        field = field_direct_n(object, name, name_len);
        if (!field)
            return NULL;
        if (!*next)
            return field->value;
        if (field->value->kind != E_OBJ)
            return NULL;
        object = field->value->as.object;
        path = next;
    }
}

const char *mica_source(const MicaContext *ctx)
{
    return ctx ? ctx->source : NULL;
}
MicaSetResult mica_set_float(MicaContext **ctx, const char *path, double value)
{
    char *literal = literal_float(value);
    MicaSetResult result = literal ? set_literal(ctx, path, literal, SET_FLOAT, 0)
                                   : MICA_SET_INVALID_ARGUMENT;
    free(literal);
    return result;
}
MicaSetResult mica_set_bool(MicaContext **ctx, const char *path, int value)
{
    char *literal = literal_bool(value);
    MicaSetResult result = set_literal(ctx, path, literal, SET_BOOL, 0);
    free(literal);
    return result;
}
MicaSetResult mica_set_string(MicaContext **ctx, const char *path, const char *value)
{
    char *literal;
    MicaSetResult result;
    if (!value)
        return MICA_SET_INVALID_ARGUMENT;
    literal = literal_string(ctx ? direct_value_at_path(*ctx, path) : NULL, value);
    result = set_literal(ctx, path, literal, SET_STRING, 0);
    free(literal);
    return result;
}
MicaSetResult mica_set_float_force(MicaContext **ctx, const char *path, double value)
{
    char *literal = literal_float(value);
    MicaSetResult result = literal ? set_literal(ctx, path, literal, SET_FLOAT, 1)
                                   : MICA_SET_INVALID_ARGUMENT;
    free(literal);
    return result;
}
MicaSetResult mica_set_bool_force(MicaContext **ctx, const char *path, int value)
{
    char *literal = literal_bool(value);
    MicaSetResult result = set_literal(ctx, path, literal, SET_BOOL, 1);
    free(literal);
    return result;
}
MicaSetResult mica_set_string_force(MicaContext **ctx, const char *path, const char *value)
{
    char *literal;
    MicaSetResult result;
    if (!value)
        return MICA_SET_INVALID_ARGUMENT;
    literal = literal_string(ctx ? direct_value_at_path(*ctx, path) : NULL, value);
    result = set_literal(ctx, path, literal, SET_STRING, 1);
    free(literal);
    return result;
}
