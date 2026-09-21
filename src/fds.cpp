#define DEBUG_MEM
#define DANGER_THINGS_OFF
#define FDS_IMPL
#include "fds.h"
#include <iostream>

int main()
{
    fds_allocator *alloca = fds_allocator_create();
    fds_allocator_push(alloca);
    SB conf {};

    if(!fds_read_entire_file("config.ini", &conf.items, &conf.count)) fds_log(FFATAL, "Could not read file!");


    std::cout << "hello" << std::endl;

    // IniConfig confI = ini_parse_sv(sv_from_sb(&conf));
    // fds_log(FINFO, "Hello temaune !");
    // int f = ini_get_int(&confI, "World", "f", 69);
    
    // fds_log(FINFO, "%d", f);
    FDS_FREE(conf.items);
    // ini_free(&confI);
    fds_allocator_print_stats(fds_allocator_current());
    fds_allocator_pop();
    return 0;
}