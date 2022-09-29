/*
 * |-----------------------------------------------------------|
 * | Copyright (c) 1991, 1990 MIPS Computer Systems, Inc.      |
 * | All Rights Reserved                                       |
 * |-----------------------------------------------------------|
 * |          Restricted Rights Legend                         |
 * | Use, duplication, or disclosure by the Government is      |
 * | subject to restrictions as set forth in                   |
 * | subparagraph (c)(1)(ii) of the Rights in Technical        |
 * | Data and Computer Software Clause of DFARS 252.227-7013.  |
 * |         MIPS Computer Systems, Inc.                       |
 * |         950 DeGuigne Avenue                               |
 * |         Sunnyvale, California 94088-3650, USA             |
 * |-----------------------------------------------------------|
 */
#ident	"$Id: abi_getgrent.c,v 1.3 1995/08/31 21:04:10 jwag Exp $"
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*	3.0 SID #	1.2	*/
/*LINTLIBRARY*/
#include "synonyms.h"
/*
 * We don't want these symbols resolving undefs from the ABI .so.
 * All these symbols are part of the archive only.
 */
#undef setgrent
#undef endgrent
#undef getgrent
#undef fgetgrent
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <grp.h>

#define	CL	':'
#define	CM	','
#define	NL	'\n'
#define TRUE	1
#define FALSE	0

static const char *GROUP = "/etc/group";
static FILE *grf = NULL;
static char *line, *gr_mem;
static int size, gr_size;
struct group *fgetgrent(FILE *f);

void
setgrent(void)
{
	if (grf == NULL)
		grf = fopen(GROUP, "r");
	else
		rewind(grf);
}

void
endgrent(void)
{
	if (grf != NULL)
		{
		(void) fclose(grf);
		grf = NULL;
		}
}

static void
cleanup(void)
{
	if (line != NULL)
		{
		free(line);
		line = NULL;
		}
	if (gr_mem != NULL)
		{
		free (gr_mem);
		gr_mem = NULL;
		}
	(void) endgrent();
}

static char *
grskip(char *p, int c)
{
	while (*p != '\0' && *p != c)
		++p;
	if (*p != '\0')
	 	*p++ = '\0';
	return(p);
}

struct group *
getgrent(void)
{
	if (grf == NULL && (grf = fopen(GROUP, "r")) == NULL)
		return(NULL);
	return (fgetgrent(grf));
}

struct group *
fgetgrent(FILE *f)
{
	char *p, **q;
	int len, count;
	long offset;
	char done;

	static struct group *grp;

	if (!grp && (grp = (struct group *)malloc(sizeof(struct group))) == NULL)
		return NULL;
	count = 1;
	if (line == NULL)
		{
		size = BUFSIZ+1;
		if ((line = calloc((unsigned)size, sizeof(char))) == NULL)
			{
			(void) cleanup();
			return(NULL);
			}
		}
	done = FALSE;
	while (!done)
		{
		offset = ftell(f);
		if ((p = fgets(line, size, f)) == NULL)
			return(NULL);
		len = strlen(p);
		if ((len <= size) && (p[len-1] == NL))
			done = TRUE;
		else
			{
			size *= 32;
			if ((line = realloc(line, (unsigned)size * sizeof(char))) == NULL)
				{
				(void) cleanup();
				return(NULL);
				}
			fseek(f, offset, 0);
			}
		}
	grp->gr_name = p;
	grp->gr_passwd = p = grskip(p, CL);
	grp->gr_gid = atol(p = grskip(p, CL));
	p = grskip(p, CL);
	(void) grskip(p, NL);
	if (gr_mem == NULL)
		{
		gr_size = 2;
		if ((gr_mem = calloc((unsigned)(gr_size), sizeof(char *))) == NULL)
			{
			(void) cleanup();
			return(NULL);
			}
		}
		grp->gr_mem = (char **)(VOID *)gr_mem;
		q = grp->gr_mem;
		while (*p != '\0')
			{
			if (count >= gr_size - 1)
				{
				*q = NULL;
				gr_size *= 2;
				if ((gr_mem = realloc(gr_mem, (unsigned)(gr_size) * sizeof(char *))) == NULL)
					{
					(void) cleanup();
					return(NULL);
					}
				grp->gr_mem = (char **)(VOID *)gr_mem;
				q = grp->gr_mem;
				while (*q != NULL)
					q++;
				}
			count++;
			*q++ = p;
			p = grskip(p, CM);
			}
		*q = NULL;
		return(grp);
}
