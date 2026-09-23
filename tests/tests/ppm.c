#define FDS_IMPLEMENTATION
#include "fds.h"
#define FDS_EXT_IMPL
#include "fds_ext.h"

#define FDS_EXT_EVENT_IMPL
#include "fds_ext_event.h"

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

    pixel.r = cos(x)*y;
    pixel.g = cos(y)*x;
    pixel.b = abs(pixel.r+pixel.g);



    return pixel;
}



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
            Pixel p = draw(x,y);
            image[pixel] =   p.r;
            image[pixel+1] = p.g;
            image[pixel+2] = p.b;
    
        }
    }
    FdsFile f = fds_file_open("test.ppm", FDS_FILE_READ | FDS_FILE_WRITE | FDS_FILE_CREATE);
    memcpy(image, "Привіт світ!", strlen("Привіт світ!"));
    fds_file_write(f, ppm_header, strlen(ppm_header));
    fds_file_write(f, image, image_size);
    fds_file_close(&f);
    free(image);
    temp_arena_destroy();
}