#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

#define delete_module(name, flags) syscall(__NR_delete_module, name, flags)

int main(int argc, char **argv) {
    if (argc != 2) {
        puts("Usage rm_mod mymodule");
        return EXIT_FAILURE;
    }
    if (delete_module(argv[1], O_NONBLOCK) != 0) {
        //perror("delete_module");
	printf ("rm_mod failed for %s.\n", argv[1]);
        return EXIT_FAILURE;
    }
    printf ("rm_mod successful for %s.\n", argv[1]);
    return EXIT_SUCCESS;
}
