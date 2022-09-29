#undef DEBUG
/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986-1993 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/*
 * 	usinit.c
 *
 *	User level locks and semaphores are based on a mapped file
 *	for sharing. The mapped file contains a header for control
 *	of the allocation as well as actually providing the area from
 *	which all the structures for locks and semas are allocated.
 *	File locks are used to control the access to the mapped file
 *	so we can maintain sanity.
 *	This is where all the initialization of lock and semaphore
 *	header structures and map file mapping happens.
 *
 *	As each process calls usinit they
 *	are added to tid map - and lock a byte in the header
 *	If they die, then the file lock is removed by the kernel and we
 *	can reuse the tid slot. Also this means that even if the process
 *	that did the original usinit dies, as long as there are ANY other
 *	processes with a file lock, another process joining the party will
 *	find an active arena and join, not re-initalize.
 */
#ident "$Revision: 1.66 $ $Author: tee $"

#ifdef __STDC__
	#pragma weak usinit = _usinit
	#pragma weak usadd = _usadd
	#pragma weak usdetach = __usdetach
#endif

#include "synonyms.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#ifdef DEBUG
#include <sys/time.h>
#endif
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <bstring.h>
#include <unistd.h>
#include "mplib.h"
#include "shlib.h"
#include "ulocks.h"
#include "malloc.h"
#include "stdarg.h"
#include "us.h"
#include <sys/usioctl.h>
#include "mp_extern.h"
#include "mallint.h"

#ifdef DEBUG
int _uerror = 1;	/* variable to print errors */
#else
int _uerror = 0;	/* variable to print errors */
#endif
int _utrace _INITBSS;	/* variable to trace adding tids, initialization... */
int _utracefd = 2;	/* fd to write tracing messages to */
int _us_systype _INITBSS;	/* are we on R3K, R4K, UP or MP */

#define TIDHASH(h, p)	((p) & ((h)->u_tidmaphashmask))

/*
 * To conserve file descriptors we keep some local info - for sproc
 * processes this is of course shared amoung all members of the share group
 * So if the share group is sharing file descriptors the local li_fd
 * can be used.
 */
struct _uslocalinfo {
	int	li_fd;			/* arena fd */
	ushdr_t	*li_haddr;		/* arena address */
	struct _uslocalinfo *li_next;
	char	*li_name;		/* arena file name */
};
static struct _uslocalinfo *_usli = NULL;

/*
 * local prototypes
 */
static void clean_tidmap(ushdr_t *, pid_t, int);
static int any_valid_tids(int, int);
static int hdr_init(ushdr_t *, int, size_t, int);
static int zapfile(int fd, ssize_t sz);
#ifdef DEBUG
static void _usdumpheader(ushdr_t *, char *);
#endif


/*
 * clean_tidmap - clean up unused entries in tid map
 *
 * It scans the tidmap, to see if processes still exist (have a file lock)
 * If not locked, it sets it to -1. In this way the tidmap is cleaned
 * out with each new person joining the game.
 * It MUST be single threaded via the access file lock
 * AND should have signals blocked
 * Since one cannot tell if the current process has a file lock, if the calling
 * pid id found in the tidmap, we assume that it has a lock
 *
 */
static void
clean_tidmap(ushdr_t *header, pid_t pid, int lfd)
{
	register short curtid, oj, j, h;
	register pid_t opid, ownerpid;

	/* clean up tidmap */
	for (curtid = 0; curtid < header->u_maxtidusers; curtid++) {
		/*
		 * we cannot do a _usfgetlock on ourselves since that
		 * will always succeed
		 */
		opid = header->u_tidmap[curtid].t_pid;
		if (opid == pid)
			continue;

		/*
		 * if tracing do extra error checking - make sure
		 * that file lock and tidmap agree
		 * otherwise, if the tidmap says noone there, ignore
		 * entry
		 */
		if (!_utrace && opid == -1)
			continue;
		if ((ownerpid = _usfgetlock(curtid + _USACCESS_LCK + 1, lfd)) == 0) {
			/* not locked - recycle entry */
			if (opid == -1)
				/* this only occurs when tracing */
				continue;
			h = (short)TIDHASH(header, opid);
			/* j is first tid on list */
			j = header->u_tidmaphash[h];
			if (j == curtid) {
				/* curtid at front of list */
				header->u_tidmaphash[h] =
						header->u_tidmap[j].t_next;
			} else {
				do {
					oj = j;
					j = header->u_tidmap[j].t_next;
					if (j == curtid) {
						header->u_tidmap[oj].t_next =
						    header->u_tidmap[j].t_next;
						break;
					}
				} while (j != -1);
			}

			if (opid >= 0 && _utrace) {
				_uprint(0,
				"TRACE: Process %d deleted as tid %d from arena %s (file lock gone) by process %d",
					opid, curtid, header->u_mfile, pid);
			}
			header->u_tidmap[curtid].t_pid = -1;
			header->u_tidmap[curtid].t_next = -1;
			header->u_lasttidzap = get_pid();
		} else if (ownerpid < 0) {
			if (_uerror)
				_uprint(1, "ERROR:Attempt by %d to clean tids in arena %s failed",
						pid, header->u_mfile);
			/* if it failed once, forget it we're in bad shape */
			return;
		} else {
			/*
			 * there is a lock - see that it matches who
			 * we think owns the lock
			 */
			if (_uerror && ownerpid != opid) {
				_uprint(0, "ERROR:Process %d in slot %d but pid %d owns lock! in arena %s by process %d",
						opid, curtid, ownerpid,
						header->u_mfile, get_pid());
			}
		}
	}
}

/* any_valid_tids
 *
 * This scans the tidmap, to see if any tid is in use. 
 *
 *	returns: -1, if error
 *		 pid, pid of first valid tid
 *		 0, if none in use
 */
static int
any_valid_tids(int lfd, int maxusers)
{
	short curtid;
	int rv;

	/* find first tid in use */
	for (curtid = 0; curtid < maxusers; curtid++) {
		if ((rv = _usfgetlock(curtid + _USACCESS_LCK + 1, lfd)) != 0) {
			return(rv);
		}
	}
	return(0);
}

/*
 * _usget_pid - return pid for given tid
 */
pid_t
_usget_pid(ushdr_t *header, short tid)
{
	return(header->u_tidmap[tid].t_pid);
}

/*
 * _usgettid
 *	Gets the tid entry for the calling pid.
 *	Returns:  tid,  when successful
 *		   NULL, if caller doesn't have a tid entry.
 * Note that there is NO race with people adding new tids to the list
 *	( they are added at the beginning)
 * But that with people removing things, we could miss our entry
 * So if we can't find ourselves, we lock the header access file lock
 * and look again
 */
short
_usgettid(ushdr_t *header)
{
	register short curtid;
	register pid_t pid = get_pid();
	register tid_entry_t *tm;
	struct _uslocalinfo *lp;
	int lfd = -1;
	sigset_t old;

	curtid = header->u_tidmaphash[TIDHASH(header, pid)];
	tm = header->u_tidmap;
	while (curtid != -1) {
		if (tm[curtid].t_pid == pid)
			return(curtid);
		curtid = tm[curtid].t_next;
	}

	/*
	 * Now grab lock and try the slow way
	 */

	/* find fd for arena */
	for (lp = _usli; lp; lp = lp->li_next) {
		if (lp->li_haddr == header) {
			lfd = lp->li_fd;
			break;
		}
	}
	if (lfd == -1) {
		if (_uerror)
			_uprint(0, "ERROR:Process %d arena 0x%x never initialized (_usgettid)",
				pid, header);
		return(-1);
	}
	/* spin on access lock */
	__usblockallsigs(&old);
	if (_usfsplock(_USACCESS_LCK, lfd) == -1) {
		__usunblockallsigs(&old);
		if (_uerror)
			_uprint(1, "ERROR:Process %d attempt to grab header file lock for arena %s failed",
					pid, header->u_mfile);
		return(-1);
	}

	/*
	 * look again
	 */

	if (MTLIB_ACTIVE()) {
		/*
		 * Slow table scan for pthreads
		 *
		 * In the common case this path is not slow at all, as the tid
		 * we're looking for is usually the first entry in the table.
		 *
		 * We need this path for pthreads to avoid lookup races, since
		 * the ACCESS file lock cannot be relied upon, and the hash
		 * list index may temporarily fall out of sync (see #609234).
		 */
		for (curtid = 0; curtid < header->u_maxtidusers; curtid++) {
			if (header->u_tidmap[curtid].t_pid == pid)
				goto done;
		}
		curtid = -1;
	} else {
		/* quick hash list scan for non-pthreads */
		curtid = header->u_tidmaphash[TIDHASH(header, pid)];
		tm = header->u_tidmap;
		while (curtid != -1) {
			if (tm[curtid].t_pid == pid)
				goto done;
			curtid = tm[curtid].t_next;
		}
	}

 done:
	_usfunsplock(_USACCESS_LCK, lfd);
	__usunblockallsigs(&old);

	/* returns -1 if no tid was found */
	return (curtid);
}

/*
 * _usclean_tidmap - external interface to request that the tidmap be checked
 * Returns 1 if there are any available tids left, -1 on error
 */
int
_usclean_tidmap(ushdr_t *header)
{
	register short curtid;
	register pid_t pid = get_pid();
	struct _uslocalinfo *lp;
	sigset_t old;
	int lfd = -1;

	/* find fd for arena */
	for (lp = _usli; lp; lp = lp->li_next) {
		if (lp->li_haddr == header) {
			lfd = lp->li_fd;
			break;
		}
	}
	if (lfd == -1) {
		if (_uerror)
			_uprint(0, "ERROR:Process %d arena @0x%x never initialized (_usclean_tidmap)",
				pid, header);
		setoserror(ESRCH);
		return(-1);
	}
	/*
	 * We really can't afford to die while updating the tidmap
	 * and we don't want to recurse (file locks will let us grab it again)
	 * so block all signals
	 */
	__usblockallsigs(&old);

	/* spin on access lock */
	if (_usfsplock(_USACCESS_LCK, lfd) == -1) {
		__usunblockallsigs(&old);
		if (_uerror)
			_uprint(1, "ERROR:Process %d attempt to check for tids in arena %s failed",
					pid, header->u_mfile);
		return(-1);
	}

	/* clean out any old musty tids/pids */
	clean_tidmap(header, pid, lfd);

	/* look for any avail tids */
	for (curtid = 0; curtid < header->u_maxtidusers; curtid++) {
		if (header->u_tidmap[curtid].t_pid == -1) {
			_usfunsplock(_USACCESS_LCK, lfd);
			__usunblockallsigs(&old);
			return(1);
		}
	}

	_usfunsplock(_USACCESS_LCK, lfd);
	__usunblockallsigs(&old);

	/* ELSE - no tids left */
	setoserror(ENOSPC);
	return(-1);
}

/*
 * _usany_tids_left - return 0 if any tids left in named arena
 */
int
_usany_tids_left(ushdr_t *header)
{
	register short curtid;
	int rv;

	/*
	 * look for any avail tids
	 * _usclean_tidmap is VERY expensive - do a quick and dirty
	 * job here
	 */
	for (curtid = 0; curtid < header->u_maxtidusers; curtid++) {
		if (header->u_tidmap[curtid].t_pid == -1)
			return(0);
	}

	rv = _usclean_tidmap(header);
	if (rv == -1 && oserror() == ENOSPC && _uerror)
		_uprint(0, "ERROR:Process %d no tids left in arena %s @0x%x. Too many users? (max = %d)",
				get_pid(), header->u_mfile,
				header->u_mapaddr,
				header->u_maxtidusers);
	return(rv);
}

/*
 * _usaddme
 *	Add pid to the tidmap at the next available tid.
 *	Lock the associated file lock, increment the tiduser count.	
 * Processes can be added either by calling usinit directly, OR by
 *	implicitly using a semaphore/lock
 * The advantage of being registered is that on restart, the usinit code
 * attempts to figure out whether or not anyone is still using the arena
 * by looking for locked sections of the mapped file - if it finds NONE
 * it then re-inits the header area - if a process that did NOT register
 * is still around doing things, disaster might strike
 *
 * Somehow, the process must have access to the header - either via
 * a usinit call or by virtue of having sproc'd or by being a forked child
 * We scan the local info list looking for the file descriptor to use to
 * lock.
 * We must be sure that the file stay open, otherwise our file lock will go
 * away.
 * This code is single threaded by having the access lock
 *
 * Caller should have blocked all signals.
 */
static short
_usaddme(ushdr_t *header, pid_t pid, int lfd)
{
	short curtid, h;
	int err;
	struct _uslocalinfo *lp;

	if (lfd < 0) {
		for (lp = _usli; lp; lp = lp->li_next) {
			if (lp->li_haddr == header) {
				lfd = lp->li_fd;
				break;
			}
		}
		if (lfd == -1) {
			err = ESRCH;
			goto bad;
		}

		/* spin on access lock */
		if (_usfsplock(_USACCESS_LCK, lfd) == -1) {
			err = oserror();
			goto bad;
		}
	}

	/* clean up any old tids - this will remove them from hash, etc. */
	clean_tidmap(header, pid, lfd);

	/* find first available tid */
	for (curtid = 0; curtid < header->u_maxtidusers; curtid++) {
		if (header->u_tidmap[curtid].t_pid == -1) {
			if ((_usfsplock(curtid + _USACCESS_LCK + 1, lfd))
								== -1) {
				err = oserror();
				_usfunsplock(_USACCESS_LCK, lfd);
				goto bad;
			}

			/* add to tid hash */
			h = (short)TIDHASH(header, pid);
			header->u_tidmap[curtid].t_pid = pid;
			header->u_tidmap[curtid].t_next = header->u_tidmaphash[h];
			/* atomically add to list */
			header->u_tidmaphash[h] = curtid;

			if (_utrace)
				_uprint(0,
				"TRACE: Process %d added as tid %d arena %s at 0x%x",
					header->u_tidmap[curtid].t_pid,
					curtid,
					header->u_mfile,
					header->u_mapaddr);
			_usfunsplock(_USACCESS_LCK, lfd);
			return(curtid);
		}
	}
	/* ELSE - no tids left */
	_usfunsplock(_USACCESS_LCK, lfd);
	err = ENOSPC;
	if (_uerror)
		_uprint(0, "ERROR:Process %d no tids left in arena %s. Too many users? (max = %d)",
				get_pid(), header->u_mfile,
				header->u_maxtidusers);
	/* drop into bad: */

bad:
	if (_uerror)
		_uprint(1, "ERROR:Process %d attempt to add to arena %s @0x%x failed",
				pid, header->u_mfile, header->u_mapaddr);
	setoserror(err);
	return(-1);
}

/*
 * _usinit_mapfile
 * Map in the shared arena
 */
/* ARGSUSED1 */
ushdr_t *  
_usinit_mapfile(
 int mfd,		/* mapped file descriptor */
 int shflags,		/* flags to indicate if using strictly shared */
 off_t size,		/* length of arena in bytes */
 int ltype,		/* lock type */
 int maxu,		/* maximum # of users */
 int ag,		/* should autogrow?? */
 int ar,		/* should autoresv?? */
 void *aaddr)		/* attach address */
{
 	register ushdr_t *header;	/* ptr to magic header */
	register int mapflags;
	struct stat sb;
	int fixedaddr = 0;
	size_t growsize = 0;

	fstat(mfd, &sb);
	if (S_ISREG(sb.st_mode)) {
		if (sb.st_size > size) {
			/* truncate to appr length - it may be too large */
			ftruncate(mfd, size);
			sb.st_size = size;
		}
		/*
		 * zero current file - don't use mmap - that could
		 * use up alot of pages if the file is really big
		 */
		if (zapfile(mfd, sb.st_size)) {
			if (_uerror)
				_uprint(1, "usinit:ERROR:unable to zero arena file");
			return(NULL);
		}

		/*
		 * if user wants pre-allocated file - grow it now
		 * Even if user doesn't want the file grown now -
		 * get it at least
		 * large enough so that poking the header and arena creation
		 * are guaranteed not to fail (because we run out of file system
		 * space) and therefore die with a SEGV.
		 */
		if (!ag)
			growsize = size;
		else if (sb.st_size < (sizeof(ushdr_t) + 4096))
			growsize = sizeof(ushdr_t) + 4096;

		if (growsize) {
			struct flock fl;
			fl.l_whence = SEEK_SET;
			fl.l_start = (off_t)growsize;
			fl.l_len = 0;
			if (fcntl(mfd, F_ALLOCSP, &fl) != 0) {
				if (_uerror)
					_uprint(1, "usinit:ERROR:unable to grow arena file to %d bytes", growsize);
				return(NULL);
			}
		}
	}

	mapflags = MAP_SHARED;
	if (ag)
		mapflags |= MAP_AUTOGROW;
	if (ar)
		/*
		 * Note that this only really does anything for /dev/zero
		 * mappings - since we always MAP_SHARED, the only mapping
		 * that needs virtual swap space is /dev/zero
		 */
		mapflags |= MAP_AUTORESRV;
	if (aaddr != (void *)~0L) {
		/*
		 * user wants particular value - lets not use MAP_FIXED since
		 * that will replace mappings.
		 * Instead, we use the 'hint' feature - if we don't get the
		 * mapping we want, we'll unmap what we got.
		 */
		fixedaddr = 1;
	} else
		aaddr = 0x0;
	if ((header = (ushdr_t *)mmap(aaddr, (size_t)size, PROT_READ|PROT_WRITE,
				mapflags, mfd, 0)) == (ushdr_t *)MAP_FAILED) {
		if (_uerror)
			_uprint(1, "usinit:ERROR:Process %d mmap failed @0x%x",
						get_pid(), aaddr);
		return(NULL);
	}
	if (fixedaddr && header != (ushdr_t *)aaddr) {
		munmap(header, size);
		if (_uerror)
			_uprint(0,
			"usinit:ERROR:Process %d virtual address 0x%x already in use",
						get_pid(), aaddr);
		setoserror(EBUSY);
		return(NULL);
	}
	header->u_mapaddr = (char *)header;
	header->u_locktype = ltype;
	header->u_shflags = shflags;
	header->u_version = _USVERSION;
	header->u_lastinit = get_pid();
	header->u_magic = _USMAGIC;
	header->u_maxtidusers = maxu;
	header->u_mmapsize = size;

	/*
	 * Do malloc, tidmap, lock initialization
	 */
	if (!hdr_init(header, maxu, size, ag+ar)) {
		if (_uerror)
			_uprint(1, "usinit:ERROR:failed to initialize header");
		munmap((char *)header, (size_t) size);
		return((ushdr_t *) NULL);
	}

	return(header);
}

/*
 * Initialize things needed to share locks and semas between processes 
 *
 *	returns: a magic cookie to the shared mapped file (ushdr_t *)
 *		 0 (NULL) if not able to initialize.
 */
usptr_t *
usinit(const char *mapfile)
{
	char			*utrfile;	/* utrace file name */
	static int		tryufd = 1;
	ushdr_t			*us_ptr;
	int			mfd;		/* mapped file fd */
	char			usbuf[4096];
	ushdr_t			*tmp_ushdr = (ushdr_t *)usbuf;	
	pid_t			mypid = get_pid();
	int			err;		/* save errno */
	int			created = 0;	/* if created file */
	size_t			mapsize;	/* size of mapped area */
	struct _uslocalinfo	*nli;
	struct _uslocalinfo 	*lp;
#ifdef DEBUG
	int			i;
#endif
	int			shflags = _us_arenatype;
	int			ltype = _us_locktype;
	int			rlfd = -1;
	int			lfd;	/* locking file descriptor */
	int			isdevzero = 0;
	sigset_t		old;

	if (getenv("USTRACE")) {
		shflags |= US_TRACING;
		_utrace = 1;
		_uerror = 1;
	}
	if (getenv("USERROR"))
		_uerror = 1;
	if (tryufd) {
		tryufd = 0;
		if (utrfile = getenv("USTRACEFILE")) {
			int nfd;
			if ((nfd = open(utrfile, O_RDWR|O_CREAT|O_APPEND, 0666)) >= 0) {
				_utracefd = nfd;
			}
		}
	}

	/* Determine system type if we haven't done that already */
	if (_us_systype == 0) {
		if ((_us_systype = _getsystype()) == 0) {
			setoserror(EINVAL);
			return(NULL);
		}
	}

	if (strcmp(mapfile, "/dev/zero") == 0)
		isdevzero = 1;

	/*
	 * First look if we already have this arena mapped (either cause we're
	 * part of the share group or forked) and started.
	 * If we find anything it must have been set up before we
	 * were sproc'ed/fork'ed so that any fd is valid - even if we're not
	 * sharing fds
	 *
	 * Note that this strcmp'ing of file names has some bad side effects:
	 * 1) if one does a usinit of a temp file name, unlinkes the temp file
	 *	and tries again (with the same temp file name) - even though
	 *	clearly the files are different, this code will attach the
	 *	caller to the same arena..
	 * 2) /dev/zero - clearly each call to usinit("/dev/zero") needs to
	 *	create a new arena. But this botches when apps follow our
	 *	'old' advice to call usinit() from the slaves!
	 *
	 * For these reasons there is now a usadd - that basically takes the
	 *	place of this stuff. But for compatibility we still need to
	 *	provide this. (perhaps we should put in a new usinit2() 
	 *	interface that solves some of these problems.
	 * For /dev/zero (which never used to work) we never do this code.
	 */
	if (!isdevzero) {
		for (lp = _usli; lp; lp = lp->li_next) {
			if (strcmp(lp->li_name, mapfile) != 0)
				continue;

			us_ptr = (ushdr_t *)lp->li_haddr;
			if (us_ptr->u_magic == (long) _USMAGIC &&
			    us_ptr->u_version == _USVERSION) {
				/* already mapped!! */
				if (_utrace)
					_uprint(0,
					"TRACE: Process %d already mapped to arena %s",
					get_pid(), mapfile);

				/*
				 * get us a tid - this is a mechanism for
				 * FORCING a tid since otherwise a sproc
				 * process may not ever get one - see comment
				 * in usaddme
				 */
				if (usadd((usptr_t *)us_ptr))
					return NULL;
				return((usptr_t *)us_ptr);
			}
			/* arena not active... */
			break;
		}
	}

	/*
	 * /dev/zero is an interesting special case - it permits the
	 * arena to consume swap space rather than file system space.
	 * The only difficulty is the file locking - clearly we can't
	 * have everyone locking /dev/zero! So if the file is /dev/zero
	 * we must create a special 'locking' file. This file can be
	 * 0 length and can always be unlinked since by definition
	 * if a user uses /dev/zero the arena is applicable to
	 * related processes only
	 */
	if (strcmp(mapfile, "/dev/zero") == 0) {
		char *lfname;
		lfname = tempnam(NULL, "uslock");
		if ((rlfd = open(lfname, O_CREAT|O_RDWR, 0600)) == -1) {
			err = oserror();
			if (_uerror)
				_uprint(1, "usinit:ERROR:Process %d open on %s failed",
							get_pid(), lfname);
			setoserror(err);
			free(lfname);
			return(NULL);
		}
		unlink(lfname);
		free(lfname);
		fcntl(rlfd, F_SETFD, FD_CLOEXEC);
	}

reacquire:
	/* open or creat the file to be mapped and shared */
	if ((mfd = open(mapfile, O_RDWR)) == -1) {
		if ((mfd = open(mapfile, O_CREAT|O_RDWR, 0600)) == -1) {
			err = oserror();
			if (_uerror)
				_uprint(1, "usinit:ERROR:Process %d open on %s failed",
							get_pid(), mapfile);
			setoserror(err);
			return(NULL);
		}
		created++;
	}
	/*
	 * since we're mapping the file - it makes no sense to have it
	 * stay open across exec
	 */
	fcntl(mfd, F_SETFD, FD_CLOEXEC);

	/* decide which fd is the one to be used for locking */
	if (rlfd >= 0)
		lfd = rlfd;
	else
		lfd = mfd;

	/* spin on access lock */
	if ((_usfsplock(_USACCESS_LCK, lfd)) == -1) {
		err = oserror();
		if (_uerror)
			_uprint(1, "usinit:ERROR:Process %d file lock on %s failed",
							get_pid(), mapfile);
		goto errout;
	}

	/* read in header to get magic and tidmap, if there */
	bzero(tmp_ushdr, sizeof(usbuf));
#ifdef DEBUG
	i =
#endif
	read(mfd, tmp_ushdr, sizeof(usbuf));
#ifdef DEBUG
	if (i < 0)
		_uprint(1, "usinit:ERROR:Read of arena file failed");
	else if (_utrace) {
		if (i < sizeof(usbuf))
			_uprint(0, "TRACE: Short read of arena file %d", i);
		else
			_usdumpheader(tmp_ushdr, "File version");
	}
#endif

	/*
	 * IF there is a party (magic and something locked), join the party
	 */
	if ((tmp_ushdr->u_magic == (long)_USMAGIC) &&
			any_valid_tids(lfd, tmp_ushdr->u_maxtidusers)) {
		if (_utrace)
			_uprint(0, "TRACE: Process %d joining existing arena %s @ 0x%x",
							get_pid(), mapfile,
							tmp_ushdr->u_mapaddr);
		if (tmp_ushdr->u_version != _USVERSION) {
			if (_uerror)
				_uprint(0, "usinit:ERROR:Version Mismatch");
			err = EINVAL;
			goto errout;
		}

		/*
		 * find out size of current arena
		 * Since arenas are (sometimes) auto-grow we must trust
		 * the mapsize field in the header
		 * We don't MAP_FIXED, since that will replace mappings
		 * instead we use the 'hint' feature - it
		 * we don't get the address we requested, we'll
		 * unmap what we got and return an error
		 */
		mapsize = tmp_ushdr->u_mmapsize;
		if ((us_ptr = (ushdr_t *) mmap(tmp_ushdr->u_mapaddr, (size_t) mapsize,
				PROT_READ|PROT_WRITE,
				MAP_AUTOGROW|MAP_SHARED, mfd, 0)) ==
				(ushdr_t *)MAP_FAILED) {
			err = oserror();
			if (_uerror)
				_uprint(1, "usinit:ERROR:mmap failed @ 0x%x",
						tmp_ushdr->u_mapaddr);
			goto errout;
		}
		if (us_ptr != (ushdr_t *)tmp_ushdr->u_mapaddr) {
			munmap(us_ptr,  mapsize);
			if (_uerror)
				_uprint(0,
				"usinit:ERROR:Process %d virtual address 0x%x already in use",
					get_pid(), tmp_ushdr->u_mapaddr);
			err = EBUSY;
			goto errout;
		}
#ifdef DEBUG
		/* XXX this will be great when its finished and
		 * this process will use the saved logfile
		 */
		if (us_ptr->u_shflags & US_TRACING)
			_utrace = 1;
#endif
		_usr4klocks_init(us_ptr->u_locktype, _us_systype);
	} else {
		/* ELSE , the party is over, start one */
		if (_utrace)
			_uprint(0, "TRACE: Process %d creating new arena %s:locktype %d maxusers %d",
						get_pid(), mapfile,
						ltype, _us_maxusers);

		/*
		 * if the file exists, but we're not the owner then
		 * remove the file and start over. THis makes sure that
		 * new arenas are owned by the creator so that they
		 * can do CONF_CHMOD, etc..
		 * This is breaks the ability
		 * for a process to set up an arena, setting the permissions
		 * and let others use it even once it no longer has the arena
		 * open. But that feature is weak - in fact we are ignoring
		 * all of the previous info from the creator, why just keep
		 * the arena file owner?? ..
		 */
		if (!created) {
			struct stat sb;
			fstat(mfd, &sb);
			/* don't go unlinking anything but a regular file! */
			if (S_ISREG(sb.st_mode) && sb.st_uid != geteuid()) {
				if (_utrace)
					_uprint(0, "TRACE: Process %d arena not owned by you. Removing %s and starting over.", get_pid(), mapfile);
				if (unlink(mapfile) != 0) {
					if (_uerror)
						_uprint(1, "ERROR:usinit:not owner and cannot unlink %s", mapfile);
					err = errno;
					goto errout;
				}
				_usfunsplock(_USACCESS_LCK, lfd);
				close(mfd);
				goto reacquire;
			}
		}
		_usr4klocks_init(ltype, _us_systype);
		if (_utrace)
			_uprint(0,
			"TRACE: Process %d Initializing R4000 %s LOCKS",
				get_pid(),
				US_ISMP(_us_systype) ? "MP" : "UP");
		mapsize = _us_mapsize;
		if ((us_ptr = _usinit_mapfile(mfd, shflags, (off_t)mapsize,
				ltype, _us_maxusers, _us_autogrow,
				_us_autoresv, _us_attachaddr)) == NULL) {
			struct stat sb;

			err = oserror();
			fstat(mfd, &sb);
			/*
			 * if we were supposed to re-init, and that failed
			 * then remove the file no matter what
			 * don't go unlinking anything but a regular file!
			 */
			if (S_ISREG(sb.st_mode))
				created++;	/* force unlink */
			goto errout;
		}

		/* copy filename into header for related processes to addme */
		strcpy(us_ptr->u_mfile, mapfile);

		/* now msync file so read up above will find all this stuff
		 * For NFS files this is necessary since once
		 * file locking is turned on, there is no caching,
		 * thus the read at the top of usinit will not read through
		 * the cache
		 */
		msync((char *)us_ptr, sizeof(ushdr_t), MS_INVALIDATE);
	}

	/*
	 * unlink the file so it goes away when everyone dies 
	 * only works for related processes 
	 */
	if ((shflags & US_SHAREDONLY) && created)
		unlink(mapfile);

	/*
	 * add new local info
	 * This is constructed in LOCAL space
	 */
	nli = malloc(sizeof(*nli));
	nli->li_haddr = us_ptr;
	nli->li_fd = lfd;
	nli->li_name = malloc(strlen(mapfile) + 1);
	strcpy(nli->li_name, mapfile);
	/*
	 * NOTE: possible race with 2 members of share group doing
	 * usinit (of different arenas) at same time!
	 */
	nli->li_next = _usli;
	_usli = nli;

	/*
	 * now register caller as using the arena
	 * usaddme ALWAYS unlocks arena lock & prints on error
	 */
	/* XXX probably should be no signals for as long as we hold
	 * the ACCESS_LCK
	 */
	__usblockallsigs(&old);
	if (_usaddme(us_ptr, mypid, lfd) == -1) {
		err = oserror();
		__usunblockallsigs(&old);
		/* even this doesn't completely undo the damage! */
		_usli = nli->li_next;
		free(nli->li_name);
		free(nli);
		munmap(us_ptr, mapsize);
		goto errout;
	}
	__usunblockallsigs(&old);

	if (_utrace)
		_uprint(0, "TRACE: Process %d usinit succeeded for arena %s @ 0x%x", 
				get_pid(), mapfile, us_ptr);
	/* lfd is left OPEN so that file locks stick */
	if (lfd != mfd)
		close(mfd);
#ifdef DEBUG
	if (_utrace)
		_usdumpheader(us_ptr, "Mapped Arena");
#endif
	return((usptr_t *)us_ptr);

errout:
	if (created) unlink(mapfile);
	if (mfd >= 0) close(mfd);
	if (rlfd >= 0) close(rlfd);
	setoserror(err);
	return(NULL);
}

/*
 * usadd - add a process to an arena
 * This is useful for related processes who have access to an arena handle
 * and want to set up everything else
 */
int
usadd(usptr_t *u)
{
	register ushdr_t *header = (ushdr_t *)u;
	pid_t mypid = get_pid();
	int rv = 0, err;
	sigset_t old;

	/*
	 * We really don't want any signals here.
	 * 1) Holding the ACCESSLCK and getting a signal is bad - if the
	 *	signal handler exits, it can call _cleanup which will
	 *	attempt to p/v some semaphores.
	 * 2) want the gettid/addme operation to be atomic
	 */
	__usblockallsigs(&old);
	if (_usgettid(header) == -1) {
		if (oserror() == ENOLCK) {
			rv = -1;
			goto bad;
		}
		if (_usaddme(header, mypid, -1) == -1) {
			err = oserror();
			if (_uerror)
				_uprint(1,
			"usadd:ERROR:Process %d failed to add to arena @0x%x",
						mypid, header);
			setoserror(err);
			rv = -1;
			goto bad;
		}
	}
bad:
	__usunblockallsigs(&old);
	return(rv);
}

/*
 * similar to usadd except that:
 * 1) we assume that we have a tid and want a fast answer
 */
short
__usfastadd(ushdr_t *header)
{
	pid_t mypid = get_pid();
	int err;
	sigset_t old;
	short tid;

	/*
	 * We really don't want any signals here.
	 * 1) Holding the ACCESSLCK and getting a signal is bad - if the
	 *	signal handler exits, it can call _cleanup which will
	 *	attempt to p/v some semaphores.
	 * 2) want the gettid/addme operation to be atomic
	 * 3) should be fast!
	 */
	if ((tid = _usgettid(header)) != -1)
		return tid;

	/*
	 * Its possible that now we have a tid - if we got a signal
	 * and performed some semaphore operation - it could have
	 * acquired one for us
	 */
	__usblockallsigs(&old);
	if ((tid = _usgettid(header)) == -1) {
		if (oserror() == ENOLCK)
			goto bad;
		if ((tid = _usaddme(header, mypid, -1)) == -1) {
			err = oserror();
			if (_uerror)
				_uprint(1,
			"usadd:ERROR:Process %d failed to add to arena @0x%x",
						mypid, header);
			setoserror(err);
		}
	}
bad:
	__usunblockallsigs(&old);
	return(tid);
}

/* 
 * hdr_init
 *	Initialize the header : set up tid list, lock, sema, and history lists.
 * We don't worry about cleaning up after errors, since we'll unmap things
 */
static int
hdr_init(register ushdr_t *header, int maxu, size_t size, int autogr)
{
	short curtid;
	register int m, r_v;

	/* init usmalloc area
	 * A little tricky, we need a lock for the arena but cannot do a newlock
	 * until the arena is set up. So we call acreate pretending we're
	 * a non-shared malloc arena, then manually alloc the lock
	 * and change the arena flags to shared
	 *
	 * If either autogrow or autoresv then have adefgrow check for
	 * out of space using sigprocmask. If neither than we know that
	 * the arena is already at its specified size and can't fail
	 */
	if (_utrace)
		_uprint(0, "TRACE: Process %d header size %d malloc arena size %d",
			get_pid(), sizeof(*header), size - sizeof(*header));
	if ((header->u_arena = acreate((char *)(header + 1),
						size - sizeof(*header),
						autogr ? 0 : MEM_NOAUTOGROW,
						header, NULL)) == NULL) {
		if (_uerror)
			_uprint(1, "usinit:ERROR:unable to create usmalloc arena");
		return(0);
	}

	/* Turn off ability to alloc small blocks which prevents
	 * later user tweaks via usmallopt().
	 */
	if (amallopt(M_MXFAST, 0, header->u_arena)) {
		if (_uerror)
			_uprint(0, "usinit:ERROR:"
				   "unable to set options on usmalloc arena");
		setoserror(EINVAL);
		return(0);
	}

	if ((header->u_tidmap = usmalloc((size_t)(maxu) * sizeof(tid_entry_t), header))
						== NULL) {
		setoserror(ENOMEM);
		return(0);
	}
	/* compute hash table size - (power of 2 < maxusers) / 8 */
	m = maxu;
        while (m & (m - 1))
                 m = (m | (m - 1)) + 1;
        m = m >> 3;
	if (m == 0)
		m = 1;
        header->u_tidmaphashmask = m - 1;
	if ((header->u_tidmaphash = usmalloc((size_t)(m) * sizeof(short), header)) == NULL) {
		setoserror(ENOMEM);
		return(0);
	}

	for (curtid = 0; curtid < maxu; curtid++) {
		header->u_tidmap[curtid].t_pid = -1;
		header->u_tidmap[curtid].t_next = -1;
	}
	for (curtid = 0; curtid < m; curtid++)
		header->u_tidmaphash[curtid] = -1;
	header->u_lasttidzap = get_pid();

	/* init histlock for history logging */
	if ((header->u_histlock = usnewlock(header)) == NULL) {
		return(0);
	}
	header->u_histfirst = NULL;
	header->u_histlast = NULL;
	header->u_histcount = 0;
	header->u_histerrors = 0;
	header->u_histsize = 0;		/* as many as can fit */

	/* init dumpsemalock for semaphore dumping */
	if ((header->u_dumpsemalock = usnewlock(header)) == NULL) {
		return(0);
	}

	/* init openpollsema for selectable semaphores */
	if ((header->u_openpollsema = usnewsema(header, 1)) == NULL) {
		return(0);
	}

	/* init dumpsemalock for lock dumping */
	if ((header->u_dumplocklock = usnewlock(header)) == NULL) {
		return(0);
	}

	/* now alloc a_lock */
	if (usmallopt(M_CRLOCK, 0, header) != 0) {
		_uprint(1, "usinit:ERROR:unable to allocate usmalloc lock");
		setoserror(EINVAL);
		return(0);
	}

	/* have to tweak malloc here, since it is soooo smart it
	 * won't work well for small arenas 
	 */
	r_v = 0;
	if (size < _USMMAPSIZE) {
		r_v += amallopt(M_BLKSZ, 512, header->u_arena);
	} else {
		/* Put back the real default */
		r_v += amallopt(M_MXFAST, MAXFAST, header->u_arena);

		if (size < 256*1024) {
			r_v += amallopt(M_BLKSZ, 1024, header->u_arena);
		}
	}

	if (r_v) {
		if (_uerror)
			_uprint(0, "usinit:ERROR:"
				   "unable to set options on usmalloc arena");
		setoserror(EINVAL);
		return(0);
	}

	return(1);
}

/* VARARGS2 */
int
_uprint(int err, char *fmt, ...)
{
	va_list ap;
	static int setappend = 1;
	char buf[512];
	int lerror = oserror();
#ifdef DEBUG
	struct timeval tv;
#endif
	va_start(ap, fmt);

#ifdef DEBUG
	gettimeofday(&tv);
	cftime(buf, "%e %X", &tv.tv_sec);
	sprintf(&buf[strlen(buf)], " %d:", tv.tv_usec);
	vsprintf(&buf[strlen(buf)], fmt, ap);
#else
	vsprintf(buf, fmt, ap);
#endif
	if (err > 0) {
		strcat(buf, " : ");
		strcat(buf, strerror(lerror));
	}
	strcat(buf, "\n");
	if (setappend) {
		int flags;
		setappend = 0;
		if ((flags = fcntl(_utracefd, F_GETFL)) >= 0)
			fcntl(_utracefd, F_SETFL, flags|O_APPEND);
	}
	write(_utracefd, buf, strlen(buf));
	setoserror(lerror);	/* in case something above trashed it */
	return(0);
}

/*
 * usdetach - detach from an arena
 *
 * We don't bother to remove tid since next attacher will
 * do that for us
 *
 * WARNINGS:
 * 1) not protected from multiple shd procs doing usdetach
 * 2) does not close pollable semaphore file descriptors
 */
void
__usdetach(usptr_t *u)
{
	register ushdr_t *header = (ushdr_t *)u;
	struct _uslocalinfo	*pli;
	struct _uslocalinfo 	*lp;
	int lfd;			/* locking file descriptor */
	ssize_t mapsize = header->u_mmapsize;

	/* unlink from local list */
	for (lp = _usli; lp; lp = lp->li_next) {
		if (header == (ushdr_t *)lp->li_haddr) {
			if (lp == _usli)
				_usli = lp->li_next;
			else
				pli->li_next = lp->li_next;
			lfd = lp->li_fd;
			free(lp->li_name);
			free(lp);
			break;
		}
		pli = lp;
	}

	/* unmap arena */
	if (munmap(header, (size_t) mapsize) == -1) {
		if (_uerror)
			_uprint(1, "usdetach:ERROR:unable to unmap arena");
	}

	/* close arena fd - which will release all file locks */
	close(lfd);
}

#ifdef DEBUG
static void
_usdumpheader(ushdr_t *u, char *s)
{
	struct timeval tv;
	char buf[4000];
	int i;
	short *hp;
	tid_entry_t *tp;

	gettimeofday(&tv);
	sprintf(buf, "Header Dump (%s) by pid %d @ %s %d\n",
			s, get_pid(), ctime(&tv.tv_sec), tv.tv_usec);
	sprintf(&buf[strlen(buf)], "hashmask 0x%x mmapsize 0x%lx maxtidusers %d shflags 0x%x spdev 0x%x\n",
		u->u_tidmaphashmask, u->u_mmapsize, u->u_maxtidusers,
		u->u_shflags, u->u_spdev);
	sprintf(&buf[strlen(buf)], "mapaddr 0x%p locktype %d info 0x%p lastinit %d lasttidzap %d\n",
		u->u_mapaddr, u->u_locktype, u->u_info, u->u_lastinit,
		u->u_lasttidzap);
	if (u->u_tidmap) {
		tp = (tid_entry_t *)(((char *)u->u_tidmap - u->u_mapaddr) + (char *)u);
		sprintf(&buf[strlen(buf)], "tidmap:");
		for (i = 0; i < u->u_maxtidusers; i++)
			sprintf(&buf[strlen(buf)], "(%d %d) ",
				tp[i].t_pid,
				tp[i].t_next);
		sprintf(&buf[strlen(buf)], "\n");
	}
	if (u->u_tidmaphash) {
		hp = (short *)(((char *)u->u_tidmaphash - u->u_mapaddr) + (char *)u);
		sprintf(&buf[strlen(buf)], "tidmaphash:");
		for (i = 0; i <= u->u_tidmaphashmask; i++)
			sprintf(&buf[strlen(buf)], "%d ", hp[i]);
		sprintf(&buf[strlen(buf)], "\n");
	}
	write(_utracefd, buf, strlen(buf));
}
#endif

/*
 * signal blocking routines
 */
void
__usblockallsigs(sigset_t *old)
{
	sigset_t s;

	sigfillset(&s);
	sigdelset(&s, SIGQUIT);
	sigdelset(&s, SIGSEGV);
	sigdelset(&s, SIGBUS);
	sigprocmask(SIG_SETMASK, &s, old);
}

void
__usunblockallsigs(sigset_t *old)
{
	sigprocmask(SIG_SETMASK, old, NULL);
}

static int
zapfile(int fd, ssize_t sz)
{
	char buf[8192];
	ssize_t n;

	bzero(buf, 8192);

	lseek(fd, 0, SEEK_SET);
	while (sz > 0) {
		n = write(fd, buf, sz > 8192 ? 8192 : sz);
		if (n < 0)
			return -1;
		sz -= n;
	}
	return 0;
}
