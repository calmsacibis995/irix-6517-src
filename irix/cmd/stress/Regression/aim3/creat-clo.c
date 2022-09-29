/*
 * AIM Suite III v3.1.1
 * (C) Copyright AIM Technology Inc. 1986,87,88,89,90,91,92.
 * All Rights Reserved.
 */

#ifndef	lint
	static char sccs_id[] = { " @(#) creat-clo.c:3.2 5/30/92 20:18:33" };
#endif

#include "suite3.h"

creat_clo( argv, res )
char *argv;
Result *res;
{
	char *mktemp();
	register int n;
	register int  j;
	char fn4[80];

	n = 1000;

	if ( *argv )
		sprintf(fn4,"%s/%s", argv, TMPFILE2);
	else
		sprintf(fn4,"%s",TMPFILE2);
	mktemp(fn4);

	while (n--)  {
		if( (j = creat(fn4,0777))<0 || close(j)<0 ) {
			fprintf(stderr, "creat/close error\n");
			res->i++;
		}
	}
	unlink(fn4);
	return(j);
}
