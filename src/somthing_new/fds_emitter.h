/*
 * fds_emitter.h - Code Generation Emitter
 * 
 * ВСТАНОВЛЕННЯ:
 * У *одному* C-файлі вашого проекту перед include визначте макрос:
 * #define FDS_EMITTER_IMPLEMENTATION
 * #include "fds_emitter.h"
 */

#ifndef FDS_EMITTER_H
#define FDS_EMITTER_H

#include <stdbool.h>
#include <stdarg.h>

// Компіляційне вимкнення дебаг-метаданих
#if defined(FDS_EMITTER_NO_DEBUG) || defined(NDEBUG)
    #define FDS_EMITTER_DEBUG_ENABLED_DEFAULT false
#else
    #define FDS_EMITTER_DEBUG_ENABLED_DEFAULT true
#endif

#define FDS_EMITTER_MAX_INDENT_STACK 32

typedef struct {
    SB *sb;
    int               indent_level;
    int               spaces_per_indent;
    bool              use_tabs;
    bool              at_line_start;
    bool              emit_debug_loc;
    
    int               indent_stack[FDS_EMITTER_MAX_INDENT_STACK];
    int               indent_stack_top;
} FdsEmitter;

// --- API Оголошення ---

void fds_emitter_init(FdsEmitter *e, SB *sb);
void fds_emitter_enable_debug(FdsEmitter *e, bool enable);
void fds_emitter_indent(FdsEmitter *e);
void fds_emitter_dedent(FdsEmitter *e);
void fds_emitter_push_indent(FdsEmitter *e);
void fds_emitter_pop_indent(FdsEmitter *e);

void fds_emitter_write_loc(FdsEmitter *e, const char *fmt, ...);
void fds_emitter_write_sv(FdsEmitter *e, SV sv);
void fds_emitter_line_loc(FdsEmitter *e, const char *file, int line, const char *func, const char *fmt, ...);
void fds_emitter_newline(FdsEmitter *e);
void fds_emitter_comment_loc(FdsEmitter *e, const char *fmt, ...);
void fds_emitter_label_loc(FdsEmitter *e, const char *fmt, ...);

// --- Публічні макроси ---

#define fds_emitter_write(e, ...) \
    fds_emitter_write_loc((e), __VA_ARGS__)

#define fds_emitter_line(e, ...) \
    fds_emitter_line_loc((e), __FILE__, __LINE__, __func__, __VA_ARGS__)

#define fds_emitter_comment(e, ...) \
    fds_emitter_comment_loc((e), __VA_ARGS__)

#define fds_emitter_label(e, ...) \
    fds_emitter_label_loc((e), __VA_ARGS__)

#define FDS_EMITTER_BLOCK(e, open_str, close_str) \
    for (int _i = (fds_emitter_line_loc((e), __FILE__, __LINE__, __func__, "%s", open_str), fds_emitter_indent(e), 0); \
         !_i; \
         _i = 1, fds_emitter_dedent(e), fds_emitter_line_loc((e), __FILE__, __LINE__, __func__, "%s", close_str))

#endif // FDS_EMITTER_H


// ============================================================================
//                              РЕАЛІЗАЦІЯ
// ============================================================================

#ifdef FDS_EMITTER_IMPLEMENTATION

void fds_emitter_init(FdsEmitter *e, SB *sb) {
    e->sb = sb;
    e->indent_level = 0;
    e->spaces_per_indent = 4;
    e->use_tabs = false;
    e->at_line_start = true;
    e->emit_debug_loc = FDS_EMITTER_DEBUG_ENABLED_DEFAULT;
    e->indent_stack_top = 0;
}

void fds_emitter_enable_debug(FdsEmitter *e, bool enable) {
    e->emit_debug_loc = enable;
}

void fds_emitter_indent(FdsEmitter *e) { e->indent_level++; }
void fds_emitter_dedent(FdsEmitter *e) { if (e->indent_level > 0) e->indent_level--; }

void fds_emitter_push_indent(FdsEmitter *e) {
    if (e->indent_stack_top < FDS_EMITTER_MAX_INDENT_STACK) {
        e->indent_stack[e->indent_stack_top++] = e->indent_level;
    }
}

void fds_emitter_pop_indent(FdsEmitter *e) {
    if (e->indent_stack_top > 0) {
        e->indent_level = e->indent_stack[--e->indent_stack_top];
    }
}

static void fds_internal_emitter_apply_indent(FdsEmitter *e) {
    if (!e->at_line_start) return;
    int total_spaces = e->indent_level * e->spaces_per_indent;
    if (e->use_tabs) {
        for (int i = 0; i < e->indent_level; i++) sb_append_char(e->sb, '\t');
    } else {
        for (int i = 0; i < total_spaces; i++) sb_append_char(e->sb, ' ');
    }
    e->at_line_start = false;
}

void fds_emitter_write_loc(FdsEmitter *e, const char *fmt, ...) {
    fds_internal_emitter_apply_indent(e);
    va_list args;
    va_start(args, fmt);
    sb_appendf(e->sb, fmt, args);
    va_end(args);
}

void fds_emitter_write_sv(FdsEmitter *e, SV sv) {
    fds_internal_emitter_apply_indent(e);
    sb_append_sv(e->sb, sv);
}

void fds_emitter_line_loc(FdsEmitter *e, const char *file, int line, const char *func, const char *fmt, ...) {
    fds_internal_emitter_apply_indent(e);
    
    va_list args;
    va_start(args, fmt);
    sb_appendf(e->sb, fmt, args);
    va_end(args);

#if !defined(FDS_EMITTER_NO_DEBUG)
    if (e->emit_debug_loc && file) {
        sb_appendf(e->sb, " /* gen: %s:%d (%s) */", file, line, func);
    }
#else
    (void)file; (void)line; (void)func;
#endif

    sb_append_char(e->sb, '\n');
    e->at_line_start = true;
}

void fds_emitter_newline(FdsEmitter *e) {
    sb_append_char(e->sb, '\n');
    e->at_line_start = true;
}

void fds_emitter_comment_loc(FdsEmitter *e, const char *fmt, ...) {
    fds_internal_emitter_apply_indent(e);
    sb_append_n(e->sb, "// ", 3);
    
    va_list args;
    va_start(args, fmt);
    sb_appendf(e->sb, fmt, args);
    va_end(args);

    sb_append_char(e->sb, '\n');
    e->at_line_start = true;
}

void fds_emitter_label_loc(FdsEmitter *e, const char *fmt, ...) {
    int old_indent = e->indent_level;
    if (e->indent_level > 0) e->indent_level--;
    
    fds_internal_emitter_apply_indent(e);
    
    va_list args;
    va_start(args, fmt);
    sb_appendf(e->sb, fmt, args);
    va_end(args);
    
    sb_append_n(e->sb, ":\n", 2);
    e->indent_level = old_indent;
    e->at_line_start = true;
}

#endif // FDS_EMITTER_IMPLEMENTATION