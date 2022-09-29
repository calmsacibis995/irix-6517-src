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
#include "ns_files.h"
#include "dstring.h"
#include "htree.h"

htree_t byname = {0};
htree_t byhost = {0};
htree_t byuser = {0};
time_t ng_version = 0;

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

/*ARGSUSED*/
static int
true(abilock_t *l)
{
	return 1;
}

static uint32_t
hash(dstring_t *ds)
{
	return ht_hash(ds->data, ds->used);
}

/*
** This is the routine which parses every netgroup setting up the reverse
** maps byhost and byuser.  This is a recursive routine since netgroups
** are recursive.  The names array contains the names of the netgroups
** above us.  We do not recurse if we have already visited this netgroup.
** It could use up a lot of space for deeply recursive netgroup files.
*/
static int
ng_line(ng_names_t *names, dstring_t *kp, dstring_t *vp)
{
	char *ng, *p, *q;
	dstring_t host, user, domain, *key, *val, *ds, fake;
	ng_names_t this, *tp, *first;
	
	nsd_logprintf(NSD_LOG_MAX, "ng_line: parsing line: %s\n", vp->data);

	ng = vp->data;

	if (! ds_init(&host, 100, 0) || ! ds_init(&user, 100, 0) ||
	    ! ds_init(&domain, 100, 0)) {
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

	while (ng) {
		for (; *ng && (isspace(*ng) || *ng == ','); ng++);
		if (! *ng) {
			break;
		}
		if (*ng == '(') {
			/* triplet */
			
			/* host */
			for (++ng; *ng && isspace(*ng); ng++);
			for (p = ng; *p && ! isspace(*p) && (*p != ',') &&
			    (*p != ')'); p++);
			if (p == ng) {
				ds_set(&host, "*", 1, 0);
			} else {
				if (! ds_set(&host, ng, p - ng, 0)) {
					nsd_logprintf(NSD_LOG_RESOURCE,
					    "ng_line: failed malloc\n");
					goto parse_error;
				}
				for (q = host.data; *q;
				    *q++ = tolower(*q));
			}
			for (ng = p; *ng && isspace(*ng); ng++);
			if (*ng != ',') {
				nsd_logprintf(NSD_LOG_OPER,
    "ng_line: netgroups parse error, expected ',' in: %s, following:\n\t%s\n",
				    kp->data, p);
				goto parse_error;
			}

			/* user */
			for (++ng; *ng && isspace(*ng); ng++);
			for (p = ng; *p && ! isspace(*p) && (*p != ',') &&
			    (*p != ')'); p++);
			if (p == ng) {
				ds_set(&user, "*", 1, 0);
			} else {
				if (! ds_set(&user, ng, p - ng, 0)) {
					nsd_logprintf(NSD_LOG_RESOURCE,
					    "ng_line: failed malloc\n");
					goto parse_error;
				}
			}
			for (ng = p; *ng && isspace(*ng); ng++);
			if (*ng != ',') {
				nsd_logprintf(NSD_LOG_OPER,
    "ng_line: netgroups parse error, expected ',' in: %s, following:\n\t%s\n",
				    kp->data, p);
				goto parse_error;
			}

			/* domain */
			for (++ng; *ng && isspace(*ng); ng++);
			for (p = ng; *p && ! isspace(*p) && (*p != ',') &&
			    (*p != ')'); p++);
			if (p == ng) {
				ds_set(&domain, "*", 1, 0);
			} else {
				if (! ds_set(&domain, ng, p - ng, 0)) {
					nsd_logprintf(NSD_LOG_RESOURCE,
					    "ng_line: failed malloc\n");
					goto parse_error;
				}
				for (q = host.data; *q; *q++ = tolower(*q));
			}
			for (ng = p; *ng && isspace(*ng); ng++);
			if (*ng++ != ')') {
				nsd_logprintf(NSD_LOG_OPER,
    "ng_line: netgroups parse error, expected ')' in: %s, following:\n\t%s\n",
				    kp->data, p);
				goto parse_error;
			}

			/* qualify host */
			if (! ds_append(&host, ".", 1, 0) ||
			    ! ds_cpy(&host, &domain, 0)) {
				nsd_logprintf(NSD_LOG_RESOURCE,
				    "ng_line: failed malloc\n");
				goto parse_error;
			}

			/* qualify user */
			if (! ds_append(&user, ".", 1, 0) ||
			    ! ds_cpy(&user, &domain, 0)) {
				nsd_logprintf(NSD_LOG_RESOURCE,
				    "ng_line: failed malloc\n");
				goto parse_error;
			}

			/* store byhost */
			p = host.data;
			if (isalnum(*p) || *p == '_' || *p == '*') {
				ds = ht_lookup(&byhost, &host);
				if (ds) {
					/* edit value */
					if (! ds_append(ds, ",", 1, 0) ||
					    ! ds_append(ds, first->name,
				            first->len, 0)) {
						nsd_logprintf(NSD_LOG_RESOURCE,
					    "ng_line: failed malloc\n");
						goto parse_error;
					}
				} else {
					/* new */
					key = ds_dup(&host, 0);
					if (! key) {
						nsd_logprintf(NSD_LOG_RESOURCE,
					    "ng_line: failed malloc\n");
						goto parse_error;
					}
					val = ds_new(first->name, first->len,
					    0);
					if (! val) {
						ds_free(key, 0);
						nsd_logprintf(NSD_LOG_RESOURCE,
					    "ng_line: failed malloc\n");
						goto parse_error;
					}
					if (! ht_insert(&byhost, key, val, 0)) {
						ds_free(key, 0);
						ds_free(val, 0);
					}
				}
			}

			/* store byuser */
			p = user.data;
			if (isalnum(*p) || *p == '_' || *p == '*') {
				ds = ht_lookup(&byuser, &user);
				if (ds) {
					/* edit value */
					if (! ds_append(ds, ",", 1, 0) ||
					    ! ds_append(ds, first->name,
				            first->len, 0)) {
						nsd_logprintf(NSD_LOG_RESOURCE,
					    "ng_line: failed malloc\n");
						goto parse_error;
					}
				} else {
					/* new */
					key = ds_dup(&user, 0);
					if (! key) {
						nsd_logprintf(NSD_LOG_RESOURCE,
					    "ng_line: failed malloc\n");
						goto parse_error;
					}
					val = ds_new(first->name, first->len,
					    0);
					if (! val) {
						ds_free(key, 0);
						nsd_logprintf(NSD_LOG_RESOURCE,
					    "ng_line: failed malloc\n");
						goto parse_error;
					}
					if (! ht_insert(&byuser, key, val, 0)) {
						ds_free(key, 0);
						ds_free(val, 0);
					}
				}
			}
		} else {
			/* netgroup */
			for (p = ng; *ng && ! isspace(*ng) && (*ng != ',');
			    ng++);
			this.name = p;
			this.len = ng - p;
			this.last = names;
			for (tp = names; tp; tp = tp->last) {
				if (tp->len == this.len &&
				    (memcmp(tp->name, this.name,
				    this.len) == 0)) {
					continue;
				}
			}
			fake.data = this.name;
			fake.used = this.len;
			ds = ht_lookup(&byname, &fake);
			if (ds) {
				if (! tp) {
					ng_line(&this, &fake, ds);
				}
			}
		}
	}

	ds_clear(&host, 0), ds_clear(&user, 0), ds_clear(&domain, 0);
	return 1;

parse_error:
	ds_clear(&host, 0), ds_clear(&user, 0), ds_clear(&domain, 0);
	return 0;
}

/*
** This routine walks the tree, and calls the ng_line routine above
** to parse that item.
*/
/*ARGSUSED*/
static int
ng_recurse(dstring_t *kp, dstring_t *vp, nsd_file_t *rq)
{
	ng_names_t this;
	
	this.name = kp->data;
	this.len = kp->used;
	this.last = 0;
	ng_line(&this, kp, vp);
	
	return 1;
}

static int
ng_init(time_t version)
{
	if (byname.top) {
		ht_clear(&byname);
	}
	if (! ht_init(&byname, (int (*)(void *, void *))ds_cmp,
	    (void (*)(void *, void *))ds_free,
	    (void (*)(void *, void *))ds_free,
	    (uint32_t (*)(void *))hash, true, true, 0,
	    HT_CHECK_DUPS)) {
		return 0;
	}

	if (byhost.top) {
		ht_clear(&byhost);
	}
	if (! ht_init(&byhost, (int (*)(void *, void *))ds_cmp,
	    (void (*)(void *, void *))ds_free,
	    (void (*)(void *, void *))ds_free,
	    (uint32_t (*)(void *))hash, true, true, 0,
	    HT_CHECK_DUPS)) {
		return 0;
	}

	if (byuser.top) {
		ht_clear(&byuser);
	}
	if (! ht_init(&byuser, (int (*)(void *, void *))ds_cmp,
	    (void (*)(void *, void *))ds_free,
	    (void (*)(void *, void *))ds_free,
	    (uint32_t (*)(void *))hash, true, true, 0,
	    HT_CHECK_DUPS)) {
		return 0;
	}

	ng_version = version;
	
	return 1;
}

/*
** The ng_parse routine will parse the netgroup file into a tree of entries
** for each "line" in the file.  This byname tree is later walked and
** parsed into the reverse maps in the ng_recurse routine above.
*/
static int
ng_parse(file_domain_t *dom)
{
	char *line, *p, *q;
	dstring_t *key, *val;

	if (! ng_init(dom->version.tv_sec)) {
		return 0;
	}

	for (line = dom->map; line && *line; line = strchr(line + 1, '\n')) {
		SKIPWSPACE(line);
		if (*line == '#') {
			continue;
		}

		/*
		** Determine key.
		*/
		for (p = line; *p && ! isspace(*p); p++);
		if (p == line) {
			continue;
		}
		key = ds_new(line, p - line, 0);
		if (! key) {
			nsd_logprintf(NSD_LOG_RESOURCE,
			    "ng_parse: failed malloc\n");
			goto line_error;
		}

		val = calloc(1, sizeof(*val));
		if (! val) {
			ds_free(key, 0);
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
				if (! ds_append(val, p, q - p, 0)) {
					ds_free(key, 0);
					ds_free(val, 0);
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
		if (! ht_insert(&byname, key, val, 0)) {
			/*
			** Already seen this one.
			*/
			ds_free(key, 0);
			ds_free(val, 0);
			continue;
		}
	}

	/*
	** Now we loop over the items filling the user and host trees.
	** This is a recursive routine which can eat up a ton of memory,
	** but . . .
	*/
	if (byname.top) {
		ht_walk(&byname, 0,
		    (int (*)(void *, void *, void *))ng_recurse);
	}

	return 1;

line_error:
	/* Invalidate the tree. */
	ng_version = 0;
	return 0;
}

/*
** This routine simply looks up an element in a tree and returns its
** result.  If the netgroup file has changed since the last lookup
** the file is reparsed.
*/
int
file_netgroup_lookup(nsd_file_t *rq)
{
	char *map, *key, *p;
	file_domain_t *dom;
	htree_t *tp;
	dstring_t *ds, fake;
	int len;
	int (*cmpfunc)(void *, void *);

	nsd_logprintf(NSD_LOG_MIN, "entering file_netgroup_lookup\n");

	/*
	** Parse the netgroup file if this has not already been done.
	*/
	dom = file_domain_lookup(rq);
	if (! dom) {
		rq->f_status = NS_NOTFOUND;
		return NSD_NEXT;
	}

	if (! rq) {
		return NSD_ERROR;
	}
	map = nsd_attr_fetch_string(rq->f_attrs, "table", (char *)0);
	if (! map) {
		rq->f_status = NS_FATAL;
		return NSD_ERROR;
	}

	key = nsd_attr_fetch_string(rq->f_attrs, "key", (char *)0);
	if (! key || ! *key) {
		return NSD_ERROR;
	}

	if (nsd_attr_fetch_bool(rq->f_attrs, "casefold", FALSE)) {
		cmpfunc = (int (*)(void *, void *))ds_casecmp;
	} else {
		cmpfunc = (int (*)(void *, void *))ds_cmp;
	}

	if (strcasecmp(map, NETGROUP_BYNAME) == 0) {
		tp = &byname;
		len = strlen(key);
		p = alloca(len + 1);
		memcpy(p, key, len);
		p[len] = 0;
		for (key = p; *p; *p++ = tolower(*p));
	} else if (strcasecmp(map, NETGROUP_BYHOST) == 0) {
		tp = &byhost;
	} else {
		tp = &byuser;
	}

	if (! tp || (ng_version != dom->version.tv_sec)) {
		if (! ng_parse(dom)) {
			rq->f_status = NS_TRYAGAIN;
			return NSD_ERROR;
		}
		if (strcasecmp(map, NETGROUP_BYNAME) == 0) {
			tp = &byname;
		} else if (strcasecmp(map, NETGROUP_BYHOST) == 0) {
			tp = &byhost;
		} else {
			tp = &byuser;
		}
	}

	fake.data = key;
	fake.used = strlen(key);
	tp->cmp = cmpfunc;
	ds = ht_lookup(tp, &fake);
	if (ds) {
		nsd_set_result(rq, NS_SUCCESS, ds->data, ds->used, VOLATILE);
		nsd_append_result(rq, NS_SUCCESS, "\n", 1);
		return NSD_OK;
	}

	rq->f_status = NS_NOTFOUND;
	return NSD_NEXT;
}

/*
** This is a recursive routine to walk a table and append each item
** value to the result.
*/
static int
ng_append(dstring_t *kp, dstring_t *vp, nsd_file_t *rq)
{
	if (vp->used > 0) {
		nsd_append_result(rq, NS_SUCCESS, kp->data, kp->used);
		nsd_append_result(rq, NS_SUCCESS, "\t", 1);

		nsd_append_result(rq, NS_SUCCESS, vp->data, vp->used);
		nsd_append_result(rq, NS_SUCCESS, "\n", 1);
	}
	
	return 1;
}

/*
** This is the routine to enumerate an entire table.
*/
int
file_netgroup_list(nsd_file_t *rq)
{
	file_domain_t *dom;
	htree_t *tp;
	char *map;
	
	nsd_logprintf(NSD_LOG_MIN, "entering file_netgroup_list\n");

	if (! rq) {
		return NSD_ERROR;
	}

	dom = file_domain_lookup(rq);
	if (! dom) {
		rq->f_status = NS_NOTFOUND;
		return NSD_NEXT;
	}

	map = nsd_attr_fetch_string(rq->f_attrs, "table", (char *)0);
	if (! map) {
		rq->f_status = NS_FATAL;
		return NSD_ERROR;
	}

	if (strcasecmp(map, NETGROUP_BYNAME) == 0) {
		tp = &byname;
	} else if (strcasecmp(map, NETGROUP_BYHOST) == 0) {
		tp = &byhost;
	} else {
		tp = &byuser;
	}

	if (! tp || (ng_version != dom->version.tv_sec)) {
		if (! ng_parse(dom)) {
			rq->f_status = NS_TRYAGAIN;
			return NSD_ERROR;
		}
		if (strcasecmp(map, NETGROUP_BYNAME) == 0) {
			tp = &byname;
		} else if (strcasecmp(map, NETGROUP_BYHOST) == 0) {
			tp = &byhost;
		} else {
			tp = &byuser;
		}
	}
	
	ht_walk(tp, rq, (int (*)(void *, void *, void *))ng_append);
	
	return NSD_NEXT;
}
