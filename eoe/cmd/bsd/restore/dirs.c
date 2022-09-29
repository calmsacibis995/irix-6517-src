#ident "$Revision: 1.9 $"

/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 * static char sccsid[] = "@(#)dirs.c	5.4 (Berkeley) 4/23/86";
 */

#include "restore.h"

/*
 * Symbol table of directories read from tape.
 */
#define HASHSIZE	997	/* a prime */
#define INOHASH(val) (val % HASHSIZE)
struct inotab {
	struct inotab	*t_next;
	efs_ino_t	t_ino;
	baddr_t		t_seekpt;
	long		t_size;
};
static struct inotab *inotab[HASHSIZE];

/*
 * Information retained about directories.
 */
struct modeinfo {
	efs_ino_t	ino;
	time_t		timep[2];
	short		mode;
	short		uid;
	short		gid;
};

/*
 * Global variables for this file.
 */
static baddr_t	seekpt;
static FILE	*df, *mf;
static RST_DIR	*dirp;
static char	dirfile[MAXPATHLEN + 1] = "#";	/* No file */
static char	modefile[MAXPATHLEN + 1] = "#";	/* No file */

/*
 * Format of old style directories.
 */
#define ODIRSIZ 14
struct odirect {
	u_short	d_ino;
	char	d_name[ODIRSIZ];
};

static struct inotab *allocinotab(efs_ino_t, struct sun_dinode *, baddr_t);
static void flushent(void);
static struct inotab *inotablookup(efs_ino_t);
#if 0
static void pmode(int);
#endif
static void putdir(char *, long);
static void putent(struct direct *);
static RST_DIR *rst_4_3_opendir(char *);
static long rst_4_3_telldir(RST_DIR *);
static void rst_seekdir(RST_DIR *, baddr_t, baddr_t);
static efs_ino_t search(efs_ino_t, char *);
static void sun_putdir(char *, int);

/*
 *	Extract directory contents, building up a directory structure
 *	on disk for extraction by name.
 *	If genmode is requested, save mode, owner, and times for all
 *	directories on the tape.
 */
void
extractdirs(int genmode)
{
	struct sun_dinode *ip;
	struct inotab *itp;
	struct direct nulldir;
	char *tmpdir = getenv("TMPDIR");

	if (tmpdir == NULL)
		tmpdir = "/tmp";
	Vprintf(stdout, "Extract directories from tape\n");
	(void) sprintf(dirfile, "%s/rstdir%d", tmpdir, dumpdate);
	df = fopen(dirfile, "w");
	if (df == 0) {
		perror(dirfile);
		fprintf(stderr,
		    "restore: %s - cannot create directory temporary\n",
		    dirfile);
		done(1);
	}
	if (genmode != 0) {
		(void) sprintf(modefile, "%s/rstmode%d", tmpdir,  dumpdate);
		mf = fopen(modefile, "w");
		if (mf == 0) {
			perror(modefile);
			fprintf(stderr,
			    "restore: %s - cannot create modefile \n",
			    modefile);
			done(1);
		}
	}
	nulldir.d_ino = 0;
	nulldir.d_namlen = 1;
	(void) strcpy(nulldir.d_name, "/");
	nulldir.d_reclen = DIRSIZ(&nulldir);
	while (curfile.ino < maxino) {
		curfile.name = "<directory file - name unknown>";
		curfile.action = USING;
		ip = curfile.dip;
		if (ip == NULL) {
			fprintf(stderr, "Skipping null inode %d\n", curfile.ino);
			goto dirzone_done;
		}
#if 0
		printf("ino %d ", curfile.ino);
		pmode(ip->di_mode);
#endif
		/*
		 *In general,1st non-dir => endOfDirZone
		 * but due to bugs in dump during active dumps:
		 */
		switch (ip->di_mode & IFMT) {
		case IFLNK:
		case IFCHR:
		case IFBLK:
		case IFREG:
		case IFIFO:
			goto dirzone_done;
		case IFDIR:
			itp = allocinotab(curfile.ino, ip, seekpt);
			getfile(putdir, null);
			putent(&nulldir);
			flushent();
			itp->t_size = seekpt - itp->t_seekpt;
			continue;
		default:
			fprintf(stderr, "Skipping unknown file (mode:%d,inode:%d)\n",
				ip->di_mode, curfile.ino );
		case 0 : /* empty file */
			skipfile();
			continue;
		}
	}
     dirzone_done:
	(void) fclose(df);
	dirp = rst_4_3_opendir(dirfile);
	if (dirp == NULL) {
		perror("opendir");
		done(1);
	}
	if (mf != NULL)
		(void) fclose(mf);
	if (dirlookup(".") == 0) {
		panic("Root directory is not on tape\n");
		done(1);
	}
}

/*
 * skip over all the directories on the tape
 */
void
skipdirs(void)
{

	while ((curfile.dip->di_mode & IFMT) == IFDIR) {
		skipfile();
	}
}

/*
 *	Recursively find names and inumbers of all files in subtree 
 *	pname and pass them off to be processed.
 */
void
treescan(char *pname, efs_ino_t ino, long (*todo)(char *, efs_ino_t, int))
{
	struct inotab *itp;
	struct direct *dp;
	int namelen;
	baddr_t bpt;
	char locname[MAXPATHLEN + 1];

	itp = inotablookup(ino);
	if (itp == NULL) {
		/*
		 * Pname is name of a simple file or an unchanged directory.
		 */
		(void) (*todo)(pname, ino, LEAF);
		return;
	}
	/*
	 * Pname is a dumped directory name.
	 */
	if ((*todo)(pname, ino, NODE) == FAIL)
		return;
	(void) strncpy(locname, pname, MAXPATHLEN);
	(void) strncat(locname, "/", MAXPATHLEN);
	namelen = strlen(locname);
	rst_seekdir(dirp, itp->t_seekpt, itp->t_seekpt);
	/*
	 * a zero inode signals end of directory
	 */
	while ((dp = rst_readdir(dirp)) != NULL) {
		if (dp->d_ino == 0)
			return;
		if (dp->d_name[0] == '.')
			if (dp->d_namlen == 1 ||
			    (dp->d_name[1] == '.' && dp->d_namlen == 2))
				continue;
		locname[namelen] = '\0';
		if (namelen + dp->d_namlen >= MAXPATHLEN) {
			fprintf(stderr, "%s%s: name exceeds %d char\n",
				locname, dp->d_name, MAXPATHLEN);
		} else {
			(void) strncat(locname, dp->d_name, (int)dp->d_namlen);
			bpt = rst_4_3_telldir(dirp);
			treescan(locname, dp->d_ino, todo);
			rst_seekdir(dirp, bpt, itp->t_seekpt);
		}
	}
	/* should have seen the zero inode entry */
	fprintf(stderr, "corrupted directory: %s.\n", locname);
}

/*
 * Search the directory tree rooted at inode ROOTINO
 * for the path pointed at by n
 */
efs_ino_t
psearch(char *n)
{
	char *cp, *cp1;
	efs_ino_t ino;
	char c;

	ino = ROOTINO;
	if (*(cp = n) == '/')
		cp++;
next:
	cp1 = cp + 1;
	while (*cp1 != '/' && *cp1)
		cp1++;
	c = *cp1;
	*cp1 = 0;
	ino = search(ino, cp);
	if (ino == 0) {
		*cp1 = c;
		return(0);
	}
	*cp1 = c;
	if (c == '/') {
		cp = cp1+1;
		goto next;
	}
	return(ino);
}

/*
 * search the directory inode ino
 * looking for entry cp
 */
static efs_ino_t
search(efs_ino_t inum, char *cp)
{
	struct direct *dp;
	struct inotab *itp;
	int len;

	itp = inotablookup(inum);
	if (itp == NULL)
		return(0);
	rst_seekdir(dirp, itp->t_seekpt, itp->t_seekpt);
	len = strlen(cp);
	do {
		dp = rst_readdir(dirp);
		if (dp == NULL || dp->d_ino == 0)
			return (0);
	} while (dp->d_namlen != len || strncmp(dp->d_name, cp, len) != 0);
	return(dp->d_ino);
}

/*
 * Put the directory entries in the directory file
 */
static void
putdir(char *buf, long size)
{
	struct direct cvtbuf, *dp = &cvtbuf;
	struct efs_dirblk *db;
	struct efs_dent *dep;
	int i, s;

	if (!efsdirs) {
		sun_putdir(buf, size);
		return;
	}
	if (size & EFS_DIRBMASK)
		panic("Odd sized dir block"), exit(1);
	for (i = 0; i < size; i += EFS_DIRBSIZE) {
		db = (struct efs_dirblk *) (buf+i);
		if (db->magic != EFS_DIRBLK_MAGIC) {
			fprintf(stderr, "bad magic # %d in dir blk %d of ino %d slots %d\n", db->magic, i/EFS_DIRBSIZE, curfile.ino, db->slots);
			continue;
		}
		for (s = 0; s < db->slots; s++) {
			if (EFS_SLOTAT(db, s) == 0)
				continue;
			dep = EFS_SLOTOFF_TO_DEP(db, EFS_SLOTAT(db,s));
			dp->d_ino = EFS_GET_INUM(dep);
			if (dp->d_ino == 0)
				continue;
			dp->d_namlen = dep->d_namelen;
			bcopy(dep->d_name, dp->d_name, dp->d_namlen);
			dp->d_name[dp->d_namlen] = '\0';
			dp->d_reclen = DIRSIZ(dp);
			putent(dp);
		}
	}
}

/*
 * These variables are "local" to the following two functions.
 */
static char dirbuf[BBSIZE];
static long dirloc = 0;
static long prev = 0;

/*
 * add a new directory entry to a file.
 */
static void
putent(struct direct *dp)
{
	dp->d_reclen = DIRSIZ(dp);
	if (dirloc + dp->d_reclen > BBSIZE) {
		((struct direct *)(dirbuf + prev))->d_reclen =
		    (u_short)(BBSIZE - prev);
		if (fwrite(dirbuf, 1, BBSIZE, df) != BBSIZE) {
			fprintf(stderr, "Error while writing to file %s", dirfile);
			done(1);
		}
		dirloc = 0;
	}
	bcopy((char *)dp, dirbuf + dirloc, (long)dp->d_reclen);
	prev = dirloc;
	dirloc += dp->d_reclen;
}

/*
 * flush out a directory that is finished.
 */
static void
flushent(void)
{
	((struct direct *)(dirbuf + prev))->d_reclen = (u_short)(BBSIZE - prev);
	if (fwrite(dirbuf, (int)dirloc, 1, df) != 1) {
		fprintf(stderr, "Error while writing to file %s", dirfile);
		done(1);
	}
	seekpt = ftell(df);
	dirloc = 0;
}

/*
 * Seek to an entry in a directory.
 * Only values returned by ``rst_4_3_telldir'' should be passed to rst_seekdir.
 * This routine handles many directories in a single file.
 * It takes the base of the directory in the file, plus
 * the desired seek offset into it.
 */
static void
rst_seekdir(RST_DIR *dirp, baddr_t loc, baddr_t base)
{
	if (loc == rst_4_3_telldir(dirp))
		return;
	loc -= base;
	if (loc < 0)
		fprintf(stderr, "bad seek pointer to rst_seekdir %d\n", loc);
	(void) lseek(dirp->dd_fd, base + (loc & ~(BBSIZE - 1)), 0);
	dirp->dd_loc = loc & (BBSIZE - 1);
	if (dirp->dd_loc != 0)
		dirp->dd_size = read(dirp->dd_fd, dirp->dd_buf, BBSIZE);
}

/*
 * get next entry in a directory.
 */
struct direct *
rst_readdir(RST_DIR *dirp)
{
	struct direct *dp;

	for (;;) {
		if (dirp->dd_loc == 0) {
			dirp->dd_size = read(dirp->dd_fd, dirp->dd_buf, 
			    BBSIZE);
			if (dirp->dd_size <= 0) {
				Dprintf(stderr, "error reading directory\n");
				return NULL;
			}
		}
		if (dirp->dd_loc >= dirp->dd_size) {
			dirp->dd_loc = 0;
			continue;
		}
		dp = (struct direct *)(dirp->dd_buf + dirp->dd_loc);
		if (dp->d_reclen == 0 ||
		    dp->d_reclen > BBSIZE + 1 - dirp->dd_loc) {
			Dprintf(stderr, "corrupted directory: bad reclen %d\n",
				dp->d_reclen);
			return NULL;
		}
		dirp->dd_loc += dp->d_reclen;
		if (dp->d_ino == 0 && strcmp(dp->d_name, "/") != 0)
			continue;
		if (dp->d_ino >= maxino) {
			Dprintf(stderr, "corrupted directory: bad inum %d\n",
				dp->d_ino);
			continue;
		}
		return (dp);
	}
}

/*
 * Simulate the opening of a directory
 */
RST_DIR *
rst_opendir(char *name)
{
	struct inotab *itp;
	efs_ino_t ino;

	if ((ino = dirlookup(name)) > 0 &&
	    (itp = inotablookup(ino)) != NULL) {
		rst_seekdir(dirp, itp->t_seekpt, itp->t_seekpt);
		return (dirp);
	}
	return (0);
}

/*
 * Set the mode, owner, and times for all new or changed directories
 */
void
setdirmodes(void)
{
	FILE *mf;
	struct modeinfo node;
	struct entry *ep;
	char *cp;
	char *tmpdir = getenv("TMPDIR");
	
	if (tmpdir == NULL)
		tmpdir = "/tmp";
	Vprintf(stdout, "Set directory mode, owner, and times.\n");
	(void) sprintf(modefile, "%s/rstmode%d", tmpdir, dumpdate);
	mf = fopen(modefile, "r");
	if (mf == NULL) {
		perror(modefile);
		fprintf(stderr, "cannot open mode file %s\n", modefile);
		fprintf(stderr, "directory mode, owner, and times not set\n");
		return;
	}
	clearerr(mf);
	for (;;) {
		(void) fread((char *)&node, 1, sizeof(struct modeinfo), mf);
		if (feof(mf))
			break;
		ep = lookupino(node.ino);
		if (command == 'i' || command == 'x') {
			if (ep == NIL)
				continue;
			if (ep->e_flags & EXISTED) {
				ep->e_flags &= ~NEW;
				continue;
			}
			if (node.ino == ROOTINO &&
		   	    reply("set owner/mode for '.'") == FAIL)
				continue;
		}
		if (ep == NIL) {
			panic("cannot find directory inode %d\n", node.ino);
			continue;
		}
		/*
		 * Under e or E, don't touch things we didn't create.
		 */
		if ((justcreate||newest) && (ep->e_flags & EXTRACTED) == 0) {
			ep->e_flags &= ~NEW;
			continue;
		}
		cp = myname(ep);
		if (!Nflag) {
			if ((suser) || (oflag))
				(void) chown(cp, node.uid, node.gid);
			(void) chmod(cp, node.mode);
			utime(cp, (struct utimbuf *)node.timep);
		}
		ep->e_flags &= ~NEW;
	}
	if (ferror(mf))
		panic("error setting directory modes\n");
	(void) fclose(mf);
}

/*
 * Generate a literal copy of a directory.
 */
int
genliteraldir(char *name, efs_ino_t ino)
{
	struct inotab *itp;
	int ofile, dp, i, size;
	char buf[BUFSIZ];

	itp = inotablookup(ino);
	if (itp == NULL)
		panic("Cannot find directory inode %d named %s\n", ino, name);
	if ((ofile = creat(name, 0666)) < 0) {
		fprintf(stderr, "%s: ", name);
		(void) fflush(stderr);
		perror("cannot create file");
		return (FAIL);
	}
	rst_seekdir(dirp, itp->t_seekpt, itp->t_seekpt);
	dp = dup(dirp->dd_fd);
	for (i = itp->t_size; i > 0; i -= BUFSIZ) {
		size = i < BUFSIZ ? i : BUFSIZ;
		if (read(dp, buf, (int) size) == -1) {
			fprintf(stderr,
				"write error extracting inode %d, name %s\n",
				curfile.ino, curfile.name);
			perror("read");
			done(1);
		}
		if (!Nflag && write(ofile, buf, (int) size) == -1) {
			fprintf(stderr,
				"write error extracting inode %d, name %s\n",
				curfile.ino, curfile.name);
			perror("write");
			done(1);
		}
	}
	(void) close(dp);
	(void) close(ofile);
	return (GOOD);
}

/*
 * Determine the type of an inode
 */
int
inodetype(efs_ino_t ino)
{
	struct inotab *itp;

	itp = inotablookup(ino);
	if (itp == NULL)
		return (LEAF);
	return (NODE);
}

/*
 * Allocate and initialize a directory inode entry.
 * If requested, save its pertinent mode, owner, and time info.
 */
static struct inotab *
allocinotab(efs_ino_t ino, struct sun_dinode *dip, baddr_t seekpt)
{
	struct inotab	*itp;
	struct modeinfo node;

	itp = (struct inotab *)calloc(1, sizeof(struct inotab));
	if (itp == 0)
		panic("no memory directory table\n");
	itp->t_next = inotab[INOHASH(ino)];
	inotab[INOHASH(ino)] = itp;
	itp->t_ino = ino;
	itp->t_seekpt = seekpt;
	if (mf == NULL)
		return(itp);
	node.ino = ino;
	node.timep[0] = dip->di_atime;
	node.timep[1] = dip->di_mtime;
	node.mode = dip->di_mode;
	node.uid = dip->di_uid;
	node.gid = dip->di_gid;
	if (fwrite((char *)&node, 1, sizeof(struct modeinfo), mf) !=
			sizeof(struct modeinfo)) {
		fprintf(stderr, "Error while writing to file %s", modefile);
		done(1);
	}
	return(itp);
}

/*
 * Look up an inode in the table of directories
 */
static struct inotab *
inotablookup(efs_ino_t ino)
{
	struct inotab *itp;

	for (itp = inotab[INOHASH(ino)]; itp != NULL; itp = itp->t_next)
		if (itp->t_ino == ino)
			return(itp);
	return ((struct inotab *)0);
}

/*
 * Clean up and exit
 */
void
done(int exitcode)
{

	closemt();
	if (modefile[0] != '#')
		(void) unlink(modefile);
	if (dirfile[0] != '#')
		(void) unlink(dirfile);
	exit(exitcode);
	/* NOTREACHED */
}

#if 0
static void
pmode(int m)
{
	char *s;

	switch (m & IFMT) {
	case IFLNK: s = "link"; break;
	case IFCHR: s = "chr";break;
	case IFBLK: s = "blk"; break;
	case IFREG: s = "reg";break;
	case IFIFO: s = "fifo";break;
	case IFDIR: s = "dir"; break;
	default: s = "?";
	}
	printf("type = %s\n", s);
}
#endif

/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

/*
 * open a directory.
 */
static RST_DIR *
rst_4_3_opendir(char *name)
{
	RST_DIR *dirp;
	int fd;

	if ((fd = open(name, 0)) == -1)
		return NULL;
	if ((dirp = (RST_DIR *)malloc(sizeof(RST_DIR))) == NULL) {
		close (fd);
		return NULL;
	}
	bzero(dirp, sizeof(RST_DIR));
	dirp->dd_fd = fd;
	dirp->dd_loc = 0;
	return dirp;
}

/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

/*
 * return a pointer into a directory
 */
static long
rst_4_3_telldir(RST_DIR *dirp)
{
	return ((long)lseek(dirp->dd_fd, 0L, 1) - dirp->dd_size + dirp->dd_loc);
}

/*
 * Convert Sun format directories to native format.
 */
struct sun_direct {	/* from sun <ufs/fsdir.h> */
        u_long  d_ino;                  /* inode number of entry */
        u_short d_reclen;               /* length of this record */
        u_short d_namlen;               /* length of string in d_name */
        char    d_name[MAXNAMLEN + 1];  /* name must be no longer than this */
};

static void
sun_putdir(char *buf, int size)
{
	struct sun_direct *sp;
	struct direct cvtbuf, *dp = &cvtbuf;
	long loc, i;

	for (loc = 0; loc < size; ) {
		sp = (struct sun_direct *)(buf + loc);
		i = BBSIZE - (loc & (BBSIZE - 1));
		if (sp->d_reclen == 0 || sp->d_reclen > i) {
			loc += i;
			continue;
		}
		loc += sp->d_reclen;
		if (sp->d_ino == 0)
			continue;
		dp->d_ino = sp->d_ino;
		dp->d_namlen = sp->d_namlen;
		bcopy(sp->d_name, dp->d_name, dp->d_namlen);
		dp->d_name[dp->d_namlen] = '\0';
		putent(dp);
	}
}
