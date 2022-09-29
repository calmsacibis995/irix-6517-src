#ifdef __STDC__
	#pragma weak innetgr = _innetgr
#endif
#include "synonyms.h"

#if !defined(lint) && defined(SCCSIDS)
static	char sccsid[] = "@(#)innetgr.c	1.1 88/03/22 4.0NFSSRC; from 1.17 88/02/08 SMI Copyr 1985 Sun Micro";
#endif

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <rpc/rpc.h>			/* required for prototyping */
#include <rpcsvc/yp_prot.h>		/* required for prototyping */
#include <rpcsvc/ypclnt.h>
#include <strings.h>

/* 
 * innetgr: test whether I'm in the NIS maps based on /etc/netgroup
 *
 */

static char *name _INITBSS, *machine _INITBSS, *domain _INITBSS;
#define	LISTSIZE 50		/* recursion limit (50 is sufficient) */
#define MLOWERSZ 256
static char **list _INITBSS;
static char **listp _INITBSS;	/* pointer into list */

static int doit(char *);
static int inlist(char *, char *);
static int lookup(char *, char *, char *, char *, int *);
static void makekey(char *, char *, char *);

int
innetgr(char *grp, char *mach, char *nm, char *dom)
{
	int res = 0;
	char mlower[MLOWERSZ];

	if (!_yellowup(0)) {
		return(0);      /* should we read /etc/netgroup? */
	}
	if ( mach ) {
		while ( (mach[res] != '\0') && (res < (MLOWERSZ-1)) ) {
			mlower[res] = tolower(mach[res]);
			res += 1;
		}
	}
	mlower[res] = '\0';
	machine = (mach == NULL) ? NULL : mlower;
	name = nm;
	domain = dom;
	if (domain) {
		if (name && !machine) {
			if (lookup("netgroup.byuser",grp,name,domain,&res)) {
				return(res);
			}
		} else if (machine && !name) {
			if (lookup("netgroup.byhost",grp,machine,domain,&res)) {
				return(res);
			}
		}
	}
	if ((list == 0) &&
	    (list = (char **)calloc(1, LISTSIZE * sizeof (char *))) == 0) {
		return(0);
	}
	listp = list;
	return doit(grp);
}
	
/* 
 * calls itself recursively
 */
static int
doit(char *group)
{
	char *key, *val;
	int vallen, keylen;
	char *r;
	int match;
	register char *p, *q;
	register char **lp;
	int err;
	
	*listp++ = group;
	if (listp > list + LISTSIZE) {
		listp--;
		return (0);
	}
	key = group;
	keylen = (int)strlen(group);
	err = yp_match(_yp_domain, "netgroup", key, keylen, &val, &vallen);
	if (err) {
#ifdef DEBUG
		if (err == YPERR_KEY)
			(void) fprintf(stderr,
			    "innetgr: no such netgroup as %s\n", group);
		else
			(void) fprintf(stderr, "innetgr: yp_match, %s\n",yperr_string(err));
#endif
		listp--;
		return(0);
	}
	/* 
	 * check for recursive loops
	 */
	for (lp = list; lp < listp-1; lp++)
		if (strcmp(*lp, group) == 0) {
			listp--;
			free(val);
			return(0);
		}
	
	p = val;
	p[vallen] = 0;
	while (p != NULL) {
		match = 0;
		while (isspace(*p))
			p++;
		if (*p == 0 || *p == '#')
			break;
		if (*p == '(') {
			p++;
			while (isspace(*p))
				p++;
			r = q = index(p, ',');
			if (q == NULL) {
				listp--;
				free(val);
				return(0);
			}
			if (p == q || machine == NULL)
				match++;
			else {
				while (isspace(q[-1]))
					q--;
				*q = '\0';
				if (strcasecmp(machine, p) == 0)
					match++;
			}
			p = r+1;

			while (isspace(*p))
				p++;
			r = q = index(p, ',');
			if (q == NULL) {
				listp--;
				free(val);
				return(0);
			}
			if (p == q || name == NULL)
				match++;
			else {
				while (isspace(q[-1]))
					q--;
				*q = '\0';
				if (strcmp(name, p) == 0)
					match++;
			}
			p = r+1;

			while (isspace(*p))
				p++;
			r = q = index(p, ')');
			if (q == NULL) {
				listp--;
				free(val);
				return(0);
			}
			if (p == q || domain == NULL)
				match++;
			else {
				while (isspace(q[-1]))
					q--;
				*q = '\0';
				if (strcasecmp(domain, p) == 0)
					match++;
			}
			p = r+1;
			if (match == 3) {
				free(val);
				listp--;
				return 1;
			}
		}
		else {
			q = strpbrk(p, " \t\n#");
			if (q && *q == '#')
				break;
			if (q)
				*q = 0;
			if (doit(p)) {
				free(val);
				listp--;
				return 1;
			}
			if (q)
				*q = ' ';
		}
		while (*p != 0 && !isspace(*p))
			p++;
	}
	free(val);
	listp--;
	return 0;
}

/*
 * return 1 if "what" is in the comma-separated, newline-terminated "list"
 */
static int
inlist(char *what, char *list)
{
#	define TERMINATOR(c)    (c == ',' || c == '\n')

	register char *p;
	unsigned int len;
         
	len = (int)strlen(what);     
	p = list;
	do {             
		if (strncmp(what,p,len) == 0 && TERMINATOR(p[len])) {
			return(1);
		}
		while (!TERMINATOR(*p)) {
			p++;
		}
		p++;
	} while (*p);
	return(0);
}

/*
 * Lookup a host or user name in a NIS map.  Set result to 1 if group in the 
 * lookup list of groups. Return 1 if the map was found.
 */
static int
lookup(char *map, char *group, char *name, char *domain, int *res)
{
	int err;
	char *val;
	int vallen;
	char key[256];
	char *wild = "*";
	int i;

	for (i = 0; i < 4; i++) {
		switch (i) {
		case 0: makekey(key,name,domain); break;
		case 1: makekey(key,wild,domain); break;	
		case 2: makekey(key,name,wild); break;
		case 3: makekey(key,wild,wild); break;	
		}
		err  = yp_match(_yp_domain,map,key,(int)strlen(key),&val,&vallen); 
		if (!err) {
			*res = inlist(group,val);
			free(val);
			if (*res) {
				return(1);
			}
		} else {
#ifdef DEBUG
			(void) fprintf(stderr,
				"yp_match(%s,%s) failed: %s.\n",map,key,yperr_string(err));
#endif
			if (err != YPERR_KEY)  {
				return(0);
			}
		}
	}
	*res = 0;
	return(1);
}



/*
 * Generate a key for a netgroup.byXXXX NIS map
 */
static void
makekey(register char *key, register char *name, register char *domain)
{
	while (*key++ = *name++)
		;
	*(key-1) = '.';
	while (*key++ = *domain++)
		;
}	
