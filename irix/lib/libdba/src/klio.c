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
        #pragma weak kaio_lio = _kaio_lio
#endif
#include "synonyms.h"
#include <aio.h>
#include "local.h"	/* from ../../libc/src/aio */
#include "klocal.h"


/*ARGSUSED*/
void
kaio_lio(int mode, aiocb_t * const list[], int cnt, sigevent_t *sig,
	 aiocb_t ***nk_list, int *nk_cnt, kaio_ret_t *kr)
{
     struct aiocb *aio;
     int ix, rv;
     *nk_list = 0;
     *nk_cnt = 0;
     for (ix = 0; ix < cnt; ix++) {
	  aio = list[ix];
	  if (aio->aio_lio_opcode == LIO_NOP)
	       continue;
	  else if ((aio->aio_lio_opcode == LIO_READ)
		   || (aio->aio_lio_opcode == LIO_WRITE)) {
	       /* Is this one eligible for kaio? */
	       if ((rv = _kaio_eligible(aio)) == -1) {
		    /* realloc or stat failed */
		    kr->back2user = 1;
		    kr->rv = -1;
		    return;
	       } else if (rv == 1) {
		    if (aio->aio_lio_opcode == LIO_READ)
			 aio_read(aio);
		    else
			 aio_write(aio);
	       } else {
		    if (!*nk_list) {
			 if ((*nk_list = malloc(sizeof(aiocb_t *) * (1 + cnt - ix))) == 0) {
			      setoserror(EAGAIN);
			      kr->back2user = 1;
			      kr->rv = -1;
			 }
		    }
		    (*nk_list)[*nk_cnt] = aio;
		    (*nk_cnt)++;
	       }
	  } else {
	       /* Invalid lio opcode */
	       setoserror(EINVAL);
	       kr->back2user = 1;
	       kr->rv = -1;
	  }
     }
     if (*nk_cnt == 0) {
	  /* kaio took them all */
	  kr->back2user = 1;
	  kr->rv = 0;
	  return;
     }
     if (!USR_AIO_IS_ACTIVE)
	  MAYBE_REINIT_TO_FORCE_SPROCS;
     kr->back2user = 0;
     return;
}

void _kaio_lio_free(aiocb_t *list[]) {
     if (list)
	  free(list);
}
