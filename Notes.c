#include <stdio.h>

#define FDS_IMPLEMENTATION
#include "src/fds.h"

int main(void)
{    SV sv = sv_from_cstr("Hello World!\n");
    SB sb = sb_from_cstr("SB: ");
    sb_append_sv(&sb, sv);
    printf("%s", sb_to_cstr(&sb));
    printf("SV: "SV_FMT, SV_ARGS(sv));

    SV sv2 = sv_from_parts("Partial String", 14);
    printf("SV2: "SV_FMT"\n", SV_ARGS(sv2));
    printf("SV2: "SV_FMT"\n", SV_ARGS(sv2));

    SV sv3 = sv_from_cstr("Hello  World!\n");
    if (sv_eq(sv, sv3))
    {
        printf("SV and SV3 are equal\n");
    }
    else
    {
        printf("SV and SV3 are NOT equal\n");
    }
    ////////////////////////////////////////////////////////////////////////
    if (sv_eq_cstr(sv, "Hello World!\n"))
    {
        printf("SV and C-string are equal\n");
    }
    else
    {
        printf("SV and C-string are NOT equal\n");
    }

    SV sv4 = sv_from_cstr("Hello World!\n");
    sv_remove_prefix(&sv4, 6);
    printf("SV4 after removing prefix: "SV_FMT"\n", SV_ARGS(sv4));
    char c = sv_at(&sv4, 0);
    printf("First character of SV4: %c\n", c);
    char* string = sv_to_cstr(sv4);
    printf("SV4 as C-string: %s\n", string);
    SV svx = sv_from_cstr("This is very huge sstring fdfsfuw wenfiwjfhiwb fiw hwqf qiwh fi2u34b2 ih r4i5b 34iu 34iutb 34urb349urb 439u r3rih 34ihrb itbohu234b h3bor243nrg3nr8x234986rt390-4xiucth7n238r yufgvryg89u\n");
    for (size_t i = 0; i < svx.count; i++)
    {
        printf("Character at index %zu: %c\n", i, sv_at(&svx, i));
    }


}