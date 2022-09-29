/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/opt_data.c	1.3"
#ifdef __STDC__
	#pragma weak opterr = _opterr
	#pragma weak optind = _optind
	#pragma weak optopt = _optopt
	#pragma weak optarg = _optarg
#endif
#include "synonyms.h"
/*
 * Global variables
 * used in getopt
 */

int	opterr = 1;
int	optind = 1;
int	optopt = 0;
char	*optarg = 0;
