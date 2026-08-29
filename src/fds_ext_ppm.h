/*
    fds_ext_ppm.h — Lightweight PPM Image Generator/Parser (FDS Extension).
    
    Features:
    - Support for binary PPM (P6 format) for efficient storage and speed.
    - Simple pixel manipulation and bitmap creation.
    - Zero external dependencies (uses standard C file I/O).

    Usage:
    In EXACTLY ONE C file, add:
       #define FDS_EXT_PPM_IMPL
       #include "fds_ext_ppm.h"
*/

#ifndef FDS_EXT_PPM_H
#define FDS_EXT_PPM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    uint32_t width;
    uint32_t height;
    uint8_t *data; // RGB24 layout: [R, G, B, R, G, B, ...]
} FdsPpmImage;

#ifndef FDS_EXT_PPM_DEF
    #define FDS_EXT_PPM_DEF extern
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Створення та знищення зображення
FDS_EXT_PPM_DEF FdsPpmImage fds_ppm_create(uint32_t width, uint32_t height);
FDS_EXT_PPM_DEF void fds_ppm_free(FdsPpmImage *img);

// Робота з пікселями
FDS_EXT_PPM_DEF void fds_ppm_set_pixel(FdsPpmImage *img, uint32_t x, uint32_t y, uint8_t r, uint8_t g, uint8_t b);
FDS_EXT_PPM_DEF void fds_ppm_get_pixel(const FdsPpmImage *img, uint32_t x, uint32_t y, uint8_t *r, uint8_t *g, uint8_t *b);

// Заповнення всього зображення одним кольором
FDS_EXT_PPM_DEF void fds_ppm_fill(FdsPpmImage *img, uint8_t r, uint8_t g, uint8_t b);

// Збереження у файл (формат P6) та завантаження
FDS_EXT_PPM_DEF bool fds_ppm_save(const FdsPpmImage *img, const char *filepath);
FDS_EXT_PPM_DEF FdsPpmImage fds_ppm_load(const char *filepath);

#ifdef __cplusplus
}
#endif

#endif // FDS_EXT_PPM_H

/* ============================================================================
   IMPLEMENTATION
   ============================================================================ */
#ifdef FDS_EXT_PPM_IMPL
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

FDS_EXT_PPM_DEF FdsPpmImage fds_ppm_create(uint32_t width, uint32_t height) {
    FdsPpmImage img = { .width = width, .height = height, .data = NULL };
    if (width == 0 || height == 0) return img;

    size_t data_size = (size_t)width * height * 3;
    img.data = (uint8_t *)malloc(data_size);
    if (img.data) {
        memset(img.data, 0, data_size);
    }
    return img;
}

FDS_EXT_PPM_DEF void fds_ppm_free(FdsPpmImage *img) {
    if (!img) return;
    if (img->data) {
        free(img->data);
        img->data = NULL;
    }
    img->width = 0;
    img->height = 0;
}

FDS_EXT_PPM_DEF void fds_ppm_set_pixel(FdsPpmImage *img, uint32_t x, uint32_t y, uint8_t r, uint8_t g, uint8_t b) {
    if (!img || !img->data || x >= img->width || y >= img->height) return;
    size_t idx = ((size_t)y * img->width + x) * 3;
    img->data[idx + 0] = r;
    img->data[idx + 1] = g;
    img->data[idx + 2] = b;
}

FDS_EXT_PPM_DEF void fds_ppm_get_pixel(const FdsPpmImage *img, uint32_t x, uint32_t y, uint8_t *r, uint8_t *g, uint8_t *b) {
    if (!img || !img->data || x >= img->width || y >= img->height) return;
    size_t idx = ((size_t)y * img->width + x) * 3;
    if (r) *r = img->data[idx + 0];
    if (g) *g = img->data[idx + 1];
    if (b) *b = img->data[idx + 2];
}

FDS_EXT_PPM_DEF void fds_ppm_fill(FdsPpmImage *img, uint8_t r, uint8_t g, uint8_t b) {
    if (!img || !img->data) return;
    size_t total_pixels = (size_t)img->width * img->height;
    for (size_t i = 0; i < total_pixels; ++i) {
        size_t idx = i * 3;
        img->data[idx + 0] = r;
        img->data[idx + 1] = g;
        img->data[idx + 2] = b;
    }
}

FDS_EXT_PPM_DEF bool fds_ppm_save(const FdsPpmImage *img, const char *filepath) {
    if (!img || !img->data || !filepath) return false;

    FILE *f = fopen(filepath, "wb");
    if (!f) return false;

    // Заголовок формату P6 (Binary PPM)
    // P6\n<width> <height>\n<maxval>\n
    fprintf(f, "P6\n%u %u\n255\n", img->width, img->height);
    
    size_t data_size = (size_t)img->width * img->height * 3;
    size_t written = fwrite(img->data, 1, data_size, f);
    
    fclose(f);
    return written == data_size;
}

FDS_EXT_PPM_DEF FdsPpmImage fds_ppm_load(const char *filepath) {
    FdsPpmImage img = { 0, 0, NULL };
    if (!filepath) return img;

    FILE *f = fopen(filepath, "rb");
    if (!f) return img;

    char magic[3];
    if (fscanf(f, "%2s", magic) != 1 || strcmp(magic, "P6") != 0) {
        fclose(f);
        return img;
    }

    uint32_t width = 0, height = 0, maxval = 0;
    if (fscanf(f, "%u %u %u", &width, &height, &maxval) != 3 || maxval != 255) {
        fclose(f);
        return img;
    }

    // Пропускаємо один білий символ після заголовочної секції (зазвичай \n)
    fgetc(f);

    img = fds_ppm_create(width, height);
    if (!img.data) {
        fclose(f);
        return img;
    }

    size_t data_size = (size_t)width * height * 3;
    size_t read_bytes = fread(img.data, 1, data_size, f);
    fclose(f);

    if (read_bytes != data_size) {
        fds_ppm_free(&img);
    }
    return img;
}

#endif // FDS_EXT_PPM_IMPL