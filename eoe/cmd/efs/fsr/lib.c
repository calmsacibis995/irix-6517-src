#ident "$Revision: 1.23 $"

/*
 * library-ish stuff
 */

#include "fsr.h"
#include <signal.h>
#include <syslog.h>
#include <diskinvent.h>

int gflag;

extern int vflag;
extern int dflag;
static int (*fsc_errpr)(const char *, ...);

static vecballoc(dev_t dev, int vecne, extent *vecex);
static void vecfree(dev_t dev, int vecne, extent *vecex);
static void offfree(struct fscarg *fap, off_t offset, int len);
static veccopy(int fd, extent *toex, extent *fromex, int nblocks);
static veccomputex(struct efs_mount *mp, struct fscarg *fap,
		int vecne, extent *vecex, off_t offset, int nblocks);
static indir(struct efs_mount *mp, dev_t dev, efs_ino_t ino, int ne,
		extent **ix, int *nie);

static inex(efs_daddr_t bn, int len, int ne, extent *ex);
static onex(efs_daddr_t bn, int len, int ne, extent *ex);
static void squirrelex(struct fscarg *from, struct fscarg *to);

#define	DEBUG
#ifdef	DEBUG	/* { */
/*
 * If dflag then save and verify stuff in these variables
 * failing a test under dflag will cause a dbg_abort()
 * This is mostly to catch bugs within fsr, but corrupt file systems are
 * caught in this net as well.  For this latter reason we ship the production
 * fsr to be highly vigilant (with DEBUG _on_) to not make things worse
 * "in the unlikely event of" a corrupt file system.
 */
static struct fscarg 	dbg_fa, dbg_nfa;
static struct efs 	dbg_fs;
static extent 		*dbg_bex;	/* record BALLOC's */
static 			dbg_bne;
static 			dbg_flags;
#define	D_GOTEX		0x0004
#define	D_LOCKED	0x0001
#define	D_COMMITTED	0x0002

#define	TEMPLATE	"/tmp/.fsrXXXXXX"

int lbsize = 64;
int max_ext_size;

void
dbg_abort(char *msg, int info)
{
	char *tfile;
	FILE *fp;

	if ((tfile = mktemp(TEMPLATE)) == NULL) {
nofile:
		fsc_errpr("internal error dev=0x%x ino=%d %s %d, exiting\n",
			dbg_fa.fa_dev, dbg_fa.fa_ino, msg, info);
		exit(1);
	}
	if ((fp = fopen(tfile, "w")) == NULL)
		goto nofile;

	fprintf(fp, "flags=0x%x dev=0x%x inode=%d\n",
		dbg_flags, dbg_fa.fa_dev, (efs_ino_t)dbg_fa.fa_ino);

	fprintf(fp, "dbg_fs:\n");
	fprintf(fp, "size=%d firstcg=%d cgfsize=%d cgisize=%d\n",
	 	dbg_fs.fs_size,	dbg_fs.fs_firstcg, dbg_fs.fs_cgfsize,
		dbg_fs.fs_cgisize);
	fprintf(fp, "ncg=%d bmblock=%d bmsize=%d\n",
		dbg_fs.fs_ncg, dbg_fs.fs_bmblock, dbg_fs.fs_bmsize);

	fprintf(fp, "dbg_fa:\n");
	fprintf(fp, "dev=0x%x ino=%d ne=%d nie=%d ex=0x%x ix=0x%x\n",
		dbg_fa.fa_dev, (efs_ino_t)dbg_fa.fa_ino, dbg_fa.fa_ne,
		dbg_fa.fa_nie, dbg_fa.fa_ex, dbg_fa.fa_ix);
	fprexf(fp, &dbg_fa);

	fprintf(fp, "dbg_nfa:\n");
	fprintf(fp, "dev=0x%x ino=%d ne=%d nie=%d ex=0x%x ix=0x%x\n",
		dbg_nfa.fa_dev, (efs_ino_t)dbg_nfa.fa_ino, dbg_nfa.fa_ne,
		dbg_nfa.fa_nie, dbg_nfa.fa_ex, dbg_nfa.fa_ix);
	fprexf(fp, &dbg_nfa);

	fprintf(fp, "dbg_bex:\n");
	fprex(fp, dbg_bne, dbg_bex, 0, 0);
	
	fprintf(fp, "caller=%s info=%d\n", msg, info);
	fclose(fp);

	fsc_errpr("internal error, see %s\n", tfile);
	exit(1);
}

void
dbg_copycheck(efs_daddr_t frombn, efs_daddr_t tobn, int nb)
{
	struct fscarg *fap;

	if (!(dbg_flags & D_LOCKED))
		dbg_abort("cpcheck0", 0);
	/*
	 * frombn+nb must be in current extents
	 * tobn+nb must be in new extents
	 */
	if (!dbg_bex || !dbg_bne)	/* had to have BALLOC'ed first */
		dbg_abort("cpcheck1", 0);

	if (dbg_flags & D_COMMITTED)
		fap = &dbg_nfa;
	else
		fap = &dbg_fa;

	if (!inex(frombn, nb, fap->fa_ne, fap->fa_ex))
		dbg_abort("cpcheck2", 0);

	if (!inex(tobn, nb, dbg_bne, dbg_bex))
		dbg_abort("cpcheck3", 0);
}
#endif /* DEBUG } */

/*
 * Kernel primitives on the pseudo-driver
 */

/* ------------------------------------------------------------------------- */
static int fsc_fd;

fsc_init(int (*func)(const char *, ...), int d, int v)
{
	int sz;

	fsc_errpr = func;
	dflag = d;
	vflag = v;


	for (sz = lbsize; sz + lbsize < EFS_MAXEXTENTLEN; sz += lbsize)
		;
	max_ext_size = sz;
	if ((fsc_fd = open(DEVFSCTL, O_RDWR)) == -1)
		return -1;
	return 0;
}

void
fsc_close(void)
{
	close(fsc_fd);
}

int
fsc_ilock(struct fscarg *fap)
{
#ifdef	DEBUG
	if (dflag && (dbg_flags & D_LOCKED))
		dbg_abort("ilock", 0);
#endif

	if (ioctl(fsc_fd, ILOCK, fap) == -1)
		return -1;

#ifdef	DEBUG
	if (dflag) {
		dbg_fa.fa_ino = fap->fa_ino;
		dbg_fa.fa_dev = fap->fa_dev;
		dbg_flags |= D_LOCKED;
	}
#endif
	return 0;
}

void
fsc_iunlock(struct fscarg *fap)
{
#ifdef	DEBUG
	if (dflag) {
		int info;
		if (fap->fa_dev != dbg_fa.fa_dev ||
	            fap->fa_ino != dbg_fa.fa_ino ||
	            !(dbg_flags & D_LOCKED))
			dbg_abort("iunlock0", 0);
		if ((dbg_flags & D_COMMITTED)
		    && (info = exverify(&dbg_fs, &dbg_nfa)))
			dbg_abort("iunlock1", info); /* ouch... bad news! */
		if (dbg_bex) {
			free(dbg_bex);
			dbg_bex = 0;
			dbg_bne = 0;
		}
		if (dbg_flags & D_GOTEX) {
			freeex(&dbg_fa);
			dbg_flags &= ~D_GOTEX;
		}
		if (dbg_flags & D_COMMITTED) {
			freeex(&dbg_nfa);
			bzero(&dbg_nfa, sizeof(dbg_nfa));
			dbg_flags &= ~D_COMMITTED;
		}
		bzero(&dbg_fa, sizeof(dbg_fa));
		dbg_flags &= ~D_LOCKED;
	}
#endif
	(void)ioctl(fsc_fd, IUNLOCK, fap);
}

int
fsc_icommit(struct fscarg *fap)
{
#ifdef	DEBUG
	if (dflag) {
		int info;
		if (!(dbg_flags & D_LOCKED) ||
		    fap->fa_dev != dbg_fa.fa_dev ||
		    fap->fa_ino != dbg_fa.fa_ino ||
		    (info = exverify(&dbg_fs, fap)))
			dbg_abort("icommit0", info);
		if ((dbg_flags & D_COMMITTED)
		    && (info = exverify(&dbg_fs, &dbg_nfa)))
			dbg_abort("icommit1", info);
	}
#endif

	if (ioctl(fsc_fd, ICOMMIT, fap) == -1) {
		int error = errno;
		if (vflag)
			fsc_errpr("ioctl(ICOMMIT 0x%x i%d ne=%d ni=%d): %s\n",
				fap->fa_dev, fap->fa_ino, fap->fa_ne,
				fap->fa_nie, strerror(errno));
		return error;
	}
#ifdef	DEBUG
	if (dflag) {
		if (dbg_flags & D_COMMITTED) {
			freeex(&dbg_fa);
			bcopy(&dbg_nfa, &dbg_fa, sizeof(dbg_fa));
			bzero(&dbg_nfa, sizeof(dbg_nfa));
		}
		dbg_nfa.fa_ino = fap->fa_ino;
		dbg_nfa.fa_dev = fap->fa_dev;
		squirrelex(fap, &dbg_nfa);
		dbg_flags |= D_COMMITTED;
		free(dbg_bex);
		dbg_bex = 0;
		dbg_bne = 0;
	}
#endif
	return 0;
}

int
fsc_bitfunc(int wha, dev_t dev, efs_daddr_t bn, int len)
{
	struct fscarg fa;
	fa.fa_dev = dev;
	fa.fa_bn = bn;
	fa.fa_len = len;

#ifdef	DEBUG
	if (dflag) {
		if (!(dbg_flags & D_LOCKED) || dbg_fa.fa_dev != dev)
			dbg_abort("bit0", 0);
		if (wha == BFREE) {
			if ((dbg_flags & D_COMMITTED) && dbg_bne == 0) {
				if (onex(bn, len, dbg_nfa.fa_ne,
							dbg_nfa.fa_ex) ||
				    onex(bn, len, dbg_nfa.fa_nie,
							dbg_nfa.fa_ix) ||
				    (!inex(bn, len, dbg_fa.fa_ne,
							dbg_fa.fa_ex) &&
				     !inex(bn, len, dbg_fa.fa_nie,
							dbg_fa.fa_ix)))
					dbg_abort("bit1", 0);
			} else if (dbg_bne) {
				if (!inex(bn, len, dbg_bne, dbg_bex))
					dbg_abort("bit2", 0);
			} else {
				dbg_abort("bit3", 0);
			}
		}
	}
#endif

	if (vflag > 1)
		fsrprintf("bitfunc %s %d(%d)\n",
			wha == BFREE ? "BFREE" : "BALLOC", bn, len);
	if (ioctl(fsc_fd, wha, &fa) == -1) {
		if (vflag > 1)
			fsc_errpr("bitfunc %d error %d %d(%d)\n", wha,
				errno, bn, len);
		return -1;
	}

#ifdef	DEBUG
	if (dflag && wha == BALLOC) {
		extent *ex;
		if ((dbg_bex=(extent*)realloc(dbg_bex,
			(dbg_bne + 1) * sizeof(extent))) == NULL) {
			fsc_errpr("malloc() failed\n");
			exit(1);
		}
		ex = &dbg_bex[dbg_bne++];
		ex->ex_bn = bn;
		ex->ex_length = len;
		if (fsc_tstalloc(fa.fa_dev, bn) < (long)len)
			dbg_abort("allocfail", bn);
	}
#endif
	return 0;
}

int
fsc_tstfunc(int wha, dev_t dev, efs_daddr_t bn)
{
	struct fscarg fa;
	int len;

	fa.fa_dev = dev;
	fa.fa_bn = bn;

	if ((len = ioctl(fsc_fd, wha, &fa)) == -1)
		return -1;
	return len;
}
/* ------------------------------------------------------------------------- */

int
getex(struct efs_mount *mp, struct efs_dinode *di,
	struct fscarg *fap, int locked)
{
	fap->fa_ex = efs_getextents(mp, di, fap->fa_ino);
	if (fap->fa_ex == 0)
		return 0;

	fap->fa_ne = di->di_numextents;
	fap->fa_nie = 0;
	fap->fa_ix = NULL;

	if (di->di_numextents > EFS_DIRECTEXTENTS) {
		struct efs_dinode di_local = *di;
		struct efs_dinode *di2 = di;

		if (!locked && fsc_ilock(fap)) {
			freeex(fap);
			return 0;
		}
		di2 = efs_iget(mp, fap->fa_ino);
		if (!locked)
			fsc_iunlock(fap);
		if (!di2) {
			freeex(fap);
			return 0;
		}
		if (di2 != di)
			*di = *di2;

		di_local.di_atime = di->di_atime;
		if (bcmp(&di_local, di, sizeof(*di))) {
			freeex(fap);
			return 0;
		}

		fap->fa_nie = di->di_u.di_extents[0].ex_offset;
		if ((fap->fa_ix = (extent*)malloc(fap->fa_nie * sizeof(extent)))
			== NULL) {
			fsc_errpr("getext(i%d) malloc failed\n", fap->fa_ino);
			freeex(fap);
			return 0;
		}
		bcopy(di->di_u.di_extents, fap->fa_ix,
			fap->fa_nie * sizeof(extent));

		if (exverify(mp->m_fs, fap)) {
			freeex(fap);
			return 0;
		}
	}

#ifdef	DEBUG
	if (dflag && dbg_fa.fa_ino == fap->fa_ino) {
		if (exverify(mp->m_fs, fap))
			return 0;
		squirrelex(fap, &dbg_fa);
		bcopy(mp->m_fs, &dbg_fs, sizeof(struct efs));
		dbg_flags |= D_GOTEX;
	}
#endif
	return 1;
}


void
freeex(struct fscarg *fap)
{
	free(fap->fa_ex);
	fap->fa_ex = 0;
	if (fap->fa_ix) {
		free(fap->fa_ix);
		fap->fa_ix = 0;
	}
}

static void
squirrelex(struct fscarg *from, struct fscarg *to)
{
	to->fa_ne = from->fa_ne;
	assert(!to->fa_ex);
	assert(!to->fa_ix);
	if ((to->fa_ex = (extent*)malloc(to->fa_ne * sizeof(extent))) == NULL) {
		fsc_errpr("malloc() failed\n");
		exit(1);
	}
	bcopy(from->fa_ex, to->fa_ex, to->fa_ne * sizeof(extent));
	if (from->fa_nie) {
		to->fa_nie = from->fa_nie;
		if ((to->fa_ix = (extent*)
		     malloc(to->fa_nie * sizeof(extent))) == NULL) {
			fsc_errpr("malloc() failed\n");
			exit(1);
		}
		bcopy(from->fa_ix, to->fa_ix,
			to->fa_nie * sizeof(extent));
	} else {
		to->fa_nie = 0;
		to->fa_ix = 0;
	}
}

/* ------------------------------------------------------------------------- */

int
exverify(struct efs *fs, struct fscarg *fap)
{
	extent *ex = fap->fa_ex;
	int ne = fap->fa_ne;
	off_t offset = 0;
	int indirbbs = 0;

	if (ne < 0 || ne > EFS_MAXEXTENTS)
		return 1;
	for (; ne--; ex++) {
		if (invalidblocks(fs, ex->ex_bn, ex->ex_length))
			return 3;
		if (fsc_tstalloc(fap->fa_dev, ex->ex_bn) < (long)ex->ex_length)
			return 4;
		if (offset != ex->ex_offset)
			return 5;
		offset += ex->ex_length;
	}
	if (fap->fa_ne <= EFS_DIRECTEXTENTS && fap->fa_nie != 0)
		return 6;
	for (ex=fap->fa_ix, ne=fap->fa_nie; ne--; ex++) {
		if (invalidblocks(fs, ex->ex_bn, ex->ex_length))
			return 8;
		if (fsc_tstalloc(fap->fa_dev, ex->ex_bn) < (long)ex->ex_length)
			return 9;
		indirbbs += ex->ex_length;
	}
	if (fap->fa_nie && BTOBB(fap->fa_ne * sizeof(extent)) != indirbbs)
		return 10;
	return 0;	/* a-ok */
}

int
invalidblocks(struct efs *fs, efs_daddr_t bn, int len)
{
	if ( 	/* just plain wacko */
		bn < 0 || len < 1 ||
		/* beyond last data block in file system */
		bn >= EFS_CGIMIN(fs, fs->fs_ncg) ||
		/* 'bn' is in an i-list */
		bn < EFS_CGIMIN(fs, EFS_BBTOCG(fs, bn)) + fs->fs_cgisize ||
		/* bn+len extents out of data block area of cg */
		EFS_BBTOCG(fs, bn) != EFS_BBTOCG(fs, bn + len - 1) )
			return 1;
	return 0;
}

static int
inex(efs_daddr_t bn, int len, int ne, extent *ex)
{
	for (; ne--; ex++)
		if (bn >= (long)ex->ex_bn
		    && bn + len <= (long)ex->ex_bn + (long)ex->ex_length)
			return 1;
	return 0;
}

static int
onex(efs_daddr_t bn, int len, int ne, extent *ex)
{
	for (; ne--; ex++)
		if ((bn >= (long)ex->ex_bn
		     && bn < (long)ex->ex_bn + (long)ex->ex_length) ||
		    (bn + len > (long)ex->ex_bn &&
		     bn + len < (long)ex->ex_bn + (long)ex->ex_length))
			return 1;
	return 0;
}
/* ------------------------------------------------------------------------- */


efs_daddr_t
freefs(struct efs_mount *mp, int cg, int wantlen, /* RETURN */ int *gotlen)
{
	int cg0, biggest = 0;
	efs_daddr_t newbn, bigbn;

	/* search in 'cg' */
	newbn = freecg(mp, cg, wantlen, gotlen);
	if (newbn == -1 || *gotlen >= wantlen)
		return newbn;
	biggest = *gotlen;
	bigbn = newbn;

	/* search from 'cg' outward */ 
	for (cg0=cg+1; cg0 < mp->m_fs->fs_ncg; cg0++) {
		newbn = freecg(mp, cg0, wantlen, gotlen);
		if (newbn == -1 || *gotlen >= wantlen)
			return newbn;
		if (*gotlen > biggest) {
			biggest = *gotlen;
			bigbn = newbn;
		}
	}
	
	/* search from 'cg' inward */
	for ( cg0=cg-1; cg0 >= 0; cg0-- ) {
		newbn = freecg(mp, cg0, wantlen, gotlen);
		if (newbn == -1 || *gotlen >= wantlen)
			return newbn;
		if (*gotlen > biggest) {
			biggest = *gotlen;
			bigbn = newbn;
		}
	}

	/* couldn't find 'wantlen', return the biggest we did find */
	*gotlen = biggest;
	return bigbn;
}

efs_daddr_t
freecg(struct efs_mount *mp, int cg, int wantlen, int *gotlen)
{
	return freebetween(mp->m_dev,
		EFS_CGIMIN(mp->m_fs, cg) + mp->m_fs->fs_cgisize,
		EFS_CGIMIN(mp->m_fs, cg+1), wantlen, 0, gotlen);
	
}

/*
 * Look for 'wantlen' blocks ['startbn','endbn').
 * Returns bn of first extent of blocks greater than 'wantlen' bbs
 * or the largest range found.  If 'prebn' is non-NULL it will be
 * used to record the 'bn' of the first hole preceding 'startbn'.
 * If 'prebn' is non-NULL and 'beforebn-1' is not free: *prebn = 0.
 *
 * assumes 'startbn' and 'beforebn-1' are in the same cg
 */
efs_daddr_t
freebetween(dev_t dev, efs_daddr_t startbn, efs_daddr_t beforebn, int wantlen,
		/* RETURN */ efs_daddr_t *prebn, int *foundlen)
{
	int len;
	efs_daddr_t bn = 0;

	*foundlen = 0;

	if (prebn)
		*prebn = 0;

	while (startbn < beforebn) {
		if ((len = fsc_tstfree(dev, startbn)) == -1)
			return -1;
		if (len == 0) {
			if ((len = fsc_tstalloc(dev, startbn)) == -1)
				return -1;
			startbn += len;
			continue;
		}
		if (len >= wantlen) {
			*foundlen = len;
			return startbn;
		}
		if (len > *foundlen) {	/* track the largest */
			*foundlen = len;
			bn = startbn;
		}
		if (prebn && startbn + len == beforebn)
			*prebn = startbn;
		startbn += len;
	}
	return bn;
}

void
print_free(dev_t dev, efs_daddr_t startbn, efs_daddr_t beforebn)
{
	int len;
	int cnt = 0;
	int free = 1;

	fsrprintf("free %d->%d", startbn, beforebn);
	while (startbn < beforebn) {
		if (!(cnt++ % 8))
			fsrprintf("\nf");
		if (free)
			len = fsc_tstfree(dev, startbn);
		else
			len = fsc_tstalloc(dev, startbn);
		fsrprintf(" %s%d", free ? "f" : "a", len);
		free = !free;
		if (len < 0)
			break;
		startbn += len;
	}
	fsrprintf("\n");
}

/*
 * 'nb' never greater than EFS_MAXEXTENTLEN
 * Use libdisk readb/writeb (syssgi(SGI_READB/WRITEB...)/rdwrb) to cover
 * devices larger than 2GB.
 */

int
copy(int fd, efs_daddr_t frombn, efs_daddr_t tobn, int nb)
{
	char buf[BBSIZE*EFS_MAXEXTENTLEN];
	int bbcount;

	if (vflag>1)
		fsc_errpr("copy() %d+%d -> %d\n", frombn, nb, tobn);

#ifdef	DEBUG
	if (dflag)
		dbg_copycheck(frombn, tobn, nb);
#endif

	assert(nb > 0 && nb <= EFS_MAXEXTENTLEN);

	bbcount = efs_readb(fd, buf, frombn, nb);
	if (bbcount == -1) {
		if (vflag)
			fsc_errpr("efs_readb(%d blocks) failed: %s\n",
				nb, strerror(errno));
		return -1;
	}
	if (bbcount != nb) {
		if (vflag)
			fsc_errpr("efs_readb(%d blocks) got %d\n", nb, bbcount);
		return -1;
	}
	bbcount = efs_writeb(fd, buf, tobn, nb);
	if (bbcount == -1) {
		if (vflag)
			fsc_errpr("efs_writeb(%d blocks) failed: %s\n",
				nb, strerror(errno));
		return -1;
	}
	if (bbcount != nb) {
		if (vflag)
			fsc_errpr("write(%d bytes) got %d\n", nb, bbcount);
		return -1;
	}
	return 0;
}	



extent *
offtoex(extent *ex, int ne, off_t offset)
{
	for (; ne--; ex++)
		if (ex->ex_offset == offset)
			return ex;
#ifdef	DEBUG
	if (dflag)
		dbg_abort("offtoex", offset);
#endif
	return 0;
}



char *
devnm(dev_t dev)
{
	char name[MAXPATHLEN];
	int namelen;

	namelen = sizeof(name);
	if (dev_to_rawpath(dev, name, &namelen, NULL))
		return strdup(name);

	fsc_errpr("could not find device file for dev number 0x%x\n", dev);

	return NULL;
}

/* ------------------------------------------------------------------------- */

void
fspieces(struct efs_mount *mp, struct efsstats *es)
{
	struct efs_dinode *di, *enddi, *inos;
	extent *ex;
	int ne, inobytes, cg;
	efs_ino_t inum;

	es->tpieces = 0;
	es->tbb = 0;
	es->tholes = 0;
	es->tfreebb = 0;

	fsync(mp->m_fd);

	inobytes = EFS_INOPBB * mp->m_fs->fs_cgisize*sizeof(struct efs_dinode);
	inos = (struct efs_dinode *) malloc(inobytes);
	if (inos == NULL) {
		fsc_errpr("malloc(%s) failed\n", inobytes);
		exit(1);
	}
	/*
	 * Compute average file fragmentation:
	 *
	 * 	foreach cg
	 *		snarf out the whole i-list
	 *		foreach reg file
	 *			filepieces()
	 */
	for (cg = 0; cg < mp->m_fs->fs_ncg; cg++) {
		inos = efs_igetcg(mp, cg);
		if (inos == 0)
			exit(1);
		enddi = &inos[EFS_INOPBB * mp->m_fs->fs_cgisize];
		if (cg == 0) {
			di = inos + 4;
			inum = 4;	/* we know 2 and 3 are dirs */
		} else {
			di = inos;
		}
		for (; di < enddi; di++, inum++)
			if (S_ISREG(di->di_mode) && di->di_numextents > 0) {
				ex = efs_getextents(mp, di, inum);
				if (ex == 0)
					exit(1);
				ne = di->di_numextents;
				es->tpieces += filepieces(ex, ne);
				es->tbb += ex[ne-1].ex_offset +
						ex[ne-1].ex_length;
				free(ex);
			}
		free(inos);
	}

	/*
	 * Compute file system free space fragmentation
	 *
	 *	read in the bitmap
	 *	foreach cg
	 *		count # free "holes"
	 *		count # free blocks
	 */	

	if (efs_bget(mp))
		exit(1);

	for (cg = 0; cg < mp->m_fs->fs_ncg; cg++) {
		int bn = EFS_CGIMIN(mp->m_fs, cg) + mp->m_fs->fs_cgisize;
		int endbn = EFS_CGIMIN(mp->m_fs, cg+1);
		while (bn < endbn) {
			if (btst(mp->m_bitmap, bn)) {
				while (bn < endbn && btst(mp->m_bitmap, bn)) {
					bn++;
					es->tfreebb++;
				}
				es->tholes++;
			} else {
				while (bn < endbn && btst(mp->m_bitmap, bn) ==0)
					bn++;
			}
		}
	}
}

/*
 * returns count of 'pieces' -- promotes abutting extents to a single piece
 */
int
filepieces(extent *ex, int ne)
{
	int npieces = 1;

	if (ne == 0) 
		return 0;
	for (; --ne > 0; ex++)
		if (ex[0].ex_bn + ex[0].ex_length != ex[1].ex_bn)
			npieces++;

	return npieces;
}

/* ------------------------------------------------------------------------- */
/*
 * overlapped movex
 *
 *       < slidesize >< length >
 *   YYYY.............XXXXXXXXXX......
 *       ^newbn       ^offset
 *
 */
int
slidex(struct efs_mount *mp, struct fscarg *fap,
	off_t offset, efs_daddr_t newbn, int length, int slidesize)
{
	int mvlen;
	int sparebn;
	int nspare;
	int error;

	if (vflag)
		fsc_errpr("slidex() i%d %d+%d -> %d (%d)\n",
			fap->fa_ino, offset, length, newbn, slidesize);
	if (slidesize * 2 >= max_ext_size)
		nspare = 0;
	else
		sparebn = freefs(mp, 0, MIN(length, max_ext_size), &nspare);
	if (nspare > 2 * slidesize) {
		while (length > 0) {
			mvlen = MIN(length, nspare);
			if (error = movex(mp, fap, offset, sparebn, mvlen))
				return error;
			if (error = movex(mp, fap, offset, newbn, mvlen))
				return error;
			newbn += mvlen;
			offset += mvlen;
			length -= mvlen;
		}
		return 0;
	}
	while (length > 0) {
		mvlen = MIN(length, slidesize);
		if (error = movex(mp, fap, offset, newbn, mvlen))
			return error;
		newbn += mvlen;
		offset += mvlen;
		length -= mvlen;
	}
	return 0;
}


/*
 * move 'offset'+'mvlen' to newbn.  newbn+mvlen assumed contiguous.
 * carves mvlen into maximal extents.
 */
int
movex(struct efs_mount *mp, struct fscarg *fap,
	off_t offset, efs_daddr_t newbn, int mvlen)
{
	extent *ex, *ex0;
	int ne, ne0, len, len2;
	int error;

	if (vflag)
		fsc_errpr("movex() i%d %d+%d -> %d\n",
			fap->fa_ino, offset, mvlen, newbn);

	assert(mvlen > 0);

	/* fit as many logical blocks as possible within an extent */
	ne = mvlen / max_ext_size;
	if (mvlen % max_ext_size)
		ne++;

	ex0 = ex = (extent*)malloc(ne * sizeof(extent));
	if (ex == NULL) {
		fsc_errpr("movex() malloc(ne=%d) failed\n", ne);
		exit(1);
	}
	len2 = mvlen;
	for (ne0 = ne; ne0--; ex0++) {
		len = MIN(len2, max_ext_size);
		ex0->ex_bn = newbn;
		ex0->ex_length = len;
		newbn += len;
		len2 -= len;
	}
	error = vecmovex(mp, fap, ne, ex, offset, mvlen);
	free(ex);
	return error;
}

#define	SIGHOLD()	sighold(SIGINT); sighold(SIGHUP); sighold(SIGTERM);
#define	SIGRELSE()	sigrelse(SIGINT); sigrelse(SIGHUP); sigrelse(SIGTERM);


int
vecmovex(struct efs_mount *mp, struct fscarg *fap,
	int vecne, extent *vecex, off_t offset, int nblocks)
{
	struct fscarg savefap;
	int ret;

	/* keep some common signals from keeping us from leaving
	 * free blocks allocated */
	SIGHOLD();
	savefap = *fap;

	if (vecballoc(fap->fa_dev, vecne, vecex) == -1) {
		SIGRELSE();
		return -1;
	}

	if (veccopy(mp->m_fd, vecex,
		offtoex(fap->fa_ex, fap->fa_ne, offset), nblocks) == -1){
		vecfree(fap->fa_dev, vecne, vecex);
		SIGRELSE();
		return -1;
	}

	if (veccomputex(mp, fap, vecne, vecex, offset, nblocks) == -1) {
		vecfree(fap->fa_dev, vecne, vecex);
		SIGRELSE();
		return -1;
	}

	if (ret = fsc_icommit(fap)) {
		/* BFREE the vecballoc'ed blocks and new indirs */
		vecfree(fap->fa_dev, vecne, vecex);
		vecfree(fap->fa_dev, fap->fa_nie, fap->fa_ix);
		free(fap->fa_ex);
		if (fap->fa_ix)
			free(fap->fa_ix);
		*fap = savefap;
		SIGRELSE();
		return ret;
	}

	/*
	 * free old data and indirect blocks
	 */
	offfree(&savefap, offset, nblocks);
	vecfree(fap->fa_dev, savefap.fa_nie, savefap.fa_ix);
	free(savefap.fa_ex);
	if (savefap.fa_ix)
		free(savefap.fa_ix);
	SIGRELSE();
	return 0;
}

static int
vecballoc(dev_t dev, int vecne, extent *vecex)
{
	int ne;
	extent *ex;

	for (ne = 0, ex = vecex; ne < vecne; ne++, ex++)
		if (fsc_balloc(dev, ex->ex_bn, ex->ex_length) == -1) {
			vecfree(dev, ne, vecex);
			return -1;
		}
	return 0;
}

/*
 * free the blocks described by the extents
 */
static void
vecfree(dev_t dev, int vecne, extent *vecex)
{
	for (; vecne--; vecex++)
		fsc_bfree(dev, vecex->ex_bn, vecex->ex_length);
}

/*
 * free the blocks corresponding to 'offset'+'len'
 */
static void
offfree(struct fscarg *fap, off_t offset, int len)
{
	int nb;
	off_t lastoff = offset + len;
	extent *ex = offtoex(fap->fa_ex, fap->fa_ne, offset);

	while (offset < lastoff) {
		if ((long)ex->ex_offset + (long)ex->ex_length > lastoff)
			nb = lastoff - ex->ex_offset;
		else
			nb = ex->ex_length;
		fsc_bfree(fap->fa_dev, ex->ex_bn, nb);
		offset += nb;
		ex++;
	}
}

/*
 * copy the contents of the data blocks from fap's 'offset'+'nblocks'
 * to the blocks described by 'vecex'
 */
static int
veccopy(int fd, extent *toex, extent *fromex, int nblocks)
{
	int frombn, fromlen;
	int tobn, tolen;
	int copylen;

	tobn = toex->ex_bn;
	tolen = toex->ex_length;
	frombn = fromex->ex_bn;
	fromlen = fromex->ex_length;

	for (;;) {
		copylen = MIN(fromlen, tolen);
		if (copy(fd, frombn, tobn, copylen) == -1)
			return -1;

		if ((nblocks -= copylen) == 0)			/* end loop */
			return 0;
			
		tolen -= copylen;
		if (tolen > 0)
			tobn += copylen;
		else {
			toex++;
			tobn = toex->ex_bn;
			tolen = toex->ex_length;
		}

		fromlen -= copylen;
		if (fromlen > 0)
			frombn += copylen;
		else {
			fromex++;
			frombn = fromex->ex_bn;
			fromlen = fromex->ex_length;
		}
	}
	/* NOTREACHED */
}


/*
 * compute new extents, BALLOC blocks and compute new indir extents
 * returns -1 and fap unchanged on error
 */
static int
veccomputex(struct efs_mount *mp, struct fscarg *fap, int vecne, extent *vecex,
			off_t offset, int nblocks)
{
	extent *ex, *ex1, *newex, *newix, splitex;
	int n0, o0, newne, headne, tailne;
	off_t tailoff;
	int newnie;
	int blendlen = 0;

	splitex.ex_bn = 0;

	/*
	 * compute number of new extents and malloc space
	 * new extent list derived from 4 possible pieces:
	 *	-head		any extents in fap->fa_ex before offset
	 *	-vec		vecex
	 *	-split		the extent created if offset+nblocks
	 *				not on an extent boundary
	 *	-tail		any extents in fap->fa_ex whose ex_offset
	 *				is > offset + nblocks
	 */
	ex = offtoex(fap->fa_ex, fap->fa_ne, offset);
	assert(ex != NULL);
	headne = (ex - fap->fa_ex);
	tailoff = offset + nblocks;
	newne = headne + vecne;

	/* fully blend 1st vecex into last head if possible */
	if (headne > 0) {
		extent *ex0 = &fap->fa_ex[headne - 1];

		if (ex0->ex_bn + ex0->ex_length == vecex->ex_bn
		    && (long)(ex0->ex_length + vecex->ex_length)
			<= max_ext_size)
		{
			blendlen = ex0->ex_length + vecex->ex_length; 
			vecex++; /* careful, may no longer be valid! */
			vecne--; /* when 0, vecex is invalid */
			newne--;
		}
	}

	if (tailoff < (long)getnblocks(fap->fa_ex, fap->fa_ne)) {
		/* file is longer than 'offset' */
		/* find the first extent after offset+nblocks */
		for (ex1 = ex;
		     tailoff > (long)(ex1->ex_offset+ex1->ex_length);
		     ex1++)
			;
		/* ex1 at possibly split extent */
		if (ex1->ex_offset + ex1->ex_length != tailoff) {
			++newne;
			splitex.ex_bn = ex1->ex_bn + (tailoff - ex1->ex_offset);
			splitex.ex_length = ex1->ex_length -
						(tailoff - ex1->ex_offset);
		}
		ex1++;
		tailne = &fap->fa_ex[fap->fa_ne] - ex1;
	} else	/* offset+nblocks goes to the end of the file */
		tailne = 0;
	newne += tailne;
	
	if (newne > EFS_MAXEXTENTS)
		return -1;

	if ((newex = (extent*)malloc(newne * sizeof(extent))) == NULL) {
		fsc_errpr("veccomputex() malloc(ne=%d) failed\n", newne);
		exit(1);
	}

	/*
	 * copy head extents from fap, mid extents from vec, tail from fap
	 */
	if (headne)
		bcopy(fap->fa_ex, newex, headne * sizeof(extent));
	if (blendlen)
		newex[headne-1].ex_length = blendlen;
	if (vecne)
		bcopy(vecex, &newex[headne], vecne * sizeof(extent));
	if (splitex.ex_bn)
		bcopy(&splitex, &newex[headne + vecne], sizeof(extent));
	if (tailne)
		bcopy(ex1, &newex[newne - tailne], tailne * sizeof(extent));
	/*
	 * compute the proper ex_offset for each extent
	 */
	for (o0 = 0, ex = newex, n0 = newne; n0--; ex++) {
		ex->ex_offset = o0;
		ex->ex_magic = 0; /* (AFS data written under efs_iupdat) */
		o0 += ex->ex_length;
	}

	/*
	 * compute number of indir extents and BALLOC a whole new set
	 * of indirect extent blocks
	 */
	if (indir(mp, fap->fa_dev, fap->fa_ino, newne, &newix, &newnie) == -1) {
		free(newex);
		return -1;
	}
	fap->fa_ex = newex;
	fap->fa_ne = newne;
	fap->fa_ix = newix;
	fap->fa_nie = newnie;
	return 0;
}

static int
indir(struct efs_mount *mp, dev_t dev, efs_ino_t ino, int ne, extent **ix,
	int *nie)
{
	int n0, indirbbs;
	extent *ex, *ex1;
	efs_daddr_t newbn;
	int nfree;
	struct efs *fs = mp->m_fs;

	if (ne <= EFS_DIRECTEXTENTS) {
		*nie = 0;
		*ix = NULL;
		return 0;
	}
	*ix = (extent*)malloc(EFS_DIRECTEXTENTS * sizeof(extent));
	if (*ix == NULL) {
		fsc_errpr("indir() malloc(newix) failed\n");
		exit(1);
	}

	indirbbs = (int)BTOBB(ne * sizeof(extent));
	*nie = 0;
	for (ex = *ix; indirbbs > 0; ex++) {
		n0 = MIN(EFS_MAXINDIRBBS, indirbbs);
		newbn = freefs(mp, EFS_ITOCG(fs, ino), n0, &nfree);
		n0 = MIN(n0, nfree);
		indirbbs -= n0;
		if (newbn == -1 ||
		    fsc_balloc(dev, newbn, n0) == -1 ||
		    ++(*nie) > EFS_DIRECTEXTENTS) {
			for (ex1 = *ix; ex1 < ex; ex1++)
				fsc_bfree(dev, ex1->ex_bn, ex1->ex_length);
			/* free(newex); */
			free(*ix);
			return -1;
		}
		ex->ex_bn = newbn;
		ex->ex_length = n0;
	}
	return 0;
}

int
moveix(struct efs_mount *mp, struct fscarg *fap, efs_daddr_t newbn, int ni)
{
	int errsave;
	efs_daddr_t freebn;

	if (fsc_balloc(mp->m_dev, newbn, fap->fa_ix[ni].ex_length) == -1)
		return errno;

	freebn = fap->fa_ix[ni].ex_bn;
	fap->fa_ix[ni].ex_bn = newbn;

	if (errsave = fsc_icommit(fap)) {
		fsc_bfree(mp->m_dev, newbn, fap->fa_ix[ni].ex_length);
		fap->fa_ix[ni].ex_bn = freebn;
		return errsave;
	}
	fsc_bfree(mp->m_dev, freebn, fap->fa_ix[ni].ex_length);
	return 0;
}


void
fprexf(FILE *fp, struct fscarg *fap)
{
	fprex(fp, fap->fa_ne, fap->fa_ex, fap->fa_nie, fap->fa_ix);
}

void
fprex(FILE *fp, int ne, extent *ex, int nie, extent *ix)
{
	if (ne < 1 || ne > EFS_MAXEXTENTS)
		return;
	for(; ne--; ex++)
	    fprintf(fp,"%d+%d(%d)\n",ex->ex_bn, ex->ex_length, ex->ex_offset);
	for(; nie--; ix++)
	    fprintf(fp,"i%d+%d(%d)\n",ix->ex_bn, ix->ex_length, ix->ex_offset);
}

#ifdef notdef

void
dump_free_list(struct efs_mount *mp)
{
	dev_t dev = mp->m_dev;
	struct efs *fs = mp->m_fs;
	int ncyl = fs->fs_ncg;
	int cyl;

	for (cyl = 0; cyl < ncyl; cyl++) {
		efs_daddr_t startbn;
		efs_daddr_t endbn;
		efs_daddr_t len;
		int pcnt;

		fsrprintf("Cyl %d", cyl);

		startbn = EFS_CGIMIN(fs, cyl) + fs->fs_cgisize;
		endbn = EFS_CGIMIN(fs, cyl+1);
		pcnt = 0;

		while (startbn < endbn) {
			len = fsc_tstalloc(dev, startbn);
			if (len != 0) {
				if (len < 0)
					break;
				startbn += len;
				continue;
			}
			len = fsc_tstfree(dev, startbn);
			if (len <= 0)
				break;
			if (!((pcnt++) % 4))
				fsrprintf("\nf");
			fsrprintf(" %d(%d)", startbn, len);
			startbn += len;
		}
		fsrprintf("\n");
	}
}
#endif

int
fsrprintf(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	if (gflag) {
		static int didopenlog;
		if (!didopenlog) {
			openlog("fsr", LOG_PID, LOG_USER);
			didopenlog = 1;
		}
		vsyslog(LOG_INFO, fmt, ap);
	} else
		vprintf(fmt, ap);
	va_end(ap);
	return 0;
}
