#define DEBUG_MEM
#define FDS_IMPL
#include "fds.h"
#include <time.h>
#include <math.h>
#include <string.h>

// Швидкий і легкий PRNG (Xorshift32), щоб не залежати від стандартного rand()
typedef struct
{
    u32 state;
} SloppyRNG;

// static inline float sloppy_rand_bell(SloppyRNG *rng, float min, float max);
// static inline float sloppy_jitter(SloppyRNG *rng, float val, float amount);
// static inline bool sloppy_chance(SloppyRNG *rng, float probability);
// static inline float sloppy_rand_range(SloppyRNG *rng, float min, float max);
// static inline float sloppy_rand01(SloppyRNG *rng);
// static inline uint32_t sloppy_rng_next(SloppyRNG *rng);
// static inline SloppyRNG sloppy_rng_init(uint32_t seed);
// static inline float sloppy_snap(float val, float step);
// static inline int sloppy_sign(float val, float deadzone);
// static inline bool sloppy_between(float val, float min, float max, float margin);
// static inline bool sloppy_eq_rel(float a, float b, float rel_eps);
// static inline bool sloppy_eq(float a, float b, float max_diff);

static inline bool sloppy_eq(float a, float b, float max_diff)
{
    return fabsf(a - b) <= max_diff;
}

// Відносне порівняння: краще підходить для дуже великих або дуже малих чисел
static inline bool sloppy_eq_rel(float a, float b, float rel_eps)
{
    float diff = fabsf(a - b);
    float largest = fmaxf(fabsf(a), fabsf(b));
    return diff <= largest * rel_eps;
}

// Перевірка, чи належить число інтервалу [min, max] з урахуванням допустимої похибки (margin)
static inline bool sloppy_between(float val, float min, float max, float margin)
{
    return val >= (min - margin) && val <= (max + margin);
}

// Знак числа, але з мертвою зоною (deadzone): якщо число занадто близьке до 0, повертає 0
static inline int sloppy_sign(float val, float deadzone)
{
    if (fabsf(val) <= deadzone)
        return 0;
    return (val > 0.0f) - (val < 0.0f);
}

// Прив'язка значення до найближчого кроку сітки (наприклад, sloppy_snap(1.82, 0.5) -> 2.0)
static inline float sloppy_snap(float val, float step)
{
    if (step <= 0.0f)
        return val;
    return roundf(val / step) * step;
}

// ============================================================================
// 2. РАНДОМ ТА ЙМОВІРНОСТІ (SLOPPY RANDOM)
// ============================================================================


static inline SloppyRNG sloppy_rng_init(uint32_t seed)
{
    SloppyRNG rng = {seed ? seed : 0xACE1u};
    return rng;
}

static inline uint32_t sloppy_rng_next(SloppyRNG *rng)
{
    uint32_t x = rng->state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return rng->state = x;
}

// Рандомне float у діапазоні [0.0, 1.0]
static inline float sloppy_rand01(SloppyRNG *rng)
{
    return (float)(sloppy_rng_next(rng) & 0xFFFFFF) / 16777215.0f;
}

// Рандомне float у довільному діапазоні [min, max]
static inline float sloppy_rand_range(SloppyRNG *rng, float min, float max)
{
    return min + sloppy_rand01(rng) * (max - min);
}

// Повертає true з вказаною ймовірністю probability (від 0.0 до 1.0)
static inline bool sloppy_chance(SloppyRNG *rng, float probability)
{
    return sloppy_rand01(rng) < probability;
}

// Додає випадковий "шум/дріжання" до значення: val +/- amount
static inline float sloppy_jitter(SloppyRNG *rng, float val, float amount)
{
    return val + sloppy_rand_range(rng, -amount, amount);
}

// Рандом з гаусовим (куполоподібним) розподілом навколо центру
// (значення частіше випадають ближче до середини min та max)
static inline float sloppy_rand_bell(SloppyRNG *rng, float min, float max)
{
    float u1 = sloppy_rand_range(rng, min, max);
    float u2 = sloppy_rand_range(rng, min, max);
    float u3 = sloppy_rand_range(rng, min, max);
    return (u1 + u2 + u3) / 3.0f;
}

static struct {
    int indent_level;
    fds_log_level min_level;
    
    FILE* log_file;
} g_state = {
    .indent_level = 0,
    .min_level = FINFO,
    .log_file = NULL
};

// ANSI Кольори
#define CLR_RESET   "\x1b[0m"
#define CLR_RED     "\x1b[31m"
#define CLR_GREEN   "\x1b[32m"
#define CLR_YELLOW  "\x1b[33m"
#define CLR_BLUE    "\x1b[34m"
#define CLR_MAGENTA "\x1b[35m"
#define CLR_CYAN    "\x1b[36m"
#define CLR_GRAY    "\x1b[90m"

// ==========================================
// 2. ЯДРО (CORE LOGGING ENGINE)
// ==========================================
static void print_indent(const char* meta_color, const char* reset) {
    for (int i = 0; i < g_state.indent_level; i++) {
        printf("%s│%s  ", meta_color, reset);
    }
}

static void core_log(fds_log_level level, const char* level_str, const char* base_color, const char* fmt, va_list args) {
    if (level < g_state.min_level) return; // Фільтрація рівнів

    const char *color = base_color;
    const char *reset = "";
    const char *meta_color = "";
    FILE *stream = stdout;

    // Якщо вивід йде у файл (не є терміналом), зануляємо всі кольори
    if (fds_should_use_color(stream)) {
        reset = "\x1b[0m";
        meta_color = "\x1b[90m"; // Сірий для метаданих
    } else {
        color = "";
        reset = "";
        meta_color = "";
    }

    // 1. Динамічне виділення пам'яті під рядок (жодних обмежень довжини!)
    va_list args_copy;
    va_copy(args_copy, args);
    int len = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);
    
    char* msg = (char*)fds_alloc_tmp(len + 1);
    vsnprintf(msg, len + 1, fmt, args);

    // 2. Прибираємо зайві \n з кінця
    while (len > 0 && (msg[len-1] == '\n' || msg[len-1] == '\r')) {
        msg[--len] = '\0';
    }

    // 3. Формуємо повний рядок логу
    time_t t = time(NULL);
    struct tm tm_info = *localtime(&t);
    
    // 4. Вивід у консоль (тепер адаптується під stream)
    print_indent(meta_color, reset);
    printf("%s[%02d:%02d:%02d] [%s]%s ", 
           meta_color, tm_info.tm_hour, tm_info.tm_min, tm_info.tm_sec, 
           level_str, reset);
    printf("%s%s%s\n", color, msg, reset);

    // 5. Опціональний вивід у файл (завжди без кольорів, як і було)
    if (g_state.log_file) {
        fprintf(g_state.log_file, "[%02d:%02d:%02d] [%s] %s\n", 
                tm_info.tm_hour, tm_info.tm_min, tm_info.tm_sec, level_str, msg);
        fflush(g_state.log_file);
    }
    
    fds_allocator_clear_tmp(fds_allocator_current());
}

// ==========================================
// 3. РЕАЛІЗАЦІЯ ФУНКЦІЙ CONSOLE
// ==========================================
static void impl_log(const char* fmt, ...) {
    va_list args; va_start(args, fmt);
    core_log(FINFO, "LOG", CLR_CYAN, fmt, args);
    va_end(args);
}

static void impl_info(const char* fmt, ...) {
    va_list args; va_start(args, fmt);
    core_log(FINFO, "INFO", CLR_BLUE, fmt, args);
    va_end(args);
}

static void impl_warn(const char* fmt, ...) {
    va_list args; va_start(args, fmt);
    core_log(FWARN, "WARN", CLR_YELLOW, fmt, args);
    va_end(args);
}

static void impl_error(const char* fmt, ...) {
    va_list args; va_start(args, fmt);
    core_log(FERROR, "ERROR", CLR_RED, fmt, args);
    va_end(args);
}

static void impl_debug(const char* fmt, ...) {
    va_list args; va_start(args, fmt);
    core_log(FDEBUG, "DEBUG", CLR_GRAY, fmt, args);
    va_end(args);
}

// --- Налаштування ---
static void impl_set_level(fds_log_level level) { g_state.min_level = level; }
static void impl_set_file(const char* path) { 
    if (g_state.log_file) fclose(g_state.log_file);
    g_state.log_file = fopen(path, "w"); 
}
static void impl_newline(void) {
    printf("\n"); // Чистий перехід на новий рядок без штампу часу
}
// ==========================================
// 5. ГЛОБАЛЬНИЙ ОБ'ЄКТ CONSOLE
// ==========================================
typedef struct {
    void (*log)(const char* fmt, ...);
    void (*info)(const char* fmt, ...);
    void (*warn)(const char* fmt, ...);
    void (*error)(const char* fmt, ...);
    void (*debug)(const char* fmt, ...);
    void (*newline)(void);

    void (*setLevel)(fds_log_level level);
    void (*setFile)(const char* path);
} Console;

static Console console = {
    .log      = impl_log,
    .info     = impl_info,
    .warn     = impl_warn,
    .error    = impl_error,
    .debug    = impl_debug,
    .newline  = impl_newline,
    .setLevel = impl_set_level,
    .setFile  = impl_set_file
};


//=======================================================
//                     Growing Arena
//=======================================================
// An arena backed by a linked list of stable memory blocks.
// Blocks are never moved, so previously returned pointers stay valid
// until growing_arena_free().
typedef struct GrowingArenaBlock GrowingArenaBlock;

struct GrowingArenaBlock
{
	unsigned char *data;
	size_t offset;
	size_t capacity;
	GrowingArenaBlock *next;
};

typedef struct
{
	GrowingArenaBlock *first;
	GrowingArenaBlock *current;
	size_t initial_capacity;
} GrowingArena;

typedef struct
{
	GrowingArenaBlock *block;
	size_t offset;
} GrowingArenaMark;



// Growing arena fuctions Start ==============================================================================================================
GrowingArena growing_arena_create(size_t initial_capacity);
void growing_arena_free(GrowingArena *arena);
void growing_arena_reset(GrowingArena *arena);
size_t growing_arena_used(const GrowingArena *arena);
size_t growing_arena_capacity(const GrowingArena *arena);
size_t growing_arena_available(const GrowingArena *arena);
int growing_arena_is_empty(const GrowingArena *arena);
int growing_arena_contains(const GrowingArena *arena, const void *ptr);
GrowingArenaMark growing_arena_mark(const GrowingArena *arena);
void growing_arena_restore(GrowingArena *arena, GrowingArenaMark mark);
void *growing_arena_alloc_align(GrowingArena *arena, size_t size, size_t alignment);
void *growing_arena_alloc(GrowingArena *arena, size_t size);
void *growing_arena_alloc_zero(GrowingArena *arena, size_t size);
void *growing_arena_alloc_array(GrowingArena *arena, size_t count, size_t element_size);
void *growing_arena_memdup(GrowingArena *arena, const void *src, size_t size);
char *growing_arena_strndup(GrowingArena *arena, const char *str, size_t len);
char *growing_arena_strdup(GrowingArena *arena, const char *str);
// Growing arena fuctions End ================================================================================================================






// Growing arena fuctions Start ==============================================================================================================

static GrowingArenaBlock *growing_arena_block_create(size_t capacity)
{
	GrowingArenaBlock *block = (GrowingArenaBlock *)FDS_MALLOC(sizeof(*block));
	FDS_ASSERT(block, "block");

	block->data = (unsigned char *)FDS_MALLOC(capacity);
	if (!block->data)
	{
		FDS_FREE(block);
		fds_log(FFATAL, "GrowingArena: failed to allocate block data (%zu bytes)", capacity);
		return NULL;
	}

	block->offset = 0;
	block->capacity = capacity;
	block->next = NULL;
	return block;
}

static GrowingArenaBlock *growing_arena_find_block(const GrowingArena *arena, const void *ptr)
{
	if (!arena || !ptr)
		return NULL;

	uintptr_t address = (uintptr_t)ptr;
	for (GrowingArenaBlock *block = arena->first; block; block = block->next)
	{
		uintptr_t begin = (uintptr_t)block->data;
		uintptr_t end = begin + block->offset;
		if (address >= begin && address < end)
			return block;
	}

	return NULL;
}

GrowingArena growing_arena_create(size_t initial_capacity)
{
	FDS_ASSERT(initial_capacity > 0, "initial_capacity > 0");

	GrowingArena arena = zero;
	GrowingArenaBlock *block = growing_arena_block_create(initial_capacity);
	arena.first = block;
	arena.current = block;
	arena.initial_capacity = initial_capacity;
	return arena;
}

void growing_arena_free(GrowingArena *arena)
{
	FDS_ASSERT(arena, "arena");

	GrowingArenaBlock *block = arena->first;
	while (block)
	{
		GrowingArenaBlock *next = block->next;
		FDS_FREE(block->data);
		FDS_FREE(block);
		block = next;
	}

	arena->first = NULL;
	arena->current = NULL;
	arena->initial_capacity = 0;
}

void growing_arena_reset(GrowingArena *arena)
{
	FDS_ASSERT(arena, "arena");

	for (GrowingArenaBlock *block = arena->first; block; block = block->next)
		block->offset = 0;

	arena->current = arena->first;
}

size_t growing_arena_used(const GrowingArena *arena)
{
	FDS_ASSERT(arena, "arena");

	size_t total = 0;
	for (GrowingArenaBlock *block = arena->first; block; block = block->next)
	{
		if (total > SIZE_MAX - block->offset)
			fds_log(FFATAL, "GrowingArena used size overflow");
		total += block->offset;
	}
	return total;
}

size_t growing_arena_capacity(const GrowingArena *arena)
{
	FDS_ASSERT(arena, "arena");

	size_t total = 0;
	for (GrowingArenaBlock *block = arena->first; block; block = block->next)
	{
		if (total > SIZE_MAX - block->capacity)
			fds_log(FFATAL, "GrowingArena capacity overflow");
		total += block->capacity;
	}
	return total;
}

size_t growing_arena_available(const GrowingArena *arena)
{
	size_t capacity = growing_arena_capacity(arena);
	size_t used = growing_arena_used(arena);
	return capacity - used;
}

int growing_arena_is_empty(const GrowingArena *arena)
{
	FDS_ASSERT(arena, "arena");
	return !arena->first || arena->first->offset == 0;
}

int growing_arena_contains(const GrowingArena *arena, const void *ptr)
{
	return growing_arena_find_block(arena, ptr) != NULL;
}

GrowingArenaMark growing_arena_mark(const GrowingArena *arena)
{
	FDS_ASSERT(arena, "arena");
	FDS_ASSERT(arena->current, "arena->current");

	GrowingArenaMark mark = zero;
	mark.block = arena->current;
	mark.offset = arena->current->offset;
	return mark;
}

void growing_arena_restore(GrowingArena *arena, GrowingArenaMark mark)
{
	FDS_ASSERT(arena, "arena");
	FDS_ASSERT(mark.block, "mark.block");

	GrowingArenaBlock *block = arena->first;
	int found = 0;
	while (block)
	{
		if (block == mark.block)
		{
			found = 1;
			break;
		}
		block = block->next;
	}

	FDS_ASSERT(found, "mark belongs to this arena");
	FDS_ASSERT(mark.offset <= mark.block->offset, "mark.offset <= mark.block->offset");

	mark.block->offset = mark.offset;
	for (block = mark.block->next; block; block = block->next)
		block->offset = 0;

	arena->current = mark.block;
}

void *growing_arena_alloc_align(GrowingArena *arena, size_t size, size_t alignment)
{
	FDS_ASSERT(arena, "arena");
	FDS_ASSERT(arena->current, "arena->current");
	FDS_ASSERT(size > 0, "size > 0");
	FDS_ASSERT(alignment > 0, "alignment > 0");
	FDS_ASSERT((alignment & (alignment - 1)) == 0, "alignment must be a power of two");

	GrowingArenaBlock *block = arena->current;

	uintptr_t ptr = (uintptr_t)(block->data + block->offset);
	uintptr_t aligned = (ptr + alignment - 1) & ~(uintptr_t)(alignment - 1);
	size_t padding = (size_t)(aligned - ptr);

	if (padding <= block->capacity - block->offset &&
		size <= block->capacity - block->offset - padding)
	{
		block->offset += padding;
		void *result = block->data + block->offset;
		block->offset += size;
		return result;
	}

	if (size > SIZE_MAX - (alignment - 1))
		fds_log(FFATAL, "GrowingArena allocation size overflow");

	size_t required = size + alignment - 1;
	size_t new_capacity;

	if (block->capacity > SIZE_MAX / 2)
		new_capacity = required;
	else
		new_capacity = block->capacity * 2;

	if (new_capacity < arena->initial_capacity)
		new_capacity = arena->initial_capacity;
	if (new_capacity < required)
		new_capacity = required;

	GrowingArenaBlock *new_block = growing_arena_block_create(new_capacity);
	block->next = new_block;
	arena->current = new_block;

	uintptr_t new_ptr = (uintptr_t)new_block->data;
	uintptr_t new_aligned = (new_ptr + alignment - 1) & ~(uintptr_t)(alignment - 1);
	size_t new_padding = (size_t)(new_aligned - new_ptr);

	FDS_ASSERT(new_padding <= new_block->capacity, "new block alignment padding fits");
	FDS_ASSERT(size <= new_block->capacity - new_padding, "new block allocation fits");

	new_block->offset = new_padding;
	void *result = new_block->data + new_block->offset;
	new_block->offset += size;
	return result;
}

void *growing_arena_alloc(GrowingArena *arena, size_t size)
{
	return growing_arena_alloc_align(arena, size, sizeof(void *));
}

void *growing_arena_alloc_zero(GrowingArena *arena, size_t size)
{
	void *ptr = growing_arena_alloc(arena, size);
	memset(ptr, 0, size);
	return ptr;
}

void *growing_arena_alloc_array(GrowingArena *arena, size_t count, size_t element_size)
{
	if (count > 0 && element_size > SIZE_MAX / count)
		fds_log(FFATAL, "Array size overflow");

	return growing_arena_alloc(arena, count * element_size);
}

void *growing_arena_memdup(GrowingArena *arena, const void *src, size_t size)
{
	void *dst = growing_arena_alloc(arena, size);
	memcpy(dst, src, size);
	return dst;
}

char *growing_arena_strndup(GrowingArena *arena, const char *str, size_t len)
{
	if (len == SIZE_MAX)
		fds_log(FFATAL, "String size overflow");

	char *dst = (char *)growing_arena_alloc(arena, len + 1);
	memcpy(dst, str, len);
	dst[len] = '\0';
	return dst;
}

char *growing_arena_strdup(GrowingArena *arena, const char *str)
{
	return growing_arena_strndup(arena, str, strlen(str));
}

// Growing arena fuctions End ================================================================================================================


#define FDS_UNIMPLEMENTED() fds_log(FFATAL, "unimplemented: %s", __func__)
#define FDS_UNREACHABLE() fds_log(FFATAL,"unreachable reached: %s:%d", __FILE__, __LINE__)
int main(void)
{
    for(s64 i =0; i < 100000; ++i)
        console.info("Hello temaune!! hahaha %d\n", i);


    s64* arrInt = New(s64);
    fds_free(arrInt);
    fds_allocator_print_stats(fds_allocator_current());
    fds_allocator_destroy(fds_allocator_current());
    return 0;
}