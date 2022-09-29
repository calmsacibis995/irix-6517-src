/*
 * time:
 *
 * Time a process' execution.
 */
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <wait.h>
#include <sys/types.h>
#include <sys/resource.h>


/*
 * Built in format declarations.
 */
static char *shortfmt =
    "\n"
    "real %E\n"
    "user %U\n"
    "sys  %S\n";

static char *longfmt =
    "\n"
    "elapsed time ........................... %E\n"
    "    user CPU ........................... %U\n"
    "    system CPU ......................... %S\n"
    "    percent busy ....................... %P%%\n"
    "page faults ............................ %V\n"
    "    page reclaims ...................... %R\n"
    "    page ins ........................... %F\n"
    "context switches and swaps ............. %C\n"
    "    voluntary context switches ......... %w\n"
    "    involuntary context switches ....... %c\n"
    "    swaps .............................. %W\n"
    "Number of I/O operations ............... %?\n"
    "    block input operations ............. %I\n"
    "    block output operations ............ %O\n"
    "signals received ....................... %k\n";


/*
 *	Argument declarations
 *	=====================
 */
static char *myname;			/* name we were executed under */
static char *format;			/* resource usage report format */
static char **command;			/* command to time */
static pid_t pid;			/* PID of process to time */


/*
 *	Local routine declarations
 *	==========================
 */
void main(int argc, char **argv);
static void parseArguments(int argc, char **argv);
static void printUsage(FILE *fp, const char *format,
		       double dt, const struct rusage *ru);

static __inline double
timeval(const struct timeval *t)
{
    return t->tv_sec + t->tv_usec/1.0E6;
}

static __inline double
timespec(const struct timespec *t)
{
    return t->tv_sec + t->tv_nsec/1.0E9;
}

static __inline double
timespecdiff(const struct timespec *t0, const struct timespec *tN)
{
    return timespec(tN) - timespec(t0);
}


void
main(int argc, char **argv)
{
    struct rusage rusage;		/* resource usage of children */
    int exitstatus;			/* exit status of child */
    struct timespec t0, tN;		/* start and end time of process */

    parseArguments(argc, argv);

    clock_gettime(CLOCK_REALTIME, &t0);
    pid = fork();
    if (pid < 0) {
	fprintf(stderr, "%s: can't fork: %s\n", myname, strerror(errno));
	exit(1); /* POSIX mandates a value between 1-125 */
    }
    if (pid == 0) {
	execvp(command[0], command);
	fprintf(stderr, "%s: can't exec %s: %s\n",
		myname, command[0], strerror(errno));
	exit(errno == ENOENT ? 127 : 126); /* values mandated by POSIX */
    }

    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    wait3(&exitstatus, 0, &rusage);
    clock_gettime(CLOCK_REALTIME, &tN);
    if (!WIFEXITED(exitstatus))
	fprintf(stderr, "%s: command terminated abnormally\n", myname);

    printUsage(stderr, format, timespecdiff(&t0, &tN), &rusage);
    exit(WEXITSTATUS(exitstatus));
}

static void
parseArguments(int argc, char **argv)
{
    char *s;
    int ch;
    extern char *optarg;
    extern int   optind;

    myname = strrchr(argv[0], '/');
    if (myname != NULL)
	myname++;
    else
	myname = argv[0];

    format = shortfmt;
    if (s = getenv("TIME"))
	format = s;

    /* process command line switches */
    while ((ch = getopt(argc, argv, "f:lp")) != -1)
	switch ((char)ch)
	{
	    default:
	    case '?':
		fprintf(stderr, "%s: unknown option -%c\n", myname, ch);
		fprintf(stderr, "usage: %s [-f format | -l | -p] command args ...\n",
			myname);
		exit(EXIT_FAILURE);
		break;

	    case 'f':
		format = optarg;
		break;

	    case 'l':
		format = longfmt;
		break;

	    case 'p':
		/* note that -p is a POSIX flag and may not be changed */
		format = shortfmt;
		break;
	}

    if (argc - optind <= 0)
	/* nothing to do ... */
	exit(EXIT_SUCCESS);
    command = argv+optind;
}

static void
printUsage(FILE *fp, const char *format,
	   double dt, const struct rusage *ru)
{
    const char *fmt;
    int c, neednl = 1;
    double ut = timeval(&ru->ru_utime);
    double st = timeval(&ru->ru_stime);

    for (fmt = format; *fmt; *fmt ? fmt++ : 0) {
	neednl = 1;
	switch (c = *fmt) {
	    default:
		putc(c, fp);
		neednl = (c != '\n');
		break;

	    case '\\':
		switch (c = *++fmt) {
		    default:
			fprintf(fp, "%s: \\%c?", myname, *fmt ? *fmt : '0');
			break;

		    case '\\':  putc('\\', fp);               break;
		    case 'n':   putc('\n', fp);  neednl = 0;  break;
		    case 'r':   putc('\r', fp);               break;
		    case 't':   putc('\t', fp);               break;

		    case '0':  case '1':  case '2':  case '3':
		    case '4':  case '5':  case '6':  case '7': {
			int n = 3;
			c = 0;
			while (*fmt >= '0' && *fmt <= '7' && n-- > 0)
			    c = 8*c + *fmt++ - '0';
			putc(c, fp);
			neednl = (c != '\n');
			break;
		    }
		}
		break;

	    case '%':
		switch (c = *++fmt) {
		    default:
			fprintf(fp, "%s: %%%c?", myname, *fmt ? *fmt : '0');
			break;

		    case '%':  putc('%', fp);                         break;
		    case 'E':  fprintf(fp, "%.3f", dt);               break;
		    case 'U':  fprintf(fp, "%.3f", ut);               break;
		    case 'S':  fprintf(fp, "%.3f", st);               break;
		    case 'R':  fprintf(fp, "%d",   ru->ru_minflt);    break;
		    case 'F':  fprintf(fp, "%d",   ru->ru_majflt);    break;
		    case 'w':  fprintf(fp, "%d",   ru->ru_nvcsw);     break;
		    case 'c':  fprintf(fp, "%d",   ru->ru_nivcsw);    break;
		    case 'W':  fprintf(fp, "%d",   ru->ru_nswap);     break;
		    case 'I':  fprintf(fp, "%d",   ru->ru_inblock);   break;
		    case 'O':  fprintf(fp, "%d",   ru->ru_oublock);   break;
		    case 'k':  fprintf(fp, "%d",   ru->ru_nsignals);  break;

		    case 'P':
			fprintf(fp, "%.1f%%", 100*(ut+st)/dt);
			break;

		    case 'V':
			fprintf(fp, "%d",   ru->ru_minflt + ru->ru_majflt);
			break;

		    case 'C':
			fprintf(fp, "%d",
				ru->ru_nvcsw + ru->ru_nivcsw + ru->ru_nswap);
			break;

		    case '?':
			fprintf(fp, "%d",   ru->ru_inblock + ru->ru_oublock);
			break;
		}
	}
    }
    if (neednl)
	putc('\n', fp);
}
