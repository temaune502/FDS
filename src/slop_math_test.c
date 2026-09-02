#define FDS_EXT_MATH_IMPL
#include "fds_ext_math.h"

#include <stdio.h>
#include <math.h>
#include <time.h>
#include <stdlib.h>

// Кількість ітерацій для тесту швидкості (10 мільйонів)
#define ITERATIONS 10000000

// Функція для отримання поточного часу в секундах
double get_time() {
    return (double)clock() / CLOCKS_PER_SEC;
}

// ============================================================================
// ТЕСТИ ШВИДКОДІЇ ТА ТОЧНОСТІ
// ============================================================================

void test_sqrt() {
    double start, end, time_std, time_fds;
    volatile float sum = 0.0f; // volatile забороняє компілятору оптимізувати/видалити цикл
    
    // 1. Тест стандартної sqrtf
    start = get_time();
    for (int i = 1; i <= ITERATIONS; i++) {
        sum += sqrtf((float)i);
    }
    end = get_time();
    time_std = end - start;

    // 2. Тест fds_fast_sqrtf
    sum = 0.0f;
    start = get_time();
    for (int i = 1; i <= ITERATIONS; i++) {
        sum += fds_fast_sqrtf((float)i);
    }
    end = get_time();
    time_fds = end - start;

    // 3. Перевірка максимальної похибки на діапазоні [0.1, 1000.0]
    float max_error = 0.0f;
    for (float x = 0.1f; x <= 1000.0f; x += 0.1f) {
        float err = fds_absf(sqrtf(x) - fds_fast_sqrtf(x));
        if (err > max_error) max_error = err;
    }

    printf("%-10s | %-10.5f | %-10.5f | %.6f\n", "sqrtf", time_std, time_fds, max_error);
}

void test_inv_sqrt() {
    double start, end, time_std, time_fds;
    volatile float sum = 0.0f;
    
    // 1. Тест стандартної 1.0f / sqrtf
    start = get_time();
    for (int i = 1; i <= ITERATIONS; i++) {
        sum += 1.0f / sqrtf((float)i);
    }
    end = get_time();
    time_std = end - start;

    // 2. Тест fds_fast_inv_sqrtf (Fast Inverse Square Root)
    sum = 0.0f;
    start = get_time();
    for (int i = 1; i <= ITERATIONS; i++) {
        sum += fds_fast_inv_sqrtf((float)i);
    }
    end = get_time();
    time_fds = end - start;

    // 3. Похибка на діапазоні [0.1, 1000.0]
    float max_error = 0.0f;
    for (float x = 0.1f; x <= 1000.0f; x += 0.1f) {
        float err = fds_absf((1.0f / sqrtf(x)) - fds_fast_inv_sqrtf(x));
        if (err > max_error) max_error = err;
    }

    printf("%-10s | %-10.5f | %-10.5f | %.6f\n", "inv_sqrtf", time_std, time_fds, max_error);
}

void test_sin() {
    double start, end, time_std, time_fds;
    volatile float sum = 0.0f;
    
    // Генеруємо значення в межах [-PI, PI]
    float step_speed = FDS_TAU / ITERATIONS;

    // 1. Тест стандартної sinf
    start = get_time();
    for (int i = 0; i < ITERATIONS; i++) {
        sum += sinf(-FDS_PI + (i * step_speed));
    }
    end = get_time();
    time_std = end - start;

    // 2. Тест fds_fast_sinf
    sum = 0.0f;
    start = get_time();
    for (int i = 0; i < ITERATIONS; i++) {
        sum += fds_fast_sinf(-FDS_PI + (i * step_speed));
    }
    end = get_time();
    time_fds = end - start;

    // 3. Похибка на діапазоні [-PI, PI]
    float max_error = 0.0f;
    for (float x = -FDS_PI; x <= FDS_PI; x += 0.001f) {
        float err = fds_absf(sinf(x) - fds_fast_sinf(x));
        if (err > max_error) max_error = err;
    }

    printf("%-10s | %-10.5f | %-10.5f | %.6f\n", "sinf", time_std, time_fds, max_error);
}

void test_cos() {
    double start, end, time_std, time_fds;
    volatile float sum = 0.0f;
    float step_speed = FDS_TAU / ITERATIONS;

    // 1. Тест стандартної cosf
    start = get_time();
    for (int i = 0; i < ITERATIONS; i++) {
        sum += cosf(-FDS_PI + (i * step_speed));
    }
    end = get_time();
    time_std = end - start;

    // 2. Тест fds_fast_cosf
    sum = 0.0f;
    start = get_time();
    for (int i = 0; i < ITERATIONS; i++) {
        sum += fds_fast_cosf(-FDS_PI + (i * step_speed));
    }
    end = get_time();
    time_fds = end - start;

    // 3. Похибка на діапазоні [-PI, PI]
    float max_error = 0.0f;
    for (float x = -FDS_PI; x <= FDS_PI; x += 0.001f) {
        float err = fds_absf(cosf(x) - fds_fast_cosf(x));
        if (err > max_error) max_error = err;
    }

    printf("%-10s | %-10.5f | %-10.5f | %.6f\n", "cosf", time_std, time_fds, max_error);
}

int main(void) {
    printf("Починаємо тестування (%d ітерацій)...\n", ITERATIONS);
    printf("=========================================================\n");
    printf("%-10s | %-10s | %-10s | %s\n", "Функція", "Std Time(s)", "FDS Time(s)", "Max Error (Diff)");
    printf("---------------------------------------------------------\n");

    test_sqrt();
    test_inv_sqrt();
    test_sin();
    test_cos();

    printf("=========================================================\n");
    return 0;
}