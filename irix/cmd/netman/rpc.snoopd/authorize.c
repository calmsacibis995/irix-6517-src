/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Snoopd Authorization
 *
 *	$Revision: 1.14 $
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
 */

#include <bstring.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <grp.h>
#include <netdb.h>
#include <pwd.h>
#include <stdio.h>
#ifdef __sgi
#include <stdlib.h>
#endif
#include <strings.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include "heap.h"
#include "snoopd.h"
#include "exception.h"
#include "macros.h"

static time_t authfile_ctime;
static struct authrule *acceptlist = 0;
static struct authrule *rejectlist = 0;

static int authorize_read(void);
static char *getword(char *, char **);
static struct authrule *auth_parserule(char *);
static struct authentry *auth_parsehost(const char *);
static struct authentry *auth_parseuser(const char *);
static struct authentry *auth_parsegroup(const char *);
static struct authentry *auth_parseservice(const char *);
static unsigned int auth_match(struct authrule *, struct in_addr,
				struct authunix_parms *, enum svctype);
static void auth_clear(void);
static void auth_deleterule(struct authrule *);
#ifdef DEBUG
static void auth_printrule(struct authrule *);
#endif

static char snoopd_auth_file[] = "/etc/rpc.snoopd.auth";
static char wildcard[] = "*";

/*
 * The authorization rules format is:
 *
 * action	host:user.group/service ...
 *
 * 'action' is either "accept" or "reject" (case insensitive).  It is
 *	followed by a list of entries.
 *
 * 'host' is a comma separated list of host names, Internet addresses, or
 *	a "*" meaning all hosts.
 *
 * 'user' is a comma separated list of user names, user ids, or
 *	a "*" meaning all users.
 *
 * 'group' is a comma separated list of group names, group ids, or
 *	a "*" meaning all groups.  The need for multiple groups can
 *	be specified by using a "+" between groups.  If 'group' is missing
 *	and 'user' present, the meaning is the same as "*" for 'group'.
 *	If both 'user' and 'group' are missing, it has the same
 *	meaning as "*.*".
 *
 * 'service' is a comma separated list of service names.  This can be used
 *	to qualify the entry a particular set of services.  If 'service' is
 *	missing, the entry applies to all services.  Current services are:
 *	netsnoop (or analyzer), netlook (or collector), and histogram
 *	(or netgraph).
 *
 *  Examples:
 *
 *	accept	sgi.sgi.com:root.sys
 *		Accept user "root" and group "sys" from "sgi.sgi.com" for
 *		all services.
 *
 *	accept	sgi.sgi.com:root,guest
 *		Accept user "root" and user "guest" with any group from
 *		"sgi.sgi.com" for all services.
 *
 *	accept	*:*.engr/histogram
 *		Accept any user with group "engr" from any machine for the
 *		histogram service only.
 *
 *	accept	*:*.engr+network/netsnoop
 *		Accept any user with both groups "engr" and "network"
 *		from any machine for the netsnoop service only.
 *
 *	reject	sgi.sgi.com,ssd.sgi.com
 *		Reject any user or any group from "sgi.sgi.com" or
 *		"ssd.sgi.com" for all services.
 *
 *	reject	sgi.sgi.com:guest
 *		Reject user "guest" with any group from "sgi.sgi.com" for
 *		all services.
 *
 */

void
authorize_init(void)
{
	struct stat st;

	if (stat(snoopd_auth_file, &st) < 0) {
		exc_errlog(LOG_ERR, errno, snoopd_auth_file);
		return;
	}
	authfile_ctime = st.st_ctime;
	if (!authorize_read())
		auth_clear();
}

static int
authorize_read(void)
{
	char buf[_POSIX_MAX_INPUT], tmpbuf[_POSIX_MAX_INPUT], *s, *next;
	FILE *fp;
	unsigned int line;
	struct authrule *ar;
	struct authrule ***p;
	struct authrule **acceptp = &acceptlist;
	struct authrule **rejectp = &rejectlist;
	int ypflag;
	extern struct options opts;
	extern int _yp_disabled;

#ifdef __sgi
	if (!opts.opt_usenameservice) {
		/* Work around sethostresorder bug */
		char s[16];
		_yp_disabled = 0;
		strcpy(s, "yp bind local");
		sethostresorder(s);
	}
#endif

	fp = fopen(snoopd_auth_file, "r");
	if (fp == 0) {
		exc_errlog(LOG_ERR, errno,
				"unable to open %s", snoopd_auth_file);
#ifdef __sgi
		if (!opts.opt_usenameservice) {
			_yp_disabled = 1;
			sethostresorder("local");
		}
#endif
		return 0;
	}

	line = 0;
	while ((s = fgets(buf, sizeof buf, fp)) != NULL) {
		line++;

		s = getword(s, &next);
		if (s == NULL || *s == '#')
			continue;

		/*
		 * Parse the action
		 */
		if (strcasecmp(s, "accept") == 0)
			p = &acceptp;
		else if (strcasecmp(s, "reject") == 0)
			p = &rejectp;
		else {
			exc_errlog(LOG_ERR, 0, "%s: line %d: bad action \"%s\"",
					    snoopd_auth_file, line, s);
			fclose(fp);
#ifdef __sgi
			if (!opts.opt_usenameservice) {
				_yp_disabled = 1;
				sethostresorder("local");
			}
#endif
			return 0;
		}

		/*
		 * Now read any number of rules
		 */
		for (s = next; s != NULL; s = next) {
			s = getword(s, &next);
			if (s == NULL)
				break;

			strcpy(tmpbuf, s);
			ar = auth_parserule(tmpbuf);
			if (ar == 0) {
				exc_errlog(LOG_ERR, 0,
					"%s: line %d: bad entry \"%s\"",
						snoopd_auth_file, line, s);
				fclose(fp);
#ifdef __sgi
				if (!opts.opt_usenameservice) {
					_yp_disabled = 1;
					sethostresorder("local");
				}
#endif
				return 0;
			}
			**p = ar;
			*p = &ar->ar_next;
		}
	}

	exc_errlog(LOG_DEBUG, 0, "%s: read %d line%s",
			snoopd_auth_file, line, line == 1 ? "" : "s");
	fclose(fp);
#ifdef __sgi
	if (!opts.opt_usenameservice) {
		_yp_disabled = 1;
		sethostresorder("local");
	}
#endif

#ifdef DEBUG
	for (ar = rejectlist; ar != 0; ar = ar->ar_next) {
		printf("reject: ");
		auth_printrule(ar);
		putchar('\n');
	}
	for (ar = acceptlist; ar != 0; ar = ar->ar_next) {
		printf("accept: ");
		auth_printrule(ar);
		putchar('\n');
	}
#endif
	return 1;
}

int
authorize(struct in_addr addr, struct authunix_parms *cred, enum svctype s)
{
	struct authrule *ar;
	struct stat st;
	unsigned int score;
	unsigned int reject = 0;
	unsigned int accept = 0;

	if (stat(snoopd_auth_file, &st) < 0) {
		/*
		 * Can't stat the authorization file.  Just set the ctime
		 * to 0 so new rules will not be read.
		 */
		st.st_ctime = 0;
		exc_errlog(LOG_DEBUG, errno,
				"unable to stat %s", snoopd_auth_file);
	}
	if (authfile_ctime < st.st_ctime) {
		auth_clear();
		authfile_ctime = st.st_ctime;
		if (!authorize_read())
			auth_clear();
	}

	for (ar = rejectlist; ar != 0; ar = ar->ar_next) {
		score = auth_match(ar, addr, cred, s);
		reject = MAX(reject, score);
	}

	for (ar = acceptlist; ar != 0; ar = ar->ar_next) {
		score = auth_match(ar, addr, cred, s);
		accept = MAX(accept, score);
	}

	return (accept - reject);
}

static char *
getword(char *s, char** nextp)
{
	while (isspace(*s))
		s++;
	if (*s == '\0')
		return NULL;

	*nextp = strpbrk(s, " \t\n");
	if (*nextp != NULL)
		*(*nextp)++ = '\0';

	return s;
}

static struct authrule *
auth_parserule(char *s)
{
	struct authrule *ar;
	struct authentry *ae, **aep;
	char *next, *p, *nextgr, pbrk;

	/*
	 * Create a new authrule structure for this rule
	 */
	ar = new(struct authrule);
	bzero(ar, sizeof *ar);

	/*
	 * Separate host part of rule
	 */
	next = strpbrk(s, ":/");
	if (next != NULL) {
		pbrk = *next;
		*next++ = '\0';
	}

	/*
	 * Go through the extracting hosts
	 */
	aep = &ar->ar_hosts;
	while ((p = strtok(s, ",")) != NULL) {
		ae = auth_parsehost(p);
		if (ae == 0)
			goto fail;
		*aep = ae;
		aep = &ae->ae_next;
		s = 0;
	}
	if (next == NULL)
		return ar;
	if (pbrk == '/')
		goto services;

	/*
	 * Users
	 */
	s = next;
	next = strpbrk(s, "./");
	if (next != NULL) {
		pbrk = *next;
		*next++ = '\0';
	}

	aep = &ar->ar_users;
	while ((p = strtok(s, ",")) != NULL) {
		ae = auth_parseuser(p);
		if (ae == 0)     {
		       s = 0;
		       continue;
	                 /*	goto fail;   */
		     }
		*aep = ae;
		aep = &ae->ae_next;
		s = 0;
	}
	if (next == NULL)
		return ar;
	if (pbrk == '/')
		goto services;

	/*
	 * Groups
	 */
	s = next;
	next = strchr(s, '/');
	if (next != NULL)
		*next++ = '\0';

	aep = &ar->ar_groups;
	for ( ; s != NULL; s = nextgr) {
		struct authentry **grp;
		struct authentry *gae;

		nextgr = strchr(s, ',');
		if (nextgr != NULL)
			*nextgr++ = '\0';

		ae = new(struct authentry);
		ae->ae_next = 0;
		ae->ae_wildcard = 0;
		ae->ae_glist = 0;
		*aep = ae;
		aep = &ae->ae_next;
		grp = &ae->ae_glist;

		while ((p = strtok(s, "+")) != NULL) {
			gae = auth_parsegroup(p);
			if (gae == 0)     {
			     s = 0;
			     continue;
			      /*	goto fail;   */
			   }
			ae->ae_wildcard++;
			*grp = gae;
			grp = &gae->ae_next;
			s = 0;
		}
	}
	if (next == NULL)
		return ar;

	/*
	 * Services
	 */
services:
	s = next;
	aep = &ar->ar_services;
	while ((p = strtok(s, ",")) != NULL) {
		ae = auth_parseservice(p);
		if (ae == 0)
			goto fail;
		*aep = ae;
		aep = &ae->ae_next;
		s = 0;
	}
	return ar;

fail:
	auth_deleterule(ar);
	return 0;
}

static struct authentry *
auth_parsehost(const char *s)
{
	struct in_addr addr;
	unsigned int wc;
	struct hostent *he;
	struct authentry *ae;

	/* Try the wildcard */
	if (strcmp(s, wildcard) == 0) {
		addr.s_addr = INADDR_NONE;
		wc = 1;
	} else {
		wc = 0;

		/* Try an Internet Address */
		addr.s_addr = inet_addr(s);
		if (addr.s_addr == INADDR_NONE) {
			/* Try a host name */
			he = gethostbyname(s);
			if (he == 0)
				return 0;
			bcopy(he->h_addr, &addr, sizeof addr);
		}
		/* Change localhost into this host's address */
		if (addr.s_addr == 0x7f000001) {
			char buf[64];
			if (gethostname(buf, 64) < 0)
				return 0;
			he = gethostbyname(buf);
			if (he == 0)
				return 0;
			bcopy(he->h_addr, &addr, sizeof addr);
		}
	}
		
	ae = new(struct authentry);
	ae->ae_next = 0;
	ae->ae_address = addr;
	ae->ae_wildcard = wc;
	return ae;
}

static struct authentry *
auth_parseuser(const char *s)
{
	long u;
	unsigned int wc;
	char *p;
	struct passwd *pw;
	struct authentry *ae;

	/* Try the wildcard */
	if (strcmp(s, wildcard) == 0) {
		u = 0;
		wc = 1;
	} else {
		wc = 0;

		/* Try an integer */
		u = strtol(s, &p, 0);
		if (p == s) {
			/* Try a user name */
			pw = getpwnam(s);
			if (pw == 0)
				return 0;
			u = pw->pw_uid;
		}
	}

	ae = new(struct authentry);
	ae->ae_next = 0;
	ae->ae_uid = (uid_t) u;
	ae->ae_wildcard = wc;
	return ae;
}

static struct authentry *
auth_parsegroup(const char *s)
{
	long g;
	unsigned int wc;
	char *p;
	struct group *gr;
	struct authentry *ae;

	/* Try the wildcard */
	if (strcmp(s, wildcard) == 0) {
		g = 0;
		wc = 1;
	} else {
		wc = 0;

		/* Try an integer */
		g = strtol(s, &p, 0);
		if (p == s) {
			/* Try a group name */
			gr = getgrnam(s);
			if (gr == 0)
				return 0;
			g = gr->gr_gid;
		}
	}

	ae = new(struct authentry);
	ae->ae_next = 0;
	ae->ae_gid = (gid_t) g;
	ae->ae_wildcard = wc;
	return ae;
}

static struct authentry *
auth_parseservice(const char *s)
{
	enum svctype service;
	unsigned int wc;
	struct authentry *ae;

	/* Try the wildcard */
	if (strcmp(s, wildcard) == 0) {
		service = 0;
		wc = 1;
	} else {
		wc = 0;

		/* Try to match a service */
		if (strcasecmp(s, "netsnoop") == 0 ||
				strcasecmp(s, "analyzer") == 0)
			service = SS_NETSNOOP;
		else if (strcasecmp(s, "netlook") == 0 ||
				strcasecmp(s, "collector") == 0)
			service = SS_NETLOOK;
		else if (strcasecmp(s, "histogram") == 0 ||
				strcasecmp(s, "netgraph") == 0)
			service = SS_HISTOGRAM;
		else if (strcasecmp(s, "addrlist") == 0)
			service = SS_ADDRLIST;
		else
			return 0;
	}

	ae = new(struct authentry);
	ae->ae_next = 0;
	ae->ae_service = service;
	ae->ae_wildcard = wc;
	return ae;
}

static unsigned int
auth_match(struct authrule *ar, struct in_addr addr,
	   struct authunix_parms *cred, enum svctype service)
{
	struct authentry *ae;
	unsigned int match, total;
#define WILD		0x1
#define EXACTHOST	0x10
#define EXACTGROUP	0x100
#define EXACTUSER	0x10000
#define EXACTSERVICE	0x100000

	total = match = 0;
	for (ae = ar->ar_hosts; ae != 0; ae = ae->ae_next) {
		if (ae->ae_wildcard != 0)
			match = WILD;
		else if (ae->ae_address.s_addr == addr.s_addr) {
			match = EXACTHOST;
			break;
		}
	}

	if (match == 0)
		return 0;
	total += match;
	if (ar->ar_users == 0)
		match = WILD;
	else {
		match = 0;
		for (ae = ar->ar_users; ae != 0; ae = ae->ae_next) {
			if (ae->ae_wildcard != 0)
				match = WILD;
			else if (ae->ae_uid == cred->aup_uid) {
				match = EXACTUSER;
				break;
			}
		}
	}

	if (match == 0)
		return 0;
	total += match;
	if (ar->ar_groups == 0)
		match = WILD;
	else {
		match = 0;

		if (cred->aup_len > 0) {
			/*
			 * Do multiple groups
			 */
			for (ae = ar->ar_groups; ae != 0; ae = ae->ae_next) {
				struct authentry *gae;
				unsigned int ngroups = cred->aup_len;
				unsigned int ewild = 0;
				unsigned int nwild = 0;
				unsigned int nexact = 0;

				/*
				 * See if the credential don't contain enough
				 * groups to match this entry.
				 */
				if (ae->ae_wildcard > ngroups)
					continue;

				/*
				 * Go through each group in the entry and
				 * see if the credentials contain that group.
				 * Keep a count of the wildcard entries and use
				 * that count later to offset any groups that
				 * couldn't be matched.
				 */
				for (gae = ae->ae_glist; gae != 0;
							gae = gae->ae_next) {
					gid_t gid;
					int i;

					if (gae->ae_wildcard != 0) {
						ewild++;
						continue;
					}

					for (i = ngroups, gid = cred->aup_gid; ;
						    gid = cred->aup_gids[--i]) {
						if (gae->ae_gid == gid) {
							nexact++;
							break;
						}
						if (i == 0) {
							nwild++;
							break;
						}
					}
				}

				/*
				 * If more wildcards were used than existed
				 * in the entry, then the match failed.
				 */
				if (ewild < nwild)
					continue;

				/*
				 * If only a wildcards exist, return match.
				 */
				if (ewild > 0 && nwild == 0 && nexact == 0)
					return 1;

				/*
				 * There was a match, calculate its weight.
				 */
				ewild = nexact * EXACTGROUP + nwild;
				match = MAX(match, ewild);
			}
		} else {
			for (ae = ar->ar_groups; ae != 0; ae = ae->ae_next) {
				struct authentry *gae;

				for (gae = ae->ae_glist; gae != 0;
							gae = gae->ae_next) {
					if (gae->ae_wildcard != 0)
						match = WILD;
					else if (gae->ae_gid == cred->aup_gid) {
						match = EXACTGROUP;
						break;
					}
				}
			}
		}
	}

	if (match == 0)
		return 0;
	total += match;
	if (ar->ar_services == 0)
		match = WILD;
	else {
		match = 0;
		for (ae = ar->ar_services; ae != 0; ae = ae->ae_next) {
			if (ae->ae_wildcard != 0)
				match = WILD;
			else if (ae->ae_service == service) {
				match = EXACTSERVICE;
				break;
			}
		}
		if (match == 0)
			return 0;
	}

	return (total + match);
}

static void
auth_clear(void)
{
	struct authrule *ar, *nar;

	for (ar = rejectlist; ar != 0; ar = nar) {
		nar = ar->ar_next;
		auth_deleterule(ar);
	}
	for (ar = acceptlist; ar != 0; ar = nar) {
		nar = ar->ar_next;
		auth_deleterule(ar);
	}
	acceptlist = rejectlist = 0;
}

static void
auth_deleterule(struct authrule *ar)
{
	struct authentry *ae, *nae;

	for (ae = ar->ar_hosts; ae != 0; ae = nae) {
		nae = ae->ae_next;
		delete(ae);
	}

	for (ae = ar->ar_users; ae != 0; ae = nae) {
		nae = ae->ae_next;
		delete(ae);
	}

	for (ae = ar->ar_groups; ae != 0; ae = nae) {
		struct authentry *gae, *ngae;

		for (gae = ae->ae_glist; gae != 0; gae = ngae) {
			ngae = gae->ae_next;
			delete(gae);
		}
		nae = ae->ae_next;
		delete(ae);
	}

	for (ae = ar->ar_services; ae != 0; ae = nae) {
		nae = ae->ae_next;
		delete(ae);
	}
}

#ifdef DEBUG
static void
auth_printrule(struct authrule *ar)
{
	struct authentry *ae;

	for (ae = ar->ar_hosts; ae != 0; ae = ae->ae_next)
		printf("%s%s", ae->ae_wildcard != 0 ? wildcard :
						inet_ntoa(ae->ae_address),
				ae->ae_next == 0 ? "" : ",");

	if (ar->ar_users != 0 || ar->ar_groups != 0)
		putchar(':');
	for (ae = ar->ar_users; ae != 0; ae = ae->ae_next)
		if (ae->ae_wildcard != 0)
			printf("%s%s", wildcard, ae->ae_next == 0 ? "" : ",");
		else
			printf("%d%s", ae->ae_uid, ae->ae_next == 0 ? "" : ",");

	if (ar->ar_groups != 0)
		putchar('.');
	for (ae = ar->ar_groups; ae != 0; ae = ae->ae_next) {
		struct authentry *gae;

		for (gae = ae->ae_glist; gae != 0; gae = gae->ae_next)
			if (gae->ae_wildcard != 0)
				printf("%s%s", wildcard,
					gae->ae_next == 0 ? "" : "+");
			else
				printf("%d%s", gae->ae_gid,
					gae->ae_next == 0 ? "" : "+");

		if (ae->ae_next != 0)
			putchar(',');
	}

	if (ar->ar_services != 0)
		putchar('/');
	for (ae = ar->ar_services; ae != 0; ae = ae->ae_next)
		if (ae->ae_wildcard != 0)
			printf("%s%s", wildcard, ae->ae_next == 0 ? "" : ",");
		else
			printf("%d%s", ae->ae_service,
					ae->ae_next == 0 ? "" : ",");
}
#endif
