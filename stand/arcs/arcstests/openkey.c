
#include <arcs/io.h>

char *devname = "key()keyboard()";

main(int argc, char **argv, char **envp)
{
    LONG fd, errno;

    errno = Open ((CHAR *)devname, OpenReadOnly, &fd);

    if (errno)
	perror(errno, devname);
    else {
	printf ("%s opened OK\n", devname);
	Close (fd);
    }
    return;
}
