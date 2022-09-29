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
	#pragma weak aio_hold64 = _aio_hold64
#endif
#include "synonyms.h"
#include <aio.h>

int 
aio_hold64(int shouldhold)
{
	return aio_hold(shouldhold);
}
