/*
** This file implements an absolute skeleton lamed method library.
** For a more complicated example look at one of the real methods.
*/

#include <stdio.h>
#include <ns_api.h>
#include <ns_daemon.h>

/*
** The init routine is called when this library is first mapped in
** or when the daemon receives a SIGHUP signal.
*/
int
init(void)
{
	nsd_logprintf(NSD_LOG_OPER, "init (skel)\n");
	return NSD_OK;
}

/*
** The lookup routine is called to fetch a single element from a table.
** The library should simple fetch the item, call nsd_set_result with
** the resulting string, and return NSD_OK.  If the item does not exist
** return NSD_NEXT.
*/
int
lookup(nsd_file_t *rq)
{
	char *map, *domain;

	map = nsd_attr_fetch_string(rq->f_attrs, "table", "unknown");
	domain = nsd_attr_fetch_string(rq->f_attrs, "domain", "unknown");

	nsd_logprintf(NSD_LOG_OPER, 
	    "lookup (skel): called with domain = %s, map = %s, key = %s\n",
	    domain, map, rq->f_name);
	nsd_set_result(rq, NS_SUCCESS, "lookup success\n",
	    sizeof("lookup success\n"), STATIC);
	return NSD_OK;
}

/*
** This list routine is called to fully enumerate a table.  This is used
** by the getXent() style library routines.  The library should simply
** enumerate the table into a buffer and call nsd_append_element() with
** the result, then return NSD_NEXT.
*/
int
list(nsd_file_t *rq)
{
	char *domain, *map;

	map = nsd_attr_fetch_string(rq->f_attrs, "table", "unknown");
	domain = nsd_attr_fetch_string(rq->f_attrs, "domain", "unknown");

	nsd_logprintf(NSD_LOG_OPER, 
	    "list (skel): called with domain = %s, map = %s, key = %s\n",
	    domain, map, rq->f_name);
	nsd_append_result(rq, NS_SUCCESS, "list success\n",
	    sizeof("list success\n"));
	return NSD_NEXT;
}

/*
** The dump command just prints the current state to the supplied file
** pointer.  When the daemon receives the SIGUSR1 signal it dumps its
** internal status then calls the dump routine in each mapped library.
*/
int
dump(FILE *fp)
{
	fprintf(fp, "this library has no state\n");

	return NSD_OK;
}
