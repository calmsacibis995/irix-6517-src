/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986-1992 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 1.53 $"

#ifdef __STDC__
	#pragma weak flockfile = _flockfile
	#pragma weak funlockfile = _funlockfile
	#pragma weak ftrylockfile = _ftrylockfile
	#pragma weak __fislockfile = ___fislockfile
#endif

#include "synonyms.h"
#include "sys/types.h"
#include "unistd.h"
#include "limits.h"
#include "stdlib.h"
#include "stdarg.h"
#include "errno.h"
#include "string.h"
#include "ulocks.h"
#include "us.h"
#include "mplib.h"

/* 
 * Interfaces to semaphored libc calls
 * A list if these is documented in intro.3 and usconfig.3p
 *
 */

/* io semas */
ulock_t __randlock _INITBSSS;		/* lock for random number stuff */
ulock_t __monlock _INITBSSS;		/* lock for monitor(3X) */
ulock_t __mmalloclock _INITBSSS;	/* main malloc lock */
ulock_t __pmqlock _INITBSSS;		/* posix mq subsystem lock */
usema_t *__dirsema _INITBSS;		/* sema for manipulating directories */
usema_t *__opensema _INITBSS;		/* sema for opening and closing files */
usema_t *__miscsema _INITBSS;		/* sema for other stuff */
usema_t *__localesema _INITBSS;		/* sema for locale (pfmt, etc) */

/* WARNING: The value of IOSYNC_MAX must be equal to the
 *          number of io semaphores and locks declaired above
 */
#define IOSYNC_MAX 8
					
struct slibcinfo {
	usema_t **iosema;		/* array of semas for file descriptors*/
	char *tmpname;
	usptr_t *ioarena;
};
static struct slibcinfo *_slibcinfo _INITBSS;
static int __maxiofiles _INITBSS;


/* Alternative IO stream locking handle
 */
extern void	*__mtlib_io_locks;


/*
 * _seminit() 
 *	initialize libc threading
 * Returns:
 *	0 Success
 *	<0 Error 
 */

int
_seminit(void)
{
	usptr_t *tmpptr;
	int i, users;
	ptrdiff_t oldag, oldar, oldsize, oldtype;

	if (__multi_thread == MT_SPROC)  {
		/* make sure enough tids for another io arena member */
		if (_usany_tids_left((ushdr_t *)_slibcinfo->ioarena) < 0)
			return(-1);
		return(0);
	}
	if (__multi_thread != MT_NONE)  {
		/* match error return from sproc system call */
		setoserror(EPERM);  
		return(-1);
	}

	if (_slibcinfo == NULL &&
		   ((_slibcinfo = calloc(1, sizeof(*_slibcinfo))) == NULL)) {
		return(-1);
	}
	/*
	 * it would be great to auto-grow the streams io semaphore array
	 * but that is rather hard.
	 * So, we allocate enough semaphores to hold the maximum
	 * # open fd's at this time..
	 */
	__maxiofiles = getdtablesize();
	if (__maxiofiles < 100)
		__maxiofiles = 100;
	if (_slibcinfo->iosema == NULL &&
		   ((_slibcinfo->iosema = calloc(__maxiofiles + 1, sizeof(usema_t *))) == NULL)) {
		free(_slibcinfo);
		_slibcinfo = NULL;
		return(-1);
	}

	/*
	 * initialize user sync header
	 * It would probably be best to always use /dev/zero...
	 * But there are some subtlies that need more testing (surrounding
	 * the fact that /dev/zero is the same 'name' for everyone and
	 * confuses file locks in usinit - these are fixed... but need more
	 * testing).
	 * We permit an env variable choose..
	 */
	if (getenv("USUSEZEROFORLIBC") != NULL)
		_slibcinfo->tmpname = "/dev/zero";
	else
		_slibcinfo->tmpname = tempnam(NULL, "io");

	if (_slibcinfo->tmpname == NULL) {
		free(_slibcinfo->iosema);
		free(_slibcinfo);
		_slibcinfo = NULL;
		return(-1);
	}

	/* make sure have enough space for libc */
	users = (int)usconfig(CONF_INITUSERS, 8);
	usconfig(CONF_INITUSERS, users);
	i = (__maxiofiles + IOSYNC_MAX) * (_ussemaspace() + 96);
	oldsize = usconfig(CONF_INITSIZE, i + 20*1024);
	oldtype = usconfig(CONF_ARENATYPE, US_SHAREDONLY);
	oldag = usconfig(CONF_AUTOGROW, 0);
	oldar = usconfig(CONF_AUTORESV, 0);

	tmpptr = usinit(_slibcinfo->tmpname);

	usconfig(CONF_INITSIZE, oldsize);
	usconfig(CONF_ARENATYPE, oldtype);
	usconfig(CONF_AUTOGROW, oldag);
	usconfig(CONF_AUTORESV, oldar);

	if (tmpptr == (usptr_t *) NULL)
		return(-1);

	/* alloc malloc) lock */
	if ((__mmalloclock = usnewlock(tmpptr)) == (ulock_t) NULL)
		goto err;

	/* alloc pmq lock */
	if ((__pmqlock = usnewlock(tmpptr)) == (ulock_t) NULL)
		goto err;

	/* alloc monitor(3X) lock */
	if ((__monlock = usnewlock(tmpptr)) == (ulock_t) NULL)
		goto err;

	/* alloc special random number lock */
	if ((__randlock = usnewlock(tmpptr)) == (ulock_t) NULL)
		goto err;

	/* alloc special directory sema */
	if ((__dirsema = usnewsema(tmpptr, 1)) == (usema_t *) NULL)
		goto err;
	usctlsema(__dirsema, CS_RECURSIVEON);

	/* alloc locale sema */
	if ((__localesema = usnewsema(tmpptr, 1)) == (usema_t *) NULL)
		goto err;
	usctlsema(__localesema, CS_RECURSIVEON);

	/* alloc sema for misc stuff */
	if ((__miscsema = usnewsema(tmpptr, 1)) == (usema_t *) NULL)
		goto err;

	/* alloc open sema */
	if ((__opensema = usnewsema(tmpptr, 1)) == (usema_t *) NULL)
		goto err;
	usctlsema(__opensema, CS_RECURSIVEON);

	/* alloc sema pointers for each iop */
	for (i = 0; i <= __maxiofiles; i++) {
		if ((_slibcinfo->iosema[i] = usnewsema(tmpptr, 1)) == (usema_t *) NULL)
			goto err;
		usctlsema(_slibcinfo->iosema[i], CS_RECURSIVEON);
	}

	_slibcinfo->ioarena = tmpptr;
	__multi_thread = MT_SPROC;
	__us_rsthread_stdio = __us_sthread_stdio;
	__us_rsthread_misc = __us_sthread_misc;
	__us_rsthread_malloc = __us_sthread_malloc;
	__us_rsthread_pmq = __us_sthread_pmq;
	return(0);

err:
	usdetach(tmpptr);
	return(-1);
}

/* add sproc'd child to io arena now - die if cannot join */
void
_semadd(void)
{
	char buf[256];
	if (usadd(_slibcinfo->ioarena) != 0) {
		sprintf(buf, "New process pid %d could not join I/O arena:%s\n", get_pid(), strerror(oserror()));
		write(2, buf, strlen(buf));
		if (oserror() == ENOSPC) {
			sprintf(buf, "Need to raise CONF_INITUSERS? (usconfig(3P))\n");
			write(2, buf, strlen(buf));
		}
		_exit(-1);
	}
}

/*
 * flockfile - single thread access to FILE
 */
void
flockfile(register FILE *iop)
{
	usema_t *s;

	if (MTLIB_ACTIVE()) {
		_mtlib_ctl(MTCTL_RLOCK_LOCK, __mtlib_io_locks, iop->_file);
		return;
	}
	if (_slibcinfo && (s = _slibcinfo->iosema[iop->_file]) != NULL)
		uspsema(s);
}

/*
 * ftrylockfile - single thread access to FILE
 */
int
ftrylockfile(register FILE *iop)
{
	usema_t *s;
	int rv = 0;	/* optimistic */

	if (MTLIB_ACTIVE()) {
		return (_mtlib_ctl(MTCTL_RLOCK_TRYLOCK,
				   __mtlib_io_locks, iop->_file));
	}
	if (_slibcinfo && (s = _slibcinfo->iosema[iop->_file]) != NULL)
		if (uscpsema(s) == 0)
			rv = 1;
	return rv;
}

void
funlockfile(register FILE *iop)
{
	usema_t *s;

	if (MTLIB_ACTIVE()) {
		_mtlib_ctl(MTCTL_RLOCK_UNLOCK, __mtlib_io_locks, iop->_file);
		return;
	}
	if (_slibcinfo && (s = _slibcinfo->iosema[iop->_file]) != NULL)
		usvsema(s);
}

/*
 * for debugging only - in case of uncertainity - return true!
 */
int
___fislockfile(FILE *iop)
{
	sem_t *sp;

	if (_slibcinfo && (sp = (sem_t *) _slibcinfo->iosema[iop->_file]))
		return(ustestsema(sp) <= 0 && (sp->sem_opid == get_pid()));
	return 1;
}

/*
 * STDIO routines protected by a semaphore per iop
 *
 * NOTES:
 * Ordering: iop is locked, then open lock where required.
 *	The few places (flush) that need to other ordering use a generation
 *	number, to permit unlocking the open lock
 *
 * fdopen, fopen: lock open since _flag field is marker whether iop is alloced
 * fclose: wants both cause _findiop doesn't lock iop
 * freopen: requires locking since it gets handed an iop
 * _setbufend: always called with iop locked
 * _bufsync:always called with iop locked
 * _filbuf:always called with iop locked
 * _findbuf:always called with iop locked
 * _findiop: is locked so callers need not lock
 * _flsbuf: always called locked
 * _xflsbuf: always called locked
 * _wrtchk: always called locked
 * fget/setpos: atomic ops
 * popen/pclose: why do these need locking??
 * tempnam/tmpfile/tmpnam: seems like a good idea to single thread -
 *		use openlock for convenience
 * opendir: no locking since until it returns there is no state
 *
 * Cataloges and locale
 * _cur_locale: protected by __localesema
 *
 * catopen/_cat_init - the only MP issue here is that one might
 *	open more than NL_SETMAX catalgoues - but there is no real harm
 *	in that.
 * _fullocale: must be called locked
 * _nativeloc: must be called locked
 * setlocale: lock to protect _cur_local
 * localeconv: lock to protect _cur_locale
 * _set_tab: must be called locked
 * fmtmsg: lock around this so messages appear non-scrammbled
 * __gtxt: always locks to protect adding open catalogues - can be
 *	called either locked or unlocked. Note - setlocale changes must
 *	be protected at a higher level!
 * __pfmt_print: lock
 * setlabel:lock to protect setting __pfmt_label
 * setcat: locked
 * strftime: lock to protect LC_TIME changing
 * strxfrm: lock to protect LC_COLLATE changing
 * strcoll: lock to protect LC_COLLATE changing
 * lfmt: lock to be consistent msg
 * vlfmt: lock to be consistent msg
 * perror: lock so message comes out well
 * nl_langinfo: lock since calls setlocale ...
 */
