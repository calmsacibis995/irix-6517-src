/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.6 $"
#include "stdio.h"
#include "sys/types.h"
#include "sys/prctl.h"
#include "stdlib.h"
#include "unistd.h"
#include "wait.h"
#include "errno.h"

/* test sproc(0) a little */

/* ARGSUSED */
int
main(int argc, char **argv)
{
	char *a;
	void slave();
	int i;

	a = malloc(1024*1024*16);

	if (a == NULL)		/* not enough memory, just exit */
		return 0;

	for (i = 0; i < 1024*1024*16; i += 1024*64)
		a[i] = 1;
	

	sproc(slave, 0, 0);

	wait(0);
	i = oserror();

	sproc(slave, 0, 0);
	return 0;
}

void
slave()
{
	exit(0);
}
