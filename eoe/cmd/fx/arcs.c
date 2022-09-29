#ifdef ARCS_SA	/* whole file */

#include <arcs/io.h>
#include <arcs/signal.h>
#include "fx.h"

/*
 * Routines not defined in arcs, or in our layer above arcs.
 * In the interests of keeping the arcs tree relatively
 * unpolluted, we define some routines here.
 * Also variables and routines used only in the arcs version.
 * Also see standalone.c and irix.c
 * Dave Olson, 7/92
*/

/* override tune.h -> sctune.o.  Some new drives, such as the
 * DEC 2GB drive have a cylsize > 256K, and wr-cmp needs 2 buffers.
 * I want to have to avoid changing it soon, so use 2.5 MB.  Olson, 10/92.
 * (maxtor 2GB *barely* makes it with 2MB).
*/
long malbuf[0x280000/sizeof(long)];	/* need 4/8 alignment for 32/64 bit */
int _max_malloc = sizeof(malbuf);	/* most we can ever allocate at once */

extern int fds[];

int errno;


/*
 * oserror - return system errno
 */
int
oserror(void)
{
	return errno;
}


/*
 * setoserror - set system errno
 */
int
setoserror(int err)
{
	errno = err;

	return err;
}


/* nop stub */
/* ARGSUSED0 */
void
fflush(void *a)
{
}

/* ARGSUSED0 */
void
exit(int val)
{
    EnterInteractiveMode();	/* silly name! */
}

int
open(char *file, int how)
{
	unsigned long fd;
	if(how == 2) how = OpenReadWrite;
	else if(how == 0) how = OpenReadOnly;
	else if(how == 1) how = OpenWriteOnly;
	/* else leave it alone, who knows? ... */
	errno = (int)Open(file,how,&fd);
	if(errno)
		fd = -1;
	return (int)fd;
}


/*
 * make the current level interruptible or not according to flag.
 *	 1 - interruptible
 *	 0 - non-interruptible
 *	-1 - killable
 * return previous flag value.  ioctl returns
 *	 1 - non-interruptible
 *	 0 - killable
 *	xx - interruptible
 * Non-ARCS version in standalone.c
 */
int
setintr(int flag)
{
    SIGNALHANDLER oldintr;

    if( flag == 0 )
	oldintr = Signal(SIGINT, SIGIgnore);
    else
    if( flag > 0 )
	oldintr = Signal(SIGINT, (SIGNALHANDLER)mpop);
    else
	oldintr = Signal(SIGINT, SIGDefault);

    return oldintr==SIGDefault ?-1 : oldintr==SIGIgnore ?0 :1;
}

/*
 * make the current level interruptible, and return the previous
 * interruptibility.  if the current level was killable, no change.
 * Non-ARCS version in standalone.c
 */
int
mkintr(void)
{
    SIGNALHANDLER oldintr;

    oldintr = Signal(SIGINT, SIGIgnore);
    if( oldintr == SIGDefault )
	Signal(SIGINT, SIGDefault);
    else
	Signal(SIGINT, (SIGNALHANDLER)mpop);

    return oldintr==SIGDefault ?-1 : oldintr==SIGIgnore ?0 :1;
}


/* next 4 (gread, gwrite, gioctl, and gclose are in io.c for non-arcs */

/*
 * read from disk at block bn, with retries
 *
 * For non SCSI, don't retry on multiple block reads or report errors
 * or add them to the error log if we are exercising the
 * disk AND this is a multiple block read.  The exercisor
 * will retry each block separately when it gets errors
 * on multiple block reads.  Otherwise we confuse the 
 * user by reporting errors as the first block of the read, with
 * some drives and firmware revs.
 * ARCS already has some provision for large block #'s, so i/o always
 * uses same calls, unlike 'normal' version.
 */
int
gread(daddr_t bn, void *buf, uint count)
{
	int i;
	int secsz = DP(&vh)->dp_secbytes;
	ULONG bcount, syscnt;
	LARGE offset;

	/* secsz is manually set to 512 if zero. This is needed to check
	   the integrity of partition zero which is the entire disk if
	   ARCS.
	*/
	if (secsz == 0) 
		secsz = 512;
	syscnt = count * secsz;

	for(i = 0; i <= ioretries; i++) {
	    ULONG blk = bn;

	    /* have to set offset inside loop, since Seek returns
	     * the new offset in the passed struct; we *hope* this
	     * doesn't loop too many times; after all, it is only
	     * one pass for a 4GB drive...  Could be handled better
	     * with long long support, but not in bvd.  Assumes
	     * sector size is even, which is not unreasonable. */
	    offset.hi = 0;
	    while((0x80000000u/(secsz/2)) <= blk) {
		    offset.hi++;
		    blk -= 0x80000000u/(secsz/2);
	    }

	    offset.lo = (ULONG)blk*secsz;
	
	    if(i)
		printf("retry #%d\n", i+1);
	    if(errno=(int)Seek(fds[driveno], &offset, SeekAbsolute))  {
		    /* should have some way to suppress trying to add
		     * a bad block for this one, but with the check above
		     * for out of range, this should be quite rare... */
		    scerrwarn("Seek to block #%d fails\n", blk);
		    return -1;
	    }
	    errno = (int)Read((LONG)fds[driveno], (CHAR *)buf, (LONG)syscnt, &bcount);

	    if(!errno && syscnt == bcount)
		    return 0;

	    if(errno == -1) {
		if(count == 1)
			scerrwarn("read error at block %u", bn);
		else {
		   if(exercising)
			break;  /* scanchunk retries sector by sector, and
				 * reports error */
		   scerrwarn("error in read starting at block %u", bn);
		}
		adderr(0, bn, "sr");
	    }
	    else {
		const char *s =
			"Tried to read %d blocks, only read %d at %u\n";
		printf(s, count, bcount/secsz, blk);
		logmsg((char *)s, count, bcount/secsz, blk);
		flushoutp();
	    }
	}

	adderr(0, bn, "hr");
	return -1;
}


/*
 * write to disk at block bn, with retries; see comments at gread()
 */
int
gwrite(daddr_t bn, void *buf, uint count)
{
	int i;
	int secsz = DP(&vh)->dp_secbytes;
	ULONG bcount, syscnt;
	LARGE offset;

	/* secsz is manually set to 512 if zero. This is needed to check
	   the integrity of partition zero which is the entire disk if
	   ARCS.
	*/
	if (secsz == 0) 
		secsz = 512;
	syscnt = count * secsz;

	for(i = 0; i <= ioretries; i++) {
	    ULONG blk = bn;

	    /* have to set offset inside loop, since Seek returns
	     * the new offset in the passed struct; we *hope* this
	     * doesn't loop too many times; after all, it is only
	     * one pass for a 4GB drive...  Could be handled better
	     * with long long support, but not in bvd.  Assumes
	     * sector size is even, which is not unreasonable. */
	    offset.hi = 0;
	    while((0x80000000u/(secsz/2)) <= blk) {
		    offset.hi++;
		    blk -= 0x80000000u/(secsz/2);
	    }

	    offset.lo = (ULONG)blk*secsz;
	
	    if(i)
		printf("retry #%d\n", i+1);
	    if(errno=(int)Seek(fds[driveno], &offset, SeekAbsolute))  {
		    /* should have some way to suppress trying to add
		     * a bad block for this one, but with the check above
		     * for out of range, this should be quite rare... */
		    scerrwarn("Seek to block #%d fails\n", blk);
		    return -1;
	    }
	    errno = (int)Write((LONG)fds[driveno], (CHAR *)buf, (LONG)syscnt, &bcount);

	    if(!errno && syscnt == bcount)
		    return 0;

	    if(errno == -1) {
		if(errno == EROFS) {	/* no point in retrying! */
			/* print message once here, instead of each caller */
			printf("drive appears to be write protected\n");
			break;
		}
		if(count == 1)
			scerrwarn("write error at block %u", bn);
		else {
		   if(exercising)
			break;  /* scanchunk retries sector by sector, and
				 * reports error */
		   scerrwarn("error in write starting at block %u", bn);
		}
		adderr(0, bn, "sr");
	    }
	    else {
		const char *s =
			"Tried to write %d blocks, only write %d at %u\n";
		printf(s, count, bcount/secsz, blk);
		logmsg((char *)s, count, bcount/secsz, blk);
		flushoutp();
	    }
	}

	adderr(0, bn, "hr");
	return -1;
}


/*
 * ioctl on the disk, with error reporting
 */
gioctl(int cmd, void *ptr)
{
	return errno=(int)ioctl((long)fds[driveno], (long)cmd, (long)ptr);
}


/*
 * close all open disks
 */
void
gclose(void)
{
	register int i;

	for( i = 0; i < MAX_DRIVE; i++ )
		if( fds[i] > 0 )
			Close(fds[i]);
}

/* get the screenwidth.  Return 80 (texport width) for standalone */
uint
getscreenwidth(void)
{
	return 80;
}

/* ARGSUSED0 */
chkmounts(char *file)
{
	return 0;
}
#endif	/* ARCS_SA (whole file) */
