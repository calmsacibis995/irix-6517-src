/*
** This file contains routines to parse the nsswitch.conf files.
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <dlfcn.h>
#include <errno.h>
#include <ns_api.h>
#include <ns_daemon.h>

nsd_libraries_t *nsd_liblist = (nsd_libraries_t *)0;

/*
** The following are a bunch of utility functions for dynamic strings
** which are used throughout the parser.
*/
typedef struct {
	char	*data;
	int	size;
	int	used;
} dstring_t;

/* Initiallize a dynamic string structure */
static int
sinit(dstring_t *ds, int len)
{
	if (ds) {
		ds->data = (char *)nsd_malloc(len);
		if (! ds->data) {
			return 0;
		}

		ds->size = len;
		ds->used = 0;

		return 1;
	}

	return 0;
}

/* Free memory for string */
static void
sclear(dstring_t *ds)
{
	if (ds && ds->data) {
		free(ds->data);
	}
}

/* Null out string */
static void
sreset(dstring_t *ds)
{
	if (ds->data) {
		*ds->data = (char)0;
	}
	ds->used = 0;
}

/* Append to string, realloc memory as needed */
static int
sappend(dstring_t *ds, char *c, int len)
{
	int needed;
	char *n;

	if (! ds) {
		return 0;
	}

	needed = ds->used + len + 1;
	if (ds->size > needed) {
		memcpy(ds->data + ds->used, c, len);
		ds->used += len;
		ds->data[ds->used] = (char)0;
	} else {
		n = (char *)nsd_malloc(needed);
		if (! n) {
			return 0;
		}
		ds->size = needed;

		if (ds->data) {
			memcpy(n, ds->data, ds->used);
			free(ds->data);
		}

		memcpy(n + ds->used, c, len);
		ds->data = n;
		ds->used += len;
		ds->data[ds->used] = (char)0;
	}

	return 1;
}

/*
** This routine sets init failure on all the libraries to force us
** to call the init routines again the next time through.
*/
void
nsd_nsw_init(void)
{
	nsd_libraries_t *lp;

	for (lp = nsd_liblist; lp; lp = lp->l_next) {
		lp->l_state &= ~LIB_INIT_FAILED;
		lp->l_state |= LIB_INIT_RETRY;
	}
}

/*
** This routine searches the libraries list, and returns a pointer to
** the matching library structure.  If one does not exist it attempts
** to open the library and run the initiallization routine.
*/
static nsd_libraries_t *
nsw_library(char *lib, int len, char *file, int lineno)
{
	nsd_libraries_t *lp;
	char buf[MAXPATHLEN];
	struct stat sbuf;

	/* Search for a pre-existing library with this name. */
	for (lp = nsd_liblist; lp; lp = lp->l_next) {
		if ((strncasecmp(lp->l_name, lib, len) == 0) &&
		    (strlen(lp->l_name) == len)) {
			if (lp->l_handle && !(lp->l_state & LIB_INIT_FAILED)) {
				if (lp->l_state & LIB_INIT_RETRY) {
					/*
					** Re-call init routine.
					*/
					lp->l_state &= ~LIB_INIT_RETRY;
					if (lp->l_funcs[INIT]) {
						if ((*lp->l_funcs[INIT])(
						    (char *)0) != NSD_OK) {
							lp->l_state |=
							    LIB_INIT_FAILED;
							return 0;
						}
					}
					lp->l_state &= ~LIB_INIT_FAILED;
				}
				return lp;
			}

			/* failed open last time through */
			return (nsd_libraries_t *)0;
		}
	}

	/* Create a new library structure and add it to list. */
	lp = (nsd_libraries_t *)nsd_calloc(1, sizeof(*lp) + len + 1);
	if (! lp) {
		nsd_logprintf(NSD_LOG_RESOURCE, "nsw_library: failed malloc\n");
		return (nsd_libraries_t *)0;
	}

	lp->l_name = (char *)(lp + 1);
	memcpy(lp->l_name, lib, len);
	lp->l_next = nsd_liblist;
	nsd_liblist = lp;

	/* Check the existence of the library. */
	nsd_cat(buf, sizeof(buf), 4, NS_LIB_DIR, "libns_", lp->l_name, ".so");

	if (stat(buf, &sbuf) != 0) {
		if (lineno) {
			nsd_logprintf(NSD_LOG_OPER,
			    "Could not open library %s in %s line %d: %s\n",
			    lp->l_name, file, lineno, strerror(errno));
		} else {
			nsd_logprintf(NSD_LOG_OPER,
		    "Could not open library %s for default table %s: %s\n",
			    lp->l_name, file, strerror(errno));
		}
		return (nsd_libraries_t *)0;
	}

	/* Open the library. */
	lp->l_handle = dlopen(buf, RTLD_NOW);
	if (! lp->l_handle) {
		if (lineno) {
			nsd_logprintf(NSD_LOG_OPER,
	    "nsw_library: failed dlopen for library %s in %s line %d: %s\n",
			    lp->l_name, file, lineno, dlerror());
		} else {
			nsd_logprintf(NSD_LOG_OPER,
    "nsw_library: failed dlopen for library %s for default table %s: %s\n",
			    lp->l_name, file, dlerror());
		}
		return (nsd_libraries_t *)0;
	}

	/* Lookup the symbols that we care about. */
	lp->l_funcs[INIT] = (nsd_init_proc *)dlsym(lp->l_handle, "init");
	lp->l_funcs[LOOKUP] = (nsd_callout_proc *)dlsym(lp->l_handle,
	    "lookup");
	lp->l_funcs[LIST] = (nsd_callout_proc *)dlsym(lp->l_handle, "list");
	lp->l_funcs[SHAKE] = (nsd_shake_proc *)dlsym(lp->l_handle, "shake");
	lp->l_funcs[DUMP] = (nsd_dump_proc *)dlsym(lp->l_handle, "dump");
	lp->l_funcs[VERIFY] = (nsd_verify_proc *)dlsym(lp->l_handle,
	    "verify");

	/* Call the init routine if it exists. */
	if (lp->l_funcs[INIT]) {
		nsd_logprintf(NSD_LOG_MIN,
		    "loading dynamic method: %s\n",lp->l_name);
		if ((*lp->l_funcs[INIT])((char *)0) != NSD_OK) {
			lp->l_state |= LIB_INIT_FAILED;
			return (nsd_libraries_t *)0;
		}
	}

	return lp;
}

/*
** This routine parses a criterion of the format:
** 	/\[?\s*([^\s\]=])\s*=\s*(^\s\]=])\s*\]?/
** like:
**	[status=action]
** where status could be one of:
**	success, notfound, unavail, tryagain
** and action can be one of:
**	return, continue
*/
static int
nsw_criterion(nsd_file_t *lib, char *crit, char *file, int lineno)
{
	char *p;
	int index;

	p = strchr(crit, ']');
	if (p) {
		*p = (char)0;
	}
	for (p = crit; *p && isspace(*p); p++);
	if (*p == '[') {
		p++;
	}
	for (; *p && isspace(*p); p++);
	if (! *p) {
		return NSD_ERROR;
	}

	do {
		if (strncasecmp(p, "success", sizeof("success") - 1) == 0) {
			index = NS_SUCCESS;
		} else if (strncasecmp(p, "notfound", sizeof("notfound") - 1)
		    == 0) {
			index = NS_NOTFOUND;
		} else if (strncasecmp(p, "unavail", sizeof("unavail") - 1)
		    == 0) {
			index = NS_UNAVAIL;
		} else if (strncasecmp(p, "tryagain", sizeof("tryagain") - 1)
		    == 0) {
			index = NS_TRYAGAIN;
		} else if (strncasecmp(p, "noperm", sizeof("noperm") - 1)
		    == 0) {
			index = NS_NOPERM;
		} else {
			nsd_logprintf(NSD_LOG_OPER,
	    "nsw_criterion: error in %s at %d: unknown status type\n",
			    file, lineno);
		}

		p = strchr(p, '=');
		if (! p) {
			nsd_logprintf(NSD_LOG_OPER,
			    "nsw_criterion: error in %s at %d: missing '='\n",
			    file, lineno);
			return NSD_ERROR;
		}
		for (++p; *p && isspace(*p); p++);

		if (strncasecmp(p, "return", sizeof("return") - 1) == 0) {
			lib->f_control[index] |= NSD_RETURN;
		} else if (strncasecmp(p, "continue", sizeof("continue") - 1)
		    == 0) {
			lib->f_control[index] |= NSD_CONTINUE;
		} else {
			nsd_logprintf(NSD_LOG_OPER,
		    "nsw_criterion: error in %s at %d: unknown action\n",
			    file, lineno);
		}

		p = strchr(p, ',');
	} while (p);

	return NSD_OK;
}

/*
** This is just a utility function for the nsw_line proceedure below.  It
** copies an entire table just changing the name.
*/
static nsd_file_t *
nsw_copy(nsd_file_t *domain, nsd_file_t *table, char *name, int len,
    char *file, int lineno)
{
	nsd_file_t *nt;

	if (nsd_file_init(&nt, name, len, domain, NFDIR, 0) != NSD_OK) {
		nsd_logprintf(NSD_LOG_RESOURCE,
		    "nsw_copy: failed to create new table: %s\n", name);
		return 0;
	}

	/* Copy attributes. */
	nsd_attr_clear(nt->f_attrs);
	nt->f_attrs = nsd_attr_copy(table->f_attrs);
	nsd_attr_continue(&nt->f_attrs, domain->f_attrs);
	table->f_mode = nsd_attr_fetch_long(table->f_attrs, "mode",
	    8, (long)domain->f_mode);
	table->f_uid = nsd_attr_fetch_long(table->f_attrs, "owner",
	    10, (long)domain->f_uid);
	table->f_gid = nsd_attr_fetch_long(table->f_attrs, "group",
	    10, (long)domain->f_gid);
	nsd_attr_store(&nt->f_attrs, "table", name);

	/* Find the cache file for this service. */
	nt->f_map = nsd_map_get(name);

	/* Duplicate the callout list. */
	nsd_file_dupcallouts(table, nt);

	if (nsd_file_appenddir(domain, nt, name, len) != NSD_OK) {
		if (lineno) {
			nsd_logprintf(NSD_LOG_MIN,
			    "table %s already exists in file %s line %d\n",
			    name, file, lineno);
		}
		return 0;
	}

	return nt;
}

/*
** This parses a line from nsswitch.conf and appends each table to the
** given domain.  A line is of the format:
**	(attrlist) table (attrlist): lib1 (attrlist) [control] lib2 ...
** where any of the attrlist and control blocks may not exist.
*/
static int
nsw_line(nsd_file_t *domain, char *line, char *file, int lineno)
{
	char *p, *q;
	nsd_file_t *table, *lib=0, *nt;
	nsd_libraries_t *lp;
	int len;

	/* Global domain attributes */
	for (p = line; *p && isspace(*p); p++);
	if (*p == '(') {
		q = strchr(p, ')');
		if (! q) {
			nsd_logprintf(NSD_LOG_OPER,
"nsw_line: unterminated attribute list at beginning of line %d in file %s\n",
			    lineno, file);
			return NSD_ERROR;
		}
		*q++ = (char)0;
		nsd_attr_parse(&domain->f_attrs, p, lineno, file);
		p = q;
	}

	/* Table name */
	for (; *p && isspace(*p); p++);
	len = strcspn(p, " \t:#(");
	if (len == 0) {
		return NSD_OK;
	}
	if (nsd_file_byname(domain->f_data, p, len)) {
		/* Table already exists in database. */
		if (lineno) {
			nsd_logprintf(NSD_LOG_RESOURCE,
			    "nsw_line: duplicate table %*.*s in %s line %d\n",
			    len, len, p, file, lineno);
		}
		return NSD_ERROR;
	}
	if (nsd_file_init(&table, p, len, domain, NFDIR, 0) != NSD_OK) {
		if (lineno) {
			nsd_logprintf(NSD_LOG_RESOURCE,
	    "nsw_line: failed to create new table %*.*s from %s line %d\n",
			    len, len, p, file, lineno);
		} else {
			nsd_logprintf(NSD_LOG_RESOURCE,
    "nsw_line: failed to create new table %*.*s from default table %s\n",
			    len, len, p, file);
		}
		return NSD_ERROR;
	}

	/* Table attributes */
	for (p += len; *p && isspace(*p); p++);
	if (*p == '(') {
		q = strchr(p, ')');
		if (! q) {
			nsd_logprintf(NSD_LOG_OPER,
	"nsw_line: unterminated attribute list on table line %d in file %s\n",
			    lineno, file);
			nsd_file_clear(&table);
			return NSD_ERROR;
		}
		*q++ = (char)0;
		nsd_attr_parse(&table->f_attrs, p, lineno, file);
		p = q;
	}
	nsd_attr_store(&table->f_attrs, "table", table->f_name);

	/* 
	** If the table has the boolean attribute "dynamic" set, set
	** the NSD_FLAGS_MKDIR bit in table->f_flags.  This allows us to
	** not have to do a lot of attribute searches later.
	*/
	if (nsd_attr_fetch_bool(table->f_attrs, "dynamic", 0)) {
		table->f_flags |= NSD_FLAGS_MKDIR;
	}

	for (; *p && isspace(*p); p++);
	if (*p++ != ':') {
		nsd_logprintf(NSD_LOG_OPER,
		    "nsw_line: missing ':' on line %d in file %s\n",
		    lineno, file);
		nsd_file_clear(&table);
		return NSD_ERROR;
	}
	table->f_mode = nsd_attr_fetch_long(table->f_attrs, "mode",
	    8, (long)domain->f_mode);
	table->f_uid = nsd_attr_fetch_long(table->f_attrs, "owner",
	    10, (long)domain->f_uid);
	table->f_gid = nsd_attr_fetch_long(table->f_attrs, "group",
	    10, (long)domain->f_gid);


	/* Libraries */
	while (*p) {
		for (; *p && isspace(*p); p++);
		if (*p == '(') {
			nsd_logprintf(NSD_LOG_OPER,
		    "nsw_line: unexpected attribute string in %s on line %d\n",
			    file, lineno);
			nsd_file_clear(&table);
			return NSD_ERROR;
		}
		if (*p == '[') {
			nsd_logprintf(NSD_LOG_OPER,
		    "nsw_line: unexpected control string in %s on line %d\n",
			    file, lineno);
			nsd_file_clear(&table);
			return NSD_ERROR;
		}

		/* Library */
		len = strcspn(p, " \t([");
		lp = nsw_library(p, len, file, lineno);
		if (lp) {
			*(p - 1) = '.';
			if (nsd_file_init(&lib, p - 1, len + 1, table, NFDIR, 0)
			    != NSD_OK) {
				nsd_logprintf(NSD_LOG_RESOURCE,
	    "nsw_item: failed to add library %*.*s in file %s line %d\n",
				    len, len, p, file, lineno);
				nsd_file_clear(&table);
				return NSD_ERROR;
			}
			if (nsd_file_appendcallout(table, lib, lib->f_name,
			    lib->f_namelen) != NSD_OK) {
				nsd_logprintf(NSD_LOG_RESOURCE,
		  "nsw_item: failed to add library %s in file %s line %d\n",
				    lib->f_name, file, lineno);
				nsd_file_clear(&lib);
				nsd_file_clear(&table);
				return NSD_ERROR;
			}
			lib->f_lib = lp;
			nsd_attr_store(&lib->f_attrs, "library", lp->l_name);
		} else {
			lib = (nsd_file_t *)0;
		}

		for (p += len; *p && isspace(*p); p++);
		if (*p == '(') {
			/* Library attributes */
			q = strchr(p, ')');
			if (! q) {
				nsd_logprintf(NSD_LOG_OPER,
	    "nsw_item: unterminated attribute string in %s on line %d\n",
				    file, lineno);
				nsd_file_clear(&lib);
				nsd_file_clear(&table);
				return NSD_ERROR;
			}
			*q++ = (char)0;
			if (lib) {
				nsd_attr_parse(&lib->f_attrs, p, lineno, file);
			}
			p = q;
		}

		for (; *p && isspace(*p); p++);
		if (*p == '[') {
			/* Library control */
			q = strchr(p, ']');
			if (! q) {
				nsd_logprintf(NSD_LOG_OPER,
		    "nsw_item: unterminated control string in %s on line %d\n",
				    file, lineno);
				nsd_file_clear(&lib);
				nsd_file_clear(&table);
				return NSD_ERROR;
			}
			*q++ = (char)0;
			if (lib) {
				nsw_criterion(lib, p, file, lineno);
			}
			p = q;
		}

		/*
		** Fix permissions.
		*/
		if (lib) {
			lib->f_mode = nsd_attr_fetch_long(lib->f_attrs,
			    "mode", 8, (long)table->f_mode);
		}
	}

	if (! nsd_file_getcallout(table)) {
		nsd_logprintf(NSD_LOG_MIN, "null table %s in file %s line %d\n",
		    table->f_name, file, lineno);
		nsd_file_clear(&table);
		return NSD_ERROR;
	}

	if (nsd_file_appenddir(domain, table, table->f_name, table->f_namelen)
	    != NSD_OK) {
		if (lineno) {
			nsd_logprintf(NSD_LOG_MIN,
			    "table %s already exists in file %s line %d\n",
			    table->f_name, file, lineno);
		}
		nsd_file_clear(&table);
		return NSD_ERROR;
	}

	/* Expand aliases */
	if ((strcasecmp(table->f_name, "aliases") == 0) ||
	    (strcasecmp(table->f_name, "mail") == 0)) {
		nsd_file_appenddir(domain, table, "mail",
		    sizeof("mail") - 1);
		nsd_file_appenddir(domain, table, "aliases",
		    sizeof("aliases") - 1);
		nsd_file_appenddir(domain, table, "mail.aliases",
		    sizeof("mail.aliases") - 1);
		nsd_attr_append(&table->f_attrs, "table", "mail.aliases",TRUE);
		table->f_map = nsd_map_get("mail.aliases");
		nsw_copy(domain, table, "mail.byaddr",
		    sizeof("mail.byaddr") - 1, file, lineno);
		if (!nsd_attr_fetch(table->f_attrs, "casefold")) {
			nsd_attr_append(&table->f_attrs, "casefold", "true", 
					TRUE);
		}
	}
	else if (strcasecmp(table->f_name, "mail.aliases") == 0) {
		if (!nsd_attr_fetch(table->f_attrs, "casefold")) {
			nsd_attr_append(&table->f_attrs, "casefold", "true",
					TRUE);
		}
	}		
	else if (strcasecmp(table->f_name, "ethers") == 0) {
		nsd_file_appenddir(domain, table, "ethers.byname",
		    sizeof("ethers.byname") - 1);
		nsd_attr_append(&table->f_attrs, "table", "ethers.byname",
				TRUE);
		table->f_map = nsd_map_get("ethers.byname");
		nsw_copy(domain, table, "ethers.byaddr",
		    sizeof("ethers.byaddr") - 1, file, lineno);
		if (!nsd_attr_fetch(table->f_attrs, "casefold")) {
			nsd_attr_append(&table->f_attrs, "casefold", "true",
					TRUE);
		}
	}
	else if (strcasecmp(table->f_name, "ethers.byname") == 0) {
		if (!nsd_attr_fetch(table->f_attrs, "casefold")) {
			nsd_attr_append(&table->f_attrs, "casefold", "true",
					TRUE);
		}
	}		
	else if (strcasecmp(table->f_name, "group") == 0) {
		nsd_file_appenddir(domain, table, "group.byname",
		    sizeof("group.byname") - 1);
		nsd_attr_append(&table->f_attrs, "table", "group.byname",
				TRUE);
		table->f_map = nsd_map_get("group.byname");
		nsw_copy(domain, table, "group.bygid",
		    sizeof("group.bygid") - 1, file, lineno);
		nsw_copy(domain, table, "group.bymember",
		    sizeof("group.bymember") - 1, file, lineno);
	}
	else if (strcasecmp(table->f_name, "hosts") == 0) {
		nsd_file_appenddir(domain, table, "hosts.byaddr",
		    sizeof("hosts.byaddr") - 1);
		nsd_attr_append(&table->f_attrs, "table", "hosts.byaddr",TRUE);
		table->f_map = nsd_map_get("hosts.byaddr");
		nt = nsw_copy(domain, table, "hosts.byname",
			      sizeof("hosts.byname") - 1, file, lineno);
		if (nt && !nsd_attr_fetch(nt->f_attrs, "casefold")) {
			nsd_attr_append(&nt->f_attrs, "casefold", "true",
					TRUE);
		}
	}
	else if (strcasecmp(table->f_name, "hosts.byname") == 0) {
		if (!nsd_attr_fetch(table->f_attrs, "casefold")) {
			nsd_attr_append(&table->f_attrs, "casefold", "true",
					TRUE);
		}
  	}		
	else if (strcasecmp(table->f_name, "netgroup") == 0) {
		nsd_attr_append(&table->f_attrs, "table", "netgroup", TRUE);
		nsw_copy(domain, table, "netgroup.byuser",
		    sizeof("netgroup.byuser") - 1, file, lineno);
		nt = nsw_copy(domain, table, "netgroup.byhost",
			      sizeof("netgroup.byhost") - 1, file, lineno);
		if (nt && !nsd_attr_fetch(nt->f_attrs, "casefold")) {
			nsd_attr_append(&nt->f_attrs, "casefold", "true",
					TRUE);
		}
	}
	else if (strcasecmp(table->f_name, "netgroup.byhost") == 0) {
		if (!nsd_attr_fetch(table->f_attrs, "casefold")) {
			nsd_attr_append(&table->f_attrs, "casefold", "true",
					TRUE);
		}
	}		
	else if (strcasecmp(table->f_name, "networks") == 0) {
		nsd_file_appenddir(domain, table, "networks.byaddr",
		    sizeof("networks.byaddr") - 1);
		nsd_attr_append(&table->f_attrs, "table", "networks.byaddr",
				TRUE);
		table->f_map = nsd_map_get("networks.byaddr");
		nsw_copy(domain, table, "networks.byname",
		    sizeof("networks.byname") - 1, file, lineno);
	}
	else if (strcasecmp(table->f_name, "passwd") == 0) {
		nsd_file_appenddir(domain, table, "passwd.byname",
		    sizeof("passwd.byname") - 1);
		nsd_attr_append(&table->f_attrs, "table", "passwd.byname",
				TRUE);
		table->f_map = nsd_map_get("passwd.byname");
		nsw_copy(domain, table, "passwd.byuid",
		    sizeof("passwd.byuid") - 1, file, lineno);
	}
	else if (strcasecmp(table->f_name, "protocols") == 0) {
		nsd_file_appenddir(domain, table, "protocols.bynumber",
		    sizeof("protocols.bynumber") - 1);
		nsd_attr_append(&table->f_attrs, "table", 
				"protocols.bynumber",TRUE);
		table->f_map = nsd_map_get("protocols.bynumber");
		nsw_copy(domain, table, "protocols.byname",
		    sizeof("protocols.byname") - 1, file, lineno);
	}
	else if (strcasecmp(table->f_name, "rpc") == 0) {
		nsd_file_appenddir(domain, table, "rpc.bynumber",
		    sizeof("rpc.bynumber") - 1);
		nsd_attr_append(&table->f_attrs, "table", "rpc.bynumber", 
				TRUE);
		table->f_map = nsd_map_get("rpc.bynumber");
		nsw_copy(domain, table, "rpc.byname",
		    sizeof("rpc.byname") - 1, file, lineno);
	}
	else if (strcasecmp(table->f_name, "services") == 0) {
		nsd_attr_append(&table->f_attrs, "table", "services", TRUE);
		nsw_copy(domain, table, "services.byname",
		    sizeof("services.byname") - 1, file, lineno);
		table->f_map = nsd_map_get("services");
	}
	else if (strcasecmp(table->f_name, "shadow") == 0) {
		nsd_file_appenddir(domain, table, "shadow.byname",
		    sizeof("shadow.byname") - 1);
		nsd_attr_append(&table->f_attrs, "table", "shadow.byname",
				TRUE);
		table->f_map = nsd_map_get("shadow.byname");
	}
	else if (strcasecmp(table->f_name, "mac") == 0) {
		nsd_file_appenddir(domain, table, "mac.byname",
		    sizeof("mac.byname") - 1);
		nsd_attr_append(&table->f_attrs, "table", "mac.byname", TRUE);
		table->f_map = nsd_map_get("mac.byname");
		nsw_copy(domain, table, "mac.byvalue",
		    sizeof("mac.byvalue") - 1, file, lineno);
	}
	else if (strcasecmp(table->f_name, "capability") == 0) {
		nsd_file_appenddir(domain, table, "capability.byname",
		    sizeof("capability.byname") - 1);
		nsd_attr_append(&table->f_attrs, "table", "capability.byname",
				TRUE);
		table->f_map = nsd_map_get("capability.byname");
	}
	else if (strcasecmp(table->f_name, "clearance") == 0) {
		nsd_file_appenddir(domain, table, "clearance.byname",
		    sizeof("clearance.byname") - 1);
		nsd_attr_append(&table->f_attrs, "table", "clearance.byname",
				TRUE);
		table->f_map = nsd_map_get("clearance.byname");
	} 
	else if (strcasecmp(table->f_name, "bootparams") == 0) {
		if (!nsd_attr_fetch(table->f_attrs, "casefold")) {
			nsd_attr_append(&table->f_attrs, "casefold", "true",
					TRUE);
		}
	}
	else {
		table->f_map = nsd_map_get(table->f_name);
	}

	return NSD_OK;
}

/*
** This routine splits the given file into lines and passes them to the
** nsw_line routine.
*/
int
nsd_nsw_parse(nsd_file_t *domain, char *path)
{
	struct stat sbuf;
	char *file, *line;
	int fd, len, lineno;
	dstring_t ds;

	fd = nsd_open(path, O_RDONLY, 0);
	if (fd < 0) {
		nsd_logprintf(NSD_LOG_RESOURCE,
		    "nsd_nsw_parse: failed to open file %s\n", path);
		return NSD_ERROR;
	}

	fstat(fd, &sbuf);
	domain->f_uid = sbuf.st_uid;
	domain->f_gid = sbuf.st_gid;

	file = nsd_mmap(0, sbuf.st_size + 8, PROT_READ,
	    MAP_PRIVATE | MAP_AUTOGROW, fd, 0);
	close(fd);
	if ((void *)file == MAP_FAILED) {
		nsd_logprintf(NSD_LOG_RESOURCE,
		    "nsd_nsw_parse: failed to map file %s\n", path);
		return NSD_ERROR;
	}

	/*
	** Loop through lines of file.  Newlines may be quoted.
	*/
	sinit(&ds, 80);
	for (line = file, lineno = 1; line; line = strchr(line + 1, '\n')) {
		while (*line == '\n') {
			line++, lineno++;
		}
		for (; *line && (*line == ' ' || *line == '\t'); line++);
		len = strcspn(line, "\n#");
		if (! len) {
			continue;
		}

		if (line[len - 1] == '\\') {
			sappend(&ds, line, len - 1);
			continue;
		}

		sappend(&ds, line, len);
		nsd_logprintf(NSD_LOG_MAX, "nsw_parse: parsing line: %s\n",
		    ds.data);
		nsw_line(domain, ds.data, path, lineno);
		sreset(&ds);
	}

	munmap(file, sbuf.st_size + 8);
	sclear(&ds);

	return NSD_OK;
}

/*
** This routine is called to setup the default tables for the local domain.
*/
int
nsd_nsw_default(nsd_file_t *domain)
{
	int i;
	dstring_t ds;
	char *defaults[] = {
		"automount(dynamic): files nis(nis_enumerate_key)",
		"capability: files nis",
		"clearance: files nis",
		"ethers: files nis",
		"group: files nis",
		"hosts: nis dns files",
		"mac: files nis",
		"mail(null_extend_key): ndbm(file=/etc/aliases) nis",
		"netgroup: nis",
		"networks: files nis",
		"passwd: files(compat) [notfound=return] nis",
		"protocols: nis [success=return] files",
		"rpc: files nis",
		"services: files nis",
		"shadow(mode=0700): files",
		0
	};

	sinit(&ds, 60);
	for (i = 0; defaults[i]; i++) {
		sappend(&ds, defaults[i], strlen(defaults[i]));
		if (nsw_line(domain, ds.data, 0, 0) == NSD_OK) {
			nsd_logprintf(NSD_LOG_MIN,
			    "using default nsswitch.conf entry: %s\n",
			    defaults[i]);
		}
		sreset(&ds);
	}
	sclear(&ds);

	return NSD_OK;
}
