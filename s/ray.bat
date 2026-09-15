gcc -c src\thing.c -o build\thing.o

gcc -c src\ppm_viewer.c -o build\ppm_viewer.o

gcc build\ppm_viewer.o build\thing.o E:\probes\fds\src\raylib\lib\libraylib.a -lopengl32 -lgdi32 -lwinmm -o ppm_viewer.exe