/*************************************************************************
*                                                                        *
*               Copyright (C) 1992, Silicon Graphics, Inc.       	 *
*                                                                        *
*  These coded instructions, statements, and computer programs  contain  *
*  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
*  are protected by Federal copyright law.  They  may  not be disclosed  *
*  to  third  parties  or copied or duplicated in any form, in whole or  *
*  in part, without the prior written consent of Silicon Graphics, Inc.  *
*                                                                        *
**************************************************************************/
#ident  "$Revision: 1.12 $ $Author: bbowen $"

#include <sgidefs.h>	/* for _MIPS_SIM defs */
#ifdef __STDC__
        #pragma weak aio_sgi_init = _aio_sgi_init
#if (_MIPS_SIM == _MIPS_SIM_ABI64 || _MIPS_SIM == _MIPS_SIM_NABI32)
        #pragma weak aio_sgi_init64 = _aio_sgi_init
        #pragma weak _aio_sgi_init64 = _aio_sgi_init
#endif
#endif
#include "synonyms.h"
#include <aio.h>
#include <ulocks.h>
#include "local.h"
#include <sys/syssgi.h>
#if !defined(_LIBC_NONSHARED) && !defined(_LIBC_ABI)
#include <dlfcn.h>
#endif

/*
 * User accessible routine available to initialize aio interface
 * maxuser - maximum number of user threads that will use aio interface
 */
void
aio_sgi_init(struct aioinit *init)
{
	int numthreads = _SGI_AIO_THREADS;
	int numlocks = _SGI_AIO_INIT_LOCKS;
	int numusers = _SGI_AIO_USER_THREADS;
#if defined(_LIBC_NONSHARED) || defined(_LIBC_ABI)
	int force_sproc_init = 1;

	if (init)
	     init->aio_usedba = 0;
#else
	int force_sproc_init = 0;
	int kaio_from_init;
	struct kaio_ret kaio_ret;
	char *kaio_env = getenv("__SGI_USE_DBA");

	if (init == (struct aioinit *)DO_SPROC_INIT) {
	     /* Assumes call with DO_SPROC_INIT comes only if kaio tried/set */
	     if (_aioinfo->sh_aio_kaio)
		  /* mixed libc / kaio environment */
		  init = &(_aioinfo->sh_aio_kaio->ainit);
	     else
		  /* kaio failed to initialize */
		  init = NULL;
	     force_sproc_init = 1;
	     goto normal_init;
	}
	kaio_from_init = (init && (init->aio_usedba == 1));
	if ((kaio_from_init || kaio_env) && !_aioinfo) {
	     void *libdba = 0, (*kip)(ARGS_TO_LIBDBA_INIT) = 0;
	     libdba = dlopen("libdba.so", RTLD_NOW);
	     if (libdba)
		  kip = (void (*)(ARGS_TO_LIBDBA_INIT))dlsym(libdba, "kaio_init");
	     else
		  printf("aio_sgi_init: dlopen failed: %s\n", dlerror());
	     if ((libdba == NULL) || (kip == NULL)) {
		  goto normal_init;
	     }
	     /* Init libc enough to get _aioinfo defined, but hold off on sprocs */
	     if (init) {
		  /*
		   * With less than two threads deadlocks are possible.
		   * We need at least the control thread plus a user
		   * thread to avoid deadlock
		   */
		  if(init->aio_threads >= 2)
		       numthreads = init->aio_threads;
		  if (init->aio_locks > 0)
		       numlocks = init->aio_locks;
		  if (init->aio_numusers > 0)
		       numusers = init->aio_numusers;
	     }
	     __aio_init1(numthreads, numlocks, numusers, INIT_KAIO);
	     /* Call libdba.so:kaio_init() */
	     (*kip)(init, _aioinfo, kaio_env, _aioinfo->maxfd,
		    &(_aioinfo->sh_aio_kaio), _aio_realloc_fds, &kaio_ret,
		    force_sproc_init);
	     if (kaio_ret.back2user == 1)
		  return;
	     if (kaio_ret.rv == -1) {
		  _aioinfo->sh_aio_kaio = 0;	/* KAIO_IS_ACTIVE <- false */
		  force_sproc_init = 1;
	     }
	}

   normal_init:
#endif
	if (init) {
	    	/*
		 * With less than two threads deadlocks are possible.
		 * We need at least the control thread plus a user
		 * thread to avoid deadlock
		 */
	    if(init->aio_threads >= 2)
		numthreads = init->aio_threads;
	    if (init->aio_locks > 0)
		numlocks = init->aio_locks;
	    if (init->aio_numusers > 0)
		numusers = init->aio_numusers;
	}

	/* start up the threads */
	__aio_init1(numthreads, numlocks, numusers, force_sproc_init?INIT_FORCE_SPROCS:INIT_NORMAL);
}


/*
 * User might have called setrlimit[64] to increase the number of available
 * file descriptors since the call to aio_init. In that case, must
 * increase the size of any arrays being used to hold descriptors. We usually
 * call this only if we see a descriptor higher than the number we thought were
 * out there. aqueue needs to do oserror() to make sure the new
 * array was actually allocated.
 */
/* ARGSUSED */
int
_aio_realloc_fds(int fd, int errsig)
{
     char *newarray;
     int oldnfds = _aioinfo->maxfd;

     _aioinfo->maxfd = getdtablesize();
     if (_aioinfo->maxfd <= oldnfds)
	  return 0;
     /* Since realloc can blow away the existing array, do it ourselves
      * with malloc.
      */
     if (KAIO_IS_ACTIVE) {
	  struct kaio_ret kaio_ret;
	  libdba_kaio_realloc_fds(fd, errsig, _aioinfo->maxfd, &kaio_ret);
     }
     if (USR_AIO_IS_ACTIVE) {
	  if (newarray = (char *)malloc(_aioinfo->maxfd * sizeof(struct aiolink *))) {
	       bzero(&newarray[oldnfds],
		     sizeof(struct aiolink **) * (_aioinfo->maxfd - oldnfds));
	       bcopy(_aioinfo->activefds, newarray, oldnfds * sizeof(struct aiolink *));
	       free(_aioinfo->activefds);
	       _aioinfo->activefds = (struct aiolink **)newarray;
	  } else {
	       _aioinfo->maxfd = oldnfds;
	       if (errsig)
		    setoserror(EAGAIN);
	  }
     }
     return 0;	/* Want to keep _AIO_SGI_ISKAIORAW from making a bad dereference */
}
