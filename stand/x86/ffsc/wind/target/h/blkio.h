/* blkIo.h - block I/O header file */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01i,22sep92,rrr  added support for c++
01h,04jul92,jcf  cleaned up.
01g,26may92,rrr  the tree shuffle
01f,04oct91,rrr  passed through the ansification filter
		  -changed READ, WRITE and UPDATE to O_RDONLY O_WRONLY O_RDWR
		  -changed copyright notice
01e,05oct90,shl  added copyright notice.
                 made #endif ANSI style.
01d,12jul90,kdl  added bd_statusChk routine field in BLK_DEV.
01c,04may90,kdl  added bd_mode and bd_readyChanged in BLK_DEV.
01b,23mar90,kdl  changed types for lint, changed BLK_DEV field names.
01a,15mar90,kdl  written
*/

#ifndef __INCblkIoh
#define __INCblkIoh

#ifdef __cplusplus
extern "C" {
#endif

#include "vxworks.h"

typedef struct		/* BLK_DEV */
    {
    FUNCPTR		bd_blkRd;		/* function to read blocks */
    FUNCPTR		bd_blkWrt;		/* function to write blocks */
    FUNCPTR		bd_ioctl;		/* function to ioctl device */
    FUNCPTR		bd_reset;		/* function to reset device */
    FUNCPTR		bd_statusChk;		/* function to check status */
    BOOL		bd_removable;		/* removable medium flag */
    ULONG		bd_nBlocks;		/* number of blocks on device */
    ULONG		bd_bytesPerBlk;		/* number of bytes per block */
    ULONG		bd_blksPerTrack;	/* number of blocks per track */
    ULONG		bd_nHeads;		/* number of heads */
    int			bd_retry;		/* retry count for I/O errors */
    int			bd_mode;		/* O_RDONLY |O_WRONLY| O_RDWR */
    BOOL		bd_readyChanged;	/* dev ready status changed */
    } BLK_DEV;

#ifdef __cplusplus
}
#endif

#endif /* __INCblkIoh */
