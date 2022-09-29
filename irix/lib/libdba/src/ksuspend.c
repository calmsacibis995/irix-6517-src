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
        #pragma weak kaio_suspend = _kaio_suspend
#if (_MIPS_SIM == _MIPS_SIM_ABI64 || _MIPS_SIM == _MIPS_SIM_NABI32)
	#pragma weak kaio_suspend64 = _kaio_suspend
	#pragma weak _kaio_suspend64 = _kaio_suspend
#endif
#endif
#include "synonyms.h"
#include <aio.h>
#include "local.h"	/* libc/src/aio/local.h */
#include <mplib.h>	/* LOCKDECLINIT,...*/
#include <sys/dbacc.h>	/* for KAIOSEL_* defines */
#include <sys/param.h>
#include "klocal.h"

void
kaio_suspend(const aiocb_t * const list[], int cnt, struct timeval *tv,
	     int pthreads, kaio_ret_t *kr)
{
     fd_set lfd;
     int i, rv, nrawinprog, non_kaio_found, select_errno;
     aiocb_t *a;

     if (pthreads) {
	  LOCKDECLINIT(l, LOCKMISC);
	  fprintf(stderr, "Kernel Async I/O with suspend is not yet supported in programs also using\n\tpthreads. Please disable kernel async I/O in your application, or set the\n\tenvironment variable __SGI_DBA_NOKAIO to 1 to forcibly disable it.\n");
	  UNLOCKMISC(l);
	  setoserror(EBADF);
	  kr->back2user = 1;
	  kr->rv = -1;
	  return;
     }
     for (i = 0, nrawinprog = 0, non_kaio_found = 0; i < cnt; i++) {
	  if ((a = (aiocb_t *)list[i]) == NULL)
	       continue;
	  if (a->aio_fildes < 0) {
	       /* Bad user */
                setoserror(EBADF);
		kr->back2user = 1;
		kr->rv = -1;
		return;
	  }
	  if (a->aio_errno != EINPROGRESS) {
	       kr->rv = aio_error(a);
	       kr->back2user = 1;
	       return;
	  }
	  if ((a->aio_fildes >= kaio_info->maxfd)
	      || (a->aio_fildes >= kaio_info->aio_maxfd)) {
	       /* make sure activefd is up to date */
	       (void)_aio_realloc_fds(a->aio_fildes, _AIO_SGI_REALLOC_SIGERR);
	       if (oserror() == EAGAIN) {
		    /* Could not realloc bigger arrays for fd tracking */
		    kr->back2user = 1;
		    kr->rv = -1;
		    return;
	       }
	       /* n.b. This probably means it's not kaio */
	  }
	    
	  if (a->aio_kaio) {
	       nrawinprog++;
	  } else {
#ifdef DEBUG
	       if (kaio_info->aio_raw_fds[a->aio_fildes] == _AIO_SGI_KAIO_RAW) {
		    /* Raw fd, but aio_kaio not set -> in theory, a kaio request
		       that got booted to libc. In practice, ??? */
	       }
#endif
	       non_kaio_found++;
	  }
     }
     if (!nrawinprog) {
	  /* No possible outstanding kaio requests */
	  if (USR_AIO_IS_ACTIVE && non_kaio_found) {
	       kr->back2user = 0;
	       kr->rv = 0;	/* don't bother waiting on /dev/kaio */
	  } else if (non_kaio_found) {
	       kr->back2user = 1;
	       kr->rv = -1;
	  } else {
	       kr->back2user = 1;
	       kr->rv = 0;	/* nothing to wait for */
	  }
	  return;
     }
     /* at least one thing on the list was possible */
     for (i = 0; i < cnt; i++) {
	  if ((a = (aiocb_t *)list[i]) == NULL)
	       continue;
	  if (a->aio_kaio) {
	       kaio_info->akwaiting[a->aio_kaio] = KAIOSEL_WAITING;
	       if (kaio_info->akcomplete[a->aio_kaio] == KAIOSEL_DONE) {
		    /* Something on the list is done */
		    kr->back2user = 1;
		    kr->rv = 0;
	       }
	  }
     }

     /* If nothing on the list came from libc aio, kaio_suspend can do the
      * poll. Otherwise, we must just pass back the fd for /dev/kaio and
      * let libc do the poll.
      */
     if (USR_AIO_IS_ACTIVE && non_kaio_found) {
	  kr->back2user = 0;
	  kr->rv = kaio_info->akfd;
	  return;
     }

     FD_ZERO(&lfd);
     FD_SET(kaio_info->akfd, &lfd);

     /* Wait for the IO (if no filesystem files) */
     rv = select(kaio_info->akfd+1, &lfd, NULL, NULL, tv);
     select_errno = errno;
     kr->back2user = 1;

     /* Swiped from libc/src/aio/asuspend.c:aio_suspend() */
     if (rv == 0 && tv) {
	  /* case where timing out */
	  setoserror(EAGAIN);
     }
     if (rv == 1) { /* /dev/kaio */
	  /* printf("*** FREE count 2 is %d\n",as->freecount); */
#ifdef DEBUG
	  if (!FD_ISSET(kaio_info->akfd, &lfd)) {
	       fprintf(stderr, "kaio_suspend - returned fd is not /dev/kaio\n");
	  }
#endif
	  kr->rv = 0;
	  return;
     }
     if (rv < 0) {
	  setoserror(select_errno);
	  if (oserror() != EINTR) {
#ifdef DEBUG
	       perror("kaio_suspend(select error)");
#endif
	       kr->rv = -1;
	       return;
	  }
     } else if (rv != 1 && !tv) {
	  LOCKDECLINIT(l, LOCKMISC);
	  fprintf(stderr, "kaio_suspend:select returned %d!\n", rv);
	  UNLOCKMISC(l);
     }
     kr->rv = -1;
     return;
}
