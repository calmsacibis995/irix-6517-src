#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/sysmp.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

char *port = "/tmp/9999";
int num_servers = 1;
int backlog = 32;
unsigned aint = 10;
int lock = 0;
int acc_cnt = 0;

/* ARGSUSED */
void
ahand(int sig)
{
	kill(0, SIGTERM);
	exit(0);
}
 
void
error(char *s)
{
	perror(s);
	exit(1);
}

/* ARGSUSED */
void
die(int sig)
{
	exit(0);
}

void
usage(void)
{
	(void) fprintf(stderr, 
		"usage: s [-L] [-i intvl] [-s int] [-l backlog] [-n num] [-p port]\n");
	exit(1);
}

#if 0
void
ahand(int sig)
	/* ARGSUSED */
{
	static acnt = 0;

	acnt++;
	(void) printf("# of connections accepted == %d\n",acc_cnt);
	(void) printf("rate = %f conn/s\n", 
		(float)acc_cnt/((float)acnt * (float)aint));
	(void) alarm(aint);
}
#endif

main(int argc, char **argv)
{
	int sunlen;
	struct sockaddr_un sun;
	int s, r;
	int i;
	int c;
	int on = 1;
	int secs = 300;
	int ncpus;
	int cpu = 0;
	pid_t pid;

	ncpus = sysmp(MP_NAPROCS);

	while ((c = getopt(argc, argv, "Li:s:l:p:n:")) != EOF) {
		switch (c) {
		case 'L':
			lock = 1;
			break;
		case 'i':
			secs = atoi(optarg);
			break;
		case 'n':
			num_servers = atoi(optarg);
			break;
		case 'p':
			port = optarg;
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

	(void) sigset(SIGALRM, ahand);
	(void) alarm(secs);

	setsid();
restart:
	s = socket(AF_UNIX, SOCK_STREAM, 0);

	if (s < 0) {
		error("socket");
	}

	memset(sun.sun_path, 0, sizeof(sun.sun_path));
	memcpy(sun.sun_path, port, strlen(port));
	sun.sun_family = AF_UNIX;

	(void) unlink(port);
	r = bind(s, (struct sockaddr *)&sun, 
		sizeof(sun.sun_family) + strlen(port) + 1);
	if (r < 0) {
		error("bind");
	}

	(void) listen(s, backlog);

	sigset(SIGUSR1, SIG_DFL);
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
	(void) sleep(3);
	(void) close(s);
	(void) kill(0, SIGUSR1);
	while (wait((int *)0) != -1)
		;
	goto restart;
	exit(0);
	
child:
	sigset(SIGUSR1, die);
#if 0
	(void) sigset(SIGALRM, ahand);
	(void) alarm(aint);
#endif
	while (1) {
		sunlen = sizeof(sun);
		r = accept(s, (struct sockaddr *)&sun, &sunlen);
		if (r < 0) {
			if (errno == EINTR) {
				continue;
			}
			error("accept");
		}
		(void) close(r);
		acc_cnt++;
	}
	/* NOTREACHED */
	exit(0);
}
