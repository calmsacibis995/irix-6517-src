 /*************************************************************************
 #									  *
 # 		 Copyright (C) 1989, Silicon Graphics, Inc.		  *
 #									  *
 #  These coded instructions, statements, and computer programs  contain  *
 #  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 #  are protected by Federal copyright law.  They  may  not be disclosed  *
 #  to  third  parties  or copied or duplicated in any form, in whole or  *
 #  in part, without the prior written consent of Silicon Graphics, Inc.  *
 #									  *
 #************************************************************************/

#ident "$Revision: 1.3 $"

#ifdef __STDC__
	#pragma weak gettimeofday = _gettimeofday
#endif
#include "synonyms.h"

#include <sys/time.h>
#include <stdarg.h>
#include "sys_extern.h"

/*  SVR4 style gettimeofday(2) */
/*VARARGS1*/
int					/* return 0 or -1 */
gettimeofday(struct timeval *tp, ...)
{
	return (tp!=0 ? BSD_getime(tp) : 0);
}
