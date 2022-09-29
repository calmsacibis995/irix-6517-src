/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*LINTLIBRARY*/
#ident	"$Revision: 1.3 $"

#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <pkginfo.h>
#include <malloc.h>

extern char	*pkgdir;
extern void	logerr(char *, ...);

#define LSIZE	512
#define MALSIZ	16

char **
pkgalias(char *pkg)
{
	FILE	*fp;
	char	path[PATH_MAX], *pkginst, *lasttok;
	char	*mypkg, *myarch, *myvers, **pkglist;
	char	line[LSIZE];
	int	n, errflg;

	pkglist = (char **) calloc(MALSIZ, sizeof(char *));
	if(pkglist == NULL)
		return((char **) 0);

	(void) sprintf(path, "%s/%s/pkgmap", pkgdir, pkg);
	if((fp = fopen(path, "r")) == NULL)
		return((char **) 0);

	n = errflg = 0;
	while(fgets(line, LSIZE, fp)) {
		lasttok = NULL;
		mypkg = strtok_r(line, " \t\n", &lasttok);
		myarch = strtok_r(NULL, "( \t\n)", &lasttok);
		myvers = strtok_r(NULL, "\n", &lasttok);

		(void) fpkginst(NULL);
		pkginst = fpkginst(mypkg, myarch, myvers); 
		if(pkginst == NULL) {
			logerr("no package instance for [%s]", mypkg);
			errflg++;
			continue;
		}
		if(errflg)
			continue;

		pkglist[n] = strdup(pkginst);
		if((++n % MALSIZ) == 0) {
			pkglist = (char **) realloc(pkglist, 
				(n+MALSIZ)*sizeof(char *));
			if(pkglist == NULL)
				return((char **) 0);
		}
	}
	pkglist[n] = NULL;

	(void) fclose(fp);
	if(errflg) {
		while(n-- >= 0)
			free(pkglist[n]);
		free(pkglist);
		return((char **) 0);
	}
	return(pkglist);
}

