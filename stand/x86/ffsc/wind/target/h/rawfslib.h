/* rawFsLib.h - header for raw block device file system library */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
02b,22sep92,rrr  added support for c++
02a,04jul92,jcf  cleaned up.
01d,26may92,rrr  the tree shuffle
01c,04oct91,rrr  passed through the ansification filter
		  -changed VOID to void
		  -changed copyright notice
01b,05oct90,shl  added ANSI function prototypes.
                 added copyright notice.
01a,02oct90,kdl  written
*/

#ifndef __INCrawFsLibh
#define __INCrawFsLibh

#ifdef __cplusplus
extern "C" {
#endif

#include "blkio.h"
#include "ioslib.h"
#include "lstlib.h"
#include "semlib.h"
#include "vwmodnum.h"


/* rawFsLib Status Codes */

#define S_rawFsLib_VOLUME_NOT_AVAILABLE		(M_rawFsLib | 1)
#define S_rawFsLib_END_OF_DEVICE		(M_rawFsLib | 2)
#define S_rawFsLib_NO_FREE_FILE_DESCRIPTORS	(M_rawFsLib | 3)
#define S_rawFsLib_INVALID_NUMBER_OF_BYTES	(M_rawFsLib | 4)
#define S_rawFsLib_ILLEGAL_NAME			(M_rawFsLib | 5)
#define S_rawFsLib_NOT_FILE			(M_rawFsLib | 6)
#define S_rawFsLib_READ_ONLY			(M_rawFsLib | 7)
#define S_rawFsLib_FD_OBSOLETE			(M_rawFsLib | 8)
#define S_rawFsLib_NO_BLOCK_DEVICE		(M_rawFsLib | 9)
#define S_rawFsLib_BAD_SEEK			(M_rawFsLib | 10)


/* Volume descriptor */

typedef struct		/* RAW_VOL_DESC */
    {
    DEV_HDR	rawvd_devHdr;		/* std. I/O system device header */
    int		rawvd_status;		/* (OK | ERROR) */
    SEM_ID	rawvd_semId;		/* volume descriptor semaphore id */
    BLK_DEV	*rawvd_pBlkDev;		/* ptr to block device info */
    int		rawvd_state;		/* state of volume (see below) */
    int		rawvd_retry;		/* current retry count for I/O errors */
    } RAW_VOL_DESC;


/* Volume states */

#define RAW_VD_READY_CHANGED	0	/* vol not accessed since ready change*/
#define RAW_VD_RESET		1	/* volume reset but not mounted */
#define RAW_VD_MOUNTED		2	/* volume mounted */
#define RAW_VD_CANT_RESET	3	/* volume reset failed */
#define RAW_VD_CANT_MOUNT	4	/* volume mount failed */


/* Function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern RAW_VOL_DESC *rawFsDevInit (char *volName, BLK_DEV *pBlkDev);
extern STATUS 	rawFsInit (int maxFiles);
extern STATUS 	rawFsVolUnmount (RAW_VOL_DESC *vdptr);
extern void 	rawFsModeChange (RAW_VOL_DESC *vdptr, int newMode);
extern void 	rawFsReadyChange (RAW_VOL_DESC *vdptr);

#else	/* __STDC__ */

extern RAW_VOL_DESC *	rawFsDevInit ();
extern STATUS 	rawFsInit ();
extern STATUS 	rawFsVolUnmount ();
extern void 	rawFsModeChange ();
extern void 	rawFsReadyChange ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCrawFsLibh */
