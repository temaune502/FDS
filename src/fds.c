#define FDS_IMPLEMENTATION
#include "fds.h"



int main(int argc, char **argv) {
    fds_cli_init(&argc, &argv);
    FlagSet *flag = flagset_new();

    char *name = NULL;

    flagset_string(flag, &name,"config", "config.ini", "Your name");
    // flagset_required(flag);

    flagset_parse(flag, argc, argv);

    SV sname = sv_from_cstr(name);
    printf("loading config file "SV_FMT"\n", SV_ARGS(sname));
    // SB file_content = sb_new();
    SB file_content = {0};
    fds_file_read_to_sb(name, &file_content);

    SV content = sv_from_sb(&file_content);
    // printf(SV_FMT"\n", SV_ARGS(content));
    
    IniConfig config = ini_parse_sv(content);
    int port = ini_get_int(&config, "database1", "port", 69);
    printf("Port: %d", port);
    ini_free(&config);
    flagset_free(flag);
    return 0;
}