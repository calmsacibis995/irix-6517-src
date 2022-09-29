
/*
 * Use readb to get the specified extent of blocks and write it
 * all out to stdout.  (this program exists because dd uses lseek/read)
 *
 * getex /dev/r<blah> bn [len]
 */
static char ident[] = "@(#) getex.c $Revision: 1.1 $";

#include <sys/param.h>
#include <fcntl.h>
#include <sys/syssgi.h>
#include <stdio.h>

main(int argc, char **argv)
{
        int fd;
        char *dev = argv[1];
        int len, bn = atoi(argv[2]);
	char *buf;

	if (argc != 3 && argc != 4) {
		fprintf(stderr,"usage: %s /dev/r<blah> bn [len]\n", argv[0]);
		exit(1);
	}
	if (argc == 4)
        	len = atoi(argv[3]);
	else
		len = 1;

        fd = open(dev, O_RDONLY);
        if (fd == -1 ) {
                perror("open");
                exit(1);
        }
	if ((buf = (char*)malloc(len * BBSIZE)) == NULL) {
		fprintf(stderr,"malloc(%d * BBSIZE) failed\n", len);
		exit(1);
	}
        if (syssgi(SGI_READB, fd, buf, bn, len) != len) {
                perror("readb");
                exit(1);
        }
        if (write(1, buf, len*BBSIZE) != len*BBSIZE) {
		perror("write");
		exit(1);
	}
        exit(0);
}
