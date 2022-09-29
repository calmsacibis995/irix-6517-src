/*
 * EFS inode operations.
 *
 * Copyright 1992-1994, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */
#ident "$Revision: 3.138 $"

#include <sys/types.h>
#include <sys/buf.h>
#include <sys/cmn_err.h>
#include <sys/conf.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/hwgraph.h>
#include <sys/kmem.h>
#include <sys/mode.h>
#include <sys/param.h>
#include <sys/pathname.h>
#include <sys/pfdat.h>		/* page flushing prototypes */
#include <sys/quota.h>
#include <sys/resource.h>
#include <sys/stat.h>		/* required by IFTOVT macro */
#include <sys/sysinfo.h> 
#include <sys/sysmacros.h> 
#include <sys/systm.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/mkdev.h>		/* for old/extended dev_t translations */
#include <sys/uthread.h>
#include "efs_fs.h"
#include "efs_inode.h"
#include "efs_sb.h"

extern void efs_free(struct inode *,extent *, int);
extern void (*afsidestroyp)(struct inode *);
extern void (*afsdptoipp)(struct efs_dinode *, struct inode *);
extern void (*afsiptodpp)(struct inode *, struct efs_dinode *);

/* Translate the on-disk representation of dev (may be old or extended
 * style) into a dev_t. A new dev beyond the old range is indicated
 * by a -1 in the old dev field: this will cause older kernels to fail
 * to open the device.
 */

static dev_t
efs_getdev(union di_addr *di)
{
	short olddev;
	major_t maj;

	if ((olddev = di->di_dev.odev) == -1)
		return (di->di_dev.ndev);
	else {
		maj = olddev >> ONBITSMINOR;
		return ((maj << NBITSMINOR) | (olddev & OMAXMIN));
	}
}

/* The converse: given an (extended) dev_t, put it into the on-disk
 * union. If its components are within the range understood by earlier
 * kernels, use the old format; if not, put -1 in the old field &
 * store it in the new one.
 */

static void
efs_putdev(dev_t dev, union di_addr *di)
{
	major_t maj;
	minor_t min;

	if ( ((maj = (dev >> NBITSMINOR)) > OMAXMAJ) ||
	     ((min = (dev & MAXMIN)) > OMAXMIN)) {
		di->di_dev.odev = -1;
		di->di_dev.ndev = dev;
	}
	else
		di->di_dev.odev = (maj << ONBITSMINOR) | min;
}

void
irealloc(struct inode *ip, size_t directbytes)
{
	int oldex;
	int newex;

	oldex = howmany(ip->i_directbytes, sizeof(extent));
	newex = howmany(directbytes, sizeof(extent));
        /*
         * If the valid direct extents can fit in i_direct, copy them
         * from the malloc'd vector and free it.
         */
        if (directbytes <= sizeof ip->i_direct) {
                if (ip->i_extents != ip->i_direct) {
                        bcopy(ip->i_extents, ip->i_direct, directbytes);
			efs_free_extent(ip->i_extents, oldex);
                        ip->i_extents = ip->i_direct;
                }
        } else {
                /*
                 * Stuck with malloc/realloc.
                 */
                if (ip->i_extents != ip->i_direct)
			ip->i_extents = efs_realloc_extent(ip->i_extents,
				oldex, newex);
                else {
                        ip->i_extents = efs_alloc_extent(newex);
                        bcopy(ip->i_direct, ip->i_extents, sizeof ip->i_direct);                }
        }

        ip->i_directbytes = directbytes;
}

/*
 * Free the storage used by this inode to hold the private inode
 * representation.
 */
void
efs_idestroy(register struct inode *ip)
{
	switch (ip->i_mode & IFMT) {
	  case IFREG:
	  case IFDIR:
	  case IFLNK:
	  case IFCHRLNK:
	  case IFBLKLNK:
		if (ip->i_numindirs > 0) {
			ASSERT(ip->i_indir != NULL);
			kmem_free(ip->i_indir,
				  ip->i_numindirs * sizeof(extent));
		}
		if (ip->i_extents != ip->i_direct)
			efs_free_extent(ip->i_extents,
				howmany(ip->i_directbytes, sizeof(extent)));
		if (ip->i_afs)
			(*afsidestroyp)(ip);
		break;
	}
	mutex_destroy(&ip->i_lock);
	kmem_zone_free(efs_inode_zone, ip);
}

/*
 * Map an inumber to a disk buffer mapping struct.
 * Returns offset into mapping.
 */
int
efs_imap(
	register struct efs *fs,
	register efs_ino_t inum,
	register struct bmapval *bmap)
{
	register efs_ino_t lowi;
	register efs_daddr_t bn;
	register struct cg *cg;

	cg = &fs->fs_cgs[EFS_ITOCG(fs, inum)];

	/*
	 * Set lowi to the inode which is at the base of the inode chunk
	 * that contains inum.  To do this, we make inum
	 * relative to the cg, round it down to the nearest fs_inopchunk,
	 * then make the resulting inumber un-relative.
	 */
	lowi = (((inum - cg->cg_firsti) / fs->fs_inopchunk) *
		fs->fs_inopchunk) + cg->cg_firsti;

	bmap->bn = bn = EFS_ITOBB(fs, lowi);
	if (bn + fs->fs_inopchunkbb > cg->cg_firstdbn)
		bmap->bsize = BBTOB(cg->cg_firstdbn - bn);
	else
		bmap->bsize = BBTOB(fs->fs_inopchunkbb);

#ifdef DEBUG
	if ((bn < cg->cg_firstbn) ||
	    (bn + BTOBBT(bmap->bsize) > cg->cg_firstdbn)) {
		prdev("efs_imap: bn=%d inumber=%d inopchunk=%d firsti=%d",
		     fs->fs_dev, bn, inum, fs->fs_inopchunk, cg->cg_firsti);
		cmn_err(CE_PANIC, "efs debug");
	}
#endif

	return (inum - lowi) * sizeof(struct efs_dinode);
}

/*
 * efs_itobp:
 *	- convert an inumber into a disk buffer containing the inode
 *	- use inode chunking, parameterized by fs_inopchunk to do the i/o
 */
static struct buf *
efs_itobp(
	register struct efs *fs,
	register efs_ino_t inum,
	struct efs_dinode **dip)
{
	register struct buf *bp;
	struct bmapval	bmap;
	register int offset;

	IGETINFO.ig_itobp++;
	offset = efs_imap(fs, inum, &bmap);

	bp = bread(fs->fs_dev, bmap.bn, BTOBBT(bmap.bsize));
	if (bp->b_flags & B_ERROR) {
		brelse(bp);
		return (NULL);
	}
	if (bp->b_flags & B_FOUND)
		IGETINFO.ig_itobpf++;
	*dip = (struct efs_dinode *)(bp->b_un.b_addr + offset);
	return (bp);
}

/*
 * Set format-dependent inode attributes.
 */
static int
efs_iformat(struct inode *ip, int ne, struct efs_dinode *dp)
{
	register int ni, nb;
	register extent	*ex;
	union di_addr *di = &(dp->di_u);

	switch (ip->i_mode & IFMT) {
	  case IFIFO:
	  case IFCHR:
	  case IFBLK:
	  case IFSOCK:
		ip->i_size = 0;
		ip->i_rdev = efs_getdev(di);
		break;

	  case IFCHRLNK:
	  case IFBLKLNK:
		ip->i_rdev = HWGRAPH_STRING_DEV;
		/* Fall through.... */
	  case IFLNK:
		if (ne == 0 && (nb = ip->i_size) <= EFS_MAX_INLINE) {
			ip->i_blocks = 0;
			ip->i_flags |= IINCORE;
			goto setup_direct;
		}
	  case IFREG:
	  case IFDIR:
		/*
		 * Check validity on number of extents.  If it is
		 * ridiculous, don't let the user reference it.
		 */
		if (ne < 0 || EFS_MAXEXTENTS < ne) {
			prdev("inode %d: illegal number of direct extents %d",
			      ip->i_dev, ip->i_number, ne);
			return EIO;
		}

		/*
		 * If indirect, check validity of the overloaded
		 * ex_offset field, which gives number of indirect
		 * extents; inode could be bad on disk.
		 */
		if (ne > EFS_DIRECTEXTENTS) {
			ni = di->di_extents[0].ex_offset;
			if (ni < 1 || EFS_DIRECTEXTENTS < ni) {
				prdev(
			    "inode %d: illegal number of indirect extents %d",
				      ip->i_dev, ip->i_number, ni);
				return EIO;
			}
			nb = ni * sizeof(extent);
			ASSERT((ip->i_flags & IINCORE) == 0);
		} else {
			/*
			 * Remember logical block high-water mark.
			 */
			if (ne > 0) {
				ex = &di->di_extents[ne - 1];
				ip->i_blocks =
					ex->ex_offset + ex->ex_length;
			}
			else
				ip->i_blocks = 0;

			nb = ne * sizeof(extent);
			ip->i_flags |= IINCORE;
		}
setup_direct:
		/*
		 * Set up direct extent descriptor.
		 */
		ip->i_numextents = ne;
		ip->i_extents = (nb <= sizeof ip->i_direct) ?
				ip->i_direct :
				efs_alloc_extent(howmany(nb, sizeof(extent)));
		bcopy(di->di_extents, ip->i_extents, nb);
		ip->i_directbytes = nb;
		break;

	  default:
		prdev("inode %d: illegal mode %o",
		      ip->i_dev, ip->i_number, ip->i_mode);
		return EIO;
	}
	return 0;
}


/*
 * Create a new inode in ip.  This routine is called from efs_ialloc
 * when a free inode has been found.  It initializes the fields that
 * efs_iread left zero, based on efs_ialloc's arguments.
 */
int
efs_icreate(struct inode *ip,
	mode_t mode,
	short nlink,
	dev_t rdev,
	struct inode *pip,
	struct cred *cr)
{
	struct vnode *vp;
	struct efs_dinode di;
	int error;

	vp = itov(ip);
	ASSERT(mutex_mine(&ip->i_lock));
	ASSERT(ip->i_mode == 0);
	ASSERT(mode);

	IGETINFO.ig_icreat++;
	/*
	 * Set the format-independent inode attributes from their
	 * corresponding arguments and call efs_iformat to set the
	 * format-dependent members.
	 */
	vp->v_type = IFTOVT(EFSTOSTAT(mode & IFMT));
	ip->i_mode = mode;
	ip->i_nlink = nlink;
	ip->i_uid = cr->cr_uid;
	ip->i_gid = cr->cr_gid;
	ip->i_size = 0;
	ip->i_remember = 0;
	vp->v_rdev = rdev;
	efs_putdev(rdev, &(di.di_u));
	if (error = efs_iformat(ip, 0, &di))
		return error;

	/*
	 * For multiple groups support: if ISGID bit is set in the parent
	 * directory, group of new file is set to that of the parent, and
	 * new subdirectory gets ISGID bit from parent.
	 */
	if ((itovfs(ip)->vfs_flag & VFS_GRPID) || (pip->i_mode & ISGID)) {
		ip->i_gid = pip->i_gid;
		if ((pip->i_mode & ISGID) && (mode & IFMT) == IFDIR)
			ip->i_mode |= ISGID;
	}

	/* 
	 * If the group ID of the new file does not match the effective group
	 * ID or one of the supplementary group IDs, the ISGID bit is
	 * cleared.
	 */
	 if (ip->i_mode & ISGID) {
		int i;

		for (i = 0 ; i < cr->cr_ngroups ; i++)
			if (ip->i_gid == cr->cr_groups[i])
				break;
		if ((ip->i_gid != cr->cr_gid) && (i >= cr->cr_ngroups))
			ip->i_mode &= ~ISGID;
	 }

	/*
	 * Now that uid and gid are set, look for a quota that applies to
	 * the new inode's owner.
	 */
	ip->i_dquot = qt_getinoquota(ip);

	/*
	 * Make sure the vnode's VENF_LOCKING flag corresponds with
	 * the inode's mode.
	 */
	if (MANDLOCK(vp, ip->i_mode))
		VN_FLAGSET(vp, VENF_LOCKING);
	else
		VN_FLAGCLR(vp, VENF_LOCKING);

	/*
	 * It is not necessary to push inode out synchronously, as
	 * its size is zero.  There is no way it can accidentally
	 * claim blocks it doesn't own, because inodes are written
	 * out synchronously when truncated.
	 */
	ip->i_flags |= IACC | IUPD | ICHG;
	return efs_iupdat(ip);
}

int
efs_ichange_type(struct inode *ip, u_short type)
{
	struct efs_dinode di;
	int error;

	SET_ITYPE(ip, type);
	if (error = efs_iformat(ip, 0, &di))
		return error;

	ip->i_flags |= IACC | IUPD | ICHG | IMOD;
	return efs_iupdat(ip);
}

/*
 * efs_iread:
 *	- read in from the disk an efs inode
 */
int
efs_iread(struct mount *mp, efs_ino_t ino, struct inode **ipp)
{
	register struct efs *fs;
	register struct buf *bp;
	struct efs_dinode *dp;
	register struct inode *ip;
	int error;

	fs = mtoefs(mp);
#ifdef	DEBUG
	if (ino > fs->fs_lastinum) {
		prdev("inumber %d too big", fs->fs_dev, ino);
		return EIO;
	}
#endif

	/*
	 * Convert inumber to buffer according to efs_itobp rules.
	 * Returned in dp is the pointer to the efs_dinode in the
	 * buffer.
	 */
	bp = efs_itobp(fs, ino, &dp);
	if (bp == NULL)
		return EIO;

	/*
	 * Allocate a fresh inode.
	 */
	ip = kmem_zone_zalloc(efs_inode_zone, KM_SLEEP);
	ip->i_gen = dp->di_gen;		/* copy gen number for all inodes */
	ip->i_number = ino;
	ip->i_dev = fs->fs_dev;

	/*
	 * Initialize attributes and extents unless unlinked.
	 */
	if (dp->di_mode == 0) {
		error = 0;
	} else {
		ip->i_mode = dp->di_mode;
		ip->i_nlink = dp->di_nlink;
		ip->i_uid = dp->di_uid;
		ip->i_gid = dp->di_gid;
		ip->i_size = dp->di_size;
		ip->i_remember = dp->di_size;
		ip->i_atime = dp->di_atime;
		ip->i_mtime = dp->di_mtime;
		ip->i_ctime = dp->di_ctime;
		if (error = efs_iformat(ip, dp->di_numextents, dp)) {
			efs_setcorrupt(mp);
			kmem_zone_free(efs_inode_zone, ip);
			ip = NULL;
			goto out;
		}

		if (doexlist_trash & 2)
			efs_inode_check(ip, "efs_iread");
		ip->i_version = dp->di_version;	/* AFS */
		if (ip->i_version == EFS_IVER_AFSSPEC ||
		    ip->i_version == EFS_IVER_AFSINO) {
			if (afsdptoipp) {
				(*afsdptoipp)(dp, ip);
			} else {
				ip = NULL;
				error = ENOPKG;
				goto out;
			}
		}
	}

	ASSERT(!(ip->i_nlink > 0 && ip->i_mode == 0));
	ASSERT(ip->i_nlink >= 0);

	bp->b_ref = EFS_INOREF;
out:
	brelse(bp);
	*ipp = ip;
	return error;
}

/*
 * efs_readindir:
 *	- read indirect extents from the filesystem for the given inode
 *	- return non-zero on error, zero if everythings ok
 */
int
efs_readindir(register struct inode *ip)
{
	register extent *ex, *iex;
	int ni, nib, nb, amount;
	register int i;
	register struct buf *bp;

	if (ip->i_numextents <= EFS_DIRECTEXTENTS) {
		ip->i_flags |= IINCORE;
		return 0;
	}

	/*
	 * Here we check and make sure that the number of indirect extents
	 * is set to something reasonable.  Older versions of the kernel
	 * only supported one indirect extent, and thus the field used
	 * above probably contained a zero.
	 */
	ex = ip->i_extents;
	ni = ex->ex_offset;
	if (ni == 0 || ni > EFS_DIRECTEXTENTS)
		ni = 1;			/* fix up old kernel mess */

	/*
	 * Indirect extents have been store where direct extents
	 * normally reside.   Copy the indirect extents elsewhere,
	 * then call bread to get the extents in core.
	 */
	nib = ip->i_directbytes;
	ASSERT(nib == ni * sizeof(extent));
	ip->i_indir = kmem_alloc(nib, KM_SLEEP);
	bcopy(ex, ip->i_indir, nib);

	nb = ip->i_numextents * sizeof(extent);
	irealloc(ip, nb);
	ex = ip->i_extents;
	iex = ip->i_indir;

	/*
	 * For each indirect extent, bread in the data and copy it into
	 * the incore extent region.  Note that after this loop completes,
	 * every indirect extent will have been examined and i_indirbytes
	 * will contain the total number of bytes of indirect extent space
	 * allocated.
	 */
	ip->i_indirbytes = 0;
	for (i = 0; i < ni; i++, iex++) {
		if (iex->ex_length == 0) {
			/*
			 * Indirect extent information in the file
			 * is messed up.  Make the file zero length
			 * so that the user can remove it.
			 */
			goto badfile;
		}
		bp = bread(ip->i_dev, (daddr_t)iex->ex_bn, (int)iex->ex_length);
		if (bp->b_flags & B_ERROR) {
			brelse(bp);
			return EIO;
		}
		amount = BBTOB(iex->ex_length);
		ip->i_indirbytes += amount;
		if (amount > nb)
			amount = nb;
		bcopy(bp->b_un.b_addr, ex, amount);
		brelse(bp);
		ex = (extent *) ((__psint_t)ex + amount);
		nb -= amount;
	}
	ip->i_numindirs = ni;

	ASSERT((ip->i_indirbytes % BBSIZE) == 0);

	/*
	 * If there are extra bytes left over, then something screwed up.
	 * Make the inode unusable forcing the user to fsck the filesystem.
	 */
	if (nb) {
		/*
		 * Inode has more extents than the indirect extents provide
		 * for.  Truncate the number of extents down to the amount
		 * provided for.  This way the user can at least recover
		 * part of the files contents.
		 */
		ip->i_numextents -= (nb / sizeof(extent));
		ASSERT(ip->i_numextents >= 0);
		ASSERT(ip->i_indirbytes >= ip->i_numextents*sizeof(extent));
		ip->i_flags |= ITRUNC;
		goto badout;
	}

	ip->i_flags |= IINCORE;
	ex = &ip->i_extents[ip->i_numextents - 1];
	ip->i_blocks = ex->ex_offset + ex->ex_length;
	if (doexlist_trash & 2)
		efs_inode_check(ip, "efs_readindir");

	return 0;

badfile:
	ip->i_size = 0;
	ip->i_remember = 0;
	ip->i_numextents = 0;
	irealloc(ip, 0);
	if (ip->i_indir)
		kmem_free(ip->i_indir, nib);
	ip->i_numindirs = 0;
	ip->i_indirbytes = 0;
	ip->i_indir = 0;

badout:
	ip->i_flags |= IUPD | ICHG;
	prdev("inode %d: indirect extent corruption", ip->i_dev, ip->i_number);
	efs_setcorrupt(itom(ip));
	return EIO;
}

static void efs_writeindir(struct inode *);


/*
 * efs_iupdat:
 *	- write out the given inode to the disk
 */
int
efs_iupdat(register struct inode *ip)
{
	register struct efs *fs;
	register int error, ne, nb;
	register struct buf *bp;
	register ushort flags;
	struct efs_dinode *dp;
	register extent *ex;
	timespec_t tv;

	IGETINFO.ig_iupdat++;
	fs = itoefs(ip);
#ifdef	DEBUG
	if (ip->i_number > fs->fs_lastinum) {
		prdev("inode number %d out of range", fs->fs_dev, ip->i_number);
		fs->fs_corrupted = 1;
		return EIO;
	}
#endif
	if (itovfs(ip)->vfs_flag & VFS_RDONLY) {
		error = (ip->i_flags & (IUPD|ICHG)) ? EROFS : 0;
#ifdef DEBUG
		if (error) {
			cmn_err(CE_WARN, "efs ip %0x%x num %d updated on a read-only file system",
				ip, ip->i_number);
			debug(0);
		}
#endif
		ip->i_flags &=
			~(INOUPDAT|IMOD|IACC|IUPD|ICHG|ISYN|INODELAY);
		return error;
	}

	/*
	 * Convert inumber to buffer according to efs_itobp rules.
	 * Returned in dp is the pointer to the efs_dinode in the
	 * buffer.
	 */
	bp = efs_itobp(fs, ip->i_number, &dp);
	if (bp == NULL)
		return EIO;

	ASSERT(ip->i_nlink >= 0);
	dp->di_nlink = ip->i_nlink;
	dp->di_uid = ip->i_uid;
	dp->di_gid = ip->i_gid;
	/*
	 * Only set the size in regular files up to what we
	 * know has been initialized on disk.  This is used
	 * to flush the inode occaisionally so we don't lose
	 * data without making uninitialized blocks a part of
	 * the inode.
	 */
	if ((ip->i_mode & IFMT) == IFREG)
		dp->di_size = ip->i_remember;
	else
		dp->di_size = ip->i_size;
	dp->di_gen = ip->i_gen;
	dp->di_version = ip->i_version;	/* AFS */

	/*
	 * We update the extents only if the file is NOT a named pipe,
	 * dev inode or socket.
	 * Named pipes have no addressing info, just time stamps.
	 */
	switch (ip->i_mode & IFMT) {
	  case IFIFO:
		/*
		 * Clear out unused portions of inode for these inode types.
		 * This repairs damage, if any existed.
		 */
		dp->di_numextents = 0;
		dp->di_size = 0;
		break;

	  case IFSOCK:
	  case IFCHR:
	  case IFBLK:
		efs_putdev(ip->i_rdev, &dp->di_u);
		dp->di_numextents = 0;
		dp->di_size = 0;
		break;

	  case 0:
		/*
		 * This inode has just been unlinked and freed.
		 */
		bzero(&dp->di_u, sizeof dp->di_u);
		break;

	  case IFCHRLNK:
	  case IFBLKLNK:
	  case IFLNK:
		if (ip->i_numextents == 0 &&
		    ip->i_directbytes <= EFS_MAX_INLINE) {
			dp->di_numextents = 0;
			bcopy(ip->i_extents, &dp->di_u.di_extents[0],
					ip->i_directbytes);
			nb = EFS_DIRECTEXTENTS * sizeof(extent) -
					ip->i_directbytes;
			if (nb > 0) {
				bzero((char *)&dp->di_u.di_extents[0] +
				      ip->i_directbytes, nb);
			}
			break;
		}

	  default:
		/*
		 * Copy out extents into inode, if they fit inside it.
		 * Otherwise write the extents to a separate location on
		 * the disk.  Because the system may have pre-extended the
		 * inode we truncate the last extent to fit within the
		 * file's i_size, to keep the on-disk inode consistent.
		 */
		ASSERT(ip->i_numextents >= 0);
		if ((ne = ip->i_numextents) && (ip->i_flags & IINCORE)) {
			ex = &ip->i_extents[ne-1];
			ip->i_blocks = ex->ex_offset + ex->ex_length;
		}
		dp->di_numextents = ne;
		if (ne == 0)
			break;
		if (doexlist_trash & 2)
			efs_inode_check(ip, "efs_iupdat");
		if (ne <= EFS_DIRECTEXTENTS) {
			ASSERT(ip->i_flags & IINCORE);
			ASSERT(ip->i_directbytes == ne * sizeof(extent));
			nb = ip->i_directbytes;
			bcopy(ip->i_extents, &dp->di_u.di_extents[0], nb);
			/*
			 * See if the last extent in the copy needs to
			 * be trimmed.
			 */
			ex = &dp->di_u.di_extents[ne-1];
			if (ex->ex_offset + ex->ex_length > BTOBB(ip->i_size)) {
				ex->ex_length =
					BTOBB(ip->i_size) - ex->ex_offset;
			}
		} else {
			/*
			 * If i_numindirs == 0, indirect extents haven't
			 * been read into core yet.
			 */
			ne = ip->i_numindirs;
			if (ne == 0)
				break;	/* nothing more to do */

			ASSERT(ip->i_flags & IINCORE);
			nb = ne * sizeof(extent);
			bcopy(ip->i_indir, &dp->di_u.di_extents[0], nb);
			dp->di_u.di_extents[0].ex_offset = ne;
			efs_writeindir(ip);
		}

		/* clear out unused extents in disk inode */
		nb = EFS_DIRECTEXTENTS * sizeof(extent) - nb;
		if (nb > 0)
			bzero(&dp->di_u.di_extents[ne], nb);
		break;
	}

	/* copy out AFS info */
	if (ip->i_afs)
		(*afsiptodpp)(ip, dp);

	/*
	 * Update mode, time stamps, and generation number.
	 */
	ASSERT(!(ip->i_nlink > 0 && ip->i_mode == 0));

	dp->di_mode = ip->i_mode;

	flags = ip->i_flags;
	if (ip->i_updtimes) {
		ip->i_updtimes = 0;
		flags |= IMOD;
	}
	if ((flags & (IMOD|IACC|ICHG|IUPD)) == 0)
		IGETINFO.ig_iupunk++;
	else {
		if (flags & (IACC|ICHG|IUPD))
			nanotime_syscall(&tv);

		if (flags & IACC) {
			IGETINFO.ig_iupacc++;
			ip->i_atime = tv.tv_sec;
		}
		dp->di_atime = ip->i_atime;
		if (flags & IUPD) {
			IGETINFO.ig_iupupd++;
			ip->i_mtime = tv.tv_sec;
			ip->i_umtime = tv.tv_nsec;
		}
		dp->di_mtime = ip->i_mtime;
		if (flags & ICHG) {
			IGETINFO.ig_iupchg++;
			ip->i_ctime = tv.tv_sec;
		}
		dp->di_ctime = ip->i_ctime;
		if (flags & IMOD)
			IGETINFO.ig_iupmod++;
	}

	bp->b_relse = efs_asynchk;  /* notify errors */

	if (flags & ISYN)
		bwrite(bp);
	else if (flags & INODELAY)
		bawrite(bp);
	else
		bdwrite(bp);

	ip->i_flags &= ~(INOUPDAT|IMOD|IACC|IUPD|ICHG|ISYN|INODELAY);
	/*
	 * If this is a regular file and i_remember is still less
	 * than i_size, then reset IMOD so that it will be flushed
	 * again later.
	 */
	if (((ip->i_mode & IFMT) == IFREG) && (ip->i_remember < ip->i_size)) {
		ip->i_flags |= IMOD;
	}
	return 0;
}

/*
 * efs_writeindir:
 *	- write to disk the indirect extents for inode ip
 */
static void
efs_writeindir(register struct inode *ip)
{
	register extent *iex, *ex, *copy;
	register struct buf *bp;
	register int ni, nb, amount;

	ASSERT(ip->i_flags & IINCORE);

	iex = ip->i_indir;
	ex = ip->i_extents;
	nb = ip->i_directbytes;
	for (ni = ip->i_numindirs; --ni >= 0; iex++) {
		bp = getblk(ip->i_dev, (daddr_t)iex->ex_bn,
				       (int)iex->ex_length);
		amount = BBTOB(iex->ex_length);
		if (amount > nb)
			amount = nb;
		bcopy(ex, bp->b_un.b_addr, amount);
		/*
		 * See if last extent needs to be trimmed.
		 * XXX Disallows preallocated blocks to persist
		 * XXX beyond final close!
		 */
		if (ni == 0 && amount) {
			copy = (extent *)(bp->b_un.b_addr+amount-sizeof *copy);
			if (copy->ex_offset+copy->ex_length > BTOBB(ip->i_size))
				copy->ex_length =
					BTOBB(ip->i_size) - copy->ex_offset;
		}
		bp->b_relse = efs_asynchk;  /* notify errors */

		if (ip->i_flags & ISYN)
			bwrite(bp);
		else if (ip->i_flags & INODELAY)
			bawrite(bp);
		else
			bdwrite(bp);

		ex = (extent *) ((caddr_t)ex + amount);
		nb -= amount;
	}

	if (nb < 0) {
		prdev("inode %d: indirect extent corruption",
		      ip->i_dev, ip->i_number);
		efs_setcorrupt(itom(ip));
	}
}

/*
 * Free extra disk blocks associated with the inode structure
 * The 'truncated' argument tells the caller whether we really did anything
 * or not.
 */
int
efs_itrunc(register struct inode *ip, scoff_t newsize, int unlink)
{
	int type;
	register int i;
	register extent *ex;
	register int newsizebb;
	register int numextents;
	register int offset;
	register struct buf *bp;
	int newlen;
	int oldlen;
	extent tempex;
	int numindirs;
	int error;

	ASSERT(mutex_mine(&ip->i_lock));

	/*
	 * There are three reasons to not truncate a file:
	 *	(1) "truncation" would lengthen the file	NOT
	 *	(2) the inode doesn't need truncation, because it has not
	 *	    been grown.
	 *	(3) the file is not the sort to be truncated
	 */
	type = (ip->i_mode & IFMT);
	if ((type != IFREG && type != IFDIR && !ISLINK(type))
	  || (((ip->i_flags & ITRUNC) == 0) && newsize == ip->i_size)) {
		if (unlink)
			goto chkunlink;
		return 0;
	}

	if ((ip->i_flags & IINCORE) == 0) {
		if (error = efs_readindir(ip))
			return error;
		ASSERT(ip->i_flags & IINCORE);
	}

	if (newsize < ip->i_size)
		ip->i_flags |= IUPD;
	else

	/* It would be fairly simple to just set i_size here (after
	 * zeroing preallocated blocks) but too many system utilities
	 * (like dump, fsck and fsr) treat i_size > blocks as an error.
	 * So grow the file here.
	 */
	if (newsize > ip->i_size) {
		char c = '\0';
		struct uio uio;
		struct iovec iov;

		iov.iov_base = &c;
		iov.iov_len = 1;
		uio.uio_iov = &iov;
		uio.uio_iovcnt = 1;

		uio.uio_limit = getfsizelimit();
		uio.uio_segflg = UIO_SYSSPACE;
		uio.uio_offset = newsize - 1;
		uio.uio_resid = 1;
		uio.uio_pmp = NULL;
		uio.uio_sigpipe = 0;
		uio.uio_pio = 0;
		uio.uio_readiolog = 0;
		uio.uio_writeiolog = 0;
		uio.uio_pbuf = 0;
		VOP_WRITE(itov(ip), &uio, 0, get_current_cred(),
				&curuthread->ut_flid, error);
		if (error) {
			ASSERT(uio.uio_sigpipe == 0);
			return error;
		}
	}

	IGETINFO.ig_truncs++;

	numextents = ip->i_numextents;
	ex = &ip->i_extents[numextents - 1];

	if (numextents > 0 && type == IFREG) {
		/*	Remove pages beyond page containing newsize
		 *	from page cache.
		 *	Pages that map beyond ip->i_size could have
		 *	been placed in the page cache via efs_clrbuf().
		 */
		VOP_TOSS_PAGES(itov(ip), newsize, BBTOB(ex->ex_offset+ex->ex_length) - 1,
			FI_REMAPF);
	}

	ip->i_size = newsize;
	if (newsize < ip->i_remember)
		ip->i_remember = newsize;

	/* Loop through each extent in the file, releasing those that are
	 * past the new end of file.  Start at the last extent.  As soon
	 * ex_offset is less than the new size, we can stop looking.
	 */
	newsizebb = BTOBB(newsize);		/* round up */
	tempex.ex_offset = 0;			/* loop invariant */
	for (i = numextents; --i >= 0; ex--) {
		if (ex->ex_offset >= newsizebb) {

			/*	Free entire extent.
			 */
			efs_free(ip, ex, (type == IFREG) ? 0 : ex->ex_length);
			ip->i_numextents--;
			continue;
		}
		if (ex->ex_offset + ex->ex_length <= newsizebb)
			break;

		/*	Free partial extent.
		 */
		newlen = newsizebb - ex->ex_offset;
		tempex.ex_bn = ex->ex_bn + newlen;
		tempex.ex_length = ex->ex_length - newlen;
		tempex.ex_offset = ex->ex_offset + newlen;
		efs_free(ip, &tempex, (type == IFREG) ? 0 : ex->ex_length);
		ex->ex_length = newlen;

		if (type != IFREG && (newsize &= BBMASK)) {
			/*
			 * Clear any portion of the BB that will remain.
			 */
			bp = bread(ip->i_dev, ex->ex_bn + newlen - 1, 1);
			if (bp->b_error == 0) {
				bzero(bp->b_un.b_addr+newsize, BBSIZE-newsize);
				bdwrite(bp);
			} else
				brelse(bp);
		}
	}

	/*
	 * Loop through the indirect extents, if any, and reduce those also.
	 * We have to handle the case where the file shrinks from indirect
	 * extents back in to direct extents.  When the dust settles,
	 * realloc the direct and indirect extent memory so as to avoid
	 * wastage.
	 */
	if (numindirs = ip->i_numindirs) {
		if (ip->i_numextents > EFS_DIRECTEXTENTS) {
			newsizebb = BTOBB(ip->i_numextents * sizeof(extent));
		} else {
			/*
			 * File is losing its indirect extents.  Truncate the
			 * indirect extents away
			 */
			newsizebb = 0;
		}
		ex = &ip->i_indir[0];
		offset = 0;
		for (i = numindirs; --i >= 0; ex++) {
			oldlen = ex->ex_length;
			if (offset >= newsizebb) {
				/*
				 * Free entire indirect extent.
				 */
				efs_free(ip, ex, oldlen);
				ip->i_indirbytes -= BBTOB(oldlen);
				ip->i_numindirs--;
			} else
			if (offset + oldlen > newsizebb) {
				/*
				 * Free partial indirect extent.
				 */
				newlen = newsizebb - offset;
				tempex.ex_bn = ex->ex_bn + newlen;
				tempex.ex_length = oldlen - newlen;
				ex->ex_length = newlen;
				efs_free(ip, &tempex, oldlen);
				ip->i_indirbytes -= BBTOB(tempex.ex_length);
			}
			offset += oldlen;
		}
	} else {
		ASSERT(ip->i_indirbytes == 0);
	}

	/*
	 * Adjust the memory allocated for the indirect extents.
	 */
	if (ip->i_numindirs == 0) {
		/*
		 * Indirect extent disk block allocation could have
		 * failed after i_indir was allocated in memory.
		 */
		if (ip->i_indir != 0) {
			/*
			 * File no longer has indirect extents.
			 * Free the memory.
			 */
			kmem_free(ip->i_indir, sizeof(extent));
			ip->i_indir = 0;
			ip->i_indirbytes = 0;
		}
	} else {
		ASSERT(ip->i_numextents > EFS_DIRECTEXTENTS);
		ASSERT(ip->i_indirbytes >= ip->i_numextents*sizeof(extent));
		if (numindirs != ip->i_numindirs) {
			ip->i_indir = kmem_realloc(ip->i_indir,
					    ip->i_numindirs * sizeof(extent),
					    KM_SLEEP);
		}
	}

	/*
	 * Adjust the memory allocated for the direct extents
	 * and update.
	 */
	ip->i_flags &= ~ITRUNC;
	ip->i_flags |= ICHG;

	if (numextents != ip->i_numextents)
		irealloc(ip, ip->i_numextents * sizeof(extent));

	/*
	 * Write inode out to disk immediately if some blocks have been
	 * freed that are currently accounted for on the disk inode.
	 *
	 * XXX There should be a way to guarantee that the inode doesn't
	 * XXX get sorted and pushed *after* a different inode, which has
	 * XXX claimed blocks this inode has released, gets pushed.
	 */
	if (i = ip->i_numextents) {
		ex = &ip->i_extents[i-1];
		i = ex->ex_offset + ex->ex_length;
	}
	if (ip->i_blocks > i)
		ip->i_flags |= INODELAY;

chkunlink:
	if (unlink) {
		/* if there were any file blocks asociated with this file
		 * then the above code would have set the NODELAY flag.
		 * Otherwise we don't really care when this inode goes out
		 */
		ip->i_gen++;
		ip->i_flags |= IUPD;
		ip->i_mode = 0;
	}
	/* XXXjwag - ordering issues */
	return efs_iupdat(ip);
}

int
efs_iaccess(struct inode *ip, mode_t mode, struct cred *cr)
{
	mode_t orgmode = mode;

	if ((mode & IWRITE) && !WRITEALLOWED(itov(ip), cr))
		return EROFS;
	if (cr->cr_uid != ip->i_uid) {
		mode >>= 3;
		if (!groupmember(ip->i_gid, cr))
			mode >>= 3;
	}
	if ((ip->i_mode & mode) == mode)
		return 0;
	if (((orgmode & IWRITE) && !cap_able_cred(cr, CAP_DAC_WRITE)) ||
	    ((orgmode & IREAD) && !cap_able_cred(cr, CAP_DAC_READ_SEARCH)) ||
	    ((orgmode & IEXEC) && !cap_able_cred(cr, CAP_DAC_EXECUTE)))
		return EACCES;
	return 0;
}

void
efs_inode_check(struct inode *ip, char *func)
{
	int i;

	if (!ip->i_numextents || !ip->i_extents || !(ip->i_flags & IINCORE))
		return;
	for (i = 0; i < ip->i_numextents; i++) {
		if (ip->i_extents[i].ex_length > 0 &&
		    ip->i_extents[i].ex_bn > 0)
			continue;
		prdev("%s: zero extent length/block I=%d num=%d offset=%d", ip->i_dev, func, ip->i_number, i, ip->i_extents[i].ex_offset);
		cmn_err(CE_PANIC, "%s: exlist trash", func);
	}
}

int
efs_whichzone(int nex)
{
	static int inited;
	static char x[513];

	if (!inited) {
		int i = 0;

		while (i <= 8)
			x[i++] = 0;
		while (i <= 16)
			x[i++] = 1;
		while (i <= 32)
			x[i++] = 2;
		while (i <= 64)
			x[i++] = 3;
		while (i <= 128)
			x[i++] = 4;
		while (i <= 256)
			x[i++] = 5;
		while (i <= 512)
			x[i++] = 6;
		inited = 1;
	}
	if (nex <= 512)
		return x[nex];
	else
		return 7;
}

/*
 * To avoid a lot of kmem_realloc-ing when a file is being grown,
 * we'll allocate incore extent space in clumps.  The size of the
 * inline inode extent list is a convenient clump size (currently 4).
 */
#define	EROUNDUP(X) ((((X) + sizeof ((struct inode *)0)->i_direct - 1) / \
				sizeof ((struct inode *)0)->i_direct) * \
			sizeof ((struct inode *)0)->i_direct)

extent *
efs_alloc_extent(int nex)
{
	int nz;

	if (!(doexlist_trash & 8) || !efs_extent_zones[nz = efs_whichzone(nex)])
		return kmem_alloc(EROUNDUP(nex * sizeof(extent)), KM_SLEEP);
	else
		return kmem_zone_alloc(efs_extent_zones[nz], KM_SLEEP);
}

extent *
efs_realloc_extent(extent *oexp, int oex, int nex)
{
	int oz, nz, nnb;
	extent *nexp;

	if (!(doexlist_trash & 8) || 
	    (((oz = efs_whichzone(oex)) == (nz = efs_whichzone(nex))) &&
	     nz == 7)) {
		nnb = EROUNDUP(nex * sizeof(extent));
		if (EROUNDUP(oex * sizeof(extent)) == nnb)
			nexp = oexp;
		else
			nexp = kmem_realloc(oexp, nnb, KM_SLEEP);
	} else if (oz == nz)
		nexp = oexp;
	else {
		nexp = efs_alloc_extent(nex);
		bcopy(oexp, nexp, MIN(oex, nex) * sizeof(extent));
		efs_free_extent(oexp, oex);
	}
	return nexp;
}

void
efs_free_extent(extent *exp, int nex)
{
	int nz;

	if (!(doexlist_trash & 8) || !efs_extent_zones[nz = efs_whichzone(nex)])
		kmem_free(exp, EROUNDUP(nex * sizeof(extent)));
	else
		kmem_zone_free(efs_extent_zones[nz], exp);
}
