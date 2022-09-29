/*
 *	ns_nis.c
 *
 *	Copyright (c) 1988-1993 Sun Microsystems Inc
 *	All Rights Reserved.
 */

#ident	"@(#)ns_nis.c	1.5	93/10/27 SMI"

#include <stdio.h>
#include <syslog.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/systeminfo.h>
#include <rpcsvc/ypclnt.h>
#include "autofsd.h"

#define	KEY		0
#define	CONTENTS	1
#define	MASK_SIZE	1024

static int nis_err(int);
static int replace_undscr_by_dot(char *);

char self[64];		/* my hostname */
char mydomain[64];	/* my domain name */

void
init_names(void)
{
	(void) gethostname(self, sizeof (self));
	(void) getdomainname(mydomain, sizeof mydomain);
}
	
getmapent_nis(key, map, ml)
	char *key, *map;
	struct mapline *ml;
{
	char *nisline = NULL;
	char *my_map = NULL;
	char *lp, *lq;
	int nislen, len;
	int nserr;

	nserr = yp_match(mydomain, map, key, strlen(key),
						&nisline, &nislen);
	if (nserr == YPERR_MAP) {
		my_map = strdup(map);
		if (my_map == NULL) {
			syslog(LOG_ERR,
				"getmapent_nis: memory alloc failed: %m");
			return (1);
		}
		if (replace_undscr_by_dot(my_map))
			nserr = yp_match(mydomain, my_map, key,
					strlen(key), &nisline, &nislen);
	}

	if (nserr) {
		if (nserr == YPERR_KEY) {
			/*
			 * Try the default entry "*"
			 */
			if (my_map == NULL)
				nserr = yp_match(mydomain, map, "*", 1,
						&nisline, &nislen);
			else
				nserr = yp_match(mydomain, my_map, "*", 1,
						&nisline, &nislen);
		} else {
			if (verbose)
				syslog(LOG_ERR, "%s: %s",
					map, yperr_string(nserr));
			nserr = 1;
		}
	}
	if (my_map != NULL)
		free(my_map);

	nserr = nis_err(nserr);
	if (nserr)
		goto done;

	/*
	 * at this point we are sure that yp_match succeeded
	 * so massage the entry by
	 * 1. ignoring # and beyond
	 * 2. trim the trailing whitespace
	 */
	if (lp = strchr(nisline, '#'))
		*lp = '\0';
	len = strlen(nisline);
	if (len == 0) {
		nserr = 1;
		goto done;
	}
	lp = &nisline[len - 1];
	while (lp > nisline && isspace(*lp))
		*lp-- = '\0';
	if (lp == nisline) {
		nserr = 1;
		goto done;
	}
	(void) strcpy(ml->linebuf, nisline);
	lp = ml->linebuf;
	lq = ml->lineqbuf;
	unquote(lp, lq);
	/* now we have the correct line */

	nserr = 0;
done:
	if (nisline)
		free((char *) nisline);
	return (nserr);

}


getnetmask_nis(netname, mask)
	char *netname;
	char **mask;
{
	int outsize, nserr;

	nserr = yp_match(mydomain, "netmasks.byaddr",
			netname, strlen(netname), mask,
			&outsize);
	if (nserr == 0)
		return (0);

	return (nis_err(nserr));
}

loadmaster_nis(mapname, defopts, stack, stkptr)
	char *mapname, *defopts;
	char **stack, ***stkptr;
{
	int first, err;
	char *key, *nkey, *val;
	int kl, nkl, vl;
	char dir[256], map[256], qbuff[256];
	char *p, *opts, *my_mapname;
	int count = 0;


	first = 1;
	key  = NULL; kl  = 0;
	nkey = NULL; nkl = 0;
	val  = NULL; vl  = 0;

	/*
	 * need a private copy of mapname, because we may change
	 * the underscores by dots. We however do not want the
	 * orignal to be changed, as we may want to use the
	 * original name in some other name service
	 */
	my_mapname = strdup(mapname);
	if (my_mapname == NULL) {
		syslog(LOG_ERR, "loadmaster_yp: memory alloc failed: %m");
		/* not the name svc's fault but ... */
		return (1);
	}
	for (;;) {
		if (first) {
			first = 0;
			err = yp_first(mydomain, my_mapname,
				&nkey, &nkl, &val, &vl);
			if ((err == YPERR_MAP) &&
			    (replace_undscr_by_dot(my_mapname)))
				err = yp_first(mydomain, my_mapname,
					&nkey, &nkl, &val, &vl);
		} else {
			err = yp_next(mydomain, my_mapname, key, kl,
				&nkey, &nkl, &val, &vl);
		}
		if (err) {
			if (err != YPERR_NOMORE && err != YPERR_MAP)
				if (verbose)
					syslog(LOG_ERR, "%s: %s",
					my_mapname, yperr_string(err));
			break;
		}
		if (key)
			free(key);
		key = nkey;
		kl = nkl;

		if (kl >= 256 || vl >= 256)
			break;
		if (kl < 2 || vl < 1)
			break;
		if (isspace(*key) || *key == '#')
			break;
		(void) strncpy(dir, key, kl);
		dir[kl] = '\0';
		(void) strncpy(map, val, vl);
		map[vl] = '\0';
		free(val);
		if (macro_expand("", dir, qbuff, sizeof (dir)) == -1 ||
		    macro_expand(dir, map, qbuff, sizeof (map)) == -1) {
			syslog(LOG_ERR, "loadmaster_yp: ignore bad %s entry",
			mapname);
			continue;
		}
		p = map;
		while (*p && !isspace(*p))
			p++;
		opts = defopts;
		if (*p) {
			*p++ = '\0';
			while (*p && isspace(*p))
				p++;
			if (*p == '-')
				opts = p+1;
		}

		/*
		 * Check for no embedded blanks.
		 */
		if (strcspn(opts, " 	") == strlen(opts)) {
			dirinit(dir, map, opts, 0, stack, stkptr);
			count++;
		} else {
pr_msg("Warning: invalid entry for %s in NIS map %s ignored.\n", dir, mapname);
		}

	}
	if (my_mapname)
		free(my_mapname);

	/*
	 * In the context of a master map, if no entry is
	 * found, it is like NOTFOUND
	 */
	if (count > 0 && err == YPERR_NOMORE)
		return (0);
	else {
		if (err)
			return (nis_err(err));
		else
			/*
			 * This case will happen if map is empty
			 *  or none of the entries is valid
			 */
			return (1);
	}
}

loaddirect_nis(nismap, localmap, opts, stack, stkptr)
	char *nismap, *localmap, *opts;
	char **stack, ***stkptr;
{
	int first, err, count;
	char *key, *nkey, *val, *my_nismap;
	int kl, nkl, vl;
	char dir[100];

	first = 1;
	key  = NULL; kl  = 0;
	nkey = NULL; nkl = 0;
	val  = NULL; vl  = 0;
	count = 0;
	my_nismap = NULL;

	my_nismap = strdup(nismap);
	if (my_nismap == NULL) {
		syslog(LOG_ERR, "loadmaster_yp: memory alloc failed: %m");
		return (1);
	}
	for (;;) {
		if (first) {
			first = 0;
			err = yp_first(mydomain, my_nismap, &nkey, &nkl,
					&val, &vl);
			if ((err == YPERR_MAP) &&
			    (replace_undscr_by_dot(my_nismap)))
				err = yp_first(mydomain, my_nismap,
						&nkey, &nkl, &val, &vl);

		} else {
			err = yp_next(mydomain, my_nismap, key, kl,
					&nkey, &nkl, &val, &vl);
		}
		if (err) {
			if (err != YPERR_NOMORE && err != YPERR_MAP)
				syslog(LOG_ERR, "%s: %s",
					my_nismap, yperr_string(err));
			break;
		}
		if (key)
			free(key);
		key = nkey;
		kl = nkl;

		if (kl < 2 || kl >= 100)
			continue;
		if (isspace(*key) || *key == '#')
			continue;
		(void) strncpy(dir, key, kl);
		dir[kl] = '\0';

		dirinit(dir, localmap, opts, 1, stack, stkptr);
		count++;
		free(val);
	}

	if (my_nismap)
		free(my_nismap);

	if (count > 0 && err == YPERR_NOMORE)
			return (0);
	else
		return (nis_err(err));

}

static int
replace_undscr_by_dot(char *map)
{
	int ret_val = 0;

	while (*map) {
		if (*map == '_') {
			ret_val = 1;
			*map = '.';
		}
		map++;
	}
	return (ret_val);
}

static int
nis_err(int err)
{
	switch (err) {
	case 0:
		return (0);
	case YPERR_KEY:
		return (1);
	case YPERR_MAP:
		return (1);
	default:
		return (1);
	}
}
