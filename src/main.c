#define FDS_IMPL
#include "fds.h"


int main(int argc, char **argv)
{
    FDS_REBUILD_YOURSELF(argc, argv);
    fds_log(FINFO, "Hello temaune!! hahaha\n");

    fds_auto int* arrInt = New(int, 100);
    
    return 0;
}