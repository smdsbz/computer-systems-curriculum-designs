#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char **argv) {
    static char *srcpath, *dstpath;
    long retcode;
    if (argc != 3) {
        printf("%s\n", "Usage: ./mycopy SOURCE TARGET");
        return -1;
    } else {
        srcpath = argv[1];
        dstpath = argv[2];
    }
    retcode = syscall(336L, srcpath, dstpath);
    return (int)retcode;
}
