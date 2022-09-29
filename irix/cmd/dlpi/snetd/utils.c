/*
 *  SpiderTCP Network Daemon
 *
 *      Utility Routines
 *
 *  Copyright 1991  Spider Systems Limited
 *
 *	 /projects/common/PBRAIN/SCCS/pbrainF/dev/src/etc/netd/0/s.utils.c
 *	@(#)utils.c	1.7
 *
 *	Last delta created	17:06:19 2/4/91
 *	This file extracted	14:52:27 11/13/92
 *
 */

#ident "@(#)utils.c	1.7	11/13/92"

#include <stdio.h>
#include "debug.h"
#include "utils.h"

char *
memory(size)
unsigned size;
{
	extern char	*malloc();
	register char	*p;

	if ((p = malloc(size)) == (char *) 0)
	{
		error("out of memory");
		exit(1);
	}

	return p;
}

char *
array(n, size)
int	n;
unsigned size;
{
	extern char	*calloc();
	register char	*p;

	if ((p = calloc(n, size)) == (char *) 0)
	{
		error("out of memory");
		exit(1);
	}

	return(p);
}

char *
mfree(p)
char *p;
{
	if (p)
		free(p);

	return((char *) 0);
}

char *
copy(s)
char *s;
{
	char	*new, *malloc();

	if ((new = malloc(strlen(s)+1)) == NULL)
		exit(error("out of memory"));
	
	strcpy(new, s);
	return(new);
}

list_t
mklist(s)
char	*s;
{
	list_t	l = (list_t) memory(sizeof(struct list));

	l->data = s;
	l->next = L_NULL;

	return l;
}
