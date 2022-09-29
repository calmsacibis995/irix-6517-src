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
	#pragma weak aio_suspend64 = _aio_suspend64
#endif
#include "synonyms.h"
#include <aio.h>
#include "local.h"

int
aio_suspend64(const aiocb64_t * const list[], int cnt, const timespec_t *ts)
{
	int		i;
	aiocb64_t	*a;

	for (i = 0; i < cnt; i++) {
		a = (aiocb64_t *)list[i];
		if (a) {
			a->aio_oldoff = -1;
			a->aio_64bit = 0xdeadbeef;
		}
	}
	return aio_suspend((const aiocb_t *const *)list, cnt, ts);
}
