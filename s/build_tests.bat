gcc -Wall -Wextra -pedantic -ggdb tests\test_svsb_old.c -I"src" -o build\test_svsb_old.exe

gcc -Wall -Wextra -pedantic -ggdb tests\test_svsb_new.c -I"src" -o build\test_svsb_new.exe
gcc -Wall -Wextra -pedantic -ggdb tests\test_fixed_arena.c -I"src" -o build\test_fixed_arena.exe
gcc -Wall -Wextra -pedantic -ggdb tests\test_temp_arena.c -lpthread -I"src" -o build\test_temp_arena.exe
gcc -Wall -Wextra -pedantic -ggdb tests\test_chunked_arena.c -I"src" -o build\test_chunked_arena.exe
gcc -Wall -Wextra -pedantic -ggdb tests\test_alloc.c -I"src" -o build\test_alloc.exe
gcc -Wall -Wextra -pedantic -ggdb tests\test_onyx_format.c -I"src" -o build\test_onyx_format.exe


gcc -Wall -Wextra -pedantic -ggdb tests\test_da.c -I"src" -o  build\test_da.exe