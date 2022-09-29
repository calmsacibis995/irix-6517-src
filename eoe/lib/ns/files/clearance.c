#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <ns_api.h>
#include <ns_daemon.h>
#include "ns_files.h"

#ident "$Revision: 1.6 $"

int
file_clearance_lookup(nsd_file_t *rq)
{
	size_t len, clen;
	char *line, *key;
	file_domain_t *dom;
	int (*cmpfunc)(const char *, const char *, size_t);

	if (! rq) {
		return NSD_ERROR;
	}

	/*
	** Open the clearance file if not already done.
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
	if (len == 0) {
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


int
file_clearance_list(nsd_file_t *rq)
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
