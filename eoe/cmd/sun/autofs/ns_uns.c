/*
 * 
 *       ns_uns.c 
 *
 */

#include <stdio.h>
#include <syslog.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/systeminfo.h>
#include <ns_api.h>
#include <alloca.h>
#include "autofsd.h"


/*
** UNS allows dynamic table creation only under directries created with the
** (dynamic) attribute set.  For autofs, we're using the parent directory
** "automount".  See ns_lookup(3C) for more information.
*/
#define UNS_PARENT     "automount:"
#define UNS_PARENT_LEN 10

#define NETMASKS_BYADDR "netmasks.byaddr"

int loaddirect_uns();
static int replace_undscr_by_dot(char *);

typedef struct autofs_map {
	ns_map_t *ns_map;
	char *map_name;
	struct autofs_map *next;
} autofs_map_t;

static autofs_map_t *MapList = NULL;
extern char *strdup(char *);

ns_map_t *
get_map(char *map_name) 
{
	autofs_map_t *p;

	AUTOFS_LOCK(map_lock);
	for (p = MapList; 
	     p && strcmp(map_name, p->map_name) != 0; 
	     p = p->next)
		;
	
	if (!p) {
		p = calloc(1, sizeof(autofs_map_t));
		if (!p) 
			goto calloc_error_1;

		p->ns_map = calloc(1, sizeof(ns_map_t));
		if (!p->ns_map)
			goto calloc_error_2;

		p->map_name = strdup(map_name);
		if (!p->map_name) 
			goto strdup_error;

		p->ns_map->m_flags |= NS_MAP_DYNAMIC;	
		
		p->next = MapList;
		MapList = p;

	}

	AUTOFS_UNLOCK(map_lock);
	return p->ns_map;

strdup_error:
	free(p->ns_map);
calloc_error_2:
	free(p);
calloc_error_1:
	syslog(LOG_ERR, "get_map: memory allocation error: %m");

	AUTOFS_UNLOCK(map_lock);
	return NULL;
}
	

const char *
unserr_string(error)
        int error;
{
	switch (error) {
	case NS_SUCCESS:
		return "NS_SUCCESS";
	case NS_NOTFOUND:
		return "NS_NOTFOUND";
	case NS_UNAVAIL:
		return "NS_UNAVAIL";
	case NS_TRYAGAIN:
		return "NS_TRYAGAIN";
	case NS_BADREQ:
		return "NS_BADREQ";
	case NS_FATAL:
		return "NS_FATAL";
	default:
		return "UNKNOWN ERROR";
	}
}

static int
replace_dot_by_undscr(char *map)
{
	int ret_val = 0;

	while (*map) {
		if (*map == '.') {
			ret_val = 1;
			*map = '_';
		}
		map++;
	}
	return (ret_val);
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

int
getmapent_uns(key, autofsmap, ml)
        char *key, *autofsmap;
        struct mapline *ml;
{
	int nserr, len;
	char nsline[LINESZ];
	char *mapname, *my_map = NULL, *lp, *lq, *p, *localkey;
	ns_map_t *automap;

	if (!autofsmap) 
		return 1;

	automap = get_map(autofsmap);
	if (!automap)
		return (1);

	memset(nsline, 0, LINESZ);

	/*
	** Create the dynamic table name; it looks like "automount:hosts"
	** for the AutoFS hosts map.
	*/
	len = UNS_PARENT_LEN + 1 + strlen(autofsmap);
	if (len > 1023) {
		syslog(LOG_ERR, "getmapent_uns: mapname rewrite too long");
		return (1);
	}
	mapname = alloca(len);
	snprintf(mapname, len, "%s%s", UNS_PARENT, autofsmap);

	/*
	 * Convert forward slashes to back slashes 
	 */
	localkey = alloca(strlen(key) + 1);
	for (lp = localkey, lq = key; lq && *lq; lp++, lq++) {
		*lp = (*lq == '/') ? '\\' : *lq;
	}
	*lp = '\0';

	nserr = ns_lookup(automap, NULL, mapname, localkey, NULL, 
			  nsline, LINESZ);

	if (nserr == NS_FATAL && strchr(mapname, '_') != NULL) { 
		/* No table */
		my_map = strdup(mapname);
		if (my_map == NULL) {
			syslog(LOG_ERR, 
			       "getmapent_uns: memory alloc failed: %m");
			return (1);
		}
		if (replace_undscr_by_dot(my_map))
			nserr = ns_lookup(automap, NULL, my_map, key, 
					  NULL, nsline, LINESZ);
	} else if (nserr == NS_FATAL && strchr(mapname, '.') != NULL) {
		/* No table */
		my_map = strdup(mapname);
		if (my_map == NULL) {
			syslog(LOG_ERR, 
			       "getmapent_uns: memory alloc failed: %m");
			return (1);
		}
		if (replace_dot_by_undscr(my_map))
			nserr = ns_lookup(automap, NULL, my_map, key, 
					  NULL, nsline, LINESZ);
	}

	if (nserr) {
		if (nserr == NS_NOTFOUND) {
			/*
			 * Try the default key "*"
			 */
			if (my_map == NULL) 
				nserr = ns_lookup(automap, NULL, 
						  mapname, "*", NULL, nsline, 
						  LINESZ);
			else
				nserr = ns_lookup(automap, NULL, 
						  my_map, "*", NULL, nsline, 
						  LINESZ);
		} 
	}

	/* Done with ns_lookup */
	if (my_map)
		free(my_map);

	if (nserr) {
		if (verbose) 
			syslog(LOG_ERR, "getmapent_uns: %s %s returned %s", 
			       key, mapname, unserr_string(nserr));
		nserr = 1;
		goto done;
	}

	/*
	 * at this point we are sure that ns_lookup succeeded
	 * so massage the entry by
	 * 1. ignoring # and beyond
	 * 2. trim the leading whitespace
	 * 3. and the key (nsd may pass back a key, which we don't need here)
	 * 4. trim the trailing whitespace
	 */

	/* Step 1 */
	if (lp = strchr(nsline, '#'))
		*lp = '\0';
	len = strlen(nsline);
	if (len == 0) {
		nserr = 1;
		goto done;
	}
	
	/* Step 2 */
	p = (char *) nsline;
	while (p && *p && isspace(*p)) /* Skip starting whitespace */
		p++;

	/* Step 3 */
	while (p && *p && !isspace(*p)) /* Skip the key */
		p++;

	/* Step 4 */
	lp = (char *) nsline + len;
	while (lp > p && isspace(*lp))
		*lp-- = '\0';
	if (lp == p) {
		nserr = 1;
		goto done;
	}
	
	/* now we have the correct data */
	(void) strcpy(ml->linebuf, p);
	lp = ml->linebuf;
	lq = ml->lineqbuf;
	unquote(lp, lq);

	nserr = 0;
done:
	return (nserr);
	       
}


getnetmask_uns(netname, mask)
	char *netname;
	char **mask;
{
	char nsline[LINESZ], *p, *lp;
	ns_map_t *automap;		/* NSD map type */
	int len;

	memset(nsline, 0, sizeof(nsline));

	automap = get_map(NETMASKS_BYADDR);
	if (!automap)
		return (1);

	if (ns_lookup(automap, NULL, NETMASKS_BYADDR, netname, NULL,
		      nsline, LINESZ) != NS_SUCCESS) {
		return (1);
	}
	
	/*
	 * at this point we are sure that ns_lookup succeeded
	 * so massage the entry by
	 * 1. ignoring # and beyond
	 * 2. Removing the key from the entry (split on whitespace)
	 * 3. trim the trailing whitespace
	 */

	/* Step 1 */
	if (p = strchr(nsline, '#'))
		*p = '\0';
	len = strlen(nsline);
	if (len == 0) {
		return (1);
	}

	/* Step 2 */
	p = (char *) nsline;
	while (p && *p && isspace(*p)) /* Skip starting whitespace */
		p++;
	/* Find the next whitespace */
	for (; p && *p && *p != '\t' && *p != ' '; p++);
	if (!p || !*p) {
		return (1);
	}

	/* Step 3 */
	lp = p + len;
	while (lp > p && isspace(*lp))
		*lp-- = '\0';
	if (lp == p) {
		return (1);
	}

	if ((*mask = calloc(strlen(p)+1, sizeof(char))) == NULL) {
		if (verbose) 
			syslog(LOG_ERR, 
			       "getnetmask_uns: memory alloc failed: %m");
		return (1);
	}
	memcpy(*mask, p, strlen(p));

	return (0);
}

int
loadmaster_uns(mastermap, defopts, stack, stkptr)
	char *mastermap, *defopts;
	char **stack, ***stkptr;
{
	FILE *fp;
	int done = 0, len, atleastoneline=0;
	char *uns_map, *line, *dir, *map, *opts;
	char linebuf[LINESZ], lineq[LINESZ];
	ns_map_t *automap;		/* NSD map type */

	if (!mastermap) 
		return (1);

	automap = get_map(mastermap);
	if (!automap) 
		return (1);

	/*
	** Create the dynamic table name; it looks like "automount:hosts"
	** for the AutoFS hosts map.
	*/
	len = UNS_PARENT_LEN + strlen(mastermap) + 1;
	uns_map = alloca(len);
	snprintf(uns_map, len, "%s%s", UNS_PARENT, mastermap);

	fp = ns_list(automap, NULL, uns_map, NULL);

	if (fp == NULL) 
		return (1);

	while ((line = get_line(fp, uns_map, linebuf, 
				sizeof (linebuf))) != NULL) {
		atleastoneline++;
		unquote(line, lineq);
		macro_expand("", line, lineq, sizeof(linebuf));
		dir = line;
		while (*dir && isspace(*dir))
			dir++;
		if (*dir == '\0')
			continue;
		map = dir;

		while (*map && !isspace(*map)) map++;
		if (*map)
			*map++ = '\0';

		while (*map && isspace(*map))
			map++;
		if (*map == '\0')
			continue;
		opts = map;
		while (*opts && !isspace(*opts))
			opts++;
		if (*opts) {
			*opts++ = '\0';
			while (*opts && isspace(*opts))
				opts++;
		}
		if (*opts != '-')
			opts = defopts;
		else
			opts++;
		/*
		 * Check for no embedded blanks.
		 */
		if (strcspn(opts, " 	") == strlen(opts)) {
			dirinit(dir, map, opts, 0, stack, stkptr);
		} else {
			pr_msg("Warning: invalid entry for %s ignored.\n", 
			       dir);
			continue;
		}
	}
	done++;

	(void) fclose(fp);

	if(!atleastoneline)
	{
		char *p=strdup(mastermap);
		int ret=0;

		if(replace_undscr_by_dot(p))
			ret=loadmaster_uns(p,defopts);
		free(p);
		
		return (ret);
	}
		
	return (0);

}

int
loaddirect_uns(mapname, local_map, opts, stack, stkptr)
	char *mapname, *local_map, *opts;
	char **stack, ***stkptr;
{
	FILE *fp;
	int done = 0, len, atleastoneline=0;
	char *uns_map, *line, *p1, *p2;
	char linebuf[LINESZ];
	ns_map_t *automap;

	if (!mapname)
		return (1);

	automap = get_map(mapname);
	if (!automap)
		return (1);

	/*
	** Create the dynamic table name; it looks like "automount:hosts"
	** for the AutoFS hosts map.
	*/
	len = UNS_PARENT_LEN + strlen(mapname) + 1;
	uns_map = alloca(len);
	snprintf(uns_map, len, "%s%s", UNS_PARENT, mapname);

	fp = ns_list(automap, NULL, uns_map, NULL);

	if (fp == NULL) 
		return (1);

	while ((line = get_line(fp, uns_map, linebuf,
				sizeof (linebuf))) != NULL) {
		atleastoneline++;
		p1 = line;
		while (*p1 && isspace(*p1))
			p1++;
		if (*p1 == '\0')
			continue;
		p2 = p1;
		while (*p2 && !isspace(*p2))
			p2++;
		*p2 = '\0';
		if (*p1 == '+') {
			p1++;
			(void) loaddirect_map(p1, local_map, opts, stack, stkptr);
		} else {
			dirinit(p1, local_map, opts, 1, stack, stkptr);
		}
		done++;
	}

	(void) fclose(fp);

	if(!atleastoneline)
	{
		char *p=strdup(mapname);
		int ret=0;

		if(replace_undscr_by_dot(p))
			ret=loaddirect_uns(p,local_map,opts);
		free(p);
		
		return (ret);
	}
		
	return (!done);
}


/*
** These don't do anything anymore but make autofsd link 
*/
char self[64];		/* my hostname */

void
init_names(void)
{
	(void) gethostname(self, sizeof (self));
}
