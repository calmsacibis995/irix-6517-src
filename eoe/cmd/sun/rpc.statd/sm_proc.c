#ifndef lint
static char sccsid[] = 	"@(#)sm_proc.c	1.1 88/04/20 4.0NFSSRC Copyr 1988 Sun Micro";
#endif

/* 
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 */

#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <rpc/rpc.h>
#include <rpcsvc/sm_inter.h>
#include <limits.h>
#include <dirent.h>
#include "sm_statd.h"
#include <sys/types.h>
#include <sys/syssgi.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>

#define curdir SM_CUR

extern int debug;
extern int Verbose;
int local_state;		/* fake local sm state */
int remote_state = 3; 		/* fake remote sm state for testing */
extern char STATE[], CURRENT[], BACKUP[];

sm_stat_res *
sm_stat_1(namep)
	sm_name *namep;
{
	static sm_stat_res resp; 

	if (debug)
		printf("sm_stat: mon_name = %s\n", (char *)namep);

	/* fake answer */
	resp.res_stat = stat_fail;
	resp.state = -1;
	return(&resp);
}

sm_stat_res *
sm_mon_1(monp)
	mon *monp;
{
	if (debug)
		printf("sm_mon: mon_name = %s, id = %d\n", 
			monp->mon_id.mon_name, *((int * )monp->priv)); 
	if (Verbose) {
		syslog(LOG_INFO, "SM_MON call for %s rejected\n",
			monp->mon_id.mon_name);
	}
	return(NULL);
}

sm_stat *
sm_unmon_1(monidp)
	mon_id *monidp;
{
	static sm_stat resp;

	if (debug)
		printf("sm_unmon: mon_name = %s, [%s, %d, %d, %d]\n",
			monidp->mon_name, monidp->my_id.my_name,
			monidp->my_id.my_prog, monidp->my_id.my_vers,
			monidp->my_id.my_proc);
	resp.state = local_state;
	return(&resp);
}

sm_stat *
sm_unmon_all_1(myidp)
	my_id *myidp;
{
	static sm_stat resp;

	if (debug)
		printf("sm_unmon_all: [%s, %d, %d, %d]\n",
			myidp->my_name,
			myidp->my_prog, myidp->my_vers,
			myidp->my_proc);

	resp.state = local_state;
	return(&resp);
}

void 
sm_simu_crash_1()
{
	if (debug)
		printf("sm_simu_crash\n");
	statd_init(CURRENT, BACKUP, STATE);
	return;
}


#include <sys/stat.h>
#include <sys/param.h>

struct lockd_args {
        char    la_name[MAXHOSTNAMELEN + 1];
        int     la_grace;
        u_int   la_lockshares:1;
        u_int   la_setgrace:1;
        u_int   ha_action:2;
        char    ha_dir[MAXPATHLEN + 1];
};


/*
 * Given a directory, call statd_init() after adding the directory
 * to the list of HA statmon directories.  There are only 2 available
 * slots for HA statmon directories, one for each node in the cluster.  Both
 * slots are filled only if the local machine is in degraded state.
 */
sm_stat_res *
sm_add_ha_dir_1(sm_name *monp)
{
	static sm_stat_res resp;
	char *interface, *path_end, *ha_dir, *current, *backup, *state;
	char string[MAXNAMLEN];
	char resolved_path[MAXPATHLEN];
	char tmpstring[MAXNAMLEN];
	struct lockd_args lockargs;

	strcpy(string, monp->mon_name);
	if (debug)
		printf("sm_add_ha_dir_1: %s\n", string);
	ha_dir = strrchr(string, ':');
	if (ha_dir == NULL)
		goto fail;
	*ha_dir = '\0';
	ha_dir++;
	interface = string;
	if (debug)
		printf("sm_add_ha_dir_1: (host: %s) (dir: %s)\n",
		       interface, ha_dir);
	/*	
	 *  check if the ha_dir string is valid :  see if the
	 *  path ends in "statmon"
	 *  remove the trailing / if there is one
 	 */
	if (ha_dir[strlen(ha_dir) - 1] == '/') {
		ha_dir[strlen(ha_dir) - 1] = '\0';
	}
	path_end = strrchr(ha_dir, '/');
	if(!path_end)
		goto fail;
	path_end++;
	if (strcmp(path_end, "statmon") != 0) {
		syslog(LOG_WARNING, "Invalid HA directory %s", ha_dir);
		goto fail;
	}
	if (!realpath(ha_dir, resolved_path)) {
		if (errno == ENOENT) {
			path_end--;
			*path_end = '\0';
			if (!realpath(ha_dir, resolved_path)) {
				*path_end = '/';
				syslog(LOG_WARNING, "Invalid realpath for HA directory %s",
					ha_dir);
				goto fail;
			} else if (strcmp(ha_dir, resolved_path) != 0) {
				*path_end = '/';
				syslog(LOG_WARNING, "Invalid realpath for HA directory",
					ha_dir);
				goto fail;
			}
			*path_end = '/';
		} else {
			syslog(LOG_WARNING, "Invalid HA directory %s",
				ha_dir);
			goto fail;
		}
	} else if (strcmp(ha_dir, resolved_path) != 0) {
		syslog(LOG_WARNING, "Invalid HA directory %s",
			ha_dir);
		goto fail;
	}

	sprintf(tmpstring, "%s/%s", ha_dir, curdir);
	if (!strcmp(tmpstring, HA_CURRENT) || !strcmp(tmpstring, HA2_CURRENT) ){
		syslog(LOG_ERR, "ha_dir %s already added earlier", tmpstring);
                resp.state = E2BIG;
                resp.res_stat = stat_fail;
                return(&resp);	
	}
	
	if (HA_CURRENT[0] == '\0') {
		sprintf(HA_CURRENT, "%s/%s", ha_dir, curdir);
		sprintf(HA_BACKUP, "%s/%s", ha_dir, backdir);
		sprintf(HA_STATE, "%s/%s", ha_dir, statefile);
		current = HA_CURRENT;
		backup = HA_BACKUP;
		state = HA_STATE;
		ha_mode = 1;
	} else if (HA2_CURRENT[0] == '\0') {
		sprintf(HA2_CURRENT, "%s/%s", ha_dir, curdir);
		sprintf(HA2_BACKUP, "%s/%s", ha_dir, backdir);
		sprintf(HA2_STATE, "%s/%s", ha_dir, statefile);
		current = HA2_CURRENT;
		backup = HA2_BACKUP;
		state = HA2_STATE;
		ha_mode = 1;
	} else {
		syslog(LOG_ERR, "All HA directories already added");
		resp.state = E2BIG;
		resp.res_stat = stat_fail;
		return(&resp);
	}


	if (mkdir(ha_dir, 0755) < 0 && errno != EEXIST) {
		struct stat hastat;
		/* if the dir already exists, ignore ENOSPC error */
		if(errno == ENOSPC && !stat(ha_dir, &hastat))
			syslog(LOG_ERR, "file system almost full");
		else {
			syslog(LOG_ERR, "rpc.statd exiting: can't create %s directory: %m", ha_dir);
			exit(1);
		}
	}
	
	/* pass the ha_dir information to the kernel */
	lockargs.ha_action = 1;
	strcpy(lockargs.ha_dir, current);
	if (syssgi(SGI_LOCKDSYS, &lockargs, sizeof(struct lockd_args))== -1) {
		syslog(LOG_ERR, "rpc.statd exiting: can't add ha_dir %s to the kernel", current);
		exit(1);
	}
	statd_init(current, backup, state);

	resp.res_stat = stat_succ;
	return (&resp);
fail:
	resp.res_stat = stat_fail;
	return (&resp);
}	/* sm_add_ha_dir_1 */


/*
 * Given a directory, delete that directory from one of the two possible
 * slots.
 */
sm_stat_res *
sm_delete_ha_dir_1(sm_name *monp)
{
	static sm_stat_res resp;
	char *ha_dir;
	char ha_current[PATH_MAX];
	struct lockd_args lockargs;

	ha_dir = monp->mon_name;

	/*
	 * first deal with the "clear ALL" case
 	 */
	if (!strcmp(ha_dir, "ALL")) {
		if (debug)
			printf("Clearing out all ha_dirs\n");
		HA_CURRENT[0] = '\0';
		HA2_CURRENT[0] = '\0';	
		ha_mode = 0;
	
                lockargs.ha_action = 2;
                lockargs.ha_dir[0] = '\0';
                if (syssgi(SGI_LOCKDSYS, &lockargs, sizeof(struct lockd_args))== -1) {
                        syslog(LOG_ERR,"rpc.statd exiting: can't clear all Ha_dirs from kernel");
                        exit(1);
                }
		resp.res_stat = stat_succ;
		return  (&resp);	
	}
	if (debug)
		printf("sm_delete_ha_dir_1: %s\n", ha_dir);

	sprintf(ha_current, "%s/%s", ha_dir, curdir);
	if (!strcmp(ha_current, HA_CURRENT)) {
		HA_CURRENT[0] = '\0';
		resp.res_stat = stat_succ;
		if (HA2_CURRENT[0] == '\0')
			ha_mode = 0;
	} else if (!strcmp(ha_current, HA2_CURRENT)) {
		HA2_CURRENT[0] = '\0';
		resp.res_stat = stat_succ;
		if (HA_CURRENT[0] == '\0')
			ha_mode = 0;
	} else {
		resp.res_stat = stat_fail;
		syslog(LOG_INFO, 
		       "cant delete non-existent ha_dir: %s\n", ha_current);
		return  (&resp);
	}

	/* pass the ha_dir information to the kernel */
	lockargs.ha_action = 2;
	strcpy(lockargs.ha_dir, ha_current);
	if (syssgi(SGI_LOCKDSYS, &lockargs, sizeof(struct lockd_args))== -1) {
		syslog(LOG_ERR, "can't delete ha_dir %s from kernel", ha_current);
		resp.res_stat = stat_fail;	
	}

	return (&resp);
}	/* sm_delete_ha_dir_1 */


/*
 * Print out the current HA directories.
 */
void
sm_query_ha_dirs_1(void)
{
	int ha_dirs_printed = 0;
	struct lockd_args lockargs;

	if (HA_CURRENT[0] != '\0') {
		syslog(LOG_ERR, "HA_CURRENT: %s\n", HA_CURRENT);
		ha_dirs_printed++;
	}
	if (HA2_CURRENT[0] != '\0') {
		syslog(LOG_ERR, "HA2_CURRENT: %s\n", HA2_CURRENT);
		ha_dirs_printed++;
	}
	if (!ha_dirs_printed)
		syslog(LOG_ERR, "No HA directories added");


	/* query the ha_dir information from the kernel */
	lockargs.ha_action = 3;
	if (syssgi(SGI_LOCKDSYS, &lockargs, sizeof(struct lockd_args))== -1) {
		syslog(LOG_ERR, "rpc.statd exiting: can't query the kernel for HA dirs");
		exit(1);
	}
	syslog(LOG_INFO, "kern_HA_dir1: %s, kern_HA_dir2 : %s",
	       lockargs.ha_dir, lockargs.la_name);
}	/* sm_query_ha_dirs_1 */


void
sm_notify(stat_chge *ntfp, SVCXPRT *transp)
{
	struct hostent *hp;
	extern int h_errno;

	if (debug)
		printf("sm_notify: %s state =%d\n", ntfp->name, ntfp->state);
	if (Verbose) {
		syslog(LOG_INFO, "notification received from %s, state %d", ntfp->name,
			ntfp->state);
	}

	/*
	 * Get the host address and tell the kernel to reclaim any locks it
	 * knows about.
	 */
	hp = gethostbyname(ntfp->name);
	if (!hp) {
		syslog(LOG_ERR, "sm_notify: unable to get host address for %s: %s\n",
			ntfp->name, hstrerror(h_errno));
	} else if ((syssgi(SGI_NFSNOTIFY, hp->h_addr, hp->h_length) == -1)) {
		syslog(LOG_ERR, "sm_notify: kernel reclaim for server %s failed: %s\n",
			ntfp->name, strerror(errno));
	}
}
