#define FDS_IMPLEMENTATION
#include "fds.h"
#define FDS_EXT_IMPL
#include "fds_ext.h"

 

int main()
{
    size_t image_x = 128;
    size_t image_y = 128;
    size_t image_size = sizeof(char)*image_x*image_x*3;
    unsigned char *image = malloc(image_size);
    char* ppm_header = temp_arena_sprintf(temp_arena_get(), "P6\n%d %d\n255\n", image_x, image_y);
    memset(image, 255, image_size);

    for(size_t x = 0; x < image_x; ++x)
    {   
        for(size_t y = 0; y < image_y; ++y)
        {
            int pixel = (y * image_x + x) * 3;
            if( y % 2 || !(x % 2) )  {image[pixel+0] = 18;image[pixel+1] = 18;image[pixel+2] = 18; }
            else {image[pixel+0] = 0;image[pixel+1] = 0;image[pixel+2] = 94; }
    
        }
    }
    FdsFile f = fds_file_open("test.ppm", FDS_FILE_READ | FDS_FILE_WRITE | FDS_FILE_CREATE);

    fds_file_write(f, ppm_header, strlen(ppm_header));
    fds_file_write(f, image, image_size);
    fds_file_close(&f);
    free(image);
    temp_arena_destroy();
}