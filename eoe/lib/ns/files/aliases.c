#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <alloca.h>
#include <ctype.h>
#include <unistd.h>
#include <ns_api.h>
#include <ns_daemon.h>
#include "ns_files.h"

extern char _files_result[];

/* ARGSUSED */
static int
file_aliasByName(nsd_file_t *rq)
{
	rq->f_status = NS_NOTFOUND;
	return NSD_NEXT;
}

/* ARGSUSED */
static int
file_aliasByAddr(nsd_file_t *rq)
{
	rq->f_status = NS_NOTFOUND;
	return NSD_NEXT;
}

int
file_aliases_lookup(nsd_file_t *rq)
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

	if (strcasecmp(map, ALIASES_BYNAME) == 0) {
		return file_aliasByName(rq);
	} else {
		return file_aliasByAddr(rq);
	}
}

/* ARGSUSED */
int
file_aliases_list(nsd_file_t *rq)
{
	return NSD_NEXT;
}
