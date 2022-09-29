#include "fx.h"
#include <sys/stat.h>
#include <sys/major.h>
#include <sys/sysmacros.h>

/*
 * i/o routines.
 * these implement software controlled retries,
 * and device addressing by driveno instead of fd.
 * THIS ASSUMES THAT FX DEALS WITH AT MOST ONE CTLR AT A TIME!
 */

#ifdef _STANDALONE
#include <saioctl.h>
#endif

int ioretries = 3;				/* error retries */

int fds[MAX_DRIVE];				/* fd's, per drive */

/* controller & drive info: set by fx_find_ctlr() which looks at
 * system hardware. */

#ifdef SCSI_NAME
char scsi_name[] = SCSI_NAME;
#endif
#ifdef SMFD_NAME
char smfd_name[] = SMFD_NAME;
#endif

/* no prototypes in headers */
#ifndef ARCS_SA
extern int open(char *, int modes);
extern int readb(int, void *, daddr_t, int);
extern int writeb(int, void *, daddr_t, int);
#endif

extern int nomenus;	/* see fx.c, -c option */
extern char *script; /* see fx.c, -s option */

/* fx_find_ctlr(). */
/* In standalone mode, this makes probe calls to the various drivers */
/* to try to find out what type of disk controller is actually here. */
/* In runtime mode, we will default to the kind of controller being */
/* used for root. We do a 'devnm' on root, and look at the resulting */
/* string. */

void
fx_find_ctlr(void)
{
#ifdef _STANDALONE
	/* coded this way so it doesn't matter which controllers are actually
		included. */
	int found = 0;
	scsilun = 0;
	if(!found) {	/* assume it must be SCSI; NOTE: this should deal
		with the second scsi chip some day... */
		driver = scsi_name;
		drivernum = SCSI_NUMBER;
		ctlrno = 0;
		driveno = 1;
	}

#else	/* runtime. If search for root fails, default to dkip, drive 1 */
	/* (Shouldn't happen of course, still, this is only a default prompt */

	if (rt_cfind() != 0){
		driver = scsi_name;
		drivernum = SCSI_NUMBER;
		ctlrno = 0;
		driveno = 1;
		printf("Unable to determine drive type, defaulting to SCSI, ctrlr %d, drive %d\n",
			ctlrno, driveno);
	}
#endif
}


/*
 * quick controller check
 * NOTE: we can't start diagnostics at runtime, so in fact all the
 * ioctl does now is is look at the 'board ok' bit. The diag_info
 * is not significant.... Dave H 3/28/88.
 * NOTE: none of the drivers use the bcnt fields at this time;
 * if any of them ever take it to be the actual block size, this
 * may need to be modified, since the actual block size may not
 * be known at this point.
 * do not use scerrwarn for failures, as we do not want to exit on this
 * error when -c INITIALIZE or similar calls are used.
 */
void
ctlrcheck(void)
{
	printf("...drive selftest...");
	flushoutp();
	if(gioctl(DIOCTEST, 0) < 0 )
		err_fmt("FAILED, may have further problems");
	else
		printf("OK\n");
}


/*
 * get volume header from driver
 * do not use scerrwarn for failures, as we do not want to exit on this
 * error when -c INITIALIZE or similar calls are used.
 */
void
get_vh(struct volume_header *vp)
{
	if( gioctl(DIOCGETVH, vp) < 0 )
		err_fmt("no volume header available from driver");
}

/*
 * set volume header in driver
 */
int
set_vh(struct volume_header *vp)
{
	int	error;


	vp->vh_magic = VHMAGIC;
	vp->vh_csum = 0;
	vp->vh_csum = -vhchksum((int *)vp, sizeof *vp);
		
	if ( gioctl(DIOCSETVH, vp) < 0 ) {

		error = oserror();		/* Keep a copy for later */

		if (error == EBUSY) {
			/*
			 * An EBUSY can only happen on non-miniroot kernels.
			 * We're either interactive, command, or script driven.
			 * If we're not miniroot, and are command or script
			 * driven, we should terminate.
			 */

			scerrwarn(
		"\n\tBy re-partitioning you are attempting to delete a\n"
		"\tpartition in use by a mounted file system!\n\n");

			if (nomenus || script)
				exit(1);

			if ( yesno(-1,
	"Continue only if you are very certain about this action.\n"
	"A kernel panic could occur if you proceed, and then attempt\n"
	"to unmount the affected file system.\n"
	"Continue by forcing the driver update" )) {

				(void)gioctl(DIOCSETVH | DIOCFORCE, vp);

				return 0;
			}

			setoserror(error);		/* Put it back	*/

			return error;

		} else {

			scerrwarn("Driver update failed!\n");

			return error;
		}
	}

	return 0;
}


/* get drive type */
void
get_drivetype(int *_n)
{
	int n;
	if( gioctl(DIOCDRIVETYPE, &n) < 0 )
	{
		scerrwarn("can't get drivetype");
		*_n = -1;
		return;
	}
	*_n = n;
}


#ifndef _STANDALONE
/*
 * runtime version diskfinder. We attempt to default to the controller
 * running the root disk, but for security the unit number will be set
 * to the back drive.
 */
int
rt_cfind(void)
{
	FILE *popf;
	char pbuf[200];
	int length;
	struct stat sbuf;
	int xlv = 0;
	int ret;

        if ((ret = stat("/", &sbuf)) != -1) {
                if (major(sbuf.st_dev) == XLV_MAJOR) {
                        xlv = 1;
                } 
		else {
                        if (dev_to_drivername(sbuf.st_dev, pbuf, &length)) {
                                if (strncmp(pbuf, "xlv", 3) == 0)
                                        xlv = 1;
                        }
		}
        }
        else {
                return -1;
        }


	if (!xlv) {

               	dev_to_alias(sbuf.st_dev, pbuf, &length);

		if (strncmp(pbuf, "/dev/dsk/", 9)) 
			return -1; 			/* unexpected format? */
		else 
			strcpy(pbuf, (pbuf + 9));	/* get the distinguishing part of */
	
	}

	/* the device name: ips or dks */

#ifdef SCSI_NAME
	if (!strncmp(pbuf, "dks", 3) || xlv){
		driver = scsi_name;
		drivernum = SCSI_NUMBER;
		ctlrno = 0;
		driveno = 1;
		return 0;
	}
#endif
#ifdef SMFD_NAME
	if (!strncmp(pbuf, "fd", 3)){
		driver = smfd_name;
		drivernum = SMFD_NUMBER;
		ctlrno = 0;
		driveno = 1;
		return 0;
	}
#endif
	return -1;
}
#endif

/*
 * open the device file associated with the given
 * driveno and parition.  driver and ctlrno are globals.
 */
int
gopen(uint d, uint p)
{
	STRBUF s;
	int fd;

	*s.c = 0;
#ifdef ARCS_SA
	if (!strcmp(driver, SCSI_NAME))
		sprintf(s.c, "scsi(%d)disk(%d)part(%d)", ctlrno, d, p);
#elif _STANDALONE	/* 'old' standalone */
	sprintf(s.c, "%s(%d,%d,%d)", driver, ctlrno, d, p);
#else
#ifdef SCSI_NAME
	if (device_name_specified)
	    strcpy(s.c, device_name);
	else if(!strcmp(driver, SCSI_NAME)) {
	    if (scsilun > 0)
	    {
		if (p == PTNUM_VOLUME)
		    sprintf(s.c, "/dev/rdsk/dks%ud%ul%dvol",
			    ctlrno, d, scsilun);
		else
		    sprintf(s.c, "/dev/rdsk/dks%ud%ul%ds%u",
			    ctlrno, d, scsilun, p);
	    }
	    else
	    {
		if (p == PTNUM_VOLUME)
		    sprintf(s.c, "/dev/rdsk/dks%ud%uvol", ctlrno, d);
		else
		    sprintf(s.c, "/dev/rdsk/dks%ud%us%u", ctlrno, d, p);
	    }
	}
#endif
#ifdef SMFD_NAME
	if(!strcmp(driver, SMFD_NAME)) {
		char *type = smfd_partname(p);
		if(!type)
			return -1;
		sprintf(s.c, "/dev/rdsk/fds%ud%u.%s", ctlrno, d, type);
	}
#endif /* SMFD_NAME */
#endif /* _STANDALONE */
	if(!s.c) {
		printf("unknown driver type \"%s\"\n", driver);
		return -1;
	}
	printf("...opening %s\n", dksub());
	fds[d] = fd = (int)open(s.c, 2);
	if(fd == -1 && errno == EACCES && (fds[d] = fd = (int)open(s.c, 0)) != -1) {
		printf("Notice: read only device\n");
	}
	if((int)fd != -1) {
	fds[d] = (int)fd;

#ifndef _STANDALONE
	if(chkmounts(s.c))  {
	    static char msg[] = "this disk appears to have mounted filesystems.\n"
		"\t Don't do anything destructive, unless you are sure that nothing\n"
		"\t you are changing will affect the parts of the disk in use.\n\n";
	    if(nomenus && !script) { /* make it fatal */
			errno = 0;
			scerrwarn(msg);
		}
		else
			errwarn(msg);
	    }
		if(nomenus)
			sleep(10);	/* give them a bit of a chance to abort */
#endif
	}
	else { /* different than what we told them we were opening, so
			* give them a hint */
	    static char msg[] = "Failed to open %s";
	    if(nomenus) /* make it fatal */
			scerrwarn(msg, s.c);
		else
			errwarn(msg, s.c);
	}
	return fd;
}


#ifndef ARCS_SA	/* arcs versions are different; in arcs.c */

/*
 * read from disk at block bn, with retries
 *
 * For SCSI, don't retry on multiple block reads or report errors
 * or add them to the error log if we are exercising the
 * disk AND this is a multiple block read.  The exercisor
 * will retry each block separately when it gets errors
 * on multiple block reads.  Otherwise we confuse the 
 * user by reporting errors as the first block of the read, with
 * some drives and firmware revs.
 */
int
gread(daddr_t bn, void *buf, uint count)
{
	int i, cnt, syscnt, doreadb;
	daddr_t blk;
	int secsz = DP(&vh)->dp_secbytes;

	/* have to avoid overflow on count, so handle sector
	 * sizes that are not a multiple of BBSIZE by doing
	 * old style reads, but only if within range.
	 * This is primarily to handle things like floppies, and
	 * perhaps some raid disks, which might have (individual) disks
	 * with a logical block size < BBSIZE.
	 */
	if(secsz%BBSIZE) {
	    if((0x80000000u/secsz) <= bn) {
		errno = 0; /* prevent bogus error message */
		scerrwarn("sector size not a multiple of %d and block #%d too"
		    " large for read system call\n",
		    BBSIZE, bn);
		return -1;
	    }
	    syscnt = count * secsz;
	    blk = bn * secsz;
	    doreadb = 0;
	}
	else  {
	    blk = bn * (secsz/BBSIZE);
	    syscnt = count * (secsz/BBSIZE);
	    doreadb = 1;
	}

	for(i = 0; i <= ioretries; i++) {
	    if(i)
		printf("retry #%d\n", i+1);
	    if(doreadb)
		cnt = readb(fds[driveno], buf, blk, syscnt);
	    else {
		if(lseek(fds[driveno], (long)blk, 0) == -1) {
			/* should have some way to suppress trying to add
			 * a bad block for this one, but with the check above
			 * for out of range, this should be quite rare... */
			scerrwarn("Seek to block #%d fails\n", blk);
			return -1;
		}
		cnt = read(fds[driveno], buf, syscnt);
	    }
	    if(cnt == syscnt)
		return 0;

	    if(cnt == -1) {
		if(count == 1)
		   scerrwarn("on read at block %u for 1 block", bn);
		else
		   scerrwarn("on read starting at block %u for %d blocks", bn, count);
		adderr(0, bn, "sr");
	    }
	    else {
		const char *s =
			"Tried to read %d blocks, only read %d at %u\n";
		if(!doreadb)
			cnt /= secsz;
		printf(s, count, cnt, blk);
		logmsg((char *)s, count, cnt, blk);
		flushoutp();
	    }
	}

	adderr(0, bn, "hr");
	return -1;
}

/*
 * write to disk at block bn, with retries.  See comments at gread() above
 */
int
gwrite(daddr_t bn, void *buf, uint count)
{
	int i, cnt, syscnt, dowriteb;
	daddr_t blk;
	int secsz = DP(&vh)->dp_secbytes;

	/* have to avoid overflow on count, so handle sector
	 * sizes that are not a multiple of BBSIZE by doing
	 * old style writes, but only if within range.
	 * This is primarily to handle things like floppies, and
	 * perhaps some raid disks, which might have (individual) disks
	 * with a logical block size < BBSIZE.
	 */
	if(secsz%BBSIZE) {
	    if((0x80000000u/secsz) <= bn) {
		errno = 0; /* prevent bogus error message */
		scerrwarn("sector size not a multiple of %d and block #%d too"
		    " large for write system call\n",
		    BBSIZE, bn);
		return -1;
	    }
	    syscnt = count * secsz;
	    blk = bn * secsz;
	    dowriteb = 0;
	}
	else  {
	    blk = bn * (secsz/BBSIZE);
	    syscnt = count * (secsz/BBSIZE);
	    dowriteb = 1;
	}

	for(i = 0; i <= ioretries; i++) {
	    if(i)
		printf("retry #%d\n", i+1);
	    if(dowriteb)
		cnt = writeb(fds[driveno], buf, blk, syscnt);
	    else {
		if(lseek(fds[driveno], (long)blk, 0) == -1) {
			/* should have some way to suppress trying to add
			 * a bad block for this one, but with the check above
			 * for out of range, this should be quite rare... */
			scerrwarn("Seek to block #%d fails\n", blk);
			return -1;
		}
		cnt = write(fds[driveno], buf, syscnt);
	    }
	    if(cnt == syscnt)
		return 0;

	    if(cnt == -1) {
		if(errno == EROFS) {	/* no point in retrying! */
			/* print message once here, instead of each caller */
			printf("drive appears to be write protected\n");
			break;
		}
		if(count == 1)
		   scerrwarn("on write at block %u for 1 block", bn);
		else
		   scerrwarn("on write starting at block %u for %d blocks", bn, count);
	        adderr(0, bn, "sr");
	    }
	    else {
		const char *s =
			"Tried to write %d blocks, only wrote %d at %u\n";
		if(!dowriteb)
			cnt /= secsz;
		printf(s, count, cnt, blk);
		logmsg((char *)s, count, cnt, blk);
		flushoutp();
	    }
	}

	adderr(0, bn, "hr");
	return -1;
}


/*
 * ioctl on the disk, with error reporting
 */
int
gioctl(int cmd, void *ptr)
{
	errno = 0;	/* for scerrwarn and err_fmt calls from caller */
	return ioctl(fds[driveno], cmd, ptr);
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
			close(fds[i]);
}
#endif /* ARCS_SA	arcs versions are different; in arcs.c */
