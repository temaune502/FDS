#define FDS_IMPL
#include "fds__cpp.h"

#define FDS_EXT_INPUT_IMPLEMENTATION
#include "fds_ext_input.h"

int main(void) {
    fds_input_set_auto_history(true);

    SB name = input("Name: ");
    int64_t age  = fds_input_int("Age [18]: ", 18);
    double  hp   = fds_input_float("HP [100.0]: ", 100.0);

    printf("\nГPlayer: %.*s | Age: %lld | HP: %.1f\n\n", 
           (int)name.count, name.items, age, hp);

    // Перегляд історії вводу
    printf("Input history (%zu records):\n", fds_input_history_count());
    for (size_t i = 0; i < fds_input_history_count(); i++) {
        SB item = fds_input_history_get(i);
        printf("  [%zu] %.*s\n", i, (int)item.count, item.items);
    }

    fds_input_cleanup();
    return 0;
}