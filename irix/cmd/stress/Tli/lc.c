#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <tiuser.h>
#include <fcntl.h>
#include <fcntl.h>

#define	NUM_CONN	20

int port = 9999;
int num_clients = 1;

char *progname;

void
error(char *s)
{
	extern int t_errno;
	fprintf(stderr,"%s[%d]: ", progname, getpid());
	if (t_errno == TSYSERR) {
		(void) perror(s);
	} else {
		t_error(s);
	}
	exit(1);
}

void
ahand(int s)
{
	kill(0, SIGTERM);
	exit(0);
}

void
usage(void)
{
	(void) fprintf(stderr, "usage: c [-n num] [-p port] host\n");
	exit(1);
}

main(int argc, char **argv)
{
	struct sockaddr_in sin;
	int r;
	struct t_call *creq = 0;
	int secs = 0;
	int i;
	int c;
	int cid[NUM_CONN];
	pid_t pid;

	struct hostent *hp;

	progname = argv[0];

	while ((c = getopt(argc, argv, "i:p:n:")) != EOF) {
		switch (c) {
		case 'n':
			num_clients = atoi(optarg);
			break;
		case 'i':
			secs = atoi(optarg);
			break;
		case 'p':
			port = atoi(optarg);
			break;
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;

	if (argc != 1) {
		usage();
	}

	setsid();
	sin.sin_port = htons(port);
	sin.sin_family = AF_INET;

	if (inet_aton(*argv, &sin.sin_addr) == 0) {
		hp = gethostbyname(*argv);
		if (hp == (struct hostent *)0) {
			(void) herror(*argv);
			exit(1);
		}

		(void) memcpy((char *)&sin.sin_addr.s_addr, hp->h_addr, 
			sizeof(sin.sin_addr.s_addr));
	}

	(void) printf("connecting to %s\n",inet_ntoa(sin.sin_addr));

	for (i = 0; i < num_clients; i++) {
		if ((pid = fork()) == 0) {
			goto child;
		}
		if (pid == -1) {
			error("fork");
			(void) kill(0, SIGTERM);
			exit(0);
		}
	}
	signal(SIGALRM, ahand);
	if (secs)
		alarm(secs);
	(void) pause();
	(void) kill(0, SIGTERM);
	exit(0);
	
child:
	i = 0;

	while (1) {
		/*
		 * keep connections around a while so that server builds
		 * up more in TIME-WAIT by closing first.
		 */
		cid[i] = t_open("/dev/ticotsord", O_RDWR, (struct t_info *)0);

		if (cid[i] < 0) {
			error("/dev/ticotsord");
		}

		r = t_bind(cid[i], (struct t_bind *)0, (struct t_bind *)0);
		if (r < 0) {
			error("t_bind");
		}
		if (creq == 0) {
			creq = (struct t_call *)t_alloc(cid[i], T_CALL, 0);
			if (creq == 0) {
				error("t_alloc");
			}

			creq->addr.buf = (char *)&sin;
			creq->addr.len = sizeof(sin);
		}

		r = t_connect(cid[i], creq, (struct t_call *)0);
		if (r < 0) {
			(void) t_close(cid[i]);
			continue;
		}
		i++;
		if (i == NUM_CONN) {
			int j;
			for (j = 0; j < NUM_CONN; j++) {
				(void) t_close(cid[j]);
			}
			i = 0;
		}
	}
	/* NOTREACHED */
	exit(0);
}
