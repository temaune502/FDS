#include <stdio.h>

int main(void)
{
    fwrite("Hello World\00", 1, 13, stdout);
    return 0;
}