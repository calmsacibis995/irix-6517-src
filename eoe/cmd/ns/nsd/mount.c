/*
** This file contains information about specific mount points that this
** server maintains.  Each nsswitch.conf file corresponds to a mount point.
** The top 32 bits of a filehandle correspond to the file system ID.
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/dir.h>
#include <errno.h>
#include <ns_api.h>
#include <ns_daemon.h>
#include "nfs.h"

nsd_file_t *__nsd_mounts = 0;
extern nsd_attr_t *__nsd_attrs;
extern long nsd_timeout;

void
nsd_mount_clear(void)
{
	if (__nsd_mounts) {
		nsd_timeout_remove(__nsd_mounts);
		nsd_file_clear(&__nsd_mounts);
	}
}

int
nsd_mount_timeout(nsd_file_t **rqp, nsd_times_t *to)
{
	nsd_file_t *top;

	*rqp = top = to->t_file;

	/*
	** Remove this timeout.
	*/
	nsd_timeout_remove(top);

	/*
	** Call the shake routine with a very low priority.  This has the
	** effect of running timeout of the filesystem tree with the current
	** time, and calling all the library timeout routines to throw away
	** low priority caches.
	*/
	nsd_shake(0);

	/*
	** Reinstall the timeout for the next interval.
	*/
	if (top) {
		nsd_timeout_new(top, nsd_timeout, nsd_mount_timeout,
		    (void *)0);
	}

	return NSD_CONTINUE;
}

/*
** The init routine will read each of the nsswitch.conf files creating
** a new tree.  When successful we attach the tree to our directory
** tree, removing the old structures.  All filesystems are stored in
** a root directory.
*/
int
nsd_mount_init(void)
{
	char buf[MAXPATHLEN];
	nsd_file_t *top, *domain;
	DIR *dp;
	struct direct *de;

	/*
	** Build a directory to hold all the mount points.
	*/
	if (nsd_file_init(&top, ".top", 4, (nsd_file_t *)0, NFDIR, 0) !=
	    NSD_OK) {
		return NSD_ERROR;
	}
	nsd_attr_continue(&top->f_attrs, __nsd_attrs);
	top->f_mode = 0755;

	/*
	** Always setup default domain ".local".
	*/
	if (nsd_file_init(&domain, ".local", 6, top, NFDIR, 0) != NSD_OK) {
		free(top);
		return NSD_ERROR;
	}
	domain->f_mode = 0755;
	nsd_attr_store(&domain->f_attrs, "local", 0);
	nsd_logprintf(NSD_LOG_MIN, "Adding domain .local: %lld\n", 
		      FILEID(domain->f_fh));
	nsd_cat(buf, MAXPATHLEN, 2, "/etc/", NS_SWITCH_FILE);
	nsd_nsw_parse(domain, buf);
	nsd_nsw_default(domain);	/* fill in defaults for missing maps */
	nsd_file_appenddir(top, domain, domain->f_name, domain->f_namelen);

	/*
	** Now we can release the old structure tree, and replace it with
	** the one we are building.
	*/
	nsd_mount_clear();
	top->f_nlink++;
	__nsd_mounts = top;
	nsd_timeout_new(top, nsd_timeout, nsd_mount_timeout, (void *)0);
	
	/*
	** Loop through all the domains for which there is a nsswitch.conf
	** file.
	*/
	dp = opendir(NS_DOMAINS_DIR);
	if (! dp) {
		nsd_logprintf(NSD_LOG_OPER, 
		    "nsd_mount_init: failed to open domain directory %s: %s\n",
		    NS_DOMAINS_DIR, strerror(errno));
		return NSD_OK;
	}
	while (de = readdir(dp)) {
		if (de->d_name[0] == '.') {
			continue;
		}
		nsd_cat(buf, MAXPATHLEN, 4, NS_DOMAINS_DIR, de->d_name, "/",
		    NS_SWITCH_FILE);
		if (access(buf, R_OK) == 0) {
			if (nsd_file_init(&domain, de->d_name,
			    strlen(de->d_name), top, NFDIR, 0) != NSD_OK) {
				nsd_logprintf(NSD_LOG_RESOURCE,
				    "malloc failed\n");
				return NSD_ERROR;
			}
			domain->f_mode = 0755;

			nsd_attr_store(&domain->f_attrs, "domain", de->d_name);

			if (nsd_nsw_parse(domain, buf) != NSD_OK) {
				nsd_file_clear(&domain);
				continue;
			}

			if (nsd_file_appenddir(top, domain, domain->f_name,
			    domain->f_namelen) != NSD_OK) {
				nsd_logprintf(NSD_LOG_RESOURCE,
				    "domain %s already exists\n",
				    domain->f_name);
				nsd_file_clear(&domain);
			} else {
				nsd_logprintf(NSD_LOG_MIN,
				    "Adding domain %s: %lld\n",
				    domain->f_name, FILEID(domain->f_fh));
			}
		}
	}
	closedir(dp);

	return NSD_OK;
}
