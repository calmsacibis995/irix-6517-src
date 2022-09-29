/*
 * gethostwrap.c --
 *
 *	Wrapper routines for gethostbyname and gethostbyaddr to allow the user
 *	to specify the query order of the NIS, DNS and file-base routines.
 *	The ordering can be changed from the default by specifying the order
 *	in a system-wide configuation file or from an process-specific
 *	environment variable or with the sethostresorder() routine.
 *
 *
 * Copyright 1990, Silicon Graphics, Inc. 
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or 
 * duplicated in any form, in whole or in part, without the prior written 
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions 
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or 
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished - 
 * rights reserved under the Copyright Laws of the United States.
 *
 * $Revision: 2.5 $
 */

#ifdef __STDC__
	#pragma weak gethostbyname = _gethostbyname
	#pragma weak gethostbyaddr = _gethostbyaddr
	#pragma weak sethostresorder = _sethostresorder
#endif
#include "synonyms.h"

#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <bstring.h>
#include <stdlib.h>	/* prototype for getenv() */
#include <ctype.h>	/* prototype for isdigit() */
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h> 	/* prototype for res_init() */
#include <alloca.h>
#include "net_extern.h"

/*
 * The query ordering can be specified with an environment variable.
 */
#define	ENVNAME	"HOSTRESORDER"

#define NUM_SERVICES 3		/* NIS, DNS, file */

/*
 * The HostFuncs array contains the current ordering of gethostby* routines.
 * When the authoritative flag for an entry is true, it means the next funcs 
 * routine is tried only if this entry's routine is not enabled (e.g., NIS
 * or named is not configured for the system). If false, the next entry is 
 * tried if this entry didn't find the name/address in its database.
 *
 * Default ordering:
 * 			nis / bind / local
 *
 * The / means the preceding routine is authoritative.
 */

typedef struct {
    struct hostent	*(*byname)(const char *name);
    struct hostent	*(*byaddr)(const char *addr, int len, int type);
    char		authoritative;	/* stop even if can't find an answer. */
} HostFuncs;

static HostFuncs funcs[NUM_SERVICES] = {
    { _gethostbyname_yp, _gethostbyaddr_yp, 1},
    { _gethostbyname_named, _gethostbyaddr_named, 1},
    { _gethtbyname, _gethtbyaddr, 0},
};

/* True if sethostresorder() or _resorderinit() have been called. */
static char initialized = 0;



/*
 * sethostresorder --
 * _setresordsubr --
 *
 *	Parse a string containing the host service ordering and update
 *	the functions array. The tokens are separated by spaces, colons,
 *	tabs or newlines. Unknown tokens are ignored.
 *	
 *	Tokens are specify NIS, DNS and /etc/hosts versions of 
 *	gethostby{name,addr}. The token / means the previous entry 
 *	is considered authoritative even if it returns with no answer.
 *	
 *	Returns 0 if the array was modified, otherwise -1.
 */

static char separators[] = " :\t\n";

int
_setresordsubr(const char *s)
{
    register char *token, *c;
    register int i = 0;
    HostFuncs new_funcs[NUM_SERVICES];
    char *str;		/* strtok modifies its argument */
	char *lasttok = NULL;

    bzero((char *)new_funcs, sizeof(new_funcs));
    str = (char *)alloca((int)strlen(s) + 1);
    strcpy(str, s);

    token = strtok_r(str, separators, &lasttok);
    while (token != NULL && i < NUM_SERVICES) {
	if (*token == '/') {
	    if (i > 0)
		new_funcs[i-1].authoritative = 1;

	    /* Look for the next keyword, skipping extra /'s */
	    while (*++token == '/')
		;
	    if (*token == '\0') {
		token = strtok_r(NULL, separators, &lasttok);
		continue;
	    }
	    /* fall through */
	} 
	if ((c = strchr(token, '/')) != NULL) {		/* trailing / */
	    *c = '\0';
	    new_funcs[i].authoritative = 1;
	}
	if (!strcasecmp(token, "dns") || !strcasecmp(token, "bind")) {
	    new_funcs[i].byname = _gethostbyname_named;
	    new_funcs[i].byaddr = _gethostbyaddr_named;
	    i++;
	} else if (!strcasecmp(token, "local") || !strcasecmp(token, "file")) {
	    new_funcs[i].byname = _gethtbyname;
	    new_funcs[i].byaddr = _gethtbyaddr;
	    i++;
	} else if (!strcasecmp(token, "nis") || !strcasecmp(token, "yp")) {
	    new_funcs[i].byname = _gethostbyname_yp;
	    new_funcs[i].byaddr = _gethostbyaddr_yp;
	    i++;
#if 0
	} else {
	    fprintf(stderr, 
		    "_setresordsubr: unknown token: %s\n", token);
#endif
	}
	token = strtok_r(NULL, separators, &lasttok);
    }
    if (i > 0) {
	bcopy(new_funcs, funcs, sizeof(funcs));
	return 0;
    }
    return -1;
}

int
sethostresorder(const char *str)
{
    /*
     * Initialize things to enable this routine to override the ordering in 
     * resolv.conf or the environment variable.
     */
    if (!initialized) {
	(void) res_init();
	initialized = 1;
    }
    return _setresordsubr(str);
}



/*
 * Set up default service ordering from the configuration file or the
 * environment variable.  The environment ordering has precedence.  
 */


static void
_resorderinit(void)
{
    char *cp;

    (void) res_init();		/* look for special entry in resolv.conf */

    /*
     * Environment variable format: "type1 [ [/] type2 ...]"
     */
    cp = getenv(ENVNAME);
    if (cp != NULL)
	(void) _setresordsubr(cp);

    initialized = 1;
}


struct hostent *
gethostbyname(const char *name)
{
    struct hostent *h;
    register HostFuncs *f;
    char *hasdot;
    char *alias = NULL;

    /* resolv.conf or the env. variable may change the funcs table. */
    if (!initialized)
	_resorderinit();

    /* 
     * If the name looks like a valid Internet address, return a hostent for it.
     */
    if (isdigit(name[0])) {
	    register char *cp;

	    for (cp = (char *)name;; ++cp) {
		    if (!*cp) {
			    if (*--cp == '.')
				    break;
			    return(_inet_atohtent(name));
		    }
		    if (!isdigit(*cp) && *cp != '.') 
			    break;
	    }
    }

    /*
     * See if there's an alias for a dotless hostname using res_search.c's 
     * hostalias(). Treat _gethostbyname_named specially due to res_search()'s
     * handling of aliases and names with dots.
     *
     * N.B. hostalias may be called O(#byname funcs) times if there's no match.
     */
    hasdot = strchr(name, '.');
    for (f = funcs; f < &funcs[NUM_SERVICES] && f->byname != NULL; f++) {
	if (f->byname != _gethostbyname_named && !hasdot &&
	    (alias != NULL || (alias = __hostalias((char *)name))))
	    h = f->byname(alias);
	else
	    h = f->byname(name);
	if (h != NULL)
	    return h;
	if (h_errno != __HOST_SVC_NOT_AVAIL && f->authoritative)
	    break;
    }
    if (h_errno == __HOST_SVC_NOT_AVAIL)
	h_errno = HOST_NOT_FOUND;
    return NULL;
}


struct hostent *
gethostbyaddr(const void *addr, int len, int type)
{
    struct hostent *h;
    register HostFuncs *f;

    if (!initialized)
	_resorderinit();

    for (f = funcs; f < &funcs[NUM_SERVICES] && f->byaddr != NULL; f++) {
	h = f->byaddr(addr, len, type);
	if (h != NULL)
	    return h;
	if (h_errno != __HOST_SVC_NOT_AVAIL && f->authoritative)
	    break;
    }
    if (h_errno == __HOST_SVC_NOT_AVAIL)
	h_errno = HOST_NOT_FOUND;
    return NULL;
}
