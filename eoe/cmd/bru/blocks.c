/************************************************************************
 *									*
 *			Copyright (c) 1984, Fred Fish			*
 *			   All Rights Reserved				*
 *									*
 *	This software and/or documentation is protected by U.S.		*
 *	Copyright Law (Title 17 United States Code).  Unauthorized	*
 *	reproduction and/or sales may result in imprisonment of up	*
 *	to 1 year and fines of up to $10,000 (17 USC 506).		*
 *	Copyright infringers may also be subject to civil liability.	*
 *									*
 ************************************************************************
 */


/*
 *  FILE
 *
 *	blocks.c    routines to handle block buffers for bru
 *
 *  SCCS
 *
 *	@(#)blocks.c	9.11	5/11/88
 *
 *  DESCRIPTION
 *
 *	Contains routines to manipulate the block buffers for
 *	the bru utility.  Provides facilities to read, write,
 *	and seek on the archive file.  Buffers blocks internally
 *	and only does I/O in increments of the blocking factor
 *	chosen.
 *
 *	The following routines are provided:
 *
 *		ar_open:	open archive stream
 *		ar_read:	read next logical block
 *		ar_write:	write current logical block
 *		ar_seek:	seek to specified logical block
 *		ar_tell:	return current logical block number
 *		ar_close:	close archive stream
 *		ar_next:	locate next logical block
 *
 *  NOTES
 *
 *	Originally, the archive device was assumed to be seekable unless
 *	the user explicitly requested no seeks via the -p option,
 *	a brutab entry indicated that the device was NOT seekable, or
 *	the archive device was a pipe.   This turned out to cause lots
 *	of confusion when devices are not specified in the brutab file
 *	and were in fact not seekable (such as a raw mag tape unit
 *	used over a network).   So... the safest thing is to make the
 *	default to be NOT seekable and only use seeks when device is
 *	known to be seekable.  Better slow than broken!
 *
 *  BUGS
 *
 *	Currently does not support mixed reading/writing
 *	of archive file.
 *
 */

#include "autoconfig.h"

#include <stdio.h>

#if unix || xenix
#  include <sys/types.h>
#  include <sys/stat.h>
#  include <sys/param.h>	/* for NBPC fall back */
#  include <signal.h>
#  include <ctype.h>
#  if BSD4_2
#    include <sys/time.h>	/* Plain vanilla 4.2 file is here */
#    include <sys/file.h>	/* 4.2 <fcntl.h> is woefully incomplete */
#  else
#    include <time.h>
#    include <fcntl.h>
#  endif
#else
#  include "sys.h"		/* Header file that fakes it for non-unix */
#endif
  
#include "typedefs.h"
#include "dbug.h"
#include "manifest.h"
#include "config.h"
#include "errors.h"
#include "blocks.h"
#include "macros.h"
#include "finfo.h"
#include "devices.h"
#include "flags.h"
#include "bruinfo.h"
#include "exeinfo.h"

BOOLEAN need_swap ();		/* Test for swap needed */
VOID ar_sync ();
VOID do_swap ();		/* Swap bytes/shorts */
static BOOLEAN do_open ();	/* Open an archive file */
static VOID eject_media ();	/* Test for and eject media if supported */
static VOID qfwrite ();		/* Query for continuation on first write */
static VOID new_bufsize ();	/* Force new buffer size */
static VOID bad_vol ();		/* Found bad volume */
static VOID check_vol ();	/* Check for volume not part of same set */
static VOID infer_end ();	/* Process inferred end of volume */
static BOOLEAN recover ();	/* Attempt to recover from I/O errors */
static VOID patch_buffer (); /* Add volume numbers and checksums */
static VOID clean_buffer ();	/* Clean out any leftovers from previous */
static VOID harderror ();	/* Handle non-recoverable read/write error */

extern VOID addz ();		/* Add a ".Z" extension to filename */
extern VOID bru_error ();	/* Report an error to user */
extern VOID fork_shell ();	/* Fork a shell */
extern int execute ();
extern VOID readsizes ();	/* Read media/buffer sizes from blk header */
extern VOID read_info ();
extern VOID sanity ();
extern char response ();		/* Prompt and get response */
extern BOOLEAN seekable ();
extern BOOLEAN forcebuffer ();		/* Nonzero if force small buffer */
extern VOID sig_push ();		/* Push current signal state */
extern VOID sig_pop ();			/* Pop previous signal state */
extern VOID sig_done ();		/* Set signals to xfer to done */
static VOID open_std ();
static VOID allocate ();
VOID switch_media ();			/* Switch media */
VOID phys_seek ();			/* Do physical seek */
static VOID r_phys_seek ();		/* Phys seek during error recovery */
static VOID rgrp ();			/* Read new block group */
static VOID rfile ();			/* Read new block group from file */
static VOID rpipe ();			/* Read new block group from pipe */
static VOID wgrp ();			/* Write block group out */
static VOID wfile ();			/* Write block group to file */
static VOID wpipe ();			/* Write block group to pipe */
static union blk *lba_seek ();		/* Seek to specific logical block */
static void chkandfixaudio(int mt);

/*
 *	In cases of extreme bugs, we can define DUMPBLK, which enables
 *	dumping of the contents of each archive block using the DBUG
 *	macro mechanism.  We don't normally want this, as it results
 *	in overwhelming output...
 */

#ifdef DUMPBLK
   extern VOID dump_blk ();
#  define DUMP_BLK(a,b) dump_blk(a,b)	/* call support routine */
#else
#  define DUMP_BLK(a,b)			/* define the calls away */
#endif


/*
 *	External system interface routines.
 */

extern int s_dup ();		/* Duplicate open file descriptor */
extern char *s_strcpy ();	/* Copy strings */
extern char *s_ctime ();	/* Convert time to string */
extern int s_open ();		/* Invoke library file open function */
extern int s_read ();		/* Invoke library file read function */
extern VOID s_sleep ();		/* Sleep for given number of seconds */
extern int s_close ();		/* Invoke library file close function */
extern int s_write ();		/* Write data to a file */
extern VOID s_free ();		/* Free allocated memory */
extern S32BIT s_lseek ();	/* Seek to file location */
extern int s_eject ();		/* Low level eject media support */
extern BOOLEAN s_format ();	/* Format the media if possible */

/*
 *   External bru functions.
 */
 
extern VOID file_chown ();	/* Change file owner and group */
extern VOID swapbytes ();	/* Swap bytes in a block */
extern VOID swapshorts ();	/* Swap shorts in a block */
extern VOID done ();		/* Clean up and exit with status */
extern VOID copyname ();	/* Copy names around */
extern CHKSUM chksum ();	/* Compute block checksum */
extern VOID tohex ();
extern BOOLEAN dir_access ();	/* Test dir for accessibility */
extern BOOLEAN file_access ();	/* Test file accessibility */
extern VOID tty_newmedia ();	/* Prompt for new media and device */
extern BOOLEAN raw_tape ();	/* Test for raw magnetic tape drive */
extern BOOLEAN end_of_device ();	/* Test for end of device */
extern BOOLEAN unformatted ();	/* Test for unformatted media */
extern BOOLEAN write_protect();	/* Test for write protected media */
extern BOOLEAN chksum_ok ();	/* Test for good block checksum */
extern BOOLEAN magic_ok ();	/* Test for a recognized magic number */
extern VOID *get_memory ();	/* Memory allocator */

extern struct device *get_ardp ();	/* Initialize the ardp */

extern struct bru_info info;	/* Information about bru invocation */
extern struct cmd_flags flags;	/* Flags given on command line */
extern struct exe_info einfo;	/* Execution information */
extern ULONG bufsize;		/* Archive read/write buffer size */
extern ULONG msize;		/* Size of archive media */
extern struct finfo afile;	/* The archive file info struct */
extern time_t artime;		/* Time of archive being read */
extern int release;		/* Major release level */
extern int level;		/* Minor release level */
extern int variant;		/* Variant of bru */
extern struct device *ardp;	/* Pointer to archive device info */
extern char mode;		/* Current execution mode */
extern int errno;		/* System error number */
extern FILE *logfp;		/* Verbosity stream */


static BOOLEAN reading;			/* Reading from archive */
static BOOLEAN bswap = FALSE;		/* Swap bytes in buffer */
static BOOLEAN sswap = FALSE;		/* Swap shorts in buffer */
union blk *blocks = NULL;		/* Pointer to block buffer */
union blk *blocks_orig = NULL;
static BOOLEAN seekflag = FALSE;	/* Use seeks instead of reads */
int lbi = 0;				/* Index into blocks[] */
static LBA gsba = 0;			/* Group start block address */
static LBA vsba = 0;			/* Volume start block address */
static LBA pba = 0;			/* Physical block addr in media */
static LBA grp = 0;			/* Current block group in buffer */
static int lvolume = 0;			/* Current volume number (if not 
					   double buffering) */
int *volume = &lvolume;			/* set to a shared memory area in
					   dblib.c if using double buffering */
int lastvolume;	/* highest volume number reached; for -d or -i after -c, etc. */
static int vol_estimate = 0;		/* Estimation volume number */
static BOOLEAN data = FALSE;		/* Flag for buffer has data */
static LBA lba_estimate = 0;		/* Estimation logical block address */
static BOOLEAN dirty = FALSE;		/* Buffer has been changed */
static BOOLEAN first = TRUE;		/* First read/write of media */
static BOOLEAN checkswap = TRUE;	/* Only swap check once per media */
static BOOLEAN pipe_io = FALSE;		/* Doing I/O to/from a pipe */
static int tries;			/* Current number of retries */
static int firsterrno;			/* Original errno from original err */
static int recursions = 0;		/* Error recovery recursion count */

#if HAVE_SHM

extern int dbl_setup ();		/* set up for buffering */
extern long dbl_read ();		/* read some data from the buffers */
extern long dbl_write ();		/* write data to the buffers */
extern int dbl_flush ();		/* flush buffers */
extern VOID dbl_bufp ();		/* set buffer sizes */
extern VOID dbl_errp ();		/* set error routine pointers */
extern VOID dbl_iop ();			/* set I/O routine pointers */
extern int dbl_rpcdown ();		/* call procedure in child */

#define	DBL_READING	0		/* reading from device */
#define DBL_WRITING	1		/* writing to device */
#define DBL_DOBUF	1		/* perform double buffering */
#define DBL_NOBUF	0		/* be transparent */
#define	DBLF_FDES	0x1		/* update file descriptor */

static int trans_dbl;			/* if in transparent mode */
static int wdblinit;			/* set if write init done */

#endif	/* HAVE_SHM */


#if HAVE_SHM

/*
 * The following two routines are called by the double buffering module
 * when an input or output error occurs.
 */

static int inerr_recover (nb, err, des)
int nb;
int err;
int *des;
{
    int status = 1;

    DBUG_ENTER ("inerr_recover");
    if (recover (nb, err, 1)) {
	*des = afile.fildes;
	status = 0;
    }
    DBUG_RETURN (status);
}

static int outerr_recover (nb, err, des)
int nb;
int err;
int *des;
{
    int status = 1;

    DBUG_ENTER ("outerr_recover");
    if (recover (nb, err, 0)) {
	*des = afile.fildes;
	status = 0;
    }
    DBUG_RETURN (status);
}

static void dupdate (nvolume, des)
long nvolume;
int *des;
{
    switch_media(nvolume);
    if (des != 0) {
	*des = afile.fildes;
    }
}

static setsize (nsize, dummy)
ULONG nsize;
int *dummy;
{
    msize = nsize;
}

#endif	/* HAVE_SHM */


/*
 *  FUNCTION
 *
 *	ar_open   open the archive stream
 *
 *  SYNOPSIS
 *
 *	VOID ar_open ();
 *	
 *  DESCRIPTION
 *
 *	Note that when the archive is opened for the first time,
 *	space is automatically allocated for the block buffer.
 *
 */

VOID ar_open ()
{
    DBUG_ENTER ("ar_open");
    pba = 0;
    gsba = 0;
    vsba = 0;
    grp = 0;
    *volume = 0;
#if HAVE_SHM
    wdblinit = 0;
#endif
    lbi = -1;				/* CAUTION ! */
    dirty = FALSE;
    data = FALSE;
    if (flags.Sflag) {
	switch_media (0);
    } else {
	if (!do_open ()) {
	    switch_media (0);
	}
    }
    if (mode == 't' && !flags.bflag && seekable (afile.fname, BLKSIZE)) {
	DBUG_PRINT ("bufsize", ("bufsize auto-adjusted for min reads"));
	bufsize = BLKSIZE;
	flags.bflag = TRUE;
    }
    allocate ();
    if (reading) {
	read_info ();
#if HAVE_SHM
	if (msize != 0) {
	    if (trans_dbl == DBL_DOBUF) {
		if (dbl_rpcdown (setsize, msize)) {
		    bru_error (ERR_DBLSUP);
		}
	    }
	}
#endif
    } else {
	qfwrite ();
    }
    sanity ();
    DBUG_VOID_RETURN;
}


/*
 *  INTERNAL FUNCTION
 *
 *	open_std    open stdin/stdout as archive
 *
 *  SYNOPSIS
 *
 *	static VOID open_std ()
 *
 *  DESCRIPTION
 *
 *	Opens either the standard input or standard output as
 *	the archive, depending upon whether an archive is being
 *	read or written respectively.
 *
 */

static VOID open_std ()
{
    DBUG_ENTER ("open_std");
    pipe_io = TRUE;
    seekflag = FALSE;
    if (mode == 'c') {
	afile.fildes = s_dup (1);
    } else {
	afile.fildes = s_dup (0);
    }
    DBUG_VOID_RETURN;
}


/*
 *  FUNCTION
 *
 *	ar_seek   seek to a logical block
 *
 *  SYNOPSIS
 *
 *	union blk *ar_seek (offset, whence)
 *	LBA offset;
 *	int whence;
 *
 *  DESCRIPTION
 *
 *	Repositions archive read/write pointer to the first byte
 *	of the logical block specified by the combination of the
 *	current read/write pointer (implicitly) and the offset and
 *	whence values (explicitly).
 *
 *	Sets the file pointer as follows:
 *
 *	    If whence is 0, the pointer is set to offset blocks.
 *
 *	    If whence is 1, the pointer is set to its current
 *		block number plus offset blocks.
 *
 *	Returns a pointer to the first byte of the logical block.
 *
 *	Note that this simply provides a place to either read
 *	data from (after a call to ar_read) or to write data
 *	to (followed by a call to ar_write).  There is no
 *	meaningful I/O implied, although physical I/O may
 *	take place if physical seeks are not allowed.
 *
 *
 */


union blk *ar_seek (offset, whence)
LBA offset;
int whence;
{
    register union blk *blkp;
    register LBA nlba;
    
    DBUG_ENTER ("ar_seek");
    if (whence == 0) {
	nlba = offset;
    } else {
        nlba = offset + gsba + lbi;
    }
    blkp = lba_seek (nlba);
    DBUG_RETURN (blkp);
}


static union blk *lba_seek (nlba)
LBA nlba;
{
    register LBA ngrp;
    register int nvolume;
    register LBA npba;
    register int bufblocks;
    register LBA mblocks;

    DBUG_ENTER("lba_seek");
    DBUG_PRINT ("seek", ("logical seek to archive block %ld", nlba));
    if (afile.fildes >= 0) {
	bufblocks = BLOCKS (bufsize);
	ngrp = nlba / bufblocks;
	DBUG_PRINT ("ngrp", ("new block in group %ld", ngrp));
	if (ngrp != grp) {
	    ar_sync ();
	    data = FALSE;
	    grp = ngrp;
	    DBUG_PRINT ("grp", ("block group %ld", grp));
	    gsba = grp * bufblocks;
	    DBUG_PRINT ("gsba", ("group start block addr %ld", gsba));
	    npba = gsba - vsba;
	    DBUG_PRINT ("npba", ("new phys block addr %ld", npba));
	    if (msize != 0) {
		mblocks = BLOCKS (msize);
		nvolume = (int) (nlba / mblocks);
		if (nvolume != *volume) {
#if HAVE_SHM
		    if (trans_dbl == DBL_DOBUF) {
			if (!reading) {
			    SIGTYPE prevINT;
			    SIGTYPE prevQUIT;

			    /*
			     * In case user pops the interrupt while the
			     * child is doing the next reel, we need to
			     * recover correctly as well.
			     */
			    sig_push (&prevINT, &prevQUIT);
			    signal(SIGINT, SIG_DFL);
			    if (dbl_rpcdown (dupdate, nvolume)) {
				bru_error (ERR_DBLSUP);
			    }
			    sig_pop (&prevINT, &prevQUIT);

			    /*
			     * After remote call completes, clean up by
			     * doing some of the switch_media stuff that
			     * needs to be done locally.
			     */
			    *volume = nvolume;
			    pba = 0;
			    first = TRUE;
			    checkswap = TRUE;
			    sswap = FALSE;
			    bswap = FALSE;
			}
		    } else {
			switch_media (nvolume);
		    }
#else
		    switch_media (nvolume);
#endif
		    vsba = *volume * mblocks;
		    DBUG_PRINT ("vsba", ("new volume start block addr %ld", vsba));
		    npba = gsba - vsba;
		    DBUG_PRINT ("npba", ("new phys block addr %ld", npba));
		}
	    }
	    phys_seek (npba);
	}
    }
    lbi = (int) (nlba - gsba);
    if (lbi < 0 || lbi >= bufblocks) {
	bru_error (ERR_BUG, "lba_seek");
    }
    DBUG_PRINT ("lba_seek", ("new logical block index %d", lbi));
    DBUG_PRINT ("grp", ("grp %ld  gsba %ld  pba %ld", grp, gsba, pba));
    DBUG_RETURN (&blocks[lbi]);
}


/*
 *	Note that the seek is done even if the npba equals
 *	the pba, since the pba may not be correct if error
 *	recovery is being attempted.
 */

VOID phys_seek (npba)
LBA npba;
{
    auto S32BIT offset;
    int nblks = npba - pba;

    DBUG_ENTER ("phys_seek");
    DBUG_PRINT ("pba", ("seek from %ld to %ld", pba, npba));
    if (!seekflag) {
	/* use nblks instead of pba < npba to handle case of tape
		change in rfile() */
	while (nblks > 0) {
	    rgrp ();
	    nblks -= BLOCKS(bufsize);
	}
	data = FALSE;
	if (nblks != 0) {
	    DBUG_PRINT ("badpba", ("seek to %ld got %ld instead", pba, npba));
	    bru_error (ERR_ARPASS);
	    done ();
	}
    } else {
	offset = npba * BLKSIZE;
	DBUG_PRINT ("ar_io", ("physical seek to block %ld", npba));
	if (s_lseek (afile.fildes, offset, 0) != offset) {
	    bru_error (ERR_ARSEEK);
	    done ();
	}
	pba = npba;
    }
    DBUG_VOID_RETURN;
}


/*
 *  FUNCTION
 *
 *	ar_read   read next logical block
 *
 *  SYNOPSIS
 *
 *	VOID ar_read (fip)
 *	struct finfo *fip;
 *
 *  DESCRIPTION
 *
 *	Performs logical read of next logical block.
 *
 */

VOID ar_read (fip)
struct finfo *fip;
{
    register LBA bfound;

    DBUG_ENTER ("ar_read");
    DBUG_PRINT ("flba", ("read file block %ld", fip -> flba));
    if (!data) {
	rgrp ();
    }
    DUMP_BLK (&blocks[lbi], (LBA) (gsba + lbi));
    if (fip == NULL) {
	bru_error (ERR_BUG, "ar_read");
    } else {
	if (!chksum_ok (&blocks[lbi])) {
	    fip -> fi_flags |= FI_CHKSUM;
	    einfo.chkerr++;
	    fip -> chkerrs++;
	    fip -> flba++;
	} else {
	    DBUG_PRINT ("alba", ("archive blk %ld", FROMHEX (blocks[lbi].HD.h_alba)));
	    bfound = FROMHEX (blocks[lbi].HD.h_flba);
	    DBUG_PRINT ("lba", ("file blk %ld", bfound));
	    if (fip -> flba != bfound) {
		DBUG_PRINT ("bseq", ("wrong file block (%ld) found", bfound));
		fip -> flba = bfound + 1;
		fip -> fi_flags |= FI_BSEQ;
	    } else {
		fip -> flba++;
	    }
	}
    }
    DBUG_VOID_RETURN;
}


/*
 *  FUNCTION
 *
 *	ar_write   logical write of current logical block
 *
 *  SYNOPSIS
 *
 *	VOID ar_write (fip, magic)
 *	struct finfo *fip;
 *	int magic;
 *
 *  DESCRIPTION
 *
 *	Performs logical write of current logical block.
 *	This is also where several things in the header
 *	get initialized.
 *
 *	Note that in particular, the current volume number and
 *	block checksum ARE NOT computed at this time.  This operation
 *	is delayed until just before the write to the archive, because
 *	the buffer may have to be written to a different volume than
 *	the current volume.  When it comes time to write out the buffer,
 *	all the blocks are patched with the volume number at the time
 *	of the write (by patch_buffer()), and their checksums are computed
 *	after the volume number patch, just before the write.
 *
 */

VOID ar_write (fip, magic)
struct finfo *fip;
int magic;
{
    DBUG_ENTER ("ar_write");
    data = TRUE;
    dirty = TRUE;
    copyname (blocks[lbi].HD.h_name, fip -> fname);
    if (IS_COMPRESSED (fip)) {
	addz (blocks[lbi].HD.h_name);
    }
    TOHEX (blocks[lbi].HD.h_alba, gsba + lbi);
    TOHEX (blocks[lbi].HD.h_flba, fip -> flba);
    TOHEX (blocks[lbi].HD.h_time, info.bru_time);
    TOHEX (blocks[lbi].HD.h_bufsize, bufsize);
    TOHEX (blocks[lbi].HD.h_msize, msize);
    TOHEX (blocks[lbi].HD.h_magic, magic);
    TOHEX (blocks[lbi].HD.h_release, release);
    TOHEX (blocks[lbi].HD.h_level, level);
    TOHEX (blocks[lbi].HD.h_variant, variant);
    DBUG_VOID_RETURN;
}


/*
 *  FUNCTION
 *
 *	ar_close   close the virtual disk file
 *
 *  SYNOPSIS
 *
 *	VOID ar_close()
 *
 *  DESCRIPTION
 *
 *	If the virtual disk file is open, then closes it.
 *
 *	If the device is unknown, assume that closing it "rewinds" it.
 *	If the device is known and the D_NOREWIND flag is NOT set then
 *	closing it rewinds it.  Setting the pba to 0 is sufficient
 *	(I believe) but initializing the rest of the buffer control
 *	variables doesn't hurt.  This code is starting to get really
 *	twisted!!  Better safe than sorry...
 *
 */

VOID ar_close ()
{
    DBUG_ENTER ("ar_close");

	lastvolume = *volume;	/* in case doing -d */

    if (afile.fildes >= 0) {
        ar_sync();
#if HAVE_SHM
	dbl_flush ();
        eject_media ();
	if (trans_dbl != DBL_DOBUF) {
	    if (s_close (afile.fildes) == SYS_ERROR) {
		bru_error (ERR_ARCLOSE);
	    }
	}
#else
	eject_media ();
	if (s_close (afile.fildes) == SYS_ERROR) {
	    bru_error (ERR_ARCLOSE);
	}
#endif
	afile.fildes = -1;
    }
    if ((ardp == NULL) || (!(ardp -> dv_flags & D_NOREWIND))) {
	pba = 0;
	gsba = 0;
	vsba = 0;
	grp = 0;
	*volume = 0;
	lbi = -1;				/* CAUTION ! */
	dirty = FALSE;
	data = FALSE;
    }
    DBUG_PRINT ("ar_io", ("virtual disk file closed"));
    DBUG_VOID_RETURN;
}



/*
 *  FUNCTION
 *
 *	rgrp    fill blocks buffer from archive stream
 *
 *  SYNOPSIS
 *
 *	rgrp ()
 *
 *  DESCRIPTION
 *
 *	Fills the blocks buffer from the archive stream at the
 *	current read/write point.
 *
 *	Also note that if the input is being read from a pipe
 *	then there is no guarantee that the desired number of
 *	bytes will be available in the pipe.  Only the number
 *	of bytes available will be returned.  Thus we continue
 *	to request additional bytes until the buffer is full or
 *	a read error occurs.
 */

static VOID rgrp ()
{
    DBUG_ENTER ("rgrp");
    if (pipe_io) {
	rpipe ();
    } else {
	rfile ();
    }
    DBUG_VOID_RETURN;
}


/*
 *  FUNCTION
 *
 *	rpipe    file archive buffer from a pipe
 *
 *  SYNOPSIS
 *
 *	static VOID rpipe ()
 *
 *  DESCRIPTION
 *
 *	Used to fill archive buffer from a pipe.  Since pipe
 *	I/O does not block until specified number of bytes are
 *	ready, but rather returns the bytes immediately available,
 *	we simply hang in a loop until the buffer is full or no
 *	bytes are returned (signifying end of file).
 *
 *	If the buffer only gets partially filled (such as when
 *	the blocking factor of the writer is different than the
 *	buffer size, no error occurs as long as the last block
 *	placed in the buffer is the end of archive marker.  This
 *	is because rpipe will never be called again since the
 *	upper level routines will find the end of the archive and
 *	things will terminate normally.  If however, the end of
 *	archive is not found in the data read during a partial
 *	buffer fill, the upper level routines will note that the
 *	remaining (unfilled) data space contains bogus data, another
 *	call to rpipe will eventually occur, it will get no data at
 *	all, a read error error will be issued, and bru will terminate
 *	via a direct call to done.
 *
 */

static VOID rpipe ()
{
    register char *destination;
    register int request;
    register int iobytes;

    DBUG_ENTER ("rpipe");
    destination = blocks[0].bytes;
    request = bufsize;
    DBUG_PRINT ("rpipe", ("get %u bytes at pba %ld", request, pba));
    do {
	iobytes = s_read (afile.fildes, destination, (UINT) request);
	request -= iobytes;
	destination += iobytes;
	DBUG_PRINT ("pipe_in", ("got %d bytes from pipe", iobytes));
    } while (iobytes > 0 && request > 0);
    einfo.breads += BLOCKS (bufsize - request);
    pba += BLOCKS (bufsize - request);
    if (request == bufsize) {
	bru_error (ERR_ARREAD, pba);
	einfo.rhard++;
	done ();
    }
    data = TRUE;
    dirty = FALSE;
    if (bswap || sswap) {
	do_swap ();
    }
    if (first) {
	check_vol ();
	first = FALSE;
    }
    DBUG_VOID_RETURN;
}


/*
 *	Note that we only check for the proper volume if the
 *	read gets at least one block of actual data.
 *
 *	One problem here with error recovery.  When the retry limit
 *	is reached, we attempt to continue by just ignoring the problem
 *	and going for a new block.  Since the file pointer still points
 *	to the current bad spot, we want to be able to skip over it by
 *	doing a phys_seek to the new location.  This works fine with seekable
 *	devices, like floppies, but for non-seekable devices phys_seek 
 *	attempts to reposition the file pointer by reading to the new
 *	block, which won't work because that's how we got into this mess
 *	in the first place.  The bottom line is, if the device is not
 *	seekable, we haven't gotten past the bad spot by retries, and
 *	read/writes do not advance the media (D_ADVANCE in brutab), we
 *	are probably stuck there forever and should just give up rather
 *	than hanging in an infinite loop.
 */
 
static VOID rfile ()
{
    register int iobytes;

    DBUG_ENTER ("rfile");
    tries = 0;
    iobytes = 0;
    while (iobytes != bufsize) {
	DBUG_PRINT ("rfile", ("read %u bytes at pba %ld", bufsize, pba));
#if HAVE_SHM
	iobytes = dbl_read (blocks[0].bytes, (long) bufsize);
#else
	iobytes = s_read (afile.fildes, blocks[0].bytes, bufsize);
#endif
	tries++;
	einfo.breads += BLOCKS (bufsize);
	if (iobytes == bufsize) {
	    pba += BLOCKS (bufsize);
	} else {
	    einfo.rsoft++;
	    if (!recover (iobytes, errno, TRUE)) {
		break;
	    }
	}
    }
    data = TRUE;
    dirty = FALSE;
    if (bswap || sswap) {
	do_swap ();
    }
    if (first) {
	check_vol ();
	first = FALSE;
    }
    DBUG_VOID_RETURN;
}


/*
 *  FUNCTION
 *
 *	wgrp    write blocks buffer to archive stream
 *
 *  SYNOPSIS
 *
 *	static VOID wgrp ()
 *
 *  DESCRIPTION
 *
 *	Writes the blocks buffer to the archive stream at the
 *	current read/write point.
 *
 *	If the output is going to a pipe, there is a limit on how
 *	much data can be written in a single write command.  Thus
 *	there is a separate routine used for writing to pipes that
 *	continues to write until the buffer is exhausted or a write
 *	error occurs.
 *
 */

static VOID wgrp ()
{
    DBUG_ENTER ("wgrp");
    if (pipe_io) {
	wpipe ();
    } else {
	wfile ();
    }
    DBUG_VOID_RETURN;
}


/*
 *  FUNCTION
 *
 *	wpipe    write buffer to a pipe
 *
 *  SYNOPSIS
 *
 *	static VOID wpipe ()
 *
 *  DESCRIPTION
 *
 *	Used to write the archive buffer out to a pipe.  Since there
 *	is a limit on how many bytes can be written to a pipe, we
 *	write out the buffer one archive block at a time.
 *
 *	Note that we specifically always write out a full buffer worth
 *	of data, even when the last buffer written contains only a few
 *	new blocks.  Although we know the last block used (in lbi), we
 *	write out the full buffer because this is the same behavior
 *	that occurs when the output is stream is not a pipe.  Thus
 *
 *		bru -cf - >/usr/me/myfile
 *		bru -cf - | dd of=/usr/me/myfile
 *		bru -cf /usr/me/myfile
 *
 *	will all produce output files of the same size (exact multiple
 *	of bufsize).  If this was not true, a subsequent read of the archive
 *	file would have a problem with the last buffer since it would be
 *	only a partial buffer.  Naturally, this scheme results in some
 *	left over blocks from the last write being used to pad out the
 *	buffer to a full bufsize bytes.  These normally cause no problem
 *	except in the very rare case where the terminator block is somehow
 *	missed during a read.
 *
 */

static VOID wpipe ()
{
    register int index;
    register int request;
    register int iobytes;
    register int bufblocks;
    auto char *source;

    DBUG_ENTER ("wpipe");
    patch_buffer ();
    bufblocks = BLOCKS (bufsize);
    DBUG_PRINT ("wpipe", ("write out %d blocks from buffer", bufblocks));
    for (index = 0; index < bufblocks; index++) {
	source = blocks[index].bytes;
	request = BLKSIZE;
	DBUG_PRINT ("pipe_out", ("write %u bytes from block %d", request, index));
	iobytes = s_write (afile.fildes, source, (UINT) request);
	if (iobytes != request) {
	    bru_error (ERR_ARWRITE, pba);
	    einfo.whard++;
	    done ();
	}
	pba++;
	einfo.bwrites++;
    }
    data = TRUE;
    dirty = FALSE;
    DBUG_VOID_RETURN;
}


/*
 *  FUNCTION
 *
 *	wfile    write buffer to a non-pipe output file
 *
 *  SYNOPSIS
 *
 *	wfile ();
 *
 *  DESCRIPTION
 *
 *	Used to write output to a non-pipe archive.  Writes are done
 *	in BUFSIZE increments, with errors being retried up to the
 *	specified limit.
 *
 *	For complications due to error recovery algorthms, see comments
 *	in description of analogous function rfile() for reads.
 *
 */

static VOID wfile ()
{
    register int iobytes;

    DBUG_ENTER ("wfile");
    tries = 0;
    iobytes = 0;
    while (iobytes != bufsize) {
	DBUG_PRINT ("wfile", ("write %u bytes at pba %ld", bufsize, pba));
#if HAVE_SHM
	iobytes = dbl_write (blocks[0].bytes, bufsize);
#else
	patch_buffer ();
	iobytes = s_write (afile.fildes, blocks[0].bytes, bufsize);
#endif
	tries++;
	einfo.bwrites += BLOCKS (bufsize);
	if (iobytes == bufsize) {
	    pba += BLOCKS (bufsize);
	    dirty = FALSE;
	} else {
#if HAVE_SHM
	    if (trans_dbl == DBL_NOBUF) {
		einfo.wsoft++;
		if (!recover (iobytes, errno, FALSE)) {
		    break;
		}
	    }
#else
	    einfo.wsoft++;
	    if (!recover (iobytes, errno, FALSE)) {
		break;
	    }
#endif
	}
    }
    first = FALSE;
    if (bswap || sswap) {
	do_swap ();
    }
    DBUG_VOID_RETURN;
}


/*
 *  FUNCTION
 *
 *	ar_sync   write out sector buffer if "dirty"
 *
 *  SYNOPSIS
 *
 *	VOID ar_sync()
 *
 *  DESCRIPTION
 *
 *	Writes the contents of block buffer if necessary.
 *
 */

VOID ar_sync()
{

    DBUG_ENTER("ar_sync");
    if (afile.fildes >= 0 && dirty) {
	DBUG_PRINT ("ar_io", ("flushing dirty buffer"));
	wgrp ();
    }
    DBUG_VOID_RETURN;
}



/*
 *  FUNCTION
 *
 *	allocate    allocate space for the block buffer
 *
 *  SYNOPSIS
 *
 *	static VOID allocate ()
 *
 *  DESCRIPTION
 *
 *	This function is called when the archive file is first
 *	opened, to allocate space for the block buffers.
 *
 *	Any error is immediately fatal.
 *
 */

static VOID allocate ()
{
	int pgsize;
    DBUG_ENTER ("allocate");

    if (blocks == NULL) {
	if(((pgsize=getpagesize())<=0) || (pgsize & (pgsize-1))) {
		bru_error (ERR_PAGESZ, pgsize);
		done ();
	}
	pgsize--;	/* make it a mask */

	blocks = (union blk *) get_memory (bufsize+pgsize, FALSE);
	if (blocks == NULL) {
	    bru_error (ERR_BALLOC, bufsize/1024);
	    done ();
	}
	/* align to page boundary for best performance */
	blocks_orig = blocks;
	blocks = (union blk *)(((unsigned int)blocks+pgsize)&~pgsize);
    }
    DBUG_VOID_RETURN;
}


VOID deallocate ()
{
    DBUG_ENTER ("deallocate");
    if (blocks_orig != NULL) {
	s_free ((char *) blocks_orig);
	blocks = blocks_orig = NULL;
    }
    DBUG_VOID_RETURN;
}


/*
 *  FUNCTION
 *
 *	ar_tell    return current logical block address
 *
 *  SYNOPSIS
 *
 *	LBA ar_tell ()
 *
 *  DESCRIPTION
 *
 *	Returns logical block address of current block in archive.
 *	The current block is the block which was the last argument of
 *	an lba_seek command or -1 by default.
 *
 *  NOTES
 *
 *	Does not use DBUG_ENTER, LEAVE, or DBUG_n macros because this
 *	function is called in the middle of printing some verbose
 *	output.
 *
 */

LBA ar_tell ()
{
    register LBA rtnval;

    if (mode == 'e') {
	rtnval = lba_estimate;
    } else {
	rtnval = gsba + lbi;
    }
    return (rtnval);
}


/*
 *  FUNCTION
 *
 *	ar_next    locate next logical block in archive
 *
 *  SYNOPSIS
 *
 *	union *blk ar_next ()
 *
 *  DESCRIPTION
 *
 *	Locates next logical block in archive and returns pointer
 *	to usuable data buffer.  Similar to lba_seek except that
 *	the logical block number defaults to the current logical
 *	block number + 1.
 *
 *	Note that no I/O is implied by this operation, it simply
 *	locates a buffer and sets up a new current logical block
 *	number.  The ar_read function must still be called if
 *	the buffer is expected to contain valid data.
 *
 */

union blk *ar_next ()
{
    register union blk *new;

    DBUG_ENTER ("ar_next");
    new = ar_seek (1L, 1);
    DBUG_RETURN (new);
}


/*
 *  NOTES
 *
 *	It is assumed that switching media will cause the
 *	next read to start at physical block 0.  This may
 *	not always be true if the archive file is not closed.
 *	One would expect it to be true for magnetic tapes but
 *	not necessarily random access devices like removable
 *	disks.  Thus it may be necessary to use the D_REOPEN
 *	flag in brutab for the specific device, to force this
 *	condition.  In some systems, such as the Convergent
 *	Miniframe, removing a floppy without closing and
 *	reopening the file (by D_REOPEN) will cause a read
 *	error anyway.
 *
 *	It is now the default to close the archive device file
 *	and reopen it on a media switch.  The reason for this is
 *	that for some devices, a reopen is a must, otherwise we
 *	will never be able to access the device again.  If this
 *	situation occurs for a device not in the brutab file, bru
 *	will simply go into an infinite prompting loop, asking for
 *	the next volume.  Thus the D_REOPEN flag is now obsolete.
 *	The D_NOREOPEN flag is used to suppress the close and reopen
 *	for known devices that can continue without the reopen.
 *
 *	If the user did not specify an explicit buffer size, and the
 *	new media's entry in the brutab file does specify a default
 *	buffer size that is not identical to the current buffer size,
 *	a warning message is printed.  The buffer size MUST remain
 *	constant across all volumes of a multiple volume archive.
 *
 */

VOID switch_media (nvolume)
int nvolume;
{
    register BOOLEAN new;
    UINT newbufsize;
    BOOLEAN new_arfile ();
    
    DBUG_ENTER ("switch_media");

    eject_media ();

    if (ardp == NULL || !(ardp -> dv_flags & D_NOREOPEN)) {
	if (afile.fildes >= 0) {
	    DBUG_PRINT ("reopen", ("close archive prior to media switch"));
	    if (s_close (afile.fildes) == SYS_ERROR) {
		bru_error (ERR_ARCLOSE);
	    }
	    afile.fildes = -1;
	}
    }
    DBUG_PRINT ("vol", ("new volume = %d", nvolume));
    new = new_arfile (nvolume);
    if (new || ardp == NULL || !(ardp -> dv_flags & D_NOREOPEN)) {
	ardp = get_ardp (afile.fname);
	if (ardp != NULL && !flags.bflag) {
	    newbufsize = BLOCKS (ardp -> dv_bsize) * BLKSIZE;
	    if (newbufsize != bufsize && !forcebuffer ()) {
		bru_error (ERR_BSIZE, bufsize, newbufsize);
	    }
	}
	if (afile.fildes >= 0) {
	    if (s_close (afile.fildes) == SYS_ERROR) {
		bru_error (ERR_ARCLOSE);
	    }
	    afile.fildes = -1;
	}
	if (!do_open ()) {
	    switch_media (nvolume);
	}
    }
    if (new) {
	qfwrite ();
    }
    *volume = nvolume;
    pba = 0;
    first = TRUE;
    checkswap = TRUE;
    sswap = FALSE;
    bswap = FALSE;
    DBUG_PRINT ("swap", ("checkswap = first = TRUE, sswap = bswap = FALSE"));
    if (reading) {
	data = FALSE;
    }
    DBUG_VOID_RETURN;
}


/*
 *  FUNCTION
 *
 *	r_phys_seek    recovery phys_seek with recursion detection
 *
 *  SYNOPSIS
 *
 *	static VOID r_phys_seek (npba)
 *	LBA npba;
 *
 *  DESCRIPTION
 *
 *	During error recovery, we may want to call phys_seek to seek
 *	to a specific physical location on the media.  For nonseekable
 *	devices, this may fail because we may attempt to seek to a given
 *	location by reading, which can itself get another error, thus
 *	getting into a recursive situation.  This routine simply
 *	increments a recursion counter for each call to the real
 *	phys_seek, and decrements it at each return.  Calls to phys_seek
 *	which never return cause the counter to increment and when
 *	it reaches the preset limit, this volume is abandoned.
 *
 */

static VOID r_phys_seek (npba)
LBA npba;
{
    DBUG_ENTER ("r_phys_seek");
    DBUG_PRINT ("recursions", ("recursions = %d", recursions));
    if (recursions < D_RECUR) {
	recursions++;
	phys_seek (npba);
	recursions--;
    } else {
	recursions = 0;
	errno = firsterrno;
	bru_error (ERR_EOV, *volume + 1);
	infer_end (gsba-vsba);
    }
    DBUG_VOID_RETURN;
}


/*
 *  FUNCTION
 *
 *	recover    recover from a read/write error
 *
 *  SYNOPSIS
 *
 *	static VOID recover (iobytes, ioerr, readflag)
 *	int iobytes;
 *	int ioerr;
 *	BOOLEAN readflag;
 *
 *  DESCRIPTION
 *
 *	When a read or write error on the archive media is detected,
 *	this routine gains control and attempts to set things up
 *	so that the next read or write will be successful.
 *	The recovery algorithms are complicated by the fact that
 *	the behavour due to various errors (such as insertion of
 *	unformatted media, end of device, media I/O errors, etc)
 *	is very implementation and device dependent.
 *
 *	The basic strategy is to switch to next media if the
 *	error was definitely due to an attempt to do I/O past
 *	the physical bounds, otherwise do at least one retry.
 *	Then attempt to infer either the end of the device or
 *	insertion of unformatted media.  Finally, simply retry
 *	the I/O up to the retry limit specified in the configuration
 *	file.
 *
 *	Because the error recovery algorithms can cause the original
 *	errno, from the original error, to be destroyed, we save
 *	errno for the first error, then restore it if all attempts
 *	to recover fail and we need to report an error.  This prevents
 *	perror, or its equivalent, from reporting mysterious reasons for
 *	failures.
 *
 */

static BOOLEAN recover (iobytes, ioerr, readflag)
int iobytes;
int ioerr;
BOOLEAN readflag;
{
    register int nvolume;
    register BOOLEAN unfmt;
    register BOOLEAN wprot;
    register BOOLEAN eod;
    register BOOLEAN rawtape;
    register LBA errorpba;
    register BOOLEAN recoverable;
    register BOOLEAN advanced;
    register BOOLEAN msgdone = FALSE;

    DBUG_ENTER ("recover");
    DBUG_PRINT ("fault", ("io of %d bytes", iobytes));
    DBUG_PRINT ("fault", ("ioerr %d", ioerr));
    if (tries == 1) {
	firsterrno = ioerr;
	DBUG_PRINT ("errno", ("errno from original error = %d", firsterrno));
    }
    errorpba = pba;
    advanced = (ardp != NULL && ardp -> dv_flags & D_ADVANCE);
    if (advanced) {
	pba += BLOCKS (bufsize);
    }
    DBUG_PRINT ("fault", ("error pba %ld, current pba %ld", errorpba, pba));
    DBUG_PRINT ("fault", ("gsba %ld grp %ld", gsba, grp));
    DBUG_PRINT ("fault", ("vsba %ld", vsba));
    s_sleep ((UINT) 3);
    unfmt = unformatted (iobytes, ioerr, readflag);
    wprot = write_protect (iobytes, ioerr, readflag);
#ifdef sgi
    /*	regardless of what write_protect() returns, if errno is EROFS,
	then the media is write-protected! */
    if(!readflag && wprot != TRUE) {
	if(ioerr == EROFS)
	    wprot = TRUE;
	else if(sgiqic24(ioerr) == TRUE) {
	    wprot = TRUE;
	    msgdone = TRUE;
	}
    }
#endif sgi
    eod = end_of_device (iobytes, ioerr, readflag);
    rawtape = raw_tape ();
    recoverable = TRUE;
    if (msize != 0 && errorpba >= BLOCKS (msize)) {
	DBUG_PRINT ("fault", ("found end of volume %d", *volume + 1));
	nvolume = (int) (*volume + (errorpba / BLOCKS (msize)));
	switch_media (nvolume);
	vsba = nvolume * BLOCKS (msize);
    } else if (!first && eod && msize == 0) {
	DBUG_PRINT ("fault", ("end of known device of unknown size"));
	infer_end (errorpba);
    } else if (ardp != NULL && first && unfmt && !wprot) {
	DBUG_PRINT ("fault", ("known device unformatted"));
	errno = firsterrno;
	if (!(ardp -> dv_flags & D_FORMAT) || !(s_format (afile.fildes))) {
	    bru_error (ERR_FORMAT);
	    switch_media (*volume);
	}
	r_phys_seek (errorpba);
    } else if (ardp != NULL && first && !unfmt && wprot && !readflag) {
	DBUG_PRINT ("fault", ("known device write protected"));
	errno = firsterrno;
	if(msgdone == FALSE)
	    bru_error (ERR_WPROT);
	switch_media (*volume);
	r_phys_seek (errorpba);
    } else if (ardp != NULL && first && iobytes > 0 && rawtape && readflag) {
	DBUG_PRINT ("fault", ("read of known raw tape with other blocking factor"));
#if HAVE_SHM
	if (trans_dbl == DBL_NOBUF) {
	    new_bufsize (iobytes);
	    r_phys_seek (errorpba);
	}
#else
	new_bufsize (iobytes);
	r_phys_seek (errorpba);
#endif
    } else if (ardp != NULL && first && unfmt && wprot) {
	DBUG_PRINT ("fault", ("known device is unformatted or write protected"));
	errno = firsterrno;
	bru_error (ERR_FIRST);
	switch_media (*volume);
	r_phys_seek (errorpba);
    } else if (first && (iobytes == -1) && (ioerr == EIO)) {
	DBUG_PRINT ("fault", ("device had IO Error"));
	errno = firsterrno;
	if(chknomedia())
		bru_error(ERR_NOMEDIA);
	else
		bru_error (ERR_IOERR);
	switch_media (*volume);
	r_phys_seek (errorpba);
    } else if (msize == 0 && (!seekflag && advanced)) {
	DBUG_PRINT ("fault", ("can't back up and media size unknown, infer end"));
	errno = firsterrno;
	bru_error (ERR_IEOV, *volume + 1);
	infer_end (errorpba);
    } else if (msize != 0 && (!seekflag && advanced)) {
	DBUG_PRINT ("fault", ("can't back up, but not end, so unrecoverable"));
	DBUG_PRINT ("fault", ("assume advanced over bad spot"));
	recoverable = FALSE;
	harderror (errorpba, readflag, iobytes);
	/* try twice as hard as most other cases, but not forever;
	 * don't retry at all if eod is set */
	if(eod || tries > (2*D_TRIES))
	    infer_end (errorpba);
    } else if (msize == 0 && tries == D_TRIES) {
	DBUG_PRINT ("fault", ("retry limit hit, media size unknown, infer end"));
	errno = firsterrno;
	bru_error (ERR_IEOV, *volume + 1);
	infer_end (errorpba);
    } else if (msize != 0 && tries >= D_TRIES) {
	recoverable = FALSE;
	harderror (errorpba, readflag, iobytes);
	if (seekflag) {
	    DBUG_PRINT ("fault", ("seek past bad spot"));
	    r_phys_seek ((LBA) (errorpba + BLOCKS (bufsize)));
	} else {
	    recursions = 0;
	    errno = firsterrno;
	    bru_error (ERR_EOV, *volume + 1);
	    infer_end (errorpba);
	}
    } else if ((ardp == NULL) && isrmt(afile.fildes) && first &&
	       !unfmt && wprot && !readflag) {
	DBUG_PRINT ("fault", ("remote device write protected"));
	errno = firsterrno;
	if(msgdone == FALSE)
	    bru_error (ERR_WPROT);
	switch_media (*volume);
	r_phys_seek (errorpba);
    } else {
	DBUG_PRINT ("fault", ("retry %d at %ld", tries, errorpba));
	r_phys_seek (errorpba);
    }
    DBUG_PRINT ("fault", ("volume %d  vsba %ld", *volume, vsba));
    DBUG_PRINT ("fault", ("gsba %ld  pba %ld", gsba, pba));
    DBUG_RETURN (recoverable);
}


static BOOLEAN do_open ()
{
    register int open_flag;
    register int open_mode;
    register BOOLEAN rtnval;
    register BOOLEAN accessible;
    register BOOLEAN exists;

    DBUG_ENTER ("do_open");
#if HAVE_SHM
    if (!flags.Dflag || pipe_io || seekflag) {
	trans_dbl = DBL_NOBUF;
    } else {
        trans_dbl = DBL_DOBUF;
    }
    dbl_errp (inerr_recover, outerr_recover);
    dbl_bufp (0, 0, bufsize);
    dbl_iop (s_read, s_write);
#endif
    if (mode == 'c') {
	reading = FALSE;
    } else {
	reading = TRUE;
    }
    if (STRSAME (afile.fname, "-")) {
	open_std ();
	rtnval = TRUE;
    } else {
	rtnval = FALSE;
	exists = file_access (afile.fname, A_EXISTS, FALSE);
	if (mode == 'c') {
	    open_flag = O_WRONLY | O_CREAT;
	    open_mode = 0666;
		/* this code used to check using the file_access, but the
		 * concept of access in bru is badly broken.  For example,
		 * if the the "device" is a symlink that isn't dangling, the
		 * test would always fail.  Since that's always the case with
		 * /dev/tape in irix 6.4, and the open code does the right thing
		 * always, I've just removed the totally bogus code, and always
		 * set accessible to TRUE.  Olson, 11/96
		*/
	    accessible = TRUE;
	} else {
	    open_flag = O_RDONLY;
	    open_mode = 0;
		/* see comments in block above.  Olson */
		accessible = TRUE;
	}
	if (!accessible) {
	    rtnval = FALSE;
	    bru_error (ERR_AROPEN, afile.fname);
	} else {
	    afile.fildes = s_open (afile.fname, open_flag, open_mode);
	    if (afile.fildes < 0) {
		rtnval = FALSE;
		eject_media ();
		bru_error (ERR_AROPEN, afile.fname);
	    } else {
		chkandfixaudio(afile.fildes);
		rtnval = TRUE;
		if (flags.pflag) {
		    seekflag = FALSE;
		} else {
		    seekflag = seekable (afile.fname, BLKSIZE);
		} 
		if (!exists) {
		    file_chown (afile.fname, info.bru_uid, info.bru_gid);
		}
#if HAVE_SHM
		if (!wdblinit) {
		    if (!reading && trans_dbl == DBL_DOBUF) {
			wdblinit++;
		    }
                    if (mode == 'c') {
                        if (dbl_setup (afile.fildes, trans_dbl, DBL_WRITING)) {
            	            bru_error (ERR_DBLSUP);
            	            done ();
            	        }
                    } else {
            	        if (dbl_setup (afile.fildes, trans_dbl, DBL_READING)) {
            	            bru_error (ERR_DBLSUP);
            	            done ();
            	        }
                    }
                    if (trans_dbl == DBL_DOBUF && flags.vflag > 1) {
			s_fprintf (logfp, "using double buffering to %s\n", afile.fname);
		    }
		}
#endif
	    }
	}
    }
    DBUG_PRINT ("arf", ("%s open on %o", afile.fname, afile.fildes));
    DBUG_RETURN (rtnval);
}


VOID do_swap ()
{
    register int maxindex;
    register int index;

    DBUG_ENTER ("do_swap");
    maxindex = BLOCKS (bufsize);
    if (bswap) {
	for (index = 0; index < maxindex; index++) {
	    DBUG_PRINT ("swap", ("swap bytes for block %ld", (long) (gsba+index)));
	    swapbytes (&blocks[index]);
	}
    }
    if (sswap) {
	for (index = 0; index < maxindex; index++) {
	    DBUG_PRINT ("swap", ("swap shorts for block %ld", (long) (gsba+index)));
	    swapshorts (&blocks[index]);
	}
    }
    DBUG_VOID_RETURN;
}


/*
 *	Check to see if each buffer needs to have any byte or
 *	short swapping done.  The checkswap flag insures that this
 *	is only done once per media, even when things are called
 *	recursively.  Otherwise it is possible to get confused over
 *	whether or not swapping is necessary.
 */
 
BOOLEAN need_swap (blkp)
register union blk *blkp;
{
    DBUG_ENTER ("need_swap");
    DBUG_PRINT ("swap", ("checkswap=%d", checkswap));
    DBUG_PRINT ("swap", ("sswap=%d bswap=%d", sswap, bswap));
    if (checkswap) {
	checkswap = FALSE;
	bswap = FALSE;
	sswap = FALSE;
	swapbytes (blkp);
	DUMP_BLK (blkp, 0L);
	if (chksum_ok (blkp) && magic_ok (blkp)) {
	    DBUG_PRINT ("swap", ("swap bytes"));
	    bswap = TRUE;
	}
	swapshorts (blkp);
	DUMP_BLK (blkp, 0L);
	if (chksum_ok (blkp) && magic_ok (blkp)) {
	    DBUG_PRINT ("swap", ("swap bytes and shorts"));
	    sswap = TRUE;
	}
	swapbytes (blkp);
	DUMP_BLK (blkp, 0L);
	if (chksum_ok (blkp) && magic_ok (blkp)) {
	    DBUG_PRINT ("swap", ("swap shorts"));
	    sswap = TRUE;
	    bswap = FALSE;
	}
	swapshorts (blkp);
	DUMP_BLK (blkp, 0L);
    }
    DBUG_PRINT ("swap", ("checkswap=%d", checkswap));
    DBUG_PRINT ("swap", ("sswap=%d bswap=%d", sswap, bswap));
    DBUG_RETURN (sswap || bswap);
}


static VOID infer_end (errpba)
LBA errpba;
{
    DBUG_ENTER ("infer_end");
    DBUG_PRINT ("fault", ("inferred end of volume %d", *volume + 1));
    switch_media (*volume + 1);
    vsba += errpba;
    phys_seek (0L);
    tries = 0;
    recursions = 0;
    DBUG_VOID_RETURN;
}


/*
 *	Note that we must check for a proper checksum because it
 *	is not guaranteed at this point that the buffer data is
 *	good (a bad read for example).  We don't want to be
 *	generating messages to load volume 67,892 if the first
 *	block of a media is unreadable for some reason!
 *
 *	Also note that since this is at a lower level than readinfo,
 *	we also need to insure that the swap flags are up to date
 *	by calling need_swap().
 *
 */
 
static VOID check_vol ()
{
    register int vol;

    DBUG_ENTER ("check_vol");
    if (need_swap (&blocks[0])) {
	do_swap ();
    }
    if (chksum_ok (&blocks[0])) {
	vol = (int) FROMHEX (blocks[0].HD.h_vol);
	if (vol != *volume) {
	    if(flags.Tflag) {
		if(flags.gflag) {
		    (VOID) s_fprintf(logfp, "E0 %d %d\n", vol+1, *volume+1);
		    s_fflush(logfp);
		    done();
		}
		else {
    		    LBA oldpba;
		    (VOID) s_fprintf(logfp, "E3 %d %d\n", vol+1, *volume+1);
		    s_fflush(logfp);
	    	    oldpba = pba;
	    	    switch_media (*volume);
	    	    phys_seek ((LBA) (oldpba - BLOCKS (bufsize)));
	    	    rgrp ();
		}
	    }
	    else {
	    	bru_error (ERR_ARVOL, vol + 1, *volume + 1);
	    	bad_vol (vol);
	    }
	}
	if (artime != (time_t)0 && artime != FROMHEX (blocks[0].HD.h_time)) {
	    if(flags.Tflag) {
		if(flags.gflag) {
		    (VOID) s_fprintf(logfp, "E1\n");
		    s_fflush(logfp);
		    done();
		}
		else {
    		    LBA oldpba;
		    (VOID) s_fprintf(logfp, "E1\n");
		    s_fflush(logfp);
	    	    oldpba = pba;
	    	    switch_media (*volume);
	    	    phys_seek ((LBA) (oldpba - BLOCKS (bufsize)));
	    	    rgrp ();
		}
	    }
	    else {
	    	bru_error (ERR_ARTIME, s_ctime ((long *) &artime));
	    	bad_vol (vol);
	    }
	}
    }
    DBUG_VOID_RETURN;
}


/*
 *	Note that for the reload case we must save the current pba, since
 *	switch_volume() currently forces it back to zero.  We need to be
 *	able to seek to the same relative point in the new volume.
 */
 
static VOID bad_vol (vol)
int vol;
{
    register char key;
    register LBA mblocks;
    register LBA oldpba;

    DBUG_ENTER ("bad_vol");
    key = response ("c => continue  q => quit  r => reload  s => shell",
    	'q');
    switch (key) {
	case 'q':
	case 'Q':
	    done ();
	    break;
	case 'r':
	case 'R':
	    oldpba = pba;
	    switch_media (*volume);
	    phys_seek ((LBA) (oldpba - BLOCKS (bufsize)));
	    rgrp ();
	    break;
	case 'c':
	case 'C':
	    readsizes (&blocks[0]);
	    mblocks = BLOCKS (msize);
	    gsba += (vol - *volume) * mblocks;
	    vsba = vol * mblocks;
	    grp = gsba / BLOCKS (bufsize);
	    *volume = vol;
	    DBUG_PRINT ("cont", ("grp %ld gsba %ld", grp, gsba));
	    DBUG_PRINT ("cont", ("volume %d vsba %ld", *volume, vsba));
	    break;
	case 'S':
	case 's':
	    fork_shell ();
	    bad_vol (vol);
	    break;
    }
    DBUG_VOID_RETURN;
}


BOOLEAN ar_ispipe ()
{
    return (pipe_io);
}


/*
 *  FUNCTION
 *
 *	new_arfile    issue prompt for new media and save file name
 *
 *  SYNOPSIS
 *
 *	BOOLEAN new_arfile (vol)
 *	int vol;
 *
 *  DESCRIPTION
 *
 *	Issue prompt for new volume and remember name if specified.
 *	Note that the new name, if any, overwrites the old name.
 *	Returns TRUE if new name was given, false otherwise.
 *
 */
 
BOOLEAN new_arfile (vol)
int vol;
{
    register BOOLEAN rtnval;
    auto char newname[NAMESIZE];
    SIGTYPE prevINT;
    SIGTYPE prevQUIT;
    
    DBUG_ENTER ("new_arfile");
    DBUG_PRINT ("vol", ("new volume = %d", vol));
    rtnval = FALSE;
    sig_push (&prevINT, &prevQUIT);
    sig_done ();
#ifdef amiga
    FlushRaw ();
    MotorOff ();
#endif
    if(flags.Tflag) {
	s_fprintf(logfp, "S %d\n", vol+1);
	s_fflush(logfp);
	fgets(newname, sizeof(newname), stdin);
	if(strncmp(newname, "C", 1) == 0)
		newname[0] = EOS;
	else {
		printf("Internal error: %s", newname);
		done();
	}
    }
    else {
    	tty_newmedia (vol + 1, afile.fname, newname, sizeof (newname));
    }
    sig_pop (&prevINT, &prevQUIT);
    if ( (newname[0] != EOS) && isgraph(newname[0]) ) {
	rtnval = TRUE;
	copyname (afile.fname, newname);
    }
    DBUG_RETURN (rtnval);
}


/*
 *  FUNCTION
 *
 *	ar_vol    return number of current volume
 *
 *  SYNOPSIS
 *
 *	int ar_vol ()
 *
 *  DESCRIPTION
 *
 *	Returns the index of the current volume [0,1,...].
 *	Note that if current mode is media estimation mode
 *	then the value returned is the estimated volume number
 *	since there is not really an archive open.
 *
 *  NOTES
 *
 *	This function does not contain DBUG package macros because
 *	it is called in the middle of printing normal output lines.
 *
 */
 
int ar_vol ()
{
    register int rtnval;
    
    if (mode == 'e') {
	rtnval = vol_estimate;
    } else {
	rtnval = *volume;
    }
    return (rtnval);
}


/*
 *	Note that the new buffer size will be smaller than the
 *	current buffer size, since we are reading from the raw
 *	magtape file and got less data than requested.  We
 *	cannot deallocate the larger buffer and allocate a
 *	smaller buffer at this level, since parent routines
 *	have been passed a pointer to the current buffer, and
 *	know nothing about the fact that we have had an "archive
 *	buffer fault".  Thus we will simply force the buffer size
 *	to be smaller but continue to use the same space allocated
 *	for the larger, previous buffer.
 * 
 *	Also note that if the old buffer size was bigger than the
 *	entire archive (say a buffer size of 200K, when the archive
 *	is 4 records of 20K each) some drivers may return all the
 *	archive data, rather than just the data in the first physical
 *	record.  The Sun 3/50 (/dev/rst8, OS release 3.0) is one such 
 *	system.  This may confuse us terribly...
 *
 *	I have commented out the flags.bflag line because it
 *	was suppressing checking of the recorded buffer size against
 *	the actual buffer size in routine readsizes() in readinfo.c.
 *	I am not sure why it was there to begin with...
 *	Fred Fish, 20-Nov-86.  
 */
 
static VOID new_bufsize (size)
int size;
{
    DBUG_ENTER ("new_bufsize");
    /* flags.bflag = TRUE; */
    bufsize = size;
    ar_close ();
    (VOID) do_open ();
    DBUG_VOID_RETURN;
}


/*
 *  FUNCTION
 *
 *	ar_estimate    accumulate estimate blocks and compute volume
 *
 *  SYNOPSIS
 *
 *	VOID ar_estimate (size);
 *	LBA size;
 *
 *  DESCRIPTION
 *
 *	Given number of logical blocks to accumulate, adds it to the
 *	current total and computes the new volume number based on
 *	the known media size.  If the media size is unknown then
 *	the volume estimate is forced to one (note that vol_estimate
 *	is the volume index).
 *
 */

VOID ar_estimate (size)
LBA size;
{
    lba_estimate += size;
    if (msize > 0) {
	vol_estimate = (int) (KBYTES (lba_estimate) / B2KB (msize));
    } else {
	vol_estimate = 0;
    }
}


/*
 *  FUNCTION
 *
 *	patch_buffer    perform final operations on buffer
 *
 *  SYNOPSIS
 *
 *	static VOID patch_buffer ()
 *
 *  DESCRIPTION
 *
 *	Because the volume number that a buffer is finally written
 *	to may not correspond to the current volume number at the
 *	time the buffer was filled, the buffer is patched with the
 *	volume number just prior to the write operation.  If the write
 *	generates a fault and the media is switched to a new volume,
 *	the buffer is automatically patched again with the updated
 *	volume number just prior to the write retry.  Since the checksums
 *	cannot be computed until the rest of the data is stable, the
 *	checksum computations are delayed until this time also.
 *
 */
 
static VOID patch_buffer ()
{
    register int index;
    
    DBUG_ENTER ("patch_buffer");
    index = BLOCKS (bufsize);
    while (index--) {
	TOHEX (blocks[index].HD.h_vol, *volume);
	TOHEX (blocks[index].HD.h_chk, chksum (&blocks[index]));
    }
    DBUG_VOID_RETURN;
}

/* like patch_buffer, but gets a pointer to a single union blk */
patch_buffer_blk(blk)
union blk *blk;
{
    register int index;
    
    DBUG_ENTER ("patch_buffer");
    index = BLOCKS (bufsize);
    while (index--) {
	TOHEX (blk->HD.h_vol, *volume);
	TOHEX (blk->HD.h_chk, chksum (blk));
	blk++;
    }
    DBUG_VOID_RETURN;
}

	

/*
 *  FUNCTION
 *
 *	clean_buffer    clear to end of io buffer
 *
 *  SYNOPSIS
 *
 *	static VOID clean_buffer (iobytes)
 *	int iobytes;
 *
 *  DESCRIPTION
 *
 *	Cleans up rest of buffer after a partial read, so that
 *	no junk from a previous read is left hanging around.
 *	This way, other routines don't get fooled by leftover blocks
 *	that have correct checksums and such.
 *
 */
 
static VOID clean_buffer (iobytes)
int iobytes;
{
    register char *zap;
    register int count;
    
    DBUG_ENTER ("clean_buffer");
    count = bufsize - iobytes;
    zap = blocks[0].bytes + iobytes;
#if HAVE_MEMSET
    (void) memset (zap, 0, count);
#else
    while (count--) {
	*zap++ = 0;
    }
#endif
    DBUG_VOID_RETURN;
}


/*
 *  FUNCTION
 *
 *	harderror    report location of hard error
 *
 *  SYNOPSIS
 *
 *	static VOID harderror (errorpba, readflag, iobytes)
 *	LBA errorpba;
 *	BOOLEAN readflag;
 *	int iobytes;
 *
 *  DESCRIPTION
 *
 *	Report location of hard error and adjust soft/hard error
 *	counts appropriately.
 *
 *	The original errno from the original error that triggered
 *	recovery was stored in firsterrno, and is restored prior to
 *	calling the routine to report the error.
 */

static VOID harderror (errorpba, readflag, iobytes)
LBA errorpba;
BOOLEAN readflag;
int iobytes;
{
    DBUG_ENTER ("harderror");
    errno = firsterrno;
    if (iobytes < 0) {
	iobytes = 0;
    }
    if (readflag) {
	bru_error (ERR_ARREAD, errorpba + BLOCKS (iobytes));
	einfo.rsoft -= tries;
	einfo.rhard++;
	clean_buffer (iobytes);
    } else {
	bru_error (ERR_ARWRITE, errorpba + BLOCKS (iobytes));
	einfo.wsoft -= tries;
	einfo.whard++;
    }
    DBUG_VOID_RETURN;
}

static VOID qfwrite ()
{
    register char key;

    DBUG_ENTER ("qfwrite");
    if (!reading && (ardp != NULL) && (ardp -> dv_flags & D_QFWRITE)) {
	bru_error (ERR_QFWRITE, afile.fname);
	key = response (
		"c => continue with next   q => quit    r => reprompt for volume\n\ts => shell",
	    	'r');
	switch (key) {
	    case 'q':
	    case 'Q':
		done ();
		break;
		default:	/* same as 'r', in case they just hit return */
		switch_media (*volume);
		break;
	    case 'c':
	    case 'C':
		break;
	    case 'S':
	    case 's':
		fork_shell ();
		qfwrite ();
		break;
	}
    }
    DBUG_VOID_RETURN;
}


/*
 *  Test for, and eject the current media, if supported.  Under
 *  A/UX, for example, an open for write on a write protected
 *  floppy will fail, so at this point we have no file descriptor
 *  on which to perform the ioctl.  Thus if no open file descriptor
 *  is available, attempt to open one for read, just to get a
 *  handle for the ioctl call.
 *
 *  Note that if the floppy is forced to be nonseekable, by hacking
 *  brutab for example, and double buffering is in effect, then
 *  the ejection fails for some currently unknown reason under A/UX.
 *  This may be some sort of kernel bug, or some bug in the double
 *  buffering scheme.  Mysterious...
 *
 */

static VOID eject_media ()
{
    int fildes;

    DBUG_ENTER ("eject_media");
    if (ardp != NULL && (ardp -> dv_flags & D_EJECT)) {
        if ((fildes = afile.fildes) < 0) {
    	    fildes = s_open (afile.fname, O_RDONLY, 0);
	}
	if (fildes >= 0) {
	    DBUG_PRINT ("eject", ("eject media in '%s'", afile.fname));
	    if (s_eject (fildes) == SYS_ERROR) {
		bru_error (ERR_EJECT, afile.fname);
	    }
	}
    }
    DBUG_VOID_RETURN;
}


#ifdef sgi
#include <sys/mtio.h>
#include <sys/tpsc.h>
#include "rmt.h"
/*	determine if we are trying to write on a QIC24 tape from a tape drive
	(like the VIPER150) that doesn't support it.  Dave Olson, 11/88
*/
sgiqic24(ioerr)
int ioerr;
{
    struct mtget mt_status;

    if(ioerr == EINVAL && 
    	s_ioctl(afile.fildes, MTIOCGET, (char *)&mt_status) == 0 &&
	(mt_status.mt_dsreg & MT_QIC24)) {
	bru_error(ERR_NOQIC24);
	return TRUE;
    }
    return FALSE;
}


/* check to see if an i/o error was due to no media being present */
chknomedia()
{
    struct mtget mt_status;

    if(s_ioctl(afile.fildes, MTIOCGET, (char *)&mt_status) == 0 &&
		!(mt_status.mt_dposn & MT_ONL))
		return TRUE;
    return FALSE;
}


/* check to see if it's a drive, and is in audio mode; if it is, then
 * try to fix it, and also notify them.  Otherwise we can write in
 * audio mode, but they won't be able to get the data back
*/
static void chkandfixaudio(int mt)
{
	static struct mtop mtop = {MTAUD, 0};
	struct mtget mtget;
	unsigned status;

	if(ioctl(mt, MTIOCGET, &mtget))
		return;
	status = mtget.mt_dsreg | ((unsigned)mtget.mt_erreg<<16);
	if(status & CT_AUDIO) {
		bru_error(ERR_AUDIO);
		if(ioctl(mt, MTIOCTOP, &mtop) < 0)
		    bru_error(ERR_FIXAUDIO);
	}
}

#endif sgi
