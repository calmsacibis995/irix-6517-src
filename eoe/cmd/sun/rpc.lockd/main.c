/* Copyright 1991 Silicon Graphics, Inc. All rights reserved. */

#if 0
static char sccsid[] = "@(#)prot_main.c	1.7 91/06/25 NFSSRC4.1 Copyr 1990 Sun Micro";
#endif

	/*
	 * Copyright (c) 1988 by Sun Microsystems, Inc.
	 */
#include <sys/param.h>
#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/fs/nfs.h>
#include <stdio.h>
#include <signal.h>
#include <syslog.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mac_label.h>
#include <rpcsvc/nlm_prot.h>
#include <netdb.h>
#include <sys/syssgi.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/capability.h>
#include <t6net.h>
extern int errno;
extern int __svc_label_agile;

extern int nfssvc(int);

/* ARGSUSED */
void
catch(int sig)
{
}

int report_sharing_conflicts;
int grace_period;
int lock_share_requests;
int LM_TIMEOUT;

struct lockd_args {
	char    la_name[MAXHOSTNAMELEN + 1];
	int     la_grace;
	u_int   la_lockshares:1;
	u_int   la_setgrace:1;
	u_int   ha_action:2;    	/* used for HA-failsafe */
	char    ha_dir[MAXPATHLEN];
};

static const cap_value_t cap_chroot = CAP_CHROOT;
static const cap_value_t cap_svc[] = {CAP_DAC_WRITE, CAP_NETWORK_MGT,
				      CAP_MAC_READ, CAP_MAC_WRITE};

int
main(int argc, char *argv[])
{
	struct lockd_args lockargs;
	register int sock;
	struct sockaddr_in addr;
	int len = sizeof(struct sockaddr_in);
	char *dir = "/";
	int nservers = 1;
	pid_t pid;
	int c;
	struct hostent *hp;
	char name[MAXHOSTNAMELEN + 1];
	cap_t ocap;

	__svc_label_agile = (sysconf(_SC_IP_SECOPTS) > 0);
	memset((void *) &lockargs, '\0', sizeof(lockargs));
	while ((c = getopt(argc, argv, "t:g:l")) != EOF) {
		switch (c) {
		case 't':
			break;
		case 'l':
			lockargs.la_lockshares = 1;
			break;
		case 'g':
			lockargs.la_setgrace = 1;
			lockargs.la_grace = atoi(optarg);
			break;
		default:
			/* report the documented options only */
			fprintf(stderr, "usage: rpc.lockd -l -g [grace_period] [servers]");
			return(0);
		}
	}
	if (optind < argc) {
		nservers = atoi(argv[optind]);
	}
	if (nservers <= 0) nservers = 1;

	if (gethostname(name, MAXHOSTNAMELEN) == -1) {
		perror("lockd: gethostname");
		exit(1);
	}
	if (!(hp = gethostbyname(name))) {
		herror(name);
		exit(1);
	}
	strcpy(lockargs.la_name, hp->h_name);

	/* 
	 * set ha_action to 0  to set the canonical name 
	 */

	lockargs.ha_action = 0;

	/*
	 * set the canonical client name
	 */
	ocap = cap_acquire(4, cap_svc);
	if (syssgi(SGI_LOCKDSYS, &lockargs, sizeof(struct lockd_args)) == -1) {
		cap_surrender(ocap);
		perror("lockd: Unable to set canonical name");
		exit(1);
	}
	cap_surrender(ocap);

	/*
	 * Set current and root dir to server root
	 */
	ocap = cap_acquire(1, &cap_chroot);
	if (chroot(dir) < 0) {
		cap_surrender(ocap);
		perror(dir);
		exit(1);
	}
	cap_surrender(ocap);
	if (chdir(dir) < 0) {
		perror(dir);
		exit(1);
	}

#ifndef DEBUG
	/*
	 * Background 
	 */
        pid = fork();
	if (pid < 0) {
		perror("nfsd: fork");
		exit(1);
	}
	if (pid != 0)
		exit(0);

	{ int s;
	  for (s = getdtablesize(); --s >= 0; )
		(void) close(s);
	}
#endif
	{ int tt = open("/dev/tty", O_RDWR);
	  if (tt >= 0) {
		ioctl(tt, TIOCNOTTY, 0);
		close(tt);
	  }
	}

	signal(SIGCLD, SIG_IGN);
	addr.sin_addr.s_addr = 0;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(NFS_PORT);
	/* register with the portmapper */
	pmap_unset(NLM_PROG, NLM_VERS);
	pmap_unset(NLM_PROG, NLM_VERSX);
	pmap_unset(NLM_PROG, NLM4_VERS);
	pmap_set(NLM_PROG, NLM_VERS, IPPROTO_UDP, NFS_PORT);
	pmap_set(NLM_PROG, NLM_VERSX, IPPROTO_UDP, NFS_PORT);
	pmap_set(NLM_PROG, NLM4_VERS, IPPROTO_UDP, NFS_PORT);
	pmap_set(NLM_PROG, NLM_VERS, IPPROTO_TCP, NFS_PORT);
	pmap_set(NLM_PROG, NLM_VERSX, IPPROTO_TCP, NFS_PORT);
	pmap_set(NLM_PROG, NLM4_VERS, IPPROTO_TCP, NFS_PORT);
	if ( ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
	    || (tsix_on(sock) < 0)
	    || (bind(sock, &addr, len) != 0)
	    || (getsockname(sock, &addr, &len) != 0) ) {
		(void)close(sock);
		/*
		 * nfsd or lockd is already running, just exit
		 */
		exit(0);
	}

	/*
	 * Start the servers.
	 */
	while (--nservers) {
		if (!fork()) {
			break;
		}
	}
	signal(SIGTERM, catch);
	ocap = cap_acquire(4, cap_svc);
	if (nfssvc(sock) == -1)
		perror("nfssvc");
	cap_surrender(ocap);
	/* Should never come here */
	pmap_unset(NLM_PROG, NLM_VERS);
	pmap_unset(NLM_PROG, NLM_VERSX);
	pmap_unset(NLM_PROG, NLM4_VERS);
	return(errno);
}

