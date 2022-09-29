/* dosFsLib.h - header for MS-DOS media-compatible file system library */

/* Copyright 1984-1995 Wind River Systems, Inc. */

/*
modification history
--------------------
02b,29mar95,kdl	 changed secPerFat fields to unsigned; added DOS_OPT_LOWERCASE.
02a,12nov94,kdl	 added DOS_OPT_EXPORT, dosFsDevInitOptionsSet(); removed
		 DOS_MAX_SEC_SIZE; added DOS_{LONG_}RESERVED_LEN.
01z,28apr94,jmm  changed uid and gid to dosvd_uid and dosvd_gid
                 added dosvd_mode to DOS_VOL_DESC
01y,18apr94,jmm  added hash table to dosFs to help support inode numbers
                 added support for setting user and group IDs (one per volume)
01x,27feb93,kdl	 added DOS_MAX_NFATS.
01w,08feb93,kdl	 DOS_VOL_DESC and DOS_VOL_CONFIG now use single options field;
		 added prototypes for dosFsMkfsOptionsSet(), 
		 dosFsVolOptionsGet(), dosFsVolOptionsSet().
01v,22sep92,rrr  added support for c++
01u,16sep92,kdl  added S_dosFs_WRITE_ONLY.
01t,04jul92,jcf  cleaned up.
01s,30jun92,kdl  added S_dosFs_INTERNAL_ERROR.
01r,23jun92,kdl  added long filename support.
01q,26may92,rrr  the tree shuffle
01p,05may92,kdl  added pFatModTbl field to volume descriptor.
01o,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed TINY and UTINY to INT8 and UINT8
		  -changed VOID to void
		  -changed copyright notice
01n,10jun91.del  added pragma for gnu960 alignment.
01m,22oct90,kdl  removed declarations of NOMANUAL routines.
01l,19oct90,kdl  changed declaration of dosFsConfigInit() to return STATUS;
		 added declarations of dosFsConfigGet() and dosFsConfigShow();
		 added S_dosFs_INVALID_PARAMETER;
		 deleted S_dosFs_INVALID_DATE and S_dosFs_INVALID_TIME.
01k,05oct90,shl  added ANSI function prototypes.
                 made #endif ANSI style.
                 added copyright notice.
01j,10aug90,dnw  added declaration of dosFsReadyChange().
01i,09aug90,kdl  changed name from msdosLib to dosFsLib, deleted
		 DOS_REQ_DIR_ENT.
01h,07aug90,shl  added IMPORT type to function declarations.
01g,14may90,kdl  added S_msdosLib_BAD_SEEK.
01f,04may90,kdl  moved volume mode from vol descriptor to BLK_DEV (blkIo.h).
01e,03apr90,kdl  moved DOS_FILE_DESC definition to msdosLib.
01d,23mar90,kdl  changed types for lint, added S_msdosLib_NO_BLOCK_DEVICE.
01c,07mar90,kdl  DOS 4.0 compatible, added DOS_DATE_TIME, new driver scheme
01b,20feb90,kdl  misc changes
01a,05oct89,kdl  written


MS-DOS is a registered trademark of Microsoft Corporation.

*/

#ifndef __INCdosFsLibh
#define __INCdosFsLibh

#ifdef __cplusplus
extern "C" {
#endif

#include "ioslib.h"
#include "lstlib.h"
#include "semlib.h"
#include "blkio.h"
#include "vwmodnum.h"
#include "hashlib.h"


#if ((CPU_FAMILY==I960) && (defined __GNUC__))
#pragma align 1			/* tell gcc960 not to optimize alignments */
#endif	/* CPU_FAMILY==I960 */

/* dosFsLib Status Codes */

#define S_dosFsLib_VOLUME_NOT_AVAILABLE		(M_dosFsLib | 1)
#define S_dosFsLib_DISK_FULL			(M_dosFsLib | 2)
#define S_dosFsLib_FILE_NOT_FOUND		(M_dosFsLib | 3)
#define S_dosFsLib_NO_FREE_FILE_DESCRIPTORS	(M_dosFsLib | 4)
#define S_dosFsLib_INVALID_NUMBER_OF_BYTES	(M_dosFsLib | 5)
#define S_dosFsLib_FILE_ALREADY_EXISTS		(M_dosFsLib | 6)
#define S_dosFsLib_ILLEGAL_NAME			(M_dosFsLib | 7)
#define	S_dosFsLib_CANT_DEL_ROOT		(M_dosFsLib | 8)
#define S_dosFsLib_NOT_FILE			(M_dosFsLib | 9)
#define	S_dosFsLib_NOT_DIRECTORY		(M_dosFsLib | 10)
#define	S_dosFsLib_NOT_SAME_VOLUME		(M_dosFsLib | 11)
#define S_dosFsLib_READ_ONLY			(M_dosFsLib | 12)
#define S_dosFsLib_ROOT_DIR_FULL		(M_dosFsLib | 13)
#define S_dosFsLib_DIR_NOT_EMPTY		(M_dosFsLib | 14)
#define S_dosFsLib_BAD_DISK			(M_dosFsLib | 15)
#define S_dosFsLib_NO_LABEL			(M_dosFsLib | 16)
#define S_dosFsLib_INVALID_PARAMETER		(M_dosFsLib | 17)
#define S_dosFsLib_NO_CONTIG_SPACE		(M_dosFsLib | 18)
#define S_dosFsLib_CANT_CHANGE_ROOT		(M_dosFsLib | 19)
#define S_dosFsLib_FD_OBSOLETE			(M_dosFsLib | 20)
#define S_dosFsLib_DELETED			(M_dosFsLib | 21)
#define S_dosFsLib_NO_BLOCK_DEVICE		(M_dosFsLib | 22)
#define S_dosFsLib_BAD_SEEK			(M_dosFsLib | 23)
#define S_dosFsLib_INTERNAL_ERROR		(M_dosFsLib | 24)
#define S_dosFsLib_WRITE_ONLY			(M_dosFsLib | 25)


/* dosFs file system constants */

#define DOS_BOOT_SEC_NUM	0	/* sector number of boot sector */
#define	DOS_MIN_CLUST		2	/* lowest cluster number used */
#define DOS_MAX_CLUSTERS	0x10000	/* max number of clusters per volume */
#define	DOS_SYS_ID_LEN		8	/* length of system ID string */
#define DOS_LONG_NAME_LEN	40	/* length of filename w/ long option */
#define DOS_NAME_LEN		8	/* length of filename (no extension) */
#define	DOS_EXT_LEN		3	/* length of filename extension */
#define	DOS_FILENAME_LEN	(DOS_NAME_LEN + DOS_EXT_LEN + 2)
					/* includes "." and null-terminator */
#define	DOS_VOL_LABEL_LEN	(DOS_NAME_LEN + DOS_EXT_LEN)
					/* length of volume label */

#define DOS_RESERVED_LEN	10	/* reserved bytes in regular dir ent */
#define DOS_LONG_RESERVED_LEN	13	/* reserved bytes in longNames dir ent*/

#define	DOS_DEL_MARK		0xe5	/* file is deleted if name[0] == 0xe5 */
#define	DOS_NAME_E5		0x05	/* value used if name[0] really is e5 */

#define	DOS_MAX_NFATS		64	/* maximum expected number of FATs */
#define DOS_MAX_DIR_LEVELS	20	/* max expected directory levels */
#define	DOS_MAX_PATH_LEN 	(DOS_NAME_LEN+1) * DOS_MAX_DIR_LEVELS
					/* max length of full pathname */


/* File Allocation Table (FAT) values */

#define	DOS_FAT_AVAIL		0x0000	/* cluster is available */
#define	DOS_FAT_BAD		0xff7	/* bad cluster marker for 12-bit FATs */
#define	DOS_FAT16_BAD		0xfff7	/* bad cluster marker for 16-bit FATs */
#define	DOS_FAT_EOF		0xfff	/* end-of-file marker for 12-bit FATs */
#define	DOS_FAT16_EOF		0xffff	/* end-of-file marker for 16-bit FATs */

#define	DOS_FAT_12BIT_MAX	4085	/* max clusters for 12-bit FAT entries*/



/* Boot sector offsets */

/*   Because the MS-DOS boot sector format has word data items
 *   on odd-byte boundaries, it cannot be represented as a standard C
 *   structure.  Instead, the following symbolic offsets are used to
 *   isolate data items.  Non-string data values longer than 1 byte are
 *   in "Intel 8086" order.
 *
 *   These definitions include fields used by MS-DOS Version 4.0.
 */

#define	DOS_BOOT_JMP		0x00	/* 8086 jump instruction     (3 bytes)*/
#define	DOS_BOOT_SYS_ID		0x03	/* system ID string          (8 bytes)*/
#define	DOS_BOOT_BYTES_PER_SEC	0x0b	/* bytes per sector          (2 bytes)*/
#define	DOS_BOOT_SEC_PER_CLUST	0x0d	/* sectors per cluster       (1 byte) */
#define	DOS_BOOT_NRESRVD_SECS	0x0e	/* # of reserved sectors     (2 bytes)*/
#define	DOS_BOOT_NFATS		0x10	/* # of FAT copies           (1 byte) */
#define	DOS_BOOT_MAX_ROOT_ENTS	0x11	/* max # of root dir entries (2 bytes)*/
#define	DOS_BOOT_NSECTORS	0x13	/* total # of sectors on vol (2 bytes)*/
#define	DOS_BOOT_MEDIA_BYTE	0x15	/* media format ID byte      (1 byte) */
#define	DOS_BOOT_SEC_PER_FAT	0x16	/* # of sectors per FAT copy (2 bytes)*/
#define	DOS_BOOT_SEC_PER_TRACK	0x18	/* # of sectors per track    (2 bytes)*/
#define	DOS_BOOT_NHEADS		0x1a	/* # of heads (surfaces)     (2 bytes)*/
#define	DOS_BOOT_NHIDDEN_SECS	0x1c	/* # of hidden sectors       (4 bytes)*/
#define	DOS_BOOT_LONG_NSECTORS	0x20	/* total # of sectors on vol (4 bytes)*/
#define DOS_BOOT_DRIVE_NUM	0x24	/* physical drive number     (1 byte) */
#define DOS_BOOT_SIG_REC	0x26	/* boot signature record     (1 byte) */
#define DOS_BOOT_VOL_ID		0x27	/* binary volume ID number   (4 bytes)*/
#define DOS_BOOT_VOL_LABEL	0x2b	/* volume label string      (11 bytes)*/
#define	DOS_BOOT_PART_TBL	0x1be	/* first disk partition tbl (16 bytes)*/

#define DOS_EXT_BOOT_SIG	0x29	/* value written to boot signature
					 * record to indicate extended (DOS v4)
					 * boot record in use
					 */


/* Disk Partition Table */

/* This structure is used to define areas of a large disk as separate
 * partitions.  An array of such entries is placed in the boot sector, at
 * offset DOS_BOOT_PART_TBL (0x1be).  The first entry in the array describes
 * the currently active partition; the other entries contain static data
 * describing each of the defined disk partitions.
 *
 * Prior to MS-DOS release 4.0, this was the standard method of using
 * a device larger than 32 megabytes.  Since the MS-DOS 4.0 extensions
 * allow direct use of such a device without partitioning, the use of
 * partitions is normally not preferred.  This structure definition is
 * provided to assist you in using partitions, if your specific application
 * requires them.
 */

typedef struct		/* DOS_PART_TBL */
    {
    UINT8		dospt_status;		/* partition status */
    UINT8		dospt_startHead;	/* starting head */
    short		dospt_startSec;		/* starting sector/cylinder */
    UINT8		dospt_type;		/* partition type */
    UINT8		dospt_endHead;		/* ending head */
    short		dospt_endSec;		/* ending sector/cylinder */
    ULONG		dospt_absSec;		/* starting absolute sector */
    ULONG		dospt_nSectors;		/* number of sectors in part */
    }  DOS_PART_TBL;

/* Disk Partition Type values */

#define	DOS_PTYPE_FAT12		1		/* DOS with 12-bit FAT */
#define	DOS_PTYPE_FAT16		4		/* DOS with 16-bit FAT */
#define	DOS_PTYPE_EXTENDED	5		/* extended DOS */



/* Standard MS-DOS directory entry, as found on disk
 *   Non-string data values longer than 1 byte are in "Intel 8086" order,
 */

typedef struct		/* DOS_DISK_DIR_ENT */
    {
    UINT8		dosdde_name [DOS_NAME_LEN]; /* filename */
    UINT8		dosdde_ext [DOS_EXT_LEN];   /* filename extension */
    UINT8		dosdde_attrib;		/* attribute byte */
    char		dosdde_reserved[DOS_RESERVED_LEN];	/* reserved*/
    USHORT		dosdde_time;		/* time */
    USHORT		dosdde_date;		/* date */
    USHORT		dosdde_cluster;		/* starting cluster number */
    ULONG		dosdde_size;		/* file size (in bytes) */
    } DOS_DISK_DIR_ENT;

/* Extended VxWorks-specific directory entry, as found on disk
 *   This format is used when the DOS_OPT_LONGNAMES option is specified.
 *   Non-string data values longer than 1 byte are in "Intel 8086" order.
 */

typedef struct		/* DOS_DISK_EDIR_ENT */
    {
    UINT8		dosdee_name [DOS_LONG_NAME_LEN]; /* filename */
    char		dosdee_reserved[DOS_LONG_RESERVED_LEN];	/* reserved*/
    UINT8		dosdee_attrib;		/* attribute byte */
    USHORT		dosdee_time;		/* time */
    USHORT		dosdee_date;		/* date */
    USHORT		dosdee_cluster;		/* starting cluster number */
    ULONG		dosdee_size;		/* file size (in bytes) */
    } DOS_DISK_EDIR_ENT;


/* HIDDEN */

/* Directory entry, as kept in memory */

typedef struct dosDirEnt	/* DOS_DIR_ENT */
    {
    HASH_NODE           dosde_hashNode;	        /* hash node for dir entry */
    struct dosDirEnt	*dosde_pNext;		/* ptr to next dir entry */
    struct dosDirEnt	*dosde_pParent;		/* ptr to parent dir entry */
    struct dosDirEnt	*dosde_pSubDir;		/* ptr to entry chain, if dir */
    UINT8		dosde_name [DOS_LONG_NAME_LEN]; /* filename */
    UINT8		dosde_ext [DOS_EXT_LEN];   /* filename extension */
    UINT8		dosde_attrib;		/* attribute byte */
    char		dosde_reserved[DOS_LONG_RESERVED_LEN];	/* reserved*/
    USHORT		dosde_time;		/* time */
    USHORT		dosde_date;		/* date */
    USHORT		dosde_cluster;		/* starting cluster number */
    ULONG		dosde_size;		/* file size (in bytes) */
    BOOL		dosde_modified;		/* flag set if dir changed */
    } DOS_DIR_ENT;

/* END_HIDDEN */


/* File attribute byte values */

#define	DOS_ATTR_RDONLY		0x01		/* read-only file */
#define	DOS_ATTR_HIDDEN		0x02		/* hidden file */
#define	DOS_ATTR_SYSTEM		0x04		/* system file */
#define	DOS_ATTR_VOL_LABEL	0x08		/* volume label (not a file) */
#define	DOS_ATTR_DIRECTORY	0x10		/* entry is a sub-directory */
#define	DOS_ATTR_ARCHIVE	0x20		/* file subject to archiving */



/* Volume configuration data */

typedef struct		/* DOS_VOL_CONFIG */
    {
    char	dosvc_mediaByte;	/* media descriptor byte */
    char	dosvc_secPerClust;	/* sectors per cluster (minimum 1) */
    short	dosvc_nResrvd;		/* number of reserved sectors (min 1) */
    char	dosvc_nFats;		/* number of FAT copies (minimum 1) */
    UINT16	dosvc_secPerFat;	/* number of sectors per FAT copy */
    short	dosvc_maxRootEnts;	/* max number of entries in root dir */
    UINT	dosvc_nHidden;		/* number of hidden sectors */
    UINT	dosvc_options;		/* volume options */
    UINT	dosvc_reserved;		/* reserved for future use */
    } DOS_VOL_CONFIG;



/* Volume descriptor */

typedef struct		/* DOS_VOL_DESC */
    {
    DEV_HDR	dosvd_devHdr;		/* i/o system device header */
    short	dosvd_status;		/* (OK | ERROR) */
    char	dosvd_sysId [DOS_SYS_ID_LEN];	/* system ID string */
    SEM_ID	dosvd_semId;		/* volume semaphore id */

    BLK_DEV	*dosvd_pBlkDev;		/* pointer to block device info */

    char	dosvd_mediaByte;	/* media format byte */
    UINT8	dosvd_secPerClust;	/* number of sectors per cluster */
    UINT	dosvd_bytesPerClust;	/* bytes-per-cluster */

    UINT8	dosvd_nFats;		/* number of copies of FAT */
    UINT16	dosvd_secPerFat;	/* number of sectors per FAT */
    int		dosvd_nFatEnts;		/* number of entries in FAT */
    BOOL	dosvd_fat12Bit;		/* FAT uses 12 (vs. 16) bit entries */
    BOOL	dosvd_fatModified;	/* TRUE if FAT has been changed */
    UINT8	*dosvd_pFatBuf;		/* pointer to FAT buffer */

    USHORT	dosvd_maxRootEnts;	/* max num of root directory entries */
    ULONG	dosvd_nRootEnts;	/* current num of root dir entries */
    int		dosvd_rootSec;		/* starting sector number of root dir */
    DOS_DIR_ENT	*dosvd_pRoot;		/* pointer to root directory entry */

    USHORT	dosvd_nResrvdSecs;	/* number of reserved sectors */
    ULONG	dosvd_nHiddenSecs;	/* number of hidden sectors */
    UINT	dosvd_dataSec;		/* starting sector number of data area*/

    UINT8	dosvd_driveNum;		/* physical drive number */
    UINT8	dosvd_bootSigRec;	/* extended boot signature record */
    ULONG	dosvd_volId;		/* binary volume ID number */
    char	dosvd_volLabel [DOS_VOL_LABEL_LEN];
					/* volume label string */

    int		dosvd_state;		/* state of volume (see below) */
    int		dosvd_retry;		/* retry count of disk operations */
    UINT	dosvd_options;		/* volume options (see below) */

    UINT8	*dosvd_pFatModTbl;	/* ptr to table of mod'd FAT sectors */
    HASH_ID	dosvd_hashTbl;	        /* hash table for volume */
    int         dosvd_uid;	        /* user id to report from stat () */
    int         dosvd_gid;              /* group id to report from stat () */
    int         dosvd_mode;	        /* file mode to report from stat () */
    } DOS_VOL_DESC;


/* Volume states */

#define DOS_VD_READY_CHANGED	0	/* vol not accessed since ready change*/
#define DOS_VD_RESET		1	/* volume reset but not mounted */
#define DOS_VD_MOUNTED		2	/* volume mounted */
#define DOS_VD_CANT_RESET	3	/* volume reset failed */
#define DOS_VD_CANT_MOUNT	4	/* volume mount failed */

/* Volume options */

#define DOS_OPT_CHANGENOWARN	0x1	/* disk may be changed w/out warning */
#define DOS_OPT_AUTOSYNC	0x2	/* autoSync mode enabled */
#define DOS_OPT_LONGNAMES	0x4	/* long file names enabled */
#define DOS_OPT_EXPORT		0x8	/* file sytem export enabled */
#define DOS_OPT_LOWERCASE	0x40	/* filenames use lower-case chars */


/* dosFs Date and Time Structure */

typedef struct		/* DOS_DATE_TIME */
    {
    int		dosdt_year;		/* current year */
    int		dosdt_month;		/* current month */
    int		dosdt_day;		/* current day */
    int		dosdt_hour;		/* current hour */
    int		dosdt_minute;		/* current minute */
    int		dosdt_second;		/* current second */
    } DOS_DATE_TIME;

/* GLOBALS */

extern int     dosFsUserId;	/* user id to use for next volume created */
extern int     dosFsGroupId;    /* group id to use for next volume created */

/* Function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern STATUS 		dosFsConfigGet (DOS_VOL_DESC *vdptr,
					DOS_VOL_CONFIG *pConfig);
extern STATUS 		dosFsConfigInit (DOS_VOL_CONFIG *pConfig,
					 char mediaByte, char secPerClust,
					 short nResrvd, char nFats,
					 UINT16 secPerFat, short maxRootEnts,
					 UINT nHidden, UINT options);
extern STATUS 		dosFsConfigShow (char *devName);
extern STATUS 		dosFsDateSet (int year, int month, int day);
extern void 		dosFsDateTimeInstall (FUNCPTR pDateTimeFunc);
extern DOS_VOL_DESC *	dosFsDevInit (char *devName, BLK_DEV *pBlkDev,
		                      DOS_VOL_CONFIG *pConfig);
extern STATUS		dosFsDevInitOptionsSet (UINT options);
extern STATUS 		dosFsInit (int maxFiles);
extern DOS_VOL_DESC *	dosFsMkfs (char *volName, BLK_DEV *pBlkDev);
extern STATUS		dosFsMkfsOptionsSet (UINT options);
extern void 		dosFsModeChange (DOS_VOL_DESC *vdptr, int newMode);
extern void 		dosFsReadyChange (DOS_VOL_DESC *vdptr);
extern STATUS 		dosFsTimeSet (int hour, int minute, int second);
extern STATUS 		dosFsVolOptionsGet (DOS_VOL_DESC *vdptr, 
					    UINT *pOptions);
extern STATUS 		dosFsVolOptionsSet (DOS_VOL_DESC *vdptr, UINT options);
extern STATUS 		dosFsVolUnmount (DOS_VOL_DESC *vdptr);

#else	/* __STDC__ */

extern STATUS 	dosFsConfigGet ();
extern STATUS 	dosFsConfigInit ();
extern STATUS 	dosFsConfigShow ();
extern STATUS 	dosFsDateSet ();
extern void 	dosFsDateTimeInstall ();
extern DOS_VOL_DESC *	dosFsDevInit ();
extern STATUS	dosFsDevInitOptionsSet ();
extern STATUS 	dosFsInit ();
extern DOS_VOL_DESC *	dosFsMkfs ();
extern STATUS	dosFsMkfsOptionsSet ();
extern void 	dosFsModeChange ();
extern void 	dosFsReadyChange ();
extern STATUS 	dosFsTimeSet ();
extern STATUS 	dosFsVolOptionsGet ();
extern STATUS 	dosFsVolOptionsSet ();
extern STATUS 	dosFsVolUnmount ();

#endif	/* __STDC__ */

#if ((CPU_FAMILY==I960) && (defined __GNUC__))
#pragma align 0			/* turn off alignment requirement */
#endif	/* CPU_FAMILY==I960 */

#ifdef __cplusplus
}
#endif

#endif /* __INCdosFsLibh */
