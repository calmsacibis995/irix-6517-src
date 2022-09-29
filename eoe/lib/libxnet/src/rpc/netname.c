/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


#ident	"@(#)librpc:netname.c	1.3.4.1"

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*	PROPRIETARY NOTICE (Combined)
*
* This source code is unpublished proprietary information
* constituting, or derived under license from AT&T's UNIX(r) System V.
* In addition, portions of such source code were derived from Berkeley
* 4.3 BSD under license from the Regents of the University of
* California.
*
*
*
*	Copyright Notice 
*
* Notice of copyright on this source code product does not indicate 
*  publication.
*
*	(c) 1986,1987,1988.1989  Sun Microsystems, Inc
*	(c) 1983,1984,1985,1986,1987,1988,1989,1990  AT&T.
*	(c) 1990,1991  UNIX System Laboratories, Inc.
*          All rights reserved.
*/
 
/*
 * netname utility routines
 * convert from unix names (uid, gid) to network wide names and vice-versa
 * This module is operating system dependent!
 * What we define here will work with any unix system that has adopted
 * the Sun NIS domain architecture.
 */
#if defined(__STDC__) 
        #pragma weak getnetname		= _getnetname
        #pragma weak host2netname	= _host2netname
        #pragma weak user2netname	= _user2netname
#endif
#include <libc_synonyms.h>
#include <libnsl_synonyms.h>

#include <sys/param.h>
#include <rpc/rpc.h>
#include <ctype.h>
#include <string.h>

#ifndef MAXHOSTNAMELEN
#define	MAXHOSTNAMELEN 256
#endif
#ifndef NGROUPS
#define	NGROUPS 16
#endif

static char *OPSYS = "unix";
static char *NETID = "netid.byname";

/*
 * Figure out my fully qualified network name
 */
getnetname(char name[MAXNETNAMELEN+1])
{
	uid_t uid;

	uid = geteuid();
	if (uid == 0) {
		return (host2netname(name, (char *) NULL, (char *) NULL));
	} else {
		return (user2netname(name, uid, (char *) NULL));
	}
}


/*
 * Convert unix cred to network-name
 */
user2netname(char netname[MAXNETNAMELEN + 1], uid_t uid, const char *domain)
{
	char *dfltdom;

#define	MAXIPRINT	(11)	/* max length of printed integer */

	if (domain == NULL) {
		if (_rpc_get_default_domain(&dfltdom) != 0) {
			return (0);
		}
		domain = dfltdom;
	}
	if (strlen(domain) + 1 + MAXIPRINT > MAXNETNAMELEN) {
		return (0);
	}
	(void) sprintf(netname, "%s.%d@%s", OPSYS, uid, domain);
	return (1);
}


/*
 * Convert host to network-name
 */
host2netname(char netname[MAXNETNAMELEN + 1], char *host, const char *domain)
{
	char *dfltdom;
	char hostname[MAXHOSTNAMELEN+1];

	if (domain == NULL) {
		if (_rpc_get_default_domain(&dfltdom) != 0) {
			return (0);
		}
		domain = dfltdom;
	}
	if (host == NULL) {
		(void) gethostname(hostname, sizeof (hostname));
		host = hostname;
	}
	if (strlen(domain) + 1 + strlen(host) > MAXNETNAMELEN) {
		return (0);
	}
	(void) sprintf(netname, "%s.%s@%s", OPSYS, host, domain);
	return (1);
}

static
atois(str)
	char **str;
{
	char *p;
	int n;
	int sign;

	if (**str == '-') {
		sign = -1;
		(*str)++;
	} else {
		sign = 1;
	}
	n = 0;
	for (p = *str; isdigit(*p); p++) {
		n = (10 * n) + (*p - '0');
	}
	if (p == *str) {
		*str = NULL;
		return (0);
	}
	*str = p;
	return (n * sign);
}
