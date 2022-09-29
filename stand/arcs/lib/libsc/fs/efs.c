#ident	"$Revision: 1.59 $"
#undef _KERNEL
/*
 * EFS standalone support
 * Written by: Kipp Hickman
 *
 * XXX add in write support?
 *
 * $Revision: 1.59 $
 */

#include <arcs/io.h>
#include <arcs/dirent.h>
#include <arcs/pvector.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <sys/fs/efs.h>
#include <sys/sysmacros.h>
#include <sys/sbd.h>
#include <sys/dkio.h>
#include <saio.h>
#include <arcs/fs.h>
#include <values.h>
#include <libsc.h>
#include <libsc_internal.h>

/*
 * Extent disk inodes don't have room for the inumber.  As a hack, we store
 * the inode number in the di_gen field
 */
#define	di_ino		di_gen

/* no write support yet */
#undef	WFS

#define	TRUE	(1)
#define	FALSE	(0)

struct efs_info {
	struct efs *efs_sb;
	struct efs_dinode *efs_dinode;
};

#define fsb_to_inode(fsb)     (((struct efs_info *)((fsb)->FsPtr))->efs_dinode)
#define fsb_to_fs(fsb)        (((struct efs_info *)((fsb)->FsPtr))->efs_sb)
#define fsb_to_fs_magic(fsb)        \
		(((struct efs_info *)((fsb)->FsPtr))->efs_sb->fs_magic)

/* indirect extent cache; this code is slightly bogus, but... */
#define	MAXEXTENTS	2048
char	indirectExtentsValid;
ino_t	indirectExtentsIno;
extent	*indirectExtents;

#ifdef	_MIPSEL
#define EFS_SMAGIC      0x59290700
#define EFS_SNEWMAGIC   0x5a290700
#define IS_EFS_SMAGIC(x) ((x == EFS_SMAGIC) || (x == EFS_SNEWMAGIC))
void efs_swap_dirblk(struct efs_dirblk *);
void efs_swap_sb(struct efs *);
void efs_swap_dinode(struct efs_dinode *);
void efs_swap_extent(struct extent *);
void efs_swap_dent(struct efs_dent *);
#endif	/* _MIPSEL */

static int _efs_strat(FSBLOCK *);
static int _efs_checkfs(FSBLOCK *);
static int _efs_open(FSBLOCK *);
static int _efs_close(FSBLOCK *);
static int _efs_read(FSBLOCK *);
static int _efs_getdirent(FSBLOCK *);
static int _efs_grs(FSBLOCK *);
#ifdef WFS
static int _efs_write(FSBLOCK *);
#endif

/*
 * efs_install - register this filesystem to the standalone kernel
 * and initialize any variables
 */
int
efs_install(void)
{
    indirectExtents = (extent *)dmabuf_malloc(sizeof(extent) * MAXEXTENTS);
    if (indirectExtents == (extent *)NULL) {
	printf ("efs: couldn't allocate memory for efs indirect extents\n");
	return ENOMEM;
    }
    return fs_register (_efs_strat, "efs", DTFS_EFS);
}

/*
 * _efs_strat - 
 */
static int
_efs_strat(FSBLOCK *fsb)
{
    /* don't support 64 bit offsets (yet?) */
    if (fsb->IO->Offset.hi)
	return fsb->IO->ErrorNumber = EINVAL;

    switch (fsb->FunctionCode) {
    case FS_CHECK:
	return _efs_checkfs(fsb);
    case FS_OPEN:
	return _efs_open(fsb);
    case FS_CLOSE:
	return _efs_close(fsb);
    case FS_READ:
	return _efs_read(fsb);
    case FS_WRITE:
#ifdef	WFS
	return _efs_write(fsb);
#else
	return fsb->IO->ErrorNumber = EROFS;
#endif
    case FS_GETDIRENT:
	return _efs_getdirent(fsb);
    case FS_GETREADSTATUS:
	return(_efs_grs(fsb));
    default:
	return fsb->IO->ErrorNumber = ENXIO;
    }
}

/*
 * _get_efsbuf -- obtain block io buffer
 * Called by file system open routine to obtain buffer.  If
 * fs open calls this, it's close routine MUST free this buffer.
 */
static char *
_get_efsbuf(void)
{
	return (char *)dmabuf_malloc(8192);
}

/*
 * _free_efsbuf -- free block io buffer
 */
static void
_free_efsbuf(char *cp)
{
	dmabuf_free(cp);
}

static void *
_get_efsinfo(void)
{
	struct efs_info *efs;
	if ((efs = (struct efs_info *)malloc(sizeof(*efs))) == 0)
		return 0;
	if ((efs->efs_sb = (struct efs *)dmabuf_malloc(BBTOB(BTOBB(sizeof(*efs))))) == 0) {
		free (efs);
		return 0;
	}
	if ((efs->efs_dinode = (struct efs_dinode *)dmabuf_malloc(sizeof(struct efs_dinode))) == 0) {
		dmabuf_free (efs->efs_sb);
		free (efs);
		return 0;
	}
	return (char *)efs;
}

static void
_free_efsinfo(void *efs)
{
	dmabuf_free(((struct efs_info *)efs)->efs_sb);
	dmabuf_free(((struct efs_info *)efs)->efs_dinode);
	free(efs);
}

static int
lbread(FSBLOCK *fsb, long bn, void *buf, long len, int printerr)
{
	IOBLOCK *io = fsb->IO;

	len = (long)BBTOB(BTOBB(len));
	io->Address = buf;
	io->Count = len;
	io->StartBlock = bn;
	if (DEVREAD(fsb) != len) {
		if(printerr)
			printf("efs read error, bad count\n");
		return (FALSE);
	}
	return (TRUE);
}

/*
 * Read an inode in.  Return TRUE if it works, FALSE otherwise
 */
static int
iread(struct efs_dinode *di, FSBLOCK *fsb, ino_t inum)
{
	struct efs *fs;
	struct efs_dinode *inobuf;

	fs = fsb_to_fs(fsb);

	/* read inode block in */
	inobuf = (struct efs_dinode *)
		dmabuf_malloc(sizeof(struct efs_dinode) * EFS_INOPBB);
    	if (inobuf == (struct efs_dinode *)NULL) {
		printf ("efs: couldn't allocate memory for efs inode\n");
		return (FALSE);
    	}
	if (!lbread(fsb, EFS_ITOBB(fs, inum), inobuf, BBSIZE, 1)) {
		dmabuf_free(inobuf);
		return (FALSE);
	}

	/* copy in disk inode to incore inode */
	bcopy(&inobuf[EFS_ITOO(fs, inum)], di, sizeof(struct efs_dinode));
#ifdef	_MIPSEL
	/* if the inode is swapped, swap it back
	 */
	if (IS_EFS_SMAGIC(fsb_to_fs_magic(fsb)))
		efs_swap_dinode(di);
#endif	/* _MIPSEL */
	di->di_ino = (unsigned int)inum;	/* on disk as __uint32_t */
	dmabuf_free(inobuf);
	return (TRUE);
}

/*
 * Translate a byte offset into a file, into an extent which covers that
 * offset.  The code doesn't deal correctly with files that have more than
 * MAXEXTENTS extents per indirect inextent.
 * Code used to fail with a misleading error message now it complains,
 * so if this crops up again, it will be easier to  pinpoint the problem
 * and get it fixed (this one has been around now for some time, and first
 * cropped up at least 6 months before it finally got pinpointed and fixed).
 * The code also doesn't deal with more than one indirect extent correctly...
 * The old code didn't work either, so we are better than we were, if not
 * really right.
 */
static int
bmap(FSBLOCK *fsb, extent *result, int maxblks)
{
	register struct efs_dinode *di;
	register off_t off, exoff;
	register extent *ex, *lastex;
	IOBLOCK *iob = fsb->IO;

	di = fsb_to_inode(fsb);
	if (di->di_numextents > EFS_DIRECTEXTENTS) {
		if (!indirectExtentsValid ||
		    (indirectExtentsIno != di->di_ino)) {
			register extent *iex;
			long count;

			/*
			 * Read in indirect extents
			 */
			iex = &di->di_u.di_extents[0];
			ex = &indirectExtents[0];
			if(di->di_u.di_extents[0].ex_offset > 1) {
				printf("Can't handle efs files with more than one indirect extent\n"
				"  (the file is too fragmented on disk).  Try rebuilding the\n"
				"  file after freeing up space on that filesystem\n");
				return FALSE;
			}
			if(iex->ex_length > BTOBB(MAXEXTENTS*sizeof(struct extent))) {
				printf("Can't handle efs files with more than %d extent blocks in indirect 0\n",
					BTOBB(MAXEXTENTS*sizeof(struct extent)));
				return FALSE;
			}
			if(di->di_numextents > MAXEXTENTS) {
				printf("Can't handle efs files with more than %d extents in indirect 0\n",
					MAXEXTENTS);
				return FALSE;
			}
			count = _min(sizeof(extent) * MAXEXTENTS, BBTOB(iex->ex_length));
			iob->StartBlock = iex->ex_bn;
			iob->Address = (char *) ex;
			iob->Count = count;
			if (DEVREAD(fsb) != count) {
				return (FALSE);
			}
			indirectExtentsValid = 1;
			indirectExtentsIno = di->di_ino;
		}
		else
			ex = &indirectExtents[0];
		lastex = &indirectExtents[MAXEXTENTS];
	} else {
		ex = &di->di_u.di_extents[0];
		lastex = &di->di_u.di_extents[EFS_DIRECTEXTENTS];
	}

	off = (off_t)BTOBBT(iob->Offset.lo);
	while (ex < lastex) {
		if ((ex->ex_offset <= off) &&
		    (ex->ex_offset + ex->ex_length > off)) {
			/*
			 * Return a piece of the extent which is no bigger than
			 * maxblks in size.
			 */
			exoff = off - ex->ex_offset;
			result->ex_bn = (unsigned int)(ex->ex_bn + exoff);
			result->ex_length = _min(maxblks,
						 ex->ex_length-(int)exoff);
			result->ex_offset = (unsigned int)(ex->ex_offset+exoff);
			return (TRUE);
		}
		ex++;
	}
	printf("efs: couldn't find offset %u in file (size=%d)\n",
		     iob->Offset.lo, di->di_size);
	return (FALSE);
}

/*
 * Translate a file name into an inode in the iobs buffer
 * Return TRUE if the translation succeeds, FALSE otherwise.
 */
static int
namei(char *fname, FSBLOCK *fsb)
{
	register struct efs_dent *dep;
	register struct efs_dirblk *db;
	register struct efs_dinode *dip;
	register int i, off, found;
	register char *sp;
	char ent[EFS_MAXNAMELEN+1];
	register int namelen;
	extent result;
	IOBLOCK *iob = fsb->IO;

	ent[0] = '/';
	ent[1] = 0;
	sp = fname;
	iob->Offset.hi = 0;
	iob->Offset.lo = 0;
	dip = fsb_to_inode(fsb);
	if (iread(dip, fsb, EFS_ROOTINO) == FALSE)
		return (FALSE);

	/*
	 * Skip over leading blanks, tabs, and slashes.
	 * All paths are grounded at root.
	 */
	while (*sp && (*sp == ' ' || *sp == '\t' || *sp == '/'))
		sp++;

	/*
	 * Search through directories until the end is found.
	 */
	while(*sp) {
		/* verify that we have a directory inode. */
		if ((dip->di_mode & S_IFMT) != S_IFDIR) {
			printf("'%s' not a directory.\n", ent);
			return (FALSE);
		}
		if (dip->di_nlink == 0) {
			printf("Link count = 0 for %s.\n", ent);
			return (FALSE);
		}
		if ((dip->di_mode & 0111) == 0) {
			printf("Directory %s not searchable.\n", ent);
			return (FALSE);
		}

		/* 
		 * Fetch current path name part and position sp for next entry
		 */
		for (i = 0; i < EFS_MAXNAMELEN && *sp && *sp != '/';
		       ent[i++] = *sp++);
		if (i >= EFS_MAXNAMELEN)
			while (*sp && *sp != '/')
				sp++;
		ent[i] = '\0';
		while (*sp == '/')
			sp++;
		namelen = (int)strlen(ent);

		/* Search current directory for wanted entry. */
		found = 0;
		for (off = 0; !found && (off < dip->di_size); ) {
		    register int dbs;

		    /* read next directory block */
		    iob->Offset.hi = 0;
		    iob->Offset.lo = off;
		    if (!bmap(fsb, &result, BTOBB(sizeof (struct efs_dirblk))))
			    return (FALSE);
		    if (!lbread(fsb, result.ex_bn, fsb->Buffer,
				   BBTOB(result.ex_length), 1))
			    return (FALSE);

		    /* search the dirblk */
		    db = (struct efs_dirblk *)fsb->Buffer;
#ifdef	_MIPSEL
		    /* swap directory block if fs is swapped
		     */
		    if(IS_EFS_SMAGIC(fsb_to_fs_magic(fsb)))
			    efs_swap_dirblk(db);
#endif	/* _MIPSEL */
		    for (dbs = 0; (dbs < result.ex_length) && !found; dbs++) {
			for (i = 0; i < db->slots; i++) {
			    if (EFS_SLOTAT(db, i) == 0)
				continue;
			    dep = EFS_SLOTOFF_TO_DEP(db,
					     EFS_SLOTAT(db, i));
			    if ((namelen == dep->d_namelen) &&
				(strncmp(ent, dep->d_name,
					      dep->d_namelen) == 0)) {
#ifdef	_MIPSEL
				if (IS_EFS_SMAGIC(fsb_to_fs_magic(fsb)))
					efs_swap_dent(dep);
#endif	/* _MIPSEL */
				found = 1;
				break;
			    }
			}
			off += EFS_DIRBSIZE;
			db = (struct efs_dirblk *)
				((char *)db + EFS_DIRBSIZE);
#ifdef	_MIPSEL
			/* swap directory block if fs is swapped
			 */
			if(IS_EFS_SMAGIC(fsb_to_fs_magic(fsb)))
				efs_swap_dirblk(db);
#endif	/* _MIPSEL */
		    }
		}
		if(!found) {
#ifdef WFS
			if ((iob->Flags & F_WRITE) && mknod(ent, fsb))
				return (TRUE);
			else
#endif
			return (FALSE);
		}
		if (!iread(dip, fsb, EFS_GET_INUM(dep)))
			return (FALSE);
	}
	/* used to check for it being a dir, and didn't allow it.  Allow it
		now so an 'ls' function can be added */
	return (TRUE);
}

/* try to read the superblock, possibly changing the device blocksize
 * if the initial attempt fails.  This assumes that all drivers will
 * fail direct reads where the cnt is less than the blksize.
*/
static int
getefs_sb(FSBLOCK *fsb, struct efs *efs)
{
	if(!lbread(fsb, EFS_SUPERBB, efs, sizeof(*efs), 0)) {
		int x = BBSIZE;
		fsb->IO->FunctionCode = FC_IOCTL;
		fsb->IO->IoctlCmd = (IOCTL_CMD)(__psint_t)DIOCSETBLKSZ;
		fsb->IO->IoctlArg = (IOCTL_ARG)&x;
		(void)fsb->DeviceStrategy(fsb->Device, fsb->IO);
		if(!lbread(fsb, EFS_SUPERBB, efs, sizeof(*efs), 1))
			return 0;
		return x;
	}
	return 1;
}

/* return ESUCCESS if the filesystem is an efs filesystem */
static int
_efs_checkfs(FSBLOCK *fsb)
{
	struct efs *efs;
	int oldbksz;
	extern int Verbose;

	if ((efs = (struct efs *)dmabuf_malloc(BBTOB(BTOBB(sizeof(*efs))))) == 0) {
		printf ("efs: couldn't allocate memory for efs superblock\n");
		return ENOMEM;
	}

	/* read the superblock in and check it */
	if(!(oldbksz=getefs_sb(fsb, efs)))
		goto bad;

#ifdef	_MIPSEL
	if(!IS_EFS_MAGIC(efs->fs_magic) && !IS_EFS_SMAGIC(efs->fs_magic))
		goto bad;
	if(Verbose && IS_EFS_SMAGIC(efs->fs_magic))
		printf ("EFS filesystem is swapped.\n");
#else	/* !_MIPSEL */
	if(!IS_EFS_MAGIC(efs->fs_magic))
		goto bad;
#endif	/* !_MIPSEL */

	dmabuf_free(efs);
	return ESUCCESS;
bad:
	dmabuf_free(efs);
	if(oldbksz>1) {	/* restore previous */
		fsb->IO->FunctionCode = FC_IOCTL;
		fsb->IO->IoctlCmd = (IOCTL_CMD)(__psint_t)DIOCSETBLKSZ;
		fsb->IO->IoctlArg = (IOCTL_ARG)&oldbksz;
		fsb->DeviceStrategy(fsb->Device, fsb->IO);
	}
	return EINVAL;
}

static int
_efs_open(FSBLOCK *fsb)
{
	char *file = (char *) fsb->Filename;
	int rc = 0, oldbksz = 0;

	if (fsb->IO->Flags & F_WRITE) {
		rc = EROFS;
		goto bad2;
	}
	if ((fsb->FsPtr = _get_efsinfo ()) == 0) {
		printf ("efs:  couldn't allocate memory for efs info\n");
		rc = ENOMEM;
		goto bad2;
	}
	if ((fsb->Buffer = (CHAR *) _get_efsbuf()) == 0) {
		printf ("efs:  couldn't allocate memory for efs buffer\n");
		_free_efsinfo(fsb->FsPtr);
		rc = ENOMEM;
		goto bad1;
	}

	/* read the superblock in and check it */
	if(!(oldbksz=getefs_sb(fsb, fsb_to_fs(fsb)))) {
		rc = EINVAL;
		goto bad;
	}
#ifdef	_MIPSEL
	if(!IS_EFS_MAGIC(fsb_to_fs(fsb)->fs_magic) 
		&& !IS_EFS_SMAGIC(fsb_to_fs(fsb)->fs_magic)) {
#else	/* !_MIPSEL */
	if(!IS_EFS_MAGIC(fsb_to_fs(fsb)->fs_magic)) {
#endif	/* !_MIPSEL */
		printf("not an extent file system\n");
		goto bad;
	}
#ifdef	_MIPSEL
	if(IS_EFS_SMAGIC(fsb_to_fs(fsb)->fs_magic))
	    efs_swap_sb(fsb_to_fs(fsb));
#endif	/* _MIPSEL */
	EFS_SETUP_SUPERB(fsb_to_fs(fsb));

	/* find the file we are interested in */
	if (!namei(file, fsb)) {
		rc = ENOENT;
		goto bad;
	}

	/*  Make sure we have the type of file we asked for (directory).
	 */
	if ((fsb_to_inode(fsb)->di_mode & S_IFMT) == S_IFDIR) {
		if ((fsb->IO->Flags & F_DIR) == 0) {
			rc = EISDIR;
			goto bad;
		}
	}
	else if (fsb->IO->Flags & F_DIR) {
		rc = ENOTDIR;
		goto bad;
	}
		

	return ESUCCESS;

bad:	_free_efsinfo(fsb->FsPtr);
bad1:	_free_efsbuf((char *)fsb->Buffer);
bad2:
	if(oldbksz>1) {	/* restore previous */
		fsb->IO->FunctionCode = FC_IOCTL;
		fsb->IO->IoctlCmd = (IOCTL_CMD)(__psint_t)DIOCSETBLKSZ;
		fsb->IO->IoctlArg = (IOCTL_ARG)&oldbksz;
		fsb->DeviceStrategy(fsb->Device, fsb->IO);
	}
	return(fsb->IO->ErrorNumber = rc);
}

static int
_efs_close(FSBLOCK *fsb)
{
#ifdef WFS
	register struct efs *fs;
	int time;

	if (fsb_to_fs(fsb)->s_fmod) {
		fs = fsb_to_fs(fsb);
		fs->fs_dirty = 0;
		fs->fs_checksum = efs_checksum(fs);
		fsb->IO->Address = (char *)fs;
		fsb->IO->Count = sizeof(struct efs);
		fsb->IO->StartBlock = EFS_SUPERB;
		DEVWRITE(fsb);
	}
	if (fsb_to_inode(fsb)->i_flag & (IUPD|IACC|ICHG)) {
		time = get_tod();
		iupdat(fsb, &time, &time);
	}
#endif

	_free_efsbuf((char *)fsb->Buffer);
	_free_efsinfo(fsb->FsPtr);
	return ESUCCESS;
}

static int
_efs_read(FSBLOCK *fsb)
{
	register int i;
	caddr_t dmaaddr;
	extent result;
	IOBLOCK *iob = fsb->IO;
	char *buf = iob->Address;
	long ocount,count = iob->Count;
	off_t off;

	if (iob->Offset.lo + count > fsb_to_inode(fsb)->di_size)
		count = (off_t)(fsb_to_inode(fsb)->di_size - iob->Offset.lo);
	if (count <= 0) {
		iob->Count = 0;
		return (ESUCCESS);
	}
	
	ocount = count;
	while (count > 0) {
		/* if more than one block, and alignments OK, dma directly,
		 * don't buffer.
		 * The i_offset check is so partial blocks are handled by the
		 * copy code, so multiple reads with small sizes (one read
		 * gets first part of sector, next efs_read gets rest)
		 * will work.
		 */
		i = (int)BTOBBT(count);
		if(!(iob->Offset.lo&BBMASK) && ((__scunsigned_t)buf&3)==0 && i)
			dmaaddr = buf;
		else {	/* reading single sectors */
			i = 1;
			dmaaddr = (char *) fsb->Buffer;
		}
			
		if (bmap(fsb, &result, i) == FALSE)
			return(iob->ErrorNumber = EIO);
		/* It's not sufficient to just check the StartBlock to
		 * decide if the transfer is currently in the buffer.  When
		 * loading "elf" standalones, higher level code first reads
		 * a block from the volume header, then does a large read of
		 * the entire file with the same StartBlock but a large count.
		 * Since we don't save the count, only bypass the read if
		 * it's for a single block.
		 */
		if(iob->StartBlock != result.ex_bn || i != BBSIZE) {
			iob->StartBlock = result.ex_bn;
			iob->Address = dmaaddr;
			iob->Count = BBTOB(result.ex_length);
			i = (int)iob->Count;		/* ok @ 32-bits */
			if(DEVREAD(fsb) != i)
				return(iob->ErrorNumber = EIO);
		}
		if(dmaaddr != buf) {	/* copy from 'block buffer' */
			off = (off_t)(iob->Offset.lo - BBTOB(result.ex_offset));
			if (off < 0)
				return(iob->ErrorNumber = EIO);
			i = _min((int)(BBTOB(result.ex_length)-off),(int)count);
			if (i <= 0)
				return(iob->ErrorNumber = EIO);
			bcopy(&fsb->Buffer[off], buf, i);
		}
		else	/* force next read from disk; in case */
			iob->StartBlock = -1; /* next read is small, and same ex_bn */
		count -= i;
		buf += i;
		iob->Offset.lo += i;
	}
	iob->Count = ocount;
	return(ESUCCESS);
}

/* get read status */
static int
_efs_grs(FSBLOCK *fsb)
{
	if (fsb->IO->Offset.lo >= fsb_to_inode(fsb)->di_size)
		return(fsb->IO->ErrorNumber = EAGAIN);
	fsb->IO->Count = (long)(fsb_to_inode(fsb)->di_size-fsb->IO->Offset.lo);
	return(ESUCCESS);
}

#ifdef WFS
_efs_write(FSBLOCK *fsb, char *buf, int count)
{
	register int ocount, pbn, off, i;
	IOBLOCK *iob = fsb->IO;
	extent result;

	if (count <= 0) {
		iob->Count = 0;
		return(ESUCCESS);
	}

	ocount = count;
	while (count > 0) {
		if (bmap(fsb, &result) == FALSE)
			return(iob->ErrorNumber = EIO);
		off = iob->Offset.lo - BBTOB(result.ex_offset);
		if (iob->StartBlock != pbn) {
			iob->StartBlock = result.ex_bn;
			iob->Address = fsb->Buffer;
			iob->Count = BBTOB(result.ex_length);
			if (DEVREAD(fsb) != BBTOB(result.ex_length))
				return(iob->ErrorNumber = EIO);
		}
		i = _min(BBTOB(result.ex_length) - off, count);
		if (i <= 0)
			return(iob->ErrorNumber = EIO);
		bcopy(buf, &fsb->Buffer[off], i);
		if (DEVWRITE(fsb) != BBTOB(result.ex_length))
			return(iob->ErrorNumber = EIO);
		count -= i;
		buf += i;
		iob->Offset.lo += i;

		/*
		 * Write inode when we close the file.
		 * Delayed until then or wait until a new block 
		 * is allocated.
		 */
		if (iob->Offset.lo > fsb_to_inode(fsb)->di_size) {
			fsb_to_inode(fsb)->di_size = iob->Offset.lo;
			fsb_to_inode(fsb)->i_flag |= IUPD|IACC;
		}
	}
	iob->Count = ocount;
	return(ESUCCESS);
}
#endif

static struct efs_dent *
getdent(FSBLOCK *fsb)
{
	IOBLOCK *iob = fsb->IO;
	unsigned long bytesleft = fsb_to_inode(fsb)->di_size -
				  EFS_DBOFF(iob->Offset.lo);
	struct efs_dirblk *db;
	extent result;
	int i, slotoffset;
	unsigned long curslot;

	db = (struct efs_dirblk *)fsb->Buffer;

	while (bytesleft > 0) {

	    /* if no block has been read yet, or the current slot number
	     * is greater than the number of slots in the block, then
	     * read the next block
	     */
	    if (!iob->Offset.lo || (EFS_SLOT(iob->Offset.lo) >= db->slots)) {

		/* slot and offset indicate same spot
		 */
		if (EFS_SLOT(iob->Offset.lo) && (
			EFS_SLOT(iob->Offset.lo) >= db->slots)) {
		    iob->Offset.lo += EFS_DIRBSIZE;
		    iob->Offset.lo &= ~EFS_DIRBMASK;
		    bytesleft -= EFS_DIRBSIZE;
		    if (bytesleft <= 0)
			break;
		}

		/* read single sector 
		 */
		if (bmap(fsb, &result, 1) == FALSE) {
		    iob->ErrorNumber = EIO;
		    return (struct efs_dent *)(__psunsigned_t)-1;
		}

		iob->StartBlock = result.ex_bn;
		iob->Address = fsb->Buffer;
		iob->Count = BBTOB(result.ex_length);
		i = (int)iob->Count;		/* 32-bits ok */
		if (DEVREAD(fsb) != i) {
		    iob->ErrorNumber = EIO;
		    return (struct efs_dent *)(__psunsigned_t)-1;
		}
#ifdef	_MIPSEL
		/* swap directory block if fs is swapped
		 */
		if(IS_EFS_SMAGIC(fsb_to_fs_magic(fsb)))
		    efs_swap_dirblk(db);
#endif	/* _MIPSEL */

		/* sanity check directory block
		 */
		if ((db->magic != EFS_DIRBLK_MAGIC) || 
			(db->slots > EFS_MAXENTS) ||
			(EFS_DIR_FREEOFF(db->firstused) <
			    (EFS_DIRBLK_HEADERSIZE + db->slots))) {
		    iob->ErrorNumber = ENOTDIR;
		    return (struct efs_dent *)(__psunsigned_t)-1;
		}
	    }

	    /* if slot is empty, continue
	     */
	    curslot = EFS_SLOT(iob->Offset.lo);
	    iob->Offset.lo++;
	    if ((slotoffset = EFS_SLOTAT(db, curslot)) == 0)
		continue;

#ifdef	_MIPSEL
	    if (IS_EFS_SMAGIC(fsb_to_fs_magic(fsb)))
		efs_swap_dent(EFS_SLOTOFF_TO_DEP(db, slotoffset));
#endif	/* _MIPSEL */
	    return EFS_SLOTOFF_TO_DEP(db, slotoffset);
	}
	return NULL;
}

static int
_efs_getdirent(FSBLOCK *fsb)
{
	register long left;
	struct efs_dent *dep;
	int namelen;
	DIRECTORYENTRY *dp = (DIRECTORYENTRY *)fsb->IO->Address;
	long count = fsb->IO->Count;
	static struct efs_dinode di;

	if (count == 0)
		return(fsb->IO->ErrorNumber = EINVAL);	/* none requested */

	left = count;
	while (left && ((dep = getdent(fsb)) != NULL)) {
		if (dep == (struct efs_dent *)(__psunsigned_t)-1)
			return(fsb->IO->ErrorNumber = EIO);

		namelen = _min(dep->d_namelen, FileNameLengthMax-1);

		dp->FileAttribute = ReadOnlyFile;

		/* Figure out if this file is a directory or not,
		 * must read the inode to do so
		 */
		if (iread(&di, fsb, EFS_GET_INUM(dep)) == FALSE) {
			fsb->IO->ErrorNumber = EIO;
			break;
		}
		if ((di.di_mode & S_IFMT) == S_IFDIR)
			dp->FileAttribute |= DirectoryFile;
		dp->FileNameLength = namelen;
		bcopy (dep->d_name, dp->FileName, (int)namelen);
		dp->FileName[namelen] = '\0';
		dp++;
		left--;
	}

	fsb->IO->Count = count - left;

	/* ARCS specifies that ENOTDIR is returned when no more entries.
	 */
	if (fsb->IO->Count == 0)
		return(fsb->IO->ErrorNumber = ENOTDIR);
		
	return(ESUCCESS);
}

void
efs_swap_extent(struct extent *ex)
{
    unsigned char *idx = (unsigned char *)ex;
    unsigned char tmp;

    tmp = idx[1];
    idx[1] = idx[3];
    idx[3] = tmp;
    tmp = idx[5];
    idx[5] = idx[7];
    idx[7] = tmp;
}

void
efs_swap_dinode(struct efs_dinode *di)
{
    int xcnt, xtents;

    swaps (di, 4);
    swapl (&di->di_size, 5);
    swaps (&di->di_numextents, 2);
    xtents = _min (di->di_numextents, EFS_DIRECTEXTENTS);
    for (xcnt = 0; xcnt < xtents; xcnt++)
	efs_swap_extent (&di->di_u.di_extents[xcnt]);
}

void
efs_swap_sb(struct efs *sb)
{
    /* swap everything except the magic number, so we have a
     * record that the fs is swapped
     */
    swapl (sb, 3);
    swaps (&sb->fs_cgisize, 5);
    swapl (&sb->fs_time, 1);
    /*  leave magic alone here */
    swapl (&sb->fs_bmsize, 5);
    swapl (&sb->fs_checksum, 1);
}

void
efs_swap_dirblk(struct efs_dirblk *db)
{
    swaps (db, 1);
}

void
efs_swap_dent(struct efs_dent *dep)
{
    unsigned short *sp = (unsigned short *)dep;
    unsigned short tmp0 = sp[0];
    unsigned short tmp1 = sp[1];
    sp[0] = ((tmp1 & 0xff) << 8) | ((tmp1 & 0xff00) >> 8);
    sp[1] = ((tmp0 & 0xff) << 8) | ((tmp0 & 0xff00) >> 8);
}
