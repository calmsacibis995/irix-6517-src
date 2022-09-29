/*	Copyright (c) 1993 UNIX System Laboratories, Inc. 	*/
/*	  All Rights Reserved                             	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.   	            	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*		copyright	"%c%" 	*/


#ident	"@(#)acct:i386/cmd/acct/acctdef.h	1.17.3.4"
#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/acct/RCS/acctdef.h,v 1.14 1996/06/14 21:05:39 rdb Exp $"

#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <sys/acct.h>

#define PACCT		"/var/adm/pacct"
#ifdef sgi
#define HOLFILE		"/usr/lib/acct/holidays"
#else
#define HOLFILE		"/etc/acct/holidays"
#endif
#define	NHOLIDAYS	366	/* max number of company holidays per year */
#define NSYS		100
#define NFILE		100	/* max number of files that acctmerg handles */

#ifndef NODEV
#define NODEV		(dev_t)(-1)	/* no device */
#endif
/*
 * BEGIN machine specific defines.
 */
#ifndef HZ
#define	HZ		100	/* HZ is 100 for 386 */
#endif
#define CSIZE 		5001	/* max # distinct commands for acctcms */
#define	MAXUSERS	10001	/* distinct login names for acctdusg and diskusg */
#define A_SSIZE 	6001     /* max num of sessions in 1 acct run */
#define A_TSIZE 	1001     /* max num of line names in 1 acct run */
#define A_USIZE 	10001	/* max num of distinct login names in 1 acct run */
/*
 * END machine specific defines
 */
/*
 * 	The above limits can be altered dynamically by setting the following
 *	environment variables
 */
#define	ACCT_CSIZE	"ACCT_CSIZE"
#define	ACCT_MAXUSERS	"ACCT_MAXUSERS"
#define	ACCT_MAXIGN	"ACCT_MAXIGN"
#define ACCT_A_SSIZE	"ACCT_A_SSIZE"
#define ACCT_A_TSIZE	"ACCT_A_TSIZE"
#define ACCT_A_USIZE	"ACCT_A_USIZE"

#define TSIZE1		100	/* # distinct names, for speed only */
#define USIZE1		100

#define	MAXIGN		150
#define	UNUSED		-1
#define	FAIL		-1
#define	MAXNAME		8
#define	SUCCEED		0
#define	TRUE		1
#define	FALSE		0
#define PRIME		0
#define NONPRIME	1
#define MEANSIZE	01
#define KCOREMIN	02
#define HOGFACTOR	04
#define	SEPTIME		010
#define	CPUFACTOR	020
#define IORW		040
#define	ROOT		0
#define	ERR		(-1)
#define	OK		0
#define	NOGOOD		1
#define	VALID		0
#define	INVALID		1
#define LSZ		12	/* maximum length of a line name */
#define MAX_DEV_PATH	LSZ + 5		/* maximum size of absolute line name path */
#define MAX_SRCH_DEPTH	4	/* maximum search depth in /dev directory for line name */
#define NSZ		8	/* sizeof login name */

#define MYKIND(flag)	((flag & ACCTF) == 0)
#define SU(flag)	((flag & ASU) == ASU)
#define TOTAL(a)	(a[PRIME] + a[NONPRIME])
#define	okay(time)	((time/100>=0) && (time/100<=24) \
			&& (time%100>=0) && (time%100<60))
#define	pf(dble)	fprintf(stdout, "%8.2lf", dble)
#define	pfb(dble)	fprintf(stdout, " %12.2lf", dble)
#define	ps(s)		fprintf(stdout, "%8.8s", s)
#define	psb(s)		fprintf(stdout, "%13.13s", s)
#define plld(ull)	fprintf(stdout, " %12lld", ull)
#define	diag(string)	fprintf(stderr, "\t%s\n", string)
#define DATE_FMT	"%a %b %e %H:%M:%S %Y\n"
#define DATE_FMT1	" %H:%M:%S"
#define CBEMPTY   	(ctab[i].ct_sess == 0)
#define UBEMPTY   	(ub[i].ut_pc == 0 && ub[i].ut_cpu[0] == 0 && \
ub[i].ut_cpu[1] == 0 && ub[i].ut_kcore[0] ==0 && ub[i].ut_kcore[1] == 0)


#define EQN(s1, s2)	(strncmp(s1, s2, sizeof(s1)) == 0)
#define CPYN(s1, s2)	strncpy(s1, s2, sizeof(s1))

#define SECSINDAY	86400L
#define SECS(tics)	((double) tics)/HZ
#define MINS(secs)	((double) secs)/60
#define MINT(tics)	((double) tics)/(60*HZ)


/*
 * BEGIN machine specific define
 * NOTE- kernel should translate all sizes into canonical 4k pages
 * since different processes could have different page sizes
 */
#define KCORE(clicks)	((double) clicks*4)
/*
 * END machine specific define
 */


/*
 *	total accounting (for acct period), also for day
 */

struct	tacct	{
	uid_t		ta_uid;		/* userid */
	char		ta_name[8];	/* login name */
	float		ta_cpu[2];	/* cum. cpu time, p/np (mins) */
	float		ta_kcore[2];	/* cum kcore-minutes, p/np */
	float		ta_con[2];	/* cum. connect time, p/np, mins */
	float		ta_du;		/* cum. disk usage */
	long		ta_pc;		/* count of processes */
	unsigned short	ta_sc;		/* count of login sessions */
	unsigned short	ta_dc;		/* count of disk samples */
	unsigned short	ta_fee;		/* fee for special services */
};


/*
 *	connect time record 
 */
struct ctmp {
	dev_t	ct_tty;			/* major minor */
	uid_t	ct_uid;			/* userid */
	char	ct_name[8];		/* login name */
	long	ct_con[2];		/* connect time (p/np) secs */
	time_t	ct_start;		/* session start time */
};

/*
 *	per process temporary data
 */
struct ptmp {
	uid_t	pt_uid;			/* userid */
	char	pt_name[8];		/* login name */
	long	pt_cpu[2];		/* CPU (sys+usr) P/NP time tics */
	unsigned pt_mem;		/* avg. memory size (64byte clicks) */
};	


/*
 * Prototypes for functions in "a.a"
 */
void	checkhol(void);
char	*copyn(char *, char *, int);
int	day_of_year(int, int, int);
char	*devtolin(dev_t);
__uint32_t expand32(comp_t);
__uint64_t expand64(comp_t);
int	inithol(int);
dev_t	lintodev(char *);
uid_t	namtouid(char *);
void	pnpsplit(long, long, long[2]);
int	ssh(struct tm *);
char	*substr(char *, char *, int, unsigned);
int	tmless();	/* Too much wierdness to declare args */
long	tmsecs();	/* Ditto */
char	*uidtonam(uid_t);
