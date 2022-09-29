#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <rpc/rpc.h>
#define _SERVER
#include "argv.h"
#include "rfind.h"
#include <sys/stat.h>
#include <sys/ioctl.h>
#include "externs.h"

#define RPC_INETDSOCK	0	/* socket descriptor if using inetd */

static void rfindd (struct svc_req *, SVCXPRT *);
static void detachfromtty(void);
static int issock(int fd);

/*
 * Remove the mapping between program,version and port.
 * Calls the pmap service remotely to do the un-mapping.
 */
extern bool_t pmap_unset(u_long program, u_long version);

main (int argc, char *argv[]){
	SVCXPRT *transp;
	int proto;
	int sock;
	char *cmd;

	signal(SIGPIPE, SIG_IGN);
	signal(SIGCLD, SIG_IGN);
	cmd = argv[0];
	switch (argc) {
	case 0:
		cmd = "rfindd";
		break;
	case 1:
		break;
	default:
		fprintf(stderr, "%s: warning -- options ignored\n", cmd);
		break;
	}

	if (issock(0)) {
		sock = RPC_INETDSOCK;
		proto = 0;
	} else {
		detachfromtty();
		sock = RPC_ANYSOCK;
		(void)pmap_unset(RFINDPROG, RFINDVERS);
		proto = IPPROTO_TCP;
	}
	transp = svctcp_create(sock, 0, 0);
	if (transp == NULL) {
		(void)fprintf(stderr, "cannot create tcp service.\n");
		exit(1);
	}
	if (!svc_register(transp, RFINDPROG, RFINDVERS, rfindd, proto)) {
		(void)fprintf(stderr, "unable to register (RFINDPROG, RFINDVERS, tcp).\n");
		exit(1);
	}
	svc_run();
	(void)fprintf(stderr, "svc_run returned\n");
	exit(1);
	/* NOTREACHED */
}

/*
 * Determine if a descriptor belongs to a socket or not
 */
int issock(int fd)
{
	struct stat64 st;

	if (fstat64(fd, &st) < 0) {
		return (0);
	} 
	/*       
	 * SunOS returns S_IFIFO for sockets, while 4.3 returns 0 and
	 * does not even have an S_IFIFO mode.  Since there is confusion 
	 * about what the mode is, we check for what it is not instead of 
	 * what it is.
	 */
	switch ((int) (st.st_mode & S_IFMT)) {
	case S_IFCHR:
	case S_IFREG:
	case S_IFLNK:
	case S_IFDIR:
	case S_IFBLK:
		return (0);
	default:	 
		return (1);
	}
}

static void detachfromtty(void)
{
	int tt;

	close(0);
	close(1);
	close(2);
	switch (fork()) {
	case -1:
		perror("fork");
		break;
	case 0:
		break;
	default:
		exit(0);
	}
	tt = open("/dev/tty", O_RDWR, 0);
	if (tt >= 0) {
		ioctl(tt, TIOCNOTTY, 0);
		close(tt);
	}
	(void) open("/dev/null", O_RDWR, 0);
	dup(0);
	dup(0);
}


SVCXPRT *transp;	/* port back to client: rfindd sets below; it and rpc_io.c use */
void sigpipe (int s);
void sigalrm (int s);

static void rfindd (struct svc_req *rqstp, SVCXPRT *tp) {
	arguments args;
	int result;

	transp = tp;
	switch ((int)(rqstp->rq_proc)) {
	case NULLPROC:
		(void)svc_sendreply(transp, xdr_void, (char *)NULL);
		return;

	default:
		svcerr_noproc(transp);
		return;

	case RFIND:
		break;
	}

	bzero((char *)&args, sizeof(args));
	if (!svc_getargs(transp, xdr_arguments, &args)) {
		svcerr_decode(transp);
		exit(0);
	}

	if (fork()) return;

	/*
	 * If client dies - we get a SIGPIPE on our next write,
	 * and take that as a clue to go away.
	 */
	signal(SIGPIPE, sigpipe);

	/*
	 * If client blocks our output for a very long time, like 4 hours,
	 * then we complain and exit.  This is so that a client cannot
	 * force the large amount of disk space used by an fsdump file
	 * to be kept around long after it is otherwise stale, filling
	 * up some server disk.
	 */
	signal(SIGALRM, sigalrm);
	alarm (4*60*60);
#if 0
    {
	int fd = open ("/tmp/rfind.log", O_WRONLY|O_CREAT, 0644);
	char log[1024];
	int i;

	sprintf(log,"argc: %u\n", args.argc);
	write(fd,log,strlen(log));
	for (i=0; i<args.argc; i++) {
		sprintf(log,"argv[%d] %s\n", i, args.argv[i]);
		write(fd,log,strlen(log));
	}
	close(fd);
    }
#endif
	result = 1;
	if (!svc_sendreply(transp, xdr_int, &result)) {
		svcerr_systemerr(transp);
		exit(0);
	}

	main_find (args.argc, args.argv);
	/* NOTREACHED */
}
