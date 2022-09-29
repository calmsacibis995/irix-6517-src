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
#include "synonyms.h"
#include <aio.h>
#include "local.h"	/* from ../../libc/src/aio */
#include <sys/dbacc.h>	/* for KAIOSEL_* defines */
#include <sys/syssgi.h> /* for args to syssgi() */
#include <sys/types.h>	/* these 2 for struct stat, etc. */
#include <sys/stat.h>
#include "klocal.h"


int
_kaio_eligible(aiocb_t *aio) {
     int fd = aio->aio_fildes;
     if ((aio->aio_sigevent.sigev_notify < 128) /* old 5.2 compat */
	 || (aio->aio_sigevent.sigev_notify != SIGEV_NONE) /*unsupported mode*/) {
	  return 0;
     }
     if ((fd >= kaio_info->maxfd)
	 || (fd >= kaio_info->aio_maxfd)){
	  (void)_aio_realloc_fds(fd, _AIO_SGI_REALLOC_SIGERR);
	  if (oserror() == EAGAIN) {
	       /* Could not realloc bigger arrays for fd tracking */
#ifdef DEBUG
	       if(aio_do_prints)
		    printf("aio_realloc_fds failed in kio\n");
#endif
	       return -1;
	  }
     }
     if (kaio_info->aio_raw_fds[fd] == _AIO_SGI_KAIO_UNKNOWN) {
	  /* First access to fd since open */
	  struct stat sb;
	  if (fstat(fd, &sb) < 0) {
	       return -1;
	  }
	  if ((sb.st_mode & S_IFMT) == S_IFCHR) {
	       kaio_info->aio_raw_fds[fd] = _AIO_SGI_KAIO_RAW;
	  } else {
	       kaio_info->aio_raw_fds[fd] = _AIO_SGI_KAIO_COOKED;
	  }
     }
     if (kaio_info->aio_raw_fds[fd] != _AIO_SGI_KAIO_RAW) {
	  return 0;
     }
     return 1;
}

void
kaio_submit(struct aiocb *aio, int op, struct kaio_ret *kr)
{
     int rv;

     if ((rv = _kaio_eligible(aio)) == -1) {
	  aio->aio_kaio = 0; /*safety*/
	  kr->back2user = 1;
	  kr->rv = -1;
	  return;
     } else if (rv == 0) {
	  if (!USR_AIO_IS_ACTIVE) {
	       MAYBE_REINIT_TO_FORCE_SPROCS;
	  }
	  kr->back2user = 0;
	  aio->aio_kaio = 0; /*safety*/
	  return;
     }

     if (aio->aio_kaio) {
	  /* Someone used this but didn't clear it - missing aio_return? */
     }
     aio->aio_kaio = 0;

     /* syssgi() only looks at the 1st 2 args, the rest are for par visibility */
     if (op == LIO_READ) {
	  aio->aio_ret = 0;
	  kr->rv = (int)syssgi(SGI_KAIO_READ, aio, aio->aio_fildes, aio->aio_nbytes, AIO_OFFSET(aio));
     } else if (op == LIO_WRITE) {
	  aio->aio_ret = 0;
	  kr->rv = (int)syssgi(SGI_KAIO_WRITE, aio, aio->aio_fildes, aio->aio_nbytes, AIO_OFFSET(aio));
     } else {
	  goto skipit;
     }
     if (kr->rv != 0) {
	  if ((errno == EAGAIN) && (aio->aio_kaio == 0)) {
	       /* Looks like table was full - too many outstanding I/Os */
	       if (USR_AIO_IS_ACTIVE) {
		    /* May as well let libc handle this one */
		    kr->back2user = 0;
	       } else {
		    /* Could failover to libc, but for now just return EAGAIN */
		    kr->back2user = 1;
		    kr->rv = -1;
	       }
	       return;
	  }
	  if (aio->aio_kaio)
	       kaio_info->akwaiting[aio->aio_kaio] = KAIOSEL_NOTINUSE;
	  if (USR_AIO_IS_ACTIVE) {
	       /* Let libc try it? */
	       kr->back2user = 0;
       	       aio->aio_kaio = 0;
#ifdef DEBUG
	       fprintf(kaio_info->dfile, "returning to let libc try on aiocb = 0x%llx\n", (long long)aio);
	       fflush(kaio_info->dfile);
#endif
	       return;
	  }
#ifdef DEBUG
	  printf("returning immediately to user\n");
#endif
     }
     kr->back2user = 1;
     return;

 skipit:
     /* Illegal operation */
     kr->back2user = 1;
     kr->rv = -1;
     setoserror(ENOTSUP);
     return;
}


void __kaio_reset(void) {
     /* The is called from aio_reset() in libc, which is called by fork
	and sproc in new procs */
     if (kaio_info) {
	  if (kaio_info->aio_raw_fds)
	       free(kaio_info->aio_raw_fds);
	  if (kaio_info->akcomplete)
	       free(kaio_info->akcomplete);
	  if (kaio_info->akwaiting)
	       free(kaio_info->akwaiting);
	  free(kaio_info);
     }
}

void __kaio_closechk(int fd) {
     /* Only called if kaio is active */
     if ((fd >= 0) && (fd < kaio_info->maxfd)) {
	  kaio_info->aio_raw_fds[fd] = _AIO_SGI_KAIO_UNKNOWN;
     }
}
