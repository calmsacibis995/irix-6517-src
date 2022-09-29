#ifdef __STDC__
	#pragma weak setnetgrent = _setnetgrent
	#pragma weak endnetgrent = _endnetgrent
	#pragma weak getnetgrent = _getnetgrent
#endif
#include "synonyms.h"

#if !defined(lint) && defined(SCCSIDS)
static	char sccsid[] = "@(#)getnetgrent.c	1.1 88/03/22 4.0NFSSRC; from 1.22 88/02/08 Copyr 1985 Sun Micro";
#endif

/* 
 * Copyright (c) 1985 by Sun Microsystems, Inc.
 */

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <strings.h>
#include <rpc/rpc.h>			/* required for prototyping */
#include <rpcsvc/yp_prot.h>		/* required for prototyping */
#include <rpcsvc/ypclnt.h>
#include <netdb.h>
#include <stddef.h>

#define MAXGROUPLEN 1024

/* 
 * access members of a netgroup
 */

static struct grouplist {		/* also used by pwlib */
	char	*gl_machine;
	char	*gl_name;
	char	*gl_domain;
	struct	grouplist *gl_nxt;
} *grouplist _INITBSS, *grlist _INITBSS;

struct list {			/* list of names to check for loops */
	char *name;
	struct list *nxt;
};

#define NETGROUP "netgroup"

static void doit(char *, struct list *);
static char *fill(char *, char **, char);
static char *match(char *);

void
setnetgrent(char *grp)
{
#ifdef _LIBC_NONSHARED
	static char oldgrp[256];
#else
	static char *oldgrp = NULL;
	
	if ((oldgrp == NULL) && ((oldgrp = calloc(1, 256)) == NULL))
		return;
#endif

	if (strcmp(oldgrp, grp) == 0 && grouplist)
		grlist = grouplist;
	else {
		if (grouplist != NULL)
			endnetgrent();
		if (_yellowup(1)) {	/* else should we read /etc/netgroup? */
			doit(grp, (struct list *) NULL);
			grlist = grouplist;
			(void) strcpy(oldgrp, grp);
		}
	}
}

void
endnetgrent(void)
{
	register struct grouplist *gl;
	
	if (grouplist) {
	    for (gl = grouplist; gl != NULL; gl = gl->gl_nxt) {
		if (gl->gl_name)
			free(gl->gl_name);
		if (gl->gl_domain)
			free(gl->gl_domain);
		if (gl->gl_machine)
			free(gl->gl_machine);
		free((char *) gl);
	    }
	    grouplist = NULL;
	}
	if (grlist) {
	    grlist = NULL;
	}
}

int
getnetgrent(char **machinep, char **namep, char **domainp)
{
	if (grlist == 0)
		return (0);
	*machinep = grlist->gl_machine;
	*namep = grlist->gl_name;
	*domainp = grlist->gl_domain;
	grlist = grlist->gl_nxt;
	return (1);
}

/*
 * recursive function to find the members of netgroup "group". "list" is
 * the path followed through the netgroups so far, to check for cycles.
 */
static void
doit(char *group, struct list *list)
{
	register char *p, *q;
	register struct list *ls;
	struct list this_group;
	char *val;
	struct grouplist *gpls;
 
	/*
	 * check for non-existing groups
	 */
	if ((val = match(group)) == NULL)
		return;
 
	/*
	 * check for cycles
	 */
	for (ls = list; ls != NULL; ls = ls->nxt)
		if (strcmp(ls->name, group) == 0) { /* cycle detected */
			free(val);
			return;
		}
 
 
    	ls = &this_group;
    	ls->name = group;
    	ls->nxt = list;
    	list = ls;
    
	p = val;
	while (p != NULL) {
		while (*p == ' ' || *p == '\t')
			p++;
		if (*p == 0 || *p =='#')
			break;
		if (*p == '(') {
			p++;
			gpls = (struct grouplist *)
			    malloc(sizeof(struct grouplist));
			if (gpls == NULL) {
				break;
			}
			if (!(p = fill(p,&gpls->gl_machine,',')))
				goto fill_error;
			if (!(p = fill(p,&gpls->gl_name,',')))
				goto fill_error;
			if (!(p = fill(p,&gpls->gl_domain,')')))
				goto fill_error;
			gpls->gl_nxt = grouplist;
			grouplist = gpls;
		} else {
			for (q = p; !isspace(*q) && *q != '#'; q++)
				;
			if (q && *q == '#')
				break;
			*q = 0;
			doit(p,list);
			*q = ' ';
		}
		p = strpbrk(p, " \t");
    	}
	free(val);
    	return;
 
fill_error:
	if (gpls != NULL) {
		if (gpls->gl_machine)
			free(gpls->gl_machine);
		if (gpls->gl_name)
			free(gpls->gl_name);
		if (gpls->gl_domain)
			free(gpls->gl_domain);
		free((char *) gpls);
	}
	free(val);
    	return;
}

/*
 * Fill a buffer "target" selectively from buffer "start".
 * "termchar" terminates the information in start, and preceding
 * or trailing white space is ignored. The location just after the
 * terminating character is returned.  
 */
static char *
fill(char *start, char **target, char termchar)
{
	register char *p, *q; 
	char *r;
	ptrdiff_t size;
 
	for (p = start; *p == ' ' || *p == '\t'; p++)
		;
	r = index(p, termchar);
	if (r == NULL) {	/* syntax error */
		return (NULL);
	}
	if (p == r)
		*target = NULL;	
	else {
		for (q = r-1; *q == ' ' || *q == '\t'; q--)
			;
		size = (ptrdiff_t)(q - p + 1);
		*target = malloc((size_t)size+1);
		if (*target == NULL) {
			return(NULL);
		}
		(void) strncpy(*target, p, (size_t)size);
		(*target)[size] = 0;
	}
	return (r+1);
}

static char *
match(char *group)
{
	char *val;
	int vallen;
	int err;

	err = yp_match(_yp_domain,NETGROUP,group, (int)strlen(group),&val,&vallen);
	if (err) {
#ifdef DEBUG
		(void) fprintf(stderr,"yp_match(netgroup,%s) failed: %s\n",group
				,yperr_string(err));
#endif
		return(NULL);
	}
	return(val);
}
