/*
 * ARCS EISA file system structures and definitions
 */

#ifndef _ARCS_FS_H
#define _ARCS_FS_H

#ident "$Revision: 1.10 $"

#include    <arcs/folder.h>

/* FSBlock function codes */

/* These codes must be specified by the EISA addendum
 */
typedef enum {
    FC_SEEK = 0x200,
    FC_GETDIRENTRY,
    FC_MKDIR,
    FC_RMDIR,
    FC_DELETE,
    FC_RENAME,
    FC_SETATTRIBUTE,
    FC_GETATTRIBUTE,
    FC_IDENTIFY,
    FC_GETFREESPACE 
} FSFUNCTIONCODE;

/* These codes are really used by the file systems
 */
typedef enum fs_ops {
    FS_CHECK,
    FS_OPEN,
    FS_CLOSE,
    FS_READ,
    FS_WRITE, 
    FS_GETDIRENT,
    FS_GETREADSTATUS,
    FS_GETFILEINFO
} FSOPS;

typedef struct fsblock {
    LONG	FunctionCode;	/* code for filesystem function */
    COMPONENT 	*Device;	/* same addr as node in cfgtree */
    STATUS	(*DeviceStrategy)(COMPONENT *, IOBLOCK *);	/* driver entrypoint for device */
    IOBLOCK	*IO;		/* I/O block for device driver */
    CHAR	*Filename;	/* name of opened file */
    CHAR	*Buffer;	/* user src/dest buffer for transfer */
    LONG	Count;		/* number of bytes requested */
    VOID 	*FsPtr;		/* filesys dependent info */
} FSBLOCK, *PFSBLOCK;

/* The FS_Entry structure for the FS Table */

#define	FS_NAME_MAX_LEN	16
typedef struct fs_entry {
    STATUS	    (*FSStrategy)(FSBLOCK *);
    LONG	    Type;	/* use for DTFS_x for now */
    CHAR	    Identifier[FS_NAME_MAX_LEN + 1];	/* +1 for the NULL */
    struct fs_entry *Next;
} FS_ENTRY;

/*
 * File system types
 */
#define	DTFS_NONE	0x00	/* no file structure on device */
#define	DTFS_FAT	0x01	/* FAT fs */
#define	DTFS_DVH	0x02	/* disk volume header */
#define	DTFS_TPD	0x04	/* boot tape directory */
#define	DTFS_EFS	0x08	/* SGI EFS file system */
#define	DTFS_BOOTP	0x10	/* bootp protocol */
#define DTFS_TAR	0x20	/* [posix] tar file */
#define DTFS_RIPL	0x40	/* IBM remote IPL */
#define DTFS_XFS	0x80	/* SGI XFS file system */
#define	DTFS_ANY	-1	/* first fs that claims it */

#endif
