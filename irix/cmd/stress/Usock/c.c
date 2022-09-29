
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#define	NUM_CONN	20

char *port = "/tmp/9999";
int num_clients = 1;

void
error(char *s)
{
	(void) perror(s);
	exit(1);
}

/*
 * ARGSUSED
 */
void
ahand(int sig)
{
	exit(0);
}

void
usage(void)
{
	(void) fprintf(stderr, "usage: c [-v] [-i secs] [-n num] [-p port]\n");
	exit(1);
}

main(int argc, char **argv)
{
	struct sockaddr_un sun;
	int r;
	int i;
	int c;
	int cid[NUM_CONN];
	int secs = 300;
	pid_t pid;
	int whine = 0;

	while ((c = getopt(argc, argv, "vi:p:n:")) != EOF) {
		switch (c) {
		case 'v':
			whine++;
			break;
		case 'i':
			secs = atoi(optarg);
			break;
		case 'n':
			num_clients = atoi(optarg);
			break;
		case 'p':
			port = optarg;
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

	memset(sun.sun_path, 0, sizeof(sun.sun_path));
	memcpy(sun.sun_path, port, strlen(port));
	sun.sun_family = AF_UNIX;

	(void) printf("connecting to %s\n",port);

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
	(void) sigset(SIGALRM, ahand);
	(void) alarm(secs);
	(void) pause();
	(void) kill(0, SIGTERM);
	exit(0);
	
child:
	i = 0;
	signal(SIGALRM, ahand);
	alarm(secs);
	while (1) {
		cid[i] = socket(AF_UNIX, SOCK_STREAM, 0);

		if (cid[i] < 0) {
			error("socket");
		}
		r = connect(cid[i], (struct sockaddr *)&sun, 
			sizeof(sun.sun_family) + strlen(port) + 1);
		if (r < 0) {
			if (whine)
				(void) perror("connect");
			(void) close(cid[i]);
			continue;
		}
                i++;
                if (i == NUM_CONN) {
                        int j;
                        for (j = 0; j < NUM_CONN; j++) {
                                (void) close(cid[j]);
                        }
                        i = 0;
		}
	}
	/* NOTREACHED */
	exit(0);
}
