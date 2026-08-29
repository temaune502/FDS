#define FDS_IMPLEMENTATION
#include "fds.h"
#define FDS_EXT_IMPL
#include "fds_ext.h"

#define FDS_EXT_BYTES_BUILDER_IMPL
#define FDS_EXT_BYTES_IMPL
#include "fds_ext_bytes.h"

#define FDS_EXT_COMPRESS_IMPL
#include "fds_ext_compress.h"

#include <math.h>

typedef struct
{
    char r;
    char g;
    char b;
} Pixel;

Pixel draw(int x, int y)
{   
    Pixel pixel = {0};
    // Простий градієнт з повторенням байтів
    pixel.r = (x / 16) * 10; 
    pixel.g = (y / 16) * 10;
    pixel.b = (x + y) / 32;
    return pixel;
}
int main()
{   

    size_t image_x = 128;
    size_t image_y = image_x;
    size_t image_size = sizeof(char)*image_x*image_x*3;
    unsigned char *image = malloc(image_size);
    char* ppm_header = temp_arena_sprintf(temp_arena_get(), "P6\n%d %d\n255\n", image_x, image_y);
    memset(image, 255, image_size);

    for(size_t x = 0; x < image_x; ++x)
    {   
        for(size_t y = 0; y < image_y; ++y)
        {
            int pixel = (y * image_x + x) * 3;
            Pixel p = draw(x,y);
            image[pixel] =   p.r;
            image[pixel+1] = p.g;
            image[pixel+2] = p.b;
    
        }
    }




    // 1. Маємо якісь сирі дані у Builder-і (наприклад, згенерована карта рівня)
    FdsBytesBuilder raw_map = fds_bb_create(1024);
    // ... додаємо дані в raw_map ...
    // fds_bb_append(&raw_map, ppm_header,strlen(ppm_header));
    fds_bb_append(&raw_map, image, image_size);
    // 2. Стискаємо їх
    FdsBytesBuilder compressed = fds_bb_create(0);
    fds_ext_compress_lz(fds_bb_to_view(&raw_map), &compressed);
    
    FdsBytesBuilder decompressed = fds_bb_create(0);
    fds_ext_decompress_lz(fds_bb_to_view(&compressed), &decompressed);

    fds_log(FINFO, "Siez of date before compressio: %lld", image_size);
    fds_log(FINFO, "Siez of date after compressio:  %d", compressed.size);
    fds_log(FINFO, "Siez of decompressed data    :  %d", decompressed.size);



    FdsFile f = fds_file_open("test.ppm", FDS_FILE_READ | FDS_FILE_WRITE | FDS_FILE_CREATE);
    // memcpy(image, "Привіт світ!", strlen("Привіт світ!"));
    // fds_file_write(f, ppm_header, strlen(ppm_header));
    // fds_file_write(f, decompressed.data, decompressed.size);
    fds_file_write(f, ppm_header, strlen(ppm_header));
    size_t bytes_writen = fds_file_write(f, compressed.data, compressed.size);
    fds_log(FINFO, "Bytes writen %lld", bytes_writen);
    fds_file_close(&f);
    
    f = fds_file_open("test2.ppm", FDS_FILE_READ | FDS_FILE_WRITE | FDS_FILE_CREATE);
    
    fds_file_write(f, ppm_header, strlen(ppm_header));

    fds_file_write(f, decompressed.data, decompressed.size);

    
    fds_file_close(&f);

    
    return 0;
}