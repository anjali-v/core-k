#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

#define init_module(module_image, len, param_values) syscall(__NR_init_module, module_image, len, param_values)
#define finit_module(fd, param_values, flags) syscall(__NR_finit_module, fd, param_values, flags)

int main(int argc, char **argv) {
    const char *params;
    int fd, use_finit;
    size_t image_size;
    struct stat st;
    void *image;

    /* CLI handling. */
    if (argc < 2) {
        puts("Usage ins_mod mymodule.ko [args="" [use_finit=0]");
        return EXIT_FAILURE;
    }
    if (argc < 3) {
        params = "";
    } else {
        params = argv[2];
    }
    if (argc < 4) {
        use_finit = 0;
    } else {
        use_finit = (argv[3][0] != '0');
    }

    /* Action. */
    fd = open(argv[1], O_RDONLY);
	if (fd < 0) {
		printf ("open failed for %s.\n", argv[1]);
		return EXIT_FAILURE;
	}
    if (use_finit) {
        puts("finit");
        if (finit_module(fd, params, 0) != 0) {
            perror("finit_module");
            return EXIT_FAILURE;
	}
        close(fd);
    } else {
        //puts("init");
        fstat(fd, &st);
        image_size = st.st_size;
	//printf ("img size=%d\n", image_size);
        image = malloc(image_size);
	if (!image) {
		printf ("malloc failed\n");
		return EXIT_FAILURE;
	}
        read (fd, image, image_size);
        close(fd);
        if (init_module(image, image_size, params) != 0) {
            //perror("init_module");
	    printf ("ins_mod failed for %s.\n", argv[1]);
            return EXIT_FAILURE;
        }
        free(image);
	printf ("ins_mod successful for %s.\n", argv[1]);
    }
    return EXIT_SUCCESS;
}
