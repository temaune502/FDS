#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>

#define FDS_IMPLEMENTATION
#include "fds.h"

#define da_foreach(Type, it, da) \
    for (Type *it = (da)->items; it < (da)->items + (da)->count; ++it)


int main(void)
{
    SV sv = sv_from_cstr("Привіт Світ!");
    printf("count = %zu", sv.count);

    return 0;
}