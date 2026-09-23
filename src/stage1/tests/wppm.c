#define FDS_IMPL
#include "fds.h"

#define FDS_EXT_PPM_IMPL
#include "fds_ext_ppm.h"

int main()
{
    uint8_t buf[2];
    // printf("%lld", sizeof(buf));s
    // return 0;
    int width = 512;
    int height = width;

    FdsPpmImage image = fds_ppm_create(width, height);
    // FdsFile f = fds_file_open("packet.bin", FDS_FILE_READ);
    // FdsFile f = fds_file_open("build/ppm.exe", FDS_FILE_READ);
    FdsFile f = fds_file_open("src/fds.h", FDS_FILE_READ);
    // FdsFile f = fds_file_open("cring.mp3", FDS_FILE_READ);
    // printf("%p", f.handle);

    int64_t  cursor = fds_file_size(f);
    // printf("%lld", cursor);
    // cursor =71*MB;
    int x, y;
    for(int64_t i = 0; i < cursor; i += 2)
    {
        fds_file_read(f, &buf, 2);
        x = (buf[0] *width)/255;
        y = (buf[1] *height)/255;
        // x = buf[0];
        // y = buf[1];
        fds_ppm_set_pixel(&image, x, y, 255, 255, 255);
        // fds_sleep_ms(500);
    }


        fds_ppm_save(&image, "image.ppm");

    fds_file_close(&f);
    fds_ppm_free(&image);

}