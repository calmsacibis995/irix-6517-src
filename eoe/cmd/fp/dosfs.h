#ifndef _DOS_FS_H_
#define _DOS_FS_H_


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
	u_long	bp_hiddensectors;	/* # of hidden sectors */
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


#ifdef FPCK  /* following area used by fpck */

typedef  u_char        BITMAPUNIT;
#define  BITMAPUNITSZ  (sizeof(BITMAPUNIT)*8)

 
/* DOS filesystem structure.  One per exported floppy filesystem.  */
typedef struct dfs {
    char         * dfs_pathname;       /* device pathname */
    int            dfs_fd;             /* device file descriptor */
    dev_t          dfs_dev;            /* filesystem's device */
    u_short        dfs_flags;          /* flags */
#define DFS_RDONLY    (1 << 0)
#define DFS_STALE     (1 << 1)
    struct bp      dfs_bp;               /* disk boot parameter */
    u_char       * dfs_fat;              /* in-core copy of FAT */
    dfile_t      * dfs_root;             /* root directory contents */
    u_long         dfs_fataddr;          /* starting address of FAT */
    u_long         dfs_fatsize;          /* FAT size in bytes */
    u_long         dfs_rootaddr;         /* starting address of root */
    u_long         dfs_rootsize;         /* root size in bytes */
    u_long         dfs_fileaddr;         /* starting address of data area */
    u_char       * dfs_buf;              /* disk in-core buffer */
    u_char       * dfs_zerobuf;          /* disk in-core zero buffer */
    u_long         dfs_clustersize;      /* cluster size in bytes */
    u_long         dfs_clustercount;     /* # of addressable clusters */
    u_long         dfs_freeclustercount; /* # of free clusters */
    u_short        dfs_endclustermark;   /* 0xfff or 0xffff */
    u_short        dfs_badclustermark;   /* 0xff7 or 0xfff7 */
    u_short        dfs_unusedclustermark;   /* 0xffe or 0xfffe */
    u_short        (*dfs_readfat)(struct dfs *, u_short);
    void           (*dfs_writefat)(struct dfs *, u_short, u_short);
    BITMAPUNIT   * fatbitmapp;           /* against the fat table */
    int            fatbitmapsz;          /* size of fatbitmap */
    FALLOCINFO   * fblkheadp;            /* file block chain list */
    CLUSTERLIST  * crosslinklist;        /* list of the cross linked cluster */
    u_short        dirtyfat;             /* flag for dirty fat */
    u_short        dirtyrootdir;         /* flag for dirty cache root */
} dfs_t;


#define    SECTOR_SIZE    (512)

#define    AREAD           04        /* check read access */
#define    AWRITE          02        /* check write access */
#define    AEXEC           01        /* execute access, not used yet */
#define    DEFAULTMODE   0644        /* default mode */

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

#define    LAST_ENTRY            0x00
#define    REUSABLE_ENTRY        0xe5
#define    DIR_ENTRY_SIZE            sizeof(dfile_t)
#define    DIR_ENTRY_PER_CLUSTER(dfs)\
    ((dfs)->dfs_clustersize / DIR_ENTRY_SIZE)
#define    DISK_ADDR(dfs, cluster) \
    (((cluster) - 2) * (dfs)->dfs_clustersize + (dfs)->dfs_fileaddr)
#define LAST_CLUSTER(dfs, cluster) \
	((cluster) >= ((dfs)->dfs_endclustermark - 0x7) && \
	 (cluster) <= (dfs)->dfs_endclustermark)

extern void    BeginScanLoop(struct dfs *, int);
extern int     fatGetvh(FPINFO *, u_int, dfs_t *);
extern int     fatChkvh(FPINFO *);
extern void    fpPart_read_disk(FPINFO *);
extern u_short readfat12(dfs_t *, u_short);
extern u_short readfat16(dfs_t *, u_short);
extern void    writefat12(dfs_t *, u_short, u_short);
extern void    writefat16(dfs_t *, u_short, u_short);
extern int     getfilesz(dfile_t *);
extern int     getstartcluster(dfile_t *);
extern void    to_dostime(u_char *, u_char *, time_t *);
extern char  * getdosname(dfile_t *);
extern void    setfatbitmap(dfs_t *, u_short);
#endif /* FPCK */
#endif /* __DOS_FS_H__ */
