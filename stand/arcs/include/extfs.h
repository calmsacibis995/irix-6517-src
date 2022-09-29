#ifndef __EXTFS_H__
#define __EXTFS_H__

#ident $Revision: 1.2 $

#define FS_CREATE   0x04
#define FS_EXCL	    0x08
#define FS_WRONLY   0x10
#define FS_RDWR	    0x20

typedef struct FileAttribute {
    UCHAR FileAtrribute;
    LONG FileLength;
    USHORT Time;
    USHORT Date;
} FILEATTRIBUTE;

typedef struct SpaceInfo {
    LONG BytesPerBlock;
    LONG TotalBlocks;
    LONG AvailableBlocks;
} SPACEINFO;

#endif
