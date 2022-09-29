
#include <arcs/io.h>

struct _modes {
	char *arg;
	char *modename;
	OPENMODE mode;
} modes[] = {
	"r", "OpenReadOnly", OpenReadOnly,
	"w", "OpenWriteOnly", OpenWriteOnly,
	"rw", "OpenReadWrite", OpenReadWrite,
	"cw", "CreateWriteOnly", CreateWriteOnly,
	"crw", "CreateReadWrite", CreateReadWrite,
	"sw", "SupersedeWriteOnly", SupersedeWriteOnly,
	"srw", "SupersedeReadWrite", SupersedeReadWrite,
	"od", "OpenDirectory", OpenDirectory,
	"cd", "CreateDirectory", CreateDirectory,
	0,0,0
};

main(int argc, char **argv, char **envp)
{
    struct _modes *m;
    char *devname;
    char *mode;
    int found = 0;
    LONG errno;
    ULONG fd;

    mode = argv[1];
    devname = argv[2];
    if (!devname || !mode || index(devname, '=')) {
	printf ("Usage: opendev [r|w|rw|cw|crw|sw|srw|d|cd] <device>\n");
	return;
    }

    for (m = modes; m->arg ; m++) {
	if (!strcmp(m->arg, mode)) {
		found++;
		break;
	}
    }

    if (!found) {
	printf ("Usage: opendev [r|w|rw|cw|crw|sw|srw|d|cd] <device>\n");
	return;
    }

    printf ("Opening %s %s.\n", devname, m->modename);

    errno = Open ((CHAR *)devname, m->mode, &fd);

    if (errno) {
	printf("FAILED: %s\n",devname);
	perror(errno, 0);
    }
    else {
	printf ("%s opened OK\n", devname);
	Close (fd);
    }
    return;
}
