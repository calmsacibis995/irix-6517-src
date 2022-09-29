#ident "$Revision: 1.10 $"

/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)dump.h	5.4 (Berkeley) 2/23/87
 */

#include <sys/param.h>
#include <sys/file.h>
#include <sys/fstyp.h>
#include <sys/mount.h>
#include <sys/signal.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/syssgi.h>
#include <sys/tpsc.h>
#include <sys/wait.h>
#include <assert.h>
#include <bstring.h>
#include <ctype.h>
#include <diskinfo.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <mntent.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>
#include <utmp.h>
#include <sys/fs/efs.h>
#include <protocols/dumprestore.h>
#include "dir.h"

#if TP_BSIZE / DEV_BSIZE == 0 || TP_BSIZE % DEV_BSIZE != 0
	! TP_BSIZE must be a multiple of DEV_BSIZE.
	! You lose.
#endif

#define BITSPERBYTE	NBBY
#define EXTSPERBB (BBSIZE/sizeof(struct extent))
#define	MWORD(m,i)	(m[(unsigned)(i-1)/NBBY])
#define	MBIT(i)		(1<<((unsigned)(i-1)%NBBY))
#define	BIS(i,w)	(MWORD(w,i) |=  MBIT(i))
#define	BIC(i,w)	(MWORD(w,i) &= ~MBIT(i))
#define	BIT(i,w)	(MWORD(w,i) & MBIT(i))

typedef int (*walkarg_t)(void *);
typedef int (*walkfunc_t)(struct extent *, walkarg_t arg);

extern void			add(struct efs_dinode *);
extern int			alloctape(void);
extern void			bitmap(char *, int);
extern void			bmapest(char *);
extern void			bread(unsigned, char *, int);
extern void			broadcast(char *);
extern char			*bsd_dumpdir(struct efs_dinode *);
extern void			close_rewind(void);
extern void			dirdump(struct efs_dinode *);
extern void			dmpblk(unsigned, int, char *);
extern void			dmpblk_end(void);
extern void			dmpblk_pad(int);
extern void			dmpblk_start(void);
extern void			dmpblk_sync(void);
extern void			dump(struct efs_dinode *);
extern void			dump_rewind(void);
extern void			dumpabort(void);
extern void			est(struct efs_dinode *);
extern void			Exit(int);
extern struct mntent		*fstabsearch(char *);
extern void			getfstab(void);
extern void			getitime(void);
extern void			inititimes(void);
extern void			interrupt();
extern void			lastdump(int);
extern void			mark(struct efs_dinode *);
extern void			msg(char *, ...);
extern void			msgtail(char *, ...);
extern void			otape(void);
extern void			pass(void (*)(struct efs_dinode *), char *);
extern char			*prdate(time_t);
extern void			putitime(void);
extern int			query(char *, int);
extern char			*rawname(struct mntent *);
extern void			set_operators(void);
extern void			sigalrm();
extern void			spclrec(void);
extern void			taprec(char *);
extern void			tflush(void);
extern void			timeest(void);
extern time_t			unctime(char *);
extern int			walkdir(struct extent *, walkarg_t);
extern int			walkextents(struct efs_dinode *, walkfunc_t, walkarg_t);

#define	HOUR	(60L*60L)
#define	DAY	(24L*HOUR)
#define	YEAR	(365L*DAY)

/*
 *	Exit status codes
 */
#define	X_FINOK		0	/* normal exit */
#define	X_REWRITE	2	/* restart writing from the check point */
#define	X_ABORT		3	/* abort all of dump; don't attempt checkpointing */

#define	NINCREM	"/etc/dumpdates"	/*new format incremental info*/
#define	TEMP	"/etc/dtmp"		/*output temp file*/

#define	TAPE	"/dev/tape"		/* default tape device */
#define	OPGRENT	"operator"		/* group entry to notify */
#define DIALUP	"ttyd"			/* prefix for dialups */

/*
 *	The contents of the file NINCREM (/etc/dumpdates) is maintained both on
 *	a linked list, and then (eventually) arrayified.
 */
struct	idates {
	char	id_name[MAXNAMLEN+3];
	char	id_incno;
	time_t	id_ddate;
};
struct	itime{
	struct	idates	it_value;
	struct	itime	*it_next;
};

#define	ITITERATE(i, ip) for (i = 0,ip = idatev[0]; i < nidates; i++, ip = idatev[i])


#define	TPTOBB(n)	((((long long)(n)) * TP_BSIZE + BBSIZE - 1) >> BBSHIFT)
#define	BBTOTP(n)	((((long long)(n)) << BBSHIFT) / TP_BSIZE)

#ifdef _DUMPMAIN_C_
#define EXTERN
#else
#define EXTERN extern
#endif /* _DUMPMAIN_C_ */

/*
 *	All calculations done in 0.1" units!
 */

EXTERN int	anydskipped;	/* set true in mark() if any directories are */
				/* skipped, this lets us avoid map pass 2 in */
				/* some cases */
EXTERN int	blockswritten;	/* total number of blocks written on all tapes */
EXTERN int	bsdformat;	/* directory dumping format */
EXTERN int	cartridge;	/* Assume non-cartridge tape */
EXTERN int	Cflag;		/* capacity flag given; see dumptape.c */
EXTERN char	*clrmap;
EXTERN int	density;	/* density in 0.1" units */
EXTERN long	dev_bsize;
EXTERN char	*dirmap;
EXTERN int	dirphase;	/* true while we're dumping directories */
EXTERN char	*disk;		/* name of the disk file */
EXTERN int	dofork;
EXTERN long	esize;		/* estimated tape size, blocks */
EXTERN int	fi;		/* disk file descriptor */
EXTERN char	*host;
EXTERN struct idates	**idatev;	/* the arrayfied version */
EXTERN char	incno;		/* increment number */
EXTERN char	*increm;	/* name of the file containing incremental information */
EXTERN efs_ino_t ino;		/* current inumber; used globally */
EXTERN char	lastincno;	/* increment number of previous dump */
EXTERN int	msiz;
EXTERN int	nadded;		/* number of added sub directories */
EXTERN int	newtape;	/* new tape flag */
EXTERN int	nidates;	/* number of records (might be zero) */
EXTERN char	*nodmap;
EXTERN int	notify;		/* notify operator flag */
EXTERN int	ntrec;		/* # tape blocks in each tape record */
EXTERN int	pipeout;	/* true => output to standard output */
EXTERN struct efs	*sblock;/* the file system super block */
EXTERN char	*tape;		/* name of the tape file */
EXTERN int	tapeno;		/* current tape number */
EXTERN int	to;		/* tape file descriptor */
EXTERN long	tsize;		/* tape size in 0.1" units or in 1K blocks if C arg use) */
EXTERN time_t	tstart_writing;	/* when started writing the first tape block */
EXTERN union u_spcl	u_spcl;
EXTERN int	uflag;		/* update flag */
