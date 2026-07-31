#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>

extern int main(int argc, char** argv);

void _start(void) {
    register long* rsp __asm__("rsp");
    int argc = (int)rsp[0];
    char** argv = (char**)(rsp + 1);
    int ret = main(argc, argv);
    syscall(SYS_exit, ret);
    __builtin_unreachable();
}