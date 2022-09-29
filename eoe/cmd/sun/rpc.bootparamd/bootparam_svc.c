#ifndef lint
static char sccsid[] =  "@(#)bootparam_svc.c	1.1 88/03/07 4.0NFSSRC; from  1.2 87/11/17 SMI";
#endif

/*
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 */

/*
 * Main program of the bootparam server.
 */

#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <rpc/rpc.h>
#include <rpcsvc/bootparam.h>
#include <syslog.h>

#ifdef sgi
#define	dup2(x,y)	BSDdup2(x,y)
#endif

extern int debug;
extern int interdomain;

static void background();
static void bootparamprog_1();

main(argc, argv)
	int argc;
	char **argv;
{
	SVCXPRT *transp;

	openlog("rpc.bootparamd", LOG_PID | LOG_NOWAIT, LOG_DAEMON);
	if (argc > 1)  {
		if (strncmp(argv[1], "-d", 2) == 0) {
			debug++;
			syslog(LOG_ERR, "In debug mode");
		} else if (strncmp(argv[1], "-i", 2) == 0) {
			interdomain++;
		} else {
			syslog(LOG_ERR, "usage: %s [-d]", argv[0]);
			exit(1);
		}
	}

	if (! issock(0)) {
		/*
		 * Started by user.
		 */
		if (!debug)
			background();

		pmap_unset(BOOTPARAMPROG, BOOTPARAMVERS);
		if ((transp = svcudp_create(RPC_ANYSOCK)) == NULL) {
			syslog(LOG_ERR, "cannot create udp service.");
			exit(1);
		}
		if (! svc_register(transp, BOOTPARAMPROG, BOOTPARAMVERS,
		    bootparamprog_1, IPPROTO_UDP)) {
			syslog(LOG_ERR, "unable to register (BOOTPARAMPROG, BOOTPARAMVERS, udp).");
			exit(1);
		}
	} else {
		/*
		 * Started by inetd.
		 */
		if ((transp = svcudp_create(0)) == NULL) {
			syslog(LOG_ERR, "cannot create udp service.");
			exit(1);
		}
		if (!svc_register(transp, BOOTPARAMPROG, BOOTPARAMVERS,
		    bootparamprog_1, 0)) {
			syslog(LOG_ERR, "unable to register (BOOTPARAMPROG, BOOTPARAMVERS, udp).");
			exit(1);
		}
	}

	/*
	 * Start serving
	 */
	svc_run();
	syslog(LOG_ERR, "svc_run returned");
	exit(1);
}

static void
background()
{
#ifndef DEBUG
	if (fork())
		exit(0);
	{ int s;
	for (s = 0; s < 10; s++)
		(void) close(s);
	}
	(void) open("/", O_RDONLY);
	(void) dup2(0, 1);
	(void) dup2(0, 2);
#endif
	{ int tt = open("/dev/tty", O_RDWR);
	  if (tt > 0) {
		ioctl(tt, TIOCNOTTY, 0);
		close(tt);
	  }
	}
}

static void
bootparamprog_1(rqstp, transp)
	struct svc_req *rqstp;
	SVCXPRT *transp;
{
	union {
		bp_whoami_arg bootparamproc_whoami_1_arg;
		bp_getfile_arg bootparamproc_getfile_1_arg;
	} argument;
	char *result;
	bool_t (*xdr_argument)(), (*xdr_result)();
	char *(*local)();
	extern bp_whoami_res *bootparamproc_whoami_1();
	extern bp_getfile_res *bootparamproc_getfile_1();

	if (debug)
		syslog(LOG_ERR,"receive request %d", rqstp->rq_proc);
	switch (rqstp->rq_proc) {
	case NULLPROC:
		svc_sendreply(transp, xdr_void, NULL);
		return;

	case BOOTPARAMPROC_WHOAMI:
		xdr_argument = xdr_bp_whoami_arg;
		xdr_result = xdr_bp_whoami_res;
		local = (char *(*)()) bootparamproc_whoami_1;
		break;

	case BOOTPARAMPROC_GETFILE:
		xdr_argument = xdr_bp_getfile_arg;
		xdr_result = xdr_bp_getfile_res;
		local = (char *(*)()) bootparamproc_getfile_1;
		break;

	default:
		svcerr_noproc(transp);
		return;
	}
	bzero(&argument, sizeof(argument));
	if (! svc_getargs(transp, xdr_argument, &argument)) {
		svcerr_decode(transp);
		return;
	}
	if ((result = (*local)(&argument)) != NULL) {
		if (!svc_sendreply(transp, xdr_result, result)) {
			svcerr_systemerr(transp);
			if (debug)
				syslog(LOG_ERR, "svc_sendreply failed");
		} else if (debug)
			syslog(LOG_ERR, "svc_sendreply done");
	} else if (debug)
		syslog(LOG_ERR, "request %d failed", rqstp->rq_proc);

	if (! svc_freeargs(transp, xdr_argument, &argument)) {
		syslog(LOG_ERR,"unable to free arguments");
		exit(1);
	}
}
