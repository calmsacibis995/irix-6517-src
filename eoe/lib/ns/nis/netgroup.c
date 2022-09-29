/*
** This file contains routines to parse a netgroup file in place.
** For each of these routines the file is first completely parsed into
** a tree, and then items are looked up in the tree.
*/

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <alloca.h>
#include <ns_api.h>
#include <ns_daemon.h>
#include "ns_nis.h"
#include "dstring.h"
#include "htree.h"

static void
ng_remove(ht_node_t **npp)
{
	ds_clear(&(*npp)->key);
	ds_clear(&(*npp)->val);

	free(*npp);
	*npp = 0;
}

/*
** The ng_names_t structure is used to stop recursion inside of a new
** netgroup.  Basically, it is just a stack list of the names of the
** netgroups.
*/
typedef struct ng_names {
	char		*name;
	int		len;
	struct ng_names	*last;
} ng_names_t;

/*
** This is the routine which parses every netgroup setting up the reverse
** maps byhost and byuser.  This is a recursive routine since netgroups
** are recursive.  The names array contains the names of the netgroups
** above us.  We do not recurse if we have already visited this netgroup.
** It could use up a lot of space for deeply recursive netgroup files.
*/
static int
ng_line(ng_names_t *names, ht_node_t *item, nisnetgr_t *ng)
{
	char *n, *p, *q;
	ht_node_t *np;
	dstring_t host, user, domain;
	ng_names_t this, *tp, *first;
	
	nsd_logprintf(NSD_LOG_MAX, "ng_line: parsing line: %s\n",
	    DS_STRING(&item->val));

	n = DS_STRING(&item->val);

	if (! ds_init(&host, 100) || ! ds_init(&user, 100) ||
	    ! ds_init(&domain, 100)) {
		nsd_logprintf(NSD_LOG_RESOURCE, "ng_line: failed malloc\n");
		goto parse_error;
	}

	/*
	** We care about the first name in the list.
	*/
	for (first = names; first->last; first = first->last);

	/*
	** A netgroup is of the format:
	**	member1, member2, . . . memberN
	** where a member is either a name of another netgroup, or:
	**	(host, user, domain)
	** Any of these can be empty or a wildcard.
	*/

	while (n) {
		for (; *n && (isspace(*n) || *n == ','); n++);
		if (! *n) {
			break;
		}
		if (*n == '(') {
			/* triplet */
			
			/* host */
			for (++n; *n && isspace(*n); n++);
			for (p = n; *p && ! isspace(*p) && (*p != ',') &&
			    (*p != ')'); p++);
			if (p == n) {
				ds_set(&host, "*", 1);
			} else {
				if (! ds_set(&host, n, p - n)) {
					nsd_logprintf(NSD_LOG_RESOURCE,
					    "ng_line: failed malloc\n");
					goto parse_error;
				}
				for (q = DS_STRING(&host); *q;
				    *q++ = tolower(*q));
			}
			for (n = p; *n && isspace(*n); n++);
			if (*n != ',') {
				nsd_logprintf(NSD_LOG_OPER,
    "ng_line: netgroups parse error, expected ',' in: %s, following:\n\t%s\n",
				    DS_STRING(&item->key), p);
				goto parse_error;
			}

			/* user */
			for (++n; *n && isspace(*n); n++);
			for (p = n; *p && ! isspace(*p) && (*p != ',') &&
			    (*p != ')'); p++);
			if (p == n) {
				ds_set(&user, "*", 1);
			} else {
				if (! ds_set(&user, n, p - n)) {
					nsd_logprintf(NSD_LOG_RESOURCE,
					    "ng_line: failed malloc\n");
					goto parse_error;
				}
			}
			for (n = p; *n && isspace(*n); n++);
			if (*n != ',') {
				nsd_logprintf(NSD_LOG_OPER,
    "ng_line: netgroups parse error, expected ',' in: %s, following:\n\t%s\n",
				    DS_STRING(&item->key), p);
				goto parse_error;
			}

			/* domain */
			for (++n; *n && isspace(*n); n++);
			for (p = n; *p && ! isspace(*p) && (*p != ',') &&
			    (*p != ')'); p++);
			if (p == n) {
				ds_set(&domain, "*", 1);
			} else {
				if (! ds_set(&domain, n, p - n)) {
					nsd_logprintf(NSD_LOG_RESOURCE,
					    "ng_line: failed malloc\n");
					goto parse_error;
				}
				for (q = DS_STRING(&host); *q;
				    *q++ = tolower(*q));
			}
			for (n = p; *n && isspace(*n); n++);
			if (*n++ != ')') {
				nsd_logprintf(NSD_LOG_OPER,
    "ng_line: netgroups parse error, expected ')' in: %s, following:\n\t%s\n",
				    DS_STRING(&item->key), p);
				goto parse_error;
			}

			/* qualify host */
			if (! ds_append(&host, ".", 1) ||
			    ! ds_cpy(&host, &domain)) {
				nsd_logprintf(NSD_LOG_RESOURCE,
				    "ng_line: failed malloc\n");
				goto parse_error;
			}

			/* qualify user */
			if (! ds_append(&user, ".", 1) ||
			    ! ds_cpy(&user, &domain)) {
				nsd_logprintf(NSD_LOG_RESOURCE,
				    "ng_line: failed malloc\n");
				goto parse_error;
			}

			/* store byhost */
			p = DS_STRING(&host);
			if (isalnum(*p) || *p == '_' || *p == '*') {
				np = ht_lookup(&ng->byhost, DS_STRING(&host),
				    DS_LEN(&host), strncmp);
				if (np) {
					/* edit value */
					if (! ds_append(&np->val, ",", 1) ||
					    ! ds_append(&np->val, first->name,
				            first->len)) {
						nsd_logprintf(NSD_LOG_RESOURCE,
					    "ng_line: failed malloc\n");
						goto parse_error;
					}
				} else {
					/* new */
					np = (ht_node_t *)nsd_calloc(1,
					    sizeof *np);
					if (! np) {
						nsd_logprintf(NSD_LOG_RESOURCE,
					    "ng_line: failed malloc\n");
						goto parse_error;
					}
					if (! ds_cpy(&np->key, &host)) {
						ng_remove(&np);
						nsd_logprintf(NSD_LOG_RESOURCE,
					    "ng_line: failed malloc\n");
						goto parse_error;
					}
					if (! ds_set(&np->val, first->name,
					    first->len)) {
						ng_remove(&np);
						nsd_logprintf(NSD_LOG_RESOURCE,
					    "ng_line: failed malloc\n");
						goto parse_error;
					}
					if (! ht_insert(&ng->byhost, np,
					    strncmp)) {
						ng_remove(&np);
					}
				}
			}

			/* store byuser */
			p = DS_STRING(&user);
			if (isalnum(*p) || *p == '_' || *p == '*') {
				np = ht_lookup(&ng->byuser, DS_STRING(&user),
				    DS_LEN(&user), strncmp);
				if (np) {
					/* edit value */
					if (! ds_append(&np->val, ",", 1) ||
					    ! ds_append(&np->val, first->name,
				            first->len)) {
						nsd_logprintf(NSD_LOG_RESOURCE,
					    "ng_line: failed malloc\n");
						goto parse_error;
					}
				} else {
					/* new */
					np = (ht_node_t *)nsd_calloc(1,
					    sizeof *np);
					if (! np) {
						nsd_logprintf(NSD_LOG_RESOURCE,
					    "ng_line: failed malloc\n");
						goto parse_error;
					}
					if (! ds_cpy(&np->key, &user)) {
						ng_remove(&np);
						nsd_logprintf(NSD_LOG_RESOURCE,
					    "ng_line: failed malloc\n");
						goto parse_error;
					}
					if (! ds_set(&np->val, first->name,
					    first->len)) {
						ng_remove(&np);
						nsd_logprintf(NSD_LOG_RESOURCE,
					    "ng_line: failed malloc\n");
						goto parse_error;
					}
					if (! ht_insert(&ng->byuser, np,
					    strncmp)) {
						ng_remove(&np);
					}
				}
			}
		} else {
			/* netgroup */
			for (p = n; *n && ! isspace(*n) && (*n != ','); n++);
			this.name = p;
			this.len = n - p;
			this.last = names;
			for (tp = names; tp; tp = tp->last) {
				if (tp->len == this.len &&
				    (memcmp(tp->name, this.name,
				    this.len) == 0)) {
					continue;
				}
			}
			np = ht_lookup(&ng->byname, this.name, this.len,
			    strncmp);
			if (np) {
				if (! tp) {
					ng_line(&this, np, ng);
				}
			}
		}
	}

	ds_clear(&host), ds_clear(&user), ds_clear(&domain);
	return 1;

parse_error:
	ds_clear(&host), ds_clear(&user), ds_clear(&domain);
	return 0;
}

/*
** This routine walks the tree, and calls the ng_line routine above
** to parse that item.
*/
static void
ng_recurse(void *vp, ht_node_t *item)
{
	ng_names_t this;
	
	this.name = DS_STRING(&item->key);
	this.len = DS_LEN(&item->key);
	this.last = 0;
	ng_line(&this, item, (nisnetgr_t *)vp);
}

void
nis_netgroup_clear(nisnetgr_t *ng)
{
	if (ng->byname.top) {
		ht_clear(&ng->byname);
	}
	if (ng->byhost.top) {
		ht_clear(&ng->byhost);
	}
	if (ng->byuser.top) {
		ht_clear(&ng->byuser);
	}
}

/*
** The nis_netgroup_parse routine will parse the netgroup file into a tree
** of entries for each "line" in the file.  This byname tree is later walked
** and parsed into the reverse maps in the ng_recurse routine above.
*/
int
nis_netgroup_parse(char *buf, nisnetgr_t *ng)
{
	char *line, *p, *q;
	ht_node_t *np;
	time_t now;

	time(&now);
	if ((now - ng->version) < 60) {
		return 1;
	}
	nis_netgroup_clear(ng);
	ng->version = now;

	for (line = buf; line && *line; line = strchr(line + 1, '\n')) {
		for (; *line && isspace(*line); line++);
		if (*line == '#') {
			continue;
		}

		np = (ht_node_t *)nsd_calloc(1, sizeof *np);
		if (! np) {
			nsd_logprintf(NSD_LOG_RESOURCE,
			    "ng_parse: failed malloc\n");
			goto line_error;
		}
		
		/*
		** Determine key.
		*/
		for (p = line; *p && ! isspace(*p); p++);
		if (p == line) {
			ng_remove(&np);
			continue;
		}
		if (! ds_set(&np->key, line, p - line)) {
			ng_remove(&np);
			nsd_logprintf(NSD_LOG_RESOURCE,
			    "ng_parse: failed malloc\n");
			goto line_error;
		}

		/*
		** Copy in line.
		*/
		for (;; line = p) {
			for (; *p && (*p == '\t' || *p == ' '); p++);
			for (q = p; *q && *q != '\\' && *q != '\n'; q++);

			if (q != p && *p != '#') {
				if (! ds_append(&np->val, p, q - p)) {
					ng_remove(&np);
					nsd_logprintf(NSD_LOG_RESOURCE,
					    "ng_parse: failed malloc\n");
					goto line_error;
				}
			}

			if (*q == '\\') {
				p = q + 2;
			} else if (*p == '#') {
				p = q + 1;
			} else {
				break;
			}
		}
		if (! ht_insert(&ng->byname, np, strncmp)) {
			/*
			** Already seen this one.
			*/
			ng_remove(&np);
			continue;
		}
		nsd_logprintf(NSD_LOG_MAX, "\tstored: %s: %s\n",
		    DS_STRING(&np->key), DS_STRING(&np->val));
	}

	/*
	** Now we loop over the items filling the user and host trees.
	** This is a recursive routine which can eat up a ton of memory,
	** but . . .
	*/
	if (ng->byname.top) {
		ht_walk(&ng->byname, ng, ng_recurse);
	}

	return 1;

line_error:
	/* Invalidate the tree. */
	ng->version = 0;
	return 0;
}
