/*
 * sysid.c
 *
 *	Simple-minded (PID, IP address) <-> system ID mapping functions
 *
 *
 * Copyright 1991,1992 Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#ident "$Revision: 1.11 $"


#include <stdio.h>
#include <sys/types.h>
#include <sys/syssgi.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/flock.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
/* #include <sys/systm.h> */
#include <netinet/in.h>
#include <syslog.h>
#include <bstring.h>
#include <assert.h>
#include <netdb.h>
#include <unistd.h>
#include "prot_lock.h"

/* region types */
#define	S_BEFORE	010
#define	S_START		020
#define	S_MIDDLE	030
#define	S_END		040
#define	S_AFTER		050
#define	E_BEFORE	001
#define	E_START		002
#define	E_MIDDLE	003
#define	E_END		004
#define	E_AFTER		005

static struct filock *insflck(struct filock **, struct flock *, struct filock *);
static void delflck(struct filock  **lck_list, struct filock  *fl);
static int regflck(struct flock *ld, struct filock *flp);
static int flckadj(struct filock **, struct filock *, struct flock *);
static struct filock *blocked(struct filock *, struct flock *, struct filock **);
static int isonlist(struct filock  *lck_list, struct filock  *fl);
static filock_t *get_filock_list(int fd);

extern int debug;
extern int pid;
extern int errno;


#define MAXIDS	1000
#define FDS 10		/* initial size of fd array */

static struct id {
    u_long		ipaddr;		/* client's address,
				 * IPADDR_ANY (0) == not used */
    pid_t		pid;		/* and PID */
    int			*fd;		/* array of fd's with active locks */
    int			nfd;		/* current # of fds in array */
    int			fdsize;		/* max size of array */
	int			filocks;
} id[MAXIDS];
static int max;			/* high water mark for last id used */



/*
 * Insert the fd into sysid's entry, if it's not there.
 * Returns 1 for success, 0 if the mallocs fail.
 */
static int
addfd(struct id *id, int fd)
{
	if (id->fd) {
		int j;
		for (j=0; j < id->nfd; j++) {
			if (id->fd[j] == fd) {
				return 1;
			}
		}
		if (id->fdsize == id->nfd) {
			id->fdsize *= 2;
			if ((id->fd = realloc(id->fd, 
					sizeof(fd) * id->fdsize)) == NULL) {
				return 0;
			}
			bzero(&id->fd[id->nfd], sizeof(fd) * id->nfd);
		}
	} else {
		id->fdsize = FDS;
		if ((id->fd = calloc(id->fdsize, sizeof(fd))) == NULL) {
			return 0;
		}
		id->nfd = 0;
	}
	id->fd[id->nfd++] = fd;
	return 1;
}

/*
 * Remove the fd from the sysid's entry, if it's there.
 * Returns the number of remaining fd's for the id, or -1 if the fd is not
 * found. 0 means the sysid can be discarded.
 */

static int
rmfd(struct id *id, int fd)
{
	int j;

	for (j=0; j < id->nfd; j++) {
		if (id->fd[j] == fd) {
			if (--id->nfd > 0) {
				bcopy(&id->fd[j+1], &id->fd[j],
				    sizeof(fd) * (id->nfd - j));
			}
			return id->nfd;
		}
	}
	return -1;
}


sysid_t
findsysid( pid_t pid, u_long ipaddr )
{
	int i;

	for ( i = 0; i < max; i++ ) {
		if ( (id[i].ipaddr == ipaddr) && (id[i].pid == pid) ) {
			return( i + 1 );
		}
	}
	return( 0 );
}

sysid_t
mksysid(pid_t pid, u_long ipaddr, int *isnewp)
{
	int i, free = -1;

	for (i = 0; i < max; i++) {
		if (id[i].ipaddr == ipaddr && id[i].pid == pid) {
			if (isnewp)
				*isnewp = 0;
			return i+1;
		}
		if (id[i].ipaddr == INADDR_ANY && free == -1) {
			free = i;
		}
	}
	if (free == -1) {	/* no free slots */
		if (max >= MAXIDS) {
			if (debug)
				printf("!! mksysid: out of IDs (%d)\n", max);
			if (isnewp)
				*isnewp = 0;
			return 0;	/* failed */
		}
		free = max++;
	}
	id[free].pid = pid;
	id[free].ipaddr = ipaddr;
	id[free].filocks = 0;
	if (isnewp)
		*isnewp = 1;
	return free+1;		/* sysid 0 == this host */
}

pid_t
sysid2pid(sysid_t sysid)
{
	if (sysid > 0 && sysid <= MAXIDS) {
		if (id[sysid-1].ipaddr != INADDR_ANY)
			return id[sysid-1].pid;
	}
	if (debug)
		printf("sysid2pid: unknown %d\n", sysid);
	return 0;
}

static void
freeid(struct id *id)
{
	if (id->fd)
		free(id->fd);
	bzero(id, sizeof(*id));
}

void
relsysid(sysid_t sysid)
{
	int lock_count = 0;
	filock_t *flp;
	struct id *idp;
	int i;

	if (sysid > 0 && sysid <= MAXIDS) {
		idp = &id[sysid-1];
		assert(idp->ipaddr != INADDR_ANY);
		/*
		 * If there are no locks held by this sysid, free the sysid.
		 * Determine this by looking at the file descriptor list.
		 * It is important that this check be done after lock accounting
		 * and after the file descriptor has been freed.
		 */
		if ( (idp->nfd == 0) || (idp->filocks == 0) ) {
			if (debug) {
				printf("relsysid: releasing sysid %d, nfd %d, filocks %d\n",
					(int)sysid, idp->nfd, idp->filocks);
			}
			/*
			 * if idp->nfd == 0 then idp->filocks == 0
			 */
			assert(idp->filocks == 0);
			/*
			 * don't worry if nfd is non-zero, just free the sysid
			 */
			freeid(idp);
		}
	} else if (debug)
		printf("relsysid: unknown %d\n", sysid);
}

static void
clear_fd(int fd)
{
	int i;

	for (i = 0; i < max; i++) {
		if (id[i].ipaddr != INADDR_ANY && rmfd(&id[i], fd) == 0) {
			if (debug)
				printf(" clr: id %d  (%x %d)\n",
					i+1, id[i].ipaddr, id[i].pid);
		}
	}
}

int
save_fd(sysid_t sysid, int fd)
{
	if (sysid > 0 && sysid <= MAXIDS) {
		assert(id[sysid-1].ipaddr != INADDR_ANY);
		return addfd(&id[sysid-1], fd);
	}
	if (debug)
		printf("save_fd: unknown %d\n", sysid);
	return 0;
}

void
foreach_fd(u_long ipaddr, void (*proc)(int, sysid_t))
{
	int i;

	for (i = 0; i < max; i++) {
		if (id[i].ipaddr == ipaddr) {
			int j;

			for (j=0; j < id[i].nfd; j++) {
				if (id[i].fd[j]) {
					proc(id[i].fd[j], i+1);
				}
			}
			freeid(&id[i]);
		}
	}
}

/* ---------------------------------------------------------------------- */

/* 
 * File handle to file descriptor mapping.
 */

static struct fdtable {
	netobj	fh;
	int	fd;		/* 0 means unused */
	int	nblocked;
	int	gc;		/* try to garbage collect */
	struct stat	stat;	/* get inumber for debugging */
	filock_t	*filocks;	/* list of locks held on this file */
	filock_t	*blocked;	/* list of locks blocked on this file */
} *fd_table;
static int lastfd;
static int used_fd;

/* ---------------------------------------------------------------------- */

static int
isunlocked(int fd)
{
	return (fd_table[fd].filocks == NULL);
}

void
init_fdtable(void)
{
	int num = sysconf(_SC_OPEN_MAX);

	fd_table = calloc(num, sizeof(struct fdtable));
	if (fd_table == NULL) {
		syslog(LOG_ERR, "fatal error: no mem. for fdtable (%d slots)",
			num);
		exit(1);
	}
}

static void
printffdt(FILE *fp)
{
	register struct fdtable *f;
	register int i;

	if (fd_table == NULL)
		return;

	fprintf(fp, "In print_fdtable()... #fd %d, last %d\n", used_fd, lastfd);

	for (f = fd_table, i = 0; i <= lastfd; f++, i++) {
		if (f->fd) {
			fprintf(fp, "%d: fd %d #bl %d gc %d inum %d/%x\n",
				i,
				f->fd,
				f->nblocked,
				f->gc,
				f->stat.st_ino,
				f->stat.st_dev);
		}
	}
}

static void
print_fdtable(void)
{
	printffdt(stdout);
}

void
dumpfdt(FILE *f)
{
	printffdt(f);
}


void
fd_block(int fd, int cnt)
{
	assert(fd > 0 && fd <= lastfd);
	assert(fd_table[fd].fd);
	fd_table[fd].nblocked += cnt;
	assert(fd_table[fd].nblocked >= 0);
}

void
fd_unblock(struct reclock *rec)
{
	int fd = rec->blk.fd;
	struct flock lckdat;

	fd_block(fd, -1);
	/*
	 * remove the file lock from the blocked list
	 */
	if ( rec->blk.flp ) {
		bcopy(&rec->blk.flp->set, &lckdat, sizeof(lckdat));
		delflck(&fd_table[fd].blocked, rec->blk.flp);
		errno = record_lock(fd, rec, &lckdat);
		if ( errno ) {
			syslog(LOG_ERR, "fd_unblock: unable to record lock: %m");
		}
	}
}


static void
free_fdt(struct fdtable *f)
{
	(void) close(f->fd);
	bzero(f->fh.n_bytes, sizeof (f->fh.n_bytes));
	xfree(&f->fh.n_bytes);
	f->fh.n_len = 0;
	if (f->fd == lastfd)
		lastfd--;
	f->fd = f->gc = 0;

	used_fd--;
	assert(used_fd >= 0);
}

void
fdgctimer(void)
{
	register struct fdtable *f;
	register int i;
	int cnt = 0;

	if (debug)
		printf("\nenter fdgctimer\n");
	for (f = fd_table, i = 0; i <= lastfd; f++, i++) {
		if (f->fd && f->gc) {
			if (isunlocked(f->fd))
				free_fdt(f);
			else
				cnt++;
		}
	}
	if (cnt != 0)
		start_timer(TIMER_FDGC);
}

void
remove_fd(int fd)
{
	struct fdtable *f = &fd_table[fd];

	assert(fd > 0 && fd <= lastfd);
	assert(f->fd);
	if (debug) {
		printf("rm_fd: %d inum %d/%x\n",
			fd, f->stat.st_ino, f->stat.st_dev);
	}
	free_fdt(f);

	if (debug)
		print_fdtable();
}


int
get_fd(a, isnewp)
	struct reclock *a;
	int *isnewp;
{
	register struct fdtable *f;
	int fd, i;

	for (f = fd_table, i = 0; i <= lastfd; f++, i++) {
		if (f->fd && obj_cmp(&(f->fh), &(a->lck.fh))) {
			if (debug) {
				printf("get_fd: old #%d %d inum %d/%x\n",
					i, f->fd,
					f->stat.st_ino, f->stat.st_dev);
			}
			if (isnewp)
				*isnewp = 0;
			f->gc = 0;
			return (f->fd);
		}
	}

	if (debug) {
		printf("get_fd: fh=");
		for (i = 0; i < a->lck.fh.n_len; i++) {
			printf("%02x", (a->lck.fh.n_bytes[i] & 0xff));
		}
		printf("\n");
	}

	/*
	 * Convert fh to fd
	 */
	if ((fd = syssgi(SGI_NFSCNVT, a->lck.fh.n_bytes, O_RDWR)) == -1) {
		syslog(LOG_ERR, "get_fd: unable to cvt fh: %m");
		if (errno == ESTALE)
			return (-1);
		return (-2);
	}

	f = &fd_table[fd];
	obj_copy(&f->fh, &a->lck.fh);
	f->fd = fd;
	if (lastfd < fd)
		lastfd = fd;
	f->gc = 0;
	(void) fstat(fd, &f->stat);	/* debugging */
	if (isnewp)
		*isnewp = 1;
	used_fd++;

	if (debug) {
		printf(" new %d inum %d/%x\n",
			f->fd,
			f->stat.st_ino, f->stat.st_dev);
		if (debug > 1)
			print_fdtable();
	}
	return (fd);
}

static filock_t *
get_filock_list(int fd)
{
	return(fd_table[fd].filocks);
}

/*
 * Free the state associated with a file descriptor if we do not hold
 * any locks on it.
 */
void
free_fd(int fd)
{
	struct fdtable *f = &fd_table[fd];

	assert(fd > 0 && fd <= lastfd);
	assert(f->fd);
	if (f->nblocked == 0) {
		if (isunlocked(fd)) {
			clear_fd(fd);
			free_fdt(f);
		} else {
			/*
			 * XXX
			 *  Since we don't keep of locks held on the fd, and
			 *  there's no fcntl to ask the kernel for that info,
			 *  try to free it from a timer.
			 */
			f->gc = 1;
			start_timer(TIMER_FDGC);
		}
	}
}


static void
printsysid(FILE *f)
{
	int i;
	struct hostent *hp;
	u_long	lastip = -1;

	for (i = 0; i < max; i++) {
		if (id[i].ipaddr != INADDR_ANY) {
			int j;

			if (!hp || id[i].ipaddr != lastip)
			    hp = gethostbyaddr(&id[i].ipaddr, 4, AF_INET);

			fprintf(f, "sysid %d = %d, %s (%s)\n", 
			    i+1, id[i].pid, 
			    inet_ntoa(*(struct in_addr *)&id[i].ipaddr),
			    (hp != NULL) ? hp->h_name : "?");
			lastip = id[i].ipaddr;

			if (id[i].nfd != 0) {
			    fprintf(f, " fd:");
			    for (j=0; j < id[i].nfd; j++)
				    fprintf(f, " %d", id[i].fd[j]);
			    fprintf(f, "\n");
			}
		}
	}
}

void
print_sysidtable(void)
{
	static FILE *f = NULL;
	if (f == NULL)
		f = fopen("/dev/console", "w");
	if (f)
		printsysid(f);
}

void
dumpsysid(FILE *f)
{
	printsysid(f);
}

/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * All record lock lists (referenced by a pointer in the vnode) are
 * ordered by starting position relative to the beginning of the file.
 * 
 * In this file the name "l_end" is a macro and is used in place of
 * "l_len" because the end, not the length, of the record lock is
 * stored internally.
 */

#undef SAMEOWNER
#define SAMEOWNER(a, b)	(((a)->l_pid == (b)->l_pid) && \
			 ((a)->l_sysid == (b)->l_sysid))

/*
 * Insert lock (lckdat) after given lock (fl); If fl is NULL place the
 * new lock at the beginning of the list and update the head ptr to
 * list which is stored at the address given by lck_list. 
 */
static struct filock *
insflck(struct filock **lck_list, struct flock *lckdat, struct filock *fl)
{
	register struct filock *new;

	if ((new = xmalloc(sizeof *new)) == NULL) {
		return NULL;
	}
	bzero( new, sizeof *new );

	new->set = *lckdat;
	new->stat.wakeflg = 0;
	if (fl == NULL) {
		new->next = *lck_list;
		if (new->next != NULL)
			new->next->prev = new;
		*lck_list = new;
	} else {
		new->next = fl->next;
		if (fl->next != NULL)
			fl->next->prev = new;
		fl->next = new;
	}
	new->prev = fl;

	return new;
}

/*
 * Delete lock (fl) from the record lock list. If fl is the first lock
 * in the list, remove it and update the head ptr to the list which is
 * stored at the address given by lck_list.
 */
static void
delflck(struct filock  **lck_list, struct filock  *fl)
{
	assert(isonlist(*lck_list, fl));
	if (fl->prev != NULL)
		fl->prev->next = fl->next;
	else
		*lck_list = fl->next;
	if (fl->next != NULL)
		fl->next->prev = fl->prev;
	free(fl);
}

/*
 * regflck sets the type of span of this (un)lock relative to the specified
 * already existing locked section.
 * There are five regions:
 *
 *  S_BEFORE        S_START         S_MIDDLE         S_END          S_AFTER
 *     010            020             030             040             050
 *  E_BEFORE        E_START         E_MIDDLE         E_END          E_AFTER
 *      01             02              03              04              05
 * 			|-------------------------------|
 *
 * relative to the already locked section.  The type is two octal digits,
 * the 8's digit is the start type and the 1's digit is the end type.
 */
static int
regflck(struct flock *ld, struct filock *flp)
{
	register int regntype;

	if (ld->l_start > flp->set.l_start) {
		if (ld->l_start-1 == flp->set.l_end)
			return S_END|E_AFTER;
		if (ld->l_start > flp->set.l_end)
			return S_AFTER|E_AFTER;
		regntype = S_MIDDLE;
	} else if (ld->l_start == flp->set.l_start)
		regntype = S_START;
	else
		regntype = S_BEFORE;

	if (ld->l_end < flp->set.l_end) {
		if (ld->l_end == flp->set.l_start-1)
			regntype |= E_START;
		else if (ld->l_end < flp->set.l_start)
			regntype |= E_BEFORE;
		else
			regntype |= E_MIDDLE;
	} else if (ld->l_end == flp->set.l_end)
		regntype |= E_END;
	else
		regntype |= E_AFTER;

	return  regntype;
}

/*
 * Adjust file lock from region specified by 'ld', in the record
 * lock list indicated by the head ptr stored at the address given
 * by lck_list. Start updates at the lock given by 'insrtp'.  It is 
 * assumed the list is ordered on starting position, relative to 
 * the beginning of the file, and no updating is required on any
 * locks in the list previous to the one pointed to by insrtp.
 * Insrtp is a result from the routine blocked().  Flckadj() scans
 * the list looking for locks owned by the process requesting the
 * new (un)lock :
 *
 * 	- If the new record (un)lock overlays an existing lock of
 * 	  a different type, the region overlaid is released.
 *
 * 	- If the new record (un)lock overlays or adjoins an exist-
 * 	  ing lock of the same type, the existing lock is deleted
 * 	  and its region is coalesced into the new (un)lock.
 *
 * When the list is sufficiently scanned and the new lock is not 
 * an unlock, the new lock is inserted into the appropriate
 * position in the list.
 */
static int
flckadj(struct filock **lck_list,
	register struct filock *insrtp,
	struct flock *ld)
{
	register struct	filock	*flp, *nflp;
	int regtyp;
	struct id *idp = &id[ld->l_sysid - 1];

	assert(idp->filocks >= 0);

	nflp = (insrtp == NULL) ? *lck_list : insrtp;

	while (flp = nflp) {
		nflp = flp->next;
		if( SAMEOWNER(&(flp->set), ld) ) {

			/* Release already locked region if necessary */

			switch (regtyp = regflck(ld, flp)) {
			case S_BEFORE|E_BEFORE:
				nflp = NULL;
				break;
			case S_BEFORE|E_START:
				if (ld->l_type == flp->set.l_type) {
					ld->l_end = flp->set.l_end;
					delflck(lck_list, flp);
					idp->filocks--;
				}
				nflp = NULL;
				break;
			case S_START|E_END:
				/*
				 * Don't bother if this is in the middle of
				 * an already similarly set section.
				 */
				if (ld->l_type == flp->set.l_type)
					return 0;
			case S_START|E_AFTER:
				insrtp = flp->prev;
				delflck(lck_list, flp);
				idp->filocks--;
				break;
			case S_BEFORE|E_END:
				if (ld->l_type == flp->set.l_type)
					nflp = NULL;
			case S_BEFORE|E_AFTER:
				delflck(lck_list, flp);
				idp->filocks--;
				break;
			case S_START|E_MIDDLE:
				insrtp = flp->prev;
			case S_MIDDLE|E_MIDDLE:
				/*
				 * Don't bother if this is in the middle of
				 * an already similarly set section.
				 */
				if (ld->l_type == flp->set.l_type)
					return 0;
			case S_BEFORE|E_MIDDLE:
				if (ld->l_type == flp->set.l_type)
					ld->l_end = flp->set.l_end;
				else {
					/* setup piece after end of (un)lock */
					register struct	filock *tdi, *tdp;
					struct flock td;

					td = flp->set;
					td.l_start = ld->l_end + 1;
					tdp = tdi = flp;
					do {
						if (tdp->set.l_start < td.l_start)
							tdi = tdp;
						else
							break;
					} while (tdp = tdp->next);
					if (insflck(lck_list, &td, tdi) == NULL) {
						return ENOLCK;
					} else {
						idp->filocks++;
					}
				}
				if (regtyp == (S_MIDDLE|E_MIDDLE)) {
					/* setup piece before (un)lock */
					flp->set.l_end = ld->l_start - 1;
					insrtp = flp;
				} else {
					delflck(lck_list, flp);
					idp->filocks--;
				}
				nflp = NULL;
				break;
			case S_MIDDLE|E_END:
				/*
				 * Don't bother if this is in the middle of
				 * an already similarly set section.
				 */
				if (ld->l_type == flp->set.l_type)
					return 0;
				flp->set.l_end = ld->l_start - 1;
				insrtp = flp;
				break;
			case S_MIDDLE|E_AFTER:
				if (ld->l_type == flp->set.l_type) {
					ld->l_start = flp->set.l_start;
					insrtp = flp->prev;
					delflck(lck_list, flp);
					idp->filocks--;
				} else {
					flp->set.l_end = ld->l_start - 1;
					insrtp = flp;
				}
				break;
			case S_END|E_AFTER:
				if (ld->l_type == flp->set.l_type) {
					ld->l_start = flp->set.l_start;
					insrtp = flp->prev;
					delflck(lck_list, flp);
					idp->filocks--;
				}
				break;
			case S_AFTER|E_AFTER:
				insrtp = flp;
				break;
			}
		}
	}

	if (ld->l_type != F_UNLCK) {
		if (flp = insrtp) {
			do {
				if (flp->set.l_start < ld->l_start)
					insrtp = flp;
				else
					break;
			} while (flp = flp->next);
		}
		if (insflck(lck_list, ld, insrtp) == NULL) {
			return ENOLCK;
		} else {
			idp->filocks++;
		}
	}

	assert(idp->filocks >= 0);

	return 0;
}

/*
 * blocked() checks whether a new lock (lckdat) would be
 * blocked by a previously set lock owned by another process.
 * Insrt is set to point to the lock where lock list updating
 * should begin to place the new lock.
 */ 
static struct filock *
blocked(struct filock *flp,
	struct flock  *lckdat,
	struct filock **insrt)
{
	register struct filock *f;

	*insrt = NULL;
	for (f = flp; f != NULL; f = f->next) {
		if (f->set.l_start < lckdat->l_start)
			*insrt = f;
		else
			break;
		if( SAMEOWNER(&(f->set), lckdat) ) {
			if ((lckdat->l_start-1) <= f->set.l_end)
				break;
		} else if (lckdat->l_start <= f->set.l_end
		  && (f->set.l_type == F_WRLCK
		    || (f->set.l_type == F_RDLCK && lckdat->l_type == F_WRLCK)))
			return f;
	}

	for (; f != NULL; f = f->next) {
		if (lckdat->l_end < f->set.l_start)
			break;
		if (lckdat->l_start <= f->set.l_end
		  && ( !SAMEOWNER(&(f->set), lckdat) )
		  && (f->set.l_type == F_WRLCK
		    || (f->set.l_type == F_RDLCK
		            && lckdat->l_type == F_WRLCK)))
			return f;
	}

	return NULL;
}

static int
isonlist(struct filock  *lck_list, struct filock  *fl)
{
	struct filock *fi;
	for (fi = lck_list; fi; fi = fi->next)
		if (fi == fl)
			return 1;
	if (debug) {
		printf("lock type %d, start %d, len %d not on list\n", fl->set.l_type,
			(int)fl->set.l_start, (int)fl->set.l_len );
	}
	return 0;
}

/*
 * add/remove a lock record to the specified file for the lock described by
 * flp
 * coalesce lock entries where possible and keep the list sorted
 * This mimics what the kernel does.
 * The code for reclock was extracted verbatim from the kernel.
 */
int
record_lock(int fd, struct reclock *reclk, struct flock *lckdat)
{
	struct	filock *found, *insrt = NULL, *new;

	errno = 0;

	assert(lckdat->l_whence == 0);

	/* Convert l_len to be the end of the rec lock l_end */
	if (lckdat->l_len < 0) {
		errno = EINVAL;
		return -1;
	}
	if (lckdat->l_len == 0)
		lckdat->l_end = MAXEND;
	else
		lckdat->l_end += (lckdat->l_start - 1);

	/* check for arithmetic overflow */
	if (lckdat->l_start > lckdat->l_end) {
		errno = EINVAL;
		return -1;
	}

	switch (lckdat->l_type) {
		case F_RDLCK:
		case F_WRLCK:
			if ((found = blocked(fd_table[fd].filocks, lckdat, &insrt))
				== NULL) {
					reclk->blk.flp = NULL;
					errno = flckadj(&fd_table[fd].filocks, insrt, lckdat);
			} else if (!(reclk->blk.flp = insflck(&fd_table[fd].blocked,
				lckdat, NULL))) {
					errno = ENOLCK;
			}
			break;
		case F_UNLCK:
			/* removing a file record lock */
			errno = flckadj(&fd_table[fd].filocks, fd_table[fd].filocks,
				lckdat);
			break;
		default:
			/* invalid lock type */
			errno = EINVAL;
			break;
	}


	/* Restore l_len */
	if (lckdat->l_end == MAXEND)
		lckdat->l_len = 0;
	else
		lckdat->l_len -= (lckdat->l_start-1);

	return( (errno == 0) ? 0 : -1 );
}
