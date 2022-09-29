#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/sysmp.h>
#include <fcntl.h>
#include <tiuser.h>

int port = 9999;
int num_servers = 1;
int backlog = 32;
unsigned aint = 10;
int lock = 0;
int acc_cnt = 0;

char *progname;

extern int t_errno;

void
error(char *s)
{
	fprintf(stderr,"%s[%d]: ", progname, getpid());
	if (t_errno == TSYSERR) {
		(void) perror(s);
	} else {
		t_error(s);
	}
	exit(1);
}

/* ARGSUSED */
void
die(int sig)
{
	_exit(0);
}

void
usage(void)
{
	(void) fprintf(stderr, 
		"usage: s [-L] [-s int] [-l backlog] [-n num] [-p port]\n");
	exit(1);
}

/* ARGSUSED */
void
ahand(int sig)
{
	kill(0, SIGTERM);
	exit(0);
}

char obuf[1024];

main(int argc, char **argv)
{
	int sinlen;
	struct sockaddr_in sin;
	struct t_call *cind = 0;
	struct t_bind *breq = 0;
	struct t_optmgmt *oreq = 0;
	struct opthdr *opt;
	int s, r;
	int s1;
	int i;
	int c;
	int on = 1;
	int ncpus;
	int secs = 0;
	int cpu = 0;
	pid_t pid;

	ncpus = sysmp(MP_NAPROCS);
	(void) printf("%s: ncpus = %d\n",argv[0], ncpus);

	progname = argv[0];

	while ((c = getopt(argc, argv, "i:Ls:l:p:n:")) != EOF) {
		switch (c) {
		case 'L':
			lock = 1;
			break;
		case 'n':
			num_servers = atoi(optarg);
			break;
		case 'i':
			secs = atoi(optarg);
			break;
		case 'p':
			port = atoi(optarg);
			break;
		case 'l':
			backlog = atoi(optarg);
			break;
		case 's':
			aint = atoi(optarg);
			break;
		default:
			usage();
		}
	}

	signal(SIGALRM, ahand);
	if (secs)
		alarm(secs);
	setsid();
restart:
	s = t_open("/dev/tcp", O_RDWR, (struct t_info *)0);

	if (s < 0) {
		error("t_open");
	}

	oreq = (struct t_optmgmt *)t_alloc(s, T_OPTMGMT, T_ALL);
	if (oreq == 0) {
		error("t_alloc");
	}
	oreq->opt.len = sizeof(struct opthdr) + sizeof(int);
	oreq->flags = T_NEGOTIATE;
	opt = (struct opthdr *)oreq->opt.buf;
	opt->level = SOL_SOCKET;
	opt->name = SO_REUSEADDR;
	opt->len = sizeof(int);
	*((int *)(opt + 1)) = 1;
	r = t_optmgmt(s, oreq, oreq);
	if (r < 0) {
		error("t_optmgmt");
	}
	(void) t_free((char *)oreq, T_OPTMGMT);
	breq = (struct t_bind *)t_alloc(s, T_BIND, 0);
	if (breq == 0) {
		error("t_alloc");
	}
	breq->addr.buf = (char *)&sin;
	breq->addr.len = sizeof(sin);
	breq->qlen = backlog;
	sin.sin_addr.s_addr = htonl(INADDR_ANY);
	sin.sin_port = htons((u_short)port);
	sin.sin_family = AF_INET;

	r = t_bind(s, breq, (struct t_bind *)0);
	if (r < 0) {
		error("t_bind");
	}

	(void) sigset(SIGUSR1, SIG_DFL);
	for (i = 0; i < num_servers; i++) {
		if ((pid = fork()) == 0) {
			if (lock) {
				sysmp(MP_MUSTRUN, cpu);
				cpu++;
				if (cpu == ncpus) {
					cpu = 0;
				}					
			}
			goto child;
		}
		if (pid == -1) {
			error("fork");
			(void) kill(0, SIGTERM);
			exit(0);
		}
	}
	(void) sigset(SIGUSR1, SIG_IGN);
	(void) sigset(SIGCLD, SIG_IGN);
	(void) sleep(3);
	(void) close(s);
	(void) kill(0, SIGUSR1);
	while (wait((int *)0) != -1)
		;
	goto restart;
	/* NOTREACHED */
	exit(0);
	
child:
	(void) sigset(SIGUSR1, die);
	while (1) {
		sinlen = sizeof(sin);
		if (cind == 0) {
			cind = (struct t_call *)t_alloc(s, T_CALL, 0);
			cind->addr.buf = (char *)&sin;
			cind->addr.maxlen = sinlen;
		}
		cind->opt.buf = obuf;
		cind->opt.maxlen = sizeof(obuf);
		r = t_listen(s, cind);
		if (r < 0) {
			if (errno == EINTR) {
				continue;
			}
			if (t_errno == TLOOK) {
				(void) t_rcvdis(s, (struct t_discon *)0);
				continue;
			}
			error("t_listen");
		}
getone:
		s1 = t_open("/dev/tcp", O_RDWR, (struct t_info *)0);
		if (s1 < 0) {
			perror("/dev/tcp");
			sleep(2);
			goto getone;
		}
		cind->opt.len = 0;
		r = t_accept(s, s1, cind);
		if (r < 0) {
			if (t_errno == TLOOK) {
				(void) t_rcvdis(s, (struct t_discon *)0);
				(void) t_close(s1);
				continue;
			}
			error("t_accept");
		}
		(void) t_close(s1);
		acc_cnt++;
	}
	/* NOTREACHED */
	exit(0);
}
