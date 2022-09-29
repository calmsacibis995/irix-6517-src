/*
 * copy -- copy files
 */

#ident "$Revision: 1.35 $"

#include <sys/types.h>
#include <sys/dkio.h>
#include <arcs/types.h>
#include <arcs/signal.h>
#include <arcs/io.h>
#include <setjmp.h>
#include <libsc.h>

static void catch_intr(void);
static jmp_buf catch;
static ULONG src_fd, dst_fd;
static char *buffer;

#define NOTVALIDFD	0xffffffff

/* library _min() uses signed ints.
 */
#define _min(a,b)	((a < b) ? a : b)

/*ARGSUSED*/
int
copy(int argc, char **argv, char **bunk1, struct cmd_table *bunk2)
{
	ULONG bcnt, dbcnt;
	int discnt;
	unsigned blocksize = BLKSIZE;
	unsigned long long totcnt;
	unsigned long long bytes = ((unsigned long long)(-1)) >> 1;	/* max # without sign set */
	/* these are volatile to guarantee on stack for longjmp */
	volatile SIGNALHANDLER prev_handler;
	unsigned bksize = 0;
	long err;

	src_fd = dst_fd = NOTVALIDFD;
	argc--; argv++;

	for (; argc > 0 && **argv == '-'; argc--, argv++) {
		switch ((*argv)[1]) {
		case 'B':	/* set device blocksize; leave undocumented, because
			it is mainly for testing, and is only for dksc at this time. */
			if (--argc <= 0)
				return(1);
			atobu(*++argv, &bksize);
			break;
		case 'b':	/* set blocksize */
			if (--argc <= 0)
				return(1);
			switch (*atobu(*++argv, &blocksize)) {
			case NULL:
				break;
			case 'b': case 'B':
				blocksize *= BLKSIZE;
				break;
			case 'k': case 'K':
				blocksize *= 1024;
				break;
			default:
				return(1);
			}
			break;

		case 'c':
			if (--argc <= 0)
				return(1);
			if (*atobu_L(*++argv, &bytes)) {
				printf("bad count: %s\n", *argv);
				return(0);
			}
			break;

		default:
			return(1);
		}
	}

	if (argc != 2)
		return(1);

	/* 
	 * Use a setjmp/longjmp to get back here to clean up and
	 * and cancel copy command if cntrl-c is entered. 
	 */

	prev_handler = Signal (SIGINT, catch_intr);
	if (setjmp(catch)) {
		Signal (SIGINT, prev_handler);
		bzero (catch, sizeof(jmp_buf));
		return (0);
	} 

	err = Open((CHAR *) *argv, OpenReadOnly, &src_fd);
	if (err != 0) {
		printf("cannot open: ");
		perror(err,*argv);
		goto ret0;
	}

	err = Open((CHAR *) *++argv, OpenWriteOnly, &dst_fd);
	if (err != 0) {
		printf("cannot open: ");
		perror(err,*argv);
		goto ret0;
	}

	if((buffer=align_malloc(blocksize,BLKSIZE)) == NULL) {
		printf("Can't allocate a buffer for blocksize %u\n", blocksize);
		goto ret0;
	}

	if(bksize && ioctl(src_fd, DIOCSETBLKSZ, (long)&bksize))
		printf("Attempt to set blocksize to %u failed\n", bksize);

	totcnt = 0;
	discnt = 0;
	while (bytes) {
		dbcnt = (ULONG)_min(bytes, (unsigned long long)blocksize);
		if (err = Read(src_fd,(CHAR *)buffer,dbcnt,&bcnt)) {
			putchar('\n');
			perror(err,"cp: read error");
			puts("    ");
			prcuroff(src_fd);
			break;
		}
		if (!bcnt) break;		/* EOF */
		if ((err = Write(dst_fd,(CHAR *)buffer,bcnt,&dbcnt)) ||
		    (bcnt!=dbcnt)) {
			putchar('\n');
			if (err) {
				perror(err,"cp: write error");
				puts("    ");
			}
			else
				printf("cp: write error @ ");
			prcuroff(dst_fd);
			break;
		}
		printf (".");
		if (++discnt == 64) {
			printf("   \n");	/* cover enet spinner */
			discnt = 0;
		}
		bytes -= (unsigned long long)bcnt;
		totcnt += (unsigned long long)bcnt;
	}

	/* reset fd's and buf as dealt with, out of paranoia for interrupts */
	align_free(buffer);
	buffer = NULL;

	printf("\n%lld (0x%llx) bytes copied\n", totcnt, totcnt);

ret0:
	if (src_fd != NOTVALIDFD) {
		Close(src_fd);
		src_fd = NOTVALIDFD;
	}
	if (dst_fd != NOTVALIDFD) {
		Close(dst_fd);
		dst_fd = NOTVALIDFD;
	}

	Signal (SIGINT, prev_handler);

	return(0);
}

static void
catch_intr(void)
{
	if(src_fd != NOTVALIDFD)
		Close(src_fd);
	if(dst_fd != NOTVALIDFD)
		Close(dst_fd);
	if(buffer)
		align_free(buffer);
	printf("\n");

	longjmp(catch, 1);
	/*NOTREACHED*/
}
