/*
 * EFS-aware file system copy
 *
 * 	efscopy fromdev|fromfile todev [todev ...]
 *	efscopy fromdev tofile
 *
 * Each 'todev' must be the same size, but can be different than 'from*' if
 * 'todev' is big enough to hold the inodes and data.
 *
 * We can deal with writing to a file system whose geometry differs from the
 * one we're reading, but to make life easier we don't change anyone's inode
 * number (directory blocks are copied untouched, etc).  We also assume that
 * the destination file system is empty (freshly mkfsed).
 *
 * In general this exits on anything suspicious.
 * 
 * TODO:
 *	support different sized to's
 *	do the "mkfs"
 *	continue in the face of errors
 */
static char ident[] = "@(#) efscopy.c $Revision: 1.2 $";

#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <stdlib.h>
#include <malloc.h>
#include <signal.h>
#include <aio.h>
#include <ustat.h>
#include <string.h>
/*#define NDEBUG /* turns off assert()'s */
#include <assert.h>

extern int errno;

#include "libefs.h"

#define	PROGNAME	"efscopy"	/* for printfs */
#define DISK_ACTIVE	0
#define DISK_NOT_ACTIVE	1
#define GUI 1 
#define	GUI_filename "/tmp/efsdks_file"
/*
 * The number of blocks written (per target).  Used in the progress "bar"
 * and to compute target super block free count.
 */
int flushblks;		

/*
 * When copying the file system we may end up allocating a different number
 * of blocks for indirect extents -- different layout causing files to
 * go from 12 to 13 extents (or vice-versa), etc.  This counter records
 * the _decrease_ in indirect blocks and is printed if -v.
 * This is to reconcile the difference in used block count between the
 * source and destination as reported by fsck and other tools.
 */
int indirfewer;

int debug;	/* might mean anything */

/*
 * Print stuff as we run.  If == 0 print nothing
 */
int vflag;

/* 
 * This 16 is the difference between getting 1.5mb/sec aggregate
 * SCSI bus throughput (4 threads) and ~6mb/sec.
 */
int aiothreads = 16;	/* (aio-internal default is 4) */

int bufblks = 8*EFS_MAXEXTENTLEN;	/* ~1mb seems to work best */

#define	TOMAX	128		/* or OPEN_MAX ... */
EFS_MOUNT *tomp[TOMAX];
int ntos;


struct aiocb asuper[TOMAX];
struct aiocb abit[TOMAX];

void addextents(extent *, int , efs_daddr_t , int, int);
void indiroffsets(extent *, int , char *);
int writefs(EFS_MOUNT *frommp, efs_ino_t maxinum, int blkcount, FILE *p);
int writeinode(EFS_MOUNT *mp, struct efs_dinode *dp, efs_ino_t inum);
extent *mkboth(EFS_MOUNT *mp, struct efs_dinode *fromdp,
			struct efs_dinode *todp, int nblocks, efs_ino_t inum);
void excopy(EFS_MOUNT *frommp, extent *fromex, extent *toex, int nblocks);
void exdatacopy(char *data, int tofd, extent *toex, int nblocks);
void datacopy(int tofd, char *data, efs_daddr_t tobn, int nb);
void copy(int fromfd, efs_daddr_t frombn, int tofd, efs_daddr_t tobn, int nb);
void alloccgs(EFS_MOUNT *mp, efs_ino_t maxinum);
void aio_writecgs(EFS_MOUNT *mp, efs_ino_t maxinum);
efs_daddr_t dalloc(EFS_MOUNT *mp, int nblocks, int *nfound);
void initbuffers(int blks);
void flushcopy(int whichbuf, efs_daddr_t bn, int nbytes);
int ifmountedfail(char *name);

main(int argc, char **argv)
{
	char *fromfile, *tofile;
	EFS_MOUNT *frommp;
	int blkcount = 0;
	int inodecount;
	efs_ino_t maxinum;
	char c, stderr_msg[100];
	extern int optind;
	int i;
	struct efs *fs;
	int toafile = 0;

/*
printf ("aaa [%d] %s %s %s %s]\n", argc, argv[0], argv[1], argv[2], argv[3]);
*/
	while ((c = getopt(argc, argv, "sva:d")) != (char)-1) {
		switch (c) {
		case 'v':	vflag = 1;		break;
		case 'd':	debug = 1;		break;
		}
	}
	if (argc - optind < 2) {
		fprintf(stderr,
		"usage: efscopy [-v] fromdev|fromfile todev [todev ...]\n");
		fprintf(stderr,
		"   or: efscopy [-v] fromdev|fromfile tofile\n");
		exit(1);
	}
	fromfile = argv[optind];
	ifmountedfail(fromfile);

	if ((frommp = efs_mount(fromfile, O_RDONLY)) == 0) {
		fprintf(stderr,"%s: efs_mount(%s, O_RDONLY) failed\n",
			PROGNAME, fromfile);
		exit(1);
	}

	/*
	 * efscopy fromdev tofile
	 */
	if (argc - optind == 2) {
		struct stat sb;

		tofile = argv[optind + 1];
		if (stat(tofile, &sb) != -1) {
			/*
			 * "tofile" exists, exit unless it's a chr device.
			 */
			if (S_ISREG(sb.st_mode)) {
				fprintf(stderr, "%s: %s already exists\n",
					PROGNAME, tofile);
				exit(1);
			} else if (!S_ISCHR(sb.st_mode)) {
				fprintf(stderr,
				"%s: %s must be chr dev or non-existent file\n",
				PROGNAME, tofile);
				exit(1);
			} /* else it's CHR and that's okay by me */
		} else if (errno == ENOENT) {
			/*
			 * File doesn't exist, create it...
			 */
			int fd;
			struct efs fs;

			if ((fd = open(tofile, O_WRONLY|O_CREAT, 0644)) == -1) {
#ifdef GUI
				fprintf(stderr, 
				"%s: %s=FAIL, open(%s) failed errno=%d\n",
				PROGNAME, tofile, errno);

				sprintf(stderr_msg, 
				"%s: %s=FAIL, open(%s) failed errno=%d\n",
				PROGNAME, tofile, errno);

				process_diskunit_error(-1, stderr_msg);
#else
				fprintf(stderr,"%s: open(%s) failed errno=%d\n",
					PROGNAME, tofile, errno);
#endif
				exit(1);
			}

			/*
			 * ...and, give it a superblock which will pass
			 * efs_mount and the capacity tests below.
			 */
			bcopy(frommp->m_fs, &fs, sizeof(struct efs));
			fs.fs_tfree = CAPACITYBLKS(&fs);
			fs.fs_tinode = CAPACITYINODES(&fs);
			if (efs_writeb(fd, (char *)&fs,
					BTOBB(EFS_SUPERBOFF), 1) != 1) {
#ifdef GUI
				fprintf(stderr, 
				"%s: %s=FAIL, efs_writeb(%s) failed\n",
				PROGNAME, tofile);

				sprintf(stderr_msg, 
				"%s: %s=FAIL, efs_writeb(%s) failed\n",
				PROGNAME, tofile);

				process_diskunit_error(-1, stderr_msg);
#else
				fprintf(stderr,"%s: efs_writeb(%s) failed\n",
					PROGNAME, tofile);
#endif
				exit(1);
			}
			close(fd);
			/*
			 * Now fall through to efs_mount as normal
			 */
			toafile = 1;
		} else {
			fprintf(stderr,"%s: stat(%s) failed errno=%d\n",
				PROGNAME, tofile, errno);
			exit(1);
		}
	}

	/*
	 * efscopy fromdev|fromfile todev ...
	 */
	for (optind++, i = 0; optind < argc; optind++, i++) {
		struct efs *fs;
		tofile = argv[optind];

		ifmountedfail(tofile);

		if ((tomp[i] = efs_mount(tofile, O_RDWR)) == 0) {
			fprintf(stderr,"%s: efs_mount(%s, O_RDWR) failed\n",
				PROGNAME, tofile);
			exit(1);
		}
#ifdef GUI
		tomp[i]->active = DISK_ACTIVE;
		if (strlen(tofile) > 39) printf("Too Long-(%s)\n", tofile);
		strcpy(tomp[i]->tofile, tofile);
#endif
		fs = tomp[i]->m_fs;
		/*
		 * all targets must be same size
		 */
		if (CAPACITYBLKS(fs) != CAPACITYBLKS(tomp[0]->m_fs)) {
#ifdef GUI
			fprintf(stderr, "%s: %s=FAIL, not same size\n",
				PROGNAME, tofile);
			sprintf(stderr_msg, "%s: %s=FAIL, not same size\n",
				PROGNAME, tofile);
			process_diskunit_error(i, stderr_msg);
#else
			fprintf(stderr, "%s: %s not same size\n",
				PROGNAME, tofile);
			exit(1);
#endif
		}
		/*
		 * assumed to be freshly mkfs'ed.  if capacity - currently
		 * free > 22 (dirblks for . and lost+found)
		 */
		if (CAPACITYBLKS(fs) - fs->fs_tfree > 22 ||
	    	    CAPACITYINODES(fs) - fs->fs_tinode > 2) {
#ifdef GUI
			fprintf(stderr, "%s=FAIL, not empty, mkfs before using %s\n",
				tofile, PROGNAME);
			sprintf(stderr_msg, "%s=FAIL, not empty, mkfs before using %s\n",
				tofile, PROGNAME);
			process_diskunit_error(i, stderr_msg);
#else
			fprintf(stderr, "%s not empty, mkfs before using %s\n",
				tofile, PROGNAME);
			exit(1);
#endif
		}
	}
	ntos = i;

	if (toafile) {
		tomp[0]->m_bitmap =
		(char *)calloc(1, BBTOB(BTOBB(tomp[0]->m_fs->fs_bmsize)));
	} else {
		if (efs_bget(tomp[0]) == -1) {
			fprintf(stderr,"%s: efs_bget(%s) failed\n",
				PROGNAME, tomp[0]->m_devname);
			exit(1);
		}
		for (i = 1; i < ntos; i++) {
#ifdef GUI
			if (tomp[i]->active == DISK_NOT_ACTIVE) {
				continue;
			}
#endif
			tomp[i]->m_bitmap = tomp[0]->m_bitmap;
		}
	}

	maxinum = efs_readinodes(frommp, vflag ? stdout : 0, &blkcount, debug);
	if (maxinum == 0) {
		fprintf(stderr, "%s: efs_readinodes returned 0 (%s mounted?)\n",
			PROGNAME, fromfile);
		exit(1);
	}
	/* is "tofile" big enough? */
	if (CAPACITYBLKS(tomp[0]->m_fs) < blkcount ||
	    CAPACITYINODES(tomp[0]->m_fs) < maxinum) {
		fprintf(stderr,
		"%s: %s too small (%d blks) to hold data in %s (%d blks)\n",
			PROGNAME,
			tofile, CAPACITYBLKS(tomp[0]->m_fs),
			fromfile, blkcount);
		exit(1);
	}
	/* allocate enough cg's of efs_dinodes (and "mkfs" them) */
	alloccgs(tomp[0], maxinum);
	for (i = 1; i < ntos; i++) {
#ifdef GUI
		if (tomp[i]->active == DISK_NOT_ACTIVE) {
			continue;
		}
#endif
		tomp[i]->m_ilist = tomp[0]->m_ilist;
	}

	/* do the real work */
	_aio_init(aiothreads);
	initbuffers(bufblks);
	inodecount = writefs(frommp, maxinum, blkcount, vflag ? stdout : 0);
	flushcurrent();

	/* recompute superblock free counts and write it out */
	fs = tomp[0]->m_fs;
	fs->fs_tfree = CAPACITYBLKS(fs) - flushblks;
	fs->fs_tinode =  CAPACITYINODES(fs) - inodecount;
	fs->fs_checksum = efs_checksum(tomp[0]->m_fs);
	/*
	 * Tell the new fsck about the last (highest number) inode.
	 * WARNING! Not doing this means fsck will make complaints
	 * like "ENTRY INUM OUTRANGE IN DIR BLK" which will cause "fsck -y"
	 * to nuke the file from the file system.
	 */
	fs->fs_lastialloc = maxinum;

	efs_aio_writeb(asuper, (char *)fs, BTOBB(EFS_SUPERBOFF), 1);
	
	/* write out the bitmap (computed in dalloc()) */
	efs_aio_writeb(abit, tomp[0]->m_bitmap,
		tomp[0]->m_fs->fs_bmblock
			? tomp[0]->m_fs->fs_bmblock
			: EFS_BITMAPBB,
		BTOBB(tomp[0]->m_fs->fs_bmsize));

	/* write out cg's */
	aio_writecgs(tomp[0], maxinum);

	efs_aio_wait(asuper);
	efs_aio_wait(abit);

	if (vflag) {
		printf("block delta %d\n", indirfewer);
	}
	exit(0);
}


/*
 * (blkcount is for the progress bar and might differ from the count --
 *  fewer indirect blocks may get allocated in the dest file system)
 */
int
writefs(EFS_MOUNT *frommp, efs_ino_t maxinum, int blkcount, FILE *progress)
{
	efs_ino_t inum, i;
	struct efs_dinode *dp;
	int inodecount = 0;
	int progressblks = 0, nextval = 10;

	if (progress) {
		fprintf(progress, "writing %d blocks..", blkcount);
		fflush(progress);
	}
	for (inum = 2, i = 0; inum <= maxinum; inum++, i++) {
		dp = INUMTODI(frommp, inum);
		if (dp->di_mode == 0)
			continue;

		inodecount++;

		progressblks += writeinode(frommp, dp, inum);
		/* XXX files larger than 10% will mess this up, oh well */
		if (progress && progressblks && blkcount / progressblks < 10) {
			fprintf(progress, "%d%%..", nextval);
			fflush(progress);
			progressblks = 0;
			nextval += 10;
		}
	}

	if (progress) {
		fprintf(progress, "done\n");
		fflush(progress);
	}
		
	return inodecount;
}

/*
 * In order to save recomputing dirblks and allocating inodes we do something
 * slightly cheesy... (we use the same inode numbers and copy the dirblks
 * right over).  After not looking at this code for a few months I've decided
 * that preserving inode numbers is not horribly cheesy after all...
 * it's a _really_ nice debug feature!
 */
int
writeinode(EFS_MOUNT *mp, struct efs_dinode *dp, efs_ino_t inum)
{
	struct efs_dinode *todp = INUMTODI(tomp[0], inum);
	extent *fromex, *toex;
	int nblocks;

	if (todp->di_mode != 0) {
		fprintf(stderr,
			"%s: writeinode todp->di_mode non-zero inum=%d\n",
			PROGNAME, inum);
		exit(1);
	}

	bcopy(dp, todp, sizeof(struct efs_dinode));

	switch (dp->di_mode & IFMT) {
	case IFREG: 
		if (dp->di_size == 0)
			break;

		if (dp->di_numextents > EFS_DIRECTEXTENTS)
			fromex = DIDATA(dp, didata *)->ex;
		else
			fromex = dp->di_u.di_extents;

		nblocks = DATABLKS(fromex, dp->di_numextents);

		toex = mkboth(mp, dp, todp, nblocks, inum);

		/* read/write file data */
		excopy(mp, fromex, toex, nblocks);

		break;
	case IFDIR:
		nblocks = DIDATA(dp, didata *)->dirblocks;

		toex = mkboth(mp, dp, todp, nblocks, inum);

		exdatacopy(DIDATA(dp, didata *)->dirblkp,
				tomp[0]->m_fd, toex, nblocks);

		break;
	case IFLNK:
		/* (how long can links get anyways?) */
		fromex = dp->di_u.di_extents;
		nblocks = DATABLKS(fromex, dp->di_numextents);
		toex = mkboth(mp, dp, todp, nblocks, inum);
		/* read/write link data */
		excopy(mp, fromex, toex, nblocks);
		break;
	case IFIFO:
	case IFCHR:
	case IFBLK:
	case IFSOCK:
		nblocks = 0;
		break;
	default:
		fprintf(stderr,"%s: writeinode unknown mode?!? 0x%x inum=%d\n", 
			PROGNAME, dp->di_mode & IFMT, inum);
		exit(1);
	}

	/* if extents indirect then direct extents were malloc'ed in mkboth */
	if (todp->di_numextents > EFS_DIRECTEXTENTS)
		free(toex);
	/* (else extents are direct an in the efs_dinode) */

	return nblocks;
}


/*
 * make both: 1) the efs_dinode (in todp) and 2) the extents
 */
extent *
mkboth(EFS_MOUNT *mp, struct efs_dinode *fromdp, struct efs_dinode *todp,
	int nblocks, efs_ino_t inum)
{
	int nb, nfound, offset;
	efs_daddr_t bn;
	extent *toex, *lastex;
	int indirblks;

	nb = nblocks;

	/*
	 * Create extents while allocating data blocks.
	 * Start by assuming the file won't go indirect.
	 */
	offset = 0;
	toex = todp->di_u.di_extents;
	lastex = &todp->di_u.di_extents[EFS_DIRECTEXTENTS];
	todp->di_numextents = 0;	 /* (todp a copy of fromdp) */
	while (nblocks) {
		int newnumex, oldnumex = todp->di_numextents;
		/* allocate data blocks */
		bn = dalloc(tomp[0], nblocks, &nfound);

		newnumex = nfound/EFS_MAXEXTENTLEN +
				(nfound % EFS_MAXEXTENTLEN ? 1 : 0) +
				oldnumex;

		if (toex + newnumex > lastex) {
			/*
 			 * Allocate space for indir extents by disk blocks.
			 */
#define	EXBYTES(n) ((n) * sizeof(extent))
#define	EXBLKS(n) (EXBYTES(n)/BBSIZE + (EXBYTES(n) % BBSIZE ? 1 : 0))
			if (oldnumex <= EFS_DIRECTEXTENTS) {
				/* just now going indir */
				extent *tmpex;
				indirblks = EXBLKS(newnumex);

				tmpex = (extent *)calloc(indirblks, BBSIZE);
				if (tmpex == 0) {
					fprintf(stderr, "%s: out of mem!\n",
						PROGNAME);
					exit(1);
				}
				bcopy(toex, tmpex, oldnumex * sizeof(extent));
				toex = tmpex;
			} else {
				int newindirblks = EXBLKS(newnumex);
				toex = (extent *)
					realloc(toex, BBSIZE*newindirblks);
				if (toex == 0) {
					fprintf(stderr, "%s: out of mem!\n",
						PROGNAME);
					exit(1);
				}
				/* zero the new blks */
				bzero((char *)toex + BBSIZE * indirblks,
					BBSIZE*(newindirblks - indirblks));
				indirblks = newindirblks;
			}
			lastex = (extent *)
				((char *)toex + BBSIZE * indirblks - 1);
		}

		/*
		 * Add the extents to describe bn+nfound at offset.
		 */
		addextents(toex + oldnumex, offset, bn, nfound,
					EFS_MAXEXTENTLEN);

		todp->di_numextents = newnumex;

		offset += nfound;
		nblocks -= nfound;
	}

	/* (the indirect extents show up after the file... oh well) */
	if (todp->di_numextents > EFS_DIRECTEXTENTS) {
		int numindirex = 0, offset = 0;
		/* save a pointer to the extents */
		extent *ex = toex, *iex = todp->di_u.di_extents;

		/* figure number of indir extent blocks */
		int nb, nblocks;

		nb = nblocks = indirblks;

		/* allocate and write out blocks for indirect extents */
		while (nblocks) {
			int bn = dalloc(tomp[0], nblocks, &nfound);

			int newex = nfound/EFS_MAXINDIRBBS +
				(nfound % EFS_MAXINDIRBBS ? 1 : 0);

			/* we do the offsets below */
			addextents(iex + numindirex, 0, bn, nfound,
							EFS_MAXINDIRBBS);
			numindirex += newex;
			nblocks -= nfound;
		}

		/* indirect extent ex_offsets are different.. */
		indiroffsets(iex, numindirex, (char *)ex);

		/* write the extents into the indirect blocks */
		exdatacopy((char *)ex, tomp[0]->m_fd, iex, nb);
	}

	/*
	 * Record the decrease in blocks used for indirect extents
	 */
#define	_INDIRBLKS(n) (n <= EFS_DIRECTEXTENTS ? 0 : EXBLKS(n))
	indirfewer += _INDIRBLKS(fromdp->di_numextents) -
						_INDIRBLKS(todp->di_numextents);

	return toex;
}

/*
 * Decompose bn+len into maximally sized extents at offset.
 * (assumes ex points to enough space)
 */
void
addextents(extent *ex, int offset, efs_daddr_t bn, int len, int maxexlen)
{
#ifdef	NDEBUG
	if (offset != 0) { /* there are previous extents */
		extent *exprev = ex - 1;
		assert(exprev->ex_offset + exprev->ex_length == offset);
	}
#endif
	while (len) {
		int thislen = MIN(len, maxexlen);
		ex->ex_offset = offset;
		ex->ex_bn = bn;
		ex->ex_length = thislen;
		offset += thislen;
		bn += thislen;
		len -= thislen;
		ex++;
	}
}

/*
 * In the indirect extents the 0th ex_offset is the number of indirect
 * extents.  Each following ex_offset is file data offset (not direct
 * extent offset) -- same as the ex_offset of the first extent of that
 * set of direct extents.
 */
void
indiroffsets(extent *iex, int ni, char *ex)
{
	extent *end = iex + ni;
	int nblocks;

	iex->ex_offset = ni;
	nblocks = iex->ex_length;
	iex++;
	for (; iex < end; iex++) {
		iex->ex_offset = ((extent *)(ex + nblocks * BBSIZE))->ex_offset;
		nblocks = iex->ex_length;
	}
}



/* (nblocks > 0) */
void
excopy(EFS_MOUNT *frommp, extent *fromex, extent *toex, int nblocks)
{
	efs_daddr_t frombn, tobn;
	int fromlen, tolen, copylen;

	tobn = toex->ex_bn;
	tolen = toex->ex_length;
	frombn = fromex->ex_bn;
	fromlen = fromex->ex_length;

	for (;;) {
		copylen = MIN(fromlen, tolen);
		copy(frommp->m_fd, frombn, tomp[0]->m_fd, tobn, copylen);

		if ((nblocks -= copylen) == 0) {		/* end loop */
			return;
		}
			
		tolen -= copylen;
		if (tolen > 0) {
			tobn += copylen;
		} else {
			toex++;
			tobn = toex->ex_bn;
			tolen = toex->ex_length;
		}

		fromlen -= copylen;
		if (fromlen > 0) {
			frombn += copylen;
		} else {
			fromex++;
			frombn = fromex->ex_bn;
			fromlen = fromex->ex_length;
		}
	}
	/* NOTREACHED */
}


/*
 * write-buffered copy
 *
 * precopy(), postcopy(), flushcopy() know about the buffering "internals"
 * copy(), exdatacopy(), datacopy() don't
 */
#define	NBUFS 2
static char *buf[NBUFS];
static char *bufp, *readp;
static efs_daddr_t wbn;
static int readbuf;
#define	POSTCOPY(nblocks)	(readp += (nblocks) * BBSIZE)

struct aiocb ap[NBUFS][TOMAX];
struct sigaction sa;

void
initbuffers(int blks)
{
	buf[0] = malloc(blks * BBSIZE);
	buf[1] = malloc(blks * BBSIZE);

	if (buf[0] == 0 || buf[1] == 0) {
		fprintf(stderr, "%s: failed to malloc %d blks of buffering",
			PROGNAME, blks);
		exit(1);
	}
}

char *
precopy(efs_daddr_t tobn, int nb)
{
	if (readp == 0) { /* instead of some external init routine */
		bufp = buf[0];
		readp = bufp;
		wbn = tobn;
	}
	assert(tobn != 0);
	assert(nb != 0);
#define	BLKSINBUF		((readp - bufp)/BBSIZE)
	if (wbn + BLKSINBUF != tobn || BLKSINBUF + nb > bufblks) {
		/* buffer's full... flush it and switch to new */
		flushcurrent();

		wbn = tobn;
		/* ...wait for writes to finish in the new buf... */
		waitbuf(readbuf);
		/* ...and return pointing the reader here */
	}
	return readp;
}

/* "external" interface to flush the last read(s) */
flushcurrent()
{
	flushcopy(readbuf, wbn, readp - bufp);
	readbuf = readbuf == 0 ? 1 : 0;
	readp = bufp = buf[readbuf];
}


void
onwritedone()
{
	/*
	 * do all the work in the main line since we don't know which
	 * write just completed
	 */
}

/*
 * Write nbytes of buf[whichbuf] to bn
 */
void
flushcopy(int whichbuf, efs_daddr_t bn, int nbytes)
{
	int i, done = 0;
	char stderr_msg[100];

	if (ap[whichbuf][0].aio_fildes == 0) { /* first time init of aiocb's */
		for (i = 0; i < ntos; i++) {
#ifdef GUI
			if (tomp[i]->active == DISK_NOT_ACTIVE) {
				continue;
			}
#endif
			ap[whichbuf][i].aio_reqprio = 0;
			ap[whichbuf][i].aio_buf = buf[whichbuf];
			ap[whichbuf][i].aio_fildes = tomp[i]->m_fd;
			ap[whichbuf][i].aio_sigevent.sigev_signo = SIGUSR1;
		}
		sa.sa_handler = onwritedone;
		sa.sa_flags = SA_RESTART;
		if (sigaction(SIGUSR1, &sa, 0) == -1) {
			fprintf(stderr,"%s: sigaction failed errno=%d\n",
				PROGNAME, errno);
			exit(1);
		}
	}

	for (i = 0; i < ntos; i++) {
#ifdef GUI
		if (tomp[i]->active == DISK_NOT_ACTIVE)
			continue;
#endif
		ap[whichbuf][i].aio_nbytes = nbytes;
		ap[whichbuf][i].aio_offset = bn * BBSIZE;
		if (aio_write(&ap[whichbuf][i]) == -1) {
#ifdef GUI
			sprintf(stderr_msg,"%s: aio_write failed errno=%d\n",
				PROGNAME, errno);
			process_diskunit_error(i, stderr_msg);
#else
			fprintf(stderr,"%s: aio_write failed errno=%d\n",
				PROGNAME, errno);
			exit(1);
#endif
		}
	}
	flushblks += nbytes/BBSIZE;/* gather stats to compute super block */
}

/*
 * return when all aio_write's for whichbuf have completed
 */
waitbuf(int whichbuf)
{
	int i;
	char stderr_msg[100];
	int r, e;

	for (;;) {
		int done = 0;
		for (i = 0; i < ntos; i++) {
#ifdef GUI
			if (tomp[i]->active == DISK_NOT_ACTIVE)
				continue;
#endif
			e = aio_error(&ap[whichbuf][i]);
			if (e == EINPROGRESS) {
				done++;
			} else if (e != 0 ||
			   	ap[whichbuf][i].aio_nbytes !=
					(r = aio_return(&ap[whichbuf][i]))) {
#ifdef GUI

				sprintf(stderr_msg, "%s: write failed on %s\n", 
					PROGNAME, tomp[i]->m_devname);
				process_diskunit_error(i, stderr_msg);
#else
				fprintf(stderr, "%s: write failed on %s\n", 
					PROGNAME, tomp[i]->m_devname);
				exit(1);
#endif
			}
		}
		if (done == 0) {
			return;
		}
		sginap(0); /* wait for a signal from async write */
	}
	/* NOTREACHED */
}

void
copy(int fromfd, efs_daddr_t frombn, int tofd, efs_daddr_t tobn, int nb)
{
	char *bp = precopy(tobn, nb);
	if (efs_readb(fromfd, bp, frombn, nb) != nb) {
		fprintf(stderr,"%s: copy() efs_readb failed!\n", PROGNAME);
		exit(1);
	}
	POSTCOPY(nb);
}

/*
 * yeah, yeah... this copies from "data" to the write buffer (as opposed to
 * somehow telling "flushcopy()" where to write from and not copying first),
 * but we assume this is used for dirblks, links and indirect extents which
 * is minimal data compared to file data (copyed by excopy/copy)
 */
void
exdatacopy(char *data, int tofd, extent *toex, int nblocks)
{
	efs_daddr_t bn;
	int length;

	while (nblocks) {
		bn = toex->ex_bn;
		length = toex->ex_length;

		datacopy(tofd, data, bn, length);
		data += (length * BBSIZE);
		toex++;
		nblocks -= length;
	}
}

void
datacopy(int tofd, char *data, efs_daddr_t tobn, int nb)
{
	char *bp = precopy(tobn, nb);
	bcopy(data, bp, nb * BBSIZE);
	POSTCOPY(nb);
}

/*
 * Allocate as many i-lists as needed to hold "maxinum".
 * "mkfs" each efs_dinode (all 0's except di_gen).
 */
void
alloccgs(EFS_MOUNT *mp, efs_ino_t maxinum)
{
	int i, ncg = EFS_ITOCG(mp->m_fs, maxinum) + 1;
	struct efs_dinode *di, *enddi;
	long igen;

	time(&igen);

	mp->m_ilist = (struct efs_dinode **)
				malloc(sizeof (struct efs_dinode *) * ncg);

	for (i = 0; i < ncg; i++) {
		mp->m_ilist[i] = (struct efs_dinode *)
					calloc(mp->m_fs->fs_cgisize, BBSIZE);
		enddi = (struct efs_dinode *) \
			((char*)mp->m_ilist[i] + mp->m_fs->fs_cgisize * BBSIZE);
		for (di = mp->m_ilist[i]; di < enddi; di++) {
			di->di_gen = igen;
		}
	}
}


struct aiocb acg[2][TOMAX];

void
aio_writecgs(EFS_MOUNT *mp, efs_ino_t maxinum)
{
	int i, ncg = EFS_ITOCG(mp->m_fs, maxinum) + 1;

	for (i = 0; i < ncg; i++) {
		efs_aio_writeb(acg[i & 0x1],
				(char *)mp->m_ilist[i],
				EFS_CGIMIN(mp->m_fs, i),
				mp->m_fs->fs_cgisize);
		if (i != 0) {
			efs_aio_wait(acg[(i - 1) & 0x1]);
		}
	}
	efs_aio_wait(acg[(ncg - 1) & 0x1]);
}


/*
 * dirt simple allocator -- a cg-aware counter
 */
efs_daddr_t
dalloc(EFS_MOUNT *mp, int nblocks, int *nfound)
{
	static int freebn, cgfree, cgno;
	int foundbn;

	if (freebn == 0) { /* happens once first time */
		freebn = EFS_CGIMIN(mp->m_fs, 0) + mp->m_fs->fs_cgisize;
		cgfree = mp->m_fs->fs_cgfsize - mp->m_fs->fs_cgisize;
	}

	foundbn = freebn;

	if (nblocks > cgfree) {
		nblocks = cgfree;
	}

	*nfound = nblocks;

	cgfree -= nblocks;
	if (cgfree == 0) {
		cgno++;
		freebn = EFS_CGIMIN(mp->m_fs, cgno) + mp->m_fs->fs_cgisize;
		cgfree = mp->m_fs->fs_cgfsize - mp->m_fs->fs_cgisize;
	} else {
		freebn += nblocks;
	}

	markused(mp->m_bitmap, foundbn, nblocks);

	return foundbn;
}

markused(char *bm, efs_daddr_t bn, int nb)
{
	for (;nb--;bn++) {
		bclr(bm, bn);
	}
}

/*
 * kick off an aio_write of buf to blk+nblks for each of nfd aiocb's
 */
efs_aio_writeb(struct aiocb *ap, char *buf, efs_daddr_t blk, int nblks)
{
	char stderr_msg[100];
	int i;
	ap, buf, blk, nblks;
	
	if (!sa.sa_handler) {
		sa.sa_handler = onwritedone;
		sa.sa_flags = SA_RESTART;
		if (sigaction(SIGUSR1, &sa, 0) == -1) {
			fprintf(stderr,"%s: sigaction failed errno=%d\n",
				PROGNAME, errno);
			exit(1);
		}
	}

	for (i = 0; i < ntos; i++) {
#ifdef GUI
		if (tomp[i]->active == DISK_NOT_ACTIVE)
			continue;
#endif
		ap[i].aio_reqprio = 0;
		ap[i].aio_buf = buf;
		ap[i].aio_fildes = tomp[i]->m_fd;
		ap[i].aio_sigevent.sigev_signo = SIGUSR1;
		ap[i].aio_nbytes = nblks * BBSIZE;
		ap[i].aio_offset = blk * BBSIZE;
		if (aio_write(&ap[i]) == -1) {
#ifdef GUI
			sprintf(stderr_msg,"%s: aio_write failed errno=%d dev=%s\n",
				PROGNAME, errno, tomp[i]->m_devname);
			process_diskunit_error(i, stderr_msg);
#else
			fprintf(stderr,"%s: aio_write failed errno=%d dev=%s\n",
				PROGNAME, errno, tomp[i]->m_devname);
			exit(1);
#endif
		}
	}
}

/*
 * wait for all aio's to finish.  exits on any error
 */
efs_aio_wait(struct aiocb *ap)
{
	int i, e;
	char stderr_msg[100];
loop:
	for (i = 0; i < ntos; i++) {
#ifdef GUI
		if (tomp[i]->active == DISK_NOT_ACTIVE)
			continue;
#endif
		e = aio_error(&ap[i]);
		if (e == EINPROGRESS) {
			sginap(0); /* wait for a signal from async write */
			goto loop;
		} else if (e != 0 || ap[i].aio_nbytes != aio_return(&ap[i])) {
#ifdef GUI
			sprintf(stderr_msg, "%s: write failed on %s\n",
				PROGNAME, tomp[i]->m_devname);
			process_diskunit_error(i, stderr_msg);
#else
			fprintf(stderr, "%s: write failed on %s\n",
				PROGNAME, tomp[i]->m_devname);
			exit(1);
#endif
		}
	}
	/* everyone's done and successful, return */
}

ifmountedfail(char *name)
{
	struct ustat ustatarea;
	struct stat sb;
	
	if (stat(name, &sb) < 0) return (0);

	if (((sb.st_mode & S_IFMT) != S_IFCHR) &&
	    ((sb.st_mode & S_IFMT) != S_IFBLK))
		return (0);

	if (ustat(sb.st_rdev,&ustatarea) >= 0) {
		fprintf(stderr, "%s: unmount %s before using %s\n",
			PROGNAME, name, PROGNAME);
		exit(1);
	}
	return (0);
}

/*
 * The disk units that have failed are set to NOT_ACTIVE.
 */
process_diskunit_error(i, msg)
int i;
char *msg;
{
	char fail_msg[200], file_name[20], name[20], *d_ptr, *e_ptr;
#ifdef GUI
	tomp[i]->active = DISK_NOT_ACTIVE;
	if (i != -1) {
		d_ptr =  strstr(tomp[i]->tofile, "dks");
		e_ptr = strstr(d_ptr, "s0");
		*e_ptr = '\0';
	}
	strcpy (name, d_ptr);
	sprintf (file_name, "/tmp/%s.err", name);
	append_tmp_file(file_name, msg);
#endif
}

#ifdef GUI
append_tmp_file(filename, write_line)
char *filename;
char *write_line;
{
	FILE		*fp;
	struct stat	file_info;
	int		stat;
	long		bytes_wrote;
	int		line_size;

	fp = fopen(filename, "w");
	if (fp == NULL){
		printf ("append file (%s) not found\n", filename);
		return;
	}

	line_size = strlen(write_line);
	bytes_wrote = fwrite(write_line, 1, line_size, fp);

	fclose(fp);
}
#endif
