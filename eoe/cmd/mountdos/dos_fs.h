/*
 *=============================================================================
 *			File:		vfat.h
 *=============================================================================
 */
#ifndef	_VFAT_H
#define	_VFAT_H

#include <sys/bsd_types.h>
#include <rpc/types.h>
#include <sys/time.h>

typedef struct fhandle fhandle_t;

#include <sys/fs/nfs_clnt.h>

#include "nfs_prot.h"

#define FHPADSIZE       (NFS_FHSIZE - sizeof(dev_t) - sizeof(struct loopfid))

typedef struct fhandle {
        dev_t           fh_dev;                 /* floppy device number */
        struct loopfid  fh_fid;                 /* see <nfs/nfs_clnt.h> */
        char            fh_pad[FHPADSIZE];      /* padding, must be zeroes */
} fhandle_t;

#define fh_fno          fh_fid.lfid_fno
#define ROOTFNO         (2 << 2)

#define	UID		(0)
#define	GID		(0)

#define	MAX_SLOTS		(21)
#define MODE_BRK                (1)
#define MODE_CNT                (2)

#define DE_FREE                 (1)
#define DE_DIRENT               (2)
#define DE_SLOT                 (3)
#define	DE_LABEL		(4)

#define ST_INIT              	(1)
#define ST_EXPECT_DIRENT        (2)
#define ST_EXPECT_SLOT          (3)
#define ST_EXPECT_FREE          (4)

#define LNAME_LEN       	(256)
#define NAME_LEN        	(8)
#define EXTN_LEN        	(3)
#define SNAME_LEN       	(NAME_LEN+EXTN_LEN)
#define MSDOS_NAME      	(11)
#define DEBUG(str)      	if (dbg) printf(str); printf("\n")

#define EOKAY           	(0)
#define	ERR			(1)
#define ERROR           	(-1)

/* DOS filesystem boot parameters */

#define DOS_MEDIA_35FLOP_2SD_18SECT	0xf0 /* supported v3.3 */
#define DOS_MEDIA_FLOPPY		DOS_MEDIA_35FLOP_2SD_18SECT
#define DOS_MEDIA_FIXED_DISK		0xf8 /* supported v2.0 */
#define DOS_MEDIA_35FLOP_2SD_9SECT	0xf9 /* supported v3.2 */

typedef struct bp {
        u_char  bp_magic[3];            /* magic # */
        u_char  bp_oem[8];              /* OEM identification */
        u_short bp_sectorsize;          /* sector size in bytes */
        u_char  bp_sectorspercluster;   /* cluster size in sectors */
        u_short bp_reservecount;        /* # of reserved sectors */
        u_char  bp_fatcount;            /* # of FAT */
        u_short bp_rootentries;         /* # of entries in root */
        u_short bp_sectorcount;         /* # of sectors on disk */
        u_char  bp_mediatype;           /* disk type */
        u_short bp_fatsize;             /* FAT size in sectors */
        u_short bp_sectorspertrack;     /* sectors per track */
        u_short bp_headcount;           /* # of heads */
        u_long  bp_hiddensectors;       /* # of hidden sectors */
} bp_t;

/* DOS disk file structure */
typedef struct dfile {
        char    df_name[8];             /* file name */
        char    df_ext[3];              /* file name extension */
        u_char  df_attribute;           /* file attribute */
#define ATTRIB_RDONLY   0x01
#define ATTRIB_HIDDEN   0x02
#define ATTRIB_SYSTEM   0x04
#define ATTRIB_LABEL    0x08
#define ATTRIB_DIR      0x10
#define ATTRIB_ARCHIVE  0x20
#define ATTRIB_RSRV1    0x40
#define ATTRIB_RSRV2    0x80
#define	ATTRIB_EXTNDD	(ATTRIB_RDONLY | ATTRIB_HIDDEN | \
			 ATTRIB_SYSTEM | ATTRIB_LABEL)
        u_char  df_reserved[10];        /* not used */
        u_char  df_time[2];             /* time file last modified */
        u_char  df_date[2];             /* date file last modified */
        u_char  df_start[2];            /* first FAT entry used by file data */
        u_char  df_size[4];             /* file size in bytes */
} dfile_t;

/* DOS file node structure.  One per active file. */
typedef struct dnode {
        struct dnode    *d_forw;        /* hash table linkage */
        struct dnode    *d_back;
        struct dnode    *d_avforw;      /* freelist linkage */
        struct dnode    *d_avback;
        struct dfs      *d_dfs;         /* filesystem we are in */
        u_long          d_fno;          /* file number */
        u_long          d_pfno;         /* parent file number */
        int             d_count;        /* reference count */
        ftype           d_ftype;        /* file type */
        struct dfile    *d_dir;         /* directory contents */
        u_short         d_mode;         /* protection */
#define READ_MODE       0444            /* read permission */
#define WRITE_MODE      0222            /* write permission */
#define SEARCH_MODE     0111            /* search permission */
        u_short         d_start;        /* first cluster */
        u_long          d_size;         /* file size in bytes */
        time_t          d_time;         /* time file last modified */
        uid_t           d_uid;          /* user id */
        gid_t           d_gid;          /* group id */
} dnode_t;

/*
 * Subdirectory entry cache
 */
typedef struct dirhash {
        dnode_t         *dnode;
        u_short         cluster;
        struct dirhash  *next;
} dirhash_t;

#define DIRHASHLOG2     6
#define DIRHASHSIZE     (1 << DIRHASHLOG2)
#define DIRHASHMASK     (DIRHASHSIZE - 1)
#define DIRHASH(clust)  dirhash[(clust) & DIRHASHMASK]

/* DOS filesystem structure.  One per exported floppy filesystem.  */
typedef struct dfs {
        struct dfs      *dfs_next;      /* next in list */
        char            *dfs_pathname;  /* device pathname */
        int             dfs_fd;         /* device file descriptor */
        dev_t           dfs_dev;        /* filesystem's device */
        u_short         dfs_flags;      /* flags */
#define DFS_RDONLY      (1 << 0)
#define DFS_STALE       (1 << 1)
        struct bp       dfs_bp;         /* disk boot parameter */
        u_char          *dfs_fat;       /* in-core copy of FAT */
        struct dnode    *dfs_root;      /* root dnode, directory contents */
        u_long          dfs_fataddr;    /* starting address of FAT */
        u_long          dfs_fatsize;    /* FAT size in bytes */
        u_long          dfs_rootaddr;   /* starting address of root */
        u_long          dfs_rootsize;   /* root size in bytes */
        u_long          dfs_fileaddr;   /* starting address of data area */
        u_char          *dfs_buf;       /* disk in-core buffer */
        u_char          *dfs_zerobuf;   /* disk in-core zero buffer */
        u_short         dfs_clustersize;        /* cluster size in bytes */
        u_short         dfs_clustercount;       /* # of addressable clusters */
        u_short         dfs_freeclustercount;   /* # of free clusters */
        u_short         dfs_endclustermark;     /* 0xfff or 0xffff */
        u_short         (*dfs_readfat)();
        void            (*dfs_writefat)();
        u_short         dirtyfat;       /* flag for dirty fat */
        u_short         dirtyrootdir;   /* flag for dirty cache root */
} dfs_t;

typedef struct slot {
        u_char sl_id;                   /* sequence number for slot  */
        u_char sl_name0_4[10];          /* first 5 characters in name*/
        u_char sl_attr;                 /* attribute byte            */
        u_char sl_reserved;             /* always 0                  */
        u_char sl_chksum;         	/* checksum for 8.3 alias    */
        u_char sl_name5_10[12];         /* 6 more characters in name */
        u_char sl_start[2];             /* starting cluster number   */
        u_char sl_name11_12[4];         /* last 2 characters in name */
} slot_t;

typedef struct  file {
        u_short f_cluster;              /* dir entry cluster locn */
        u_short f_offset;               /* dir entry offset  locn */
        int     f_nentries;             /* dir entry number slots */
        dfile_t f_entries[MAX_SLOTS];   /* dir entry structures   */
} file_t;

#define DE_SIZ                  (sizeof(dfile_t))

#define CLEAR_SLOTS(fp)     \
        fp->f_nentries = 0; \
        bzero(fp->f_entries, MAX_SLOTS*DE_SIZ)

#define COPY_SLOT_FROM_FP(fp, de, id) \
        bcopy(&(fp->f_entries[id]), de, DE_SIZ)

#define COPY_SLOT_TO_FP(fp, de, id) \
        bcopy(de, &(fp->f_entries[id]), DE_SIZ)

#define START_CLUSTER(fp) \
        ((fp->f_entries[0].df_start[0]) | (fp->f_entries[0].df_start[1] << 8))

#define AREAD           04              /* check read access */
#define AWRITE          02              /* check write access */
#define AEXEC           01              /* execute access, not used yet */
#define DEFAULTMODE     0777            /* default mode */
#define	DEFAULTREADMODE	0555		/* default read mode */

#define MAGIC_PART(bs)  (bs[510] == 0x55 && bs[511] == 0xaa)
#define MAGIC_DOS(bs)   (bs[VH_MAGIC0] == 0xe9 || \
                        (bs[VH_MAGIC0] == 0xeb && bs[VH_MAGIC2] == 0x90))
#define MAGIC_EXTENDED_BOOT(bs)	(bs[VH_EXTB_MAGIC] == 0x29)

/*
 * Function prototypes for "dos_fs.c"
 */
int     dos_addfs(char *, u_int, u_int, dfs_t **);
dfs_t   *dos_getfs(dev_t);
int     dos_getnode(dfs_t *, u_long, dnode_t **);
int     dos_getvh(dfs_t *, u_int);
void    dos_initnodes();
void    dos_purgenode(dnode_t *);
void    dos_purgenodes(dfs_t *);
void    dos_putnode(dnode_t *);
void    dos_removefs(dfs_t *);
int     dos_statfs(dfs_t *, statfsokres *);


/* 
 * Volume Header fields offset 
 */
#define VH_MAGIC0       0x0             /* magic # */
#define VH_MAGIC1       0x1
#define VH_MAGIC2       0x2
#define VH_OEM0         0x3             /* OEM identification */
#define VH_OEM1         0x4
#define VH_OEM2         0x5
#define VH_OEM3         0x6
#define VH_OEM4         0x7
#define VH_OEM5         0x8
#define VH_OEM6         0x9
#define VH_OEM7         0xa
#define VH_SSLO         0xb             /* sector size in bytes */
#define VH_SSHI         0xc
#define VH_CS           0xd             /* cluster size in sectors */
#define VH_RCLO         0xe             /* # of reserved sectors */
#define VH_RCHI         0xf
#define VH_FC           0x10            /* # of FAT */
#define VH_RELO         0x11            /* # of entries in root */
#define VH_REHI         0x12
#define VH_SCLO         0x13            /* disk size in sectors */
#define VH_SCHI         0x14
#define VH_MT           0x15            /* media descriptor byte */
#define VH_FSLO         0x16            /* FAT size in sectors */
#define VH_FSHI         0x17
#define VH_STLO         0x18            /* track size in sectors, v3.0 */
#define VH_STHI         0x19
#define VH_HCLO         0x1a            /* # of heads, v3.0 */
#define VH_HCHI         0x1b
#define VH_HSLLO        0x1c            /* # of hidden sectors, v3.0 */
#define VH_HSLHI        0x1d
#define VH_HSHLO        0x1e            /* more hidden sectors, v4.0 */
#define VH_HSHHI        0x1f
#define VH_SCLLO        0x20            /* disk size in sectors (type=6) v4.0 */
#define VH_SCLHI        0x21
#define VH_SCHLO        0x22            /* more disk size in sectors v4.0 */
#define VH_SCHHI        0x23
#define VH_PDNO		0x24		/* physical drive number ext v4.0 */
#define VH_EXTRES	0x25		/* reserved ext v4.0 */
#define VH_EXTB_MAGIC	0x26		/* extended boot signature record ext v4.0 */
#define VH_BVIDLLO	0x27		/* binary volume ID ext v4.0 */
#define VH_BVIDLHI	0x28
#define VH_BVIDHLO	0x29
#define VH_BVIDHHI	0x2a
#define VH_VOLAB0	0x2b		/* volume label ext v4.0 */
#define VH_VOLAB1	0x2c
#define VH_VOLAB2	0x2d
#define VH_VOLAB3	0x2e
#define VH_VOLAB4	0x2f
#define VH_VOLAB5	0x30
#define VH_VOLAB6	0x31
#define VH_VOLAB7	0x32
#define VH_VOLAB8	0x33
#define VH_VOLAB9	0x34
#define VH_VOLAB10	0x35
#define VH_EXTRES0	0x36		/* reserved ext v4.0 */
#define VH_EXTRES1	0x37
#define VH_EXTRES2	0x38
#define VH_EXTRES3	0x39
#define VH_EXTRES4	0x3a
#define VH_EXTRES5	0x3b
#define VH_EXTRES6	0x3c
#define VH_EXTRES7	0x3d

#define PDESCRIPTOR_OFFSET              0x1BE
#define SECTOR_SIZE                     512
#define PD_SIZE                         0x10
#define PD_OFFSET                       PDESCRIPTOR_OFFSET
#define PARTITION_NOTUSED		0
#define PARTITION_FAT12			1
#define PARTITION_FAT16			4
#define PARTITION_EXTENDED              5
#define PARTITION_HUGE                  6 /* MS-DOS versions 4.0 and later */

#define PARTN_TYPE(bs, p_num)   (bs[PD_OFFSET+(p_num-1)*0x10+4])
#define PARTN_OFFSET(bs, p_num) ((bs[PD_OFFSET+(p_num-1)*0x10+8])        |\
                                 (bs[PD_OFFSET+(p_num-1)*0x10+9] << 8)   |\
                                 (bs[PD_OFFSET+(p_num-1)*0x10+10]<< 16)  |\
                                 (bs[PD_OFFSET+(p_num-1)*0x10+11]<< 24)) *\
                                 SECTOR_SIZE

#define PARTN_SIZE(bs, p_num)   ((bs[PD_OFFSET+(p_num-1)*0x10+12])       |\
                                 (bs[PD_OFFSET+(p_num-1)*0x10+13] << 8)  |\
                                 (bs[PD_OFFSET+(p_num-1)*0x10+14] << 16) |\
                                 (bs[PD_OFFSET+(p_num-1)*0x10+15] << 24))*\
                                 SECTOR_SIZE

#define PARTN1_TYPE(bs)         PARTN_TYPE(bs, 1)
#define PARTN2_TYPE(bs)         PARTN_TYPE(bs, 2)
#define PARTN3_TYPE(bs)         PARTN_TYPE(bs, 3)
#define PARTN4_TYPE(bs)         PARTN_TYPE(bs, 4)

#define PARTN1_OFFSET(bs)       PARTN_OFFSET(bs, 1)
#define PARTN2_OFFSET(bs)       PARTN_OFFSET(bs, 2)
#define PARTN3_OFFSET(bs)       PARTN_OFFSET(bs, 3)
#define PARTN4_OFFSET(bs)       PARTN_OFFSET(bs, 4)

#define PARTN1_SIZE(bs)         PARTN_SIZE(bs, 1)
#define PARTN2_SIZE(bs)         PARTN_SIZE(bs, 2)
#define PARTN3_SIZE(bs)         PARTN_SIZE(bs, 3)
#define PARTN4_SIZE(bs)         PARTN_SIZE(bs, 4)

#define SECTOR_COUNT(bs)        ((bs[VH_SCLO])        |\
                                 (bs[VH_SCHI] << 8))
#define SECTOR_COUNT_EXTD(bs)	((bs[VH_SCLLO])       |\
                                 (bs[VH_SCLHI] << 8)  |\
                                 (bs[VH_SCHLO] << 16) |\
                                 (bs[VH_SCHHI] << 24))

#define LAST_ENTRY      0x00

#define REUSABLE_ENTRY  0xe5

#define DIR_ENTRY_SIZE  sizeof(dfile_t)

#define DIR_ENTRY_PER_CLUSTER(dfs)\
        ((dfs)->dfs_clustersize / DIR_ENTRY_SIZE)

#define DISK_ADDR(dfs, cluster) \
        (((cluster) - 2) * (dfs)->dfs_clustersize + (dfs)->dfs_fileaddr)

#define	IS_DIR(f) \
	((f.f_entries[0].df_attribute) & ATTRIB_DIR)

#define	IS_RDONLY(f) \
	((f.f_entries[0].df_attribute) & ATTRIB_RDONLY)

#define	IS_LABEL(df) \
	(df->df_attribute != ATTRIB_EXTNDD && (df->df_attribute & ATTRIB_LABEL))

#define	DF_START(df)		(df->df_start[0] | (df->df_start[1] << 8))

#define	DF_SIZE(df)		((df->df_size[0]) | (df->df_size[1]<<8) | \
				 (df->df_size[2]<<16) | (df->df_size[3]<<24))

#define ROOT_CLUSTER            1

#define FNO(cluster, offset)    (((cluster) << 16) | (offset))

#define CLUSTER(fno)            ((fno) >> 16)

#define OFFSET(fno)             ((fno) & 0xffff)

#define LAST_CLUSTER(dfs, cluster) \
        ((cluster) >= ((dfs)->dfs_endclustermark - 0x7) && \
         (cluster) <= (dfs)->dfs_endclustermark)

#define	DPRINTF(string)		if (debug) printf(string)


/*
 * Everything related to Track cache
 */
#define MASKBYTES (sizeof(u_long))
#define MASKBYTESZ (sizeof(u_long)*8)
#define CACHE_READ   1
#define CACHE_WRITE  2

typedef struct dbufp {
   u_long * dirty;
   int dirtymaskcnts;
   int clusterspertrk;
   int clustersize;
   int trksize;
   int trkbgnaddr;
   int trkendaddr;
   u_char * bufp;
} dbufp_t;

extern struct timeval timeout;
extern struct timeval * timeoutp;
extern dbufp_t trkcache;

/*
 * Miscellaneous functions.
 */
void    panic(char *, ...);
void    *safe_malloc(unsigned);
void    terminate(int);
void    trace(char *, ...);

/*
 * Function Prototypes in "vfat.c"
 */
/*
 *-----------------------------------------------------------------------------
 * Function prototypes.
 * Directory level operations.
 *-----------------------------------------------------------------------------
 */
int     vfat_dir_read(dnode_t *dp);
int     vfat_dir_write(dnode_t *dp);
int     vfat_dir_lookup(dnode_t *dp, file_t *fp);
int     vfat_dir_getent_sname(dnode_t *dp, char *sname, file_t *fp);
int     vfat_dir_getent_lname(dnode_t *dp, char *lname, file_t *fp);
int     vfat_dir_printent(dnode_t *dp, char *aux, file_t *fp);
int     vfat_dir_delent(dnode_t *dp, file_t *fp);
int	vfat_dir_nulent(dnode_t *dp, file_t *fp);
int     vfat_dir_freent(dnode_t *dp, int nentries, u_short *clst, u_short *off);int     vfat_dir_writent(dnode_t *dp, file_t *fp);
int     vfat_dir_generic(dnode_t *dp, int mode, char *aux,
                         file_t *fp, int (*func)(file_t *, char *));
/*
 *-----------------------------------------------------------------------------
 * Directory entry level operations.
 *-----------------------------------------------------------------------------
 */
int     vfat_dirent_packname(file_t *fp,  char *lname, char *sname, int sflag);
int     vfat_dirent_upackname(file_t *fp, char *lname, char *sname);
int	vfat_dirent_read_disk(dfs_t *dfs, file_t *fp);
int	vfat_dirent_broken(file_t *fp);
/*
 *-----------------------------------------------------------------------------
 * Cluster level operations.
 *-----------------------------------------------------------------------------
 */
int     vfat_clust_writ(dfs_t *dfs, u_long daddr, u_char *clustbuf);
int     vfat_clust_read(dfs_t *dfs, u_long daddr, u_char *clustbuf);
/*
 *-----------------------------------------------------------------------------
 * Name conversion operations.
 *-----------------------------------------------------------------------------
 */
int     vfat_create_sname(dnode_t *dp, char *lname, char *sname, int *sflag);
int     vfat_type_lname(char *lname, char **lname_ext, int *lname_size);
int     vfat_compose_base(char *lname, char *lname_ext, int lname_size,
                          char *base,  int *baselen);
int     vfat_compose_ext(char *lname, char *lname_ext, int lname_size,
			 char *ext, int *extlen);
int     vfat_compose_sname(dnode_t *dp,
                           char *sname1, char *sname2,
                           char *base, int baselen,
                           char *ext,  int extlen);
int     vfat_format_sname(char *sname1, char *sname2, int len);
int     vfat_isvalid_lname(char *lname, int lname_len);

/*
 *-----------------------------------------------------------------------------
 * Miscellaneous operations.
 *-----------------------------------------------------------------------------
 */
char	*vfat_print11(char *sname);
u_char  vfat_sname_chksum(char *sname);
int     vfat_update_fat(dfs_t *dfs);
int     vfat_update_root(dfs_t *dfs);
u_short vfat_free_fat(dfs_t *dfs);
int     vfat_copy_attributes(file_t *fp1, file_t *fp2);

int     vfat_update_disk_fat(dfs_t *dfs);
int     vfat_update_disk_root(dfs_t *dfs);

int     dirent_print(file_t *fp);
int     dirent_cmp_sname(file_t *fp, char *sname);
int     dirent_cmp_lname(file_t *fp, char *lname);

int	dosname_cmp(char *s1, char *s2);
int	dosname_cmp_lname(char *s1, char *s2);

int     xtract_fp(dfs_t *dfs, dfile_t *clustbuf, u_short offset, file_t *fp);
int     copy_dir_to_fp(dnode_t *dp, file_t *fp);
int     copy_fp_to_dir(dnode_t *dp, file_t *fp);

u_char  a2ucode1(char ch);
u_char  a2ucode2(char ch);
u_char  ucode2a(u_short word, int *end);

void    to_dostime(u_char *d_date, u_char *d_time, time_t *u_time);
void    to_unixtime(u_char *d_date, u_char *d_time, time_t *u_time);

void    to_dosname(char *name, char *ext, char *uname);
void    to_unixname(char *name, char *ext, char *sname);

int     set_attribute(dnode_t *dp, int archive_attrib);
int     expand_file(dfs_t *dfs, u_short start, u_int o_size, u_int n_size);
u_int   subdir_size(dnode_t *dp);
int	end_cluster(dfs_t *dfs, u_short cluster);
u_short to_cluster(dnode_t *dp, u_short offset);

u_short readfat12(dfs_t *dfs, u_short cluster);
u_short readfat16(dfs_t *dfs, u_short cluster);
void    writefat12(dfs_t *dfs, u_short cluster, u_short value);
void    writefat16(dfs_t *dfs, u_short cluster, u_short value);

#endif
