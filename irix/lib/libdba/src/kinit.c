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
#ident  "$Revision: 1.1 $ $Author: wombat $"
#include <sgidefs.h>	/* for _MIPS_SIM defs */
#ifdef __STDC__
        #pragma weak kaio_init = _kaio_init
#endif
#include "synonyms.h"
#include <aio.h>
#include <mplib.h>
#include "local.h"	/* from ../../libc/src/aio */
#include <sys/syssgi.h>
#include <sys/dbacc.h>
#include "klocal.h"

#define RV_ABORT_KAIO_INIT -1	/* causes _aioinfo->sh_aio_kaio to be cleared */
#define RV_KAIO_ALREADY_INITED 2
#define RV_SOMETHING_TO_FORCE_SPROC_INIT 3

#ifndef KAIOUSER_1_1	/* July 1998 */
#define KAIOUSER_1_1	3
#endif

/* This is called from libc/src/aio/ainit.c, and is the only exported symbol.
 * The return structure contains pointers to all functions libc needs.
 */

struct kaioinfo *kaio_info;
struct sh_aio_kaio *sh_aio_kaio;

/*ARGSUSED*/
void
kaio_init(struct aioinit *init, struct aioinfo *ai, char *kaio_env, int aio_maxfd,
	  struct sh_aio_kaio **sakp, int (*arf)(int, int), struct kaio_ret *kr, int force_sproc_init)
{
     struct dba_conf dcf;
     int kfd;
     ptrdiff_t rv;
     char *tkcmpl, *tkwait;
     int ithreads, max_aio/*, i*/;
     char *gsp = getenv("__SGI_DBA_NSPROCS");
     char *force_no_kaio = getenv("__SGI_DBA_NOKAIO");
     struct __kaioinit kaioinit;
#if defined(DEBUG)
#define TSZ 30
     char tfile[TSZ];
#endif
     /* For malloc/calloc/free, below */
     LOCKDECLINIT(l, LOCKMISC);

     if (force_no_kaio || force_sproc_init) {
	  kr->back2user = 0;
	  kr->rv = RV_SOMETHING_TO_FORCE_SPROC_INIT;
	  UNLOCKMISC(l);
	  return;
     }

     if (kaio_info) {
	  /* We have already been here - maybe this is an sproc force */
	  kr->back2user = 0;
	  kr->rv = RV_KAIO_ALREADY_INITED;
#ifdef DEBUG
	  printf("libdba:kaio_init() called more than once, force_sproc = %d\n",
		 force_sproc_init);
#endif
	  UNLOCKMISC(l);
	  return;
     }
     if ((kaio_info = malloc(sizeof(struct kaioinfo))) == 0) {
	  /* abort! abort! abort! */
	  kr->back2user = 0;
	  kr->rv = RV_ABORT_KAIO_INIT;
	  UNLOCKMISC(l);
	  return;
     }
     bzero(kaio_info, sizeof(struct kaioinfo));
     kaio_info->pid = getpid();
     sh_aio_kaio = &(kaio_info->sh_aio_kaio);
     *sakp = sh_aio_kaio;

     if (kaio_env)
	  kaio_info->kaio_from_env = 1;
     if (gsp)
	  ithreads = atoi(gsp);
     if (init) {
	  /* Must save the user's params, in case the user is also
	   * using files and we need to do a full init later.
	   * (Actually, that doesn't work because we already did part
	   * of user init before getting here. Unfortunate.)
	   */
	  bcopy(init, &(kaio_info->ainit), sizeof(struct aioinit));
	  if (gsp)
	       sh_aio_kaio->ainit.aio_threads = ithreads;
	  if (init->aio_usedba == 1)
	       kaio_info->kaio_from_init = 1;
	  else if (init->aio_usedba == -1) {
	       kr->back2user = 0;
	       kr->rv = RV_SOMETHING_TO_FORCE_SPROC_INIT;
	       UNLOCKMISC(l);
	       return;
	  }
     } else {
	  if (gsp)
	       sh_aio_kaio->ainit.aio_threads = ithreads;
	  else
	       sh_aio_kaio->ainit.aio_threads = 0;
	  sh_aio_kaio->ainit.aio_locks = 0;
	  sh_aio_kaio->ainit.aio_numusers = 0;
	  sh_aio_kaio->ainit.aio_usedba = 0;
     }

     if (!kaio_env && !kaio_info->kaio_from_init) {
	  /* Then why are we here??? (sanity check) */
	  kr->back2user = 0;
	  kr->rv = RV_ABORT_KAIO_INIT;
	  UNLOCKMISC(l);
	  return;
     }
     /* If the user requests kaio, check for kernel support.
      *
      * If program requested kaio by calling aio_sgi_init with the
      * usedba field set, it should be prepared to check for -1 in
      * that field on return to know if the init succeeded. This lets
      * the application retry with a different number of threads
      * requested, for example. --- Hmmm, no longer true, since we
      * already did the thread init. User is stuck with what was
      * requested. Really need to find a way around this.
      *
      * If kaio is requested because the user set __SGI_USE_DBA in the
      * environment, it's probably because the DB server (like Oracle
      * 8 or newer Sybase) hasn't been modified to take advantage of
      * the DBA and won't retry if aio_sgi_init "failed". In that case,
      * disable further use of kaio and just use the library.
      *
      * Must distinguish between user-retry-after-kaio-init-fail and
      * spurious later attempt to initialize.
      */
     if ((_syssgi(SGI_DBA_CONFIG, SGI_DBCF_GET, &dcf, sizeof(dcf)) == -1)
	 || !(dcf.dbcf_installed & SGI_DBACF_AIO)
	 || !(dcf.dbcf_installed & SGI_DBACF_AIOSUSPEND)) {
	  /* KAIO probably not linked into kernel, or not 6.5+ */
	  if (kaio_info->kaio_from_init) {
	       init->aio_usedba = -1;
	       UNLOCKMISC(l);
	       return;
	  } else {
#ifdef DEBUG
	       printf("aio_init - syssgi call failed, aborting kaio suspend: errno = %d\n",errno);
#endif
	       kr->rv = RV_ABORT_KAIO_INIT;
	       goto abort_kaio;
	  }
     }

     max_aio = dcf.dbcf_aio_max; /* override libc 2K max in-flight */
     /* Set up areas to track launched/completed for suspend */
     /* WWW In the threaded world, need to redo this */
     if ((tkcmpl = calloc(1, max_aio)) == (char *)0) {
	  kr->rv = RV_ABORT_KAIO_INIT;
	  goto abort_kaio;
     }
     if ((tkwait = calloc(1, max_aio)) == (char *)0) {
	  kr->rv = RV_ABORT_KAIO_INIT;
	  goto abort_kaio;
     }
     /* Remember how many open files allowed - user could call setrlimit */
     if (aio_maxfd < KAIO_DFLT_NFDS)
	  kaio_info->maxfd = KAIO_DFLT_NFDS; /* Try to avoid reallocs later */
     else
	  kaio_info->maxfd = aio_maxfd;
     if ((kaio_info->aio_raw_fds = malloc(kaio_info->maxfd)) == (char *)0) {
	  kr->rv = RV_ABORT_KAIO_INIT;
	  goto abort_kaio;
     } else {
	  /* User will now do KAIO to all raw devs */
	  int ix;
	  for (ix = 0; ix < kaio_info->maxfd; ix++) {
	       kaio_info->aio_raw_fds[ix] = _AIO_SGI_KAIO_UNKNOWN;
	  }
     }

#if defined(DEBUG)
     if (getenv("__SGI_DBA_DEBUG")) {
	  snprintf(&(tfile[0]), TSZ, "/tmp/k/kdbg.%d", getpid());
	  kaio_info->dfile = fopen(&(tfile[0]), "a");
	  if (kaio_info->dfile == (FILE *)NULL)
	       exit(17);
     }
#endif
     /* Register these areas with the kernel */
     kaioinit.k_version = KAIOUSER_1_1;
     kaioinit.k_completed = (uint64_t)tkcmpl;
     kaioinit.k_waiting = (uint64_t)tkwait;
     kaioinit.k_naio = max_aio;
     if ((rv = syssgi(SGI_KAIO_USERINIT, &kaioinit, sizeof kaioinit)) == 0) {
	  /* SGI_KAIO_USERINIT must happen before this open() */
	  if ((kfd = open("/dev/kaio", O_RDONLY)) < 0) {
	       fprintf(stderr, "kaio_init: Could not open /dev/kaio: %s. As root,\n", strerror(errno));
	       fprintf(stderr, "\t# rm -f /dev/kaio\n\t# ln -s /hw/kaio /dev/kaio\n");
	       kr->rv = RV_ABORT_KAIO_INIT;
	       goto abort_kaio;
	  }
	  if (kfd > FD_SETSIZE) {
	       /* too big for our tiny brains to handle */
	       fprintf(stderr, "libdba/kaio_init: fd %d for /dev/kaio is too large\n (FD_SETSIZE = %d)", kfd, FD_SETSIZE);
	       close(kfd);
	       kr->rv = RV_ABORT_KAIO_INIT;
	       goto abort_kaio;
	  }
     } else {
	  fprintf(stderr, "libdba/kaio_init: userinit failed: %ld, err = %lld\n",
		 rv, (long long)kaioinit.k_err);
	  kr->rv = RV_ABORT_KAIO_INIT;
	  goto abort_kaio;
     }

     /* Keep track of max allowed by libdba+kernel, for akwaiting, akcomplete */
     if (max_aio != kaioinit.k_naio_used) {
	  printf("libdba/kaio_init: requested %lld aio, got %lld\n",
		 (long long)max_aio, (long long)kaioinit.k_naio_used);
     }
     if (max_aio < kaioinit.k_naio_used)
	  kaio_info->maxaio = (int)kaioinit.k_naio_used;
     else
	  kaio_info->maxaio = max_aio;

     kaio_info->aio_maxfd = aio_maxfd;
     kaio_info->akcomplete = tkcmpl;
     kaio_info->akwaiting = tkwait;
     kaio_info->aioinfo = ai;	/* do not use - future escape hatch only */
     kaio_info->akfd = kfd;
     kaio_info->aio_realloc_fds_fn = arf;
     sh_aio_kaio->kaio_fns.kaio_init_fn = kaio_init;
     sh_aio_kaio->kaio_fns.kaio_read_fn = kaio_read;
     sh_aio_kaio->kaio_fns.kaio_write_fn = kaio_write;
     sh_aio_kaio->kaio_fns.kaio_submit_fn = kaio_submit;
     sh_aio_kaio->kaio_fns.kaio_suspend_fn = kaio_suspend;
     sh_aio_kaio->kaio_fns.kaio_error_fn = kaio_error;
     sh_aio_kaio->kaio_fns.kaio_return_fn = kaio_return;
     sh_aio_kaio->kaio_fns.kaio_lio_fn = kaio_lio;
     sh_aio_kaio->kaio_fns.kaio_cancel_fn = kaio_cancel;
     sh_aio_kaio->kaio_fns.kaio_fsync_fn = kaio_fsync;
     sh_aio_kaio->kaio_fns.kaio_hold_fn = kaio_hold;
     sh_aio_kaio->kaio_fns.kaio_closechk_fn = _kaio_closechk;
     sh_aio_kaio->kaio_fns.kaio_reset_fn = _kaio_reset;
     sh_aio_kaio->kaio_fns.kaio_realloc_fds_fn = _kaio_realloc_fds;
     sh_aio_kaio->kaio_fns.kaio_lio_free_fn = _kaio_lio_free;
     sh_aio_kaio->kaio_fns.kaio_user_active_fn = _kaio_user_active;

#ifdef DEBUG
     printf("aio_sgi_init: using kaio, kfd = %d\n", kfd);
#endif
     UNLOCKMISC(l);
     kr->back2user = 1;
     kr->rv = 0;
     return;


 abort_kaio:
#ifdef DEBUG
     printf("had to abort kaio, rv=%lld, kfd = %d, errno = %d, tkc = 0x%llx, tkw = 0x%llx\n",
	    kr->rv,kfd,errno,(long long)tkcmpl,(long long)tkwait);
#endif
     if (tkcmpl) {
	  free(tkcmpl);   	tkcmpl = 0;
     }
     if (tkwait) {
	  free(tkwait);   	tkwait = 0;
     }
     if (kaio_info) {
	  free(kaio_info);	kaio_info = 0;
     }
     *sakp = 0;
     UNLOCKMISC(l);
     kr->back2user = 0;
     kr->rv = -1;
     /* On return to aio_sgi_init, will fall through into sproc init */
     return;
}


void
_kaio_user_active(int newstate) {
     kaio_info->user_active = newstate;
}

#include <sys/param.h>	/* for MAX() */

void
_kaio_realloc_fds(int fd, int errsig, int new_aio_maxfd, struct kaio_ret *kr) {
     char *newarray;
     int newkmax;
     int ix;
     LOCKDECLINIT(l, LOCKMISC);	/* WWW ??? */

     kaio_info->aio_maxfd = new_aio_maxfd; /* libc size */
     newkmax = MAX(new_aio_maxfd, fd) + 1;
     if (USR_AIO_IS_ACTIVE)
	  kr->back2user = 0;
     else
	  kr->back2user = 1;
     if (newkmax <= kaio_info->maxfd) {
	  UNLOCKMISC(l);
	  return;
     }

     if (newarray = (char *)malloc(newkmax)) {
	  bcopy(kaio_info->aio_raw_fds, newarray, kaio_info->maxfd);
	  free(kaio_info->aio_raw_fds);
	  kaio_info->aio_raw_fds = newarray;
	  for (ix = kaio_info->maxfd; ix < newkmax; ix++)
	       kaio_info->aio_raw_fds[ix] = _AIO_SGI_KAIO_UNKNOWN;
	  kaio_info->maxfd = newkmax;	/* libdba size */
     } else {
	  if (errsig)
	       setoserror(EAGAIN);
     }
     UNLOCKMISC(l);
}
