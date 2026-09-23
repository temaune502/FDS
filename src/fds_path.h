#ifndef FDS_PATH_H
#define FDS_PATH_H

#include <stdbool.h>
#include <stddef.h>

#if defined(_WIN32) || defined(__NT__)
    #define FDS_PATH_SEP '\\'
    #define FDS_PATH_SEP_STR "\\"
    #define FDS_PATH_ALT_SEP '/'
#else
    #define FDS_PATH_SEP '/'
    #define FDS_PATH_SEP_STR "/"
    #define FDS_PATH_ALT_SEP '\\'
#endif

/* ============================================================================
 *  ПЕРЕВІРКИ ТА СЕГМЕНТАЦІЯ (Zero-Allocation, повертають SV)
 * ============================================================================ */

static inline bool fds_path_is_sep(char c) {
    return c == '/' || c == '\\';
}

static inline bool fds_path_is_abs(SV path) {
    if (path.count == 0) return false;
    if (fds_path_is_sep(path.data[0])) return true;
#if defined(_WIN32) || defined(__NT__)
    if (path.count >= 3 && path.data[1] == ':' && fds_path_is_sep(path.data[2])) {
        return true;
    }
#endif
    return false;
}

static inline bool fds_path_is_rel(SV path) {
    return !fds_path_is_abs(path);
}

// Повертає ім'я файлу або останню папку (наприклад, "file.txt" для "/a/b/file.txt")
static inline SV fds_path_basename(SV path) {
    if (path.count == 0) return sv_from_parts("", 1);

    // Ігноруємо кінцеві роздільники (окрім випадку, коли шлях складається лише з них)
    size_t end = path.count;
    while (end > 1 && fds_path_is_sep(path.data[end - 1])) {
        end--;
    }

    size_t start = end;
    while (start > 0 && !fds_path_is_sep(path.data[start - 1])) {
        start--;
    }

    return sv_slice(path, start, end);
}

// Повертає директорію верхнього рівня (наприклад, "/a/b" для "/a/b/file.txt")
static inline SV fds_path_dirname(SV path) {
    if (path.count == 0) return sv_from_parts(".", 2);

    size_t end = path.count;
    while (end > 1 && fds_path_is_sep(path.data[end - 1])) {
        end--;
    }

    size_t i = end;
    while (i > 0 && !fds_path_is_sep(path.data[i - 1])) {
        i--;
    }

    if (i == 0) return sv_from_parts(".", 2);

    // Видаляємо зайві роздільники перед директорією
    while (i > 1 && fds_path_is_sep(path.data[i - 1])) {
        i--;
    }

    return sv_slice(path, 0, i);
}

// Повертає розширення з крапкою (наприклад, ".txt" для "file.txt")
static inline SV fds_path_ext(SV path) {
    SV base = fds_path_basename(path);
    if (base.count == 0) return sv_from_parts("", 1);

    size_t i = base.count;
    while (i > 0) {
        i--;
        if (base.data[i] == '.') {
            // Приховувані файли (типу .gitignore) без іншого розширення не вважаються розширенням
            if (i == 0) return sv_from_parts("", 1);
            sv_slice(&base, i, base.count);
            return base;
        }
    }

    return sv_from_parts("", 1);
}

// Повертає ім'я файлу без розширення (наприклад, "file" для "/a/b/file.txt")
static inline SV fds_path_stem(SV path) {
    SV base = fds_path_basename(path);
    SV ext = fds_path_ext(path);
    sv_slice(&base, 0, base.count - ext.count);
    return  base;
}

// Перевіряє, чи закінчується шлях вказаним розширенням
static inline bool fds_path_has_ext(SV path, SV ext) {
    SV actual_ext = fds_path_ext(path);
    return sv_eq(actual_ext, ext);
}

/* ============================================================================
 *  ТРАНСФОРМАЦІЯ ТА ЗБИРАННЯ ШЛЯХІВ (Запис у SB)
 * ============================================================================ */

// Об'єднання двох частин шляху
static inline void fds_path_join_sb(SB *sb, SV head, SV tail) {
    if (tail.count > 0 && fds_path_is_abs(tail)) {
        sb_append_sv(sb, tail);
        return;
    }

    if (head.count > 0) {
        sb_append_sv(sb, head);
        bool head_ends_sep = fds_path_is_sep(head.data[head.count - 1]);
        bool tail_starts_sep = (tail.count > 0) && fds_path_is_sep(tail.data[0]);

        if (!head_ends_sep && !tail_starts_sep) {
            sb_append_char(sb, FDS_PATH_SEP);
        } else if (head_ends_sep && tail_starts_sep) {
            sv_slice(&tail, 1, tail.count);
        }
    }

    sb_append_sv(sb, tail);
}

// Нормалізація роздільників та розділення на сегменти без `.` та `..`
static inline void fds_path_normalize_sb(SB *sb, SV path) {
    if (path.count == 0) {
        sb_append_sv(sb, sv_from_parts(".", 2));
        return;
    }

    bool is_abs = fds_path_is_abs(path);
    
    // Масив для збереження сегментів для обробки `.` та `..`
    SV segments[64];
    size_t seg_count = 0;

    size_t start = 0;
    for (size_t i = 0; i <= path.count; ++i) {
        if (i == path.count || fds_path_is_sep(path.data[i])) {
            if (i > start) {
                SV seg = sv_slice(path, start, i);
                if (sv_eq(seg, sv_from_parts(".", 2))) {
                    // Ігноруємо поточну директорію
                } else if (sv_eq(seg, sv_from_parts("..", 3))) {
                    if (seg_count > 0 && !sv_eq(segments[seg_count - 1], sv_from_parts("..", 3))) {
                        seg_count--;
                    } else if (!is_abs) {
                        if (seg_count < 64) segments[seg_count++] = seg;
                    }
                } else {
                    if (seg_count < 64) segments[seg_count++] = seg;
                }
            }
            start = i + 1;
        }
    }

    if (is_abs) {
        sb_append_char(sb, FDS_PATH_SEP);
    }

    if (seg_count == 0 && !is_abs) {
        sb_append_sv(sb, sv_from_parts(".", 2));
        return;
    }

    for (size_t i = 0; i < seg_count; ++i) {
        if (i > 0) sb_append_char(sb, FDS_PATH_SEP);
        sb_append_sv(sb, segments[i]);
    }
}

// Зміна розширення шляху
static inline void fds_path_change_ext_sb(SB *sb, SV path, SV new_ext) {
    SV dir = fds_path_dirname(path);
    SV stem = fds_path_stem(path);

    if (!sv_eq(dir, sv_from_parts(".", 2)) && dir.count > 0) {
        sb_append_sv(sb, dir);
        sb_append_char(sb, FDS_PATH_SEP);
    }
    
    sb_append_sv(sb, stem);

    if (new_ext.count > 0) {
        if (new_ext.data[0] != '.') {
            sb_append_char(sb, '.');
        }
        sb_append_sv(sb, new_ext);
    }
}

// Побудова відносного шляху від base до path
static inline void fds_path_rel_sb(SB *sb, SV path, SV base) {
    SV p_seg[32], b_seg[32];
    size_t p_count = 0, b_count = 0;

    // Розбираємо path
    size_t start = 0;
    for (size_t i = 0; i <= path.count; ++i) {
        if (i == path.count || fds_path_is_sep(path.data[i])) {
            if (i > start && p_count < 32) p_seg[p_count++] = sv_slice(path, start, i);
            start = i + 1;
        }
    }

    // Розбираємо base
    start = 0;
    for (size_t i = 0; i <= base.count; ++i) {
        if (i == base.count || fds_path_is_sep(base.data[i])) {
            if (i > start && b_count < 32) b_seg[b_count++] = sv_slice(base, start, i);
            start = i + 1;
        }
    }

    // Знаходимо спільний префікс
    size_t common = 0;
    while (common < p_count && common < b_count && sv_eq(p_seg[common], b_seg[common])) {
        common++;
    }

    if (common == 0 && p_count == 0 && b_count == 0) {
        sb_append_sv(sb, sv_from_parts("."));
        return;
    }

    // Піднімаємося вгору для решти елементів base
    size_t up_steps = b_count - common;
    for (size_t i = 0; i < up_steps; ++i) {
        if (i > 0) sb_append_char(sb, FDS_PATH_SEP);
        sb_append_sv(sb, sv_from_parts(".."));
    }

    // Додаємо залишок з path
    for (size_t i = common; i < p_count; ++i) {
        if (up_steps > 0 || i > common) sb_append_char(sb, FDS_PATH_SEP);
        sb_append_sv(sb, p_seg[i]);
    }

    if (sb->count == 0) {
        sb_append_sv(sb, sv_from_parts("."));
    }
}

// Канонінізація (нормалізація + гарантований форматинг)
static inline void fds_path_canonicalize_sb(SB *sb, SV path) {
    fds_path_normalize_sb(sb, path);
}

#endif // FDS_PATH_H