/*
 *=============================================================================
 *                      File:           dos.h.h
 *=============================================================================
 */
#ifndef _DOS_H_
#define _DOS_H_

#include	<sys/bsd_types.h>
#include	"misc.h"

#define	MAGIC_LEN		(3)
#define	OEM_LEN	 		(8)
#define	FILE_LEN		(8)
#define	EXTN_LEN		(3)
#define MAX_DOS_VOL_NAME       	(11)

/* DOS filesystem boot parameters */
typedef struct bp {
	u_char	bp_magic[3];		/* magic # */
	u_char	bp_oem[8];		/* OEM identification */
	u_short	bp_sectorsize;		/* sector size in bytes */
	u_char	bp_sectorspercluster;	/* cluster size in sectors */
	u_short	bp_reservecount;	/* # of reserved sectors */
	u_char	bp_fatcount;		/* # of FAT */
	u_short	bp_rootentries;		/* # of entries in root */
	u_short	bp_sectorcount;		/* # of sectors on disk */
	u_char	bp_mediatype;		/* disk type */
	u_short	bp_fatsize;		/* FAT size in sectors */
	u_short	bp_sectorspertrack;	/* sectors per track */
	u_short	bp_headcount;		/* # of heads */
	u_int	bp_hiddensectors;	/* # of hidden sectors */
	u_int	bp_numsectors;		/* # sectors on disk */
} bp_t;


/* DOS disk file structure */
typedef struct dfile {
	char	df_name[8];		/* file name */
	char	df_ext[3];		/* file name extension */
	u_char	df_attribute;		/* file attribute */
#define	ATTRIB_RDONLY	0x01
#define	ATTRIB_HIDDEN	0x02
#define	ATTRIB_SYSTEM	0x04
#define	ATTRIB_LABEL	0x08
#define	ATTRIB_DIR	0x10
#define	ATTRIB_ARCHIVE	0x20
#define	ATTRIB_RSRV1	0x40
#define	ATTRIB_RSRV2	0x80
	u_char	df_reserved[10];	/* not used */
	u_char	df_time[2];		/* time file last modified */
	u_char	df_date[2];		/* date file last modified */
	u_char	df_start[2];		/* first FAT entry used by file data */
	u_char	df_size[4];		/* file size in bytes */
} dfile_t;

/* in-core DOS disk file structure */
typedef struct file {
    struct dfile   f_dfile;     /* see above */
    u_short        f_cluster;   /* location on diskette where this */
    u_short        f_offset;    /* directory entry can be found */
} file_t;


typedef  u_char        BITMAPUNIT;
#define  BITMAPUNITSZ  (sizeof(BITMAPUNIT)*8)

 
/* DOS filesystem structure.  One per exported floppy filesystem.  */
typedef struct dfs {
    int            dfs_fd;             /* device file descriptor   */
    u_char	   *dfs_boot;	       /* in-core copy of BOOT     */	
    u_char         *dfs_fat;           /* in-core copy of FAT      */
    u_char         *dfs_root;          /* in-core copy of ROOT     */
    u_long	   dfs_partaddr;       /* starting address of part */
    u_long         dfs_fataddr1;       /* starting address of FAT1 */
    u_long         dfs_fataddr2;       /* starting address of FAT2 */
    u_long         dfs_fatsize;        /* FAT size in sectors      */
    u_long         dfs_rootaddr;       /* starting address of root */
    u_long         dfs_rootsize;       /* root size in sectors     */
    u_long         dfs_fileaddr;       /* starting address of data area */
} dfs_t;

/* volume header fields offset */
#define VH_MAGIC0    0x0         /* magic # */
#define VH_MAGIC1    0x1
#define VH_MAGIC2    0x2
#define VH_OEM0      0x3         /* OEM identification */
#define VH_OEM1      0x4
#define VH_OEM2      0x5
#define VH_OEM3      0x6
#define VH_OEM4      0x7
#define VH_OEM5      0x8
#define VH_OEM6      0x9
#define VH_OEM7      0xa
#define VH_SSLO      0xb         /* sector size in bytes */
#define VH_SSHI      0xc
#define VH_CS        0xd         /* cluster size in sectors */
#define VH_RCLO      0xe         /* # of reserved sectors */
#define VH_RCHI      0xf
#define VH_FC        0x10        /* # of FAT */
#define VH_RELO      0x11        /* # of entries in root */
#define VH_REHI      0x12
#define VH_SCLO      0x13        /* disk size in sectors */
#define VH_SCHI      0x14
#define VH_MT        0x15        /* media descriptor byte */
#define VH_FSLO      0x16        /* FAT size in sectors */
#define VH_FSHI      0x17
#define VH_STLO      0x18        /* track size in sectors, v3.0 */
#define VH_STHI      0x19
#define VH_HCLO      0x1a        /* # of heads, v3.0 */
#define VH_HCHI      0x1b
#define VH_HSLLO     0x1c        /* # of hidden sectors, v3.0 */
#define VH_HSLHI     0x1d
#define VH_HSHLO     0x1e        /* more hidden sectors, v4.0 */
#define VH_HSHHI     0x1f
#define VH_SCLLO     0x20        /* disk size in sectors (type=6) */
#define VH_SCLHI     0x21
#define VH_SCHLO     0x22        /* more disk size in sectors */
#define VH_SCHHI     0x23

#define    	DIR_ENTRY_SIZE          sizeof(dfile_t)
#define		DOS_RESERVECOUNT 	(1)
#define         DOS_MAGIC1      	(0xEB)
#define         DOS_MAGIC2      	(0x28)
#define         DOS_MAGIC3      	(0x90)
#define         DOS_MEDIA_TYPE  	(0xF8)
#define		DOS_FATID		(DOS_MEDIA_TYPE)
#define		DOS_FATCOUNT		(2)
#define         DOS_ROOTENTRIES 	(512)
#define         DOS_ROOTSIZE(entries)   ((entries*DIR_ENTRY_SIZE)/SECTOR_SIZE)
#define		DOS_SECTORCOUNT(size)	(size/SECTOR_SIZE)


#endif /* __DOS_H__ */
