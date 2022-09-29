/* unixperf - an x11perf-style Unix performance benchmark */

/* mp.c contains code to run multiple copies of unixperf in parallel */

#include "unixperf.h"

#include <string.h>
#include <signal.h>
#include <dirent.h>
#include <ctype.h>
#include <stdarg.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/sysmp.h>

static int
FindNextMustrun(uint64_t mask[], int cpu)
{
	if (cpu >= MAXCPU)
		cpu = 0;

	for (;;) {
		if (mask[cpu/64] & (1ULL << (cpu & 0x3f)))
			break;
		if (++cpu >= MAXCPU)
			cpu = 0;
	}

	return cpu;
}

static void
ChildStatusError(int pid, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);

	printf("*** Error: child (pid %d) ", pid);
	vprintf(fmt, ap);
	printf("\n*** child may not have run test; "
		"results may be suspect.\n\n");

	va_end(ap);
}

static void
MpWaitForChildren()
{
	int rc;
	int pp;
	int status;

	for (pp = 0; pp < MpNum; pp++) {
		if (MpChildren[pp] == 0)
			continue;

		rc = waitpid(MpChildren[pp], &status, 0);
		SYSERROR(rc, "waitpid");

		if (WIFEXITED(status) &&
			WEXITSTATUS(status) != EXIT_SUCCESS) {
			ChildStatusError(MpChildren[pp],
				"exited with error %d",
				WEXITSTATUS(status));
		}
		else if (WIFSTOPPED(status)) {
			ChildStatusError(MpChildren[pp],
				"stopped by signal %d",
				WSTOPSIG(status));
		}
		else if (WIFSIGNALED(status)) {
			ChildStatusError(MpChildren[pp],
				"killed by signal %d",
				WTERMSIG(status));
		}

		MpChildren[pp] = 0;
	}
}

int
MpBeginTest()
{
	int pp;
	int curproc = 0;
	int rc;
	int nullfd;

	memset(MpChildren, 0, MpNum * sizeof(pid_t));

	nullfd = open("/dev/null", O_RDWR);
	SYSERROR(nullfd, "open(/dev/null)");

	for (pp = 0; pp < MpNum; pp++) {
		pid_t child_pid;

		if (MpFlags & U_MP_MUSTRUN)
			curproc = FindNextMustrun(MpMustrunMask, ++curproc);

		fflush(NULL);

		child_pid = fork();
		SYSERROR(child_pid, "fork");

		if (child_pid == 0) {
			if (MpFlags & U_MP_MUSTRUN) {
				rc = sysmp(MP_MUSTRUN, curproc);
				SYSERROR(rc, "mustrun");
			}

			if (pp != 0) {
				dup2(nullfd, 0);
				dup2(nullfd, 1);
			}

			return 0;
		}

		MpChildren[pp] = child_pid;
	}

	MpWaitForChildren();
	return 1;
}

void
MpEndTest(int nproblems)
{
	if (nproblems == 0)
		exit(EXIT_SUCCESS);
	else
		exit(EXIT_FAILURE);
}

static void
ProcStrError(const char *str, const char *where, const char *message)
{
	const char *preamble = "Error: invalid processor set: ";
	int len;

	printf("%s%s\n", preamble, str);

	len = strlen(preamble) + 1;
	while (--len)
		putchar(' ');

 	len = (int) (where - str) + 1;
	while (--len)
		putchar('-');
	printf("^\n");

	printf("                    %s\n\n", message);
}

int
MpParseProcStr(uint64_t mask[], const char *str)
{
#define proc_syntax(msg) { ProcStrError(str, cp, msg); return 0; }

	const char *cp = str;
	int ncpus;

	ncpus = sysmp(MP_NPROCS);
	if (ncpus < 0) {
		printf("unable to get the correct number of processors\n");
		exit(EXIT_FAILURE);
	}

	while (*cp) {
		int m, n;

		if (!isdigit(*cp))
			proc_syntax("expecting CPU number");

		m = 0;
		do {
			m = 10*m + (*cp-'0');
		} while (isdigit(*++cp));

		if (*cp == '-') {
			cp++;
			if (isdigit(*cp)) {
				n = 0;
				do {
					n = 10*n + (*cp-'0');
				} while (isdigit(*++cp));
			} else if (*cp == '\0') {           /* e.g. 2- */
				n = ncpus - 1;
			} else
				proc_syntax("expecting CPU number");
		} else if (*cp == ',') {
			cp++;
			n = m;
		} else if (*cp == '\0') {
			n = m;
		} else
			proc_syntax("expecting ',' or '-' after CPU number");

		if (m > n) {
			printf("inverted CPU range, %d > %d\n", m, n);
			return 0;
		}

		if (m >= ncpus) {
			printf("%d: CPU number out of range; "
				"localhost has only %d processors\n",
				m, ncpus);
			return 0;
		}

		for (; m <= n; m++)
			mask[m>>6] |= ((uint64_t) 1)<<(m&0x3f);
	}

	return 1;

#undef proc_syntax
}
