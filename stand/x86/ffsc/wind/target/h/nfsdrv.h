/* nfsDrv.h - nfsDrv header */

/* Copyright 1984-1995 Wind River Systems, Inc. */

/*
modification history
--------------------
01m,28mar95,kdl  added includes of limits.h and hostLib.h (SPR 4157)..
01l,01dec93,jag  added NFS_DEV_INFO, nfsDevListGet(), and nfsDevInfoGet().
01k,22sep92,rrr  added support for c++
01j,04jul92,jcf  cleaned up.
01i,26may92,rrr  the tree shuffle
01h,04oct91,rrr  passed through the ansification filter
		  -changed VOID to void
		  -changed copyright notice
01g,05oct90,dnw  changed nfsMountAll() arg from verbose to quiet.
01f,05oct90,shl  added ANSI function prototypes.
                 made #endif ANSI style.
                 added copyright notice.
01e,07aug90,shl  added IMPORT type to function declarations.
01d,01may90,llk  added function declarations.
01c,30sep88,llk  removed S_nfsDrv_UNKNOWN_HOST.
01b,30jun88,llk  added S_nfsDrv_NO_HOST_NAME_SPECIFIED.
01a,19apr88,llk  written.
*/

#ifndef __INCnfsDrvh
#define __INCnfsDrvh

#ifdef __cplusplus
extern "C" {
#endif

#include "vwmodnum.h"
#include "limits.h"
#include "hostlib.h"


typedef struct
    {

    char hostName   [MAXHOSTNAMELEN];	/* Host of imported NFS     */
    char remFileSys [PATH_MAX + 1];	/* Remote File System Name  */
    char locFileSys [PATH_MAX + 1];	/* Local File System Name   */

    } NFS_DEV_INFO;

/* nfsDrv status codes */

#define S_nfsDrv_INVALID_NUMBER_OF_BYTES	(M_nfsDrv | 1)
#define S_nfsDrv_BAD_FLAG_MODE			(M_nfsDrv | 2)
#define S_nfsDrv_CREATE_NO_FILE_NAME		(M_nfsDrv | 3)
#define	S_nfsDrv_FATAL_ERR_FLUSH_INVALID_CACHE	(M_nfsDrv | 4)
#define	S_nfsDrv_WRITE_ONLY_CANNOT_READ		(M_nfsDrv | 5)
#define	S_nfsDrv_READ_ONLY_CANNOT_WRITE		(M_nfsDrv | 6)
#define	S_nfsDrv_NOT_AN_NFS_DEVICE		(M_nfsDrv | 7)
#define	S_nfsDrv_NO_HOST_NAME_SPECIFIED		(M_nfsDrv | 8)

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern STATUS 	nfsDrv (void);
extern STATUS 	nfsMount (char *host, char *fileSystem, char *localName);
extern STATUS 	nfsMountAll (char *host, char *clientName, BOOL quiet);
extern void 	nfsDevShow (void);
extern STATUS 	nfsUnmount (char *localName);
extern int      nfsDevListGet (unsigned long nfsDevList [], int listSize);
extern STATUS   nfsDevInfoGet (unsigned long nfsDevHandle, 
			       NFS_DEV_INFO  * pnfsInfo);
#else

extern STATUS 	nfsDrv ();
extern STATUS 	nfsMount ();
extern STATUS 	nfsMountAll ();
extern void 	nfsDevShow ();
extern STATUS 	nfsUnmount ();
extern int      nfsDevListGet ();
extern STATUS   nfsDevInfoGet ();

#endif	/* __STDC__ */


#ifdef __cplusplus
}
#endif

#endif /* __INCnfsDrvh */
