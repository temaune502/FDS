#ifndef FDS_H
#define FDS_H

#define SB_INITIAL_CAPACITY 16



#define SV_FMT "%.*s"
#define SV_ARGS(sv) (int)(sv).count, (sv).data
#define array_len(arr) (sizeof(arr)/sizeof((arr)[0]))

typedef struct
{
    size_t count;
    size_t capacity;
    char *items;
} SB;
typedef struct
{
    size_t count;
    const char *data;
} SV;


SB sb_from_cstr(const char *str);
SB sb_new(void);
static void sb_grow(SB *sb, size_t size);
void sb_append_sv(SB *sb, SV sv);
void sb_free(SB *sb);
void sb_append(SB *sb, const char *str);
void sb_append_n(SB *sb, const char *str, size_t len);
void sb_reserve(SB *sb, size_t capacity);
void sb_alloc(SB *sb, size_t capacity);
char *sb_to_cstr(SB *sb);
void sb_append_null(SB *sb);
void sb_append_char(SB *sb, char c);

#ifdef FDS_IMPLEMENTATION

void sb_append_sv(SB *sb, SV sv)
{
    sb_grow(sb, sv.count);

    memcpy(sb->items + sb->count, sv.data, sv.count);
    sb->count += sv.count;
    sb->items[sb->count] = '\0';
}
static void sb_grow(SB *sb, size_t size)
{
    void *item;

    if (sb->count + size + 1 <= sb->capacity)
        return;

    // ростемо в 2 рази
    while (sb->count + size + 1 > sb->capacity)
        sb->capacity *= 2;

    item = realloc(sb->items, sb->capacity);
    if (item == NULL)
    {
        fprintf(stderr, "Out of memory\n");
        abort();
    }

    sb->items = item;
}
SB sb_from_cstr(const char *str)
{
    SB sb = {0};
    const size_t len = strlen(str);

    sb.count = len;
    sb.capacity = len + 1;
    sb.items = malloc(sb.capacity);

    if (sb.items == NULL)
    {
        fprintf(stderr, "Out of memory\n");
        abort();
    }

    memcpy(sb.items, str, len + 1);
    return sb;
}

SB sb_new(void)
{
    SB sb = {0};

    sb.capacity = SB_INITIAL_CAPACITY;
    sb.items = malloc(sb.capacity);

    if (sb.items == NULL)
    {
        fprintf(stderr, "Out of memory\n");
        abort();
    }

    sb.items[0] = '\0';
    return sb;
}

void sb_free(SB *sb)
{
    free(sb->items);
    sb->items = NULL;
    sb->count = 0;
    sb->capacity = 0;
}

void sb_append(SB *sb, const char *str)
{
    const size_t len = strlen(str);

    sb_grow(sb, len);

    memcpy(sb->items + sb->count, str, len);
    sb->count += len;
    sb->items[sb->count] = '\0';
}

void sb_append_n(SB *sb, const char *str, size_t len)
{
    sb_grow(sb, len);

    memcpy(sb->items + sb->count, str, len);
    sb->count += len;
    sb->items[sb->count] = '\0';
}

void sb_reserve(SB *sb, size_t capacity)
{
    if (capacity <= sb->capacity)
        return;

    void *item = realloc(sb->items, capacity);

    if (item == NULL)
    {
        fprintf(stderr, "Out of memory\n");
        abort();
    }

    sb->items = item;
    sb->capacity = capacity;
}

void sb_alloc(SB *sb, size_t capacity)
{
    void *item;

    if (capacity == 0)
        return;

    sb->capacity += capacity;

    item = realloc(sb->items, sb->capacity);
    if (item == NULL)
    {
        fprintf(stderr, "Out of memory\n");
        abort();
    }

    sb->items = item;
}

char *sb_to_cstr(SB *sb)
{
    sb->items[sb->count] = '\0';
    return sb->items;
}

void sb_append_null(SB *sb)
{
    sb_grow(sb, 0);
    sb->items[sb->count] = '\0';
}

void sb_append_char(SB *sb, char c)
{
    sb_grow(sb, 1);

    sb->items[sb->count++] = c;
    sb->items[sb->count] = '\0';
}

#endif // FDS_IMPLEMENTATION

#endif // FDS_H