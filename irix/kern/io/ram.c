/**************************************************************************
 *									  *
 * 		 Copyright (C) 1993, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"$Revision: 1.4 $"

/*
 * block (and character) access to private RAM ("RAM disk")
 */

/* #define FAKEVH */
#define	EFSMKFS

#include "sys/types.h"
#include "sys/cred.h"
#include "sys/buf.h"
#include "sys/errno.h"
#include "sys/open.h"
#include "sys/uio.h"
#include "sys/dkio.h"
#include "sys/dvh.h"
#include "sys/kmem.h"
#include "sys/errno.h"
#include "sys/param.h"
#ifdef	EFSMKFS
#include "sys/fs/efs.h"
#endif

#include "sys/ram.h"

#define	RAMUNITS	4
/*
 * Initally minor bits encoded the size, flags, etc.  Now these are in
 * master.d/ram, but the macros hide this from the rest of the driver.
 */
extern long rambytes[RAMUNITS];
extern char ramflags[RAMUNITS];
#define	RAMUNIT(minor)		(minor)
#define	RAMBYTES(minor)		rambytes[RAMUNIT(minor)]
#define	RAMISFLAG(minor, flag)	(ramflags[RAMUNIT(minor)] & (flag))

static struct ram_unit {
	char	*ru_ram;
	char	*ru_endram;
} ram_unit[RAMUNITS];

#define	INTHEWEEDS(rup, a, l)	(a < rup->ru_ram || a + l > rup->ru_endram)

#ifdef	EFSMKFS
static void efs_mini_mkfs(char *, int);
static int efs_checksum(struct efs *);
#endif

extern time_t time;

/*ARGSUSED*/
int
ramopen(dev_t *devp, int flag, int otype, cred_t *cred)
{
	minor_t minor = getminor(*devp);
	struct ram_unit *rup = &ram_unit[RAMUNIT(minor)];

	if (!rup->ru_ram) {
		rup->ru_ram = kmem_zalloc(RAMBYTES(minor), KM_SLEEP);
		rup->ru_endram = rup->ru_ram + RAMBYTES(minor) - 1;
#ifdef	EFSMKFS
		if (RAMISFLAG(minor,RAM_EFS))
			efs_mini_mkfs(rup->ru_ram, RAMBYTES(minor)/BBSIZE);
#endif
	}
	return 0;
}

/*ARGSUSED*/
int
ramclose(dev_t *devp, int flag, int otype, cred_t *cred)
{
	/* XXX free? */
	return 0;
}

void
ramstrategy(struct buf *bp)
{
	struct ram_unit *rup = &ram_unit[RAMUNIT(getminor(bp->b_edev))];
	char *rp = rup->ru_ram + bp->b_blkno * BBSIZE;

	bp_mapin(bp); /* basically, ensures b_dmaaddr is valid */
	if (INTHEWEEDS(rup, rp, bp->b_bcount)) {
		bp->b_error = EFAULT;
		iodone(bp);
		return;
	}
	if (bp->b_flags & B_READ)
		bcopy(rp, bp->b_dmaaddr, bp->b_bcount);
	else	/* what else is there? */
		bcopy(bp->b_dmaaddr, rp, bp->b_bcount);

	iodone(bp);
}

/* needed in efs_mountfs (spec_somethingotherother) */
int
ramsize(dev_t dev)
{
	return RAMBYTES(getminor(dev))/BBSIZE;
}

static int
ramrw(dev_t dev, int offset, int len, int rw, uio_t *uio)
{
	struct ram_unit *rup = &ram_unit[RAMUNIT(getminor(dev))];
	char *rp = rup->ru_ram + offset;
	if (INTHEWEEDS(rup, rp, len))
		return EFAULT;
	return uiomove(rp, len, rw, uio);
}

/*ARGSUSED*/
int
ramread(dev_t dev, uio_t *uio, cred_t *cred)
{
	return ramrw(dev, uio->uio_offset, uio->uio_iov->iov_len, UIO_READ,uio);
}

/*ARGSUSED*/
int
ramwrite(dev_t dev, uio_t *uio, cred_t *cred)
{
	return ramrw(dev, uio->uio_offset, uio->uio_iov->iov_len,UIO_WRITE,uio);
}

#ifdef	FAKEVH
/*
 * Stock "short_form" /etc/mkfs uses DIOCGETVH to get size, # heads, etc
 */
static struct volume_header ramvh;

int
ramioctl(dev_t dev, int cmd, int arg, int mode, cred_t *cred, int *rval)
{
	int error = 0;
	switch (cmd) {
	case DIOCGETVH:
		{
		ramvh.vh_magic = VHMAGIC;
		/* XXX	1) breaks /etc/mkfs if /dev/ram minor not unit #
		 *	2) efs_mini_mkfs() eliminates the need to use /etc/mkfs
		 */
		ramvh.vh_pt[0].pt_firstlbn = 0;
		ramvh.vh_pt[0].pt_nblks = RAMBYTES(getminor(dev))/BBSIZE;
		ramvh.vh_pt[0].pt_type = PTYPE_RAW;
		ramvh.vh_dp.dp_interleave = 1;
		ramvh.vh_dp.dp_trks0 = 1;
		ramvh.vh_dp.dp_secs = RAMBYTES(getminor(dev))/BBSIZE;
		ramvh.vh_dp.dp_secbytes = BBSIZE;
		ramvh.vh_dp.dp_cyls = 1;
		ramvh.vh_csum = -vh_checksum(&ramvh);
		if(copyout(&ramvh, arg, sizeof(ramvh)))
			error = EFAULT;
		break;
		}
	}
	return error;
}
#endif

#ifdef	EFSMKFS
/* Make a 1 cg EFS -- code liberally lifted from mkfs.c */
static void
efs_mini_mkfs(char *rp, int ramblocks)
{
	struct efs *fs;
	int bitmap_bytes, bitmap_blocks;
	char *bitmap;
	struct efs_dinode *di;
	struct efs_dirblk *dp;
	struct efs_dent *dentp;
	int rootdirbn, bn, j;

	/* superblock */
	fs = (struct efs*)(rp + BBSIZE);
	fs->fs_size = ramblocks;
#define	BITSPERBYTE 8
        fs->fs_bmsize = (fs->fs_size + BITSPERBYTE - 1) / BITSPERBYTE;
	fs->fs_firstcg = 2 + BTOBB(fs->fs_bmsize);
	fs->fs_cgfsize = fs->fs_size - fs->fs_firstcg;
#define GOOD_INODE_TOTAL(b) (b / 10 + (b > 20000 ? 1000 : b / 20))
	fs->fs_cgisize = (GOOD_INODE_TOTAL(ramblocks) + EFS_INOPBB - 1)
				/ EFS_INOPBB;
	fs->fs_sectors = ramblocks;
	fs->fs_ncg = 1;
	fs->fs_time = time;
	fs->fs_magic = EFS_MAGIC;
	fs->fs_bmblock = 0;
	fs->fs_heads = 1;
        fs->fs_tfree = fs->fs_cgfsize - fs->fs_cgisize;
        fs->fs_tinode = fs->fs_cgisize * EFS_INOPBB - 2; /* inum 0,1 unused */
	fs->fs_dirty = 0; /* XXX zmalloc? */
	fs->fs_checksum = efs_checksum(fs);

	/* bitmap -- mark everything free */
	bitmap = rp + BBTOB(2);	/* starts the block after the superb */
	bn = fs->fs_firstcg + fs->fs_cgisize;
	for (j = fs->fs_cgisize; j < fs->fs_cgfsize; j++, bn++)
		bset(bitmap, bn);

	/* root director inode */
	di = (struct efs_dinode *)(rp + fs->fs_firstcg * BBSIZE +
			2 * sizeof(struct efs_dinode));
	di->di_mode = 040755; /* S_IFDIR | 755 */
	di->di_nlink = 2;	/* . and .. */
	di->di_uid = di->di_gid = 0;
	di->di_atime = di->di_mtime = di->di_ctime = time;
	di->di_numextents = 1;
	rootdirbn = fs->fs_firstcg + fs->fs_cgisize;
	di->di_u.di_extents[0].ex_bn = rootdirbn;
	di->di_u.di_extents[0].ex_length = 1;
	di->di_u.di_extents[0].ex_offset = 0;
	di->di_size = BBSIZE;

	bclr(bitmap, rootdirbn);	/* allocate root dirblk */

	/* root directory block -- . and .. */
	dp = (struct efs_dirblk *)(rp + BBTOB(rootdirbn));
	dp->magic = EFS_DIRBLK_MAGIC;
#define	DOTOFF		506
#define	DOTDOTOFF	498
	dp->firstused = DOTDOTOFF >> 1;
	dp->slots = 2;
	dp->space[0] = DOTOFF >> 1;
	dp->space[1] = DOTDOTOFF >> 1;
	dentp = (struct efs_dent*)((char *)dp + DOTOFF);
	dentp->ud_inum.l = 2;
	dentp->d_namelen = 1;
	dentp->d_name[0] = '.';
	dentp = (struct efs_dent*)((char *)dp + DOTDOTOFF);
	dentp->ud_inum.l = 2;
	dentp->d_namelen = 2;
	dentp->d_name[0] = '.';
	dentp->d_name[1] = '.';
}

static int
efs_checksum(struct efs *fs)
{
	ushort *sp = (ushort *)fs;
	int checksum = 0;

	while (sp < (ushort *)&fs->fs_checksum) {
		checksum ^= *sp++;
		checksum = (checksum << 1) | (checksum < 0);
	}
	return checksum;
}
#endif	/* EFSMKFS */

int ramdevflag = 0;	
