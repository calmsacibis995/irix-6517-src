#ifndef __SYS_FAT_H__
#define __SYS_FAT_H__

#ident "$Revision: 1.2 $"

/* BIOS parameter block */
typedef struct bpb {
   USHORT BytesPerSector;
   UCHAR   SectorsPerCluster;
   USHORT ReservedSectors;
   UCHAR   FatCount;
   USHORT RootDirEntries;
   USHORT TotalSectors;
   UCHAR   MediaDesc;
   USHORT SectorsPerFat;
   USHORT SectorsPerTrack;
   USHORT HeadCount;
   USHORT HiddenSectors;
   } BPB;

/* offsets within boot sector */
#define BYTES_PER_SECTOR 0x0b
#define SECTOR_PER_CLUSTER 0x0d
#define RESERVED_SECTOR 0x0e
#define FAT_COUNT 0x10
#define ROOTDIR_ENTRIES 0x11
#define TOTAL_SECTORS 0x13
#define MEDIA_DESC 0x15
#define SECTORS_PER_FAT 0x16
#define SECTORS_PER_TRACK 0x18
#define HEAD_COUNT 0x1a
#define HIDDEN_SECTORS 0x1c

/* size of directory entries */
#define DIRENTRYSZ 32

/* support FAT types */
#define DOS12  0x01
#define DOS16  0x04
#define DOS331 0x06

/* file attributes */
#define READONLY  0x00
#define HIDDEN    0x02
#define SYSTEM    0x04
#define VOLUMELABEL 0x08
#define DIRECTORY 0x10
#define ARCHIVE   0x20

/* directory entry offsets */
#define FILENAME  0x00
#define EXTENSION 0x08
#define ATTRIBUTE 0x0b
#define TIMESTAMP 0x16
#define DATESTAMP 0x18
#define STARTCLUSTER 0x1a
#define FILESIZE 0x1c

/* this structures stores the information for open requests */
typedef struct openinfo {
   LONG   Fid;
   USHORT BytesPerSector;
   USHORT SectorsPerCluster;
   USHORT BytesPerCluster;
   USHORT DataOffset;
   CHAR   FatType;
   USHORT FatStart;
   USHORT StartingCluster;
   ULONG  FileLength;
   UCHAR  Attribute;
   } OPENINFO;

#endif
