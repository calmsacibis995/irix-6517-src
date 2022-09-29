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
#ident "$Revision: 1.12 $"

/*
 * Scan the directory dirname calling select to make a list of selected
 * directory entries then sort using qsort and compare routine dcomp.
 * Returns the number of entries and a pointer to a list of pointers to
 * struct dirent (through namelist). Returns -1 if there were any errors.
 */

#include <sgidefs.h>
#ifdef __STDC__
	#pragma weak scandir = _scandir
	#pragma weak alphasort = _alphasort
#if (_MIPS_SIM == _MIPS_SIM_ABI64 || _MIPS_SIM == _MIPS_SIM_NABI32)
	#pragma weak scandir64 = _scandir
	#pragma weak _scandir64 = _scandir
	#pragma weak alphasort64 = _alphasort
	#pragma weak _alphasort64 = _alphasort
#endif
#endif
#include "synonyms.h"

#include "shlib.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <bstring.h>
#include <unistd.h>

#define CHUNKSIZE 64

int
scandir(const char *dirname, struct dirent *(*namelist[]),
	int (*select)(struct dirent *),
	int (*dcomp)(struct dirent **, struct dirent **))
{
	register struct dirent *d, *p, **names, **tnames;
	register int nitems, i;
	register char *cp1, *cp2;
	int arraysz;
	DIR *dirp;

	if ((dirp = opendir(dirname)) == NULL)
		return(-1);

	arraysz = CHUNKSIZE;
	names = (struct dirent **)malloc((size_t)(arraysz) * sizeof(p));
	if (names == NULL)
		return(-1);

	nitems = 0;
	while ((d = readdir(dirp)) != NULL) {
		if (select != NULL && !(*select)(d))
			continue;	/* just selected names */
		/*
		 * Make a minimum size copy of the data
		 */
		p = (struct dirent *)malloc(d->d_reclen);
		if (p == NULL)
			goto freeall;
		p->d_ino = d->d_ino;
		p->d_off = d->d_off;
		p->d_reclen = d->d_reclen;
		for (cp1 = p->d_name, cp2 = d->d_name; *cp1++ = *cp2++; );
		/*
		 * Check to make sure the array has space left
		 * Rather than wire fs dependent stuff in here lets make it
		 * really independent.
		 */
		if (nitems >= arraysz) {
			arraysz += CHUNKSIZE;
			tnames = (struct dirent **)malloc((size_t)(arraysz) * sizeof(p));
			if (tnames == NULL)
				goto freeall;
			bcopy(names, tnames, nitems * (int)sizeof(p));
			free(names);
			names = tnames;
		}
		names[nitems++] = p;
	}
	closedir(dirp);
	if (nitems && dcomp != NULL)
		qsort(names, (size_t)nitems, sizeof(struct dirent *),
		      (int (*)(const void *, const void *))dcomp);
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
alphasort(struct dirent **d1, struct dirent **d2)
{
	return(strcmp((*d1)->d_name, (*d2)->d_name));
}
