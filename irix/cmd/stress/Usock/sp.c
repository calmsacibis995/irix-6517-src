#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

/* ARGSUSED */
void
ahand(int sig)
{
	exit(0);
}

void
error(char *s)
{
	perror(s);
	exit(1);
}

void
usage(void)
{
	fprintf(stderr,"usage: sp [-d] [-i intvl] [-n nproc]\n");
	exit(1);
}

char buf[BUFSIZ];

main(int argc, char **argv)
{
	extern char *optarg;
	extern int optind;
	int socktype = SOCK_STREAM;
	int sv[2];
	int secs = 300;
	int r, c;
	int i;
	int nproc = 1;

	memset(buf, 'a', sizeof(buf));

	while ((c = getopt(argc, argv, "i:n:d")) != -1) {
		switch (c) {
		case 'i':
			secs = atoi(optarg);
			break;
		case 'd':
			socktype = SOCK_DGRAM;
			break;
		case 'n':
			nproc = atoi(optarg);
			if (nproc < 1) {
				fprintf(stderr,"sp: bad proc count %s\n",
					optarg);
				exit(1);
			}
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc) {
		usage();
	}

       	sigset(SIGCHLD, SIG_IGN);
	for (i = 0; i < nproc; i++) {
		switch (fork()) {
		case -1:
			kill(0, SIGTERM);
			error("fork");
		case 0:
			goto child;
		default:
			continue;
		}
	}

	signal(SIGALRM, ahand);
	alarm(secs);
	pause();

child:

	signal(SIGALRM, ahand);
	alarm(secs);
	while (1) {
	        r = socketpair(AF_UNIX, socktype, 0, sv);
		if (r < 0) {
			error("socketpair");
		}
		switch(fork()) {
		case -1:
			error("fork");
		case 0:
		        /* The children should have been finished by 10 secs*/
		        alarm(10);
			r = write(sv[1], buf, sizeof(buf));
			if (r < 0) {
				error("write");
			}
			r = read(sv[1], buf, sizeof(buf));
			if (r < 0) {
				error("read");
			}
			exit(0);
		default:
			r = write(sv[0], buf, sizeof(buf));
			if (r < 0) {
				error("write");
			}
			r = read(sv[0], buf, sizeof(buf));
			if (r < 0) {
				error("read");
			}
			close(sv[0]);
			close(sv[1]);
		}
	}
	/* NOTREACHED */
	exit(0);
}
