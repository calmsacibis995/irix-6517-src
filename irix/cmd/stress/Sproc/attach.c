#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "unistd.h"
#include "ulocks.h"
#include "stddef.h"
#include "getopt.h"
#include "sys/prctl.h"
#include "signal.h"
#include "stress.h"
#include "errno.h"
#include "wait.h"

/*
 * attach - test unrelated attaches
 */

void doexp(int, int, int interptime);
void alc(int);
pid_t spawnproc(void);

char *afile;		/* arena name */
int maxtotaltime;	/* max time (in S) to stay alive */
int maxinterpvtime;	/* time (in Ms) to wait after p before v */
int maxinterptime;	/* time in mS to wait between p's */
int timetoquit;
int verbose;
char *Cmd;

int
main(int argc, char **argv)
{
	int nprocs;		/* # of processes to run */
	int c, i;
	int nloops;

	Cmd = errinit(argv[0]);
	maxtotaltime = 5; /* 5 seconds */
	maxinterpvtime = 2; /* mS */
	maxinterptime = 7; /* mS */

	nprocs = 2;
	nloops = 100;

	while ((c = getopt(argc, argv, "n:dp:vt:")) != EOF) {
		switch (c) {
		case 'd':
			putenv("USTRACE=1");
			putenv("USTRACEFILE=attach.log");
			break;
		case 'v':
			verbose++;
			break;
		case 'p':
			nprocs = atoi(optarg);
			break;
		case 't':
			maxtotaltime = atoi(optarg);
			break;
		case 'n':
			nloops = atoi(optarg);
			break;
		default:
			fprintf(stderr, "Usage:attach [-dv][-t time][-p procs][-n loops]\n");
			exit(-1);
		}
	}

	afile = tempnam(NULL, "attach");
	usconfig(CONF_INITUSERS, nprocs+1);

	for (i = 0; i < nprocs; i++)
		spawnproc();

	for (i = 0; i < nloops; i++) {
		if (wait(NULL) > 0)
			spawnproc();
		else
			errprintf(3, "wait returned error");
	}
	while (wait(0) >= 0 || errno == EINTR)
		;
	unlink(afile);
	printf("%s:%d:PASSED\n", Cmd, getpid());
	return 0;
}

pid_t
spawnproc(void)
{
	int totaltime = 0, interpvtime, interptime;
	pid_t pid;

	while (totaltime == 0)
		totaltime = rand() % maxtotaltime;
	interpvtime = rand() % maxinterpvtime;
	interptime = rand() % maxinterptime;

	if ((pid = fork()) == 0) {
		/* child */
		doexp(totaltime, interpvtime, interptime);
		exit(0);
	} else if (pid < 0) {
		if (errno != EAGAIN)
			errprintf(ERR_ERRNO_EXIT, "fork failed");
			/* NOTREACHED */

		fprintf(stderr, "%s:can't fork:%s\n", Cmd, strerror(errno));
		/* no big deal */
	}
	return(pid);
}

void
doexp(int totaltime, int interpvtime, int interptime)
{
	usptr_t *handle;
	usema_t *s;

	sigset(SIGHUP, SIG_DFL);
	sigset(SIGALRM, alc);
	prctl(PR_TERMCHILD);
	if (getppid() == 1)
		exit(-1);

	if ((handle = usinit(afile)) == NULL)
		errprintf(1, "pid %d usinit of %s failed", getpid(), afile);

	while ((s = (usema_t *)usgetinfo(handle)) == NULL) {
		/* I am first */
		if ((s = usnewsema(handle, 1)) == NULL) {
			errprintf(1, "pid %d usnewsema failed", getpid());
		}
		if (uscasinfo(handle, NULL, s) == 0) {
			/* someone else there! */
			fprintf(stderr, "%s:pid %d raced to info!\n",
				Cmd, getpid());
			usfreesema(s, handle);
		}
	}
	timetoquit = 0;
	alarm(totaltime);

	while (!timetoquit) {
		if (uspsema(s) < 0)
			errprintf(1, "pid %d uspsema failed", getpid());
		sginap(interpvtime);
		if (usvsema(s) < 0)
			errprintf(1, "pid %d usvsema failed", getpid());
		sginap(interptime);
	}
}

/* ARGSUSED */
void
alc(int sig)
{
	timetoquit++;
}
