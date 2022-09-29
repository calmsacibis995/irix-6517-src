/* ioLib.h - I/O interface library header */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
05g,29aug95,hdn  added S_ioLib_UNFORMATED for floppy disk driver.
05f,29dec93,jmm  added FIOTIMESET for utime() support
                 added FIOINODETONAME to translate inodes to names
		 added FIOFSTATFSGET to get information for fstatfs()
05e,02oct92,srh  added FIOGETFL ioctl code
05d,22sep92,rrr  added support for c++
05c,18sep92,smb  moved open and creat prototypes to fcntl.h.
05b,16sep92,kdl  added include of unistd.h; added L_* definitions for compat.
05a,02aug92,jcf  added creat().
04z,29jul92,smb  added include for fcntl.h. removed fopen options.
04y,22jul92,kdl	 added include of stdio.h if using delete() macro.
04x,22jul92,kdl	 removed references to delete(); added conditional macro
		 definition for delete() to use remove() instead.
04w,04jul92,jcf  cleaned up.
04v,30jun92,kdl	 added FIONCONTIG and FIOTRUNC ioctl codes; added CONTIG_MAX.
04u,26may92,rrr  the tree shuffle
04t,05dec91,rrr  moved O_ and L_ macros to their posix header files
04s,26nov91,llk  fixed delete() prototype.
04r,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed READ, WRITE and UPDATE to O_RDONLY O_WRONLY and
		   O_RDWR
		  -changed VOID to void
		  -changed copyright notice
04q,10jun91.del  added pragma for gnu960 alignment.
04p,23oct90,shl  changed ioctl()'s last parameter type from "int" to "void *".
04o,20oct90,dnw  removed "iosLib.h" and declaration that required it.
04n,19oct90,shl  added #include "ioslib.h".
04m,05oct90,dnw  deleted private routines.
		 added chdir(), getcwd(), remove(), rename()
04l,05oct90,shl  added ANSI function prototypes.
                 made #endif ANSI style.
                 added copyright notice.
04k,10aug90,dnw  added declaration of ioDefDirGet().
04j,11jul90,jcc  added FIOSCSICOMMAND.
04i,14jun90,kdl  added FIOUNMOUNT.
04h,25may90,dnw  added include of limits.h and defined MAX_FILENAME_LENGTH
		   in terms of PATH_MAX
04g,20apr90,kdl  added FIOREADDIR, FIOFSTATGET.
04f,01apr90,llk  added S_ioLib_NAME_TOO_LONG.
		 decreased MAX_DIRNAMES.
04e,17mar90,rdc  added FIOSELECT and FIOUNSELECT.
	    dab  changed O_CREAT and O_TRUNC values (from 0x80 and 0x100 to
		 001000 and 002000).  fixed spelling of S_ioLib_CANCELLED.
	    kdl  added MS-DOS functions: FIONFREE, FIOMKDIR, FIORMDIR,
		 FIOLABELGET, FIOLABELSET, FIOATTRIBSET, FIOCONTIG.
04d,27jul89,hjb  added ty protocol hook stuff: FIOPROTOHOOK,FIOPROTOARG
		   FIORBUFSET,FIOWBUFSET,FIORFLUSH,FIOWFLUSH.
04c,21apr89,dab  added FIOSYNC for sync to disk.
04b,18nov88,dnw  removed NOT_GENERIC stuff.
04a,22jun88,dnw  moved READ, WRITE, and UPDATE back to vxWorks.h.
03z,04jun88,llk  added MAX_DIRNAMES and S_ioLib_NO_DEVICE_NAME_IN_PATH for path
		   parsing.
		 added FSTAT for file types (inspired by nfs).
		 added DEFAULT_FILE_PERM and DEFAULT_DIR_PERM for newly created
		   files and directories.
03y,30apr88,gae  got read/write modes right.
03x,29mar88,gae  added FIOISATTY.  Added UNIX style open() and lseek() flags.
		 moved READ, WRITE, and UPDATE here from vxWorks.h.
03w,31dec87,jlf  added FOLLOW_LINK.
03v,01oct87,gae  added FIOGETNAME for all file descriptors.
		 added FIO[SG]ETOPTIONS for line options on devices.
03u,09sep87,dnw  added FIONBIO for sockets.
		 added FIONMSGS for pipes.
03t,09sep87,dnw  added FIOSQUEEZE and HD_1, HD_2 for rt-11 disks.
03s,09jun87,ecs  added S_ioLib_CANCELED & FIOCANCEL.
03r,14feb87,dnw  added S_ioLib_NO_FILENAME.
03q,04feb87,llk  added FIODISKCHANGE.
03p,24dec86,gae  changed stsLib.h to vwModNum.h.
03o,01dec86,dnw  increased MAX_FILENAME_LENGTH from 32 to 100.
03n,20nov86,dnw	 added S_ioLib_DISK_NOT_PRESENT.
03m,17oct86,gae	 added S_ioLib_WRITE_PROTECTED.
03l,21may86,llk	 corrected comments.
03k,23mar86,jlf  changed GENERIC to NOT_GENERIC
03j,08mar86,dnw  added S_ioLib_DEVICE_TIMEOUT.
*/

#ifndef __INCioLibh
#define __INCioLibh

#ifdef __cplusplus
extern "C" {
#endif

#include "vwmodnum.h"
#include "limits.h"
#include "net/uio.h"
#include "fcntl.h"
#include "unistd.h"

#if ((CPU_FAMILY==I960) && (defined __GNUC__))
#pragma align 1			/* tell gcc960 not to optimize alignments */
#endif	/* CPU_FAMILY==I960 */

#define MAX_DIRNAMES		32		/* max directory names in path*/
#define MAX_FILENAME_LENGTH	(PATH_MAX + 1)	/* max chars in filename
						 * including EOS*/

/* I/O status codes */

#define S_ioLib_NO_DRIVER		(M_ioLib | 1)
#define S_ioLib_UNKNOWN_REQUEST		(M_ioLib | 2)
#define S_ioLib_DEVICE_ERROR		(M_ioLib | 3)
#define S_ioLib_DEVICE_TIMEOUT		(M_ioLib | 4)
#define S_ioLib_WRITE_PROTECTED		(M_ioLib | 5)
#define S_ioLib_DISK_NOT_PRESENT	(M_ioLib | 6)
#define S_ioLib_NO_FILENAME		(M_ioLib | 7)
#define S_ioLib_CANCELLED		(M_ioLib | 8)
#define S_ioLib_NO_DEVICE_NAME_IN_PATH	(M_ioLib | 9)
#define S_ioLib_NAME_TOO_LONG		(M_ioLib | 10)
#define S_ioLib_UNFORMATED		(M_ioLib | 11)


/* ioctl function codes */

#define FIONREAD	1		/* get num chars available to read */
#define FIOFLUSH	2		/* flush any chars in buffers */
#define FIOOPTIONS	3		/* set options (FIOSETOPTIONS) */
#define FIOBAUDRATE	4		/* set serial baud rate */
#define FIODISKFORMAT	5		/* format disk */
#define FIODISKINIT	6		/* initialize disk directory */
#define FIOSEEK		7		/* set current file char position */
#define FIOWHERE	8		/* get current file char position */
#define FIODIRENTRY	9		/* return a directory entry (obsolete)*/
#define FIORENAME	10		/* rename a directory entry */
#define FIOREADYCHANGE	11		/* return TRUE if there has been a
					   media change on the device */
#define FIONWRITE	12		/* get num chars still to be written */
#define FIODISKCHANGE	13		/* set a media change on the device */
#define FIOCANCEL	14		/* cancel read or write on the device */
#define FIOSQUEEZE	15		/* squeeze out empty holes in rt-11
					 * file system */
#define FIONBIO		16		/* set non-blocking I/O; SOCKETS ONLY!*/
#define FIONMSGS	17		/* return num msgs in pipe */
#define FIOGETNAME	18		/* return file name in arg */
#define FIOGETOPTIONS	19		/* get options */
#define FIOSETOPTIONS	FIOOPTIONS	/* set options */
#define FIOISATTY	20		/* is a tty */
#define FIOSYNC		21		/* sync to disk */
#define FIOPROTOHOOK	22		/* specify protocol hook routine */
#define FIOPROTOARG	23		/* specify protocol argument */
#define FIORBUFSET	24		/* alter the size of read buffer  */
#define FIOWBUFSET	25		/* alter the size of write buffer */
#define FIORFLUSH	26		/* flush any chars in read buffers */
#define FIOWFLUSH	27		/* flush any chars in write buffers */
#define FIOSELECT	28		/* wake up process in select on I/O */
#define FIOUNSELECT	29		/* wake up process in select on I/O */
#define FIONFREE        30              /* get free byte count on device */
#define FIOMKDIR        31              /* create a directory */
#define FIORMDIR        32              /* remove a directory */
#define FIOLABELGET     33              /* get volume label */
#define FIOLABELSET     34              /* set volume label */
#define FIOATTRIBSET    35              /* set file attribute */
#define FIOCONTIG       36              /* allocate contiguous space */
#define FIOREADDIR      37              /* read a directory entry (POSIX) */
#define FIOFSTATGET     38              /* get file status info */
#define FIOUNMOUNT      39              /* unmount disk volume */
#define FIOSCSICOMMAND  40              /* issue a SCSI command */
#define FIONCONTIG      41              /* get size of max contig area on dev */
#define FIOTRUNC        42              /* truncate file to specified length */
#define FIOGETFL        43		/* get file mode, like fcntl(F_GETFL) */
#define FIOTIMESET      44		/* change times on a file for utime() */
#define FIOINODETONAME  45		/* given inode number, return filename*/
#define FIOFSTATFSGET   46              /* get file system status info */


/* ioctl option values */

#define OPT_ECHO	0x01		/* echo input */
#define OPT_CRMOD	0x02		/* lf -> crlf */
#define OPT_TANDEM	0x04		/* ^S/^Q flow control protocol */
#define OPT_7_BIT	0x08		/* strip parity bit from 8 bit input */
#define OPT_MON_TRAP	0x10		/* enable trap to monitor */
#define OPT_ABORT	0x20		/* enable shell restart */
#define OPT_LINE	0x40		/* enable basic line protocol */
#define OPT_RTS	0x80		/* enable hardware flow control on read buffer */

#define OPT_RAW		0		/* raw mode */

#define OPT_TERMINAL	(OPT_ECHO | OPT_CRMOD | OPT_TANDEM | \
			 OPT_MON_TRAP | OPT_7_BIT | OPT_ABORT | OPT_LINE)

#define CONTIG_MAX	-1		/* "count" for FIOCONTIG if requesting
					 *  maximum contiguous space on dev
					 */

/* miscellaneous */

#define FOLLOW_LINK	-2			/* value for driver to return */

#define DEFAULT_FILE_PERM	0000640		/* default file permissions
						   unix style rw-r----- */
#define DEFAULT_DIR_PERM	0000750		/* default directory permissions
						   unix style rwxr-x--- */

/* file types -- NOTE:  these values are specified in the NFS protocol spec */

#define FSTAT_DIR		0040000		/* directory */
#define FSTAT_CHR		0020000		/* character special file */
#define FSTAT_BLK		0060000		/* block special file */
#define FSTAT_REG		0100000		/* regular file */
#define FSTAT_LNK		0120000		/* symbolic link file */
#define FSTAT_NON		0140000		/* named socket */


/* ioctl augmented arguments */

/* the following is obsolete and only used now by rt-11 */

typedef struct		/* REQ_DIR_ENTRY */
    {
    int entryNum;		/* number of directory entry */
    char name[MAX_FILENAME_LENGTH];	/* name of file */
    int nChars;			/* number of chars in file */
    short day;			/* creation day of month */
    short month;		/* creation month of year */
    short year;			/* creation year of era */
    } REQ_DIR_ENTRY;

/* INODE_TO_NAME_IOCTL is used to call FIOINODETONAME */
						 
typedef struct
    {
    ULONG  inode;		/* file inode */
    char * fileName;		/* pointer to string to hold file name */
    } INODE_TO_NAME_IOCTL;
					 
/* disk formats */

#define SS_1D_8		1	/* single side, single density, 8"     */
#define SS_2D_8		2	/* single side, double density, 8"     */
#define DS_1D_8		3	/* double side, single density, 8"     */
#define DS_2D_8		4	/* double side, double density, 8"     */
#define SS_1D_5		5	/* single side, single density, 5 1/4" */
#define SS_2D_5		6	/* single side, double density, 5 1/4" */
#define DS_1D_5		7	/* double side, single density, 5 1/4" */
#define DS_2D_5		8	/* double side, double density, 5 1/4" */

#define HD_1		129	/* hard disk - type 1 */
#define HD_2		130	/* hard disk - type 2 */

/* globals */

extern int ioMaxLinkLevels;	/* max number of symbolic links to traverse */
						 
/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

#if 0
extern STATUS 	ioFullFileNameGet (char *pathName, DEV_HDR **ppDevHdr,
				   char *fullFileName);
#else
extern STATUS 	ioFullFileNameGet ();
#endif
extern STATUS 	ioDefPathCat (char *name);
extern STATUS 	ioDefPathSet (char *name);
extern char *	getwd (char *pathname);
extern int 	ioGlobalStdGet (int stdFd);
extern int 	ioTaskStdGet (int taskId, int stdFd);
extern int 	ioctl (int fd, int function, int arg);
extern int      readv (int fd, struct iovec *iov, int iovcnt);
extern void 	ioDefDevGet (char *devName);
extern void 	ioDefDirGet (char *dirName);
extern void 	ioDefPathGet (char *pathname);
extern void 	ioGlobalStdSet (int stdFd, int newFd);
extern void 	ioTaskStdSet (int taskId, int stdFd, int newFd);
extern int      writev (int fd, struct iovec *iov, int iovcnt);

#else	/* __STDC__ */

extern STATUS 	ioDefPathCat ();
extern STATUS 	ioDefPathSet ();
extern STATUS 	ioFullFileNameGet ();
extern char *	getwd ();
extern int 	ioGlobalStdGet ();
extern int 	ioTaskStdGet ();
extern int 	ioctl ();
extern int	readv ();
extern void 	ioDefDevGet ();
extern void 	ioDefDirGet ();
extern void 	ioDefPathGet ();
extern void 	ioGlobalStdSet ();
extern void 	ioTaskStdSet ();
extern int	writev ();

#endif	/* __STDC__ */


/* Conditional definition of delete() for compatibility with earlier VxWorks */

#define __DELETE_FUNC  FALSE 		/* change to TRUE to use delete() */

#if  __DELETE_FUNC
#include "stdio.h"
#define delete(filename)  remove(filename)
#endif  /* __DELETE_FUNC */


/* Miscellaneous obsolete definitions, for backward compatibility */

#define	L_SET	SEEK_SET		/* see unistd.h */
#define L_INCR	SEEK_CUR
#define L_XTND	SEEK_END



#if ((CPU_FAMILY==I960) && (defined __GNUC__))
#pragma align 0			/* turn off alignment requirement */
#endif	/* CPU_FAMILY==I960 */

#ifdef __cplusplus
}
#endif

#endif /* __INCioLibh */
