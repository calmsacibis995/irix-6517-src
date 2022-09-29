#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <malloc.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <ns_api.h>
#include <ns_daemon.h>
#include "ns_files.h"

extern char _files_result[];

static int
file_groupByName(nsd_file_t *rq)
{
	int len, clen;
	char *line, *key;
	file_domain_t *dom;
	int (*cmpfunc)(const char *, const char *, size_t);

	/*
	** Open the group file if not already done.
	*/
	dom = file_domain_lookup(rq);
	if (! dom) {
		rq->f_status = NS_NOTFOUND;
		return NSD_NEXT;
	
	}

	/*
	** Start at the beginning and do a linear seach for the name.
	*/
	key = nsd_attr_fetch_string(rq->f_attrs, "key", (char *)0);
	if (! key) {
		rq->f_status = NS_FATAL;
		return NSD_ERROR;
	}
	len = strlen(key);
	if (len <= 0) {
		rq->f_status = NS_FATAL;
		return NSD_ERROR;
	}

	if (nsd_attr_fetch_bool(rq->f_attrs, "casefold", FALSE)) {
		cmpfunc = strncasecmp;
	} else {
		cmpfunc = strncmp;
	}
	for (line = dom->map; line && *line; line = strchr(line + 1, '\n')) {
		SKIPWSPACE(line);
		if (! *line || *line == '#') {
			continue;
		}
		clen = strcspn(line, ":\n");
		if (clen == len) {
			if ((cmpfunc)(key, line, len) == 0) {
				len = strcspn(line, "\n");
				nsd_set_result(rq, NS_SUCCESS, line,
				    len, VOLATILE);
				nsd_append_result(rq, NS_SUCCESS, "\n",
				    sizeof("\n"));
				return NSD_OK;
			}
		}
	}

	rq->f_status = NS_NOTFOUND;
	return NSD_NEXT;
}

static int
file_groupByGid(nsd_file_t *rq)
{
	char *p, *q, *line, *key;
	unsigned long gid, check;
	int count, len;
	file_domain_t *dom;

	/*
	** Open the group file if not already done.
	*/
	dom = file_domain_lookup(rq);
	if (! dom) {
		rq->f_status = NS_NOTFOUND;
		return NSD_NEXT;
	}

	/*
	** Start at the beginning and do a linear seach for the gid.
	*/
	key = nsd_attr_fetch_string(rq->f_attrs, "key", (char *)0);
	if (! key) {
		rq->f_status = NS_FATAL;
		return NSD_ERROR;
	}
	gid = strtol(key, (char **)0, 10);
	for (line = dom->map; line && *line; line = strchr(line + 1, '\n')) {
		SKIPWSPACE(line);
		if (! *line || *line == '#') {
			continue;
		}
		count = 0;
		for (p = line; *p; p++) {
			if (*p == '\n') {
				break;
			}
			if (*p == ':') {
				count++;
				continue;
			}
			if (count == 2) {
				check = strtol(p, &q, 10);
				if (check == gid && p != q) {
					len = strcspn(line, "\n");
					nsd_set_result(rq, NS_SUCCESS,
					    line, len, VOLATILE);
					nsd_append_result(rq, NS_SUCCESS,
					    "\n", sizeof("\n"));
					return NSD_OK;
				}
				break;
			}
		}
	}

	rq->f_status = NS_NOTFOUND;
	return NSD_NEXT;
}

/*
** The group.bymember map requires us to walk the file looking for
** a matching member and building a record to return.  The returning
** data is of the format:
**	name: gid, gid, gid, gid, . . .
*/
int
file_groupByMember(nsd_file_t *rq)
{
	int len, rlen, klen;
	char *p, *r, *key, *end;
	file_domain_t *dom;

	/*
	** Open the group file if not already done.
	*/
	dom = file_domain_lookup(rq);
	if (! dom) {
		rq->f_status = NS_NOTFOUND;
		return NSD_NEXT;
	}

	/*
	** Initialize our result if needed.  group.bymember is a concatenation
	** of each of the callout methods defined so only the first method
	** has to initialize the result.
	*/
	key = nsd_attr_fetch_string(rq->f_attrs, "key", (char *)0);
	if (! key) {
		rq->f_status = NS_FATAL;
		return NSD_ERROR;
	}
	klen = strlen(key);
	if (! rq->f_data) {
		rlen = nsd_cat(_files_result, 1024, 2, key, ":");
		nsd_set_result(rq, NS_SUCCESS, _files_result, rlen,
		    VOLATILE);
	}

	/*
	** Use strstr to find matching members, then verify that they are
	** within a valid group line.
	*/
	p = dom->map;
	end = dom->map + dom->size - 1;
	while ((p < end) && (p = strstr(p+1, key))) {
		/*
		** Verify that this is not a substring of something else.
		*/
		r = p - 1;
		if (!(r < dom->map || isspace(*r) || *r == ',' || *r == ':') ||
		    !(isspace(p[klen]) || p[klen] == ',' ||
		     p[klen] == (char)0)) {
			continue;
		}

		/*
		** Backup to beginning of line.
		*/
		for (r = p; (r > dom->map) && *r && (*r != '\n'); --r);

		/*
		** Skip if we are in comment.
		*/
		for (; *r && isspace(*r); r++);
		if (*r == '#') {
			continue;
		}

		/*
		** Forward to gid.
		*/
		for (r++; *r && (*r != ':') && (*r != '\n'); r++);
		for (r++; *r && (*r != ':') && (*r != '\n'); r++);
		if (! *r || (*r == '\n') || (r > p)) {
			continue;
		}

		/*
		** Append gid to record.
		*/
		len = strcspn(++r, ": \t\n");
		strncpy(_files_result, r, len);
		strcpy(&_files_result[len], ",");
		nsd_append_result(rq, NS_SUCCESS, _files_result,
		    len + sizeof(","));
	}

	return NSD_NEXT;
}

int
file_group_lookup(nsd_file_t *rq)
{
	char *map;

	if (! rq) {
		return NSD_ERROR;
	}

	map = nsd_attr_fetch_string(rq->f_attrs, "table", (char *)0);
	if (! map) {
		rq->f_status = NS_FATAL;
		return NSD_ERROR;
	}

	if (strcasecmp(map, GROUP_BYNAME) == 0) {
		return file_groupByName(rq);
	} else if (strcasecmp(map, GROUP_BYGID) == 0) {
		return file_groupByGid(rq);
	} else {
		return file_groupByMember(rq);
	}
}

int
file_group_list(nsd_file_t *rq)
{
	file_domain_t *dom;

	if (! rq) {
		return NSD_ERROR;
	}

	dom = file_domain_lookup(rq);
	if (! dom) {
		rq->f_status = NS_NOTFOUND;
		return NSD_NEXT;
	}

	nsd_append_element(rq, NS_SUCCESS, dom->map, dom->size);
	if (dom->map[dom->size - 1] != '\n') {
		nsd_append_result(rq, NS_SUCCESS, "\n", sizeof("\n"));
	}

	return NSD_NEXT;
}
