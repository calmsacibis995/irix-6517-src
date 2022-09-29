/* @(#)where_main.c	1.2 87/08/13 3.2/4.3NFSSRC */
#include <stdio.h>
#include <sys/param.h>

main(argc, argv)
	char **argv;
{
	char host[MAXPATHLEN];
	char fsname[MAXPATHLEN];
	char within[MAXPATHLEN];
	char *pn;
	int many;

	many = argc > 2;
	while (--argc > 0) {
		pn = *++argv;
		where(pn, host, fsname, within);
		if (many)
			printf("%s:\t", pn);
		printf("%s:%s%s\n", host, fsname, within);
	}
}


/* 
 * NFSSRC 3.2/4.3 for the VAX*
 * Copyright (C) 1987 Sun Microsystems, Inc.
 * 
 * (*)VAX is a trademark of Digital Equipment Corporation
 */
