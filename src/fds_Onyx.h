#ifndef FDS_EXT_CFG_H
#define FDS_EXT_CFG_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifndef FDS_STRING_VIEW_DEFINED
typedef struct {
    const char *data;
    size_t len;
} fds_string_view;
#define FDS_STRING_VIEW_DEFINED
#endif

typedef enum Fds_Cfg_Type {
    FDS_CFG_NIL = 0,
    FDS_CFG_INT,
    FDS_CFG_FLOAT,
    FDS_CFG_BOOL,
    FDS_CFG_STR,
    FDS_CFG_ARRAY,
    FDS_CFG_STRUCT
} Fds_Cfg_Type;

typedef enum Fds_Cfg_Op {
    FDS_CFG_OP_NONE = 0,
    FDS_CFG_OP_ADD,
    FDS_CFG_OP_SUB,
    FDS_CFG_OP_MUL,
    FDS_CFG_OP_DIV,
    FDS_CFG_OP_VAR,
    FDS_CFG_OP_NEG,
    FDS_CFG_OP_NOT,
    FDS_CFG_OP_AND,
    FDS_CFG_OP_OR,
    FDS_CFG_OP_EQ,
    FDS_CFG_OP_NE,
    FDS_CFG_OP_LT,
    FDS_CFG_OP_GT,
    FDS_CFG_OP_LE,
    FDS_CFG_OP_GE,
    FDS_CFG_OP_COND
} Fds_Cfg_Op;

typedef enum Fds_Cfg_ErrCode {
    FDS_CFG_OK = 0,
    FDS_CFG_ERR_UNEXPECTED_TOKEN,
    FDS_CFG_ERR_TYPE_MISMATCH,
    FDS_CFG_ERR_UNDEFINED_VAR,
    FDS_CFG_ERR_DIV_BY_ZERO,
    FDS_CFG_ERR_SYNTAX,
    FDS_CFG_ERR_OOM,
    FDS_CFG_ERR_ASSERT_FAILED,
    FDS_CFG_ERR_RUNTIME
} Fds_Cfg_ErrCode;

typedef struct Fds_Cfg_Error {
    Fds_Cfg_ErrCode code;
    size_t line;
    size_t column;
    const char *msg;
} Fds_Cfg_Error;

/* -------------------------------------------------------------------------- */
/*  Публічний інтерфейс                                                       */
/* -------------------------------------------------------------------------- */

typedef struct Fds_Cfg_Doc Fds_Cfg_Doc;

Fds_Cfg_Doc* fds_cfg_parse(fds_string_view source, Fds_Cfg_Error *out_err);
Fds_Cfg_Doc* fds_cfg_load(const char *filename, Fds_Cfg_Error *out_err);
Fds_Cfg_Doc* fds_cfg_load_binary(const void *data, size_t size, Fds_Cfg_Error *out_err);
void fds_cfg_free(Fds_Cfg_Doc *doc);

bool fds_cfg_has_key(const Fds_Cfg_Doc *doc, const char *key);
int64_t fds_cfg_get_int(const Fds_Cfg_Doc *doc, const char *key, int64_t default_val);
double fds_cfg_get_float(const Fds_Cfg_Doc *doc, const char *key, double default_val);
bool fds_cfg_get_bool(const Fds_Cfg_Doc *doc, const char *key, bool default_val);
const char* fds_cfg_get_string(const Fds_Cfg_Doc *doc, const char *key, const char *default_val);
Fds_Cfg_Type fds_cfg_get_type(const Fds_Cfg_Doc *doc, const char *key);

#endif /* FDS_EXT_CFG_H */

/* ========================================================================== */
/*  РЕАЛІЗАЦІЯ                                                                */
/* ========================================================================== */
#ifdef FDS_EXT_CFG_IMPLEMENTATION

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* -------------------------------------------------------------------------- */
/*  Внутрішній блоб                                                           */
/* -------------------------------------------------------------------------- */

#define FDS_CFG_MAGIC 0x43534446
#define FDS_CFG_VERSION 1

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t total_size;
    uint32_t entry_count;
    uint32_t entries_offset;
    uint32_t strings_offset;
} Fds_Blob_Header;

typedef struct {
    uint32_t name_offset;
    uint32_t type;
    union {
        int64_t i_val;
        double f_val;
        uint8_t b_val;
        uint32_t data_offset;
    } as;
} Fds_Blob_Entry;

struct Fds_Cfg_Doc {
    void *blob;
    size_t blob_size;
};

/* -------------------------------------------------------------------------- */
/*  AST та середовище компіляції                                              */
/* -------------------------------------------------------------------------- */

typedef enum {
    FDS_AST_STRUCT,
    FDS_AST_FIELD,
    FDS_AST_DEF,
    FDS_AST_IF,
    FDS_AST_ASSERT
} Fds_Ast_Kind;

typedef struct Fds_Expr_Node Fds_Expr_Node;

typedef struct Fds_Ast_Node {
    Fds_Ast_Kind kind;
    struct Fds_Ast_Node *next;
    size_t line;
    size_t column;
    union {
        struct {
            struct Fds_Ast_Node *first;
        } struct_body;
        struct {
            fds_string_view key;
            Fds_Expr_Node *expr;
        } field;
        struct {
            fds_string_view name;
            fds_string_view *params;
            size_t param_count;
            Fds_Expr_Node *body;
        } def;
        struct {
            Fds_Expr_Node *condition;
            struct Fds_Ast_Node *then_branch;
            struct Fds_Ast_Node *else_branch;
        } if_block;
        struct {
            Fds_Expr_Node *condition;
            fds_string_view message;
        } assert;
    };
} Fds_Ast_Node;

typedef enum {
    FDS_EXPR_VALUE,
    FDS_EXPR_VAR,
    FDS_EXPR_UNARY,
    FDS_EXPR_BINARY,
    FDS_EXPR_COND,
    FDS_EXPR_CALL
} Fds_Expr_Kind;

struct Fds_Expr_Node {
    Fds_Expr_Kind kind;
    Fds_Cfg_Type value_type;
    union {
        int64_t i;
        double f;
        bool b;
        fds_string_view s;
    } value;
    fds_string_view var_name;
    struct {
        int up_count;
        bool is_root;
        fds_string_view *parts;
        size_t parts_count;
    } path;
    Fds_Cfg_Op op;
    Fds_Expr_Node *left;
    Fds_Expr_Node *right;
    Fds_Expr_Node *third;
    fds_string_view func_name;
    Fds_Expr_Node **args;
    size_t arg_count;
};

typedef struct {
    fds_string_view name;
    Fds_Cfg_Type type;
    bool owns_string;
    union {
        int64_t i;
        double f;
        bool b;
        fds_string_view s;
    } value;
} Fds_Cfg_Var;

typedef struct {
    Fds_Cfg_Var *items;
    size_t count;
    size_t cap;
} Fds_Cfg_Env;

typedef struct {
    fds_string_view name;
    fds_string_view *params;   /* тепер копіюється */
    size_t param_count;
    Fds_Expr_Node *body;
} Fds_Cfg_Func;

typedef struct {
    Fds_Cfg_Func *items;
    size_t count;
    size_t cap;
} Fds_Func_Table;

typedef struct {
    char *key;
    Fds_Cfg_Type type;
    union {
        int64_t i;
        double f;
        bool b;
        char *s;
    } value;
} Fds_Flat_Entry;

/* -------------------------------------------------------------------------- */
/*  Лексер                                                                    */
/* -------------------------------------------------------------------------- */

typedef enum {
    TOK_EOF = 0, TOK_IDENT, TOK_INT, TOK_FLOAT, TOK_STR, TOK_BOOL,
    TOK_ASSIGN, TOK_SEMI, TOK_LBRACE, TOK_RBRACE, TOK_LPAREN, TOK_RPAREN,
    TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_SLASH, TOK_DOT, TOK_AT,
    TOK_BANG, TOK_AMP, TOK_PIPE, TOK_QUESTION, TOK_COLON,
    TOK_EQ, TOK_NE, TOK_LT, TOK_GT, TOK_LE, TOK_GE,
    TOK_AND, TOK_OR,
    TOK_DEF, TOK_AT_IF, TOK_AT_ELSE, TOK_AT_ASSERT,
    TOK_DOLLAR, TOK_COMMA,
    TOK_ERR
} Fds_Cfg_TokKind;

typedef struct {
    Fds_Cfg_TokKind kind;
    fds_string_view lexeme;
    size_t line;
    size_t col;
} Fds_Cfg_Token;

typedef struct {
    const char *src;
    size_t len;
    size_t cursor;
    size_t line;
    size_t col;
} Fds_Cfg_Lexer;

static bool fds__sv_eq(fds_string_view a, fds_string_view b) {
    if (a.len != b.len) return false;
    return memcmp(a.data, b.data, a.len) == 0;
}

static fds_string_view fds__sv_from_cstr(const char *str) {
    return (fds_string_view){str, strlen(str)};
}

static int64_t fds__sv_to_int(fds_string_view sv) {
    int64_t res = 0; bool neg = false; size_t i = 0;
    if (sv.len > 0 && sv.data[0] == '-') { neg = true; i++; }
    for (; i < sv.len; i++) {
        if (sv.data[i] >= '0' && sv.data[i] <= '9')
            res = res * 10 + (sv.data[i] - '0');
    }
    return neg ? -res : res;
}

static double fds__sv_to_float(fds_string_view sv) {
    char buf[64];
    size_t len = sv.len < 63 ? sv.len : 63;
    memcpy(buf, sv.data, len);
    buf[len] = '\0';
    return atof(buf);
}

static Fds_Cfg_Token fds_cfg_lexer_next(Fds_Cfg_Lexer *lexer) {
    while (lexer->cursor < lexer->len) {
        char c = lexer->src[lexer->cursor];

        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            if (c == '\n') { lexer->line++; lexer->col = 1; }
            else lexer->col++;
            lexer->cursor++;
            continue;
        }

        if (c == '#' || (c == '/' && lexer->cursor+1 < lexer->len && lexer->src[lexer->cursor+1] == '/')) {
            while (lexer->cursor < lexer->len && lexer->src[lexer->cursor] != '\n') lexer->cursor++;
            continue;
        }

        size_t start_col = lexer->col;
        size_t start_pos = lexer->cursor;

        if (c == '=') {
            lexer->cursor++; lexer->col++;
            if (lexer->cursor < lexer->len && lexer->src[lexer->cursor] == '=') {
                lexer->cursor++; lexer->col++;
                return (Fds_Cfg_Token){TOK_EQ, {lexer->src+start_pos,2}, lexer->line, start_col};
            }
            return (Fds_Cfg_Token){TOK_ASSIGN, {lexer->src+start_pos,1}, lexer->line, start_col};
        }
        if (c == '!' && lexer->cursor+1 < lexer->len && lexer->src[lexer->cursor+1] == '=') {
            lexer->cursor += 2; lexer->col += 2;
            return (Fds_Cfg_Token){TOK_NE, {lexer->src+start_pos,2}, lexer->line, start_col};
        }
        if (c == '<') {
            lexer->cursor++; lexer->col++;
            if (lexer->cursor < lexer->len && lexer->src[lexer->cursor] == '=') {
                lexer->cursor++; lexer->col++;
                return (Fds_Cfg_Token){TOK_LE, {lexer->src+start_pos,2}, lexer->line, start_col};
            }
            return (Fds_Cfg_Token){TOK_LT, {lexer->src+start_pos,1}, lexer->line, start_col};
        }
        if (c == '>') {
            lexer->cursor++; lexer->col++;
            if (lexer->cursor < lexer->len && lexer->src[lexer->cursor] == '=') {
                lexer->cursor++; lexer->col++;
                return (Fds_Cfg_Token){TOK_GE, {lexer->src+start_pos,2}, lexer->line, start_col};
            }
            return (Fds_Cfg_Token){TOK_GT, {lexer->src+start_pos,1}, lexer->line, start_col};
        }
        if (c == '&' && lexer->cursor+1 < lexer->len && lexer->src[lexer->cursor+1] == '&') {
            lexer->cursor += 2; lexer->col += 2;
            return (Fds_Cfg_Token){TOK_AND, {lexer->src+start_pos,2}, lexer->line, start_col};
        }
        if (c == '|' && lexer->cursor+1 < lexer->len && lexer->src[lexer->cursor+1] == '|') {
            lexer->cursor += 2; lexer->col += 2;
            return (Fds_Cfg_Token){TOK_OR, {lexer->src+start_pos,2}, lexer->line, start_col};
        }

        switch (c) {
            case ';': lexer->cursor++; lexer->col++; return (Fds_Cfg_Token){TOK_SEMI, {lexer->src+start_pos,1}, lexer->line, start_col};
            case '{': lexer->cursor++; lexer->col++; return (Fds_Cfg_Token){TOK_LBRACE, {lexer->src+start_pos,1}, lexer->line, start_col};
            case '}': lexer->cursor++; lexer->col++; return (Fds_Cfg_Token){TOK_RBRACE, {lexer->src+start_pos,1}, lexer->line, start_col};
            case '(': lexer->cursor++; lexer->col++; return (Fds_Cfg_Token){TOK_LPAREN, {lexer->src+start_pos,1}, lexer->line, start_col};
            case ')': lexer->cursor++; lexer->col++; return (Fds_Cfg_Token){TOK_RPAREN, {lexer->src+start_pos,1}, lexer->line, start_col};
            case '+': lexer->cursor++; lexer->col++; return (Fds_Cfg_Token){TOK_PLUS, {lexer->src+start_pos,1}, lexer->line, start_col};
            case '-': lexer->cursor++; lexer->col++; return (Fds_Cfg_Token){TOK_MINUS, {lexer->src+start_pos,1}, lexer->line, start_col};
            case '*': lexer->cursor++; lexer->col++; return (Fds_Cfg_Token){TOK_STAR, {lexer->src+start_pos,1}, lexer->line, start_col};
            case '/': lexer->cursor++; lexer->col++; return (Fds_Cfg_Token){TOK_SLASH, {lexer->src+start_pos,1}, lexer->line, start_col};
            case '.': lexer->cursor++; lexer->col++; return (Fds_Cfg_Token){TOK_DOT, {lexer->src+start_pos,1}, lexer->line, start_col};
            case '@': lexer->cursor++; lexer->col++; return (Fds_Cfg_Token){TOK_AT, {lexer->src+start_pos,1}, lexer->line, start_col};
            case '!': lexer->cursor++; lexer->col++; return (Fds_Cfg_Token){TOK_BANG, {lexer->src+start_pos,1}, lexer->line, start_col};
            case '?': lexer->cursor++; lexer->col++; return (Fds_Cfg_Token){TOK_QUESTION, {lexer->src+start_pos,1}, lexer->line, start_col};
            case ':': lexer->cursor++; lexer->col++; return (Fds_Cfg_Token){TOK_COLON, {lexer->src+start_pos,1}, lexer->line, start_col};
            case '$': lexer->cursor++; lexer->col++; return (Fds_Cfg_Token){TOK_DOLLAR, {lexer->src+start_pos,1}, lexer->line, start_col};
            case ',': lexer->cursor++; lexer->col++; return (Fds_Cfg_Token){TOK_COMMA, {lexer->src+start_pos,1}, lexer->line, start_col};
        }

        if (c == '"') {
            lexer->cursor++; lexer->col++;
            size_t str_start = lexer->cursor;
            while (lexer->cursor < lexer->len) {
                if (lexer->src[lexer->cursor] == '\\' && lexer->cursor+1 < lexer->len) {
                    lexer->cursor += 2; lexer->col += 2; continue;
                }
                if (lexer->src[lexer->cursor] == '"') break;
                if (lexer->src[lexer->cursor] == '\n') lexer->line++;
                lexer->cursor++; lexer->col++;
            }
            if (lexer->cursor >= lexer->len)
                return (Fds_Cfg_Token){TOK_ERR, {NULL,0}, lexer->line, start_col};
            size_t str_len = lexer->cursor - str_start;
            lexer->cursor++; lexer->col++;
            return (Fds_Cfg_Token){TOK_STR, {lexer->src+str_start, str_len}, lexer->line, start_col};
        }

        if (c >= '0' && c <= '9') {
            bool is_float = false;
            while (lexer->cursor < lexer->len &&
                   ((lexer->src[lexer->cursor] >= '0' && lexer->src[lexer->cursor] <= '9') ||
                    lexer->src[lexer->cursor] == '.')) {
                if (lexer->src[lexer->cursor] == '.') {
                    if (is_float) break;
                    is_float = true;
                }
                lexer->cursor++; lexer->col++;
            }
            return (Fds_Cfg_Token){is_float ? TOK_FLOAT : TOK_INT,
                                   {lexer->src+start_pos, lexer->cursor-start_pos},
                                   lexer->line, start_col};
        }

        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') {
            while (lexer->cursor < lexer->len &&
                   ((lexer->src[lexer->cursor] >= 'a' && lexer->src[lexer->cursor] <= 'z') ||
                    (lexer->src[lexer->cursor] >= 'A' && lexer->src[lexer->cursor] <= 'Z') ||
                    (lexer->src[lexer->cursor] >= '0' && lexer->src[lexer->cursor] <= '9') ||
                    lexer->src[lexer->cursor] == '_')) {
                lexer->cursor++; lexer->col++;
            }
            fds_string_view lexeme = {lexer->src+start_pos, lexer->cursor-start_pos};
            if (fds__sv_eq(lexeme, fds__sv_from_cstr("def"))) return (Fds_Cfg_Token){TOK_DEF, lexeme, lexer->line, start_col};
            if (fds__sv_eq(lexeme, fds__sv_from_cstr("true")) || fds__sv_eq(lexeme, fds__sv_from_cstr("false")))
                return (Fds_Cfg_Token){TOK_BOOL, lexeme, lexer->line, start_col};
            return (Fds_Cfg_Token){TOK_IDENT, lexeme, lexer->line, start_col};
        }

        lexer->cursor++; lexer->col++;
    }
    return (Fds_Cfg_Token){TOK_EOF, {NULL,0}, lexer->line, lexer->col};
}

/* -------------------------------------------------------------------------- */
/*  Парсер                                                                    */
/* -------------------------------------------------------------------------- */

typedef struct {
    Fds_Cfg_Lexer lexer;
    Fds_Cfg_Token curr;
    Fds_Cfg_Error *err;
} Fds_Cfg_Parser;

static void fds__set_err(Fds_Cfg_Parser *p, Fds_Cfg_ErrCode code, const char *msg) {
    if (p->err && p->err->code == FDS_CFG_OK) {
        p->err->code = code;
        p->err->line = p->curr.line;
        p->err->column = p->curr.col;
        p->err->msg = msg;
    }
}

static void fds__advance(Fds_Cfg_Parser *p) {
    p->curr = fds_cfg_lexer_next(&p->lexer);
    if (p->curr.kind == TOK_ERR)
        fds__set_err(p, FDS_CFG_ERR_SYNTAX, "Лексична помилка");
}

static bool fds__match(Fds_Cfg_Parser *p, Fds_Cfg_TokKind kind) {
    if (p->curr.kind == kind) { fds__advance(p); return true; }
    return false;
}

static Fds_Expr_Node* fds__parse_expression(Fds_Cfg_Parser *p);
static Fds_Ast_Node* fds__parse_struct_body(Fds_Cfg_Parser *p);
static void fds__free_ast(Fds_Ast_Node *node);

static Fds_Ast_Node* fds__new_ast_node(Fds_Ast_Kind kind) {
    Fds_Ast_Node *node = calloc(1, sizeof(Fds_Ast_Node));
    if (node) node->kind = kind;
    return node;
}

static Fds_Expr_Node* fds__new_expr_value(Fds_Cfg_Type type, int64_t i, double f, bool b, fds_string_view s) {
    Fds_Expr_Node *node = calloc(1, sizeof(Fds_Expr_Node));
    if (!node) return NULL;
    node->kind = FDS_EXPR_VALUE;
    node->value_type = type;
    if (type == FDS_CFG_INT) node->value.i = i;
    else if (type == FDS_CFG_FLOAT) node->value.f = f;
    else if (type == FDS_CFG_BOOL) node->value.b = b;
    else if (type == FDS_CFG_STR) node->value.s = s;
    return node;
}

static Fds_Expr_Node* fds__new_expr_var(fds_string_view name) {
    Fds_Expr_Node *node = calloc(1, sizeof(Fds_Expr_Node));
    if (!node) return NULL;
    node->kind = FDS_EXPR_VAR;
    node->var_name = name;
    return node;
}

static Fds_Expr_Node* fds__new_expr_unary(Fds_Cfg_Op op, Fds_Expr_Node *operand) {
    Fds_Expr_Node *node = calloc(1, sizeof(Fds_Expr_Node));
    if (!node) return NULL;
    node->kind = FDS_EXPR_UNARY;
    node->op = op;
    node->left = operand;
    return node;
}

static Fds_Expr_Node* fds__new_expr_binary(Fds_Cfg_Op op, Fds_Expr_Node *left, Fds_Expr_Node *right) {
    Fds_Expr_Node *node = calloc(1, sizeof(Fds_Expr_Node));
    if (!node) return NULL;
    node->kind = FDS_EXPR_BINARY;
    node->op = op;
    node->left = left;
    node->right = right;
    return node;
}

static Fds_Expr_Node* fds__new_expr_cond(Fds_Expr_Node *cond, Fds_Expr_Node *then, Fds_Expr_Node *els) {
    Fds_Expr_Node *node = calloc(1, sizeof(Fds_Expr_Node));
    if (!node) return NULL;
    node->kind = FDS_EXPR_COND;
    node->left = cond;
    node->right = then;
    node->third = els;
    return node;
}

static Fds_Expr_Node* fds__new_expr_call(fds_string_view name, Fds_Expr_Node **args, size_t arg_count) {
    Fds_Expr_Node *node = calloc(1, sizeof(Fds_Expr_Node));
    if (!node) return NULL;
    node->kind = FDS_EXPR_CALL;
    node->func_name = name;
    node->args = args;
    node->arg_count = arg_count;
    return node;
}

static void fds__free_expr(Fds_Expr_Node *node) {
    if (!node) return;
    if (node->path.parts) free(node->path.parts);
    if (node->args) {
        for (size_t i = 0; i < node->arg_count; i++) fds__free_expr(node->args[i]);
        free(node->args);
    }
    fds__free_expr(node->left);
    fds__free_expr(node->right);
    fds__free_expr(node->third);
    free(node);
}

static Fds_Expr_Node* fds__parse_primary(Fds_Cfg_Parser *p) {
    Fds_Expr_Node *node = NULL;
    if (p->curr.kind == TOK_INT) {
        node = fds__new_expr_value(FDS_CFG_INT, fds__sv_to_int(p->curr.lexeme), 0, false, (fds_string_view){0});
        fds__advance(p);
    } else if (p->curr.kind == TOK_FLOAT) {
        node = fds__new_expr_value(FDS_CFG_FLOAT, 0, fds__sv_to_float(p->curr.lexeme), false, (fds_string_view){0});
        fds__advance(p);
    } else if (p->curr.kind == TOK_BOOL) {
        node = fds__new_expr_value(FDS_CFG_BOOL, 0, 0, p->curr.lexeme.data[0]=='t', (fds_string_view){0});
        fds__advance(p);
    } else if (p->curr.kind == TOK_STR) {
        node = fds__new_expr_value(FDS_CFG_STR, 0, 0, false, p->curr.lexeme);
        fds__advance(p);
    } else if (p->curr.kind == TOK_LPAREN) {
        fds__advance(p);
        node = fds__parse_expression(p);
        if (!fds__match(p, TOK_RPAREN)) fds__set_err(p, FDS_CFG_ERR_SYNTAX, "Очікувався ')'");
    } else if (p->curr.kind == TOK_IDENT) {
        fds_string_view name = p->curr.lexeme;
        fds__advance(p);
        if (p->curr.kind == TOK_LPAREN) {
            fds__advance(p);
            Fds_Expr_Node *args[16];
            size_t arg_count = 0;
            if (p->curr.kind != TOK_RPAREN) {
                do {
                    if (arg_count >= sizeof(args) / sizeof(args[0])) {
                        fds__set_err(p, FDS_CFG_ERR_SYNTAX, "Забагато аргументів у виклику функції");
                        return NULL;
                    }
                    args[arg_count++] = fds__parse_expression(p);
                } while (fds__match(p, TOK_COMMA));
            }
            if (!fds__match(p, TOK_RPAREN)) fds__set_err(p, FDS_CFG_ERR_SYNTAX, "Очікувався ')'");
            Fds_Expr_Node **args_copy = NULL;
            if (arg_count > 0) {
                args_copy = malloc(sizeof(Fds_Expr_Node*) * arg_count);
                if (!args_copy) {
                    fds__set_err(p, FDS_CFG_ERR_OOM, "Недостатньо пам'яті для аргументів");
                    return NULL;
                }
                memcpy(args_copy, args, sizeof(Fds_Expr_Node*) * arg_count);
            }
            node = fds__new_expr_call(name, args_copy, arg_count);
        } else {
            if (p->curr.kind == TOK_DOT || p->curr.kind == TOK_AT) {
                fds_string_view parts[32];
                memset(parts, 0, sizeof(parts));  // <-- ініціалізація
                size_t count = 0;
                bool is_root = false;
                int up_count = 0;
                if (p->curr.kind == TOK_AT) {
                    is_root = true;
                    fds__advance(p);
                } else {
                    while (p->curr.kind == TOK_DOT) {
                        up_count++;
                        fds__advance(p);
                    }
                }
                parts[count++] = name;
                while (p->curr.kind == TOK_DOT) {
                    fds__advance(p);
                    if (p->curr.kind == TOK_IDENT) {
                        if (count >= sizeof(parts) / sizeof(parts[0])) {
                            fds__set_err(p, FDS_CFG_ERR_SYNTAX, "Занадто довгий шлях змінної");
                            return NULL;
                        }
                        parts[count++] = p->curr.lexeme;
                        fds__advance(p);
                    } else break;
                }
                node = calloc(1, sizeof(Fds_Expr_Node));
                node->kind = FDS_EXPR_VAR;
                node->var_name = name;
                node->path.is_root = is_root;
                node->path.up_count = up_count;
                node->path.parts = malloc(sizeof(fds_string_view)*count);
                memcpy(node->path.parts, parts, sizeof(fds_string_view)*count);
                node->path.parts_count = count;
            } else {
                node = fds__new_expr_var(name);
            }
        }
    } else if (p->curr.kind == TOK_DOLLAR) {
        fds__advance(p);
        if (p->curr.kind == TOK_IDENT) {
            fds_string_view name = p->curr.lexeme;
            fds__advance(p);

            // Статичні рядки для системних змінних
            static const fds_string_view sys_os    = { "$OS", 3 };
            static const fds_string_view sys_arch  = { "$ARCH", 5 };
            static const fds_string_view sys_threads = { "$THREADS", 8 };

            fds_string_view sys_name;
            if (fds__sv_eq(name, fds__sv_from_cstr("OS"))) {
                sys_name = sys_os;
            } else if (fds__sv_eq(name, fds__sv_from_cstr("ARCH"))) {
                sys_name = sys_arch;
            } else if (fds__sv_eq(name, fds__sv_from_cstr("THREADS"))) {
                sys_name = sys_threads;
            } else {
                fds__set_err(p, FDS_CFG_ERR_SYNTAX, "Невідома системна змінна");
                return NULL;
            }
            node = fds__new_expr_var(sys_name);
        } else {
            fds__set_err(p, FDS_CFG_ERR_SYNTAX, "Очікувався ідентифікатор після $");
        }
    } else {
        fds__set_err(p, FDS_CFG_ERR_SYNTAX, "Неочікуваний токен у виразі");
    }
    return node;
}

static Fds_Expr_Node* fds__parse_unary(Fds_Cfg_Parser *p) {
    if (p->curr.kind == TOK_MINUS || p->curr.kind == TOK_BANG) {
        Fds_Cfg_Op op = (p->curr.kind == TOK_MINUS) ? FDS_CFG_OP_NEG : FDS_CFG_OP_NOT;
        fds__advance(p);
        Fds_Expr_Node *operand = fds__parse_unary(p);
        return fds__new_expr_unary(op, operand);
    }
    return fds__parse_primary(p);
}

static Fds_Expr_Node* fds__parse_mul_div(Fds_Cfg_Parser *p) {
    Fds_Expr_Node *left = fds__parse_unary(p);
    while (p->curr.kind == TOK_STAR || p->curr.kind == TOK_SLASH) {
        Fds_Cfg_Op op = (p->curr.kind == TOK_STAR) ? FDS_CFG_OP_MUL : FDS_CFG_OP_DIV;
        fds__advance(p);
        Fds_Expr_Node *right = fds__parse_unary(p);
        left = fds__new_expr_binary(op, left, right);
    }
    return left;
}

static Fds_Expr_Node* fds__parse_add_sub(Fds_Cfg_Parser *p) {
    Fds_Expr_Node *left = fds__parse_mul_div(p);
    while (p->curr.kind == TOK_PLUS || p->curr.kind == TOK_MINUS) {
        Fds_Cfg_Op op = (p->curr.kind == TOK_PLUS) ? FDS_CFG_OP_ADD : FDS_CFG_OP_SUB;
        fds__advance(p);
        Fds_Expr_Node *right = fds__parse_mul_div(p);
        left = fds__new_expr_binary(op, left, right);
    }
    return left;
}

static Fds_Expr_Node* fds__parse_comparison(Fds_Cfg_Parser *p) {
    Fds_Expr_Node *left = fds__parse_add_sub(p);
    while (p->curr.kind == TOK_EQ || p->curr.kind == TOK_NE || p->curr.kind == TOK_LT ||
           p->curr.kind == TOK_GT || p->curr.kind == TOK_LE || p->curr.kind == TOK_GE) {
        Fds_Cfg_Op op;
        switch (p->curr.kind) {
            case TOK_EQ: op = FDS_CFG_OP_EQ; break;
            case TOK_NE: op = FDS_CFG_OP_NE; break;
            case TOK_LT: op = FDS_CFG_OP_LT; break;
            case TOK_GT: op = FDS_CFG_OP_GT; break;
            case TOK_LE: op = FDS_CFG_OP_LE; break;
            case TOK_GE: op = FDS_CFG_OP_GE; break;
            default: op = FDS_CFG_OP_NONE;
        }
        fds__advance(p);
        Fds_Expr_Node *right = fds__parse_add_sub(p);
        left = fds__new_expr_binary(op, left, right);
    }
    return left;
}

static Fds_Expr_Node* fds__parse_logical_and(Fds_Cfg_Parser *p) {
    Fds_Expr_Node *left = fds__parse_comparison(p);
    while (p->curr.kind == TOK_AND) {
        fds__advance(p);
        Fds_Expr_Node *right = fds__parse_comparison(p);
        left = fds__new_expr_binary(FDS_CFG_OP_AND, left, right);
    }
    return left;
}

static Fds_Expr_Node* fds__parse_logical_or(Fds_Cfg_Parser *p) {
    Fds_Expr_Node *left = fds__parse_logical_and(p);
    while (p->curr.kind == TOK_OR) {
        fds__advance(p);
        Fds_Expr_Node *right = fds__parse_logical_and(p);
        left = fds__new_expr_binary(FDS_CFG_OP_OR, left, right);
    }
    return left;
}

static Fds_Expr_Node* fds__parse_ternary(Fds_Cfg_Parser *p) {
    Fds_Expr_Node *cond = fds__parse_logical_or(p);
    if (p->curr.kind == TOK_QUESTION) {
        fds__advance(p);
        Fds_Expr_Node *then_expr = fds__parse_expression(p);
        if (!fds__match(p, TOK_COLON)) fds__set_err(p, FDS_CFG_ERR_SYNTAX, "Очікувався ':'");
        Fds_Expr_Node *else_expr = fds__parse_ternary(p);
        cond = fds__new_expr_cond(cond, then_expr, else_expr);
    }
    return cond;
}

static Fds_Expr_Node* fds__parse_expression(Fds_Cfg_Parser *p) {
    return fds__parse_ternary(p);
}

static Fds_Ast_Node* fds__parse_struct_body(Fds_Cfg_Parser *p) {
    Fds_Ast_Node *head = NULL, *tail = NULL;
    while (p->curr.kind != TOK_RBRACE && p->curr.kind != TOK_EOF) {
        if (p->err && p->err->code != FDS_CFG_OK) break;

        size_t node_line = p->curr.line;
        size_t node_column = p->curr.col;
        Fds_Ast_Node *node = NULL;
        if (p->curr.kind == TOK_DEF) {
            fds__advance(p);
            if (p->curr.kind != TOK_IDENT) {
                fds__set_err(p, FDS_CFG_ERR_SYNTAX, "Очікувався ім'я функції після def");
                break;
            }
            fds_string_view func_name = p->curr.lexeme;
            fds__advance(p);
            if (!fds__match(p, TOK_LPAREN)) {
                fds__set_err(p, FDS_CFG_ERR_SYNTAX, "Очікувалась '(' після імені функції");
                break;
            }
            fds_string_view params[16];
            memset(params, 0, sizeof(params));  // ініціалізація
            size_t param_count = 0;
            if (p->curr.kind != TOK_RPAREN) {
                do {
                    if (p->curr.kind != TOK_IDENT) {
                        fds__set_err(p, FDS_CFG_ERR_SYNTAX, "Очікувався параметр");
                        break;
                    }
                    if (param_count >= sizeof(params) / sizeof(params[0])) {
                        fds__set_err(p, FDS_CFG_ERR_SYNTAX, "Забагато параметрів у функції");
                        break;
                    }
                    params[param_count++] = p->curr.lexeme;
                    fds__advance(p);
                } while (fds__match(p, TOK_COMMA));
            }
            if (!fds__match(p, TOK_RPAREN)) {
                fds__set_err(p, FDS_CFG_ERR_SYNTAX, "Очікувалась ')'");
                break;
            }
            if (!fds__match(p, TOK_ASSIGN)) {
                fds__set_err(p, FDS_CFG_ERR_SYNTAX, "Очікувався '=' після параметрів функції");
                break;
            }
            Fds_Expr_Node *body = fds__parse_expression(p);
            fds__match(p, TOK_SEMI);

            node = calloc(1, sizeof(Fds_Ast_Node));
            node->kind = FDS_AST_DEF;
            node->def.name = func_name;
            node->def.params = malloc(sizeof(fds_string_view)*param_count);
            if (node->def.params) {
                memcpy(node->def.params, params, sizeof(fds_string_view)*param_count);
            }
            node->def.param_count = param_count;
            node->def.body = body;
        } else if (p->curr.kind == TOK_AT) {
            fds__advance(p);
            if (p->curr.kind != TOK_IDENT) {
                fds__set_err(p, FDS_CFG_ERR_SYNTAX, "Очікувалась директива після '@'");
                break;
            }

            fds_string_view directive = p->curr.lexeme;
            fds__advance(p);
            if (fds__sv_eq(directive, fds__sv_from_cstr("if"))) {
                Fds_Expr_Node *condition = fds__parse_expression(p);
                if (!fds__match(p, TOK_LBRACE)) {
                    fds__set_err(p, FDS_CFG_ERR_SYNTAX, "Очікувалась '{' після умови");
                    fds__free_expr(condition);
                    break;
                }

                Fds_Ast_Node *then_branch = fds__parse_struct_body(p);
                if (!fds__match(p, TOK_RBRACE)) {
                    fds__set_err(p, FDS_CFG_ERR_SYNTAX, "Очікувалась '}' після @if");
                    fds__free_ast(then_branch);
                    fds__free_expr(condition);
                    break;
                }

                Fds_Ast_Node *else_branch = NULL;
                if (p->curr.kind == TOK_AT) {
                    fds__advance(p);
                    if (p->curr.kind != TOK_IDENT ||
                        !fds__sv_eq(p->curr.lexeme, fds__sv_from_cstr("else"))) {
                        fds__set_err(p, FDS_CFG_ERR_SYNTAX, "Очікувався '@else' після @if");
                        fds__free_ast(then_branch);
                        fds__free_expr(condition);
                        break;
                    }
                    fds__advance(p);
                    if (!fds__match(p, TOK_LBRACE)) {
                        fds__set_err(p, FDS_CFG_ERR_SYNTAX, "Очікувалась '{' після @else");
                        fds__free_ast(then_branch);
                        fds__free_expr(condition);
                        break;
                    }
                    else_branch = fds__parse_struct_body(p);
                    if (!fds__match(p, TOK_RBRACE)) {
                        fds__set_err(p, FDS_CFG_ERR_SYNTAX, "Очікувалась '}' після @else");
                        fds__free_ast(then_branch);
                        fds__free_ast(else_branch);
                        fds__free_expr(condition);
                        break;
                    }
                }

                node = fds__new_ast_node(FDS_AST_IF);
                if (!node) {
                    fds__free_ast(then_branch);
                    fds__free_ast(else_branch);
                    fds__free_expr(condition);
                    fds__set_err(p, FDS_CFG_ERR_OOM, "Недостатньо пам'яті для @if");
                    break;
                }
                node->if_block.condition = condition;
                node->if_block.then_branch = then_branch;
                node->if_block.else_branch = else_branch;
            } else if (fds__sv_eq(directive, fds__sv_from_cstr("assert"))) {
                Fds_Expr_Node *condition = fds__parse_expression(p);
                fds_string_view message = fds__sv_from_cstr("Перевірка конфігурації не пройдена");
                if (fds__match(p, TOK_COMMA)) {
                    if (p->curr.kind != TOK_STR) {
                        fds__set_err(p, FDS_CFG_ERR_SYNTAX, "Очікувався рядок повідомлення після ','");
                        fds__free_expr(condition);
                        break;
                    }
                    message = p->curr.lexeme;
                    fds__advance(p);
                }
                if (!fds__match(p, TOK_SEMI)) {
                    fds__set_err(p, FDS_CFG_ERR_SYNTAX, "Очікувалась ';' після @assert");
                    fds__free_expr(condition);
                    break;
                }
                node = fds__new_ast_node(FDS_AST_ASSERT);
                if (!node) {
                    fds__free_expr(condition);
                    fds__set_err(p, FDS_CFG_ERR_OOM, "Недостатньо пам'яті для @assert");
                    break;
                }
                node->assert.condition = condition;
                node->assert.message = message;
            } else {
                fds__set_err(p, FDS_CFG_ERR_SYNTAX, "Невідома директива");
                break;
            }
        } else if (p->curr.kind == TOK_IDENT) {
            fds_string_view key = p->curr.lexeme;
            fds__advance(p);
            if (!fds__match(p, TOK_ASSIGN)) {
                fds__set_err(p, FDS_CFG_ERR_SYNTAX, "Очікувався '=' після імені поля");
                break;
            }
            Fds_Expr_Node *expr = fds__parse_expression(p);
            fds__match(p, TOK_SEMI);
            node = calloc(1, sizeof(Fds_Ast_Node));
            node->kind = FDS_AST_FIELD;
            node->field.key = key;
            node->field.expr = expr;
        } else {
            fds__set_err(p, FDS_CFG_ERR_SYNTAX, "Неочікуваний токен на верхньому рівні");
            break;
        }

        if (node) {
            node->line = node_line;
            node->column = node_column;
            if (tail) tail->next = node;
            else head = node;
            tail = node;
        }
    }
    return head;
}

static void fds__free_ast(Fds_Ast_Node *node) {
    while (node) {
        Fds_Ast_Node *next = node->next;
        if (node->kind == FDS_AST_FIELD) {
            fds__free_expr(node->field.expr);
        } else if (node->kind == FDS_AST_DEF) {
            free(node->def.params);
            fds__free_expr(node->def.body);
        } else if (node->kind == FDS_AST_IF) {
            fds__free_expr(node->if_block.condition);
            fds__free_ast(node->if_block.then_branch);
            fds__free_ast(node->if_block.else_branch);
        } else if (node->kind == FDS_AST_ASSERT) {
            fds__free_expr(node->assert.condition);
        }
        free(node);
        node = next;
    }
}

/* -------------------------------------------------------------------------- */
/*  Evaluation                                                                 */
/* -------------------------------------------------------------------------- */

static Fds_Cfg_ErrCode fds__eval_expr(Fds_Expr_Node *expr, Fds_Cfg_Env *env, Fds_Func_Table *funcs, Fds_Cfg_Var *result);
static Fds_Cfg_ErrCode fds__eval_ast(Fds_Ast_Node *ast, Fds_Cfg_Env *env, Fds_Func_Table *funcs, const char *prefix, Fds_Flat_Entry **out_entries, size_t *out_count, size_t *out_cap, Fds_Cfg_Error *out_err);

static void fds__env_add(Fds_Cfg_Env *env, fds_string_view name, Fds_Cfg_Type type, int64_t i, double f, bool b, fds_string_view s, bool owns_string) {
    if (env->count >= env->cap) {
        env->cap = env->cap ? env->cap*2 : 16;
        env->items = realloc(env->items, sizeof(Fds_Cfg_Var)*env->cap);
    }
    env->items[env->count].name = name;
    env->items[env->count].type = type;
    env->items[env->count].owns_string = owns_string;
    if (type == FDS_CFG_INT) env->items[env->count].value.i = i;
    else if (type == FDS_CFG_FLOAT) env->items[env->count].value.f = f;
    else if (type == FDS_CFG_BOOL) env->items[env->count].value.b = b;
    else if (type == FDS_CFG_STR) env->items[env->count].value.s = s;
    env->count++;
}

static Fds_Cfg_Var* fds__env_find(Fds_Cfg_Env *env, fds_string_view name) {
    for (size_t i = 0; i < env->count; i++) {
        if (fds__sv_eq(env->items[i].name, name)) return &env->items[i];
    }
    return NULL;
}

static void fds__env_free(Fds_Cfg_Env *env) {
    for (size_t i = 0; i < env->count; i++) {
        if (env->items[i].owns_string && env->items[i].type == FDS_CFG_STR) {
            free((void*)env->items[i].value.s.data);
        }
    }
    free(env->items);
}

static void fds__func_add(Fds_Func_Table *table, fds_string_view name, fds_string_view *params, size_t param_count, Fds_Expr_Node *body) {
    if (table->count >= table->cap) {
        table->cap = table->cap ? table->cap*2 : 8;
        table->items = realloc(table->items, sizeof(Fds_Cfg_Func)*table->cap);
    }
    table->items[table->count].name = name;
    table->items[table->count].param_count = param_count;
    table->items[table->count].body = body;
    if (param_count > 0) {
        table->items[table->count].params = malloc(sizeof(fds_string_view) * param_count);
        if (table->items[table->count].params) {
            memcpy(table->items[table->count].params, params, sizeof(fds_string_view) * param_count);
        } else {
            // У випадку невдачі залишимо NULL, але це вже критична помилка
        }
    } else {
        table->items[table->count].params = NULL;
    }
    table->count++;
}

static Fds_Cfg_Func* fds__func_find(Fds_Func_Table *table, fds_string_view name) {
    for (size_t i = 0; i < table->count; i++) {
        if (fds__sv_eq(table->items[i].name, name)) return &table->items[i];
    }
    return NULL;
}

static void fds__func_table_free(Fds_Func_Table *table) {
    for (size_t i = 0; i < table->count; i++) {
        free(table->items[i].params);
    }
    free(table->items);
}

static void fds__env_init_system(Fds_Cfg_Env *env) {
#ifdef _WIN32
    fds__env_add(env, fds__sv_from_cstr("$OS"), FDS_CFG_STR, 0,0,false, fds__sv_from_cstr("windows"), false);
#else
    fds__env_add(env, fds__sv_from_cstr("$OS"), FDS_CFG_STR, 0,0,false, fds__sv_from_cstr("linux"), false);
#endif
    fds__env_add(env, fds__sv_from_cstr("$ARCH"), FDS_CFG_STR, 0,0,false, fds__sv_from_cstr("x86_64"), false);
    fds__env_add(env, fds__sv_from_cstr("$THREADS"), FDS_CFG_INT, 8,0,false, (fds_string_view){0}, false);
}

static Fds_Cfg_ErrCode fds__eval_expr(Fds_Expr_Node *expr, Fds_Cfg_Env *env, Fds_Func_Table *funcs, Fds_Cfg_Var *result) {
    if (!expr) return FDS_CFG_ERR_SYNTAX;

    switch (expr->kind) {
        case FDS_EXPR_VALUE:
            result->type = expr->value_type;
            result->owns_string = false;
            if (expr->value_type == FDS_CFG_INT) result->value.i = expr->value.i;
            else if (expr->value_type == FDS_CFG_FLOAT) result->value.f = expr->value.f;
            else if (expr->value_type == FDS_CFG_BOOL) result->value.b = expr->value.b;
            else if (expr->value_type == FDS_CFG_STR) result->value.s = expr->value.s;
            break;

        case FDS_EXPR_VAR: {
            Fds_Cfg_Var *var = fds__env_find(env, expr->var_name);
            if (!var) return FDS_CFG_ERR_UNDEFINED_VAR;
            *result = *var;
            if (result->type == FDS_CFG_STR) result->owns_string = false;
            break;
        }

        case FDS_EXPR_UNARY: {
            Fds_Cfg_Var operand;
            Fds_Cfg_ErrCode err = fds__eval_expr(expr->left, env, funcs, &operand);
            if (err) return err;
            if (expr->op == FDS_CFG_OP_NEG) {
                if (operand.type == FDS_CFG_INT) { result->type = FDS_CFG_INT; result->value.i = -operand.value.i; }
                else if (operand.type == FDS_CFG_FLOAT) { result->type = FDS_CFG_FLOAT; result->value.f = -operand.value.f; }
                else return FDS_CFG_ERR_TYPE_MISMATCH;
            } else if (expr->op == FDS_CFG_OP_NOT) {
                if (operand.type != FDS_CFG_BOOL) return FDS_CFG_ERR_TYPE_MISMATCH;
                result->type = FDS_CFG_BOOL;
                result->value.b = !operand.value.b;
            }
            result->owns_string = false;
            break;
        }

        case FDS_EXPR_BINARY: {
            Fds_Cfg_Var left, right;
            Fds_Cfg_ErrCode err = fds__eval_expr(expr->left, env, funcs, &left);
            if (err) return err;
            err = fds__eval_expr(expr->right, env, funcs, &right);
            if (err) return err;

            if (expr->op == FDS_CFG_OP_ADD || expr->op == FDS_CFG_OP_SUB ||
                expr->op == FDS_CFG_OP_MUL || expr->op == FDS_CFG_OP_DIV) {
                if (left.type == FDS_CFG_INT && right.type == FDS_CFG_INT) {
                    result->type = FDS_CFG_INT;
                    switch (expr->op) {
                        case FDS_CFG_OP_ADD: result->value.i = left.value.i + right.value.i; break;
                        case FDS_CFG_OP_SUB: result->value.i = left.value.i - right.value.i; break;
                        case FDS_CFG_OP_MUL: result->value.i = left.value.i * right.value.i; break;
                        case FDS_CFG_OP_DIV:
                            if (right.value.i == 0) return FDS_CFG_ERR_DIV_BY_ZERO;
                            result->value.i = left.value.i / right.value.i; break;
                        default: break;
                    }
                    result->owns_string = false;
                } else if ((left.type == FDS_CFG_FLOAT || left.type == FDS_CFG_INT) &&
                           (right.type == FDS_CFG_FLOAT || right.type == FDS_CFG_INT)) {
                    double l = left.type==FDS_CFG_FLOAT ? left.value.f : (double)left.value.i;
                    double r = right.type==FDS_CFG_FLOAT ? right.value.f : (double)right.value.i;
                    result->type = FDS_CFG_FLOAT;
                    switch (expr->op) {
                        case FDS_CFG_OP_ADD: result->value.f = l + r; break;
                        case FDS_CFG_OP_SUB: result->value.f = l - r; break;
                        case FDS_CFG_OP_MUL: result->value.f = l * r; break;
                        case FDS_CFG_OP_DIV:
                            if (r == 0.0) return FDS_CFG_ERR_DIV_BY_ZERO;
                            result->value.f = l / r; break;
                        default: break;
                    }
                    result->owns_string = false;
                } else if (left.type == FDS_CFG_STR && right.type == FDS_CFG_STR && expr->op == FDS_CFG_OP_ADD) {
                    size_t len = left.value.s.len + right.value.s.len;
                    char *buf = malloc(len + 1);
                    if (!buf) return FDS_CFG_ERR_OOM;
                    memcpy(buf, left.value.s.data, left.value.s.len);
                    memcpy(buf+left.value.s.len, right.value.s.data, right.value.s.len);
                    buf[len] = '\0';
                    result->type = FDS_CFG_STR;
                    result->value.s = (fds_string_view){buf, len};
                    result->owns_string = true;
                } else {
                    return FDS_CFG_ERR_TYPE_MISMATCH;
                }
            }
            else if (expr->op >= FDS_CFG_OP_EQ && expr->op <= FDS_CFG_OP_GE) {
                bool res = false;
                if (left.type == FDS_CFG_INT && right.type == FDS_CFG_INT) {
                    int64_t l = left.value.i, r = right.value.i;
                    switch (expr->op) {
                        case FDS_CFG_OP_EQ: res = (l==r); break;
                        case FDS_CFG_OP_NE: res = (l!=r); break;
                        case FDS_CFG_OP_LT: res = (l<r); break;
                        case FDS_CFG_OP_GT: res = (l>r); break;
                        case FDS_CFG_OP_LE: res = (l<=r); break;
                        case FDS_CFG_OP_GE: res = (l>=r); break;
                        default: break;
                    }
                } else if ((left.type==FDS_CFG_FLOAT||left.type==FDS_CFG_INT) &&
                           (right.type==FDS_CFG_FLOAT||right.type==FDS_CFG_INT)) {
                    double l = left.type==FDS_CFG_FLOAT ? left.value.f : (double)left.value.i;
                    double r = right.type==FDS_CFG_FLOAT ? right.value.f : (double)right.value.i;
                    switch (expr->op) {
                        case FDS_CFG_OP_EQ: res = (l==r); break;
                        case FDS_CFG_OP_NE: res = (l!=r); break;
                        case FDS_CFG_OP_LT: res = (l<r); break;
                        case FDS_CFG_OP_GT: res = (l>r); break;
                        case FDS_CFG_OP_LE: res = (l<=r); break;
                        case FDS_CFG_OP_GE: res = (l>=r); break;
                        default: break;
                    }
                } else if (left.type == FDS_CFG_STR && right.type == FDS_CFG_STR) {
                    int cmp = memcmp(left.value.s.data, right.value.s.data,
                                     left.value.s.len < right.value.s.len ? left.value.s.len : right.value.s.len);
                    if (cmp == 0 && left.value.s.len != right.value.s.len) cmp = left.value.s.len < right.value.s.len ? -1 : 1;
                    switch (expr->op) {
                        case FDS_CFG_OP_EQ: res = (cmp==0); break;
                        case FDS_CFG_OP_NE: res = (cmp!=0); break;
                        case FDS_CFG_OP_LT: res = (cmp<0); break;
                        case FDS_CFG_OP_GT: res = (cmp>0); break;
                        case FDS_CFG_OP_LE: res = (cmp<=0); break;
                        case FDS_CFG_OP_GE: res = (cmp>=0); break;
                        default: break;
                    }
                } else if (left.type == FDS_CFG_BOOL && right.type == FDS_CFG_BOOL) {
                    bool l = left.value.b, r = right.value.b;
                    if (expr->op == FDS_CFG_OP_EQ) res = (l==r);
                    else if (expr->op == FDS_CFG_OP_NE) res = (l!=r);
                    else return FDS_CFG_ERR_TYPE_MISMATCH;
                } else return FDS_CFG_ERR_TYPE_MISMATCH;
                result->type = FDS_CFG_BOOL;
                result->value.b = res;
                result->owns_string = false;
            }
            else if (expr->op == FDS_CFG_OP_AND || expr->op == FDS_CFG_OP_OR) {
                if (left.type != FDS_CFG_BOOL || right.type != FDS_CFG_BOOL) return FDS_CFG_ERR_TYPE_MISMATCH;
                result->type = FDS_CFG_BOOL;
                if (expr->op == FDS_CFG_OP_AND) result->value.b = left.value.b && right.value.b;
                else result->value.b = left.value.b || right.value.b;
                result->owns_string = false;
            }
            break;
        }

        case FDS_EXPR_COND: {
            Fds_Cfg_Var cond;
            Fds_Cfg_ErrCode err = fds__eval_expr(expr->left, env, funcs, &cond);
            if (err) return err;
            if (cond.type != FDS_CFG_BOOL) return FDS_CFG_ERR_TYPE_MISMATCH;
            if (cond.value.b) return fds__eval_expr(expr->right, env, funcs, result);
            else return fds__eval_expr(expr->third, env, funcs, result);
        }

        case FDS_EXPR_CALL: {
            Fds_Cfg_Func *func = fds__func_find(funcs, expr->func_name);
            if (!func) return FDS_CFG_ERR_UNDEFINED_VAR;
            if (func->param_count != expr->arg_count) return FDS_CFG_ERR_SYNTAX;
            Fds_Cfg_Env local_env = {0};
            local_env.items = malloc(sizeof(Fds_Cfg_Var) * func->param_count);
            local_env.cap = func->param_count;
            local_env.count = 0;
            for (size_t i = 0; i < func->param_count; i++) {
                Fds_Cfg_Var arg_val;
                Fds_Cfg_ErrCode err = fds__eval_expr(expr->args[i], env, funcs, &arg_val);
                if (err) { free(local_env.items); return err; }
                fds__env_add(&local_env, func->params[i], arg_val.type, arg_val.value.i, arg_val.value.f, arg_val.value.b, arg_val.value.s, arg_val.owns_string);
            }
            Fds_Cfg_Var res;
            Fds_Cfg_ErrCode err = fds__eval_expr(func->body, &local_env, funcs, &res);
            if (err == FDS_CFG_OK && res.type == FDS_CFG_STR) {
                char *copy = malloc(res.value.s.len + 1);
                if (!copy) {
                    fds__env_free(&local_env);
                    return FDS_CFG_ERR_OOM;
                }
                memcpy(copy, res.value.s.data, res.value.s.len);
                copy[res.value.s.len] = '\0';
                res.value.s.data = copy;
                res.owns_string = true;
            }
            fds__env_free(&local_env);
            if (err) return err;
            *result = res;
            break;
        }
    }
    return FDS_CFG_OK;
}

static Fds_Cfg_ErrCode fds__eval_ast(Fds_Ast_Node *ast, Fds_Cfg_Env *env, Fds_Func_Table *funcs, const char *prefix,
                                      Fds_Flat_Entry **out_entries, size_t *out_count, size_t *out_cap,
                                      Fds_Cfg_Error *out_err) {
    Fds_Cfg_ErrCode err = FDS_CFG_OK;
    for (Fds_Ast_Node *node = ast; node; node = node->next) {
        switch (node->kind) {
            case FDS_AST_FIELD: {
                Fds_Cfg_Var val;
                err = fds__eval_expr(node->field.expr, env, funcs, &val);
                if (err) {
                    if (out_err && out_err->code == FDS_CFG_OK) {
                        out_err->code = err;
                        out_err->line = node->line;
                        out_err->column = node->column;
                    }
                    return err;
                }
                fds__env_add(env, node->field.key, val.type, val.value.i, val.value.f, val.value.b, val.value.s, val.owns_string);
                size_t key_len = (prefix ? strlen(prefix) + 1 : 0) + node->field.key.len;
                char *key = malloc(key_len + 1);
                if (prefix) {
                    sprintf(key, "%s.", prefix);
                    strncat(key, node->field.key.data, node->field.key.len);
                } else {
                    strncpy(key, node->field.key.data, node->field.key.len);
                    key[node->field.key.len] = '\0';
                }
                if (*out_count >= *out_cap) {
                    *out_cap = *out_cap ? *out_cap*2 : 16;
                    *out_entries = realloc(*out_entries, sizeof(Fds_Flat_Entry)*(*out_cap));
                }
                Fds_Flat_Entry *entry = &(*out_entries)[*out_count];
                entry->key = key;
                entry->type = val.type;
                if (val.type == FDS_CFG_INT) entry->value.i = val.value.i;
                else if (val.type == FDS_CFG_FLOAT) entry->value.f = val.value.f;
                else if (val.type == FDS_CFG_BOOL) entry->value.b = val.value.b;
                else if (val.type == FDS_CFG_STR) {
                    entry->value.s = strndup(val.value.s.data, val.value.s.len);
                }
                (*out_count)++;
                break;
            }
            case FDS_AST_IF: {
                Fds_Cfg_Var condition;
                err = fds__eval_expr(node->if_block.condition, env, funcs, &condition);
                if (err) {
                    if (out_err && out_err->code == FDS_CFG_OK) {
                        out_err->code = err;
                        out_err->line = node->line;
                        out_err->column = node->column;
                    }
                    return err;
                }
                if (condition.type != FDS_CFG_BOOL) {
                    if (out_err && out_err->code == FDS_CFG_OK) {
                        out_err->code = FDS_CFG_ERR_TYPE_MISMATCH;
                        out_err->line = node->line;
                        out_err->column = node->column;
                    }
                    return FDS_CFG_ERR_TYPE_MISMATCH;
                }
                Fds_Ast_Node *branch = condition.value.b
                    ? node->if_block.then_branch
                    : node->if_block.else_branch;
                err = fds__eval_ast(branch, env, funcs, prefix,
                                     out_entries, out_count, out_cap, out_err);
                if (err) return err;
                break;
            }
            case FDS_AST_ASSERT: {
                Fds_Cfg_Var condition;
                err = fds__eval_expr(node->assert.condition, env, funcs, &condition);
                if (err) {
                    if (out_err && out_err->code == FDS_CFG_OK) {
                        out_err->code = err;
                        out_err->line = node->line;
                        out_err->column = node->column;
                    }
                    return err;
                }
                if (condition.type != FDS_CFG_BOOL) {
                    if (out_err && out_err->code == FDS_CFG_OK) {
                        out_err->code = FDS_CFG_ERR_TYPE_MISMATCH;
                        out_err->line = node->line;
                        out_err->column = node->column;
                    }
                    return FDS_CFG_ERR_TYPE_MISMATCH;
                }
                if (!condition.value.b) {
                    if (out_err && out_err->code == FDS_CFG_OK) {
                        out_err->code = FDS_CFG_ERR_ASSERT_FAILED;
                        out_err->line = node->line;
                        out_err->column = node->column;
                    }
                    return FDS_CFG_ERR_ASSERT_FAILED;
                }
                break;
            }
            default:
                break;
        }
    }
    return err;
}

/* -------------------------------------------------------------------------- */
/*  Пакування                                                                  */
/* -------------------------------------------------------------------------- */

static int fds__flat_entry_cmp(const void *a, const void *b) {
    const Fds_Flat_Entry *ea = (const Fds_Flat_Entry*)a;
    const Fds_Flat_Entry *eb = (const Fds_Flat_Entry*)b;
    return strcmp(ea->key, eb->key);
}

static Fds_Cfg_Doc* fds__pack_entries(Fds_Flat_Entry *entries, size_t count) {
    qsort(entries, count, sizeof(Fds_Flat_Entry), fds__flat_entry_cmp);

    size_t strings_size = 0;
    for (size_t i = 0; i < count; i++) {
        strings_size += strlen(entries[i].key) + 1;
        if (entries[i].type == FDS_CFG_STR) {
            strings_size += strlen(entries[i].value.s) + 1;
        }
    }

    size_t entries_size = sizeof(Fds_Blob_Entry) * count;
    size_t total_size = sizeof(Fds_Blob_Header) + entries_size + strings_size;

    Fds_Cfg_Doc *doc = malloc(sizeof(Fds_Cfg_Doc));
    if (!doc) return NULL;
    doc->blob = malloc(total_size);
    if (!doc->blob) { free(doc); return NULL; }
    doc->blob_size = total_size;

    Fds_Blob_Header *hdr = (Fds_Blob_Header*)doc->blob;
    hdr->magic = FDS_CFG_MAGIC;
    hdr->version = FDS_CFG_VERSION;
    hdr->total_size = (uint32_t)total_size;
    hdr->entry_count = (uint32_t)count;
    hdr->entries_offset = sizeof(Fds_Blob_Header);
    hdr->strings_offset = hdr->entries_offset + (uint32_t)entries_size;

    Fds_Blob_Entry *blob_entries = (Fds_Blob_Entry*)((char*)doc->blob + hdr->entries_offset);
    char *str_pool = (char*)doc->blob + hdr->strings_offset;
    size_t str_offset = 0;

    for (size_t i = 0; i < count; i++) {
        Fds_Flat_Entry *e = &entries[i];
        size_t key_len = strlen(e->key);
        memcpy(str_pool + str_offset, e->key, key_len + 1);
        blob_entries[i].name_offset = (uint32_t)str_offset;
        str_offset += key_len + 1;

        blob_entries[i].type = (uint32_t)e->type;
        if (e->type == FDS_CFG_INT) {
            blob_entries[i].as.i_val = e->value.i;
        } else if (e->type == FDS_CFG_FLOAT) {
            blob_entries[i].as.f_val = e->value.f;
        } else if (e->type == FDS_CFG_BOOL) {
            blob_entries[i].as.b_val = (uint8_t)e->value.b;
        } else if (e->type == FDS_CFG_STR) {
            size_t s_len = strlen(e->value.s);
            memcpy(str_pool + str_offset, e->value.s, s_len + 1);
            blob_entries[i].as.data_offset = (uint32_t)str_offset;
            str_offset += s_len + 1;
        }
    }

    for (size_t i = 0; i < count; i++) {
        free(entries[i].key);
        if (entries[i].type == FDS_CFG_STR) free(entries[i].value.s);
    }
    free(entries);

    return doc;
}

/* -------------------------------------------------------------------------- */
/*  Публічні функції                                                           */
/* -------------------------------------------------------------------------- */

Fds_Cfg_Doc* fds_cfg_parse(fds_string_view source, Fds_Cfg_Error *out_err) {
    if (out_err) { out_err->code = FDS_CFG_OK; out_err->msg = NULL; }

    Fds_Cfg_Parser parser = {0};
    parser.lexer.src = source.data;
    parser.lexer.len = source.len;
    parser.lexer.line = 1;
    parser.lexer.col = 1;
    parser.err = out_err;

    fds__advance(&parser);
    Fds_Ast_Node *ast = fds__parse_struct_body(&parser);
    if (out_err && out_err->code != FDS_CFG_OK) {
        fds__free_ast(ast);
        return NULL;
    }

    Fds_Cfg_Env env = {0};
    fds__env_init_system(&env);

    Fds_Func_Table funcs = {0};

    Fds_Flat_Entry *entries = NULL;
    size_t entry_count = 0, entry_cap = 0;

    /* Збираємо всі def у таблицю функцій (одноразово) */
    for (Fds_Ast_Node *n = ast; n; n = n->next) {
        if (n->kind == FDS_AST_DEF) {
            fds__func_add(&funcs, n->def.name, n->def.params, n->def.param_count, n->def.body);
        }
    }

    Fds_Cfg_ErrCode err = fds__eval_ast(ast, &env, &funcs, NULL, &entries, &entry_count, &entry_cap, out_err);
    if (err != FDS_CFG_OK) {
        if (out_err) {
            out_err->code = err;
            switch (err) {
                case FDS_CFG_ERR_TYPE_MISMATCH: out_err->msg = "Несумісні типи у виразі"; break;
                case FDS_CFG_ERR_UNDEFINED_VAR: out_err->msg = "Невизначена змінна"; break;
                case FDS_CFG_ERR_DIV_BY_ZERO: out_err->msg = "Ділення на нуль"; break;
                case FDS_CFG_ERR_ASSERT_FAILED: out_err->msg = "Перевірка конфігурації не пройдена"; break;
                case FDS_CFG_ERR_OOM: out_err->msg = "Недостатньо пам'яті"; break;
                default: out_err->msg = "Помилка виконання конфігурації"; break;
            }
        }
        fds__env_free(&env);
        fds__func_table_free(&funcs);
        if (entries) {
            for (size_t i = 0; i < entry_count; i++) {
                free(entries[i].key);
                if (entries[i].type == FDS_CFG_STR) free(entries[i].value.s);
            }
            free(entries);
        }
        fds__free_ast(ast);
        return NULL;
    }

    Fds_Cfg_Doc *doc = fds__pack_entries(entries, entry_count);
    if (!doc && out_err) {
        out_err->code = FDS_CFG_ERR_OOM;
        out_err->msg = "Недостатньо пам'яті для створення блобу";
    }

    fds__env_free(&env);
    fds__func_table_free(&funcs);
    fds__free_ast(ast);

    return doc;
}

Fds_Cfg_Doc* fds_cfg_load(const char *filename, Fds_Cfg_Error *out_err) {
    FILE *f = fopen(filename, "rb");
    if (!f) {
        if (out_err) { out_err->code = FDS_CFG_ERR_RUNTIME; out_err->msg = "Не вдалося відкрити файл"; }
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(size);
    if (!buf) {
        fclose(f);
        if (out_err) { out_err->code = FDS_CFG_ERR_OOM; out_err->msg = "Недостатньо пам'яті"; }
        return NULL;
    }
    fread(buf, 1, size, f);
    fclose(f);

    Fds_Cfg_Doc *doc = fds_cfg_parse((fds_string_view){buf, size}, out_err);
    free(buf);
    return doc;
}

Fds_Cfg_Doc* fds_cfg_load_binary(const void *data, size_t size, Fds_Cfg_Error *out_err) {
    if (!data || size < sizeof(Fds_Blob_Header)) {
        if (out_err) { out_err->code = FDS_CFG_ERR_SYNTAX; out_err->msg = "Замалий розмір блобу"; }
        return NULL;
    }
    const Fds_Blob_Header *hdr = (const Fds_Blob_Header*)data;
    if (hdr->magic != FDS_CFG_MAGIC || hdr->version != FDS_CFG_VERSION ||
        (size_t)hdr->total_size != size || hdr->total_size < sizeof(Fds_Blob_Header)) {
        if (out_err) { out_err->code = FDS_CFG_ERR_SYNTAX; out_err->msg = "Невірний формат блобу"; }
        return NULL;
    }

    size_t entries_offset = hdr->entries_offset;
    size_t strings_offset = hdr->strings_offset;
    size_t entry_count = hdr->entry_count;
    if (entries_offset < sizeof(Fds_Blob_Header) || entries_offset > size ||
        entry_count > (size - entries_offset) / sizeof(Fds_Blob_Entry)) {
        if (out_err) { out_err->code = FDS_CFG_ERR_SYNTAX; out_err->msg = "Невірний діапазон записів блобу"; }
        return NULL;
    }

    size_t entries_end = entries_offset + entry_count * sizeof(Fds_Blob_Entry);
    if (strings_offset < entries_end || strings_offset > size) {
        if (out_err) { out_err->code = FDS_CFG_ERR_SYNTAX; out_err->msg = "Невірний пул рядків блобу"; }
        return NULL;
    }

    const Fds_Blob_Entry *entries = (const Fds_Blob_Entry*)((const char*)data + entries_offset);
    const char *strings = (const char*)data + strings_offset;
    size_t strings_size = size - strings_offset;
    for (size_t i = 0; i < entry_count; i++) {
        if (entries[i].type < FDS_CFG_INT || entries[i].type > FDS_CFG_STR ||
            entries[i].name_offset >= strings_size ||
            !memchr(strings + entries[i].name_offset, '\0', strings_size - entries[i].name_offset)) {
            if (out_err) { out_err->code = FDS_CFG_ERR_SYNTAX; out_err->msg = "Невірний запис блобу"; }
            return NULL;
        }
        if (entries[i].type == FDS_CFG_STR &&
            (entries[i].as.data_offset >= strings_size ||
             !memchr(strings + entries[i].as.data_offset, '\0', strings_size - entries[i].as.data_offset))) {
            if (out_err) { out_err->code = FDS_CFG_ERR_SYNTAX; out_err->msg = "Невірний рядок блобу"; }
            return NULL;
        }
    }

    Fds_Cfg_Doc *doc = malloc(sizeof(Fds_Cfg_Doc));
    if (!doc) {
        if (out_err) { out_err->code = FDS_CFG_ERR_OOM; out_err->msg = "Недостатньо пам'яті"; }
        return NULL;
    }
    doc->blob = malloc(size);
    if (!doc->blob) {
        free(doc);
        if (out_err) { out_err->code = FDS_CFG_ERR_OOM; out_err->msg = "Недостатньо пам'яті"; }
        return NULL;
    }
    memcpy(doc->blob, data, size);
    doc->blob_size = size;
    return doc;
}

void fds_cfg_free(Fds_Cfg_Doc *doc) {
    if (!doc) return;
    free(doc->blob);
    free(doc);
}

static const Fds_Blob_Entry* fds__blob_find(const Fds_Cfg_Doc *doc, const char *key) {
    if (!doc || !doc->blob) return NULL;
    const Fds_Blob_Header *hdr = (const Fds_Blob_Header*)doc->blob;
    const Fds_Blob_Entry *entries = (const Fds_Blob_Entry*)((const char*)doc->blob + hdr->entries_offset);
    const char *str_pool = (const char*)doc->blob + hdr->strings_offset;

    int left = 0, right = (int)hdr->entry_count - 1;
    while (left <= right) {
        int mid = left + (right - left) / 2;
        const char *name = str_pool + entries[mid].name_offset;
        int cmp = strcmp(key, name);
        if (cmp == 0) return &entries[mid];
        else if (cmp < 0) right = mid - 1;
        else left = mid + 1;
    }
    return NULL;
}

bool fds_cfg_has_key(const Fds_Cfg_Doc *doc, const char *key) {
    return fds__blob_find(doc, key) != NULL;
}

Fds_Cfg_Type fds_cfg_get_type(const Fds_Cfg_Doc *doc, const char *key) {
    const Fds_Blob_Entry *entry = fds__blob_find(doc, key);
    if (!entry) return FDS_CFG_NIL;
    return (Fds_Cfg_Type)entry->type;
}

int64_t fds_cfg_get_int(const Fds_Cfg_Doc *doc, const char *key, int64_t default_val) {
    const Fds_Blob_Entry *entry = fds__blob_find(doc, key);
    if (!entry || entry->type != FDS_CFG_INT) return default_val;
    return entry->as.i_val;
}

double fds_cfg_get_float(const Fds_Cfg_Doc *doc, const char *key, double default_val) {
    const Fds_Blob_Entry *entry = fds__blob_find(doc, key);
    if (!entry || (entry->type != FDS_CFG_FLOAT && entry->type != FDS_CFG_INT)) return default_val;
    if (entry->type == FDS_CFG_FLOAT) return entry->as.f_val;
    return (double)entry->as.i_val;
}

bool fds_cfg_get_bool(const Fds_Cfg_Doc *doc, const char *key, bool default_val) {
    const Fds_Blob_Entry *entry = fds__blob_find(doc, key);
    if (!entry || entry->type != FDS_CFG_BOOL) return default_val;
    return entry->as.b_val != 0;
}

const char* fds_cfg_get_string(const Fds_Cfg_Doc *doc, const char *key, const char *default_val) {
    const Fds_Blob_Entry *entry = fds__blob_find(doc, key);
    if (!entry || entry->type != FDS_CFG_STR) return default_val;
    const Fds_Blob_Header *hdr = (const Fds_Blob_Header*)doc->blob;
    const char *str_pool = (const char*)doc->blob + hdr->strings_offset;
    return str_pool + entry->as.data_offset;
}

#endif /* FDS_EXT_CFG_IMPLEMENTATION */