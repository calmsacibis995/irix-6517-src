#include "stdlib.h"
#include "stdarg.h"
#include "unistd.h"
#include "stdio.h"
#include "getopt.h"
#include "errno.h"
#include "fcntl.h"
#include "sys/types.h"
#include "wait.h"
#include "signal.h"

/*
 * simulate users
 * The rlogin mode is difficult - lots of potential problems.
 * The first one is that one needs to get rid of the login babble
 * so this program looks for a prompt of some kind. Since folks
 * can have customized prompts this is difficult.
 */
#define MAXCFILES	100
#define MAXARGS		20
struct cmds {
	char *cmd;
	char *args[MAXARGS];
};

extern char *pickuid(int, char *);
extern pid_t spawnone(char *uid, char *, struct cmds *);
extern void catcher(int), almcatcher(int);
int verbose;
int dologins;
pid_t *pids;
int ncurrent;
char *prompt;

main(int argc, char **argv)
{
	int rv, i, c;
	siginfo_t sip;
	struct cmds cfiles[MAXCFILES];
	int errflg = 0, ncfiles;
	char *machine = "bigm";
	char *uidpref = "guest";
	int nuids = 1;
	int tlimit = -1;
	int ano = 0;

	sigset(SIGINT, catcher);
	sigset(SIGHUP, catcher);
	sigset(SIGALRM, almcatcher);
	while ((c = getopt(argc, argv, "U:t:p:u:m:vn:l")) != EOF)
		switch (c) {
		case 't':
			tlimit = atoi(optarg);
			break;
		case 'p':
			prompt = optarg;
			break;
		case 'l':
			dologins = 1;
			break;
		case 'n':
			ncurrent = atoi(optarg);
			break;
		case 'v':
			verbose++;
			break;
		case 'm':
			machine = optarg;
			break;
		case 'u':
			/* use this uid prefix */
			uidpref = optarg;
			break;
		case 'U':
			/* use this number of uids */
			nuids = atoi(optarg);
			break;
		default:
			errflg++;
			break;
		}

	
	bzero(cfiles, sizeof(cfiles));
	for (ncfiles = -1; optind < argc; optind++) {
		if (argv[optind][0] == '-') {
			/* arguments for previous command */
			cfiles[ncfiles].args[ano] = argv[optind];
			ano++;
		} else {
			/* new command */
			ncfiles++;
			cfiles[ncfiles].cmd = argv[optind];
			ano = 0;
		}
	}
	ncfiles++;

	if (ncfiles == 0 || errflg) {
		fprintf(stderr, "Usage:spawn [-v][-m mach][-l][-n #][-U #][-u user][-t sec] cmd [opt] [cmd [opt]]\n");
		fprintf(stderr, "\t-n #\tnumber of concurrent sessions\n");
		fprintf(stderr, "\t-v\tverbose\n");
		fprintf(stderr, "\t-u user\tuser id prefix\n");
		fprintf(stderr, "\t-U #\t# of different users to connect as\n");
		fprintf(stderr, "\t-m mach\tconnect to machine\n");
		fprintf(stderr, "\t-l\tuse rlogin rather than rsh\n");
		fprintf(stderr, "\t-t sec\trun for 'sec' seconds\n");
		exit(-1);
	}
	pids = calloc(ncurrent, sizeof(pid_t));
	for (i = 0; i < ncurrent; i++)
		pids[i] = spawnone(pickuid(nuids, uidpref),
				machine, &cfiles[rand()%ncfiles]);
	
	if (tlimit > 0)
		alarm(tlimit);
	for (;;) {
		while (((rv = waitid(P_ALL, 0, &sip, WEXITED)) < 0) && errno == EINTR)
			;
		if (rv < 0) {
			perror("waitid?");
			exit(-1);
		}
		if (verbose)
			printf("parent:pid %d finished\n", sip.si_pid);
		for (i = 0; i < ncurrent; i++) {
			if (pids[i] == sip.si_pid) {
				pids[i] = spawnone(pickuid(nuids, uidpref),
					machine, &cfiles[rand()%ncfiles]);
				break;
			}
		}
		if (i >= ncurrent)
			fprintf(stderr, "status for pid %d but ??\n", sip.si_pid);
	}
}

void
catcher(int signo)
{
	int i;

	if (verbose)
		printf("parent got signal %d\n", signo);
	for (i = 0; i < ncurrent; i++) {
		if (pids[i] > 0)
			kill(pids[i], SIGHUP);
	}
	if (signo != SIGALRM)
		exit(1);
}

void
almcatcher(int sig)
{
	while (wait(NULL) > 0  || errno == EINTR)
		;
	exit(0);
}

/*
 * run an rsh job
 * assumes a .rhost
 */
void
runrsh(char *command, char *uid, char *machine, char **cargs)
{
	char *args[40];
	char who[30];
	int i, fd;

	fd = open("/dev/null", O_WRONLY);
	close(1);
	dup(fd);
	sprintf(who, "%s@%s", uid, machine);
	args[0] = "rsh";
	args[1] = who;
	args[2] = "-n";
	args[3] = command;
	for (i = 4; *cargs; cargs++, i++)
		args[i] = *cargs;
	args[i] = NULL;
	execvp("rsh", args);
	perror("execv failed");
	exit(1);
}

pid_t rpid;
extern void lcatcher(int);
/*
 * run an rlogin job
 * assumes a .rhost so no password cruft
 */
void
runrlogin(char *command, char *uid, char *machine, char **cargs)
{
	siginfo_t sip;
	char *args[40];
	char who[30];
	int i, rv;
	int toremfd[2];
	int fromremfd[2];
	char fromhost[512];
	char tohost[512];

	pipe(toremfd);
	pipe(fromremfd);
	sigset(SIGINT, lcatcher);
	sigset(SIGHUP, lcatcher);
	if ((rpid = fork()) < 0)
		exit(2);
	else if (rpid == 0) {
		/* child */
		close(toremfd[1]);
		close(fromremfd[0]);
		close(0);
		dup(toremfd[0]);
		close(1);
		dup(fromremfd[1]);
		close(2);
		dup(fromremfd[1]);
		sprintf(who, "%s@%s", uid, machine);
		args[0] = "rlogin";
		args[1] = who;
		i = 2;
		args[i] = NULL;
		execvp("rlogin", args);
		perror("execv failed");
		exit(1);
	}
	close(toremfd[0]);
	close(fromremfd[1]);

	/* wait for something */
	while ((rv = read(fromremfd[0], fromhost, sizeof(fromhost))) > 0) {
		char *c;
		size_t n;

		fromhost[rv] = '\0';
		if ((c = strrchr(fromhost, '\n')) == NULL)
			c = fromhost;
		else
			c++;
		n = strlen(c);
		if (verbose > 1) {
			write(1, "<", 1);
			write(1, fromhost, rv);
			write(1, ">\n", 2);
			write(1, "C<", 2);
			write(1, c, 5);
			write(1, ">\n", 2);
		}
		if (strncmp(c, "$ ", 2) == 0 || strncmp(c, "# ", 2) == 0)
			break;
		if (strncmp(c + n - 2, "$ ", 2) == 0 ||
					strncmp(c + n - 2, "# ", 2) == 0)
			break;
		if (prompt) {
			size_t np = strlen(prompt);

			if ((n >= np) &&
				strncmp(c + n - np, prompt, np) == 0)
			break;
		}
	}
	if (rv == 0) {
		printf("eof from host\n");
		goto bad;
	} else if (rv < 0) {
		perror("read from host");
		goto bad;
	} else if (verbose > 1)
		write(1, "Logged in ok\n", 13);

	strcpy(tohost, "exec sh -c \"");
	strcat(tohost, command);
	strcat(tohost, " ");
	for (; *cargs; cargs++) {
		strcat(tohost, *cargs);
		strcat(tohost, " ");
	}
	strcat(tohost, "\"; exit\n");
	write(toremfd[1], tohost, strlen(tohost));

	while ((rv = read(fromremfd[0], fromhost, sizeof(fromhost))) > 0) {
		/* wait till another prompt */
		char *c;
		fromhost[rv] = '\0';
		if ((c = strrchr(fromhost, '\n')) == NULL)
			c = fromhost;
		else
			c++;
		if (verbose > 1) {
			write(1, "<", 1);
			write(1, fromhost, rv);
			write(1, ">\n", 2);
			write(1, "C<", 2);
			write(1, c, 5);
			write(1, ">\n", 2);
		}
		if (strncmp(c, "$ ", 2) == 0 || strncmp(c, "# ", 2) == 0)
			break;
		else if (prompt) {
			size_t n = strlen(c);
			size_t np = strlen(prompt);

			if ((n >= np) &&
				strncmp(c + n - np, prompt, np) == 0)
			break;
		}
	}
	close(toremfd[1]);
	
	if (rv < 0)
		perror("read from host");

	while (((rv = waitid(P_PID, rpid, &sip, WEXITED)) < 0) && errno == EINTR)
		;
	if (rv < 0) {
		perror("runrlogin waitid?");
	}
	if (verbose)
		printf("pid %d finished with rlogin session %d\n",
			getpid(), rpid);
	exit(3);
bad:
	kill(rpid, SIGKILL);
	while (((rv = waitid(P_PID, rpid, &sip, WEXITED)) < 0) && errno == EINTR)
		;
	exit(-1);
}

void
lcatcher(int signo)
{
	/* for rlogin process */
	/*kill(rpid, SIGHUP);*/
	exit(2);
}

char *
pickuid(int nuids, char *pref)
{
	int uid;
	static char uidbuf[20];

	if (nuids == 1) {
		strcpy(uidbuf, pref);
	} else {
		uid = rand() % nuids;
		sprintf(uidbuf, "%s%d", pref, uid);
	}
	return uidbuf;
}

pid_t
spawnone(char *uid, char *machine, struct cmds *cmd)
{
	pid_t pid;
	char **cargs;

	if ((pid = fork()) != 0)
		/* parent or error */
		return pid;
	
	/* child */
	if (verbose) {
		printf("pid %d running %s as %s\n", getpid(), cmd->cmd, uid);
		if (cmd->args[0]) {
			printf("\tArgs:");
			for (cargs = &cmd->args[0]; *cargs; cargs++)
				printf("<%s> ", *cargs);
			printf("\n");
		}
	}
	if (dologins)
		runrlogin(cmd->cmd, uid, machine, cmd->args);
	else
		runrsh(cmd->cmd, uid, machine, cmd->args);
}
