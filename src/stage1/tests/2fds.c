#define FDS_IMPLEMENTATION
#include "fds.h"
#define FDS_EXT_IMPL
#include "fds_ext.h"

// typedef struct {
//     float x;
//     float y;
//     unsigned long long t;
//     char* name;
// } Hell;

int main()
{   
    // size_t time = (size_t)fds_time_now();
    // char *string = "Hello temaune";
    // FdsFile f = fds_file_open("test.b", FDS_FILE_CREATE | FDS_FILE_WRITE);
    // fds_file_write_magic(&f, "HELL", 1448);
    // fds_file_write(f, (void*)string, strlen(string));

    // fds_file_write(f, (void*)&time, sizeof(size_t));
    // fds_file_close(&f);
    
    void* string = malloc(sizeof(char)*13);
    size_t time = 0;
    FdsFile f = fds_file_open("test.b", FDS_FILE_READ);
    if(!fds_file_check_magic(&f, "HELL", 1448)) exit(1);

    fds_file_read(f, (void*)string, 13);

    fds_file_read(f, (void*)&time, sizeof(size_t));
    fds_file_close(&f);

    fds_log(FINFO, "String from file %s, and time is %lld", string, time);



//    Hell hello = { .x = 45.0f, .y = 69.1f, .t = 4444432, .name = "temaune" };

//     // --- ЗАПИС ---
//     FdsFile f = fds_file_open("test.b", FDS_FILE_CREATE | FDS_FILE_WRITE);
//     if (f.is_valid) {
//         fds_file_write_magic(&f, "HEL1", 1448);

//         // 1. Пишемо фіксовані примітиви
//         fds_file_write(f, &hello.x, sizeof(float));
//         fds_file_write(f, &hello.y, sizeof(float));
//         fds_file_write(f, &hello.t, sizeof(unsigned long long));

//         // 2. Пишемо довжину рядка та самі байти
//         uint32_t name_len = (uint32_t)strlen(hello.name);
//         fds_file_write(f, &name_len, sizeof(uint32_t));
//         fds_file_write(f, hello.name, name_len);

//         fds_file_close(&f);
//     }

//     // --- ЧИТАННЯ ---
//     Hell hello2 = {0};
//     FdsFile f2 = fds_file_open("test.b", FDS_FILE_READ);
//     // if (!fds_file_check_magic(&f2, "HEL1", 1448)) {
//     //     if (f2.is_valid) {
//     //     fds_log(FWARN, "Неверсія або пошкоджений файл!");
//     //     fds_file_close(&f2);
//     //     return 1;
//     // }
//     // }
//     fds_file_skip(f2, sizeof(FdsFileHeader));
//     // 1. Читаємо примітиви
//     fds_file_read(f2, &hello2.x, sizeof(float));
//     fds_file_read(f2, &hello2.y, sizeof(float));
//     fds_file_read(f2, &hello2.t, sizeof(unsigned long long));

//     // 2. Читаємо довжину рядка та виділяємо під нього пам'ять
//     uint32_t name_len = 0;
//     fds_file_read(f2, &name_len, sizeof(uint32_t));

//     // Тут ідеально підійде TempArena, але для тесту використаємо malloc
//     hello2.name = (char*)malloc(name_len + 1);
//     fds_file_read(f2, hello2.name, name_len);
//     hello2.name[name_len] = '\0'; // NUL-terminator

//     fds_file_close(&f2);

//     fds_log(FINFO, "x: %f, y: %f, t: %llu, name: %s", 
//             hello2.x, hello2.y, hello2.t, hello2.name);

//     free(hello2.name);

    // FdsMappedFile mfile = fds_file_map("big.blob", FDS_MAP_READ);

    // FdsFile f = {
    //     .handle = mfile.data,
    //     .is_valid = true,
    // };
    // char buffer[512];
    // fds_file_read(f, &buffer, 500);
    // printf("%s", buffer);

    // SV content = fds_file_mapped_as_sv(&mfile);

    // printf(SV_FMT"\n", SV_ARGS(sv_chop_left(&content, 10)));

    // fds_file_unmap(&mfile);

//     FdsMappedFile mfile = fds_file_map("small.blob", FDS_MAP_READ_WRITE);

//    if (mfile.is_valid) {

//     FILE *f_mem = fmemopen(mfile.data, mfile.size, "w+b");
//     FdsFile f = { .handle = (uintptr_t)f_mem, .is_valid = true };

//     fds_file_seek(f, 12, FDS_SEEK_END);

//     fds_file_write_magic(&f, "test", 9832);
//     // 1. Прямий запис заголовка
//     // FdsFileHeader *hdr = (FdsFileHeader*)mfile.data;
//     // memcpy(hdr->magic, "HEL1", 4);
//     // hdr->version = 1448;
    
//     // 2. Прямий запис структури зі зсувом після заголовка
//     // Hell *hello = (Hell*)((uint8_t*)mfile.data + sizeof(FdsFileHeader));
//     // hello->x = 100.0f;
//     // hello->y = 200.0f;
//     // hello->t = 99999;

//     // 3. (Опціонально) Примусово скидаємо зміни на диск негайно
//     fds_file_close(&f);
//     fds_file_flush_mapped(&mfile);

//     fds_file_unmap(&mfile);
// }


    return 0;
}