#ident	"include/sys/file.h:  $Revision: 1.4 $"

/*
 * file.h - defines for file syscalls
 */

/*
 * flags- also for fcntl call.
 */
#define	FOPEN		(-1)
#define	FREAD		00001		/* descriptor read/receive'able */
#define	FWRITE		00002		/* descriptor write/send'able */
#define	FNDELAY		00004		/* no delay */
#define	FAPPEND		00010		/* append on each write */
#define	FMARK		00020		/* mark during gc() */
#define	FDEFER		00040		/* defer for next gc pass */
#define	FASYNC		00100		/* signal pgrp when data ready */
#define	FSHLOCK		00200		/* shared lock present */
#define	FEXLOCK		00400		/* exclusive lock present */

/* bits to save after open */
#define	FMASK		00113
#define	FCNTLCANT	(FREAD|FWRITE|FMARK|FDEFER|FSHLOCK|FEXLOCK)

/* open only modes */
#define	FCREAT		01000		/* create if nonexistant */
#define	FTRUNC		02000		/* truncate to zero length */
#define	FEXCL		04000		/* error if already created */

/*
 * User definitions.
 */

/*
 * Open call (standalone doesn't support all POSIX modes).
 */
#define	O_RDONLY	OpenReadOnly	/* open for reading */
#define	O_WRONLY	OpenWriteOnly	/* open for writing */
#define	O_RDWR		OpenReadWrite	/* open for read & write */

/*
 * Flock call.
 */
#define	LOCK_SH		1	/* shared lock */
#define	LOCK_EX		2	/* exclusive lock */
#define	LOCK_NB		4	/* don't block when locking */
#define	LOCK_UN		8	/* unlock */

/*
 * Access call.
 */
#define	F_OK		0	/* does file exist */
#define	X_OK		1	/* is it executable by caller */
#define	W_OK		2	/* writable by caller */
#define	R_OK		4	/* readable by caller */

/*
 * Lseek call.
 */
#define	L_SET		0	/* absolute offset */
#define	L_INCR		1	/* relative to current offset */
#define	L_XTND		2	/* relative to end of file */
