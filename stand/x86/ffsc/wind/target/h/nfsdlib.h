/* nfsdLib.h - Network File System Server library header */

/* Copyright 1994 Wind River Systems, Inc. */

/*
modification history
--------------------
01c,21apr94,jmm  more cleanup of prototypes; added NFSD_ARGUMENT
01b,20apr94,jmm  added new prototypes
01a,31mar94,jmm  written.
*/

#ifndef __INCnfsdLibh
#define __INCnfsdLibh

#ifdef __cplusplus
extern "C" {
#endif

#include "limits.h"
#include "xdr_nfs.h"

/* VxWorks file handle information */

typedef struct 
    {
    int    volumeId;            /* Volume identifier for the file system */
    ULONG  fsId;                /* Inode of the exported directory */
    ULONG  inode;		/* Inode of the file as returned from stat */
    INT8   reserved [24];       /* NFS file handles have 32 bytes */
    } NFS_FILE_HANDLE;

/* nfsdStatusGet() information */

typedef struct
    {
    int nullCalls;
    int getattrCalls;
    int setattrCalls;
    int rootCalls;
    int lookupCalls;
    int readlinkCalls;
    int readCalls;
    int writecacheCalls;
    int writeCalls;
    int createCalls;
    int removeCalls;
    int renameCalls;
    int linkCalls;
    int symlinkCalls;
    int mkdirCalls;
    int rmdirCalls;
    int readdirCalls;
    int statfsCalls;
    } NFS_SERVER_STATUS;

typedef union
    {
    nfs_fh nfsproc_getattr_2_arg;
    sattrargs nfsproc_setattr_2_arg;
    diropargs nfsproc_lookup_2_arg;
    nfs_fh nfsproc_readlink_2_arg;
    readargs nfsproc_read_2_arg;
    writeargs nfsproc_write_2_arg;
    createargs nfsproc_create_2_arg;
    diropargs nfsproc_remove_2_arg;
    renameargs nfsproc_rename_2_arg;
    linkargs nfsproc_link_2_arg;
    symlinkargs nfsproc_symlink_2_arg;
    createargs nfsproc_mkdir_2_arg;
    diropargs nfsproc_rmdir_2_arg;
    readdirargs nfsproc_readdir_2_arg;
    nfs_fh nfsproc_statfs_2_arg;
    } NFSD_ARGUMENT;

#if defined(__STDC__) || defined(__cplusplus)

extern STATUS nfsdInit (int nServers, int nExportedFs, int priority,
			FUNCPTR authHook, FUNCPTR mountAuthHook, int options);
extern void nfsdRequestProcess (void);
extern void nfsd (void);
extern STATUS nfsdStatusGet (NFS_SERVER_STATUS * serverStats);
extern STATUS nfsdStatusShow (int options);
extern STATUS nfsdFhCreate (NFS_FILE_HANDLE * parentDir, char * fileName,
			    NFS_FILE_HANDLE * fh);
extern STATUS nfsdFhToName (NFS_FILE_HANDLE * fh, char * fileName);
extern STATUS nfsdFattrGet (NFS_FILE_HANDLE * fh,
			    struct fattr * fileAttributes);
extern nfsstat nfsdErrToNfs (int theErr);
extern void nfsdFhHton (NFS_FILE_HANDLE * fh);
extern void nfsdFhNtoh (NFS_FILE_HANDLE * fh);

#else 

extern STATUS nfsdInit ();
extern void nfsdRequestProcess ();
extern void nfsd ();
extern STATUS nfsdStatusGet ();
extern STATUS nfsdStatusShow ();
extern STATUS nfsdFhCreate ();
extern STATUS nfsdFhToName ();
extern STATUS nfsdFattrGet ();
extern nfsstat nfsdErrToNfs ();
extern void nfsdFhHton ();
extern void nfsdFhNtoh ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCnfsdLibh */
