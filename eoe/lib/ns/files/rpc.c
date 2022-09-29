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
file_rpcByName(nsd_file_t *rq)
{
	int len, clen;
	char *line, *key, *p;
	file_domain_t *dom;
	int (*cmpfunc)(const char *, const char *, size_t);

	/*
	** Open the rpc file if not already done.
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
				if ((cmpfunc)(key, p, len) == 0) {
					len = strcspn(line, "\n");
					nsd_set_result(rq, NS_SUCCESS,
					    line, len, VOLATILE);
					nsd_append_result(rq, NS_SUCCESS,
					    "\n", sizeof("\n"));
					return NSD_OK;
				}
			}
		}
	}

	rq->f_status = NS_NOTFOUND;
	return NSD_NEXT;
}

static int
file_rpcByNumber(nsd_file_t *rq)
{
	char *p, *line, *key;
	unsigned long number, check;
	int len;
	file_domain_t *dom;

	/*
	** Open the rpc file if not already done.
	*/
	dom = file_domain_lookup(rq);
	if (! dom) {
		rq->f_status = NS_NOTFOUND;
		return NSD_NEXT;
	}

	/*
	** Start at the beginning and do a linear seach for the number.
	*/
	key = nsd_attr_fetch_string(rq->f_attrs, "key", (char *)0);
	if (! key) {
      		rq->f_status = NS_FATAL;
		return NSD_ERROR;
	}
	number = strtoul(key, (char **)0, 10);
	for (line = dom->map; line && *line; line = strchr(line + 1, '\n')) {
		SKIPWSPACE(line);
		if (! *line || *line == '#') continue;
		p = line;
		TOSPACE(p);
		SKIPSPACE(p);
		check = strtoul(p, (char **)0, 10);
		if (*p && check == number) {
			len = strcspn(line, "\n");
			memcpy(_files_result, line, len);
			_files_result[len] = '\n';
			nsd_set_result(rq, NS_SUCCESS,
			    _files_result, len + 1, VOLATILE);
			return NSD_OK;
		}
	}

	rq->f_status = NS_NOTFOUND;
	return NSD_NEXT;
}

int
file_rpc_lookup(nsd_file_t *rq)
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

	if (strcasecmp(map, RPC_BYNAME) == 0) {
		return file_rpcByName(rq);
	} else {
		return file_rpcByNumber(rq);
	}
}

int
file_rpc_list(nsd_file_t *rq)
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
