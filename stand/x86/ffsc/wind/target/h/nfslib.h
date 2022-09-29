/* nfsLib.h - Network File System library header */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01p,28jul93,jmm  moved AUTH_UNIX_FIELD_LEN and MAX_GRPS definitions to here
01o,22sep92,rrr  added support for c++
01n,18sep92,jcf  removed ansi warnings.
01m,07sep92,smb  added includes xdr_nfs.h and sys/stat.h to remove ANSI warnings
01l,04jul92,jcf  cleaned up.
01k,26may92,rrr  the tree shuffle
01j,16dec91,gae  added missing prototypes for ANSI.
01i,04oct91,rrr  passed through the ansification filter
		  -changed VOID to void
		  -changed copyright notice
01h,08mar91,elh  added NFS_REXMIT_{SEC,USEC}.
01g,25oct90,dnw  deleted private function.
01f,05oct90,shl  added ANSI function prototypes.
                 made #endif ANSI style.
                 added copyright notice.
01e,10aug90,dnw  added declarations of nfsAuthUnix[GS]et().
01d,09sep88,llk  added NFS_TIMEOUT_SEC, NFS_TIMEOUT_USEC, NFS_SOCKOPTVAL.
01c,26aug88,gae  removed unused imports.
01b,04jun88,llk  moved FOLLOW_LINK, DEFAULT_FILE_PERM and DEFAULT_DIR_PERM
		   to ioLib.h.
01a,19apr88,llk  written.
*/

#ifndef __INCnfsLibh
#define __INCnfsLibh

#ifdef __cplusplus
extern "C" {
#endif

#include "vwmodnum.h"
#include "dirent.h"
#include "sys/stat.h"
#include "xdr_nfsserv.h"
#include "xdr_nfs.h"
    
/* file status types -- from the nfs protocol spec, if these change,
   then the FSTAT values in ioLib.h should also be changed */

#define NFS_FSTAT_DIR		0040000		/* directory */
#define NFS_FSTAT_CHR		0020000		/* character special file */
#define NFS_FSTAT_BLK		0060000		/* block special file */
#define NFS_FSTAT_REG		0100000		/* regular file */
#define NFS_FSTAT_LNK		0120000		/* symbolic link file */
#define NFS_FSTAT_NON		0140000		/* named socket */

/* nfsLib status codes */

#define	S_nfsLib_NFS_AUTH_UNIX_FAILED		(M_nfsLib | 1)
#define	S_nfsLib_NFS_INAPPLICABLE_FILE_TYPE	(M_nfsLib | 2)

/* nfsLib status codes derived from Sun's NFS specification (see xdr_nfs.h)  */

#define S_nfsLib_NFS_OK			(M_nfsStat | (int) NFS_OK)
#define S_nfsLib_NFSERR_PERM		(M_nfsStat | (int) NFSERR_PERM)
#define S_nfsLib_NFSERR_NOENT		(M_nfsStat | (int) NFSERR_NOENT)
#define S_nfsLib_NFSERR_IO		(M_nfsStat | (int) NFSERR_IO)
#define S_nfsLib_NFSERR_NXIO		(M_nfsStat | (int) NFSERR_NXIO)
#define S_nfsLib_NFSERR_ACCES		(M_nfsStat | (int) NFSERR_ACCES)
#define S_nfsLib_NFSERR_EXIST		(M_nfsStat | (int) NFSERR_EXIST)
#define S_nfsLib_NFSERR_NODEV		(M_nfsStat | (int) NFSERR_NODEV)
#define S_nfsLib_NFSERR_NOTDIR		(M_nfsStat | (int) NFSERR_NOTDIR)
#define S_nfsLib_NFSERR_ISDIR		(M_nfsStat | (int) NFSERR_ISDIR)
#define S_nfsLib_NFSERR_FBIG		(M_nfsStat | (int) NFSERR_FBIG)
#define S_nfsLib_NFSERR_NOSPC		(M_nfsStat | (int) NFSERR_NOSPC)
#define S_nfsLib_NFSERR_ROFS		(M_nfsStat | (int) NFSERR_ROFS)
#define S_nfsLib_NFSERR_NAMETOOLONG	(M_nfsStat | (int) NFSERR_NAMETOOLONG)
#define S_nfsLib_NFSERR_NOTEMPTY	(M_nfsStat | (int) NFSERR_NOTEMPTY)
#define S_nfsLib_NFSERR_DQUOT		(M_nfsStat | (int) NFSERR_DQUOT)
#define S_nfsLib_NFSERR_STALE		(M_nfsStat | (int) NFSERR_STALE)
#define S_nfsLib_NFSERR_WFLUSH		(M_nfsStat | (int) NFSERR_WFLUSH)


/* default NFS parameters */

#define NFS_TIMEOUT_SEC         25
#define NFS_TIMEOUT_USEC        0
#define NFS_SOCKOPTVAL          10000
#define NFS_REXMIT_SEC		5
#define NFS_REXMIT_USEC		0

#define AUTH_UNIX_FIELD_LEN	50	/* UNIX authentication info */
#define MAX_GRPS		20	/* max. # of groups that user is in */

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern STATUS 	nfsExportShow (char *hostName);
extern void 	nfsAuthUnixGet (char *machname, int *pUid, int *pGid,
			        int *pNgids, int *gids);
extern void 	nfsAuthUnixSet (char *machname, int uid, int gid,
			        int ngids, int *aup_gids);
extern void 	nfsAuthUnixPrompt (void);
extern void 	nfsAuthUnixShow (void);
extern void 	nfsHelp (void);
extern void 	nfsIdSet (int uid);

extern void 	nfsClientClose (void);
extern STATUS 	nfsDirMount (char *hostName, dirpath dirname,
			     nfs_fh *pFileHandle);
extern STATUS 	nfsDirUnmount (char *hostName, dirpath dirname);
extern STATUS 	nfsDirReadOne (char *hostName, nfs_fh *pDirHandle, DIR *pDir);
extern STATUS 	nfsExportFree (exports *pExports);
extern STATUS 	nfsExportRead (char *hostName, exports *pExports);
extern void 	nfsFileAttrGet (fattr *pFattr, struct stat *pStat);
extern int 	nfsFileRead (char *hostName, nfs_fh *pHandle,
			     unsigned int offset, unsigned int count,
			     char *buf, fattr *pNewAttr);
extern STATUS 	nfsFileRemove (char *hostName, nfs_fh *pMountHandle,
			       char *fullFileName);
extern int 	nfsFileWrite (char *hostName, nfs_fh *pHandle,
			      unsigned int offset, unsigned int count,
			      char *data, fattr *pNewAttr);
extern int 	nfsLookUpByName (char *hostName, char *fileName,
			         nfs_fh *pMountHandle, diropres *pDirOpRes,
			         nfs_fh *pDirHandle);
extern int 	nfsThingCreate (char *hostName, char *fullFileName,
			        nfs_fh *pMountHandle, diropres *pDirOpRes,
			        nfs_fh *pDirHandle, u_int mode);

#else

extern STATUS 	nfsExportShow ();
extern void 	nfsAuthUnixGet ();
extern void 	nfsAuthUnixSet ();
extern void 	nfsAuthUnixPrompt ();
extern void 	nfsAuthUnixShow ();
extern void 	nfsHelp ();
extern void 	nfsIdSet ();

extern void 	nfsClientClose ();
extern STATUS 	nfsDirMount ();
extern STATUS 	nfsDirUnmount ();
extern STATUS 	nfsDirReadOne ();
extern STATUS 	nfsExportFree ();
extern STATUS 	nfsExportRead ();
extern void 	nfsFileAttrGet ();
extern int 	nfsFileRead ();
extern STATUS 	nfsFileRemove ();
extern int 	nfsFileWrite ();
extern int 	nfsLookUpByName ();
extern int 	nfsThingCreate ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCnfsLibh */
