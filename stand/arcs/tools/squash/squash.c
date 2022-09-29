

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#define SEG_SIZE		(64*1024)	
#define SEG_OFFSET		256
#define BUF_SIZE		4096

int verbose = 0;
char buffer[BUF_SIZE];

/* object file section globals */
unsigned long load_address;			/* code loaded here */
unsigned long start_address;		/* control passed here on execution */


/* prototypes */
void usage(void);


void main(int argc, char **argv) {
	int ifd;
	int ofd; 
	int arg = 1, len, blen, tmp;
	char *buf;
	struct stat filestat;

	/* check arguments */
	if (argc != 4 && argc != 5)
		usage();

	/* verbose mode? */
	if (!strcmp(argv[1], "-v"))
		arg++, verbose++;
	
	/* open the output file */
	if ((ofd = open(argv[arg+2], O_WRONLY | O_CREAT, 0666)) < 0) {
		printf("unable to open %s for writing\n", argv[arg+2]);
		exit(1);
	}

	/* open the first file */
	if ((ifd = open(argv[arg], O_RDONLY)) < 0) {
		printf("unable to open %s for reading\n", argv[arg]);
		exit(1);
	}

	/* init as elf file */
	ElfInitialize(ifd);

	/* copy over the file */
	blen = len = ElfRead(buffer, BUF_SIZE, ifd);
	while (len) {
		if (write(ofd, buffer, len) < 0) {
			printf("failed at writing output file\n");
			exit(1);
		}
		blen += (len = ElfRead(buffer, BUF_SIZE, ifd));
	}

	ElfClose(ifd);
	close(ifd);

	if (verbose)
		printf("bprom length is %d\n", blen);

	/* write zeros to the end of the segment */
	bzero(buffer, BUF_SIZE);
	tmp = SEG_SIZE - blen % SEG_SIZE;
	if (verbose)
		printf("zero'd %d to end of segment\n", tmp);
	blen += tmp;
	while (tmp) {
		if (write(ofd, buffer, tmp > BUF_SIZE ? BUF_SIZE : tmp) < 0) {
			printf("failed at writing output file\n");
			exit(1);
		}
		tmp -= tmp > BUF_SIZE ? BUF_SIZE : tmp;
	}

	/* write zeros in the header of next segment */
	if (write(ofd, buffer, SEG_OFFSET) < 0) {
		printf("failed at writing output file\n");
		exit(1);
	}
	blen += SEG_OFFSET;
	
	/* open the second file */
	if ((ifd = open(argv[arg+1], O_RDONLY)) < 0) {
		printf("unable to open %s for reading\n", argv[arg+1]);
		exit(1);
	}

	/* init as elf file */
	ElfInitialize(ifd);

	/* copy over the file */
	blen += (len = ElfRead(buffer, BUF_SIZE, ifd));
	while (len) {
		if (write(ofd, buffer, len) < 0) {
			printf("failed at writing output file\n");
			exit(1);
		}
		blen += (len = ElfRead(buffer, BUF_SIZE, ifd));
	}

	if (verbose)
		printf("total size is %d\n", blen);

	ElfClose(ifd);
	close(ifd);
	
	close(ofd);
}


void usage(void) {
	printf("usage: squash [-v] bprom fprom outprom\n");
	printf("  -v         verbose mode\n");
	printf("  bprom      boot prom file\n");
	printf("  fprom      prom file\n");
	printf("  outprom    output concatinated prom\n");

	exit(0);
}


