gcc -Wall -Wextra -pedantic -ggdb tests\test_svsb_old.c -I"src" -o build\test_svsb_old.exe

gcc -Wall -Wextra -pedantic -ggdb tests\test_svsb_new.c -I"src" -o build\test_svsb_new.exe
gcc -Wall -Wextra -pedantic -ggdb tests\test_fixed_arena.c -I"src" -o build\test_fixed_arena.exe
gcc -Wall -Wextra -pedantic -ggdb tests\test_temp_arena.c -I"src" -o build\test_temp_arena.exe


gcc -Wall -Wextra -pedantic -ggdb tests\test_da.c -I"src" -o  build\test_da.exe