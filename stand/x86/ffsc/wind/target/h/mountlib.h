/* mountLib.h - Mount protocol library header */

/* Copyright 1994 Wind River Systems, Inc. */

/*
modification history
--------------------
01c,25apr94,jmm  added S_mountLib_ILLEGAL_MODE; changed mode to readOnly 
01b,21apr94,jmm  added MOUNTD_ARGUMENT; prototype cleanup
01a,31mar94,jmm  written.
*/

#include "xdr_nfsserv.h"

#ifndef __INCmountLibh
#define __INCmountLibh

#ifdef __cplusplus
extern "C" {
#endif

/* mountLib status codes */

#define S_mountLib_ILLEGAL_MODE             (M_mountLib | 1)

/* mountLib defaults */

#define MAX_EXPORTED_FILESYSTEMS 30
#define MOUNTD_PRIORITY_DEFAULT  55
#define MOUNTD_STACKSIZE_DEFAULT 10240

/* Exported file systems */

typedef struct
    {
    char * next;		/* next and prev are used by lstLib */
    char * prev;
    char   dirName [PATH_MAX];
    int    dirFd;		/* File descriptor for the exported directory */
    ULONG  fsId;
    int    volumeId;            /* Identifier for the file system */
    BOOL   readOnly;
    } NFS_EXPORT_ENTRY;

typedef union
    {
    dirpath mountproc_mnt_1_arg;
    dirpath mountproc_umnt_1_arg;
    } MOUNTD_ARGUMENT;

#if defined(__STDC__) || defined(__cplusplus)

extern STATUS mountdInit (int priority, int stackSize, FUNCPTR authHook,
			  int nExports, int options);
extern void mountd (void);
extern STATUS nfsExport (char * directory, int id, int mode, int options);
extern STATUS nfsUnexport (char * dirName);
extern NFS_EXPORT_ENTRY * nfsExportFindByName (char * dirName);
extern NFS_EXPORT_ENTRY * nfsExportFindById (int volumeId);
extern int fdToInode (int fd);
extern int nameToInode (char * fileName);

#else 

extern STATUS mountdInit ();
extern void mountd ();
extern STATUS nfsExport ();
extern STATUS nfsUnexport ();
extern NFS_EXPORT_ENTRY * nfsExportFindByName ();
extern NFS_EXPORT_ENTRY * nfsExportFindById ();
extern int fdToInode ();
extern int nameToInode ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCmountLibh */
