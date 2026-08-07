#define STRIP_PREFIX
#define NOB_IMPLEMENTATION
#include "nob.h"

int main(void)
{
    String_Builder sb = {0};
    sb_append_cstr(&sb, "Hello, World!");
    read_entire_file("nob.h", &sb);
    printf("File content:\n%s\n", sb.items);
    return 0;
}