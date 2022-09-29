/*************************************************************************
 *									  *
 * 		 Copyright (C) 1989, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 *************************************************************************/

#ident "$Revision: 1.11 $"

#ifdef __STDC__
	#pragma weak pathconf = _pathconf
	#pragma weak fpathconf = _fpathconf
#endif
#include "synonyms.h"

#include <sys/types.h>
#include <sys/syssgi.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>

/* Wrapper for the POSIX pathconf() system call, added by SGI */

long
pathconf(path,name)
  const char *path;
  int name;
{
	return(syssgi(SGI_PATHCONF,path,name,PATHCONF));
}

long
fpathconf(fildes,name)
  int fildes,name;
{
	return(syssgi(SGI_PATHCONF,fildes,name,FPATHCONF));
}
