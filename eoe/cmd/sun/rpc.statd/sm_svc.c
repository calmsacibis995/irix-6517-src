#ifndef lint
static char sccsid[] = 	"@(#)sm_svc.c	1.1 88/04/20 4.0NFSSRC Copyr 1988 Sun Micro";
#endif

/* 
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 */

#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <syslog.h>
#include <limits.h>
#include <unistd.h>
#include <rpc/rpc.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <rpcsvc/sm_inter.h>
#include <sys/param.h>
#include <sys/syssgi.h>
#include "sm_statd.h"

#define statdir		SM_STATDIR
#define curdir		SM_CUR
#define backdir		"sm.bak"
#define statefile	"state"

/* for Failsafe HA */
#define SM_ADD_HA_DIR 10
#define SM_DELETE_HA_DIR 11
#define SM_QUERY_HA_DIRS 12


struct lockd_args {
        char    la_name[MAXHOSTNAMELEN + 1];
        int     la_grace;
        u_int   la_lockshares:1;
        u_int   la_setgrace:1;
        u_int   ha_action:2;
        char    ha_dir[MAXPATHLEN + 1];
};

char STATE[PATH_MAX], CURRENT[PATH_MAX], BACKUP[PATH_MAX];

/* From sm_statd.c
 */
extern char HA_STATE[PATH_MAX], HA_CURRENT[PATH_MAX], HA_BACKUP[PATH_MAX];
extern char HA2_STATE[PATH_MAX], HA2_CURRENT[PATH_MAX], HA2_BACKUP[PATH_MAX];

int Verbose = 0;
int debug = 0;
extern crash_notice(), recovery_notice();
extern void sm_try();

int ha = FALSE;         /* HighAvailability enabled or not */

static void
sm_prog_1(rqstp, transp)
	struct svc_req *rqstp;
	SVCXPRT *transp;
{
	union {
		struct sm_name sm_stat_1_arg;
		struct mon sm_mon_1_arg;
		struct mon_id sm_unmon_1_arg;
		struct my_id sm_unmon_all_1_arg;
		struct stat_chge ntf_arg;
	} argument;
	char *result;
	bool_t (*xdr_argument)(), (*xdr_result)();
	char *(*local)();
	struct hostent *hp = NULL;
	char *procname = NULL;
	extern struct sm_stat_res *sm_stat_1();
	extern struct sm_stat_res *sm_mon_1();
	extern struct sm_stat *sm_unmon_1();
	extern struct sm_stat *sm_unmon_all_1();
	extern void *sm_simu_crash_1();
	extern void *sm_notify();
	extern bool_t xdr_notify();
	extern void *sm_add_ha_dir_1();
	extern void *sm_delete_ha_dir_1();
	extern void sm_query_ha_dirs_1();

	switch (rqstp->rq_proc) {
	case NULLPROC:
		svc_sendreply(transp, xdr_void, NULL);
		return;

	case SM_STAT:
		xdr_argument = xdr_sm_name;
		xdr_result = xdr_sm_stat_res;
		local = (char *(*)()) sm_stat_1;
		break;

	case SM_MON:
		hp = gethostbyaddr((char *)&transp->xp_raddr.sin_addr,
			sizeof(struct in_addr), (int)transp->xp_raddr.sin_family);
		bzero(&argument, sizeof(argument));
		if (!svc_getargs(transp, xdr_mon, &argument)) {
			syslog(LOG_WARNING, "SM_MON decode error, caller %s\n",
				hp ? hp->h_name : inet_ntoa(transp->xp_raddr.sin_addr));
			svcerr_decode(transp);
			return;
		}
		if (Verbose) {
			syslog(LOG_INFO, "SM_MON call from %s for %s rejected\n",
				hp ? hp->h_name : inet_ntoa(transp->xp_raddr.sin_addr),
				argument.sm_mon_1_arg.mon_id.mon_name);
		}
		svcerr_noproc(transp);
		return;

	case SM_UNMON:
		hp = gethostbyaddr((char *)&transp->xp_raddr.sin_addr,
			sizeof(struct in_addr), (int)transp->xp_raddr.sin_family);
		bzero(&argument, sizeof(argument));
		if (!svc_getargs(transp, xdr_mon_id, &argument)) {
			syslog(LOG_WARNING, "SM_UNMON decode error, caller %s\n",
				hp ? hp->h_name : inet_ntoa(transp->xp_raddr.sin_addr));
			svcerr_decode(transp);
			return;
		}
		if (Verbose) {
			syslog(LOG_INFO, "SM_UNMON call from %s for %s rejected\n",
				hp ? hp->h_name : inet_ntoa(transp->xp_raddr.sin_addr),
				argument.sm_unmon_1_arg.mon_name);
		}
		svcerr_noproc(transp);
		return;

	case SM_UNMON_ALL:
		hp = gethostbyaddr((char *)&transp->xp_raddr.sin_addr,
			sizeof(struct in_addr), (int)transp->xp_raddr.sin_family);
		bzero(&argument, sizeof(argument));
		if (!svc_getargs(transp, xdr_my_id, &argument)) {
			syslog(LOG_WARNING, "SM_UNMON_ALL decode error, caller %s\n",
				hp ? hp->h_name : inet_ntoa(transp->xp_raddr.sin_addr));
			svcerr_decode(transp);
			return;
		}
		if (Verbose) {
			syslog(LOG_INFO, "SM_UNMON_ALL call from %s for %s rejected\n",
				hp ? hp->h_name : inet_ntoa(transp->xp_raddr.sin_addr),
				argument.sm_unmon_all_1_arg.my_name);
		}
		svcerr_noproc(transp);
		return;


	case SM_SIMU_CRASH:
		xdr_argument = xdr_void;
		xdr_result = xdr_void;
		local = (char *(*)()) sm_simu_crash_1;
		break;

	case SM_NOTIFY:
		xdr_argument = xdr_notify;
		xdr_result = xdr_void;
		local = (char *(*)()) sm_notify;
		break;

	case SM_ADD_HA_DIR:
		if(ha) {
			xdr_argument = xdr_sm_name;
			xdr_result = xdr_sm_stat_res;
			local = (char *(*)()) sm_add_ha_dir_1;
			break;
		} else {
			svcerr_noproc(transp);
			return;
		}

	case SM_DELETE_HA_DIR:
		if(ha) {
			xdr_argument = xdr_sm_name;
			xdr_result = xdr_sm_stat_res;
			local = (char *(*)()) sm_delete_ha_dir_1;
			break;
		} else {
			svcerr_noproc(transp);
			return;
		}

	case SM_QUERY_HA_DIRS:
		if(ha) {
			xdr_argument = xdr_void;
			xdr_result = xdr_void;
			local = (char *(*)()) sm_query_ha_dirs_1;
			break;
		} else {
			svcerr_noproc(transp);
			return;
		}

	default:
		svcerr_noproc(transp);
		return;
	}
	bzero(&argument, sizeof(argument));
	if (!svc_getargs(transp, xdr_argument, &argument)) {
		svcerr_decode(transp);
		return;
	}
	result = (*local)(&argument, transp);
	if (!svc_sendreply(transp, xdr_result, result)) {
		svcerr_systemerr(transp);
	}
	if (rqstp->rq_proc != SM_MON)
	if (!svc_freeargs(transp, xdr_argument, &argument)) {
		syslog(LOG_ERR, "unable to free arguments");
		exit(1);
	}
}

extern int __svc_label_agile;

main(argc, argv)
	int argc;
	char **argv;
{
	SVCXPRT *transp;
	int c;
	extern int optind;
	extern char *optarg;
	int choice = 0;
	char *sdir;

	__svc_label_agile = (sysconf(_SC_IP_SECOPTS) > 0);

	openlog("rpc.statd", LOG_PID, LOG_DAEMON);

	(void) signal(SIGALRM, sm_try);
	while ((c = getopt(argc, argv, "hvDd:")) != EOF)
		switch(c) {
		case 'h':
			ha = TRUE;
			break;
		case 'v':
			Verbose++;
			break;
		case 'd':
			(void) sscanf(optarg, "%d", &debug);
			break;
		case 'D':
			choice = 1;
			break;
		default:
			syslog(LOG_ERR, "-d [debug-level] -D");
			exit(1);
		}
	if (choice == 0) {
		sdir = statdir;
		if (mkdir(sdir, 0755) < 0 && errno != EEXIST) {
			syslog(LOG_ERR, "can't create %s directory: %m", sdir);
			exit(1);
		}
	} else {
		sdir = ".";
	}
	sprintf(CURRENT, "%s/%s", sdir, curdir);
	sprintf(BACKUP, "%s/%s", sdir, backdir);
	sprintf(STATE, "%s/%s", sdir, statefile);
	if (debug) {
		setlinebuf(stdout);
		printf("debug is on, create entry: %s, %s, %s\n", 
			CURRENT, BACKUP, STATE);
	} else {
		if (_daemonize(0, -1,-1,-1) < 0)
			exit(1);
		openlog("rpc.statd", LOG_PID, LOG_DAEMON);
	}
	pmap_unset(SM_PROG, SM_VERS);

	/* For FailSafe systems only
	 */
	if (ha) {
		struct lockd_args lockargs;
		DIR	*dp;
		char from[PATH_MAX], to[PATH_MAX];
		struct dirent *dirp;
		char    ha_dir[PATH_MAX], *ha_ptr;

	    	/*
	     	 * make sure we first copy all enteries for /var/statmon/sm.bck (BACKUP)
	     	 * to /var/statmon/sm (CURRENT).
	     	 */
	    	if (Verbose)
			syslog(LOG_INFO,"moving any and all enteries from %s to %s",
				BACKUP,CURRENT);
		/* get all entries in BACKUP into CURRENT into */
		if ((dp = opendir(BACKUP)) == NULL) {
			syslog(LOG_ERR, "opendir: %m");
			exit(1);
		}
		for (dirp = readdir(dp); dirp != NULL; dirp = readdir(dp)) {
			if (strcmp(dirp->d_name, ".") != 0  &&
				strcmp(dirp->d_name, "..") != 0) {
				/* rename all entries from current to backup */
				(void) strcpy(from , BACKUP);
				(void) strcpy(to, CURRENT);
				(void) strcat(from, "/");
				(void) strcat(to, "/");
				(void) strcat(from, dirp->d_name);
				(void) strcat(to, dirp->d_name);
				if (rename(from, to) == -1) {
					syslog(LOG_ERR, "rename: %m");
					exit(1);
				}
				if (Verbose)
					syslog(LOG_INFO,"rename: %s to %s", from ,to);
		   	}
	    	}
	    	/* post a blind query to see if we have any
	         * pre-loaded ha_dirs */
	    	lockargs.ha_action = 3;
	    	if (Verbose)
			syslog(LOG_INFO,"post a quering for ha-dirs from the kernel");
	    
	    	if (syssgi(SGI_LOCKDSYS, &lockargs, sizeof(struct lockd_args))== -1) {
			syslog(LOG_ERR,"This kernel is not HA capable"); 
	    	} else {
			if (lockargs.ha_dir[0] == '\0') {
		    		syslog(LOG_INFO,"first HA directory is NULL");
			} else {
		    		/* we need to load the sm directories */
		    		syslog(LOG_INFO, "kern_HA_dir1: %s, kern_HA_dir2 : %s",
			   		lockargs.ha_dir, lockargs.la_name);
				/* call the main init function to process our ha_dirs.
				 * The directories return by the syssgi call, already
				 * have the /sm added, so we need to strip off the /sm
				 * then add /sm, sm.bck and statefile back on.
				 */
				strcpy(ha_dir,lockargs.ha_dir);
				if ((ha_ptr=strrchr(ha_dir,'/')) == NULL) {
					syslog(LOG_ERR, "failed to parse ha_dir %s",
						lockargs.ha_dir);
					exit (1);
				}
				*ha_ptr='\0';
				sprintf(HA_CURRENT,"%s/%s",ha_dir, curdir);
				sprintf(HA_BACKUP,"%s/%s",ha_dir, backdir);
				sprintf(HA_STATE,"%s/%s",ha_dir, statefile);
		    		if (Verbose) {
					syslog(LOG_INFO,"current %s, backup %s, state %s",
			       			HA_CURRENT,HA_BACKUP,HA_STATE);
		    		}
		    		statd_init(HA_CURRENT,HA_BACKUP,HA_STATE);
			}
                        if (lockargs.la_name[0] == '\0') {
                                syslog(LOG_INFO,"second HA directory is NULL");
                        } else {
                                /* we need to load the sm directories */
                                syslog(LOG_INFO, "kern_HA_dir1: %s, kern_HA_dir2 : %s",
                                        lockargs.ha_dir, lockargs.la_name);
                                /* call the main init function to process our ha_dirs.
                                 * The directories return by the syssgi call, already
                                 * have the /sm added, so we need to strip off the /sm
                                 * then add /sm, sm.bck and statefile back on.
                                 */
                                strcpy(ha_dir,lockargs.la_name);
                                if ((ha_ptr=strrchr(ha_dir,'/')) == NULL) {
                                        syslog(LOG_ERR, "failed to parse la_name %s",
                                                lockargs.la_name);
                                        exit (1);
                                }
                                *ha_ptr='\0';
                                sprintf(HA2_CURRENT,"%s/%s",ha_dir, curdir);
                                sprintf(HA2_BACKUP,"%s/%s",ha_dir, backdir);
                                sprintf(HA2_STATE,"%s/%s",ha_dir, statefile);
                                if (Verbose) {
                                        syslog(LOG_INFO,"current %s, backup %s, state %s",
                                                HA2_CURRENT,HA2_BACKUP,HA2_STATE);
                                }
                                statd_init(HA2_CURRENT,HA2_BACKUP,HA2_STATE);
                       }

	   	}
	}

	transp = svcudp_create(RPC_ANYSOCK);
	if (transp == NULL) {
		syslog(LOG_ERR, "cannot create udp service.");
		exit(1);
	}
	if (!svc_register(transp, SM_PROG, SM_VERS, sm_prog_1, IPPROTO_UDP)) {
		syslog(LOG_ERR, "unable to register (SM_PROG, SM_VERS, udp).");
		exit(1);
	}

	transp = svctcp_create(RPC_ANYSOCK, 0, 0);
	if (transp == NULL) {
		syslog(LOG_ERR, "cannot create tcp service.");
		exit(1);
	}
	if (!svc_register(transp, SM_PROG, SM_VERS, sm_prog_1, IPPROTO_TCP)) {
		syslog(LOG_ERR, "unable to register (SM_PROG, SM_VERS, tcp).");
		exit(1);
	}
	statd_init(CURRENT, BACKUP, STATE);
	svc_run();
	syslog(LOG_ERR, "svc_run returned");
	exit(1);
}
