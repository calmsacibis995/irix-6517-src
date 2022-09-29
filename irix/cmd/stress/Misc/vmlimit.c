#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <getopt.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <ulimit.h>
#include "stress.h"

/*
 * simple limits checking
 */

char *Cmd;
extern int _fdata[];

int
main(int argc, char **argv)
{
	int c;
	struct rlimit64 rldata, rlvmem, rlrss, rlstack, rltmp;
	long brkmax;
	int pagesz;
	long database;
	pid_t pid;
	char *sargv[3];
	int status;

	Cmd = errinit(argv[0]);
	pagesz = getpagesize();
	/* XXX round _fdata down to page size */
	database = (long)_fdata / pagesz;
	database *= pagesz;

	while ((c = getopt(argc, argv, "T")) != -1)
		switch (c) {
		case 'T':
			/* should never get here */
			abort();
			/* NOTREACHED */
		default:
			break;
		}

	getrlimit64(RLIMIT_DATA, &rldata);
	getrlimit64(RLIMIT_STACK, &rlstack);
	getrlimit64(RLIMIT_VMEM, &rlvmem);
	getrlimit64(RLIMIT_RSS, &rlrss);
	brkmax = ulimit(UL_GMEMLIM);

	printf("%s:datacur %lld (%lldMb) stkcur %lld (%lldMb) vmemcur %lld (%lldMb)\n",
		Cmd,
		rldata.rlim_cur, rldata.rlim_cur / (1024*1024LL),
		rlstack.rlim_cur, rlstack.rlim_cur / (1024*1024LL),
		rlvmem.rlim_cur, rlvmem.rlim_cur / (1024*1024LL));

	printf("%s:rsscur %lld (%lldMb) brkmax 0x%lx brk val %p\n",
		Cmd,
		rlrss.rlim_cur, rlrss.rlim_cur / (1024*1024LL),
		brkmax,
		sbrk(0));

	/* set our max data size down to 128K */
	rltmp = rldata;
	rltmp.rlim_cur = 128*1024;

	if (setrlimit64(RLIMIT_DATA, &rltmp) != 0) {
		errprintf(ERR_ERRNO_EXIT, "setrlimit data failed\n");
		/* NOTREACHED */
	}

	/* malloc should fail */
	if (malloc(128*1024) != NULL) {
		errprintf(ERR_ERRNO_EXIT, "malloc succeded\n");
		/* NOTREACHED */
	}

	/* check brkmax */
	brkmax = ulimit(UL_GMEMLIM);
	if (brkmax != database + (128*1024)) {
		errprintf(ERR_EXIT, "brkmax 0x%x _fdata 0x%x database 0x%x\n",
			brkmax, _fdata, database);
		/* NOTREACHED */
	}

	if (setrlimit64(RLIMIT_DATA, &rldata) != 0) {
		errprintf(ERR_ERRNO_EXIT, "setrlimit data-back failed\n");
		/* NOTREACHED */
	}

	/* malloc should work */
	if (malloc(128*1024) == NULL) {
		errprintf(ERR_ERRNO_EXIT, "malloc failed\n");
		/* NOTREACHED */
	}

	/* check at least a bit of exec limit checking */
	if ((pid = fork()) == 0) {
		/* child */
		rltmp = rlvmem;
		rltmp.rlim_cur = 8*1024;

		if (setrlimit64(RLIMIT_VMEM, &rltmp) != 0) {
			errprintf(ERR_ERRNO_EXIT, "setrlimit vmem failed\n");
			/* NOTREACHED */
		}

		sargv[0] = argv[0];
		sargv[1] = "-T";
		sargv[2] = NULL;
		execvp(argv[0], sargv);
		errprintf(ERR_ERRNO_EXIT, "exec failed");
		/* NOTREACHED */
	} else if (pid < 0) {
		printf("%s:fork failed:%s\n", Cmd, strerror(errno));
	} else {
		siginfo_t si;
		int error;

		for (;;) {
			error = waitid(P_PID, pid, &si, WEXITED);
			if (error < 0 && errno == ECHILD)
				break;
			else if (error == 0) {
				/*
				 * we should have died with a SIGKILL from
				 * the kernel, with a code of ENOMEM
				 */
				if (si.si_status != SIGKILL ||
						si.si_code != CLD_KILLED) {
					errprintf(ERR_RET, "proc %d died of signal %d\n",
						pid, si.si_status);
					abort();
				}
			}
		}
	}
		
	return 0;
}
