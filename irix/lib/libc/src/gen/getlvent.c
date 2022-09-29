
/**************************************************************************
 *									  *
 * 		 Copyright (C) 1988, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.6 $"

/* functions for parsing /etc/lvtab */

/* ##mowat: NOTE THIS FILE IS NO LONGER USED!  Only lv_to_xlv(1) is
 * supported any longer.  These entry points are stubbed out.
 */
#ifdef __STDC__
	#pragma weak freelvent = _freelvent
	#pragma weak getlvent = _getlvent
#endif
#include "synonyms.h"
#include <stddef.h>
#include <stdio.h>

/*ARGSUSED*/
void
freelvent(void *tabent)
{
}

/*ARGSUSED*/
void *
getlvent(void *lvtabp)
{
	return NULL;
}
