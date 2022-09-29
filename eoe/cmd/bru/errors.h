/************************************************************************
 *									*
 *			Copyright (c) 1984, Fred Fish			*
 *			     All Rights Reserved			*
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
 *	errors.h    error code definitions
 *
 *  SCCS
 *
 *	@(#)errors.h	9.11	5/11/88
 *
 *  SYNOPSIS
 *
 *	#include "errors.h"
 *
 *  DESCRIPTION
 *
 *	Define error numbers.  Each error is assigned a name of
 *	the form "ERR_XXX..", where XXX... is some mnemonically
 *	meaningful string for the specific error.
 *
 *	Each time an error condition (or warning) is encountered,
 *	the error handling routine "bru_error" is called to
 *	issue an appropriate message.  The first argument to
 *	bru_error is the error number.
 *
 */


#define ERR_MODE	1	/* Confusion over mode */
#define ERR_AROPEN	2	/* Error opening archive */
#define ERR_ARCLOSE	3	/* Error closing archive */
#define ERR_ARREAD	4	/* Error reading archive */
#define ERR_ARWRITE	5	/* Error writing archive */
#define ERR_ARSEEK	6	/* Error seeking on archive */
#define ERR_BUFSZ	7	/* Media smaller than I/O buffer */
#define ERR_FORMAT	8	/* Archive media unformatted */
#define ERR_BALLOC	9	/* Can't allocate block buffers */
#define ERR_BSEQ	10	/* Block sequence error */
#define ERR_DSYNC	11	/* File header error; resync */
#define ERR_EACCESS	12	/* File does not exist */
#define ERR_STAT	13	/* Can't stat file */
#define ERR_BIGPATH	14	/* Pathname too big */
#define ERR_BIGFAC	15	/* Blocking factor too big */
#define ERR_OPEN	16	/* Can't open file */
#define ERR_CLOSE	17	/* Error closing file */
#define ERR_READ	18	/* Error reading from file */
#define ERR_FTRUNC	19	/* File was truncated */
#define ERR_FGREW	20	/* File grew while archiving */
#define ERR_SUID	21	/* Not owner, can't set user id */
#define ERR_SGID	22	/* Not in group, can't set group id */
#define ERR_EXEC	23	/* Can't exec a file */
#define ERR_FORK	24	/* Can't fork */
#define ERR_BADWAIT	25	/* Unrecognized wait return */
#define ERR_EINTR	26	/* Interrupted system call */
#define ERR_CSTOP	27	/* Child process stopped */
#define ERR_CTERM	28	/* Child process terminated */
#define ERR_CORE	29	/* Child process dumped core */
#define ERR_WSTATUS	30	/* Inconsistent wait status */
#define ERR_AVAIL0	31	/* available for re-use */
#define ERR_AVAIL1	32	/* available for re-use */
#define ERR_SUM		33	/* Archive checksum error */
#define ERR_BUG		34	/* Internal bug detected */
#define ERR_MALLOC	35	/* Error allocating space */
#define ERR_WALK	36	/* Internal consistency error in tree walk */
#define ERR_DEPTH	37	/* Pathname too big in recursive save */
#define ERR_SEEK	38	/* Error seeking on file */
#define ERR_ISUM	39	/* Checksum error in info block */
#define ERR_WRITE	40	/* Error writing to file */
#define ERR_SMODE	41	/* Error setting file mode */
#define ERR_CHOWN	42	/* Error setting uid/gid */
#define ERR_STIMES	43	/* Error setting times */
#define ERR_MKNOD	44	/* Error making a non-regular file */
#define ERR_MKLINK	45	/* Error making link */
#define ERR_ARPASS	46	/* Inconsistent phys block addresses */
#define ERR_IMAGIC	47	/* Bad info block magic number */
#define ERR_LALLOC	48	/* Lost linkage for file */
#define ERR_URLINKS	49	/* Unresolved links */
#define ERR_TTYOPEN	50	/* Can't open tty */
#define ERR_NTIME	51	/* Error converting time */
#define ERR_UNAME	52	/* Error getting unix name */
#define ERR_LABEL	53	/* Archive label string too big */
#define ERR_GUID	54	/* Error converting uid for -o option */
#define ERR_CCLASS	55	/* Botched character class pattern */
#define ERR_OVRWRT	56	/* Can't overwrite file */
#define ERR_WACCESS	57	/* Can't access file for write */
#define ERR_RACCESS	58	/* Can't access file for read */
#define ERR_ARTIME	59	/* Volume has different creation time */
#define ERR_ARVOL	60	/* Volume number not what expected */
#define ERR_STDIN	61	/* Illegal use of standard input */
#define ERR_EOV		62	/* Premature end of volume of known size */
#define ERR_WPROT	63	/* Media appears to be write protected */
#define ERR_FIRST	64	/* General first read/write error */
#define ERR_BRUTAB	65	/* Can't find device table file */
#define ERR_SUPERSEDE	66	/* File not superseded */
#define ERR_IEOV	67	/* Inferred end of volume */
#define ERR_IGNORED	68	/* File not in archive or ignored */
#define ERR_FASTMODE	69	/* May need to use -F option on read */
#define ERR_BACKGND	70	/* Abort if any interaction required */
#define ERR_MKDIR	71	/* Mkdir system call failed */
#define ERR_RDLINK	72	/* Readlink system call failed */
#define ERR_NOSYMLINKS	73	/* System does not support symbolic links */
#define ERR_MKSYMLINK	74	/* Could not make the symbolic link */
#define ERR_MKFIFO	75	/* Could not make a fifo */
#define ERR_SYMTODIR	76	/* Hard link to a directory if no symlinks */
#define ERR_HARDLINK	77	/* Target for a hard link does not exist */
#define ERR_FIFOTOREG	78	/* Extracted a fifo as a regular file */
#define ERR_ALINKS	79	/* Additional links added while running */
#define ERR_OBTF	80	/* Obsolete brutab entry */
#define ERR_DEFDEV	81	/* No default device in bru device table */
#define ERR_NOBTF	82	/* No support for obsolete brutab format */
#define ERR_BSIZE	83	/* Attempt to change buffer size on nth vol */
#define ERR_QFWRITE	84	/* Query on first write to new volume */
#define	ERR_DBLSUP	85	/* Error setting up double buffering */
#define ERR_EJECT	86	/* Error attempting to eject media */
#define ERR_NOSHRINK	87	/* Compressed version was larger, saved uncompressed */
#define ERR_ZXFAIL	88	/* Extraction of compressed file failed */
#define ERR_NOEZ	89	/* Estimate mode ignores compression */
#define ERR_UNLINK	90	/* Failed to unlink a file */
#define ERR_ZFAILED	91	/* Compression failed for some nonspecific reason */
#define ERR_NOQIC24	92	/* Can't write QIC24 tapes on QIC150 drives */

#define ERR_CWD		93	/* Can't get the current working directory */
#define ERR_IOERR	94	/* Input/Output Error */
#define ERR_ARGS	95	/* bad argstring */
#define ERR_NOTNUM	96	/* string wasn't a valid numeric value */
#define ERR_NOMEDIA 97	/* no media in the drive */
#define ERR_CREAT	98	/* Can't create file */
#define ERR_MKLINK_J	99	/* Error making link */
#define ERR_PAGESZ	100	/* Error getting system page size */
#define ERR_TOO_BIG	101	/* File too big - Added for xfs */
#define ERR_DIFF_BIG	102	/* File size indeterminate - Added for xfs */
#define ERR_NO_KFLAG	103	/* File >2Gb and -K not set - Added for xfs */
#define ERR_NO_ZFLAG	104	/* File >2Gb and -Z not set - Added for xfs */
#define ERR_OK_FLAG	105	/* File >2Gb and -K and -Z set - Added for xfs */
#define ERR_AUDIO	106	/* dat drive in audio mode */
#define ERR_FIXAUDIO	107	/* couldn't take out of audio mode */
