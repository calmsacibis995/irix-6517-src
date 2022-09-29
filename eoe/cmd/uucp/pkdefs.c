/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident "$Revision: 1.3 $"

#include "uucp.h"

#define USER 1
#include "pk.h"
char next[8]	={ 1,2,3,4,5,6,7,0};	/* packet sequence numbers */
char mask[8]	={ 1,2,4,010,020,040,0100,0200 };
int	pkactive;
int pkdebug;
int pksizes[] = {
	1, 32, 64, 128, 256, 512, 1024, 2048, 4096, 1
};
struct pack *pklines[NPLINES];
