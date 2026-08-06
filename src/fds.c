#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FDS_IMPLEMENTATION
#include "fds.h"


void sv_remove_prefix(SV *sv, size_t count)
{
    if (count > sv->count)
        count = sv->count;

    sv->data += count;
    sv->count -= count;
}

char *sv_to_cstr(SV sv)
{
    char *cstr = malloc(sv.count + 1);
    if (cstr == NULL)
    {
        fprintf(stderr, "Out of memory\n");
        abort();
    }

    memcpy(cstr, sv.data, sv.count);
    cstr[sv.count] = '\0';
    return cstr;
}

char sv_at(SV *sv, size_t index)
{
    if (index >= sv->count)
    {
        fprintf(stderr, "Index out of bounds\n");
        abort();
    }
    return sv->data[index];
}



SV sv_new(void)
{
    SV sv = {0};
    return sv;
}

SV sv_from_cstr(const char *str)
{
    SV sv = {0};
    sv.count = strlen(str);
    sv.data = str;
    return sv;
}
SV sv_from_sb(const SB *sb)
{
    SV sv = {0};
    sv.count = sb->count;
    sv.data = sb->items;
    return sv;
}
SV sv_from_parts(const char *str, size_t len)
{
    SV sv;
    sv.data = str;
    sv.count = len;
    return sv;
}

int sv_eq(SV sv1, SV sv2)
{
    if (sv1.count != sv2.count)
        return 0;

    return strncmp(sv1.data, sv2.data, sv1.count) == 0;
}
int sv_eq_cstr(SV sv1, const char *str)
{
    size_t len = strlen(str);
    if (sv1.count != len)
        return 0;

    return strncmp(sv1.data, str, sv1.count) == 0;
}

int main(void)
{
    // SB sb = sb_new();
    // sb_append_n(&sb, "Hello ", 6);
    // sb_append_char(&sb, 'x');
    // sb_append_char(&sb, 'x');
    // sb_append_char(&sb, 'x');
    // sb_append_char(&sb, ' ');
    // sb_append(&sb, "World!\n");
    // printf("%s", sb_to_cstr(&sb));

    // sb_free(&sb); 

    SV sv = sv_from_cstr("Hello World!\n");
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

    return 0;
}