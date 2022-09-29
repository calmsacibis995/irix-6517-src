/* rt11FsLib.h - header for RT-11 media-compatible file system library */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
02b,22sep92,rrr  added support for c++
02a,04jul92,jcf  cleaned up.
02h,26may92,rrr  the tree shuffle
02g,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed READ, WRITE and UPDATE to O_RDONLY O_WRONLY O_RDWR
		  -changed VOID to void
		  -changed copyright notice
02f,10jun91.del  added pragma for gnu960 alignment.
02e,08oct90,kdl  removed declarations of functions now made LOCAL.
02d,05oct90,shl  added ANSI function prototypes.
                 added copyright notice.
02c,10aug90,dnw  added declaration of rt11FsReadyChange().
02b,09aug90,kdl  changed name from rt11Lib to rt11FsLib.
02a,07aug90,shl  added IMPORT type to function declarations.
01z,04may90,kdl  moved volume mode from vol descriptor to BLK_DEV (blkIo.h),
		 added vd_changeNoWarn.
01y,23mar90,kdl	 added S_rt11Lib_NO_BLOCK_DEVICE.
01x,07mar90,kdl	 changed to support multi-filesystem device drivers.
01w,05jun88,dnw  changed rtLib to rt11Lib.
01v,30may88,dnw  changed to v4 names.
01u,04may88,jcf  changed SEMAPHORE to SEM_ID.
01t,25may88,dnw  deleted erroneous ")" in S_rtLib_ENTRY_NUMBER_TOO_BIG def'n.
01s,19aug87,dnw  changed rfd_start to be int instead of short to support
		   disks larger than 32k blocks.
01r,21mar87,gae  spelling, S_rtLib_ENTRY_NUMBER_TO_BIG.
01q,04feb87,llk  made changes which allow a one segment directory to have any
		   number of file entries.  Segments are now dynamically
		   allocated (vd_dir_seg).  Added vd_nSegBlocks field to
		   volume descriptor.  It tells how many blocks are in the
		   volume descriptor's segment.
01p,14jan87,llk  added RT_MAX_FILE_SIZE.  Made de_nblocks unsigned.
01o,24dec86,gae  changed stsLib.h to vwModNum.h.
01n,16oct86,gae	 added vd_mode to RT_VOL_DESC.
01m,07apr86,dnw  replaced vd_ready flag with vd_state in RT_VOL_DESC.
01l,11mar86,dnw  added vd_retry to RT_VOL_DESC.
01k,04sep85,jlf  added vd_ready and vd_reset to RT_VOL_DESC.
01j,14aug85,dnw  added S_rtLib_INVALID_DEVICE_PARAMETERS.
01i,28may85,jlf  changed RT_VOL_DESC to contain disk format info, and sec
		 read and write routines rather than block.
01h,13aug84,ecs  changed status codes: NO_ROOM_ON_DISK to DISK_FULL,
		   NO_SUCH_FILE to FILE_NOT_FOUND,
		   NAME_IN_USE to FILE_ALREADY_EXISTS.
01g,08aug84,ecs  added status codes and include of stsLib.h.
01f,01aug84,dnw  removed single density disk definitions, SD_..., to fd208Drv.c.
01e,06jul84,ecs  removed rfd_entry_num from RT_FILE_DESC.
		 added vd_sem to RT_VOL_DESC.
		 added include of semLib.h.
		 changed rfd_modified from TBOOL to BOOL.
01d,11jun84,dnw  removed vd_volume and added vd_rdblk, vd_wrtblk, and vd_nblocks
		   to RT_VOL_DESC.
		 added definitions of SD_RT_... for single density specific
		   values.
01c,27jan84,ecs  added inclusion test.
01b,14aug83,dnw  replaced RT_FILE_DESC.rfd_vdnum volume descriptor number with
		   .rfd_vdptr volume descriptor pointer.
		 replaced RT_FILE_DESC.rfd_maxblock end block number with
		   .rfd_endptr end byte pointer.
01a,14Feb83,dnw  written
*/

#ifndef __INCrt11FsLibh
#define __INCrt11FsLibh

#ifdef __cplusplus
extern "C" {
#endif

#include "ioslib.h"
#include "semlib.h"
#include "blkio.h"
#include "vwmodnum.h"


#if ((CPU_FAMILY==I960) && (defined __GNUC__))
#pragma align 1			/* tell gcc960 not to optimize alignments */
#endif	/* CPU_FAMILY==I960 */
/* rt11FsLib status codes */

#define S_rt11FsLib_VOLUME_NOT_AVAILABLE		(M_rt11FsLib | 1)
#define S_rt11FsLib_DISK_FULL				(M_rt11FsLib | 2)
#define S_rt11FsLib_FILE_NOT_FOUND			(M_rt11FsLib | 3)
#define S_rt11FsLib_NO_FREE_FILE_DESCRIPTORS		(M_rt11FsLib | 4)
#define S_rt11FsLib_INVALID_NUMBER_OF_BYTES		(M_rt11FsLib | 5)
#define S_rt11FsLib_FILE_ALREADY_EXISTS			(M_rt11FsLib | 6)
#define S_rt11FsLib_BEYOND_FILE_LIMIT			(M_rt11FsLib | 7)
#define S_rt11FsLib_INVALID_DEVICE_PARAMETERS		(M_rt11FsLib | 8)
#define S_rt11FsLib_NO_MORE_FILES_ALLOWED_ON_DISK	(M_rt11FsLib | 9)
#define	S_rt11FsLib_ENTRY_NUMBER_TOO_BIG		(M_rt11FsLib | 10)
#define	S_rt11FsLib_NO_BLOCK_DEVICE			(M_rt11FsLib | 11)

/* rt11Fs file system constants */

#define RT_DIR_BLOCK		6
#define RT_BYTES_PER_BLOCK	512
#define RT_FILES_FOR_2_BLOCK_SEG 72
#define RT_MAX_BLOCKS_PER_FILE	0xffff	/* maximum number of blocks per file,
					   largest size described by an unsigned
					   short (de_nblocks field of
					   RT_DIR_ENTRY) */


/* directory entry status values */

#define DES_TENTATIVE	0x0100		/* tentative file */
#define DES_EMPTY	0x0200		/* empty space */
#define DES_PERMANENT	0x0400		/* permanent file */
#define DES_END		0x0800		/* end of directory segment marker */

#define DES_BOGUS	0x0000		/* not a real rt11Fs directory entry */


/* miscellaneous */

#define NOT_IN_USE	-1		/* descriptor not-in-use flag,
					 *   in fd.rfd_status, vd.rfd_status */

/* rt11Fs radix-50 name structure */

typedef struct		/* RT_NAME */
    {
    unsigned short nm_name1;	/* filename chars 1-3 in radix-50 */
    unsigned short nm_name2;	/* filename chars 4-6 in radix-50 */
    unsigned short nm_type;	/* file type chars 1-3 in radix-50 */
    } RT_NAME;


/* directory entry */

typedef struct		/* RT_DIR_ENTRY */
    {
    short de_status;		/* file status */
    RT_NAME de_name;		/* filename in radix-50 structure */
    unsigned short de_nblocks;	/* number of blocks in file */
    char  de_jobnum;		/* (temp file only) job with file open */
    char  de_channel;		/* (temp file only) channel with file is open */
    short de_date;		/* file creation date:
				 *	bits 14-10 = month (1-12) in decimal
				 *      bits  9- 5 = day (1-31) in decimal
				 *      bits  4- 0 = year minus 110 in octal
				 */
    } RT_DIR_ENTRY;


/* directory segment */

typedef struct		/* RT_DIR_SEG */
    {
    short ds_nsegs;		/* number of segments in directory */
    short ds_next_seg;		/* number of next segment (0 = end of list) */
    short ds_last_seg;		/* number of highest segment used */
    short ds_extra;		/* number of extra bytes on each dir entry */
    short ds_start;		/* number of first data block for this seg */
    RT_DIR_ENTRY ds_entries [1]; /* directory entries,
				    actual size gets dynamically allocated */
    } RT_DIR_SEG;


/* volume descriptor */

typedef struct		/* RT_VOL_DESC */
    {
    DEV_HDR vd_devhdr;		/* i/o system device header (MUST COME FIRST!)*/
    short vd_status;		/* (OK | ERROR) */
    SEM_ID vd_semId;		/* volume descriptor semaphore id */
    BLK_DEV *vd_pBlkDev;	/* pointer to block device info */
    int vd_nblocks;		/* number of blocks in volume */
    BOOL vd_rtFmt;		/* TRUE if using RT-11 skew and track offset */
    int vd_secBlock;		/* Number of sectors per block */
    int vd_state;		/* state of volume (see below) */
    int vd_retry;		/* retry count of disk operations */
    int vd_nSegBlocks;		/* number of blocks in the directory segment,
				   there are always at least 2 */
    RT_DIR_SEG *vd_dir_seg;	/* pointer to current directory segment */
    BOOL vd_changeNoWarn;	/* TRUE if disk changed without readyChange */
    } RT_VOL_DESC;

/* volume states */

#define RT_VD_READY_CHANGED	0	/* vol not accessed since ready change*/
#define RT_VD_RESET		1	/* volume reset but not mounted */
#define RT_VD_MOUNTED		2	/* volume mounted */
#define RT_VD_CANT_RESET	3	/* volume reset failed */
#define RT_VD_CANT_MOUNT	4	/* volume mount failed */


/* rt11Fs file descriptor */

typedef struct		/* RT_FILE_DESC */
    {
    short rfd_status;		/* (OK | NOT_IN_USE) */
    RT_VOL_DESC *rfd_vdptr;	/* pointer to rt11Fs volume descriptor */
    int rfd_start;		/* number of first block in file */
    short rfd_mode;		/* access mode: O_RDONLY, O_WRONLY, O_RDWR */
    RT_DIR_ENTRY rfd_dir_entry;	/* directory entry for this file */
    int rfd_curptr;		/* file byte ptr of current buffer byte 0 */
    int rfd_newptr;		/* file byte ptr for new read/writes */
    int rfd_endptr;		/* file byte ptr to end of file */
    BOOL rfd_modified;		/* TRUE = buffer has been modified */
    char rfd_buffer [RT_BYTES_PER_BLOCK];
				/* file I/O buffer */
    } RT_FILE_DESC;


/* Function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern RT_VOL_DESC *rt11FsDevInit (char *devName, BLK_DEV *pBlkDev,
				   BOOL rt11Fmt, int nEntries,
			 	   BOOL changeNoWarn);
extern RT_VOL_DESC *rt11FsMkfs (char *volName, BLK_DEV *pBlkDev);
extern STATUS 	rt11FsInit (int maxFiles);
extern void 	rt11FsDateSet (int year, int month, int day);
extern void 	rt11FsModeChange (RT_VOL_DESC *vdptr, int newMode);
extern void 	rt11FsReadyChange (RT_VOL_DESC *vdptr);

#else	/* __STDC__ */

extern RT_VOL_DESC *rt11FsDevInit ();
extern RT_VOL_DESC *rt11FsMkfs ();
extern STATUS 	rt11FsInit ();
extern void 	rt11FsDateSet ();
extern void 	rt11FsModeChange ();
extern void 	rt11FsReadyChange ();

#endif	/* __STDC__ */

#if ((CPU_FAMILY==I960) && (defined __GNUC__))
#pragma align 0			/* turn off alignment requirement */
#endif	/* CPU_FAMILY==I960 */

#ifdef __cplusplus
}
#endif

#endif /* __INCrt11FsLibh */
