#ifndef lint
static char sccsid[] = 	"@(#)rpc.rwalld.c	1.1 88/03/07 4.0NFSSRC Copyr 1988 Sun Micro";
#endif

#include <rpcsvc/rwall.h>
#include <rpc/rpc.h>
#include <stdio.h>
#include <netdb.h>
#include <sys/socket.h>
#include <signal.h>

void splat();
void die();

main()
{
	register SVCXPRT *transp;
	struct sockaddr_in addr;
	int len = sizeof(struct sockaddr_in);
	
	(void) signal (SIGALRM, die);

	if (getsockname(0, &addr, &len) != 0) {
		perror("rstat: getsockname");
		exit(1);
	}
	if ((transp = svcudp_create(0)) == NULL) {
		fprintf(stderr, "svc_rpc_udp_create: error\n");
		exit(1);
	}
	if (!svc_register(transp, WALLPROG, WALLVERS, splat, 0)) {
		fprintf(stderr, "svc_rpc_register: error\n");
		exit(1);
	}
	svc_run();
	fprintf(stderr, "Error: svc_run shouldn't have returned\n");
}

char *oldmsg;

void
splat(rqstp, transp)
	register struct svc_req *rqstp;
	register SVCXPRT *transp;
{
	FILE *fp;
	char *msg = NULL;

	switch (rqstp->rq_proc) {
		case 0:
			if (svc_sendreply(transp, xdr_void, 0)  == FALSE) {
				fprintf(stderr, "err: rusersd");
				exit(1);
			    }
			exit(0);
		case WALLPROC_WALL:
			if (!svc_getargs(transp, xdr_wrapstring, &msg)) {
			    	svcerr_decode(transp);
				exit(1);
			}
			if (svc_sendreply(transp, xdr_void, 0)  == FALSE) {
				fprintf(stderr, "err: rusersd");
				exit(1);
			}

			/* primitive duplicate filtering */
			if ((oldmsg == (char *) 0) || (strcmp (msg, oldmsg))) {
				fp = popen("/sbin/wall", "w");
				fprintf(fp, "%s", msg);
				(void) pclose(fp);
				if (oldmsg != (char *) 0)
					(void) svc_freeargs (transp,
							     xdr_wrapstring,
							     &oldmsg);
				oldmsg = msg;
			}
			alarm (60);
			return;
		default: 
			svcerr_noproc(transp);
			exit(0);
	}
}


void
die()
{
	exit (0);
}
