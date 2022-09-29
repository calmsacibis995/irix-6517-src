#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
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
usage(void)
{
	fprintf(stderr, "usage: udgclose [-i secs] [port]\n");
	exit(1);
}

void
error(char *s)
{
	perror(s);
	exit(1);
}

char buf[BUFSIZ];

main(int argc, char **argv)
{
	int s;
	int r;
	struct sockaddr_un sun;
	unsigned sunlen;
	char *port;
	int c;
	int secs = 300;

	while ((c = getopt(argc, argv, "i:")) != EOF) {
		switch(c) {
		case 'i':
			secs = atoi(optarg);
			break;
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;

	if (argc > 0) {
		port = argv[0];
	} else {
		port = "/tmp/9999";
	}

	signal(SIGALRM, ahand);
	alarm(secs);

	while (1) {
		s = socket(AF_UNIX, SOCK_DGRAM, 0);
		if (s < 0) {
			error("socket");
		}

		memset((char *)&sun, 0, sizeof(sun));
		sun.sun_family = AF_UNIX;
		memcpy(sun.sun_path, port, strlen(port));


		(void) unlink(port);
		r = bind(s, (struct sockaddr *)&sun, sizeof(sun.sun_family) +
			strlen(port) + 1);
		if (r < 0) {
			error("bind");
		}

		close(s);
	}
	/* NOTREACHED */
	exit(0);
}
