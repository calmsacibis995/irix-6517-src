/*
 * AIM Suite III v3.1.1
 * (C) Copyright AIM Technology Inc. 1986,87,88,89,90,91,92.
 * All Rights Reserved.
 */

#ifndef	lint
	static char sccs_id[] = { " @(#) lptest.c:3.2 5/30/92 20:18:41" };
#endif

#include <stdio.h>
main(argc,argv)
int argc;
char **argv;
{
	char prcmd[80];

	if (argc > 1)
		sprintf(prcmd,"cat multiuser.c | %s",argv[1]);
	else
		sprintf(prcmd,"cat multiuser.c | lp");
	system(prcmd);
}
