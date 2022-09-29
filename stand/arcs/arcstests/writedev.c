
#include <arcs/io.h>

char *writestring = "Hello World\r\n";
int writelen = sizeof(writestring) - 1;
char readbuffer[128];
char *gets();

main(int argc, char **argv, char **envp)
{
    ULONG fd, count;
    char *devname;
    LONG errno;

    devname = argv[1];
    if (!devname || index(devname, '=')) {
	printf ("Usage: writedev <device>\n");
	return;
    }

    printf ("Opening %s ReadWrite.\n", devname);

    errno = Open ((CHAR *)devname, OpenReadWrite, &fd);

    if (argc > 2) {
	writestring = argv[2];
	writelen = strlen(writestring);
    }
    if (errno)
	perror(errno, devname);
    else {
	printf ("%s opened OK\n", devname);
	if (*writestring == '<') {
	    printf("input string: ");
	    writestring = gets(readbuffer);
	    writelen = strlen(writestring);
	}
	printf ("Writing '%s' to %s\n", writestring, devname);
	errno = Write (fd, writestring, writelen, &count);
	if (errno)
	    perror(errno, devname);
	else 
	    printf ("Write succeeded\n");
	    
	Close (fd);
    }
    return;
}
