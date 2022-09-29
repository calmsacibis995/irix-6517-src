#ident "$Revision: 1.14 $"

/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 * static char sccsid[] = "@(#)tape.c	5.6 (Berkeley) 5/2/86";
 */

#include "restore.h"
#include "dumprmt.h"
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/mkdev.h>
#include <sys/mtio.h>
#include <sys/tpsc.h>
#include <setjmp.h>

static long	fssize = MAXBSIZE;
static int	mt = -1;
static int	pipein = 0;
static char	magtape[BUFSIZ];
static int	bct;
static char	*tbf;
static union	u_spcl endoftapemark;
static long	blksread;
static long	tapesread;
static jmp_buf	restart;
static int	gettingfile = 0;	/* restart has a valid frame */

static int	ofile;
static char	*map;
static char	lnkbuf[MAXPATHLEN + 1];
static int	pathlen;
static int	nvol;

static int	Bcvt;		/* Swap Bytes (for CCI or sun) */
static int	Qcvt;		/* Swap quads (for sun) */
/* local or remote I/O functions */
static int	(*Ropen)(const char *, int, ...);
static int	(*Rclose)(int);
static ssize_t	(*Rread)(int, void *, size_t);
static off_t	(*Rseek)(int, off_t, int);
static int	(*Rioctl)(int, int, ...);

static void accthdr(struct s_spcl *);
static int checksum(int *);
static int checktype(struct s_spcl *, int);
static int checkvol(struct s_spcl *, long);
static void chkandfixaudio(int);
static dev_t conv_devnum(dev_t);
static void findinode(struct s_spcl *);
static void findtapeblksize(void);
static void flsht(void);
static int gethead(struct s_spcl *);
static int ishead(struct s_spcl *);
static int myrmtioctl(int, int, ...);
static ssize_t myrmtread(int, void *, size_t);
static off_t myrmtseek(int, off_t, int);
static int readhdr(struct s_spcl *);
static void readtape(char *);
static void setdumpnum(void);
static long swabl(int);
static void swabst(char *, char *);
static void xtrfile(char *, long);
static void xtrlnkfile(char *, long);
static void xtrlnkskip(char *, long);
static void xtrmap(char *, long);
static void xtrmapskip(char *, long);
static void xtrskip(char *, long);

/*
 * Set up an input source
 */
void
setinput(char *source)
{
	char *host, *tape;

	flsht();
	if (bflag)
		newtapebuf(ntrec);
	else
#if NTREC > HIGHDENSITYTREC
		newtapebuf(NTREC);
#else
		newtapebuf(HIGHDENSITYTREC);
#endif
	terminal = stdin;
	if ((tape = index(source, ':')) != NULL) {
		host = source;
		*tape++ = '\0';
		(void) strcpy(magtape, tape);
		if (rmthost(&host) == 0)
			done(1);
		Ropen = rmtopen;
		Rclose = rmtclose;
		Rread = myrmtread;
		Rseek = myrmtseek;
		Rioctl = myrmtioctl;
	} else {
		(void) strcpy(magtape, source);
		Ropen = open;
		Rclose = close;
		Rread = read;
		Rseek = lseek;
		Rioctl = ioctl;
	}
	setuid(getuid());	/* no longer need or want root privileges */
	if (strcmp(source, "-") == 0) {
		/*
		 * Since input is coming from a pipe we must establish
		 * our own connection to the terminal.
		 */
		terminal = fopen("/dev/tty", "r");
		if (terminal == NULL) {
			perror("Cannot open(\"/dev/tty\")");
			terminal = fopen("/dev/null", "r");
			if (terminal == NULL) {
				perror("Cannot open(\"/dev/null\")");
				done(1);
			}
		}
		pipein++;
	}
}

void
newtapebuf(long size)
{
	static int tbfsize = -1;

	ntrec = size;
	if (size <= tbfsize)
		return;
	if (tbf != NULL)
		free(tbf);
	tbf = (char *)malloc(size * TP_BSIZE);
	if (tbf == NULL) {
		fprintf(stderr, "Cannot allocate space for tape buffer\n");
		done(1);
	}
	tbfsize = size;
}

/*
 * Verify that the tape drive can be accessed and
 * that it actually is a dump tape.
 */
void
setup(void)
{
	int i, j, *ip;

	Vprintf(stdout, "Verify tape and initialize maps\n");
	if (pipein)
		mt = 0;
	else if ((mt = (*Ropen)(magtape, 0)) < 0)
	{
		perror(magtape);
		done(1);
	}
	chkandfixaudio(mt);
	volno = 1;
	setdumpnum();
	flsht();
	if (!pipein && !bflag)
		findtapeblksize();
	if (gethead(&spcl) == FAIL) {
		{
			fprintf(stderr, "Tape is not a dump tape\n");
			done(1);
		}
		fprintf(stderr, "Converting to new file system format.\n");
	}
	efsdirs = (spcl.c_flags & DR_EFSDIRS) ? 1 : 0;
	if (efsdirs)
		fprintf(stderr, "Converting from EFS directory format.\n");
	if (pipein) {
		endoftapemark.s_spcl.c_magic = cvtflag ? OFS_MAGIC : NFS_MAGIC;
		endoftapemark.s_spcl.c_type = TS_END;
		ip = (int *)&endoftapemark;
		j = sizeof(union u_spcl) / sizeof(int);
		i = 0;
		do
			i += *ip++;
		while (--j);
		endoftapemark.s_spcl.c_checksum = CHECKSUM - i;
	}
	if (vflag || command == 't')
		printdumpinfo();
	dumptime = spcl.c_ddate;
	dumpdate = spcl.c_date;
	if (((fssize - 1) & fssize) != 0) {
		fprintf(stderr, "bad block size %d\n", fssize);
		done(1);
	}
	if (checkvol(&spcl, (long)1) == FAIL) {
		fprintf(stderr, "Tape is not volume 1 of the dump\n");
		done(1);
	}
	if (readhdr(&spcl) == FAIL)
		panic("no header after volume mark!\n");
	findinode(&spcl);
	if (checktype(&spcl, TS_CLRI) == FAIL) {
		fprintf(stderr, "Cannot find file removal list\n");
		done(1);
	}
	maxino = (spcl.c_count * TP_BSIZE * NBBY) + 1;
	Dprintf(stdout, "maxino = %d\n", maxino);
	map = calloc((unsigned)1, (unsigned)howmany(maxino, NBBY));
	if (map == (char *)NIL)
		panic("no memory for file removal list\n");
	clrimap = map;
	curfile.action = USING;
	getfile(xtrmap, xtrmapskip);
	if (checktype(&spcl, TS_BITS) == FAIL) {
		fprintf(stderr, "Cannot find file dump list\n");
		done(1);
	}
	map = calloc((unsigned)1, (unsigned)howmany(maxino, NBBY));
	if (map == (char *)NULL)
		panic("no memory for file dump list\n");
	dumpmap = map;
	curfile.action = USING;
	getfile(xtrmap, xtrmapskip);
}

/*
 * Prompt user to load a new dump volume.
 * "Nextvol" is the next suggested volume to use.
 * This suggested volume is enforced when doing full
 * or incremental restores, but can be overrridden by
 * the user when only extracting a subset of the files.
 */
void
getvol(long nextvol)
{
	long newvol;
	long savecnt, i;
	union u_spcl tmpspcl;
#	define tmpbuf tmpspcl.s_spcl
	char buf[TP_BSIZE];

	if (nextvol == 1) {
		tapesread = 0;
		gettingfile = 0;
	}
	if (pipein) {
		if (nextvol != 1)
			panic("Changing volumes on pipe input?\n");
		if (volno == 1)
			return;
		goto gethdr;
	}
	savecnt = blksread;
again:
	if (pipein)
		done(1); /* pipes do not get a second chance */
	if (command == 'R' || command == 'r' || curfile.action != SKIP ||
	    (tapesread == 0 && dumpnum > 1))	/* 's' key given */
		newvol = nextvol;
	else 
		newvol = 0;
	while (newvol <= 0) {
		if (tapesread == 0) {
			fprintf(stderr, "%s%s%s%s%s",
			    "You have not read any tapes yet.\n",
			    "Unless you know which volume your",
			    " file(s) are on you should start\n",
			    "with the last volume and work",
			    " towards the first.\n");
		} else {
			fprintf(stderr, "You have read volumes");
			strcpy(tbf, ": ");
			for (i = 1; i < 32; i++)
				if (tapesread & (1 << i)) {
					fprintf(stderr, "%s%d", tbf, i);
					strcpy(tbf, ", ");
				}
			fprintf(stderr, "\n");
		}
		do	{
			fprintf(stderr, "Specify next volume #: ");
			(void) fflush(stderr);
			(void) fgets(tbf, BUFSIZ, terminal);
		} while (!feof(terminal) && tbf[0] == '\n');
		if (feof(terminal))
			done(1);
		newvol = atoi(tbf);
		if (newvol <= 0) {
			fprintf(stderr,
			    "Volume numbers are positive numerics\n");
		}
	}
	if (newvol == volno) {
		tapesread |= 1 << volno;
		return;
	}
	closemt();
	fprintf(stderr, "Mount tape volume %d\n", newvol);
	fprintf(stderr, "then enter tape name (default: %s) ", magtape);
	(void) fflush(stderr);
	(void) fgets(tbf, BUFSIZ, terminal);
	if (feof(terminal))
		done(1);
	if (tbf[0] != '\n') {
		(void) strcpy(magtape, tbf);
		magtape[strlen(magtape) - 1] = '\0';
	}
	if ((mt = (*Ropen)(magtape, 0)) == -1)
	{
		fprintf(stderr, "Cannot open %s\n", magtape);
		volno = -1;
		goto again;
	}
	chkandfixaudio(mt);
gethdr:
	volno = newvol;
	setdumpnum();
	flsht();
	if (readhdr(&tmpbuf) == FAIL) {
		fprintf(stderr, "tape is not dump tape\n");
		volno = 0;
		goto again;
	}
	if (checkvol(&tmpbuf, volno) == FAIL) {
		fprintf(stderr, "Wrong volume (%d)\n", tmpbuf.c_volume);
		volno = 0;
		goto again;
	}
	if (tmpbuf.c_date != dumpdate || tmpbuf.c_ddate != dumptime) {
		fprintf(stderr, "Wrong dump date\n\tgot: %s",
			ctime(&tmpbuf.c_date));
		fprintf(stderr, "\twanted: %s", ctime(&dumpdate));
		volno = 0;
		goto again;
	}
	tapesread |= 1 << volno;
	blksread = savecnt;
	if (curfile.action == USING) {
		if (volno == 1)
			panic("active file into volume 1\n");
		return;
	}
	/*
	 * Skip up to the beginning of the next record
	 */
	if (tmpbuf.c_type == TS_TAPE && (tmpbuf.c_flags & DR_NEWHEADER))
		for (i = tmpbuf.c_count; i > 0; i--)
			readtape(buf);
	(void) gethead(&spcl);
	findinode(&spcl);
	if (gettingfile) {
		gettingfile = 0;
		longjmp(restart, 1);
	}
}

/*
 * handle multiple dumps per tape by skipping forward to the
 * appropriate one.
 */
static void
setdumpnum(void)
{
	struct mtop tcom;

	if (dumpnum == 1 || volno != 1)
		return;
	if (pipein) {
		fprintf(stderr, "Cannot have multiple dumps on pipe input\n");
		done(1);
	}
	tcom.mt_op = MTFSF;
	tcom.mt_count = dumpnum - 1;
	if ((*Rioctl)(mt, (int)MTIOCTOP, (char *)&tcom) < 0)
		perror("ioctl MTFSF");
}

void
printdumpinfo(void)
{

	fprintf(stdout, "Dump   date: %s", ctime(&spcl.c_date));
	fprintf(stdout, "Dumped from: %s",
	  (spcl.c_ddate == 0) ? "the epoch\n" : ctime(&spcl.c_ddate));
	if (spcl.c_host[0] == '\0')
		return;
	fprintf(stderr, "Level %d dump of %s on %s:%s\n",
		spcl.c_level, spcl.c_filesys, spcl.c_host, spcl.c_dev);
	fprintf(stderr, "Label: %s\n", spcl.c_label);
}

int
extractfile(char *name, struct entry *nameep)
{
	int mode;
	time_t timep[2];
	struct entry *ep;

	if (newer && curfile.dip->di_ctime < newer) {
		Vprintf(stdout, "%s: too old\n", name);
		skipfile();
		return FAIL;
	}
	if (newest) {
		struct stat64 st;
		if (stat64(name, &st) == 0
	            && st.st_ctime >= curfile.dip->di_ctime) {
			Vprintf(stdout, "%s: existing version is newer\n",name);
			skipfile();
			return FAIL;
		}
		nameep->e_flags |= EXTRACTED;
	}
	if (justcreate) {
		if (access(name, F_OK) == 0) {
			Vprintf(stdout, "%s: exists, ignoring\n", name);
			skipfile();
			return FAIL;
		}
		nameep->e_flags |= EXTRACTED;
	}
	curfile.name = name;
	curfile.action = USING;
	timep[0] = curfile.dip->di_atime;
	timep[1] = curfile.dip->di_mtime;
	mode = curfile.dip->di_mode;
	switch (mode & IFMT) {

	default:
		fprintf(stderr, "%s: unknown file mode 0%o\n", name, mode);
		skipfile();
		return (FAIL);

#ifdef IFSOCK
	case IFSOCK:
		Vprintf(stdout, "skipped socket %s\n", name);
		skipfile();
		return (GOOD);
#endif

	case IFDIR:
		if (mflag) {
			ep = lookupname(name);
			if (ep == NIL || ep->e_flags & EXTRACT)
				panic("unextracted directory %s\n", name);
			skipfile();
			return (GOOD);
		}
		Vprintf(stdout, "extract file %s\n", name);
		return (genliteraldir(name, curfile.ino));

	case IFLNK:
		lnkbuf[0] = '\0';
		pathlen = 0;
		getfile(xtrlnkfile, xtrlnkskip);
		if (pathlen == 0) {
			Vprintf(stdout,
			    "%s: zero length symbolic link (ignored)\n", name);
			return (GOOD);
		}
		return linkit(lnkbuf, name, SYMLINK);

	case IFCHR:
	case IFBLK:
	case IFIFO:
		Vprintf(stdout, "extract %s %s\n",
			(mode&IFMT) == IFIFO ? "fifo" : "special file",
			name);
		if (Nflag) {
			skipfile();
			return (GOOD);
		}
		if (mknod(name, mode, conv_devnum((dev_t)curfile.dip->di_rdev)) < 0) {
			fprintf(stderr, "%s: ", name);
			(void) fflush(stderr);
			perror((mode&IFMT) == IFIFO ? "cannot create fifo" :
				"cannot create special file");
			skipfile();
			return (FAIL);
		}
		if (suser || oflag)
			(void) chown(name, curfile.dip->di_uid,
				curfile.dip->di_gid);
		(void) chmod(name, mode);
		skipfile();
		utime(name, (struct utimbuf *)timep);
		return (GOOD);

	case IFREG:
		if (justcreate) {
			Vprintf(stdout, "extract file %s (old ino %lu)\n",
				name, nameep->e_ino);
		} else {
			Vprintf(stdout, "extract file %s\n", name);
		}
		if (Nflag) {
			skipfile();
			return (GOOD);
		}
		if ((ofile = creat(name, 0666)) < 0) {
			fprintf(stderr, "%s: ", name);
			(void) fflush(stderr);
			perror("cannot create file");
			if ((errno == ENOSPC) || (errno == EDQUOT)) {
				static int first = 1;
				extern char *sys_errlist[];
				char buffer[100];

				sprintf(buffer,"%s. Continue?", sys_errlist[errno]);
				if (first) {
					if (reply(buffer) == FAIL)
						done(1);
					first = 0;
				}
			}
			skipfile();
			return (FAIL);
		}
		if (suser || oflag)
			(void) fchown(ofile, curfile.dip->di_uid,
				curfile.dip->di_gid);
		(void) fchmod(ofile, mode);
		getfile(xtrfile, xtrskip);
		(void) close(ofile);
		utime(name, (struct utimbuf *)timep);
		return (GOOD);
	}
	/* NOTREACHED */
}

/*
 * skip over bit maps on the tape
 */
void
skipmaps(void)
{

	while (checktype(&spcl, TS_CLRI) == GOOD ||
	       checktype(&spcl, TS_BITS) == GOOD)
		skipfile();
}

/*
 * skip over a file on the tape
 */
void
skipfile(void)
{
	curfile.action = SKIP;
	getfile(null, null);
}

/*
 * Do the file extraction, calling the supplied functions
 * with the blocks
 */
void
getfile(void (*f1)(char *, long), void (*f2)(char *, long))
{
	int i;
	int curblk = 0;
	off_t size = spcl.c_dinode.di_size;
	static char clearedbuf[MAXBSIZE];
	char buf[MAXBSIZE / TP_BSIZE][TP_BSIZE];

	if (checktype(&spcl, TS_END) == GOOD)
		panic("ran off end of tape\n");
	if (ishead(&spcl) == FAIL)
		panic("not at beginning of a file\n");
#ifdef sun
	/*
	 * This function is in the main loop.
	 * We don't need to save the signal mask, so use _setjmp
	 * and save a couple of system calls.
	 */
	if (!gettingfile && _setjmp(restart) != 0)
#else
	if (!gettingfile && setjmp(restart) != 0)
#endif
		return;
	gettingfile++;
loop:
	for (i = 0; i < spcl.c_count; i++) {
		/* ASSERT(curblk < MAXBSIZE/TP_BSIZE); */
		if (spcl.c_addr[i]) {
			readtape(&buf[curblk++][0]);
			if (curblk == fssize / TP_BSIZE) {
				(*f1)(buf[0], size > TP_BSIZE ?
				     fssize :
				     (long)((curblk - 1) * TP_BSIZE + size));
				curblk = 0;
			}
		} else {
			if (curblk > 0) {
				(*f1)(buf[0], size > TP_BSIZE ?
				     curblk * TP_BSIZE :
				     (long)((curblk - 1) * TP_BSIZE + size));
				curblk = 0;
			}
			(*f2)(clearedbuf, size > TP_BSIZE ?
				(long)TP_BSIZE : (long)size);
		}
		if ((size -= TP_BSIZE) <= 0) {
			for (i++; i < spcl.c_count; i++)
				if (spcl.c_addr[i])
					readtape((char *)0);
			break;
		}
	}
	if (readhdr(&spcl) == GOOD && size > 0) {
		if (checktype(&spcl, TS_ADDR) == GOOD)
			goto loop;
		Dprintf(stdout, "Missing address (header) block for %s\n",
			curfile.name);
	}
	if (curblk > 0)
		(*f1)(buf[0], (long)((curblk * TP_BSIZE) + size));
	findinode(&spcl);
	gettingfile = 0;
}

/*
 * The next routines are called during file extraction to
 * put the data into the right form and place.
 */
static void
xtrfile(char *buf, long size)
{

	if (Nflag)
		return;
	if (write(ofile, buf, (int) size) != (int)size) {
		fprintf(stderr, "write error extracting inode %d, name %s\n",
			curfile.ino, curfile.name);
		perror("write");
		done(1);
	}
}

/* ARGSUSED */
static void
xtrskip(char *buf, long size)
{
	if (lseek(ofile, size, 1) == (long)-1) {
		fprintf(stderr, "seek error extracting inode %d, name %s\n",
			curfile.ino, curfile.name);
		perror("lseek");
		done(1);
	}
}

static void
xtrlnkfile(char *buf, long size)
{
	pathlen += size;
	if (pathlen > MAXPATHLEN) {
		fprintf(stderr, "symbolic link name: %s->%s%s; too long %d\n",
		    curfile.name, lnkbuf, buf, pathlen);
		done(1);
	}
	buf[size] = '\0';
	(void) strcat(lnkbuf, buf);
}

/* ARGSUSED */
static void
xtrlnkskip(char *buf, long size)
{
	fprintf(stderr, "unallocated block in symbolic link %s\n",
		curfile.name);
	done(1);
}

static void
xtrmap(char *buf, long size)
{
	bcopy(buf, map, size);
	map += size;
}

/* ARGSUSED */
static void
xtrmapskip(char *buf, long size)
{
	panic("hole in map\n");
	map += size;
}

/* ARGSUSED */
void
null(char *buf, long size)
{
}

/*
 * Do the tape i/o, dealing with volume changes
 * etc..
 */
static void
readtape(char *b)
{
	long i;
	long rd, newvol;
	int cnt;

	if (bct < ntrec) {
		if (b)
			bcopy(&tbf[(bct*TP_BSIZE)], b, (long)TP_BSIZE);
		bct++;
		blksread++;
		return;
	}
	for (i = 0; i < ntrec; i++)
		((struct s_spcl *)&tbf[i*TP_BSIZE])->c_magic = 0;
	bct = 0;
	cnt = ntrec*TP_BSIZE;
	rd = 0;
getmore:
	i = (*Rread)(mt, &tbf[rd], cnt);
	if (i > 0 && i != ntrec*TP_BSIZE) {
		if (pipein) {
			rd += i;
			cnt -= i;
			if (cnt > 0)
				goto getmore;
			i = rd;
		} else {
			if (i % TP_BSIZE != 0)
				panic("partial block read: %d should be %d\n",
					i, ntrec * TP_BSIZE);
			bcopy((char *)&endoftapemark, &tbf[i],
				(long)TP_BSIZE);
		}
	}
	if (i < 0) {
		fprintf(stderr, "Tape read error while ");
		switch (curfile.action) {
		default:
			fprintf(stderr, "trying to set up tape\n");
			break;
		case UNKNOWN:
			fprintf(stderr, "trying to resyncronize\n");
			break;
		case USING:
			fprintf(stderr, "restoring %s\n", curfile.name);
			break;
		case SKIP:
			fprintf(stderr, "skipping over inode %d\n",
				curfile.ino);
			break;
		}
		if (!yflag && !reply("continue"))
			done(1);
		i = ntrec*TP_BSIZE;
		bzero(tbf, i);
		if ((*Rseek)(mt, i, 1) == (long)-1)
		{
			perror("continuation failed");
			done(1);
		}
	}
	if (i == 0) {
		if (!pipein) {
			newvol = volno + 1;
			volno = 0;
			getvol(newvol);
			readtape(b);
			return;
		}
		if (rd % TP_BSIZE != 0)
			panic("partial block read: %d should be %d\n",
				rd, ntrec * TP_BSIZE);
		bcopy((char *)&endoftapemark, &tbf[rd], (long)TP_BSIZE);
	}
	readtape(b);
}

static void
findtapeblksize(void)
{
	long i;

	for (i = 0; i < ntrec; i++)
		((struct s_spcl *)&tbf[i * TP_BSIZE])->c_magic = 0;
	bct = 0;
	i = (*Rread)(mt, tbf, ntrec * TP_BSIZE);
	if (i <= 0) {
		perror("Tape read error");
		done(1);
	}
	if (i % TP_BSIZE != 0) {
		fprintf(stderr, "Tape block size (%d) %s (%d)\n",
			i, "is not a multiple of dump block size", TP_BSIZE);
		done(1);
	}
	ntrec = i / TP_BSIZE;
	Vprintf(stdout, "Tape block size is %d\n", ntrec);
}

static void
flsht(void)
{
	bct = ntrec+1;
}

void
closemt(void)
{
	if (mt < 0)
		return;
	(void) (*Rclose)(mt);
}

static int
checkvol(struct s_spcl *b, long t)
{
	if (b->c_volume != t)
		return(FAIL);
	return(GOOD);
}

static int
readhdr(struct s_spcl *b)
{

	if (gethead(b) == FAIL) {
		Dprintf(stdout, "readhdr fails at %d blocks\n", blksread);
		return(FAIL);
	}
	return(GOOD);
}

/*
 * read the tape into buf, then return whether or
 * or not it is a header block.
 */
static int
gethead(struct s_spcl *buf)
{
	long i, *j;

	if (!cvtflag) {
		readtape((char *)buf);
		if (buf->c_magic != NFS_MAGIC) {
			if (swabl(buf->c_magic) != NFS_MAGIC)
				return (FAIL);
			if (!Bcvt) {
				Vprintf(stdout, "Note: Doing Byte swapping\n");
				Bcvt = 1;
			}
		}
		if (checksum((int *)buf) == FAIL)
			return (FAIL);
		if (Bcvt)
			swabst("8l4s31l", (char *)buf);
		goto good;
	}
good:
	j = buf->c_dinode.di_ic.ic_size.val;
	i = j[1];
	if (buf->c_dinode.di_size == 0 &&
	    (buf->c_dinode.di_mode & IFMT) == IFDIR && Qcvt==0) {
		if (*j || i) {
			printf("Note: Doing Quad swapping\n");
			Qcvt = 1;
		}
	}
	if (Qcvt) {
		j[1] = *j; *j = i;
	}

	switch (buf->c_type) {

	case TS_CLRI:
	case TS_BITS:
		/*
		 * Have to patch up missing information in bit map headers
		 */
		buf->c_inumber = 0;
		buf->c_dinode.di_size = buf->c_count * TP_BSIZE;
		for (i = 0; i < buf->c_count; i++)
			buf->c_addr[i]++;
		break;

	case TS_END:
		nvol = volno;
	case TS_TAPE:
		buf->c_inumber = 0;
		break;

	case TS_INODE:
	case TS_ADDR:
		break;

	default:
		panic("gethead: unknown inode type %d\n", buf->c_type);
		break;
	}
	if (dflag)
		accthdr(buf);
	return(GOOD);
}

/*
 * Check that a header is where it belongs and predict the next header
 */
static void
accthdr(struct s_spcl *header)
{
	static efs_ino_t previno = 0x7fffffff;
	static int prevtype;
	static long predict;
	long blks, i;

	if (header->c_type == TS_TAPE) {
		fprintf(stderr, "Volume header\n");
		previno = 0x7fffffff;
		return;
	}
	if (previno == 0x7fffffff)
		goto newcalc;
	switch (prevtype) {
	case TS_BITS:
		fprintf(stderr, "Dump mask header");
		break;
	case TS_CLRI:
		fprintf(stderr, "Remove mask header");
		break;
	case TS_INODE:
		fprintf(stderr, "File header, ino %d", previno);
		break;
	case TS_ADDR:
		fprintf(stderr, "File continuation header, ino %d", previno);
		break;
	case TS_END:
		fprintf(stderr, "End of tape header");
		break;
	}
	if (predict != blksread - 1)
		fprintf(stderr, "; predicted %d blocks, got %d blocks",
			predict, blksread - 1);
	fprintf(stderr, "\n");
newcalc:
	blks = 0;
	if (header->c_type != TS_END)
		for (i = 0; i < header->c_count; i++)
			if (header->c_addr[i] != 0)
				blks++;
	predict = blks;
	blksread = 0;
	prevtype = header->c_type;
	previno = header->c_inumber;
}

/*
 * Find an inode header.
 * Complain if had to skip, and complain is set.
 */
static void
findinode(struct s_spcl *header)
{
	static long skipcnt = 0;
	long i;
	char buf[TP_BSIZE];

	curfile.name = "<name unknown>";
	curfile.action = UNKNOWN;
	curfile.dip = 0;
	curfile.ino = 0;
	if (ishead(header) == FAIL) {
		skipcnt++;
		while (gethead(header) == FAIL || header->c_date != dumpdate)
			skipcnt++;
	}
	for (;;) {
		if (checktype(header, TS_ADDR) == GOOD) {
			/*
			 * Skip up to the beginning of the next record
			 */
			for (i = 0; i < header->c_count; i++)
				if (header->c_addr[i])
					readtape(buf);
			(void) gethead(header);
			continue;
		}
		if (checktype(header, TS_INODE) == GOOD) {
			curfile.dip = &header->c_dinode;
			curfile.ino = header->c_inumber;
			break;
		}
		if (checktype(header, TS_END) == GOOD) {
			curfile.ino = maxino;
			break;
		}
		if (checktype(header, TS_CLRI) == GOOD) {
			curfile.name = "<file removal list>";
			break;
		}
		if (checktype(header, TS_BITS) == GOOD) {
			curfile.name = "<file dump list>";
			break;
		}
		while (gethead(header) == FAIL)
			skipcnt++;
	}
	if (skipcnt > 0)
		fprintf(stderr, "resync restore, skipped %d blocks\n", skipcnt);
	skipcnt = 0;
}

/*
 * return whether or not the buffer contains a header block
 */
static int
ishead(struct s_spcl *buf)
{

	if (buf->c_magic != NFS_MAGIC)
		return(FAIL);
	return(GOOD);
}

static int
checktype(struct s_spcl *b, int t)
{
	if (b->c_type != t)
		return(FAIL);
	return(GOOD);
}

static int
checksum(int *b)
{
	int i, j;

	j = sizeof(union u_spcl) / sizeof(int);
	i = 0;
	if(!Bcvt) {
		do
			i += *b++;
		while (--j);
	} else {
		/* What happens if we want to read restore tapes
			for a 16bit int machine??? */
		do 
			i += swabl(*b++);
		while (--j);
	}
			
	if (i != CHECKSUM) {
		fprintf(stderr, "Checksum error %o, inode %d file %s\n", i,
			curfile.ino, curfile.name);
		return(FAIL);
	}
	return(GOOD);
}

static void
swabst(char *cp, char *sp)
{
	int n = 0;
	char c;
	while(*cp) {
		switch (*cp) {
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			n = (n * 10) + (*cp++ - '0');
			continue;
		
		case 's': case 'w': case 'h':
			c = sp[0]; sp[0] = sp[1]; sp[1] = c;
			sp++;
			break;

		case 'l':
			c = sp[0]; sp[0] = sp[3]; sp[3] = c;
			c = sp[2]; sp[2] = sp[1]; sp[1] = c;
			sp += 3;
		}
		sp++; /* Any other character, like 'b' counts as byte. */
		if (n <= 1) {
			n = 0; cp++;
		} else
			n--;
	}
}

static long
swabl(int x)
{
	unsigned long l = x;
	swabst("l", (char *)&l);
	return (long)l;
}

int
all_tapes_read(void)
{

	if (nvol <= 0 || tapesread == 0)
		return(0);
	return tapesread == (((1<<nvol)-1) << 1);
}

/* dumprmt.c wants this */
void
msg(char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

/*
 * Cover functions for the stuff in dumprmt.c,
 * which has a different calling sequence.
 */
/* ARGSUSED */
static ssize_t
myrmtread(int fd, void *buf, size_t n)
{
	return rmtread(buf, n);
}

/* ARGSUSED */
static off_t
myrmtseek(int fd, off_t off, int pos)
{
	return rmtseek(off, pos);
}

/* ARGSUSED */
static int
myrmtioctl(int fd, int cmd, ...)
{
	va_list ap;
	void *arg;
	int rval;

	va_start(ap, cmd);
	arg = va_arg(ap, void *);
	rval = rmtioctl(cmd, arg);
	va_end(ap);
	return rval;
}

/*
 * Since this only runs on 5.0 or greater systems, convert
 * the all style device numbers to the new 32 bit device numbers
 */
static dev_t
conv_devnum(dev_t dn)
{
	major_t maj;
	minor_t min;

	if (dn >> NBITSMINOR) /* new style device number */
		return(dn);
	else {
		/* convert old style to new style */
		maj = ((dn >> ONBITSMINOR) << NBITSMINOR);
		min = (dn & OMAXMIN);
		return((dev_t)maj|min);
	}
}

/*
 * check to see if it's a drive, and is in audio mode; if it is, then
 * try to fix it, and also notify them.  Otherwise we can write in
 * audio mode, but they won't be able to get the data back
 */
static void
chkandfixaudio(int mt)
{
	static struct mtop mtop = {MTAUD, 0};
	struct mtget mtget;
	int err;
	unsigned status;

	err = Rioctl(mt, MTIOCGET, &mtget);
	if(err)
		return;
	status = mtget.mt_dsreg | ((unsigned)mtget.mt_erreg<<16);
	if(status & CT_AUDIO) {
		fprintf(stderr, "dump: Warning: drive was in audio mode, turning audio mode off\n");
		err = Rioctl(mt, MTIOCTOP, &mtop);
		if(err)
			fprintf(stderr, "dump: Warning: unable to disable audio mode.  Tape may not be readable\n");
	}
}
