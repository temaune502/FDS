@echo off
setlocal
set "GCC=E:\msys\ucrt64\bin\gcc.exe"
if defined FDS_GCC set "GCC=%FDS_GCC%"
set "PATH=E:\msys\ucrt64\bin;E:\msys\usr\bin;%PATH%"
if not exist build mkdir build
"%GCC%" -std=c11 -Wall -Wextra -pedantic -Isrc -Isrc\raylib\include src\fds2_dsl_raylib_game.c src\raylib\lib\libraylib.a -lopengl32 -lgdi32 -lwinmm -lws2_32 -o build\fds2_dsl_raylib_game.exe
if errorlevel 1 exit /b %errorlevel%
build\fds2_dsl_raylib_game.exe
