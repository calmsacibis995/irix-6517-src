#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <ns_api.h>
#include <ns_daemon.h>
#include "ns_files.h"

/*
** The files method is made up of a number of map specific parser
** routines.  This file contains the wrapper routines which determine
** which specific file routines to forward to.
*/
extern int file_aliases_lookup(nsd_file_t *);
extern int file_aliases_list(nsd_file_t *);
extern int file_bootparams_lookup(nsd_file_t *);
extern int file_bootparams_list(nsd_file_t *);
extern int file_ethers_lookup(nsd_file_t *);
extern int file_ethers_list(nsd_file_t *);
extern int file_group_lookup(nsd_file_t *);
extern int file_group_list(nsd_file_t *);
extern int file_hosts_lookup(nsd_file_t *);
extern int file_hosts_list(nsd_file_t *);
extern int file_networks_lookup(nsd_file_t *);
extern int file_networks_list(nsd_file_t *);
extern int file_passwd_lookup(nsd_file_t *);
extern int file_passwd_list(nsd_file_t *);
extern int file_protocols_lookup(nsd_file_t *);
extern int file_protocols_list(nsd_file_t *);
extern int file_rpc_lookup(nsd_file_t *);
extern int file_rpc_list(nsd_file_t *);
extern int file_shadow_lookup(nsd_file_t *);
extern int file_shadow_list(nsd_file_t *);
extern int file_services_lookup(nsd_file_t *);
extern int file_services_list(nsd_file_t *);
extern int file_mac_lookup(nsd_file_t *);
extern int file_mac_list(nsd_file_t *);
extern int file_capability_lookup(nsd_file_t *);
extern int file_capability_list(nsd_file_t *);
extern int file_clearance_lookup(nsd_file_t *);
extern int file_clearance_list(nsd_file_t *);
extern int file_netgroup_lookup(nsd_file_t *);
extern int file_netgroup_list(nsd_file_t *);
extern int file_generic_lookup(nsd_file_t *);
extern int file_generic_list(nsd_file_t *);

extern int _ltoa(long, char *);

char _files_result[MAXRESULT];
int _file_sock;
static file_domain_t *domains;

typedef int (file_proc)(nsd_file_t *);
typedef int (file_call_proc)(nsd_file_t *, char *);

typedef struct {
	file_proc		*lookup;
	file_proc		*list;
	char			*file;
	int			maps;
	char			*names[3];
} file_funcs_t;

static file_funcs_t file_funcs[] = {
	{file_aliases_lookup, file_aliases_list, "aliases", 2,
	    {ALIASES_BYNAME, ALIASES_BYADDR, 0}},
	{file_bootparams_lookup, file_bootparams_list, "bootparams", 1,
	    {BOOTPARAMS_BYNAME, 0, 0}},
	{file_ethers_lookup, file_ethers_list, "ethers", 2,
	    {ETHERS_BYNAME, ETHERS_BYADDR, 0}},
	{file_group_lookup, file_group_list, "group", 3,
	    {GROUP_BYNAME, GROUP_BYMEMBER, GROUP_BYGID}},
	{file_hosts_lookup, file_hosts_list, "hosts", 2,
	    {HOSTS_BYNAME, HOSTS_BYADDR, 0}},
	{file_networks_lookup, file_networks_list, "networks", 2,
	    {NETWORKS_BYNAME, NETWORKS_BYADDR, 0}},
	{file_passwd_lookup, file_passwd_list, "passwd", 2,
	    {PASSWD_BYNAME, PASSWD_BYUID, 0}},
	{file_protocols_lookup, file_protocols_list, "protocols", 2,
	    {PROTOCOLS_BYNAME, PROTOCOLS_BYNUMBER, 0}},
	{file_rpc_lookup, file_rpc_list, "rpc", 2,
	    {RPC_BYNAME, RPC_BYNUMBER, 0}},
	{file_shadow_lookup, file_shadow_list, "shadow", 1,
	    {SHADOW_BYNAME, 0, 0}},
	{file_services_lookup, file_services_list, "services", 2,
	    {SERVICES_BYNAME, SERVICES_BYPORT, 0}},
	{file_mac_lookup, file_mac_list, "mac", 2,
	    {MAC_BYNAME, MAC_BYVALUE, 0}},
	{file_capability_lookup, file_capability_list, "capability", 1,
	    {CAPABILITY_BYNAME, 0, 0}},
	{file_clearance_lookup, file_clearance_list, "clearance", 1,
	    {CLEARANCE_BYNAME, 0, 0}},
	{file_netgroup_lookup, file_netgroup_list, "netgroup", 3,
	    {NETGROUP_BYNAME, NETGROUP_BYHOST, NETGROUP_BYUSER}},
	{file_generic_lookup, file_generic_list, 0, 0, {0, 0, 0}}
};

/*
** This routine will just return the index into the function pointer
** array for a given map.
*/
static unsigned
file_index(char *name)
{
	int i, j;

	for (i = 0; file_funcs[i].maps; i++) {
		for (j = 0; j < file_funcs[i].maps; j++) {
			if (strcasecmp(file_funcs[i].names[j], name) == 0) {
				return i;
			}
		}
	}

	return GENERIC;
}

static void
file_close(file_domain_t *dom)
{
	if (dom->map) {
		munmap(dom->map, dom->size);
		dom->map = (char *)0;
		dom->size = 0;
	}
}

/*
** This routine will call timeout of the subdirectories of the tree for
** which this file matches.
*/
/*ARGSUSED*/
int
verify(nsd_file_t *rq, nsd_cred_t *cp)
{
	file_domain_t *dom;

	nsd_logprintf(NSD_LOG_MIN, "entering verify (files):\n");

	dom = file_domain_lookup(rq);

	if (! dom) {
		return NSD_ERROR;
	}

	if ((rq->f_ctime.tv_sec < dom->version.tv_sec) ||
	    ((rq->f_ctime.tv_sec == dom->version.tv_sec) &&
	     (rq->f_ctime.tv_usec < dom->version.tv_usec))) {
		/* 
		** If the date is in the future, don't return ERROR.  
		** All lookups will fail until the file has a date in the
		** past (or present).
		*/
		if (!(dom->flags & FUTUREDATE)) {
			return NSD_ERROR;
		}
	}

	return NSD_OK;
}

/*
** This routine will open/reopen a file and mmap it in to our address
** space.
*/
int
file_open(file_domain_t *dom)
{
	struct stat sbuf;
	int fd;
	time_t now;
	struct timeval tv;
	
	if (stat(dom->path, &sbuf) < 0) {
		nsd_logprintf(NSD_LOG_HIGH,
		    "file_open: failed to stat %s: %s\n", dom->path,
		    strerror(errno));
		return 0;
	}

	now = time(NULL);
	timespec_to_timeval(&tv, &sbuf.st_mtim);

	if ((dom->version.tv_sec != tv.tv_sec) ||
	    (dom->version.tv_usec != tv.tv_usec)) {
		file_close(dom);
	}

	if (! dom->map) {
		if ((fd = nsd_open(dom->path, O_RDONLY, 0400)) < 0) {
			nsd_logprintf(NSD_LOG_OPER,
			    "failed to open file %s: %s\n",
			    dom->path, strerror(errno));
			return 0;
		}

		if (fstat(fd, &sbuf) < 0) {
			close(fd);
			nsd_logprintf(NSD_LOG_HIGH,
			    "file_open: failed to fstat %s: %s\n", dom->path,
			    strerror(errno));
			return 0;
		}

		/*
		** Many of the string functions will read past the end of
		** the buffer and look for a null.  Thus we have to make
		** sure that there is a NULL at the end of the mmaped
		** space.  Thus, we map an extra couple of words and write
		** a null at where we think the end of the file is.  Since
		** This is MAP_PRIVATE it will mean that we will have a
		** private copy of the last page of the file.
		*/
		dom->size = sbuf.st_size + (2 * sizeof(long));

#ifdef MAPWAR
		{
			int x;

			x = nsd_open("/dev/zero", O_RDWR, 0400);
			if (x < 0) {
				close(fd);
				dom->map = 0;
				dom->size = 0;
				nsd_logprintf(NSD_LOG_HIGH,
				    "file_open: failed to open /dev/zero: %s\n",
				    strerror(errno));
				return 0;
			}

			dom->map = nsd_mmap(0, dom->size, PROT_READ|PROT_WRITE,
			    MAP_PRIVATE, x, 0);
			close(x);
			if ((void *)dom->map == MAP_FAILED) {
				close(fd);
				dom->map = 0;
				dom->size = 0;
				nsd_logprintf(NSD_LOG_HIGH,
				    "file_open: failed to map /dev/zero: %s\n",
				    strerror(errno));
				return 0;
			}

			x = read(fd, dom->map, dom->size - 1);
			if (x < 0) {
				close(fd);
				munmap(dom->map, dom->size);
				dom->map = 0;
				dom->size = 0;
				nsd_logprintf(NSD_LOG_HIGH,
				    "file_open: failed to read %s: %s\n",
				    dom->path, strerror(errno));
				return 0;
			}
			dom->map[x] = 0;
			close(fd);
		}
#else
		dom->map = nsd_mmap(0, dom->size, PROT_READ|PROT_WRITE,
		    MAP_PRIVATE|MAP_AUTOGROW, fd, 0);
		close(fd);
		if ((void *)dom->map == MAP_FAILED) {
			dom->map = 0;
			dom->size = 0;
			nsd_logprintf(NSD_LOG_HIGH,
			    "file_open: failed to mmap %s: %s\n",
			    dom->path, strerror(errno));
			return 0;
		}
		dom->map[sbuf.st_size] = 0;
#endif
			
		timespec_to_timeval(&dom->version, &sbuf.st_mtim);

		/* 
		** Is the date in the future? If so, set FUTUREDATE and
		** give warning only once.  Also try to detect if the 
		** src file date has been fixed.
		*/
		if (dom->version.tv_sec > now) {
			if (!dom->flags & FUTUREDATE) {
				nsd_logprintf(NSD_LOG_CRIT, 
				     "WARNING: Future date on %s: %s",
				     dom->path, ctime(&dom->version.tv_sec));
			}
			dom->flags |= FUTUREDATE;
		} else {
			dom->flags &= ~FUTUREDATE;
		}

	}

	dom->touched = now;
	return 1;
}

/*
** This routine searches for a domain structure, and creates a new one
** if not found.
*/
file_domain_t *
file_domain_lookup(nsd_file_t *rq)
{
	file_domain_t *dom;
	int index, len;
	char *file, *domain, *map, path[MAXPATHLEN];

	map = nsd_attr_fetch_string(rq->f_attrs, "table", (char *)0);
	if (! map) {
		return (file_domain_t *)0;
	}

	domain = nsd_attr_fetch_string(rq->f_attrs, "domain", (char *)0);

	index = file_index(map);

	file = nsd_attr_fetch_string(rq->f_attrs, "file", 0);
	if (! file) {
		if (index < GENERIC) {
			file = file_funcs[index].file;
		} else {
			file = map;
		}
	}
	if (*file != '/') {
		if (domain && *domain) {
			len = nsd_cat(path, sizeof(path), 4, NS_DOMAINS_DIR,
			    domain, "/", file);
		} else {
			len = nsd_cat(path, sizeof(path), 2, "/etc/", file);
		}
	} else {
		len = nsd_cat(path, sizeof(path), 1, file);
	}

	for (dom = domains; dom; dom = dom->next) {
		if (strcasecmp(dom->path, path) == 0) {
			if (file_open(dom)) {
				return dom;
			} else {
				return (file_domain_t *)0;
			}
		}
	}

	dom = (file_domain_t *)nsd_calloc(1, sizeof(file_domain_t) + len);
	if (! dom) {
		nsd_logprintf(NSD_LOG_OPER,
		    "file_domain_lookup: malloc failed\n");
		return dom;
	}

	memcpy(dom->path, path, len);
	dom->path[len] = 0;

	if (file_open(dom)) {
		dom->next = domains;
		domains = dom;
		return dom;
	} else {
		free(dom);
		return (file_domain_t *)0;
	}
}

/*
** This routine will loop through all of the domain structures closing
** files and freeing space.  This is called on init, or whenever we fail
** to open a file.
*/
void
file_domain_clear(void)
{
	file_domain_t *dom;

	for (dom = domains; dom; dom = domains) {
		domains = dom->next;
		if (dom->map) {
			file_close(dom);
		}
		free(dom);
	}
}

/*
** This routine is called to unmap files which we haven't touched recently.
*/
int
shake(int priority, time_t now)
{
	file_domain_t *dom;

	if (priority == 10) {
		file_domain_clear();
		return NSD_OK;
	}

	now -= (10 - priority) * 30;
	for (dom = domains; dom; dom = dom->next) {
		if (dom->touched < now) {
			file_close(dom);
		}
	}

	return NSD_OK;
}

/*
** The init routine is called when the library is first mapped in,
** or when the init routine is explicitely called.  The files method
** uses this to setup internal state, and to reopen any files that
** we currently have mapped.  If map is null then all file init
** routines are called.
*/
/* ARGSUSED */
int
init(char *map)
{

	if (domains) {
		file_domain_clear();
	}
	
	return NSD_OK;
}

/*
** The lookup procedure is called to fetch an element out of the given
** file.
*/
int
lookup(nsd_file_t *rq)
{
	unsigned index;
	char *map;

	nsd_logprintf(NSD_LOG_MIN, "entering lookup (files)\n");

	if (! rq) {
		nsd_logprintf(NSD_LOG_LOW, "no file\n");
		return NSD_ERROR;
	}

	map = nsd_attr_fetch_string(rq->f_attrs, "table", (char *)0);
	if (! map) {
		nsd_logprintf(NSD_LOG_LOW, "no map set\n");
		return NSD_ERROR;
	}

	index = file_index(map);

	return (*file_funcs[index].lookup)(rq);
}

/*
** The list procedure will expand the entire file into the data buffer.
*/
int
list(nsd_file_t *rq)
{
	unsigned index;
	char *map;

	nsd_logprintf(NSD_LOG_MIN, "entering list (files)\n");

	if (! rq) {
		return NSD_ERROR;
	}

	map = nsd_attr_fetch_string(rq->f_attrs, "table", (char *)0);
	if (! map) {
		return NSD_ERROR;
	}

	index = file_index(map);
	if (index == ERROR_INDEX) {
		return NSD_NEXT;
	}

	return (*file_funcs[index].list)(rq);
}

/*
** The dump function just prints the current state of the library to the
** given file pointer.  For this library we just walk through the open
** file cache.
*/
int
dump(FILE *fp)
{
	file_domain_t *dom;

	fprintf(fp, "Current file list:\n");
	for (dom = domains; dom; dom = dom->next) {
		fprintf(fp, "path: %s\n", dom->path);
		fprintf(fp, "\tversion: %s", ctime(&dom->version.tv_sec));
		fprintf(fp, "\tfuture date: %s\n", 
			dom->flags & FUTUREDATE ? "yes" : "no");
		fprintf(fp, "\tlast touched: %s", ctime(&dom->touched));
		fprintf(fp, "\tstatus: %s\n", (dom->map) ? "open" : "closed");
	}

	return NSD_OK;
}
