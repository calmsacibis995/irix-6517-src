/**************************************************************************
 *									  *
 * 		 Copyright (C) 1992-1992 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.11 $"

#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdio.h>
#include <signal.h> 
#include <getopt.h> 
#include <string.h> 
#include <ulocks.h>
#include <wait.h>
#include <errno.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "stress.h"

/*
 * semstdio - basic semaphored stdio testing
 * Basically works over fopen//flush/fclose and dir routines
 */

char *Cmd;

char *afile;
usptr_t *handle;
void slave(void *), simple1(void), closeem(void);
extern void quitit(), abrt();
extern void rmit(char *dir);
extern void segcatch(int sig, int code, struct sigcontext *sc);

int debug = 0;
int verbose = 0;
int mpid;
int nusers = 8;
int nfiles = 140;
int nloops = 80;
int spinonabort;
DIR *dp;
struct files {
	char *name;	/* basename of file */
	FILE *f;	/* open file handle */
	int visited;	/* # times visited */
	int access;	/* # of folks accessing */
	ulock_t olock;	/* spin lock */
} *files;
char *dir = NULL;

int
main(int argc, char **argv)
{
	int		c;
	int		err = 0;
	int		i;
	FILE *f;
	char *tf;
	char dirtemps[PATH_MAX];

	Cmd = errinit(argv[0]);
	while ((c = getopt(argc, argv, "al:vdf:u:")) != EOF) {
		switch (c) {
		case 'a':
			spinonabort++;
			break;
		case 'v':
			verbose++;
			break;
		case 'd':
			debug++;
			break;
		case 'u':
			nusers = atoi(optarg);
			break;
		case 'l':
			nloops = atoi(optarg);
			break;
		case 'f':
			nfiles = atoi(optarg);
			break;
		case '?':
			err++;
			break;
		}
	}
	if (err) {
		fprintf(stderr, "Usage: %s [-v][-d][-l loops][-u users][-f nfiles]\n", Cmd);
		exit(1);
	}

	/* This is really to protect 032 which has a max of 255.
	 */
#if (_MIPS_SIM == _MIPS_SIM_ABI64)
#define FD_MAX  INT_MAX
#elif (_MIPS_SIM == _MIPS_SIM_NABI32)
#define FD_MAX  USHRT_MAX
#else
#define FD_MAX  UCHAR_MAX
#endif
	if (nfiles + 3 > FD_MAX) {
		fprintf(stderr, "nfiles too large [%d]\n", FD_MAX - 3);
		exit(1);
	}
	mpid = get_pid();
	sigset(SIGSEGV, segcatch);
	sigset(SIGBUS, segcatch);
	if (debug)
		sigset(SIGINT, quitit);
	parentexinit(0);

	if (spinonabort)
		sigset(SIGABRT, abrt);
	if (debug)
		usconfig(CONF_LOCKTYPE, US_DEBUGPLUS);

	printf("%s:%d:# users %d # files %d\n",
		Cmd, getpid(), nusers, nfiles);
	if (usconfig(CONF_INITUSERS, nusers) < 0) {
		errprintf(1, "INITUSERS");
		/* NOTREACHED */
	}
	usconfig(CONF_ARENATYPE, US_SHAREDONLY);

	afile = tempnam(NULL, "semlibc");
	if ((handle = usinit(afile)) == NULL)
		errprintf(1, "usinit of %s failed", afile);
		/* NOTREACHED */

	/*
	 * set up temp dir for roaming around in
	 */
	dir = tempnam(NULL, "semlibc");
	if (mkdir(dir, 0777) != 0)
		errprintf(1, "cannot create directory %s", dir);
		/* NOTREACHED */
	if ((dp = opendir(dir)) == NULL)
		errprintf(1, "cannot opendir directory %s", dir);
		/* NOTREACHED */

	files = calloc(nfiles, sizeof(struct files));
	for (i = 0; i < nfiles; i++) {
		sprintf(dirtemps, "%s/%d", dir, i);
		tf = dirtemps;
		files[i].name = strdup(tf);
		if ((f = fopen(tf, "w+")) == NULL) {
			errprintf(3, "cannot create file %s", tf);
			goto err;
		}
		fwrite("Hello", 10, 1, f);
		fclose(f);
		if ((files[i].olock = usnewlock(handle)) == NULL) {
			errprintf(3, "cannot alloc lock");
			goto err;
		}
	}

	prctl(PR_SETSTACKSIZE, 64*1024);
	/* start up all users */
	for (i = 0; i < nusers-1; i++) {
		if (sproc(slave, PR_SALL, 0) < 0) {
			if (errno != EAGAIN) {
				errprintf(ERR_ERRNO_RET, "sproc failed i:%d", i);
				goto err;
			} else {
				fprintf(stderr, "%s:can't sproc:%s\n",
					Cmd, strerror(errno));
				rmit(dir);
				exit(1);
			}
		}
	}
	simple1();

	while (wait(0) >= 0 || errno == EINTR)
		;
	if (verbose) {
		for (i = 0; i < nfiles; i++) {
			printf("File %s visited %d times\n",
				files[i].name, files[i].visited);
		}
	}
	rmit(dir);
	printf("%s:%d:PASSED\n", Cmd, getpid());
	return 0;
err:
	rmit(dir);
	abort();
	/* NOTREACHED */
}

/* ARGSUSED */
void
slave(void *arg)
{
	sigset(SIGINT, SIG_DFL);
	slaveexinit();
	simple1();
}

void
simple1(void)
{
	register int found, i, j;
	struct dirent *res, *dent;
	int randdirent;

	dent = malloc(sizeof(struct dirent) + NAME_MAX + 1);
	if ((handle = usinit(afile)) == NULL) {
		errprintf(ERR_ERRNO_EXIT, "%d slave usinit", get_pid());
		/* NOTREACHED */
	}

	for (j = 0; j < nloops; j++) {
		randdirent = rand() % (nfiles >> (nfiles > 16 ? 4 : 0));
		do {
			while (readdir_r(dp, dent, &res) || res == NULL) {
				/* error or EOF ... */
				/* XXX single thread?? */
				rewinddir(dp);
			}
		} while (--randdirent > 0);
		if (strcmp(dent->d_name, ".") == 0 ||
		    strcmp(dent->d_name, "..") == 0)
			continue;

		/* find name in list */
		for (i = 0, found = 0; i < nfiles; i++) {
			if (strcmp(strrchr(files[i].name, '/') + 1,
							dent->d_name) == 0) {
				found++;
				break;
			}
		}
		if (!found) {
			errprintf(0, "%d name %s not found in dir!",
				get_pid(), dent->d_name);
			/* NOTREACHED */
		}

again:
		/* grab olock */
		if (ussetlock(files[i].olock) != 1) {
			errprintf(1, "%d ussetlock olock failed",
				get_pid());
			/* NOTREACHED */
		}
		if (!files[i].f) {
			if ((files[i].f = fopen(files[i].name, "w+")) == NULL) {
				/* couldn't open file - perhaps cause
				 * we got so many open??
				 */
				if (errno == EMFILE) {
					usunsetlock(files[i].olock);
					closeem();
					goto again;
				} else
					errprintf(1, "%d fopen on %s failed",
						get_pid(), files[i].name);
					/* NOTREACHED */
			}
		} else {
			FILE *tf;
			char tmp[256];

			strcpy(tmp, dir);
			strcat(tmp, "/tmp");
			if ((tf = fopen("tmp", "w+")) != NULL) {

				/* Close fp for slot we are about to use.
				 */
				fclose(files[i].f);
				if ((files[i].f = freopen(files[i].name, "w+", tf)) == NULL) {
					if (errno == EMFILE) {
						usunsetlock(files[i].olock);
						closeem();
						goto again;
					} else
						errprintf(1, "%d freopen on %s failed",
							get_pid(), files[i].name);
						/* NOTREACHED */
				}
			}
		}
		files[i].access++;
		files[i].visited++;
		usunsetlock(files[i].olock);

		/* now play with it */
		if (fwrite("Hello", 10, 1, files[i].f) != 1)
			errprintf(0, "%d fwrite failed", get_pid());
			/* NOTREACHED */
		fflush(files[i].f);

		/* un-access lock */
		if (ussetlock(files[i].olock) != 1) {
			errprintf(1, "%d ussetlock olock failed",
				get_pid());
			/* NOTREACHED */
		}
		if (files[i].access <= 0) {
			errprintf(0, "%d access cnt is %d",
				get_pid(), files[i].access);
			/* NOTREACHED */
		}
		files[i].access--;
		usunsetlock(files[i].olock);
		if (verbose > 1)
			printf("%s:%d pass:%d file %d %s\n", Cmd, get_pid(),
					j+1, i, files[i].name);
	}
}

void
segcatch(int sig, int code, struct sigcontext *sc)
{
	fprintf(stderr, "%s:%d:ERROR: caught signal %d at pc 0x%llx code %d badvaddr %llx\n",
			Cmd, getpid(), sig, sc->sc_pc, code,
			sc->sc_badvaddr);
	abort();
	/* NOTREACHED */
}

void
quitit()
{
	rmit(dir);
	exit(0);
}

void
abrt()
{
	write(2, "spin in abrt catcher\n",  21);
	for (;;) ;
}


void
rmit(char *dir)
{
	char cmd[1024];

	strcpy(cmd, "/bin/rm -fr ");
	strcat(cmd, dir);
	system(cmd);
}

void
closeem(void)
{
	int i;
	for (i = 0; i < nfiles; i++) {
		if (ussetlock(files[i].olock) != 1) {
			errprintf(1, "%d ussetlock olock failed",
				get_pid());
			/* NOTREACHED */
		}
		if (files[i].f && files[i].access == 0) {
			fclose(files[i].f);
			files[i].f = NULL;
		}
		usunsetlock(files[i].olock);
	}
}
