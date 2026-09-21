#ifndef FDS_EXT_FORMATTING_H
#define FDS_EXT_FORMATTING_H

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Форматування у StringBuilder
bool fds_sbuilder_append_fmt(SB *sb, const char *fmt, ...);
bool fds_sbuilder_append_fmtv(SB *sb, const char *fmt, va_list args);

// Hex-dump дамп пам'яті/рядка у StringBuilder (формат канонічного hex-редактора)
void fds_sbuilder_append_hexdump(SB *sb, SV sv, size_t bytes_per_line);

// Scratch-форматування: повертає вказівник на внутрішній буфер
// Дійсний до наступного виклику fds_fmt / fds_fmtv у цьому ж потоці
const char *fds_fmt(const char *fmt, ...);
const char *fds_fmtv(const char *fmt, va_list args);
void        fds_fmt_free(void);

#ifdef __cplusplus
}
#endif

#endif // FDS_EXT_FORMATTING_H

/* ============================================================================
                                РЕАЛІЗАЦІЯ
   ============================================================================ */

#ifdef FDS_EXT_FORMATTING_IMPLEMENTATION
#ifndef FDS_EXT_FORMATTING_IMPLEMENTATION_DONE
#define FDS_EXT_FORMATTING_IMPLEMENTATION_DONE

typedef struct FdsFmtSpec {
    bool   zero_pad;
    bool   left_align;
    bool   has_precision;
    size_t width;
    size_t precision;
} FdsFmtSpec;

static void fds_fmt_pad(SB *sb, char ch, size_t count) {
    char pad_buf[32];
    for (size_t i = 0; i < sizeof(pad_buf); ++i) pad_buf[i] = ch;
    while (count > 0) {
        size_t chunk = count < sizeof(pad_buf) ? count : sizeof(pad_buf);
        sb_append_sv(sb, sv_from_parts(pad_buf, chunk));
        count -= chunk;
    }
}

static void fds_fmt_write_str(SB *sb, const char *str, size_t len, FdsFmtSpec spec) {
    if (spec.has_precision && spec.precision < len) {
        len = spec.precision;
    }
    if (spec.width > len && !spec.left_align) {
        fds_fmt_pad(sb, ' ', spec.width - len);
    }
    if (len > 0 && str) {
        sb_append_sv(sb, sv_from_parts(str, len));
    }
    if (spec.width > len && spec.left_align) {
        fds_fmt_pad(sb, ' ', spec.width - len);
    }
}

static void fds_fmt_write_uint64(SB *sb, uint64_t val, uint32_t base, bool uppercase, bool is_negative, FdsFmtSpec spec) {
    static const char hex_upper[] = "0123456789ABCDEF";
    static const char hex_lower[] = "0123456789abcdef";
    const char *digits = uppercase ? hex_upper : hex_lower;

    char buf[64];
    size_t len = 0;

    do {
        buf[len++] = digits[val % base];
        val /= base;
    } while (val > 0);

    for (size_t i = 0; i < len / 2; ++i) {
        char tmp = buf[i];
        buf[i] = buf[len - 1 - i];
        buf[len - 1 - i] = tmp;
    }

    size_t total_len = len + (is_negative ? 1 : 0);
    char pad_char = spec.zero_pad ? '0' : ' ';

    if (spec.width > total_len && !spec.left_align && pad_char == ' ') {
        fds_fmt_pad(sb, ' ', spec.width - total_len);
    }
    if (is_negative) {
        sb_append_sv(sb, sv_from_cstr("-"));
    }
    if (spec.width > total_len && !spec.left_align && pad_char == '0') {
        fds_fmt_pad(sb, '0', spec.width - total_len);
    }

    sb_append_sv(sb, sv_from_parts(buf, len));

    if (spec.width > total_len && spec.left_align) {
        fds_fmt_pad(sb, ' ', spec.width - total_len);
    }
}

static void fds_fmt_write_quoted_sv(SB *sb, SV sv) {
    sb_append_sv(sb, sv_from_cstr("\""));
    for (size_t i = 0; i < sv.count; ++i) {
        char c = sv.data[i];
        switch (c) {
            case '"':  sb_append_sv(sb, sv_from_cstr("\\\"")); break;
            case '\\': sb_append_sv(sb, sv_from_cstr("\\\\")); break;
            case '\n': sb_append_sv(sb, sv_from_cstr("\\n")); break;
            case '\r': sb_append_sv(sb, sv_from_cstr("\\r")); break;
            case '\t': sb_append_sv(sb, sv_from_cstr("\\t")); break;
            case '\0': sb_append_sv(sb, sv_from_cstr("\\0")); break;
            default:
                if ((unsigned char)c < 32 || (unsigned char)c > 126) {
                    static const char hex_upper[] = "0123456789ABCDEF";
                    char esc[4] = {'\\', 'x', hex_upper[(unsigned char)c >> 4], hex_upper[(unsigned char)c & 0x0F]};
                    sb_append_sv(sb, sv_from_parts(esc, 4));
                } else {
                    sb_append_sv(sb, sv_from_parts(&c, 1));
                }
                break;
        }
    }
    sb_append_sv(sb, sv_from_cstr("\""));
}

static void fds_fmt_write_double(SB *sb, double val, FdsFmtSpec spec) {
    size_t prec = spec.has_precision ? spec.precision : 6;
    bool is_neg = val < 0.0;
    if (is_neg) val = -val;

    uint64_t int_part = (uint64_t)val;
    double frac_part = val - (double)int_part;

    uint64_t mult = 1;
    for (size_t i = 0; i < prec; ++i) mult *= 10;

    uint64_t frac_int = (uint64_t)(frac_part * (double)mult + 0.5);
    if (frac_int >= mult) {
        int_part += 1;
        frac_int -= mult;
    }

    FdsFmtSpec int_spec = {0};
    fds_fmt_write_uint64(sb, int_part, 10, false, is_neg, int_spec);

    if (prec > 0) {
        sb_append_sv(sb, sv_from_cstr("."));
        FdsFmtSpec frac_spec = { .zero_pad = true, .width = prec };
        fds_fmt_write_uint64(sb, frac_int, 10, false, false, frac_spec);
    }
}

static void fds_fmt_write_human_size(SB *sb, uint64_t bytes, FdsFmtSpec spec) {
    static const char *units[] = {"B", "KiB", "MiB", "GiB", "TiB"};
    size_t unit_idx = 0;
    double size = (double)bytes;

    while (size >= 1024.0 && unit_idx < 4) {
        size /= 1024.0;
        unit_idx++;
    }

    if (unit_idx == 0) {
        fds_fmt_write_uint64(sb, bytes, 10, false, false, spec);
        sb_append_sv(sb, sv_from_cstr(" B"));
    } else {
        FdsFmtSpec fspec = spec;
        if (!fspec.has_precision) {
            fspec.has_precision = true;
            fspec.precision = 2;
        }
        fds_fmt_write_double(sb, size, fspec);
        sb_append_sv(sb, sv_from_cstr(" "));
        sb_append_sv(sb, sv_from_cstr(units[unit_idx]));
    }
}

bool fds_sbuilder_append_fmtv(SB *sb, const char *fmt, va_list args) {
    if (!sb || !fmt) return false;

    const char *curr = fmt;

    while (*curr != '\0') {
        if (*curr != '%') {
            const char *start = curr;
            while (*curr != '\0' && *curr != '%') curr++;
            sb_append_sv(sb, sv_from_parts(start, (size_t)(curr - start)));
            continue;
        }

        curr++; // Пропускаємо '%'
        if (*curr == '\0') break;

        FdsFmtSpec spec = {0};

        // Прапорці
        while (*curr == '0' || *curr == '-') {
            if (*curr == '-') spec.left_align = true;
            if (*curr == '0') spec.zero_pad = true;
            curr++;
        }
        if (spec.left_align) spec.zero_pad = false;

        // Ширина
        while (*curr >= '0' && *curr <= '9') {
            spec.width = spec.width * 10 + (size_t)(*curr - '0');
            curr++;
        }

        // Точність
        if (*curr == '.') {
            curr++;
            spec.has_precision = true;
            spec.precision = 0;
            while (*curr >= '0' && *curr <= '9') {
                spec.precision = spec.precision * 10 + (size_t)(*curr - '0');
                curr++;
            }
        }

        switch (*curr) {
            case '%': 
                sb_append_sv(sb, sv_from_cstr("%")); 
                break;

            case 'V':
            case 'B': {
                SV sv = va_arg(args, SV);
                fds_fmt_write_str(sb, sv.data, sv.count, spec);
                break;
            }
            case 'q': {
                SV sv = va_arg(args, SV);
                fds_fmt_write_quoted_sv(sb, sv);
                break;
            }
            case 'H': {
                uint64_t bytes = va_arg(args, uint64_t);
                fds_fmt_write_human_size(sb, bytes, spec);
                break;
            }
            case 'f': {
                double val = va_arg(args, double);
                fds_fmt_write_double(sb, val, spec);
                break;
            }
            case 's': {
                const char *str = va_arg(args, const char *);
                if (!str) str = "(null)";
                size_t len = 0;
                while (str[len] != '\0') len++;
                fds_fmt_write_str(sb, str, len, spec);
                break;
            }
            case 'd':
            case 'i': {
                int val = va_arg(args, int);
                bool neg = val < 0;
                uint64_t uval = neg ? (uint64_t)(-(val + 1)) + 1 : (uint64_t)val;
                fds_fmt_write_uint64(sb, uval, 10, false, neg, spec);
                break;
            }
            case 'u': {
                unsigned int val = va_arg(args, unsigned int);
                fds_fmt_write_uint64(sb, (uint64_t)val, 10, false, false, spec);
                break;
            }
            case 'x': {
                unsigned int val = va_arg(args, unsigned int);
                fds_fmt_write_uint64(sb, (uint64_t)val, 16, false, false, spec);
                break;
            }
            case 'X': {
                unsigned int val = va_arg(args, unsigned int);
                fds_fmt_write_uint64(sb, (uint64_t)val, 16, true, false, spec);
                break;
            }
            case 'p': {
                uintptr_t val = (uintptr_t)va_arg(args, void *);
                sb_append_sv(sb, sv_from_cstr("0x"));
                if (spec.width >= 2) spec.width -= 2; else spec.width = 0;
                fds_fmt_write_uint64(sb, (uint64_t)val, 16, false, false, spec);
                break;
            }
            default: {
                if (*curr != '\0') {
                    sb_append_sv(sb, sv_from_parts(curr, 1));
                }
                break;
            }
        }
        if (*curr != '\0') curr++;
    }

    return true;
}

bool fds_sbuilder_append_fmt(SB *sb, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    bool result = fds_sbuilder_append_fmtv(sb, fmt, args);
    va_end(args);
    return result;
}

void fds_sbuilder_append_hexdump(SB *sb, SV sv, size_t bytes_per_line) {
    if (!sb || !sv.data || sv.count == 0) return;
    if (bytes_per_line == 0) bytes_per_line = 16;

    static const char hex_chars[] = "0123456789ABCDEF";

    for (size_t i = 0; i < sv.count; i += bytes_per_line) {
        // Офсет
        FdsFmtSpec off_spec = { .zero_pad = true, .width = 8 };
        fds_fmt_write_uint64(sb, (uint64_t)i, 16, false, false, off_spec);
        sb_append_sv(sb, sv_from_cstr("  "));

        // Hex байти
        for (size_t j = 0; j < bytes_per_line; ++j) {
            if (i + j < sv.count) {
                unsigned char b = (unsigned char)sv.data[i + j];
                char hex[3] = { hex_chars[b >> 4], hex_chars[b & 0x0F], ' ' };
                sb_append_sv(sb, sv_from_parts(hex, 3));
            } else {
                sb_append_sv(sb, sv_from_cstr("   "));
            }
            if (j == 7) sb_append_sv(sb, sv_from_cstr(" "));
        }

        sb_append_sv(sb, sv_from_cstr(" |"));

        // ASCII символи
        for (size_t j = 0; j < bytes_per_line && (i + j) < sv.count; ++j) {
            unsigned char b = (unsigned char)sv.data[i + j];
            char c = (b >= 32 && b <= 126) ? (char)b : '.';
            sb_append_sv(sb, sv_from_parts(&c, 1));
        }

        sb_append_sv(sb, sv_from_cstr("|\n"));
    }
}

static SB g_fds_fmt_builder = {0};

const char *fds_fmtv(const char *fmt, va_list args) {
    if (!fmt) return "";
    g_fds_fmt_builder.count = 0; // Очищаємо перед новим форматуванням
    if (!fds_sbuilder_append_fmtv(&g_fds_fmt_builder, fmt, args)) {
        return "";
    }
    // Гарантуємо null-terminator для використання як const char*
    sb_append_sv(&g_fds_fmt_builder, sv_from_parts("", 1));
    return (const char *)g_fds_fmt_builder.items;
}

const char *fds_fmt(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    const char *result = fds_fmtv(fmt, args);
    va_end(args);
    return result;
}

void fds_fmt_free(void) {
    if (g_fds_fmt_builder.items) {
        sb_free(&g_fds_fmt_builder);
        g_fds_fmt_builder.items = NULL;
        g_fds_fmt_builder.capacity = 0;
        g_fds_fmt_builder.count = 0;
    }
}

#endif // FDS_EXT_FORMATTING_IMPLEMENTATION_DONE
#endif // FDS_EXT_FORMATTING_IMPLEMENTATION