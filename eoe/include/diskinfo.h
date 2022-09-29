#ifndef __DISKINFO_H__
#define __DISKINFO_H__
#ifdef __cplusplus
extern "C" {
#endif
/**************************************************************************
 *									  *
 * 		 Copyright (C) 1988, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"libdisk/diskinfo.h $Revision: 1.25 $"

#include <sgidefs.h>

/*	Data structures, etc. for routines in libdisk.a.
	First version created by Dave Olson @ SGI, 10/88
*/

/* defines */
/*
 * Maximum drives per controller not including SCSI luns
 * This is used to limit the size of names generated for drives
 * and is not representative of hardware capabilities.
 * See also irix/cmd/fx/fx.h
 */
#define MAX_DRIVE	1000

/* partition definitions for standard partitions  */
#define PT_FSROOT	0  /* root partition */
#define PT_FSSWAP	1  /* swap partition */
#define PT_FSUSR	6  /* alt usr partition */
#define PT_FSALL	7  /* one big partition */
#define PT_VOLHDR	8  /* volume header partition */
#define PT_REPL	9  /* trk/sec replacement part */
#define PT_VOLUME	10 /* entire volume partition */
#define NPARTS	16	/* same as NPARTAB in dvh.h, but avoids having
	all users of this library having to include dvh.h */
#define PT_BASE NPARTS	/* pseudo partition used with partnum(),
	getnextdisk(void), or getdiskname(inventory_t *, unsigned),
	to get just the base part of the name */

/* states for fsstate */
#define NOFS 0	/* no valid fs */
#define MOUNTEDFS   1	/* filesystem mounted */
#define MTRDONLY  2	/* filesystem mounted readonly */
#define CLEAN     4	/* file system not mounted, but is clean */
#define DIRTY     5	/* has filesystem, but marked dirty */

/*	defines for values of mtpt_fromwhere */
#define MTPT_NOTFOUND	0
#define MTPT_FROM_FSTAB	1
#define MTPT_FROM_MTAB	2
#define MTPT_FROM_SBLK	3


/* structures */
/* 'full' and 'nickname' pathnames for disk devices,
	relative to the "devdir" directory.  A pointer to
	this structure is returned by the getdiskname(inventory_t *, unsigned)
	routine.  The ptr returned by getdiskname() points to a static structure,
	and must be copied if you want to keep the data across calls to
	getdiskname() or getnextdisk().
*/
struct disknames {
	char fullname[32];	/* full name in the devdir/rdsk directory */
	char bnickname[32];	/* short block name, e.g. "usr" for "dks0d1s6" */
	char rnickname[32];	/* short raw name, e.g. "rusr" for "dks0d1s6" */
	char nodeok;	/* does the node for fullname exist in the file system? */
	struct inventory_s *diskinv;	/* pointer to inventory structure for this
		device, valid only until endnextdisk() is called (avoid typedef
		so only those who need it have to include sys/invent.h) */
};


/* per partition information */
struct ptinfo {
	unsigned ptnum;	/* partition # */
	unsigned psize;	/* partition size in sectors */
	unsigned pstartblk;	/* sector offset from start of disk */
	unsigned ptype;	/* partition type (PTYPE_* from dvh.h) */
	char fsstate;	/* see states above */
		/* rest of fields valid only if fsstate != NOFS */
	char fsname[8];	/* from super block */
	char packname[8];	/* from super block */
	__int64_t fssize;	/* size of file system in sectors */
	__int64_t ftotal;	/* total data blocks in file system */
	__int64_t itotal;	/* total inodes in file system */
	__int64_t fsfree;	/* free sectors in file system */
	__int64_t ifree;	/* free inodes in file system */
	char mountdir[64];	/* mount point if mounted, 'normal' mount point if not */
	struct ptinfo **overlaps;	/* pointer to array of ptrs to overlapping
		partitions (NULL if none). terminated by NULL ptr */
};


/* structure defining the layout and state of a disk.  Contains
	information from volume header, mount table, and super block.
*/
struct diskinfo {
	unsigned short sectorsize;	/* bytes per sector */
	unsigned capacity;	/* number of sectors on volume */
};


/* extern declarations */
/*	prefixes for the disknames of various disk controllers.  Indexed
	by the types defined in invent.h
*/
extern char *drive_prefixes[];

extern int mtpt_fromwhere;	/* set by getmountpt to indicate source
	of information.  See MTPT* defines above */


/* function prototypes */

int setdiskname(char *);
struct disknames *getdiskname(struct inventory_s *, unsigned);
void enddiskname(void);

int setnextdisk(char *, unsigned);
struct disknames *getnextdisk(void);
void endnextdisk(void);

unsigned setdiskinfo(char *, char *, int);
struct ptinfo *getpartition(unsigned);
struct diskinfo *getdiskinfo(unsigned);
int putpartition(unsigned, struct ptinfo *);
void enddiskinfo(unsigned);

char *pttype_to_name(int);
char *getmountpt(char *, char *);

/* returns 1 if disk partition has a valid efs or xfs filesystem, else 0,
 * second arg is a size to validate against filesystem size. */
int has_fs(char *, __int64_t);

/* returns 1 if disk partition has a valid filesystem of that type, else 0 */
int has_efs_fs(char *);
int has_xfs_fs(char *);

char *partname(char *, unsigned);

char *findrawpath(char *);
char *findblockpath(char *);
__int64_t findsize(char *);
int findtrksize(char *);

struct volume_header;
/* getdiskheader gets it from driver with DKIOCGETVH, while
 * getheaderfromdisk gets it from the disk device with a read,
 * both verify the volhdr with vhchksum.  putdiskheader does both
 * the ioctl and a write to disk */
int getdiskheader(int, struct volume_header *);
int getheaderfromdisk(int, struct volume_header *);
int putdiskheader(int, struct volume_header *, int);
int valid_vh(struct volume_header *);
int vhchksum(struct volume_header *);

int readb(int, void *, daddr_t, int);
int writeb(int, void *, daddr_t, int);

struct efs;
struct xfs_sb;
int getsuperblk_efs(char *, struct efs *);
int getsuperblk_xfs(char *, struct xfs_sb *);
int is_rootpart(char *);

void xlv_get_subvol_stripe(char *dfile, int type, int *sunit, int *swidth);

int disk_openraw (char *);	/* open raw path associated with specified path */
int disk_openvh (char *);	/* open volume header associated with specified path */
int disk_getblksz (int);	/* get blocksize from raw device */
int disk_setblksz (int, int);	/* set raw device blocksize */

#ifdef __cplusplus
}
#endif

/*
 * Structures and function declarations used for disk accounting
 * programs such as sar/sadc in conjunction with libdisk.
 * It is expected that PCP will also use these.
 */
struct diosetup
{
	struct diosetup		*next;		/* Pointer to next */
	char			 dname[12];	/* Container for dname */
	struct iotime		*iotim_addrs;	/* Pointer to iotim_addrs */
};

struct disk_iostat_info
{
	struct disk_iostat_info	*next;		/* Pointer to next */
	char			*diskname;	/* Pointer to disk name */
	char			*devicename;	/* Pointer to device name */
};

void sgi_neo_disk_setup(struct disk_iostat_info * (alloc_fn)(void));
int dkiotime_neoget(char *dsname, struct iotime *iotim);

#endif /* !__DISKINFO_H__ */
