
#include "fds.h"
#include "fds_ext_ppm.h"
#include "raylib/include/raylib.h"
#include <string.h>

int main()
{

    char file[1024];
    Texture2D tex = {0};
    bool dropped = false;
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(800, 600, "PPM view");
    SetTargetFPS(60);
    while (!WindowShouldClose())
    {

        if (IsFileDropped())
        {
            FilePathList droppedFiles = LoadDroppedFiles();

            memcpy(&file, droppedFiles.paths[0], strlen(droppedFiles.paths[0]));

            UnloadDroppedFiles(droppedFiles);

            FdsPpmImage image = fds_ppm_load(file);
            Image im = {.data = image.data,
                        .format = PIXELFORMAT_UNCOMPRESSED_R5G6B5,
                        .mipmaps = 4,
                        .height = image.height,
                        .width = image.width};
            tex = LoadTextureFromImage(im);
            dropped = true;
        }

        BeginDrawing();

        if (dropped)
            DrawTexture(tex, 100, 100, WHITE);

        ClearBackground((Color){.r = 18, .g = 18, .b = 18, .a = 0});
        EndDrawing();
    }
}