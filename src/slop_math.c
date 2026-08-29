#include <stdio.h>

// 1. Обов'язково додаємо макрос реалізації перед підключенням хедера в ОДНОМУ файлі
#define FDS_EXT_MATH_IMPL
#include "fds_ext_math.h"

int main(void) {
    printf("=== FDS Sloppy Math Demo ===\n\n");

    // --- 1. Ініціалізація RNG ---
    // Використовуємо seed, наприклад, системний час або фіксоване число
    FdsRng rng = fds_rng_init(1337); 
    
    printf("[RNG]\n");
    printf("Критичний удар (25%% шанс): %s\n", fds_rng_chance(&rng, 0.25f) ? "ТАК!" : "НІ");
    printf("Випадковий урон (10-25): %d\n\n", fds_rng_rangei(&rng, 10, 25));

    // --- 2. Симуляція руху снаряда (Вектори + Fast Math) ---
    // Снаряд стартує з (0, 5) і рухається вправо, коливаючись по Y
    FdsVec2 proj_start = fds_vec2(0.0f, 5.0f);
    float time = 1.25f; // Уявний час у грі
    float speed = 15.0f;
    
    // Рух по осі X
    float move_x = time * speed;
    // Коливання по осі Y з амплітудою 3.0, використовуємо fds_fast_sinf()
    float move_y = fds_fast_sinf(time * FDS_PI) * 3.0f; 

    // Поточна позиція снаряда
    FdsVec2 proj_pos = fds_vec2_add(proj_start, fds_vec2(move_x, move_y));
    FdsCircle projectile = fds_circle(proj_pos, 2.0f); // Радіус 2.0

    printf("[Вектори та Fast Math]\n");
    printf("Позиція снаряда на час %.2f: X=%.2f, Y=%.2f\n\n", time, proj_pos.x, proj_pos.y);

    // --- 3. Геометрія та Колізії ---
    // Гравець стоїть на позиції (15, 2), розмір 6x6
    FdsAABB player = fds_aabb_from_pos_size(fds_vec2(15.0f, 2.0f), fds_vec2(6.0f, 6.0f));

    printf("[Колізії (AABB vs Circle)]\n");
    printf("Гравець: min(%.1f, %.1f), max(%.1f, %.1f)\n", 
            player.min.x, player.min.y, player.max.x, player.max.y);
            
    if (fds_overlap_aabb_circle(player, projectile)) {
        printf("=> БАМ! Снаряд влучив у гравця!\n\n");
    } else {
        printf("=> ПРОМАХ! Снаряд пролетів повз.\n\n");
    }

    // --- 4. Інтерполяції та Утиліти ---
    float current_hp = 45.0f;
    float max_hp = 120.0f;
    
    // Дізнаємось, скільки це у відсотках (0.0 ... 1.0)
    float hp_percent = fds_inv_lerp(0.0f, max_hp, current_hp);
    
    // Переводимо відсоток у ширину UI смужки здоров'я (від 0 до 200 пікселів)
    float ui_bar_width = fds_lerp(0.0f, 200.0f, hp_percent);
    // Альтернатива в один рядок: fds_remap(current_hp, 0.0f, max_hp, 0.0f, 200.0f);

    printf("[Інтерполяція (UI)]\n");
    printf("Здоров'я: %.1f / %.1f (%.1f%%)\n", current_hp, max_hp, hp_percent * 100.0f);
    printf("Ширина смужки HP: %.1f пікселів\n", ui_bar_width);

    return 0;
}