/* 
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 * 1.9 88/02/08 
 */

/*
 * netname utility routines
 * convert from unix names to network names and vice-versa
 * This module is operating system dependent!
 * What we define here will work with any unix system that has adopted
 * the Sun NIS domain architecture.
 */
#ifdef sgi
#ifdef __STDC__
	#pragma weak netname2user = _netname2user
	#pragma weak netname2host = _netname2host
	#pragma weak getnetname   = _getnetname
	#pragma weak user2netname = _user2netname
	#pragma weak host2netname = _host2netname
#endif
#include "synonyms.h"
#endif
#include <sys/param.h>
#include <rpc/rpc.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>
#include <ctype.h>
#include <string.h>	/* prototypes for string functions */
#include <unistd.h>	/* prototype for gethostname() */
#include "priv_extern.h"

static const char OPSYS[] = "unix";
static const char NETID[] = "netid.byname";	

#ifdef sgi
static int atois(char **str);
#endif

/*
 * Convert network-name into unix credential
 */

/* ARGSUSED */
int
netname2user(char netname[MAXNETNAMELEN+1],
	int *uidp,
	int *gidp,
	int *gidlenp,
	int *gidlist)
{
	int stat;
	char *val;
	char *p;
	int vallen;
	char *domain;
	int gidlen;

	stat = yp_get_default_domain(&domain);
	if (stat != 0) {
		return (0);
	}
	stat = yp_match(domain, NETID, netname, (int)strlen(netname), &val, &vallen);
	if (stat != 0) {
		return (0);
	}
	val[vallen] = '\0';
	p = val;
	*uidp = atois(&p);
	if (p == NULL || *p++ != ':') {
		free(val);
		return (0);
	}
	*gidp = atois(&p);
	gidlen = 0;
	*gidlenp = gidlen;
	free(val);
	return (1);
}

/*
 * Convert network-name to hostname
 */
int
netname2host(char netname[MAXNETNAMELEN+1],
	char *hostname,
	int hostlen)
{
	int stat;
	char *val;
	int vallen;
	char *domain;

	stat = yp_get_default_domain(&domain);
	if (stat != 0) {
		return (0);
	}
	stat = yp_match(domain, NETID, netname, (int)strlen(netname), &val, &vallen);
	if (stat != 0) {
		return (0);
	}
	val[vallen] = '\0';
	if (*val != '0') {
		free(val);
		return (0);
	}	
	if (val[1] != ':') {
		free(val);
		return (0);
	}
	(void) strncpy(hostname, val + 2, (size_t)hostlen);
	free(val);
	return (1);
}


/*
 * Figure out my fully qualified network name
 */
int
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
int
user2netname(char netname[MAXNETNAMELEN + 1], 
	uid_t uid, 
	const char *domain)
{
	char *dfltdom;

#define MAXIPRINT	(11)	/* max length of printed integer */

	if (domain == NULL) {
		if (yp_get_default_domain(&dfltdom) != 0) {
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
int
host2netname(char netname[MAXNETNAMELEN + 1],
	char *host, 
	const char *domain)
{
	char *dfltdom;
	char hostname[MAXHOSTNAMELEN+1]; 

	if (domain == NULL) {
		if (yp_get_default_domain(&dfltdom) != 0) {
			return (0);
		}
		domain = dfltdom;
	}
	if (host == NULL) {
		(void) gethostname(hostname, sizeof(hostname));
		host = hostname;
	}
	if (strlen(domain) + 1 + strlen(host) > MAXNETNAMELEN) {
		return (0);
	} 
	(void) sprintf(netname, "%s.%s@%s", OPSYS, host, domain);
	return (1);
}


static int
atois(char **str)
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
