// #define FDS_IMPL
#include "fds.h"

#include <string.h>
int main()
{
    size_t image_x = 2048/4;
    size_t image_y = image_x;
    size_t image_size = sizeof(char)*image_x*image_x*3;
    //unsigned char *image = malloc(image_size);
    char* ppm_header = temp_arena_sprintf(temp_arena_get(), "P6\n%d %d\n255\n", image_x, image_y);


    FdsBytesBuilder data = fds_bb_create(200*KB);
    FdsFile f = fds_file_open("build/soc2.exe", FDS_FILE_READ);
    fds_bb_reserve(&data, image_size);
    data.size = fds_file_read(f,data.data, image_size);

    fds_file_close(&f);

    f = fds_file_open("test3.ppm", FDS_FILE_READ | FDS_FILE_WRITE | FDS_FILE_CREATE);
    fds_file_write(f, ppm_header, strlen(ppm_header));
    fds_file_write(f, data.data, data.size);
    fds_file_close(&f);



}