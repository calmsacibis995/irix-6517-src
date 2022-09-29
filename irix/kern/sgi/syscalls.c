/**************************************************************************
 *									  *
 * 		 Copyright (C) 1990, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 4.91 $"

#include <sys/types.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/par.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/ksignal.h>
#include <sys/kopt.h>
#include <procfs/prsystm.h>
#include <string.h>
#include <sys/schedctl.h>
#include <sys/timers.h>


/*
 * libc usage: "bytes = sgikopt(option, buf, buflen);
 *	check if incoming option is legal.  If so, copy the kernel 
 *	argument list into buf.  Insure that no more than buflen 
 *	characters of space is used.  Each string copied out has a
 *	null byte at the end.
 */

struct sgikopta {
	char		*instring;
	char		*outstring;
	sysarg_t	len;
};

/*ARGSUSED*/
int
sgikopt(struct sgikopta *uap, rval_t *rvp)
{
	int error;
	char envname[80];
	size_t size;
	char *envptr;

	if (error = copyinstr(uap->instring, envname, sizeof envname, &size))
		return error;
	if ((envptr = kopt_find(envname)) == 0)
		return EINVAL;
	size = min(strlen(envptr), uap->len - 1);
	if (copyout(envptr, uap->outstring, size))
		return EFAULT;
	if (subyte(uap->outstring + size, 0) == -1)
		return EFAULT;
	return 0;
}
