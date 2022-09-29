/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libnsl:nsl/_dummy.c	1.2"
#include <libc_synonyms.h>
#include <libnsl_synonyms.h>

#include "sys/errno.h"
#include "sys/tiuser.h"
#include "_import.h"

extern int t_errno;
extern int errno;

_dummy()
{
	t_errno = TSYSERR;
	errno = ENXIO;
	return(-1);
}
