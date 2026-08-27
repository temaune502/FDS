gcc -Wall -Wextra -pedantic -static -static-libgcc -O3 -Isrc  src\fds.c -o build\fds.exe

build\fds.exe -v -i Notes.c -o b.c

c:\Users\temaune\Desktop\DrMemory-Windows-2.6.20434\bin\drmemory.exe -show_reachable -- build\fds.exe -v -i Notes.c -o b.c