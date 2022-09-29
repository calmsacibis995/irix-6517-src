#ifndef	_FMT_DOS_H
#define	_FMT_DOS_H

#include <sys/bsd_types.h>

/*
 * Partition table entry structure
 * Please note that the starting and ending
 * cylinder fields are actually 10 bits, and
 * the starting and ending sector fields are
 * actually 6 bits.
 */
typedef struct pte {
    unsigned char       sys_ind;
    unsigned char       bhead;
    unsigned char       bsect;          /*  6 bits */
    unsigned char       bcyl;           /* 10 bits */
    unsigned char       type;
    unsigned char       ehead;
    unsigned char       esect;          /*  6 bits */
    unsigned char       ecyl;           /* 10 bits */
    unsigned char       start_sect_ll;
    unsigned char       start_sect_lh;
    unsigned char       start_sect_hl;
    unsigned char       start_sect_hh;
    unsigned char       numbr_sects_ll;
    unsigned char       numbr_sects_lh;
    unsigned char       numbr_sects_hl;
    unsigned char       numbr_sects_hh;
} pte_t;

#define offset(b, n)    ((struct pte *)((b) + 0x1BE + (n)*sizeof(struct pte)))

/* DOS filesystem boot parameters */
typedef struct bp {
        u_char  bp_magic[3];            /* Magic # */
        u_char  bp_oem[8];              /* OEM identification */
        u_short bp_sectorsize;          /* Sector size in bytes */
        u_char  bp_sectorspercluster;   /* Cluster size in sectors */
        u_short bp_reservecount;        /* # of reserved sectors */
        u_char  bp_fatcount;            /* # of FAT */
        u_short bp_rootentries;         /* # of entries in root */
        u_short bp_sectorcount;         /* # of sectors on disk */
        u_char  bp_mediatype;           /* Disk type */
        u_short bp_fatsize;             /* FAT size in sectors */
        u_short bp_sectorspertrack;     /* Sectors per track */
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
        u_char  df_reserved[10];        /* not used */
        u_char  df_time[2];             /* time file last modified */
        u_char  df_date[2];             /* date file last modified */
        u_char  df_start[2];            /* first FAT entry used by file data */
        u_char  df_size[4];             /* file size in bytes */
} dfile_t;

#define	FILE_LEN	(8)
#define	EXTN_LEN	(3)

#define PDESCRIPTOR_OFFSET              0x1BE
#define EXTENDED_PARTITION              5
#define HUGE_PARTITION                  6
#define PARTN_TYPE(buf, partn)   (buf[PD_OFFSET+(partn-1)*0x10+4])
#define PARTN_OFFSET(buf, partn) ((buf[PD_OFFSET+(partn-1)*0x10+8])      |\
                                  (buf[PD_OFFSET+(partn-1)*0x10+9] << 8) |\
                                  (buf[PD_OFFSET+(partn-1)*0x10+10]<< 16)|\
                                  (buf[PD_OFFSET+(partn-1)*0x10+11]<< 24))

/* volume header fields offset */
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
#define VH_SCLLO        0x20            /* disk size in sectors (type=6) */
#define VH_SCLHI        0x21
#define VH_SCHLO        0x22            /* more disk size in sectors */
#define VH_SCHHI        0x23

/*
 * - MAGIC_PART is true if the sector is a partition table sector
 * - MAGIC_DOS  is true if the sector is a DOS boot sector
 * - SECTOR_COUNT  gives num. sectors in partition (for type != 6)
 * - SECTOR_COUNT6 gives num. sectors in partition (for type == 6)
 */
#define MAGIC_PART(buf)    (buf[510] == 0x55 && buf[511] == 0xaa)
#define MAGIC_DOS(buf)     (buf[VH_MAGIC0] == 0xe9 || \
                           (buf[VH_MAGIC0] == 0xeb && buf[VH_MAGIC2] == 0x90))

#define SECTOR_COUNT(buf)  ((buf[VH_SCLO]) | (buf[VH_SCHI] << 8))
#define SECTOR_COUNT6(buf) ((buf[VH_SCLLO])| (buf[VH_SCLHI] << 8) |\
                            (buf[VH_SCHLO] << 16) | (bs[VH_SCHHI] << 24))

#define	RESERVE_COUNT(buf) ((buf[VH_RCHI] << 8) | buf[VH_RCLO])
#define	FAT_SIZE(buf)	   ((buf[VH_FSHI] << 8) | buf[VH_FSLO])
#define	FAT_COUNT(buf)	   (buf[VH_FC])

#define PARTN_START_SECT(p)        ((p->start_sect_ll)        |\
                                    (p->start_sect_lh << 8)   |\
                                    (p->start_sect_hl << 16)  |\
                                    (p->start_sect_hh << 24))

#define PARTN_NUMBR_SECT(p)        ((p->numbr_sects_ll)       |\
                                    (p->numbr_sects_lh << 8)  |\
                                    (p->numbr_sects_hl << 16) |\
                                    (p->numbr_sects_hh << 24))


#endif

