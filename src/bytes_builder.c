#define FDS_IMPLEMENTATION
#include "fds.h"
#define FDS_EXT_IMPL
#include "fds_ext.h"

#define FDS_EXT_EVENT_IMPL
#include "fds_ext_event.h"

#define FDS_EXT_BYTES_BUILDER_IMPL
#include "fds_ext_bytes_builder.h"
#define FDS_BYTES_VIEW_IMPL
#include "fds_ext_bytes_view.h"

int main()
{
        // 1. Формуємо бінарний пакет
    FdsBytesBuilder builder = fds_bb_create(64); // Початковий розмір

    fds_bb_append_view(&builder, fds_bv_from_cstr("MAGIC")); // Заголовок (5 байт)
    fds_bb_append_u32_le(&builder, 1337);                    // Ціле число (4 байти)
    fds_bb_append_byte(&builder, 0x00);                      // Нульовий байт

    // 2. Перетворюємо на View для читання без копіювання
    FdsBytesView stream = fds_bb_to_view(&builder);

    // 3. Розбираємо пакет
    if (fds_bv_has_prefix(stream, fds_bv_from_cstr("MAGIC"))) {
        stream = fds_bv_skip(stream, 5); // Пропускаємо заголовок
        
        uint32_t my_number;
        if (fds_bv_read_u32_le(&stream, &my_number)) {
            printf("Прочитали число: %u\n", my_number);
        }
    }

    // 4. Звільняємо пам'ять Builder-а
    fds_bb_destroy(&builder);
}