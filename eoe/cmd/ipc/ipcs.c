/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* #ident	"@(#)ipc:/ipcs.c	1.9" */
#ident "$Revision: 1.25 $"
/*
**	ipcs - IPC status
**	Examine and print certain things about message queues, semaphores,
**		and shared memory.
*/

#include	"sys/types.h"
#include	"sys/ipc.h"
#include	"sys/msg.h"
#include	"sys/sem.h"
#include	"sys/shm.h"
#include	<unistd.h>
#include	"errno.h"
#include	"nlist.h"
#include	"fcntl.h"
#include	"time.h"
#include	"grp.h"
#include	"pwd.h"
#include	"getopt.h"
#include	"stdio.h"
#include	"stdlib.h"
#include	"sys/sysmp.h"
#include	"sys/sysget.h"

#include 	"sys.s"
extern int syscall(int, ...);

#define MSG     0
#define MSGINFO 2

void hp(char, char *, struct ipc_perm *, int, int);
void tp(time_t);

static void lseeke(int, off_t, int);
static void reade(int fildes, void *buf, size_t nbyte);

void print_shmstat(void);
void print_semstat(void);

#if (_MIPS_SZLONG == 64)
#define NLIST   nlist64
#else
#define NLIST   nlist
#endif /* if 64bit */

struct NLIST nl[] = {		/* name list entries for IPC facilities */
	{"msgque", 0, 0, 0},
	{"msginfo", 0, 0, 0},
	{NULL}
};

char	chdr[] = "T         ID     KEY        MODE       OWNER    GROUP",
				/* common header format */
	chdr2[] = "  CREATOR   CGROUP",
				/* c option header format */
	*name = "/unix",	/* name list file */
	*mem = "/dev/kmem",	/* memory file */
	opts[] = "abcemopqstC:N:";/* allowable options for getopt */
extern char	*optarg;	/* arg pointer for getopt */
int		bflg,		/* biggest size:
					segsz on m; qbytes on q; nsems on s */
		cflg,		/* creator's login and group names */
		eflg,		/* shared memory extentions */
		mflg,		/* shared memory status */
		oflg,		/* outstanding data:
					nattch on m; cbytes, qnum on q */
		pflg,		/* process id's: lrpid, lspid on q;
					cpid, lpid on m */
		qflg,		/* message queue status */
		sflg,		/* semaphore status */
		tflg,		/* times: atime, ctime, dtime on m;
					ctime, rtime, stime on q;
					ctime, otime on s */
		err;		/* option error count */
int		Nflg;		/* specified -N */


main(
	int	argc,	/* arg count */
	char	**argv)	/* arg vector */
{
	register int	i,	/* loop control */
			md,	/* memory file file descriptor */
			o;	/* option flag */
	time_t		curtime;/* date in memory file */
	struct msqid_ds	*qds;	/* message queue data structure */
	struct msginfo msginfo;	/* message information structure */
	struct msqid_ds *qp;
	sgt_cookie_t ck;

	/* Go through the options and set flags. */
	while((o = getopt(argc, argv, opts)) != EOF)
		switch(o) {
		case 'a':
			bflg = cflg = oflg = pflg = tflg = 1;
			break;
		case 'b':
			bflg = 1;
			break;
		case 'c':
			cflg = 1;
			break;
		case 'C':
			mem = optarg;
			break;
		case 'e':
			eflg = 1;
			break;
		case 'm':
			mflg = 1;
			break;
		case 'N':
			name = optarg;
			Nflg++;
			break;
		case 'o':
			oflg = 1;
			break;
		case 'p':
			pflg = 1;
			break;
		case 'q':
			qflg = 1;
			break;
		case 's':
			sflg = 1;
			break;
		case 't':
			tflg = 1;
			break;
		case '?':
			err++;
			break;
		}
	if(err || (optind < argc)) {
		fprintf(stderr,
			"usage:  ipcs [-abcemopqst] [-C corefile] [-N namelist]\n");
		exit(1);
	}
	if((mflg + qflg + sflg) == 0)
		mflg = qflg = sflg = 1;

	/* Check out namelist and memory files. */
	if (Nflg) {
		NLIST(name, nl);
		if(!nl[MSG].n_value) {
			fprintf(stderr, "ipcs:  no namelist\n");
			exit(1);
		}
	}

	if(Nflg && ((md = open(mem, O_RDONLY)) < 0)) {
		fprintf(stderr, "ipcs:  no memory file\n");
		exit(1);
	}

	if (Nflg && (qflg || sflg)) {
		/* msgque is a struct msqid_ds **, so we have
		 * to step through the indirection. But only if
		 * Nflg is set - otherwise sysmp returns the
		 * correct value.
		 */
		lseeke(md, nl[MSG].n_value, 0);
		reade(md, &nl[MSG].n_value, sizeof(nl[MSG].n_value));
	}

	curtime = time(NULL);
	printf("IPC status from %s as of %s", mem, ctime(&curtime));

	/* Print Message Queue status report. */
	if(qflg) {
		if (Nflg) {
			lseeke(md, (long)nl[MSGINFO].n_value, 0);
			reade(md, &msginfo, sizeof(msginfo));
			lseeke(md, (long)nl[MSG].n_value, 0);
		}
		else {
			SGT_COOKIE_INIT(&ck);
			SGT_COOKIE_SET_KSYM(&ck, KSYM_MSGINFO);
			if (sysget(SGT_KSYM, (char *)&msginfo, sizeof(msginfo),
				SGT_READ, &ck) < 0) {
				perror("SGT_READ msginfo");
				exit(1);
			}
		}
		printf("%s%s%s%s%s%s\nMessage Queues:\n", chdr,
			cflg ? chdr2 : "",
			oflg ? " CBYTES  QNUM" : "",
			bflg ? " QBYTES" : "",
			pflg ? " LSPID LRPID" : "",
			tflg ? "   STIME    RTIME    CTIME " : "");
		qds = (struct msqid_ds *)calloc(msginfo.msgmni,
			 sizeof(struct msqid_ds));
		if (!qds) {
			fprintf(stderr, "no memory for message queue\n");
			exit(1);
		}
		if (!Nflg) {
			SGT_COOKIE_INIT(&ck);
			if (sysget(SGT_MSGQUEUE, (char *)qds, msginfo.msgmni *
					sizeof(struct msqid_ds), SGT_READ,
					&ck) < 0) {
				perror("SGT_READ msgqueue");
				exit(1);
			}
		}
		for (i = 0; i < msginfo.msgmni; i++) {
			if (Nflg) {
				lseeke(md, (long)nl[MSG].n_value +
					 (i*sizeof(int *)), 0);
				reade(md, &qp, sizeof(qp));
				if (qp == NULL) {
					continue;
				}
				lseeke(md, (off_t)qp, 0);
				reade(md, &qds[i], sizeof(struct msqid_ds));
			}
			if(!(qds[i].msg_perm.mode & IPC_ALLOC)) {
				continue;
			}
			hp('q', "SRrw-rw-rw-", &qds[i].msg_perm, i,
				 msginfo.msgmni);
			if(oflg)
				printf("%7u%6u", qds[i].msg_cbytes,
					 qds[i].msg_qnum);
			if(bflg)
				printf("%7u", qds[i].msg_qbytes);
			if(pflg)
				printf("%6u%6u", qds[i].msg_lspid,
					 qds[i].msg_lrpid);
			if(tflg) {
				tp(qds[i].msg_stime);
				tp(qds[i].msg_rtime);
				tp(qds[i].msg_ctime);
			}
			printf("\n");
		}
	}

	/* Print Shared Memory status report. */
	if (mflg) 
		print_shmstat();

	/* Print Semaphore facility status. */
	if (sflg) 
		print_semstat();

	exit(0);
	/* NOTREACHED */
}

extern int __semstatus(struct semstat *);

void
print_semstat(void)
{
	struct semstat	semstat;
	int rv;

	if (bflg || tflg || !qflg && !mflg)
		printf("%s%s%s%s\n", chdr,
			cflg ? chdr2 : "",
			bflg ? " NSEMS" : "",
			tflg ? "   OTIME    CTIME " : "");
	printf("Semaphores:\n");
	semstat.sm_id = 0;
	semstat.sm_location = -1LL;
	while (((rv = __semstatus(&semstat)) == 0) || (errno == EACCES)) {
		struct semid_ds	*semds;

		if (!rv && (errno == EACCES))
			continue;

		semds = &semstat.sm_semds;

		hp('s', "--ra-ra-ra-", &semds->sem_perm, semstat.sm_id, 0);
		if(bflg)
			printf("%6u", semds->sem_nsems);
		if(tflg) {
			tp(semds->sem_otime);
			tp(semds->sem_ctime);
		}
		printf("\n");
	}
}

extern int __shmstatus(struct shmstat *);

void
print_shmstat(void)
{
	struct shmstat	shmstat;
	int rv;

	if (oflg || bflg || tflg || !qflg || eflg)
		printf("%s%s%s%s%s%s%s\n", chdr,
			cflg ? chdr2 : "",
			oflg ? " NATTCH " : "",
			bflg ? "    SEGSZ " : "",
			pflg ? "  CPID  LPID" : "",
			tflg ? "   ATIME    DTIME    CTIME " : "",
			eflg ? " EXTENSIONS" : "");
	printf("Shared Memory:\n");
	shmstat.sh_id = 0;
	shmstat.sh_location = -1LL;
	while (((rv = __shmstatus(&shmstat)) == 0) || (errno == EACCES)) {
		struct shmid_ds	*shmds;

		shmds = &shmstat.sh_shmds;

		if (!rv && (errno == EACCES)) {
			if (shmds->shm_cpid == NOPID)
				continue;

			/* Special case: We created this segments, but we
			   didn't give ourselves read access.  MP_SHMSTAT
			   fills in just the cpid/cgid/cuid fields.  We'll
			   overwrite the uid and gid fields with cuid/cgid
			   just to make it more obvious who should do
			   something about this situation.
			 */
			shmds->shm_perm.uid = shmds->shm_perm.cuid;
			shmds->shm_perm.gid = shmds->shm_perm.cgid;
		}

		hp('m', "DCrw-rw-rw-", &shmds->shm_perm, 
		    shmstat.sh_id,
		    0);
		if(oflg)
			printf("%7u ", shmds->shm_nattch);
		if(bflg)
			printf("%9ld ", shmds->shm_segsz);
		if(pflg)
			printf("%6u%6u", shmds->shm_cpid,
					 shmds->shm_lpid);
		if(tflg) {
			tp(shmds->shm_atime);
			tp(shmds->shm_dtime);
			tp(shmds->shm_ctime);
		}
		if (eflg) {
			printf(" %c", shmds->shm_perm.mode & IPC_AUTORMID ? 'A' : ' ');
		}
		printf("\n");
	}
}

/*
**	lseeke - lseek with error exit
*/
static void
lseeke(
	int	fd,
	off_t	offset,
	int	whence)
{
#if _MIPS_SZPTR == 64
	offset &= 0x7fffffffffffffff;
#else
	offset &= 0x7fffffff;
#endif
	if (lseek(fd, offset, whence) == -1) {
		perror("ipcs:  seek error");
		exit(1);
	}
}

/*
**	reade - read with error exit
*/
static void
reade(
	int	fd,
	void	*bufaddr,
	size_t	size)
{
	if (read(fd, bufaddr, size) != size) {
		perror("ipcs:  read error");
		exit(1);
	}
}

/*
**	hp - common header print
*/
void
hp(
	char		type,	/* facility type */
	char		*modesp,/* ptr to mode replacement characters */
	struct ipc_perm	*permp,	/* ptr to permission structure */
	int		slot,	/* facility slot number */
	int		slots)	/* # of facility slots */
{
	register int		i,	/* loop control */
				j;	/* loop control */
	register struct group	*g;	/* ptr to group group entry */
	register struct passwd	*u;	/* ptr to user passwd entry */

	printf("%c%11d%s%#8.8x ", type, slot + slots * permp->seq,
		permp->key ? " " : " 0x", permp->key);
	for(i = 02000;i;modesp++, i >>= 1)
		printf("%c", (permp->mode & i) ? *modesp : '-');
	if((u = getpwuid(permp->uid)) == NULL)
		printf("%9d", permp->uid);
	else
		printf("%9.8s", u->pw_name);
	if((g = getgrgid(permp->gid)) == NULL)
		printf("%9d", permp->gid);
	else
		printf("%9.8s", g->gr_name);
	if(cflg) {
		if((u = getpwuid(permp->cuid)) == NULL)
			printf("%9d", permp->cuid);
		else
			printf("%9.8s", u->pw_name);
		if((g = getgrgid(permp->cgid)) == NULL)
			printf("%9d", permp->cgid);
		else
			printf("%9.8s", g->gr_name);
	}
}

/*
**	tp - time entry printer
*/
void
tp(
	time_t	time)	/* time to be displayed */
{
	register struct tm	*t;	/* ptr to converted time */

	if(time) {
		t = localtime(&time);
		printf(" %2d:%2.2d:%2.2d", t->tm_hour, t->tm_min, t->tm_sec);
	} else
		printf(" no-entry");
}
