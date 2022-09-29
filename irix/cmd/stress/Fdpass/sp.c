#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>

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
	fprintf(stderr,"usage: sp [-d] [-i secs] [-n proc] [file]\n");
	exit(1);
}

char buf[BUFSIZ];

main(int argc, char **argv)
{
	extern char *optarg;
	extern int optind;
	struct iovec iov;
	struct msghdr msg;
	int socktype = SOCK_STREAM;
	int sv[2];
	int secs = 300;
	int r, c;
	int i;
	int pfd;
	int ppfd;
	char *file = "/etc/passwd";
	int nproc = 1;

	/* we don't want the parent to exit if the child finishes first */
	sigset(SIGCLD, SIG_IGN);

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

	if (argc > 1) {
		usage();
	}
	if (argc) {
		file = argv[1];
	}

	printf("file = %s\n", file);

	sigset(SIGCLD, SIG_IGN);
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
	if (secs)
		alarm(secs);
	pause();
	kill(0, SIGTERM);
child:

	signal(SIGALRM, ahand);
	if (secs)
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
			/*
			 * Child process opens the file and then passes the
			 * descriptor to the parent via sendmsg()
			 */
			pfd = open(file, O_RDONLY, 0644);
			if (pfd == -1) {
				error(file);
			}

			/*
			 * We send one byte of data so that the parent will block.
			 * Otherwise, recvmsg() may return before our sendmsg() call
			 * completes
			 */
			iov.iov_base = buf;
			iov.iov_len = 1;
			msg.msg_iov = &iov;
			msg.msg_iovlen = 1;
			/*
			 * We don't need an address since we are connected
			 */
			msg.msg_name = (caddr_t)0;
			msg.msg_namelen = 0;
			/*
			 * "access rights" are file descriptors.  The format is
			 * an array of integers, with the length being the number
			 * of bytes, i.e. (nfds * sizeof(fd)).
			 * Since we only have one, we can refer to it directly, and
			 * the length is its size.
			 */
			msg.msg_accrights = (caddr_t)&pfd;
			msg.msg_accrightslen = sizeof(pfd);
			r = sendmsg(sv[1], &msg, 0);
			if (r < 0) {
				error("sendmsg");
			}
			exit(0);
			/* NOTREACHED */
		default:
			/*
			 * Set up for receive in the parent.
			 * We expect no address, but we read one byte of data to
			 * ensure that we won't return before the child has a chance
			 * to execute.
			 */
			iov.iov_base = buf;
			iov.iov_len = sizeof(buf);
			msg.msg_iov = &iov;
			msg.msg_iovlen = 1;
			msg.msg_name = (caddr_t)0;
			msg.msg_namelen = 0;
			/*
			 * When the recvmsg() completes, ppfd will now contain the
			 * file descriptor passed from the child, adjusted for our
			 * open descriptor table (in other words, it might be on a
			 * different fd, but will refer to the correct file.
			 */
			msg.msg_accrightslen = sizeof(ppfd);
			msg.msg_accrights = (caddr_t)&ppfd;

			r = recvmsg(sv[0], &msg, 0);
			if (r < 0) {
				error("recvmsg");
			}

			if ((msg.msg_accrights == 0) ||
			    (msg.msg_accrightslen != sizeof(ppfd))) {
				/* something went wrong */
				error("no access rights");
			}

			/*
			 * Dump the contents of the file out to the terminal
			 */
			while ((r = read(ppfd, buf, sizeof(buf))) > 0) {
				write(1, buf, r);
			}
			(void)close(ppfd);
			(void)close(sv[0]);
			(void)close(sv[1]);
		}
	}
	/* NOTREACHED */
	exit(0);
}
