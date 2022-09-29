/**************************************************************************
 *									  *
 * 		 Copyright (C) 1987, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"$Revision: 3.75 $"

/*
 * EFS directory implementation.
 */
#include <values.h>
#include <string.h>
#include <sys/types.h>
#include <sys/buf.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/dirent.h>
#include <sys/errno.h>
#include <sys/kabi.h>
#include <sys/kmem.h>
#include <sys/param.h>
#include <sys/pathname.h>
#include <sys/sysinfo.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/vnode.h>
#include <sys/fcntl.h>
#include "efs_inode.h"
#include "efs_dir.h"
#include "efs_fs.h"
#include "efs_sb.h"		/* just for efs_setcorrupt prototype */

/*
 * Stream abstraction for reading directory blocks and scanning entries
 * within each block.
 */
struct dirstream {
	struct inode	*dir;		/* directory inode */
	scoff_t		count;		/* bytes yet to be read in directory */
	scoff_t		curoffset;	/* file offset to current dirblk */
	scoff_t		nextoffset;	/* file offset to next dirblk */
	struct buf	*buffer;	/* directory block buffer */
	scoff_t		bufresid;	/* bytes left in block buffer */
	struct efs_dirblk *dirblk;	/* pointer to dirblk */
	short		curslot;	/* current slot # in dirblk */
	short		nextslot;	/* next slot # in dirblk */
	short		error;		/* stream status */
	struct cred	*cred;		/* user credentials */
};

/* Open/initialize a directory stream */
#define	OPENDIR(dp, cnt, cookie, cr, dsp) { \
	(dsp)->dir = (dp); \
	(dsp)->count = (cnt); \
	(dsp)->curoffset = (dsp)->nextoffset = EFS_DBOFF(cookie); \
	(dsp)->buffer = NULL; \
	(dsp)->bufresid = 0; \
	(dsp)->dirblk = NULL; \
	(dsp)->curslot = (dsp)->nextslot = EFS_SLOT(cookie); \
	(dsp)->error = 0; \
	(dsp)->cred = (cr); \
}

#define	CLOSEDIR(dsp) \
	if ((dsp)->buffer != NULL) (void) brelse((dsp)->buffer)

static void
dircorrupted(struct inode *dp, char *message, char *name)
{
	static dev_t		dev;
	static efs_ino_t	inum;
	static char		*msg;
#define PRDEV_MAXNAME	80

	if (dev == dp->i_dev && inum == dp->i_number && msg == message)
		return;
	efs_setcorrupt(itom(dp));

	/* protect against stack overflow for limited stack buffer in prdev */
	if(strlen(name) > PRDEV_MAXNAME) {
		char *pbuf;

		if (pbuf = kmem_alloc (PRDEV_MAXNAME, KM_NOSLEEP)) {
			strncpy(pbuf, name, PRDEV_MAXNAME);
			pbuf[PRDEV_MAXNAME-1] = '\0';
			prdev("Directory %d is corrupted (%s%s)", dp->i_dev,
				dp->i_number, message, pbuf);
			kmem_free (pbuf, PRDEV_MAXNAME);
		} else {
			pbuf = "";	
			prdev("Directory %d is corrupted (%s%s)", dp->i_dev,
				dp->i_number, message, pbuf);
		}
	} else
		prdev("Directory %d is corrupted (%s%s)", dp->i_dev,
			dp->i_number, message, name);
	dev = dp->i_dev;
	inum = dp->i_number;
	msg = message;
}

/*
 * Read in a directory block.  Using dsp->nextoffset, read in the given
 * directory block.  Return non-zero for success, zero for failure.
 * XXX should the system repair damage, or just report it?
 */
static int
getdirblk(register struct dirstream *dsp)
{
	register struct inode *dp;
	register struct buf *bp;
	register struct efs_dirblk *db;
	struct bmapval bmv[2];
	int nmapvals;
	char *thebadnews;

	dp = dsp->dir;
	if (dp->i_size & EFS_DIRBMASK) {
		thebadnews = "operation was: getdirblk, i_size";
		goto badnews;
	}

	if (dsp->buffer) {
		/* see if there are more dirblk's in the current buffer */
		ASSERT((dsp->bufresid & EFS_DIRBMASK) == 0);
		if (dsp->bufresid != 0) {
			/* yup */
			dsp->bufresid -= EFS_DIRBSIZE;
			dsp->dirblk = (struct efs_dirblk *)
				((caddr_t)dsp->dirblk + EFS_DIRBSIZE);
			dsp->curslot = 0;
			dsp->nextslot = 0;
			dsp->curoffset = dsp->nextoffset;
			dsp->nextoffset += EFS_DIRBSIZE;
			return (1);
		}
		brelse(dsp->buffer);
		dsp->buffer = 0;
	}

	/* translate offset into a buffer mapping */
	nmapvals = 2;
	dsp->error = efs_bmap(itobhv(dp), dsp->nextoffset, 4096, B_READ,
			      dsp->cred, bmv, &nmapvals);
	
	if (dsp->error)
		return 0;

	if (bmv[0].length == 0) {
		/* end of file */
		dsp->error = ENOENT;
		return (0);
	}

	/* read in buffer */
	SYSINFO.dirblk += bmv[0].length;
	if (nmapvals > 1) {
		bp = breada(dp->i_dev, bmv[0].bn, (int) bmv[0].length,
			    bmv[1].bn, (int) bmv[1].length);
	} else {
		bp = bread(dp->i_dev, bmv[0].bn, (int) bmv[0].length);
	}
	if (bp->b_flags & B_ERROR) {
		brelse(bp);
		dsp->error = EIO;
		return (0);
	}
	bp->b_ref = EFS_DIRREF;
	dsp->buffer = bp;
	dsp->dirblk = (struct efs_dirblk *)(bp->b_un.b_addr + bmv[0].pboff);

	/*
	 * Do some simple checks on the state of the dirblk to see if its
	 * okay to use.
	 */
	db = dsp->dirblk;
	if (db->magic != EFS_DIRBLK_MAGIC) {
		thebadnews = "operation was: getdirblk, magic";
		goto badnews;
	}
	if (db->slots > EFS_MAXENTS) {
		thebadnews = "operation was: getdirblk, slots";
		goto badnews;
	}
	if (EFS_DIR_FREEOFF(db->firstused) <
			    (EFS_DIRBLK_HEADERSIZE + db->slots)) {
		thebadnews = "operation was: getdirblk, 1stused";
		goto badnews;
	}

	/* now setup dirstream components */
	ASSERT((bmv[0].pboff & EFS_DIRBMASK) == 0);
	ASSERT((bmv[0].pbsize & EFS_DIRBMASK) == 0);
	dsp->bufresid = bmv[0].pbsize - EFS_DIRBSIZE;
	dsp->curoffset = dsp->nextoffset;
	dsp->nextoffset += EFS_DIRBSIZE;
	return (1);

badnews:
	/* directory is messed up */
	dircorrupted(dp, thebadnews, "");
	dsp->error = ENOENT;
	if (dsp->buffer) {
		brelse(dsp->buffer);
		dsp->buffer = 0;			/*XXX*/
	}
	return (0);
}

/*
 * Update a directory that has been written in.
 */
static int
putdirblk(register struct dirstream *dsp)
{
	dsp->buffer->b_relse = efs_asynchk;	/* so errors will be noticed! */
	bwrite(dsp->buffer);
	dsp->dir->i_flags |= IUPD|ICHG;
	IGETINFO.ig_dirupd++;
	dsp->error = efs_iupdat(dsp->dir);
	return (dsp->error);
}

/*
 * Grow a directory by appending a new block.
 */
static int
growdir(register struct dirstream *dsp)
{
	register struct inode *dp;
	register struct buf *bp;
	register struct efs_dirblk *db;
	struct bmapval bmv;
	int nmapvals = 1;

	dp = dsp->dir;
	/* make sure directory is a multiple of a dirblk in size */
	if (dp->i_size & EFS_DIRBMASK) {
		dsp->error = EIO;
		dircorrupted(dp, "operation was: growdir, i_size", "");
		return (0);
	}
	if (bp = dsp->buffer) {
		brelse(bp);
		dsp->buffer = 0;
	}
	if (dsp->error = efs_bmap(itobhv(dp), dp->i_size, EFS_DIRBSIZE, B_WRITE,
				  dsp->cred, &bmv, &nmapvals)) {
		return (0);
	}
	dp->i_size += EFS_DIRBSIZE;
	bp = bread(dp->i_dev, bmv.bn, (int) bmv.length);
	if (bp->b_flags & B_ERROR) {
		brelse(bp);
		dsp->error = EIO;
		return (0);
	}
	bp->b_ref = EFS_DIRREF;
	bzero(bp->b_un.b_addr + bmv.pboff, bmv.pbsize);

	/* fill in dsp fields */
	dsp->buffer = bp;
	dsp->dirblk = (struct efs_dirblk *)(bp->b_un.b_addr + bmv.pboff);
	dsp->curslot = 0;
	dsp->nextslot = 0;
	dsp->curoffset = dsp->nextoffset = dp->i_size;

	/* initialize dirblk */
	ASSERT((bmv.pboff & EFS_DIRBMASK) == 0);
	ASSERT((bmv.pbsize & EFS_DIRBMASK) == 0);
	db = dsp->dirblk;
	db->magic = EFS_DIRBLK_MAGIC;
	db->slots = 0;
	db->firstused = 0;
	return (1);
}

/*
 * Get a directory entry from the directory stream "dsp".  Special handling
 * is done here for returning freespace entries.  When a freespace entry
 * is returned, dsp->curslot == EFS_FREESLOT and the dep pointer is non NULL,
 * but not valid as an efs_dent pointer.
 */
static struct efs_dent *
getdent(register struct dirstream *dsp)
{
	register struct efs_dirblk *db;
	int slotoffset;

	ASSERT((dsp->count & EFS_DIRBMASK) == 0);
	while (dsp->count > 0) {
		if ((dsp->buffer == NULL) || (dsp->curslot == EFS_FREESLOT)) {
			/*
			 * Get a dirblk if we don't have one, or if we have
			 * reached the end of the current one.
			 */
			if (dsp->nextoffset >= dsp->dir->i_size) {
				/* wrap offset rotor */
				dsp->curoffset = 0;
				dsp->nextoffset = 0;
				dsp->curslot = 0;
				dsp->nextslot = 0;
				/*
				 * Insure a clean start once we wrap by
				 * releasing the buffer now.
				 */
				if (dsp->buffer) {
					brelse(dsp->buffer);
					dsp->buffer = 0;
				}
			}
			if (dsp->curslot == EFS_FREESLOT) {
				/* reset slots for new dirblk */
				dsp->curslot = 0;
				dsp->nextslot = 0;
			}
			if (!getdirblk(dsp)) {
				/* bad news */
				break;
			}
		}

		db = dsp->dirblk;
		if (dsp->nextslot >= db->slots) {
			/* ran off dirblk.  return free space */
			dsp->curslot = EFS_FREESLOT;
			dsp->count -= EFS_DIRBSIZE;	/* gobble dirblk */
			return (struct efs_dent *) db;
		}
		dsp->curslot = dsp->nextslot;
		dsp->nextslot++;
		if ((slotoffset = EFS_SLOTAT(db, dsp->curslot)) == 0) {
			/* skip empty slots */
			continue;
		}
		return EFS_SLOTOFF_TO_DEP(db, slotoffset);
	}
	return NULL;					/* EOF */
}

/*
 * Test the sticky attribute of a directory.  We can unlink from a sticky
 * directory that's writable by us if: we are superuser, we own the file,
 * we own the directory, or the file is writable.
 */
static int
efs_stickytest(struct inode *dp, struct inode *ip, struct cred *cr)
{
	extern int	xpg4_sticky_dir;

	if ((dp->i_mode & ISVTX)
	    && cr->cr_uid != ip->i_uid
	    && cr->cr_uid != dp->i_uid
	    && !cap_able_cred(cr, CAP_DAC_WRITE)) {
		if (xpg4_sticky_dir) {
			return EACCES;
		} else {
			return efs_iaccess(ip, IWRITE, cr);
		}
	}
	return 0;
}

/*
 * Lookup name in directory dp.  Return via ep the inumber and offset cookie
 * for the entry, if found.  If not found, return a zero inum and the offset
 * cookie for a free spot in the directory capable of holding name.
 */
int					/* 0 or an error number */
efs_dirlookup(
	register struct inode *dp,	/* pointer to directory inode */
	register char *name,		/* name which we seek in dp */
	struct pathname *pnp,		/* name length and hash value */
	register int flags,		/* directory lookup flags */
	struct entry *ep,		/* lookup results */
	struct cred *cr)
{
	unsigned int namlen, wantsize;
	int error, isdotdot;
	struct vnode *vp;
	struct inode *ip;
	scoff_t cookie, freespot;
	struct dirstream dirstream;
	struct efs_dent *dep;
	bhv_desc_t *bdp;

	ASSERT(mutex_mine(&dp->i_lock));

	if (error = efs_iaccess(dp, IEXEC, cr))
		return error;

	/*
	 * Handle degenerate pathname component.
	 */
	if (*name == '\0') {
		ASSERT((flags & (DLF_ENTER|DLF_REMOVE)) == 0);
		ihold(dp);
		ep->e_name = name;
		ep->e_namlen = 0;
		ep->e_offset = -1;
		ep->e_inum = dp->i_number;
		ep->e_ip = dp;
		return 0;
	}

	/*
	 * Check permissions if we're looking up to enter or remove.
	 */
	if (flags & (DLF_ENTER|DLF_REMOVE)) {
		if (error = efs_iaccess(dp, IWRITE, cr))
			return error;
	}

	/*
	 * Try the directory name lookup cache.
	 */
	if (bdp = dnlc_lookup_fast(itov(dp), name, pnp, &ep->e_fastdata,
				   NOCRED, 0)) {

		vp = BHV_TO_VNODE(bdp);
		if (flags & DLF_EXCL) {
			VN_RELE(vp);
			return EEXIST;
		}

		ip = bhvtoi(bdp);
		ep->e_inum = ip->i_number;
		if ((flags & DLF_IGET) == 0) {
			VN_RELE(vp);
			ep->e_ip = NULL;
		} else {
			if (ip != dp) {
				isdotdot = (ep->e_flags & PN_ISDOTDOT);
				if (isdotdot)
					iunlock(dp);
				ilock(ip);
				if (isdotdot) {
					ilock(dp);
					/*
					 * Was dp removed while igetting ip?
					 */
					if (dp->i_nlink <= 0) {
						iunlock(ip);
						return ENOENT;
					}
				}
				if ((flags & DLF_REMOVE)
				    && (error = efs_stickytest(dp, ip, cr))) {
					iput(ip);
					return(error);
				}
			}
			ep->e_ip = ip;
		}
		return 0;
	}

	/*
	 * Check name length and compute allocation size.
	 */
	namlen = ep->e_namlen;
	if (namlen > EFS_MAXNAMELEN)
		namlen = EFS_MAXNAMELEN;
	wantsize = efs_dentsizebynamelen(namlen);

	cookie = 0;

	OPENDIR(dp, dp->i_size, cookie, cr, &dirstream);

	if (!getdirblk(&dirstream))
		return(dirstream.error);

	if (EFS_SLOT(cookie) >= dirstream.dirblk->slots) {
		/*
		 * Slot contained in cookie is junk.  Fix up dirstream
		 * to start at slot zero.
		 */
		dirstream.curslot = 0;
		dirstream.nextslot = 0;
	}

	/* scan directory, component by component */
	freespot = dp->i_size;
	while ((dep = getdent(&dirstream)) != NULL) {
		if (dirstream.curslot == EFS_FREESLOT) {
			if (dirstream.curoffset < freespot
			    && EFS_DIR_FREESPACE(dirstream.dirblk) > wantsize) {
				freespot = dirstream.curoffset;
			}
			continue;
		}
		if (EFS_INUM_ISZERO(dep)) {
			dircorrupted(dp, "operation was: lookup ", name);
			CLOSEDIR(&dirstream);
			return EIO;
		}

		/* compare names looking for a match */
		if (namlen == dep->d_namelen
		    && !bcmp(name, dep->d_name, namlen)) {
			ep->e_offset = EFS_MKCOOKIE(dirstream.curoffset,
						    dirstream.curslot);
			ep->e_inum = EFS_GET_INUM(dep);
			CLOSEDIR(&dirstream);
			if (flags & DLF_EXCL)
				return EEXIST;
			if ((flags & DLF_IGET) == 0) {
				ep->e_ip = NULL;
			} else if (ep->e_inum == dp->i_number) {
				ihold(dp);
				ep->e_ip = dp;
			} else {
				ip = NULL;
				isdotdot = (ep->e_flags & PN_ISDOTDOT);
				if (isdotdot)
					iunlock(dp);
				error = iget(dp->i_mount, ep->e_inum, &ip);
				if (isdotdot) {
					ilock(dp);
					/*
					 * Was dp removed while igetting ip?
					 */
					if (dp->i_nlink <= 0) {
						if (ip)
							iput(ip);
						return ENOENT;
					}
				}
				if (!error && ip->i_nlink <= 0) {
					dircorrupted(dp, "unallocated inode ",
						name);
					error = ENOENT;
				}
				if (!error && (flags & DLF_REMOVE))
					error = efs_stickytest(dp, ip, cr);
				if (error) {
					if (ip)
						iput(ip);
					return error;
				}
				ep->e_ip = ip;
			}
			if (ip = ep->e_ip) {
				dnlc_enter_fast(itov(dp), &ep->e_fastdata,
						itobhv(ip), NOCRED);
			}
			return 0;
		}
	}

	/*
	 * Name was not found.  Return cookie describing free space.
	 * XXX make a not-found entry in the name cache
	 */
	CLOSEDIR(&dirstream);
	if (flags & DLF_MUSTHAVE)
		return ENOENT;
	ep->e_offset = EFS_MKCOOKIE(freespot, EFS_FREESLOT);
	ep->e_inum = 0;
	ep->e_ip = NULL;
	return dirstream.error;
}

/*
 * Enter a new name or rename an existing name in a directory.  If the
 * name already exists, then just rewrite the inumber.
 */
int					/* 0 or an error number */
efs_direnter(
	register struct inode *dp,	/* locked directory inode */
	struct inode *ip,		/* new entry's inode */
	struct entry *ep,		/* name, inumber, and offset */
	struct cred *cr)
{
	unsigned int namlen, dentsize;
	int slot, error;
	struct dirstream dirstream;
	register scoff_t dirblkoff;
	register struct efs_dent *dep;
	register struct efs_dirblk *db;

	ASSERT(mutex_mine(&dp->i_lock));
	ASSERT(ep->e_offset != -1);

	ep->e_inum = ip->i_number;
	ep->e_ip = ip;
	namlen = MIN(ep->e_namlen, EFS_MAXNAMELEN);
	dentsize = efs_dentsizebynamelen(namlen);

	OPENDIR(dp, EFS_DIRBSIZE, ep->e_offset, cr, &dirstream);
	if (EFS_DBOFF(ep->e_offset) >= dp->i_size) {
		/* adding new data to the file.  grow it now */
		if (!growdir(&dirstream))
			return dirstream.error;
	} else {
		if (!getdirblk(&dirstream))
			return dirstream.error;
	}

	db = dirstream.dirblk;
	slot = EFS_SLOT(ep->e_offset);
	ASSERT(slot == EFS_FREESLOT);
	ASSERT((dentsize & 1) == 0);
	/*
	 * Check and make sure that the free space is large
	 * enough for this name.  If not, something weird
	 * happened.
	 * XXX fix this code to try to recover by starting
	 * XXX over at EOF?
	 */
	if ((EFS_DIR_FREESPACE(db) <= dentsize) ||
	    (db->slots > EFS_MAXENTS)) {
		goto messed_up;
	}

	/*
	 * Find location of free space in the dirblk.  Backup the
	 * firstused offset to point to the new firstused entry.
	 * Set dep to point to it in the dirblk buffer, then fill
	 * in the efs_dent.
	 */
	dirblkoff = EFS_DIR_FREEOFF(db->firstused) - dentsize;
	db->firstused = EFS_COMPACT(dirblkoff);
	if (dirblkoff <= EFS_DIRBLK_HEADERSIZE + db->slots)
		goto messed_up;

	dep = EFS_SLOTOFF_TO_DEP(dirstream.dirblk, dirblkoff);
	EFS_SET_INUM(dep, ep->e_inum);
	dep->d_namelen = namlen;
	bcopy(ep->e_name, dep->d_name, namlen);

	/*
	 * Now scan the slots, looking for an empty one to use.
	 * If none are empty, add a new slot.
	 */
	for (slot = 0; slot < db->slots; slot++) {
		if (EFS_SLOTAT(db, slot) == 0)
			break;
	}
#ifdef notdef
	ASSERT(EFS_SLOTAT(db, slot) == 0);
#endif
	EFS_SET_SLOT(db, slot, db->firstused);
	if (slot == db->slots)
		db->slots++;

	/* update the offset value-result parameter */
	ep->e_offset = EFS_MKCOOKIE(ep->e_offset, slot);
	if (error = putdirblk(&dirstream))
		return error;
	dnlc_enter_fast(itov(dp), &ep->e_fastdata, itobhv(ip), NOCRED);
	return 0;

messed_up:
	dircorrupted(dp, "operation was: enter ", ep->e_name);
	(void) putdirblk(&dirstream);
	return EIO;
}

/*
 * Rewrite the entry described by ep to point to ip.  Used by rename.
 */
int					/* 0 or an error number */
efs_dirrewrite(
	register struct inode *dp,	/* locked directory inode */
	struct inode *ip,		/* new entry's inode */
	struct entry *ep,		/* name, inumber, and offset */
	struct cred *cr)
{
	int slot, error;
	struct dirstream dirstream;
	register struct efs_dirblk *db;
	register struct efs_dent *dep;

	ASSERT(mutex_mine(&dp->i_lock));
	ASSERT(ep->e_offset != -1);
	ASSERT(EFS_DBOFF(ep->e_offset) < dp->i_size);

	ep->e_inum = ip->i_number;
	ep->e_ip = ip;

	OPENDIR(dp, EFS_DIRBSIZE, ep->e_offset, cr, &dirstream);
	if (!getdirblk(&dirstream))
		return dirstream.error;

	db = dirstream.dirblk;
	slot = EFS_SLOT(ep->e_offset);
	dep = EFS_SLOTOFF_TO_DEP(db, EFS_SLOTAT(db, slot));
#ifdef DEBUG
{
	unsigned int namlen = MIN(ep->e_namlen, EFS_MAXNAMELEN);
	ASSERT(dep->d_namelen == namlen);
	ASSERT(!bcmp(ep->e_name, dep->d_name, namlen));
}
#endif
	EFS_SET_INUM(dep, ep->e_inum);
	if (error = putdirblk(&dirstream))
		return error;
	dnlc_enter_fast(itov(dp), &ep->e_fastdata, itobhv(ip), NOCRED);
	return 0;
}

/*
 * Remove an existing name in the directory.  The cookie accurately refers
 * to the name, that was previously found using lookup.  We are never
 * going to be called with a name that was already removed.
 */
int					/* 0 or an error number */
efs_dirremove(
	register struct inode *dp,
	register struct entry *ep,	/* name, inumber and cookie */
	struct cred *cr)
{
	struct dirstream dirstream;
	register struct efs_dirblk *db;
	register struct efs_dent *dep;
	unsigned int namlen;
	register int slot, slotoff, firstusedoff;
	register int size;

        /*
         * return error when removing . and ..
         */
        if (ep->e_name[0] == '.') {
                if (ep->e_namlen == 1)
                        return (EINVAL);
                else if (ep->e_namlen == 2 && ep->e_name[1] == '.') {
                        return (EEXIST);        /* SIGH should be ENOTEMPTY */
		}
        }


	/*
	 * Open dp at the right offset and just getdirblk.
	 */
	ASSERT(ep->e_offset != -1);
	OPENDIR(dp, EFS_DIRBSIZE, ep->e_offset, cr, &dirstream);
	if (!getdirblk(&dirstream)) {
		ASSERT(dirstream.error);
		return dirstream.error;
	}

	/*
	 * Look at the cookie and directory entry and insure it's the
	 * right one as best we can.
	 */
	db = dirstream.dirblk;
	if ((slot = EFS_SLOT(ep->e_offset)) >= db->slots)
		goto not_found;
	if ((slotoff = EFS_SLOTAT(db, slot)) == 0)
		goto not_found;
	dep = EFS_SLOTOFF_TO_DEP(db, slotoff);
	if (EFS_GET_INUM(dep) != ep->e_inum)
		goto not_found;
	namlen = MIN(ep->e_namlen, EFS_MAXNAMELEN);
	if (dep->d_namelen != namlen || bcmp(dep->d_name, ep->e_name, namlen))
		goto not_found;

	size = efs_dentsize(dep);
	firstusedoff = EFS_REALOFF(db->firstused);

	/*
	 * Remove the entry.  If the entry is adjacent to the free space in
	 * the directory then all we need do is zero its contents and increase
	 * the size of the free space.  Otherwise, we have to compact the
	 * directory and then adjust the free space.  Before we compact,
	 * adjust the offsets for entries in the directory that are moved
	 * because of this.
	 */
	if (firstusedoff != slotoff) {
		register int i;
		register int movedslotoff;

		/*
		 * For each entry that is before the current entry,
		 * adjust its offset to account for its movement when
		 * the current entry is erased.
		 */
		for (i = 0; i < db->slots; i++) {
			movedslotoff = EFS_SLOTAT(db, i);
			if ((movedslotoff == 0) || (movedslotoff >= slotoff)) {
				/* empty entry/entry is not moving */
				continue;
			}
			EFS_SET_SLOT(db, i, EFS_COMPACT(movedslotoff + size));
		}

		bcopy(EFS_SLOTOFF_TO_DEP(db, firstusedoff),
		      EFS_SLOTOFF_TO_DEP(db, firstusedoff + size),
		      slotoff - firstusedoff);
		dep = EFS_SLOTOFF_TO_DEP(db, firstusedoff);
	}

	/* zero bytes no longer being used */
	bzero(dep, size);
	firstusedoff += size;
	ASSERT((firstusedoff & 1) == 0);
	db->firstused = EFS_COMPACT(firstusedoff);
	if (db->firstused == 0)
		db->slots = 0;
	else if (slot == db->slots - 1)
		db->slots--;

	/* zero out slot offset */
	EFS_SET_SLOT(db, slot, 0);

	dnlc_remove_fast(itov(dp), &ep->e_fastdata);
	return putdirblk(&dirstream);

not_found:
	/* bogus cookie */
	dircorrupted(dp, "operation was: remove ", ep->e_name);
	CLOSEDIR(&dirstream);
	return ENOENT;
}

/*
 * Initialize a directory with "." and ".." entries.
 * Used either for "mkdir" or for "rename" to rewrite a directories ".".
 */
#define	DOTSIZE		efs_dentsizebynamelen(1)
#define	DOTDOTSIZE	efs_dentsizebynamelen(2)

int				/* 0 or an error number */
efs_dirinit(
	struct inode *dp,	/* ptr to locked directory inode */
	efs_ino_t parentino,	/* dp's parent inode number */
	struct cred *cr)
{
	register struct efs_dent *dep;
	register struct efs_dirblk *db;
	struct dirstream dirstream;

	OPENDIR(dp, EFS_DIRBSIZE, 0, cr, &dirstream);
	if (dp->i_size == 0) {
		ASSERT(mutex_mine(&dp->i_lock));
		if (!growdir(&dirstream))
			return dirstream.error;
	} else {
		if (!getdirblk(&dirstream))
			return dirstream.error;
	}
	db = dirstream.dirblk;

	/* fill in "." */
	dep = EFS_SLOTOFF_TO_DEP(dirstream.dirblk, EFS_DIRBSIZE - DOTSIZE);
	EFS_SET_INUM(dep, dp->i_number);
	dep->d_namelen = 1;
	dep->d_name[0] = '.';
	EFS_SET_SLOT(db, 0, EFS_COMPACT(EFS_DIRBSIZE - DOTSIZE));

	/* fill in ".." */
	dep = EFS_SLOTOFF_TO_DEP(dirstream.dirblk,
				 EFS_DIRBSIZE - DOTSIZE - DOTDOTSIZE);
	EFS_SET_INUM(dep, parentino);
	dep->d_namelen = 2;
	dep->d_name[0] = '.';
	dep->d_name[1] = '.';
	EFS_SET_SLOT(db, 1, EFS_COMPACT(EFS_DIRBSIZE - DOTSIZE - DOTDOTSIZE));

	if (db->slots == 0) {
		/* initialize firstused value for new dirs */
		db->slots = 2;
		db->firstused =
			EFS_COMPACT(EFS_DIRBSIZE - DOTSIZE - DOTDOTSIZE);
	}

	dnlc_remove(itov(dp), "..");
	return putdirblk(&dirstream);
}

/*
 * Check whether dp is empty, really empty, or really-really empty (has
 * both "." and "..", only one, or neither, respectively).  The check is
 * loose; it doesn't cope with corruptions such as free space fragmented
 * within a directory block by an island of dents.
 */
int					/* 1 if empty, 0 otherwise */
efs_dirisempty(
	register struct inode *dp,	/* locked directory inode */
	register int *flagp,		/* result emptiness flag bits */
	struct cred *cr)
{
	register struct efs_dent *dep;	/* current dent pointer */
	struct dirstream dirstream;	/* i/o stream and fast access to it */

	ASSERT(mutex_mine(&dp->i_lock));
	*flagp = 0;
	OPENDIR(dp, dp->i_size, 0, cr, &dirstream);
	while ((dep = getdent(&dirstream)) != NULL) {
		if (dirstream.curslot == EFS_FREESLOT)
			continue;	/* skip free space */
		if (dep->d_namelen == 1 && dep->d_name[0] == '.') {
			*flagp |= DIR_HASDOT;
		} else if (dep->d_namelen == 2 && dep->d_name[0] == '.'
		    && dep->d_name[1] == '.') {
			*flagp |= DIR_HASDOTDOT;
		} else {
			dirstream.error = ENOTEMPTY;
			break;
		}
	}
	CLOSEDIR(&dirstream);
	return dirstream.error;
}

/*
 * Check that dp is not tdp, and is not a proper ancestor of tdp.
 * If two efs_notancestor calls overlapped execution, the partially
 * completed work of one call could be invalidated by the rename that
 * activated the other.  To avoid this, we serialize efs_notancestor
 * calls using efs_ancestormon.
 */
mutex_t	efs_ancestormon;	/* initialized in efs_init */

int
efs_notancestor(struct inode *dp, struct inode *tdp, struct cred *cr)
{
	int error;
	struct inode *ip;

	/*
	 * We must unlock tdp before locking the efs_ancestormon, in
	 * order to avoid deadlocking with a process renaming into a
	 * subdirectory of tdp that already has the monitor and wants
	 * to iget tdp as "..".
	 */
	iunlock(tdp);
	mutex_lock(&efs_ancestormon, PINOD);

	/*
	 * Ascend tdp's ancestor line, stopping at dp or the root of
	 * the filesystem.  If a ".." is missing, return ENOENT.
	 */
	error = 0;
	ip = tdp;
	ilock(ip);
	while (ip->i_number != EFS_ROOTINO) {
		struct entry parent;

		if (ip == dp) {
			error = EINVAL;
			break;
		}
		if (error = efs_dirlookup(ip, "..", NULL, 0, &parent, cr))
			break;
		if (parent.e_inum == ip->i_number) {
			prdev("Directory inode %d has bad parent link",
			      ip->i_dev, ip->i_number);
			error = ENOENT;
			break;
		}
		if (ip == tdp)
			iunlock(ip);
		else
			iput(ip);

		/*
		 * This is a race to get "..".
		 */
		if (error = iget(tdp->i_mount, parent.e_inum, &ip))
			break;
		if ((ip->i_mode & IFMT) != IFDIR || ip->i_nlink <= 0) {
			prdev("Ancestor inode %d is not a directory",
				ip->i_dev, ip->i_number);
			error = ENOTDIR;
			break;
		}
	}

	/*
	 * If ip is a proper ancestor of tdp, release it, then relock tdp.
	 * Make sure tdp was not removed while it was unlocked.
	 */
	if (ip != tdp) {
		if (ip != NULL)
			iput(ip);
		ilock(tdp);
	}
	if (error == 0 && tdp->i_nlink <= 0)
		error = ENOENT;

	/*
	 * Unserialize efs_notancestor calls.
	 */
	mutex_unlock(&efs_ancestormon);
	return error;
}

static void irix5_efs_fmtdirent(void *, struct efs_dent *, struct dirstream *,
			  int, unsigned short);
static void efs_fmtdirent(void *, struct efs_dent *, struct dirstream *,
		    int, unsigned short);

/*
 * Read dp's entries starting at uiop->uio_offset and translate them into
 * bufsize bytes worth of struct dirents starting at bufbase.
 */
int					/* 0 or an error number */
efs_readdir(
	bhv_desc_t *bdp,
	uio_t *uiop,
	struct cred *cr,		/* from file table entry */
	int *eofp)
{
	struct inode *dp;
	register int bufsize;		/* size of user's buffer */
	register void *bep;		/* ptr to dirent in output buffer */
	register struct efs_dent *dep;	/* efs-internal dent ptr */
	register int bufleft;		/* bytes left in user's buf */
	caddr_t bufbase;		/* buffer for translation */
	struct dirstream dirstream;	/* directory i/o stream */
	size_t allocsize;
	void (*dirent_func)(void *, struct efs_dent *, struct dirstream *,
			  int, unsigned short);
	int target_abi;

	target_abi = GETDENTS_ABI(get_current_abi(), uiop);
	switch(target_abi) {
	case ABI_IRIX5_64:
		dirent_func = efs_fmtdirent;
		break;

	case ABI_IRIX5:
		dirent_func = irix5_efs_fmtdirent;
		break;
	}

	dp = bhvtoi(bdp);
	ilock(dp);

	/* If the directory has been removed after it was opened. */
	if (dp->i_nlink <= 0) {
		iunlock(dp);
		return 0;
	}

	/*
	 * Get some memory to fill with fs-independent versions of the
	 * requested entries.
	 */
	bufsize = uiop->uio_resid;
	if (uiop->uio_segflg == UIO_SYSSPACE) {
		ASSERT(uiop->uio_iovcnt == 1);
		bufbase = NULL;
		bep = uiop->uio_iov->iov_base;
		ASSERT(((__psint_t)bep & (NBPW-1)) == 0);
	} else {
		/*
		 * Don't try to allocate some arbitrarily large bucket.
		 */
		bufsize = MIN(bufsize, 0x4000);

		allocsize = bufsize;
		bufbase = kmem_alloc(bufsize, KM_SLEEP);
		bep = bufbase;
		ASSERT(((__psint_t)bep & (NBPW-1)) == 0);
	}
	bufleft = bufsize;

	/*
	 * Open the directory at the user's offset and loop over its entries
	 * till eof or the caller's buffer is full.  Note that uiop->uio_offset
	 * is a cookie (!).
	 */
	OPENDIR(dp, dp->i_size - EFS_DBOFF(uiop->uio_offset), uiop->uio_offset,
		cr, &dirstream);
	while ((dep = getdent(&dirstream)) != NULL) {
		register int namlen;	/* name length */
		unsigned short reclen;	/* dirent length */

		ASSERT(((__psint_t)bep & (NBPW-1)) == 0);
		if (dirstream.curslot == EFS_FREESLOT)
			continue;		/* skip free space */

		/*
		 * Calculate name length and, from it, externalized
		 * record length.  Check whether this record will fit
		 * in the user's buffer.
		 */
		namlen = dep->d_namelen;
		switch(target_abi) {
		case ABI_IRIX5_64:
			reclen = DIRENTSIZE(namlen);
			break;

		case ABI_IRIX5:
			reclen = IRIX5_DIRENTSIZE(namlen);
			break;
		}

		ASSERT((reclen & (NBPW-1)) == 0);
		if (reclen > bufleft) {
			/*
			 * Check whether we read even one entry.  If not, tell
			 * the user that bufsize was too small.
			 */
			if (bufleft == bufsize) {
				dirstream.error = EINVAL;
				bufleft = -1;
			}
			break;	/* filled buffer */
		}

		/*
		 * There's room for this entry, so fill it in and advance.
		 * Note that the name is 0-terminated, and that the DIRENTSIZE
		 * macro took account of this fact.
		 */
		(*dirent_func)(bep, dep, &dirstream, namlen, reclen);

		/*
		 * Update buffer state variables.
		 */
		bep = (void *)((char *)bep + reclen);
		bufleft -= reclen;
	}

	/*
	 * If bufleft is negative then the requested bufsize was too small even
	 * for one externalized entry.  Otherwise, we got some entries, so copy
	 * them out and return the number of bytes copied.
	 */
	if (bufleft >= 0) {
		bufsize -= bufleft;
		ASSERT(bufsize >= 0);
		if (bufsize > 0 && uiop->uio_segflg == UIO_USERSPACE) {
			dirstream.error =
				uiomove(bufbase, bufsize, UIO_READ, uiop);
		} else {
			uiop->uio_resid -= bufsize;
		}
		*eofp = (dep == NULL);
		if (*eofp) {
			/* ran off end of directory */
			uiop->uio_offset = EFS_MKCOOKIE(dp->i_size, 0);
		} else {
			/* ran out of buffer space - restart at current spot */
			uiop->uio_offset = EFS_MKCOOKIE(dirstream.curoffset,
							dirstream.curslot);
		}
		dp->i_flags |= IACC;
	}
	if (bufbase != NULL)
		kmem_free(bufbase, allocsize);
	CLOSEDIR(&dirstream);
	iunlock(dp);
	return dirstream.error;
}

static void
efs_fmtdirent(
	void *buf, 
	struct efs_dent *dep,
	struct dirstream *dirstream,
	int namlen,
	unsigned short reclen)
{
	register struct dirent *bep = (struct dirent *)buf;

	bep->d_ino = EFS_GET_INUM(dep);
	if (dirstream->nextslot >= dirstream->dirblk->slots)
		bep->d_off = EFS_MKCOOKIE(dirstream->nextoffset, 0);
	else
		bep->d_off = EFS_MKCOOKIE(dirstream->curoffset,
					  dirstream->nextslot);
	bep->d_reclen = reclen;
	bcopy(dep->d_name, bep->d_name, namlen);
	bep->d_name[namlen] = '\0';
}

static void
irix5_efs_fmtdirent(
	void *buf, 
	struct efs_dent *dep,
	struct dirstream *dirstream,
	int namlen,
	unsigned short reclen)
{
	register struct irix5_dirent *bep = (struct irix5_dirent *)buf;

	bep->d_ino = EFS_GET_INUM(dep);
	if (dirstream->nextslot >= dirstream->dirblk->slots)
		bep->d_off = EFS_MKCOOKIE(dirstream->nextoffset, 0);
	else
		bep->d_off = EFS_MKCOOKIE(dirstream->curoffset,
					  dirstream->nextslot);
	bep->d_reclen = reclen;
	bcopy(dep->d_name, bep->d_name, namlen);
	bep->d_name[namlen] = '\0';
}
