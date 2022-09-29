/*
 * Copyright (c) 1985 Sun Microsystems, Inc.
 */

/* NFS server */

#include <sys/param.h>
#include <rpc/rpc.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/fs/nfs.h>
#include <stdio.h>
#include <signal.h>
#include <syslog.h>
#include <sys/capability.h>
#include <errno.h>
#include <getopt.h>
#include <t6net.h>
#include <sys/types.h>
#include <sys/sysmp.h>

/* ARGSUSED */
static void
catch(int sig)
{
}

static char *progname; 

static void
usage(void)
{
	fprintf(stderr, "usage: %s [-a] [-p protocol] [-c #conn] [nservers]\n", progname);
	exit(1);
}

#define NFSD_TCP  0x1
#define NFSD_UDP  0x2

static const cap_value_t cap_svc[] = {CAP_DAC_READ_SEARCH, CAP_DAC_WRITE,
				      CAP_MAC_READ, CAP_MAC_WRITE,
				      CAP_FOWNER, CAP_NETWORK_MGT};
static const int cap_svc_size = (int) (sizeof (cap_svc) / sizeof (cap_svc[0]));

/*
 * XXX:
 * NB: The first parameter to the nfssvc() syscall is a kludge for
 * kudzu/6.2 transition.  It (and it's corresponding code in the kernel)
 * should be removed when developers start running kudzu kernels on 
 * kudzu-built nfsds.
 */
int
main(int argc, char *argv[])
{
	register int socktcp, sockudp, so, c;
	struct sockaddr_in addr;
	int len = sizeof(struct sockaddr_in);
	char *dir = "/";
	int pid, t;
	int tcp = 0; 
	int udp = 0;
	int nservers;
	int nconns = 0;
	cap_t ocap;
	const cap_value_t cap_chroot = CAP_CHROOT;

	progname = argv[0];

	while ((c = getopt(argc, argv, "ap:c:")) != EOF) {
	    switch(c) {
		case 'a':
		    tcp = 1;
		    udp = 1;
		    break;
		case 'p':
		    if (!strcmp(optarg,"udp") || !strcmp(optarg,"UDP"))
			udp = 1;
		    else if (!strcmp(optarg,"tcp") || !strcmp(optarg,"TCP"))
			tcp = 1;
		    else
			fprintf(stderr,"%s: Only tcp and udp protocols supported\n",progname);
		    break;
		case 'c':
		    nconns = atoi(optarg);
		    break;
		default:
		    usage();
	    }
	}
	
	if (!udp && !tcp) {
	    tcp = udp = 1;
	}

	if (optind < argc)
	    nservers = atoi(argv[optind]);
	else {
	    nservers = sysmp(MP_NAPROCS);
	    if (nservers < 4)
		nservers = 4;
	}

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

	if (udp) {
	    if (((sockudp = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
		|| (tsix_on(sockudp) < 0)
		|| (bind(sockudp, &addr, len) != 0)
		|| (getsockname(sockudp, &addr, &len) != 0) ) {
		    (void)close(sockudp);
		    syslog(LOG_ERR, "%s: udp server already active", argv[0]);
		    udp = 0;
	    }
	}
	if (tcp) {
	    if (((socktcp = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
		|| (tsix_on(socktcp) < 0)
		|| (bind(socktcp, &addr, len) != 0)
		|| (getsockname(socktcp, &addr, &len) != 0) ) {
		    (void)close(socktcp);
		    syslog(LOG_ERR, "%s: tcp server already active", argv[0]);
		    tcp = 0;
	    }
	}
	if (udp || tcp) {
	    pmap_unset(NFS_PROGRAM, NFS_VERSION);
	    pmap_unset(NFS_PROGRAM, NFS3_VERSION);
	    if (udp) {
		pmap_set(NFS_PROGRAM, NFS_VERSION, IPPROTO_UDP, NFS_PORT);
		pmap_set(NFS_PROGRAM, NFS3_VERSION, IPPROTO_UDP, NFS_PORT);
	    }
	    if (tcp) {
		pmap_set(NFS_PROGRAM, NFS_VERSION, IPPROTO_TCP, NFS_PORT);
		pmap_set(NFS_PROGRAM, NFS3_VERSION, IPPROTO_TCP, NFS_PORT);
	    }
	} else
	    exit(-1);

	/* umask(0); */

	if (tcp) {
	    if (!fork()) {
		signal(SIGTERM, catch);
		ocap = cap_acquire(cap_svc_size, cap_svc);
		nfssvc(-1, socktcp, NFSD_TCP, nconns);
		cap_surrender(ocap);
		if (errno)
		    syslog(LOG_ERR, "%s: tcp config failed %d\n", argv[0], errno);
		exit(errno);
	    }
	}

	while (--nservers) 
	    if (!fork()) 
		break;

	signal(SIGTERM, catch);
	ocap = cap_acquire(cap_svc_size, cap_svc);
	nfssvc(-1, -1, (udp ? NFSD_UDP : 0), 0);
	cap_surrender(ocap);
	/* comes here only on sigterm */
	return(errno);
}

