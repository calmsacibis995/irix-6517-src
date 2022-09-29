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
#ident  "$Revision: 1.1 $ $Author: doucette $"

#ifdef __STDC__
        #pragma weak aio_sgi_init64 = _aio_sgi_init64
#endif
#include "synonyms.h"
#include <aio.h>

void
aio_sgi_init64(struct aioinit *init)
{
	aio_sgi_init(init);
}
