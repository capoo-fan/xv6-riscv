#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    int time = atoi(argv[1]);
    sleep(time);
    exit(0);
}
