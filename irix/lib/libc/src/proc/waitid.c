/**************************************************************************
 *									  *
 * 		 Copyright (C) 1991, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 1.6 $"
#ifdef __STDC__
	#pragma weak waitid = _waitid
#endif
#include "synonyms.h"
#include "mplib.h"
#include <stddef.h>
#include <wait.h>
#include "proc_extern.h"

int 
waitid(idtype_t idtype, id_t id, siginfo_t *info, int options)
{
	extern int __waitsys(idtype_t, id_t, siginfo_t *, int, struct rusage *);
	MTLIB_BLOCK_CNCL_RET( int, __waitsys(idtype, id, info, options, 0) );
}
