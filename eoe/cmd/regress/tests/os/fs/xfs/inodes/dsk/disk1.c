/*
**      Disk files are written and read and copied using very
**      large buffers (10 Kilobytes) to explore the peak
**      bandwidth.  No processing on the bytes is done, just i/o calls.
*/

#include <fcntl.h>
#include "dsk.h"

#define BSIZE	4		/* Block size is 4K, change with nbuf size */
#define SHIFT	12		/* Change with block size */

#define BUFCOUNT	100
#define BUFCNT1         100
#define BUFCNT2         100
/*
#define BUFCOUNT        500
*/
static char fn1[STRLEN];
static char fn2[STRLEN];
/*
static char nbuf[32*512];       ** 16K blocks
static char nbuf[20*512];       ** 10K blocks
static char nbuf[16*512];       ** 8K blocks
static char nbuf[4*512];        ** 2K blocks
static char nbuf[2*512];        ** 1K blocks
*/
static char nbuf[8*512];        /* 4K blocks */


/*
 * "Semi"-Random disk read
 */
disk_rr( char *argv )
{
	int	i, fd, n=BUFCNT1;
	long	sk=0l;

	if (debug > 3)
		fprintf(stderr, "disk_rr: argv=%s\n", argv);

	if ( argv && *argv )
		sprintf(fn2,"%s/%s", argv, TMPFILE2);
	else
		sprintf(fn2,"%s", TMPFILE2);
	mktemp(fn2);

	if ( (fd = creat(fn2,0666)) == -1 ) {
		perror("disk_rr:creat");
		fprintf(stderr,"disk_rr : cannot create %s\n", fn2);
		return(-1);
	}
/*
 * We do this to "encourage" the system to read from disk
 * instead of the buffer cache.
 */
	while (n--)  {
		if (write(fd, nbuf, sizeof nbuf)!=sizeof nbuf) {
			perror("disk_rr:write");
			perror("disk_rr()");
			fprintf(stderr,"disk_rr : cannot write %s\n", fn2);
			close(fd);
			unlink(fn2); 
			return(-1);
		}
	}
	close(fd);
	if ( (fd = open(fn2,O_RDONLY|O_NDELAY)) == -1) {
		perror("disk_rr:open");
		fprintf(stderr,"disk_rr : cannot open %s\n", fn2);
		return(-1);
	}

	unlink(fn2); 

/********** pseudo random read *************/
	for (i=0; i<BUFCNT1; i++) {
		sk = rand() % ((BUFCNT1 * (long)sizeof(nbuf)) >> SHIFT);
		sk <<= SHIFT;
		if ( lseek(fd,sk,0) == -1 ) {
			perror("disk_rr:lseek");
			fprintf(stderr, "disk_rr : can't lseek %s\n", fn2);
			close(fd);
			return(-1);
		}
		if ( (n=read(fd,nbuf,sizeof nbuf)) != sizeof nbuf ) {
			perror("disk_rr:read");
			fprintf(stderr, "disk_rr : can't read %s\n", fn2);
			close(fd);
			return(-1);
		}
	}
	close(fd);
	return(0);
}


/*
 * "Semi"-Random disk write
 */

disk_rw( char *argv )
{
        int     i, fd, n=BUFCNT2;
        long    sk=0l;

	if (debug > 3)
		fprintf(stderr, "disk_rw: argv=%s\n", argv);

        if ( argv && *argv )
                sprintf(fn2,"%s/%s", argv, TMPFILE2);
        else
                sprintf(fn2,"%s", TMPFILE2);
        mktemp(fn2);

        if ( (fd = creat(fn2,0666)) == -1 ) {
		perror("disk_rw:creat");
                fprintf(stderr,"disk_rw : cannot create %s\n", fn2);
                return(-1);
        }

	while (n--)  {
                if (write(fd, nbuf, sizeof nbuf)!=sizeof nbuf) {
                        perror("disk_rw:write");
                        fprintf(stderr,"disk_rw : cannot write %s\n", fn2);
                        close(fd);
                        unlink(fn2);
                        return(-1);
                }
        }
        close(fd);
        if ( (fd = open(fn2,O_WRONLY|O_NDELAY)) == -1) {
		perror("disk_rw:open");
                fprintf(stderr,"disk_rw : cannot open %s\n", fn2);
                return(-1);
        }

	unlink(fn2);

/********** pseudo random write *************/
        for (i=0; i<BUFCNT2; i++) {
                sk = rand() % ((BUFCNT2 * (long)sizeof(nbuf)) >> SHIFT);
		sk <<= SHIFT;
                if ( lseek(fd,sk,0) == -1 ) {
                        perror("disk_rw:lseek");
                        fprintf(stderr, "disk_rw : can't lseek %s\n", fn2);
                        close(fd);
                        return(-1);
                }
                if ( (n=write(fd,nbuf,sizeof nbuf)) != sizeof nbuf ) {
                        perror("disk_rw:write");
                        fprintf(stderr, "disk_rw : can't read %s\n", fn2);
                        close(fd);
                        return(-1);
                }
        }
        close(fd);
        return(0);
}

disk_rd( char *argv )
{

	int	i, fd;

	if (debug > 3)
		fprintf(stderr, "disk_rd: argv=%s\n", argv);

	if ( argv && *argv )
		sprintf(fn1, "%s/%s", argv, TMPFILE1);
	else
		sprintf(fn1, "%s", TMPFILE1);

	if ( (fd = open(fn1, O_RDONLY|O_NDELAY)) == -1) {
		perror("disk_rd:open");
		fprintf(stderr,"disk_rd : cannot open %s\n", fn1);
		return(-1);
	}

	/* forward sequential read only */
	if ( lseek(fd,0L,0) == -1 ) {
		perror("disk_rd:lseek");
		fprintf(stderr,"disk_rd : can't lseek %s\n", fn1);
		close(fd);
		return(-1);
	}
	for (i=0; i<BUFCOUNT; i++) {
		if ( read(fd, nbuf, sizeof nbuf) != sizeof nbuf ) {
			perror("disk_rd:read");
			fprintf(stderr,"disk_rd : can't read %s\n", fn1);
			close(fd);
			return(-1);
		}
	}
	close(fd);
	return(0);
}

disk_cp( char *argv )
{
	int n=BUFCOUNT;
	int fd, fd2;

	if (debug > 3)
		fprintf(stderr, "disk_cp: argv=%s\n", argv);

	if ( argv && *argv ) {
		sprintf(fn1,"%s/%s",argv,TMPFILE1);
		sprintf(fn2,"%s/%s",argv,TMPFILE2);
	}
	else
	{
		sprintf(fn1,"%s",TMPFILE1);
		sprintf(fn2,"%s",TMPFILE2);
	}
	mktemp(fn2);

	if ( (fd = open(fn1,O_RDONLY|O_NDELAY)) == -1) {
		perror("disk_cp:open");
		fprintf(stderr,"disk_cp: cannot open %s\n", fn1);
		return(-1);
	}
	if ( (fd2 = creat(fn2,0666)) == -1) {
		perror("disk_cp:creat");
		fprintf(stderr,"disk_cp: cannot create %s\n", fn2);
		close(fd);
		return(-1);
	}

	unlink(fn2); 
	if ( lseek(fd,0L,0) == -1 ) {
		perror("disk_cp:lseek");
		fprintf(stderr,"disk_cp : cannot lseek %s\n", fn1);
		close(fd);
		close(fd2);
		return(-1);
	}
	while (n--)  {
		if ( read(fd,nbuf,sizeof nbuf) != sizeof nbuf ) {
			perror("disk_cp:read");
			fprintf(stderr,"disk_cp: cannot read %s\n", fn1);
			close(fd);
			close(fd2);
			return(-1);
		}
		if ( write(fd2,nbuf,sizeof nbuf)!= sizeof nbuf ) {
			perror("disk_cp:write");
			fprintf(stderr,"disk_cp: cannot write %s\n", fn2);
			close(fd);
			close(fd2);
			return(-1);
		}
	}
	close(fd);
	close(fd2);
	return(0);
}

disk_wr( char *argv )
{

	int n=BUFCOUNT*2;
	int fd, i;

	if (debug > 3)
		fprintf(stderr, "disk_wr: argv=%s\n", argv);

	if ( argv && *argv )
		sprintf(fn2,"%s/%s", argv, TMPFILE2);
	else
		sprintf(fn2,"%s",TMPFILE2);
	mktemp(fn2);

	if ( (fd = creat(fn2,0666)) == -1 ) {
		perror("disk_wr:creat");
		fprintf(stderr,"disk_wrt : cannot create %s\n", fn2);
		return(-1);
	}

	unlink(fn2); 
	while (n--)  {
		if ( (i=write(fd, nbuf, sizeof nbuf)) != sizeof nbuf ) {
			perror("disk_wr:write");
			fprintf(stderr,"disk_wrt : cannot write %s\n", fn2);
			close(fd);
			return(-1);
		}
	}
	close(fd);
	return(0);
}
