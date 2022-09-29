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

int
file_shadow_lookup(nsd_file_t *rq)
{
	int len, clen;
	char *line, *key;
	file_domain_t *dom;
	int (*cmpfunc)(const char *, const char *, size_t);

	nsd_logprintf(NSD_LOG_LOW, "Entering file_shadow_lookup:\n");

	if (! rq) {
		nsd_logprintf(NSD_LOG_HIGH, "\tno request\n");
		return NSD_ERROR;
	}

	/*
	** Open the shadow file if not already done.
	*/
	dom = file_domain_lookup(rq);
	if (! dom) {
		nsd_logprintf(NSD_LOG_HIGH, "\tfailed to open file\n");
		rq->f_status = NS_NOTFOUND;
		return NSD_NEXT;
	}

	/*
	** Start at the beginning and do a linear seach for the name.
	*/
	key = nsd_attr_fetch_string(rq->f_attrs, "key", (char *)0);
	if (! key) {
		rq->f_status = NS_FATAL;
		nsd_logprintf(NSD_LOG_HIGH, "\tno key\n");
		return NSD_ERROR;
	}
	len = strlen(key);
	if (len <= 0) {
		rq->f_status = NS_FATAL;
		nsd_logprintf(NSD_LOG_HIGH, "\tinvalid length\n");
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
				nsd_logprintf(NSD_LOG_HIGH, "\tsuccess\n");
				return NSD_OK;
			}
		}
	}

	nsd_logprintf(NSD_LOG_HIGH, "\tnot found\n");
	rq->f_status = NS_NOTFOUND;
	return NSD_NEXT;
}

int
file_shadow_list(nsd_file_t *rq)
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
