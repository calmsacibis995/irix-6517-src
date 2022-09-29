/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 1.4 $"

/*LINTLIBRARY*/

/*
 *  memory.c
 *	sysmem()	Get the amount of memory on the system
 *	asysmem()	Get the amount of available memory on the system
 */

#include	<sys/types.h>
#include	<sys/types.h>
#include	<sys/param.h>
#include	<sys/sysinfo.h>
#include	<sys/sysmp.h>
#include	"libadm.h"


/*
 * sysmem()
 *
 *	Return the amount of memory configured on the system.
 */

long 
sysmem(void)
{
	struct rminfo           rminfo;
	static int              rminfosz;

	rminfosz = sysmp(MP_SASZ, MPSA_RMINFO);
	sysmp(MP_SAGET, MPSA_RMINFO, (char *) &rminfo, rminfosz);
	return((long) rminfo.physmem);
}


/*
 * int asysmem()
 *
 *	This function returns the amount of available memory on the system.
 *	This is defined by
 *
 */
long
asysmem(void)
{
	struct rminfo           rminfo;
	static int              rminfosz;

	rminfosz = sysmp(MP_SASZ, MPSA_RMINFO);
	sysmp(MP_SAGET, MPSA_RMINFO, (char *) &rminfo, rminfosz);
	return((long) rminfo.freemem);
}

