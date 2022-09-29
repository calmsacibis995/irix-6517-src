#include <sys/types.h>
#include <sys/stropts.h>
#include <sys/poll.h>
#include <sys/syssgi.h>
#include <sys/wait.h>
#include <sys/prctl.h>

#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <bstring.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include "stress.h"
#include <getopt.h>

char *Cmd;
int verbose;
int halfway;	/* send but don't receive files */
int nfiles = 20;
char **fnames;
void usage(void);
void sigc(int sig);
void cleanup(void);

int
recvfd(int recvstr, int *rfds, int nrfds)
{
    int i;
    struct strrecvfd rfd;

    for (i = 0; i < nrfds; i++) {
	if (ioctl(recvstr, I_RECVFD, &rfd) != 0)
		return -1;
	*rfds++ = rfd.fd;
    }
    return 0;
}

int
sendfd(int sendstr, int *sfds, int nsfds)
{
    int i;

    if (verbose) {
	printf("Sending %d fds:", nsfds);
	for (i = 0; i < nsfds; i++)
		printf("%d ", sfds[i]);
	printf("\n");
    }
    
    for (i = 0; i < nsfds; i++) {
	if (ioctl(sendstr, I_SENDFD, *sfds) != 0)
		return -1;
	sfds++;
    }
    return 0;
}

void
hello(void *a)
{
	int fd = (int)(ptrdiff_t)a;
	write(fd, "hello world\n", 12);
}

int
main(int argc, char *argv[])
{
	int sendstr, recvstr, p[2], st;
	pid_t pid, spid;
	int c, i, j;
	int sflags = PR_SADDR;
	int *rfds, *sfds;
	int nper = 5;

	Cmd = errinit(argv[0]);
	while ((c = getopt(argc, argv, "hvp:n:f")) != EOF)
		switch (c) {
		case 'v':
			verbose = 1;
			break;
		case 'n':
			nfiles = atoi(optarg);
			break;
		case 'f':
			sflags |= PR_SFDS;
			break;
		case 'p':
			nper = atoi(optarg);
			break;
		case 'h':
			halfway = 1;
			break;
		default:
			usage();
			/* NOTREACHED */
		}

	/* round nfiles up to multiple of nper */
	nfiles = (((nfiles + nper - 1) / nper) * nper);
	fnames = (char **)malloc(nfiles * sizeof(char *));
	for (i = 0; i < nfiles; i++)
		fnames[i] = NULL;
	rfds = malloc(nper * sizeof(int));
	sfds = malloc(nper * sizeof(int));

	sigset(SIGPIPE, sigc);
	syssgi(SGI_SPIPE, 1); /* turn on streams pipes */

	if (pipe(p) < 0) {
		errprintf(1, "pipe");
		/* NOTREACHED */
	}

	if (verbose)
		printf("%s:sending %d fds - %d at a time\n", Cmd, nfiles, nper);
	sendstr = p[0];
	recvstr = p[1];

	pid = fork();

	if (pid > 0) {
		/* parent */
		close(recvstr);
		for (i = 0; i < nfiles; i += nper) {
			for (j = 0; j < nper; j++) {
				int fno = j + i;
				fnames[fno] = tempnam(NULL, "fds");

				sfds[j] = open(fnames[fno], O_WRONLY | O_CREAT, 0666);
				if (sfds[j] < 0) {
					errprintf(3, fnames[fno]);
					goto bad;
				}
			}
			if (verbose)
				printf("%s:hifd %d\n", Cmd, getdtablehi());
			if (sendfd(sendstr, sfds, nper)) {
				errprintf(3, "sendfd");
				goto bad;
			}
		        for (j = 0; j < nper; j++)
				if (close(sfds[j])) {
					errprintf(3, "close of fd %d failed", sfds[j]);
					goto bad;
				}
			if (verbose)
				printf("%s:hifd %d\n", Cmd, getdtablehi());
		}
		unblockproc(pid);
	} else if (pid == 0) {
		/* child */
		close(sendstr);
		for (i = 0; i < nfiles; i += nper) {
		    struct pollfd pfd;

		    pfd.fd = recvstr;
		    pfd.events = POLLIN;
		    if (poll(&pfd, 1, -1) < 0) {
			errprintf(1, "poll");
			/* NOTREACHED */
		    }
		    if (halfway)
			continue;

		    if (recvfd(recvstr, rfds, nper)) {
			errprintf(1, "recvfd");
			/* NOTREACHED */
		    }

		    spid = sproc(hello, sflags, (void *)(ptrdiff_t)rfds[0]);

		    if (spid < 0) {
			errprintf(3, "sproc");
			continue;
		    }

		    waitpid(spid, &st, 0);
		    if (verbose)
			printf("%s:child:hifd %d\n", Cmd, getdtablehi());
		    for (j = 0; j < nper; j++)
			    if (close(rfds[j])) {
				errprintf(3, "close of fd %d failed", rfds[j]);
				goto bad;
			    }
		    if (verbose)
			printf("%s:child:hifd %d\n", Cmd, getdtablehi());
		}
		/* for halfway case - let parent finish .. */
		blockproc(getpid());
		exit(0);
	} else {
		/* not really a catasrophe ... */
		if (errno != EAGAIN) {
			errprintf(ERR_ERRNO_RET, "fork");
			goto bad;
		} else
			fprintf(stderr, "%s:fork failed:%s\n", Cmd, strerror(errno));
	}
	cleanup();
	exit(0);
bad:
	cleanup();
	abort();
	/* NOTREACHED */
}

void
usage(void)
{
	fprintf(stderr, "Usage:%s [-vfh][-n files][-p #fds]\n", Cmd);
	fprintf(stderr, "\t-v\tverbose\n");
	fprintf(stderr, "\t-h\tsend but don't receive fds\n");
	fprintf(stderr, "\t-f\thave sproc child shared fds\n");
	fprintf(stderr, "\t-n\ttotal # files to create\n");
	fprintf(stderr, "\t-p\tfiles to send at once\n");
}

void
sigc(int sig)
{
	errprintf(2, "Caught signal %d\n", sig);
	cleanup();
	abort();
}

void
cleanup(void)
{
	int i;
	for (i = 0; i < nfiles; i++)
		if (fnames[i])
			unlink(fnames[i]);
}
