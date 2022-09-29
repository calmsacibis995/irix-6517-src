/*
 * AIM Suite III v3.1.1
 * (C) Copyright AIM Technology Inc. 1986,87,88,89,90,91,92.
 * All Rights Reserved.
 */

#ifndef	lint
	static char sccs_id[] = { " @(#) disk1.c:3.2 5/30/92 20:18:34" };
#endif


/*
**  DISK EXERCISERS
**                      read    write   copy
**              -----------------------------
**                      diskr   diskw   diskc
**                      diskrr  diskrw
**
**      Disk files are written and read and copied using very
**      large buffers (10 Kilobytes) to explore the peak
**      bandwidth.  No processing on the bytes is done, just i/o calls.
**
**
*/

#include "suite3.h"

#ifndef SYS5
#include <sys/file.h>
#ifdef sun
#include <fcntl.h>
#endif
#else
#include <fcntl.h>
#endif

static char fn1[STRLEN];
static char fn2[STRLEN];
/*
static char nbuf[32*512];       ** 16K blocks
static char nbuf[20*512];       ** 10K blocks
static char nbuf[16*512];       ** 8K blocks
static char nbuf[8*512];        ** 4K blocks
static char nbuf[2*512];        ** 1K blocks
*/

static char nbuf[4*512];        /* 2K blocks */
#define BSIZE	2		/* Block size is 2K, change with nbuf size */
#define SHIFT	11		/* Change with block size */

#define BUFCOUNT        500
#define BUFCNT1         500
#define BUFCNT2         500
/*
#define BUFCOUNT	125
*/
char *mktemp();

/*
 * "Semi"-Random disk read
 * TVL 11/28/89
*/
disk_rr( argv, res )
char *argv;
Result *res;
{
	unsigned int rand();
	int	i, fd, n=BUFCNT1;
	long	sk=0l;

	if ( *argv )
		sprintf(fn2,"%s/%s", argv, TMPFILE2);
	else
		sprintf(fn2,"%s", TMPFILE2);
	mktemp(fn2);

	if ( (fd = creat(fn2,0666))<0 ) {
		fprintf(stderr,"disk_rr : cannot create %s\n", fn2);
		return(-1);
	}
/*
 * We do this to "encourage" the system to read from disk
 * instead of the buffer cache.
 * 12/12/89 TVL
 */
	while (n--)  {
		if (write(fd, nbuf, sizeof nbuf)!=sizeof nbuf) {
			perror("disk_rr()");
			fprintf(stderr,"disk_rr : cannot write %s\n", fn2);
			close(fd);
			unlink(fn2); 
			return(-1);
		}
	}
	close(fd);
	if ( (fd = open(fn2,O_RDONLY|O_NDELAY))<0) {
		fprintf(stderr,"disk_rr : cannot open %s\n", fn2);
		return(-1);
	}

	unlink(fn2); 

/********** pseudo random read *************/
	for (i=0; i<BUFCNT1; i++) {
		sk = rand() % ((BUFCNT1 * (long)sizeof(nbuf)) >> SHIFT);
		sk <<= SHIFT;
		if ( lseek(fd,sk,0) == -1 ) {
			perror("disk_rr()");
			fprintf(stderr, "disk_rr : can't lseek %s\n", fn2);
			close(fd);
			return(-1);
		}
		if ( (n=read(fd,nbuf,sizeof nbuf)) != sizeof nbuf ) {
			perror("disk_rr()");
			fprintf(stderr, "disk_rr : can't read %s\n", fn2);
			close(fd);
			return(-1);
		}
	}
	close(fd);
	res->d = n;
	return(0);
}


/*
 * "Semi"-Random disk write
*/

disk_rw( argv, res )
char *argv;
Result *res;
{
        unsigned int rand();
        int     i, fd, n=BUFCNT2;
        long    sk=0l;

        if ( *argv )
                sprintf(fn2,"%s/%s", argv, TMPFILE2);
        else
                sprintf(fn2,"%s", TMPFILE2);
        mktemp(fn2);

        if ( (fd = creat(fn2,0666))<0 ) {
                fprintf(stderr,"disk_rw : cannot create %s\n", fn2);
                return(-1);
        }

	while (n--)  {
                if (write(fd, nbuf, sizeof nbuf)!=sizeof nbuf) {
                        perror("disk_rw()");
                        fprintf(stderr,"disk_rw : cannot write %s\n", fn2);
                        close(fd);
                        unlink(fn2);
                        return(-1);
                }
        }
        close(fd);
        if ( (fd = open(fn2,O_WRONLY|O_NDELAY))<0) {
                fprintf(stderr,"disk_rw : cannot open %s\n", fn2);
                return(-1);
        }

	unlink(fn2);

/********** pseudo random write *************/

        for (i=0; i<BUFCNT2; i++) {
                sk = rand() % ((BUFCNT2 * (long)sizeof(nbuf)) >> SHIFT);
		sk <<= SHIFT;
                if ( lseek(fd,sk,0) == -1 ) {
                        perror("disk_rw()");
                        fprintf(stderr, "disk_rw : can't lseek %s\n", fn2);
                        close(fd);
                        return(-1);
                }
                if ( (n=write(fd,nbuf,sizeof nbuf)) != sizeof nbuf ) {
                        perror("disk_rw()");
                        fprintf(stderr, "disk_rw : can't read %s\n", fn2);
                        close(fd);
                        return(-1);
                }
        }
        close(fd);
        res->d = n;
        return(0);
}

disk_rd( argv, res )
char *argv;
Result *res;
{

	int	i, fd;

	if ( *argv )
		sprintf(fn1,"%s/%s", argv, TMPFILE1);
	else
		sprintf(fn1,"%s", TMPFILE1);

	if ( (fd = open(fn1,O_RDONLY|O_NDELAY))<0) {
		fprintf(stderr,"disk_rd : cannot open %s\n", fn1);
		return(-1);
	}

	/* forward sequential read only */
	if ( lseek(fd,0L,0)<0 ) {
		fprintf(stderr,"disk_rd : can't lseek %s\n", fn1);
		close(fd);
		return(-1);
	}
	for (i=0; i<BUFCOUNT; i++) {
		if ( read(fd,nbuf,sizeof nbuf) != sizeof nbuf ) {
			fprintf(stderr,"disk_rd : can't read %s\n", fn1);
			close(fd);
			return(-1);
		}
	}
	close(fd);
	res->d = i;
	return(0);
}

disk_cp( argv, res )
char *argv;
Result *res;
{
	int n=BUFCOUNT;
	int fd, fd2;

	if ( *argv ) {
		sprintf(fn1,"%s/%s",argv,TMPFILE1);
		sprintf(fn2,"%s/%s",argv,TMPFILE2);
	}
	else
	{
		sprintf(fn1,"%s",TMPFILE1);
		sprintf(fn2,"%s",TMPFILE2);
	}
	mktemp(fn2);

	if ( (fd = open(fn1,O_RDONLY|O_NDELAY))<0) {
		fprintf(stderr,"disk_cp: cannot open %s\n", fn1);
		return(-1);
	}
	if ( (fd2 = creat(fn2,0666))<0) {
		fprintf(stderr,"disk_cp: cannot create %s\n", fn2);
		close(fd);
		return(-1);
	}
/* Yes, it look stupid to unlink(fn2) now, but this really works...
** Trust me, I am a professional.... 11/20/89 TVL
*/
	unlink(fn2); 
	if ( lseek(fd,0L,0)<0 ) {
		fprintf(stderr,"disk_cp : cannot lseek %s\n", fn1);
		close(fd);
		close(fd2);
		return(-1);
	}
	while (n--)  {
		if ( read(fd,nbuf,sizeof nbuf) != sizeof nbuf ) {
			fprintf(stderr,"disk_cp: cannot read %s\n", fn1);
			close(fd);
			close(fd2);
			return(-1);
		}
		if ( write(fd2,nbuf,sizeof nbuf)!= sizeof nbuf ) {
			fprintf(stderr,"disk_cp: cannot write %s\n", fn2);
			close(fd);
			close(fd2);
			return(-1);
		}
	}
	close(fd);
	close(fd2);
	res->d = BUFCOUNT;
	return(0);
}

disk_wrt( argv,res )
char *argv;
Result *res;
{

	int n=BUFCOUNT*2;
	int fd, i;

	if ( *argv )
		sprintf(fn2,"%s/%s", argv, TMPFILE2);
	else
		sprintf(fn2,"%s",TMPFILE2);
	mktemp(fn2);

	if ( (fd = creat(fn2,0666))<0 ) {
		fprintf(stderr,"disk_wrt : cannot create %s\n", fn2);
		return(-1);
	}
/* Yes, it look stupid to unlink(fn2) now, but this really works...
** Trust me, I am a professional.... 11/20/89 TVL
*/
	unlink(fn2); 

	while (n--)  {
		if ( (i=write(fd, nbuf, sizeof nbuf)) != sizeof nbuf ) {
			fprintf(stderr,"disk_wrt : cannot write %s\n", fn2);
			close(fd);
			return(-1);
		}
	}
	close(fd);
	res->d = BUFCOUNT * i;
	return(0);
}
