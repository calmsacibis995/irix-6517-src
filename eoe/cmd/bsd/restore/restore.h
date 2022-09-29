#ident "$Revision: 1.7 $"

/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)restore.h	5.1 (Berkeley) 5/28/85
 */

#include <sys/param.h>
#include <sys/file.h>
#include <sys/fstyp.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/time.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/fs/efs.h>
#include <protocols/dumprestore.h>
#define ROOTINO EFS_ROOTINO	/* easier... */
#define BITSPERBYTE NBBY   /* ? */
#include "dir.h"

/*
 * We can't include these in sun/inode.h, because the
 * EFS structure fields have the same names.
 * Restore doesn't need to know about EFS inodes per se.
 */
#define di_ic           di_un.di_icom
#define di_mode         di_ic.ic_mode
#define di_uid          di_ic.ic_uid
#define di_gid          di_ic.ic_gid
#if defined(vax) || defined(i386)
#define di_size         di_ic.ic_size.val[0]
#endif
#if defined(mc68000) || defined(sparc) || defined(mips)
#define di_size         di_ic.ic_size.val[1]
#endif
#define di_atime        di_ic.ic_atime
#define di_mtime        di_ic.ic_mtime
#define di_ctime        di_ic.ic_ctime
#define di_rdev         di_ic.ic_db[0]

typedef struct {
	int	dd_fd;
	long	dd_loc;
	long	dd_size;
	long	dd_bbase;
	long	dd_entno;
	long	dd_bsize;
	char	dd_buf[BBSIZE];
} RST_DIR;

/*
 * Flags
 */
extern int	cvtflag;	/* convert from old to new tape format */
extern int	bflag;		/* set input block size */
extern int	dflag;		/* print out debugging info */
extern int	hflag;		/* restore heirarchies */
extern int	mflag;		/* restore by name instead of inode number */
extern int	Nflag;		/* do not write the disk */
extern int	vflag;		/* print out actions taken */
extern int	yflag;		/* always try to recover from tape errors */
extern int	oflag;		/* chown(2) files */
/*
 * Global variables
 */
extern char	*dumpmap; 	/* map of inodes on this dump tape */
extern char	*clrimap; 	/* map of inodes to be deleted */
extern efs_ino_t maxino;	/* highest numbered inode in this file system */
extern long	dumpnum;	/* location of the dump on this tape */
extern long	volno;		/* current volume being read */
extern long	ntrec;		/* number of TP_BSIZE records per tape block */
extern time_t	dumptime;	/* time that this dump begins */
extern time_t	dumpdate;	/* time that this dump was made */
extern char	command;	/* opration being performed */
extern FILE	*terminal;	/* file descriptor for the terminal input */
extern time_t	newer;		/* st_mtime of "newer" file under n key */
extern int	newest;		/* E key: extract only newer versions */
extern int	justcreate;	/* e key: don't overwrite existing files */
extern int	efsdirs;	/* directories on tape are in EFS format */
extern int	suser;		/* suser == 1 implies being run as root */
extern union	u_spcl u_spcl;

/*
 * Each file in the file system is described by one of these entries
 */
struct entry {
	char	*e_name;		/* the current name of this entry */
	u_char	e_namlen;		/* length of this name */
	char	e_type;			/* type of this entry, see below */
	short	e_flags;		/* status flags, see below */
	efs_ino_t e_ino;		/* inode number in previous file sys */
	long	e_index;		/* unique index (for dumpped table) */
	struct	entry *e_parent;	/* pointer to parent directory (..) */
	struct	entry *e_sibling;	/* next element in this directory (.) */
	struct	entry *e_links;		/* hard links to this inode */
	struct	entry *e_entries;	/* for directories, their entries */
	struct	entry *e_next;		/* hash chain list */
};
/* types */
#define	LEAF 1			/* non-directory entry */
#define NODE 2			/* directory entry */
#define LINK 4			/* synthesized type, stripped by addentry */
/* flags */
#define EXTRACT		0x0001	/* entry is to be replaced from the tape */
#define NEW		0x0002	/* a new entry to be extracted */
#define KEEP		0x0004	/* entry is not to change */
#define REMOVED		0x0010	/* entry has been removed */
#define TMPNAME		0x0020	/* entry has been given a temporary name */
#define EXISTED		0x0040	/* directory already existed during extract */
#define EXTRACTED	0x0100	/* entry was extracted (used under e key) */
/*
 * functions defined on entry structs
 */
extern struct entry *addentry(char *, efs_ino_t, int);
extern void badentry(struct entry *, char *);
extern int extractfile(char *, struct entry *);
extern char *flagvalues(struct entry *);
extern void freeentry(struct entry *);
extern char *gentempname(struct entry *);
extern struct entry *lookupino(efs_ino_t);
extern struct entry *lookupname(char *);
extern void mktempname(struct entry *);
extern void moveentry(struct entry *, char *);
extern char *myname(struct entry *);
extern void newnode(struct entry *);
extern void removeleaf(struct entry *);
extern void removenode(struct entry *);

#define NIL ((struct entry *)(0))
/*
 * Constants associated with entry structs
 */
#define HARDLINK	1
#define SYMLINK		2
#define TMPHDR		"RSTTMP"

/*
 * The entry describes the next file available on the tape
 */
struct context {
	char	*name;		/* name of file */
	efs_ino_t ino;		/* inumber of file */
	struct	sun_dinode *dip;	/* pointer to inode */
	char	action;		/* action being taken on this file */
};
extern struct context curfile;
/* actions */
#define	USING	1	/* extracting from the tape */
#define	SKIP	2	/* skipping */
#define UNKNOWN 3	/* disposition or starting point is unknown */

/*
 * Other exported routines
 */
extern long addfile(char *, efs_ino_t, int);
extern int all_tapes_read(void);
extern void canon(char *, char *);
extern void checkrestore(void);
extern void closemt(void);
extern void createfiles(void);
extern void createleaves(char *);
extern void createlinks(void);
extern long deletefile(char *, efs_ino_t, int);
extern void deleteino(efs_ino_t);
extern efs_ino_t dirlookup(char *);
extern void done(int);
extern void dumpsymtable(char *, long);
extern void extractdirs(int);
extern void findunreflinks(void);
extern void freename(char *);
extern int genliteraldir(char *, efs_ino_t);
extern void getfile(void (*)(char *, long), void (*)(char *, long));
extern void getvol(long);
extern void initsymtable(char *);
extern int inodetype(efs_ino_t);
extern int linkit(char *, char *, int);
extern long listfile(char *, efs_ino_t, int);
extern efs_ino_t lowerbnd(efs_ino_t);
extern void msg(char *, ...);
extern void newtapebuf(long);
extern long nodeupdates(char *, efs_ino_t, int);
extern void null(char *, long);
extern void onintr();
extern void panic(char *, ...);
extern void pathcheck(char *);
extern efs_ino_t psearch(char *);
extern void printdumpinfo(void);
extern void removeoldleaves(void);
extern void removeoldnodes(void);
extern void renameit(char *, char *);
extern int reply(char *);
extern RST_DIR *rst_opendir(char *);
extern struct direct *rst_readdir(RST_DIR *);
extern void runcmdshell(void);
extern char *savename(char *);
extern void setdirmodes(void);
extern void setinput(char *);
extern void setup(void);
extern void skipdirs(void);
extern void skipfile(void);
extern void skipmaps(void);
extern void treescan(char *, efs_ino_t, long (*)(char *, efs_ino_t, int));
extern efs_ino_t upperbnd(efs_ino_t);
extern long verifyfile(char *, efs_ino_t, int);

/*
 * Useful macros
 */
#define	MWORD(m,i) (m[(unsigned)(i-1)/NBBY])
#define	MBIT(i)	(1<<((unsigned)(i-1)%NBBY))
#define	BIS(i,w)	(MWORD(w,i) |=  MBIT(i))
#define	BIC(i,w)	(MWORD(w,i) &= ~MBIT(i))
#define	BIT(i,w)	(MWORD(w,i) & MBIT(i))

#define Dprintf		if (dflag) fprintf
#define Vprintf		if (vflag) fprintf

#define GOOD 1
#define FAIL 0

/*
 *	Exit status codes (for dumprmt)
 */
#define	X_FINOK		0	/* normal exit */
#define	X_REWRITE	2	/* restart writing from the check point */
#define	X_ABORT		3	/* abort all of dump; don't attempt checkpointing */
