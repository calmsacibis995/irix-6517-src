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
file_networkByName(nsd_file_t *rq)
{
	int len, clen;
	char *line, *key, *p;
	file_domain_t *dom;
	int (*cmpfunc)(const char *, const char *, size_t);

	/*
	** Open the network file if not already done.
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
		if (! *line || *line == '#') continue;
		for (p = line; p && *p && *p != '\n';
		    p = strpbrk(p, " \t\n#")) {
			SKIPSPACE(p);
			if (! *p || *p == '#') break;
			clen = strcspn(p, " \t\n#");
			if (clen == len) {
				if ((cmpfunc)(key, line, len) == 0) {
					len = strcspn(line, "\n");
					nsd_set_result(rq, NS_SUCCESS,
					    line, len, VOLATILE);
					nsd_append_result(rq, NS_SUCCESS, "\n",
					    sizeof("\n"));
					return NSD_OK;
				}
			}
		}
	}

	rq->f_status = NS_NOTFOUND;
	return NSD_NEXT;
}

static int
file_networkByAddr(nsd_file_t *rq)
{
	int len, clen;
	char *p, *line, *key;
	file_domain_t *dom;

	/*
	** Open the network file if not already done.
	*/
	dom = file_domain_lookup(rq);
	if (! dom) {
		rq->f_status = NS_NOTFOUND;
		return NSD_NEXT;
	}

	/*
	** Start at the beginning and do a linear seach for the addr.
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
	for (line = dom->map; (line != NULL) && (*line != (char)0);
	    line = strchr(line + 1, '\n')) {
		SKIPWSPACE(line);
		if (! *line || *line == '#') continue;
		p = line;
		TOSPACE(p);
		SKIPSPACE(p);
		clen = strcspn(p, " \t\n#");
		if (clen == len) {
			if (strncmp(key, p, len) == 0) {
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

int
file_networks_lookup(nsd_file_t *rq)
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

	if (strcasecmp(map, NETWORKS_BYNAME) == 0) {
		return file_networkByName(rq);
	} else {
		return file_networkByAddr(rq);
	}
}

int
file_networks_list(nsd_file_t *rq)
{
	file_domain_t *dom;

	if (! rq) {
		return NSD_ERROR;
	}

	/*
	** Open the networks file if not already done.
	*/
	dom = file_domain_lookup(rq);
	if (! dom) {
      		rq->f_status = NS_NOTFOUND;
		return NSD_NEXT;
	}
	
	nsd_append_element(rq, NS_SUCCESS, dom->map, dom->size);
	if (dom->map[dom->size - 1] != '\n') {
		nsd_append_result(rq, NS_SUCCESS, "\n", sizeof("\n"));
	}

	rq->f_status = NS_NOTFOUND;
	return NSD_NEXT;
}
