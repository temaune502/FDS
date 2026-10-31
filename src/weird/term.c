// #ifndef FDS_TERM_H
// #define FDS_TERM_H

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef _WIN32
#include <windows.h>
#endif

// ============================================================================
//                          FdsOut (Fast Stdio Writer)
// ============================================================================

typedef struct {
    FILE *file;
} FdsOut;

static FdsOut fds_stdout_inst = { .file = NULL };
static FdsOut fds_stderr_inst = { .file = NULL };

#define fds_stdout() (&fds_stdout_inst)
#define fds_stderr() (&fds_stderr_inst)

 void fds_out_write(FdsOut *out, const char *data, size_t len) {
    if (!out || len == 0) return;
    FILE *f = out->file ? out->file : (out == &fds_stdout_inst ? stdout : stderr);
    fwrite(data, 1, len, f);
}

 void fds_out_cstr(FdsOut *out, const char *cstr) {
    if (!out || !cstr) return;
    FILE *f = out->file ? out->file : (out == &fds_stdout_inst ? stdout : stderr);
    fputs(cstr, f);
}

 void fds_out_char(FdsOut *out, char c) {
    if (!out) return;
    FILE *f = out->file ? out->file : (out == &fds_stdout_inst ? stdout : stderr);
    fputc(c, f);
}

 void fds_out_flush(FdsOut *out) {
    if (!out) return;
    FILE *f = out->file ? out->file : (out == &fds_stdout_inst ? stdout : stderr);
    fflush(f);
}

// Ультрашвидкий вивід цілих чисел без форматування через fprintf
 void fds_out_u64(FdsOut *out, uint64_t val) {
    char buf[32];
    int i = 31;
    buf[i--] = '\0';
    if (val == 0) {
        buf[i--] = '0';
    } else {
        while (val > 0) {
            buf[i--] = '0' + (val % 10);
            val /= 10;
        }
    }
    fds_out_cstr(out, &buf[i + 1]);
}

 void fds_out_i64(FdsOut *out, int64_t val) {
    if (val < 0) {
        fds_out_char(out, '-');
        fds_out_u64(out, (uint64_t)(-val));
    } else {
        fds_out_u64(out, (uint64_t)val);
    }
}

// ============================================================================
//                          FdsTerm (ANSI Control)
// ============================================================================

#define FDS_ESC "\x1b["

#define FDS_RESET       FDS_ESC "0m"
#define FDS_BOLD        FDS_ESC "1m"
#define FDS_DIM         FDS_ESC "2m"
#define FDS_UNDERLINE   FDS_ESC "4m"

#define FDS_FG_BLACK    FDS_ESC "30m"
#define FDS_FG_RED      FDS_ESC "31m"
#define FDS_FG_GREEN    FDS_ESC "32m"
#define FDS_FG_YELLOW   FDS_ESC "33m"
#define FDS_FG_BLUE     FDS_ESC "34m"
#define FDS_FG_MAGENTA  FDS_ESC "35m"
#define FDS_FG_CYAN     FDS_ESC "36m"
#define FDS_FG_WHITE    FDS_ESC "37m"

 void fds_term_init(void) {
    fds_stdout_inst.file = stdout;
    fds_stderr_inst.file = stderr;

#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD mode = 0;
        if (GetConsoleMode(hOut, &mode)) {
            SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
    }
    HANDLE hErr = GetStdHandle(STD_ERROR_HANDLE);
    if (hErr != INVALID_HANDLE_VALUE) {
        DWORD mode = 0;
        if (GetConsoleMode(hErr, &mode)) {
            SetConsoleMode(hErr, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
    }
#endif
}

 void fds_term_clear_screen(void) {
    fds_out_cstr(fds_stdout(), FDS_ESC "2J" FDS_ESC "H");
}

 void fds_term_clear_line(void) {
    fds_out_cstr(fds_stdout(), FDS_ESC "2K" FDS_ESC "1G");
}

 void fds_term_hide_cursor(void) {
    fds_out_cstr(fds_stdout(), FDS_ESC "?25l");
}

 void fds_term_show_cursor(void) {
    fds_out_cstr(fds_stdout(), FDS_ESC "?25h");
}

// #endif // FDS_TERM_H


int main(void)
{
    fds_term_init();
    Sleep(2*1000);
    fds_term_clear_screen();
    Sleep(2*1000);
    fds_term_hide_cursor();
    fds_out_cstr(fds_stdout(), "Hello world!\n");
    fds_out_cstr(fds_stdout(), "Hello world!\n");
    Sleep(2*1000);
    fds_term_clear_line();
    Sleep(2*1000);
    fds_out_cstr(fds_stdout(), "Hello world!\n");
    fds_out_cstr(fds_stdout(), "Hello world!\n");
    fds_out_cstr(fds_stdout(), "Hello world!\n");
    fds_out_cstr(fds_stdout(), "Hello world!\n");
    fds_out_cstr(fds_stdout(), "Hello world!\n");
    Sleep(5*1000);
    fds_term_show_cursor();
    return 69;

}