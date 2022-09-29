/*************************************************************************
 *                                                                        *
 *               Copyright (C) 1995, Silicon Graphics, Inc.       	 *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident  "$Revision: 1.4 $ $Author: ack $"


#ifdef __STDC__
	#pragma weak lio_listio64 = _lio_listio64
#endif
#include "synonyms.h"
#include <aio.h>
#include "local.h"

int
lio_listio64(int mode, aiocb64_t * const list[], int cnt, sigevent_t *sig)
{
	int	i;

	for (i = 0; i < cnt; i++) {
		list[i]->aio_oldoff = -1;
		list[i]->aio_64bit = 0xdeadbeaf;
		
	}
	return lio_listio(mode, (aiocb_t *const *)list, cnt, sig);
}
