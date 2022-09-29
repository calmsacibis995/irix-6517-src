/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986,1990 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 1.11 $"

/*
 * Scan the directory dirname calling select to make a list of selected
 * directory entries then sort using qsort and compare routine dcomp.
 * Returns the number of entries and a pointer to a list of pointers to
 * struct direct (through namelist). Returns -1 if there were any errors.
 */

#ifdef __STDC__
	#pragma weak BSDscandir = _BSDscandir
	#pragma weak BSDalphasort = _BSDalphasort
#endif
#include "synonyms.h"

/*
 * The following are redefined in sys/dir.h
 */
#undef opendir
#undef readdir
#undef telldir
#undef seekdir
#undef closedir
#undef scandir
#undef alphasort

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/dir.h>
#include <bstring.h>		/* for prototyping */
#include <string.h>		/* for prototyping */
#include <stdlib.h>		/* for prototyping */

#define CHUNKSIZE 64

int
BSDscandir(const char *dirname, 
	struct direct *(*namelist[]), 
	int (*select)(struct direct *),
	int (*dcomp)(struct direct **, 
	struct direct **))
{
	register struct direct *d, *p, **names, **tnames;
	register int nitems, i;
	register char *cp1, *cp2;
	long arraysz;
	DIR *dirp;

	if ((dirp = BSDopendir(dirname)) == NULL)
		return(-1);

	arraysz = CHUNKSIZE;
	names = (struct direct **)malloc((size_t)(arraysz) * sizeof(p));
	if (names == NULL)
		return(-1);

	nitems = 0;
	while ((d = BSDreaddir(dirp)) != NULL) {
		if (select != NULL && !(*select)(d))
			continue;	/* just selected names */
		/*
		 * Make a minimum size copy of the data
		 */
		p = (struct direct *)malloc(DIRSIZ(d));
		if (p == NULL)
			goto freeall;
		p->d_ino = d->d_ino;
		p->d_reclen = d->d_reclen;
		p->d_namlen = d->d_namlen;
		for (cp1 = p->d_name, cp2 = d->d_name; *cp1++ = *cp2++; );
		/*
		 * Check to make sure the array has space left
		 * Rather than wire fs dependent stuff in here lets make it
		 * really independent.
		 */
		if (nitems >= arraysz) {
			arraysz += CHUNKSIZE;
			tnames = (struct direct **)malloc((size_t)(arraysz) * sizeof(p));
			if (tnames == NULL)
				goto freeall;
			bcopy(names, tnames, nitems * (int)(sizeof(p)));
			free(names);
			names = tnames;
		}
		names[nitems++] = p;
	}
	BSDclosedir(dirp);
	if (nitems && dcomp != NULL)
		qsort(names, (size_t)(nitems), sizeof(struct direct *), (int (*) (const void *, const void *))dcomp);
	*namelist = names;
	return(nitems);

freeall:
	for (i = 0; i < nitems; i++)
		free(names[i]);
	free(names);
	return(-1);
}

/*
 * Alphabetic order comparison routine for those who want it.
 */
int 
BSDalphasort(struct direct **d1, struct direct **d2)
{
	return(strcmp((*d1)->d_name, (*d2)->d_name));
}
