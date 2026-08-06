#define NOB_IMPLEMENTATION
#define NOT_STRIP_PREFIX
#include "nob.h"

int main(int argc, char **argv)
{
    NOB_GO_REBUILD_URSELF(argc, argv);

    //TODO("Hello world");
    int sum = (69*69)<<7227;
    printf("%d", sum);
}