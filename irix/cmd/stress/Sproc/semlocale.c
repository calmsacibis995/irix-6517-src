/**************************************************************************
 *									  *
 * 		 Copyright (C) 1992-1995 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.5 $"

#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdio.h>
#include <signal.h> 
#include <string.h> 
#include <ulocks.h>
#include <getopt.h>
#include <wait.h>
#include <errno.h>
#include <dirent.h>
#include <time.h>
#include <pfmt.h>
#include <locale.h>
#include "stress.h"
#include <sys/types.h>
#include <sys/times.h>

/*
 * semlocale - basic semaphored locale testing
 */

char *Cmd;

char *afile;
usptr_t *handle;
extern void simple1(void), slave(void *);
extern void quitit(int), segcatch(int, int, struct sigcontext *), abrt(int);

extern void doaddsev(void), dogettxt(void), dopfmt(void), dosetcat(void),
		dosetlabel(void), dostrcoll(void),
		dostrftime(void), dostrxfrm(void);
void (*fcns[])(void) = {
	doaddsev,
	dopfmt,
	dosetcat,
	dosetlabel,
	dostrcoll,
	dostrftime,
	dostrxfrm
};
int nfcns = sizeof(fcns) / sizeof (int (*)());
#define MAXMSGS	10

char *cats[] =  {
	"uxsgicore",
	"uxcore.abi"
};
int ncats = sizeof(cats) / sizeof(char *);

char *labels[] = {
	"L1",
	"L2L2"
};
int nlabels = sizeof(labels) / sizeof(char *);

struct sevs_s {
	char *name;
	int val;
} sevs[] = {
	{ "VERYHIGH", 5 },
	{ "VERYVERYHIGH", 6}
};
int nsevs = sizeof(sevs) / sizeof(struct sevs_s);

int debug = 0;
int verbose = 0;
pid_t mpid;
int nloops = 80;
int nusers = 6;
int spinonabort;
int nomsgs = 0;		/* set true if the system has no message catalogues */
usema_t *syncsema;
struct msgs_s {
	char *msg;
	int msgid;
} *msgs;

int
main(int argc, char **argv)
{
	char		*catstr;
	int		c;
	int		err = 0;
	int		id, i, j;

	Cmd = errinit(argv[0]);
	while ((c = getopt(argc, argv, "al:vdu:")) != EOF) {
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
		case '?':
			err++;
			break;
		}
	}
	if (err) {
		fprintf(stderr, "Usage: %s [-vda][-l loops][-u users]\n", Cmd);
		exit(1);
	}

	mpid = get_pid();
	sigset(SIGSEGV, segcatch);
	sigset(SIGBUS, segcatch);
	sigset(SIGINT, quitit);
	parentexinit(0);

	if (spinonabort)
		sigset(SIGABRT, abrt);
	if (debug)
		usconfig(CONF_LOCKTYPE, US_DEBUGPLUS);

	printf("%s:%d:# users %d\n", Cmd, getpid(), nusers);
	if (usconfig(CONF_INITUSERS, nusers) < 0) {
		errprintf(ERR_ERRNO_EXIT, "INITUSERS");
		/* NOTREACHED */
	}
	usconfig(CONF_ARENATYPE, US_SHAREDONLY);

	afile = tempnam(NULL, "semlocale");
	if ((handle = usinit(afile)) == NULL)
		errprintf(ERR_ERRNO_EXIT, "usinit of %s failed", afile);
		/* NOTREACHED */

	if ((syncsema = usnewsema(handle, 0)) == NULL) {
		errprintf(ERR_ERRNO_EXIT, "usnewsema 'syncsema' failed");
		/* NOTREACHED */
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
				exit(1);
			}
		}
	}

	/*
	 * do setup - we want to do this after sproc so even the 
	 * setup functions as least try to lock
	 */
	setlocale(LC_MESSAGES, "C");
	if (strcmp(setlocale(LC_MESSAGES, NULL), "C") != 0) {
		errprintf(ERR_EXIT, "setlocale MESSAGES doesn't equal C");
		/* NOTREACHED */
	}
	setlocale(LC_MONETARY, "COO");
	setlocale(LC_ALL, "C");
	msgs = malloc(MAXMSGS * sizeof(struct msgs_s *) * ncats);
	
	for (i = 0, id = 0; i < MAXMSGS && !nomsgs; i++) {
		/*
		 * Find ids such that the same id in each catalogue
		 * points to a message w/o any '%' in it
		 */
		int foundnewid = 0;
		while (!foundnewid && !nomsgs) {
			++id;
			for (j = 0; j < ncats; j++) {
				char msgid[32];
				sprintf(msgid, "%s:%d", cats[j], id);
				catstr = gettxt(msgid, "Nada");
				if (strcmp(catstr, "Nada") == 0) {
					/* no message catalogues */
					nomsgs++;
					break;
				}
				if (strchr(catstr, '%') != NULL)
					break;
			}
			if (j >= ncats)
				foundnewid++;
		}
		for (j = 0; j < ncats && !nomsgs; j++) {
			char msgid[32];
			char *msg;
			sprintf(msgid, "%s:%d", cats[j], id);
			msg = gettxt(msgid, "Nada");
			msgs[i + (j * MAXMSGS)].msg = strdup(msg);
			msgs[i + (j * MAXMSGS)].msgid = id;
		}
	}
	setlabel(labels[0]);
	for (i = 0; i < nsevs; i++)
		addsev(sevs[i].val, sevs[i].name);

	/* let everyone go */
	for (i = 0; i < nusers; i++)
		usvsema(syncsema);
	simple1();

	while (wait(0) >= 0 || errno == EINTR)
		;
	printf("%s:%d:PASSED\n", Cmd, getpid());
	return 0;
err:
	abort();
	/* NOTREACHED */
}

/* ARGSUSED */
void
slave(void *arg)
{
	slaveexinit();
	simple1();
}

void
simple1(void)
{
	register int i;

	if ((handle = usinit(afile)) == NULL) {
		errprintf(ERR_ERRNO_EXIT, "%d slave usinit", get_pid());
		/* NOTREACHED */
	}

	/* wait */
	uspsema(syncsema);

	for (i = 0; i < nloops; i++) {
		/* do a function */
		fcns[rand() % nfcns]();
	}
}

void
dopfmt(void)
{
	char *pfile;
	FILE *fpfile;
	char buf[512];
	int i, sev, found;
	char *p, *s;

	if (nomsgs)
		return;
	/* set up tmp file to handle pfmt traffic */
	pfile = tempnam(NULL, "seml1");
	if ((fpfile = fopen(pfile, "w+")) == NULL) {
		errprintf(ERR_EXIT, "can't open %s", pfile);
		/* NOTREACHED */
	}
	unlink(pfile);

	sprintf(buf, "%s:%d:Nothing", cats[rand() % ncats],
				msgs[rand() % MAXMSGS].msgid);
	sev = sevs[rand() % nsevs].val;
	pfmt(fpfile, MM_GET|sev, buf);
	fflush(fpfile);

	/* read back and check */
	memset(buf, '\0', sizeof(buf));
	rewind(fpfile);
	if (fread(buf, 1, sizeof(buf), fpfile) == 0) {
		errprintf(ERR_EXIT, "error reading back pfmt file");
		/* NOTREACHED */
	}

	fclose(fpfile);

	/*
	 * see if matches any saved message
	 * Msgs have 3 parts - label, severity, msg
	 */
	s = buf;
	if ((p = strchr(s, ':')) == NULL) {
		errprintf(ERR_EXIT, "Msg missing first ':' <%s>", s);
		/* NOTREACHED */
	}
	*p = '\0';
	for (i = 0, found = 0; i < nlabels; i++) {
		if (strcmp(s, labels[i]) == 0) {
			found++;
			break;
		}
	}
	if (!found) {
		errprintf(ERR_EXIT, "Never found label <%s>", s);
		/* NOTREACHED */
	}

	/* now look for severity */
	s = p + 2;
	if ((p = strchr(s, ':')) == NULL) {
		errprintf(ERR_EXIT, "Msg missing second ':' <%s>", s);
		/* NOTREACHED */
	}
	*p = '\0';
	for (i = 0, found = 0; i < nsevs; i++) {
		if (strcmp(s, sevs[i].name) == 0) {
			found++;
			break;
		}
	}
	if (!found) {
		errprintf(ERR_EXIT, "Never found severity <%s>", s);
		/* NOTREACHED */
	}
	
	/* finally look at message - skip the ' ' pfmt adds */
	s = p + 2;
	for (i = 0, found = 0; i < MAXMSGS * ncats; i++) {
		if (strcmp(s, msgs[i].msg) == 0) {
			found++;
			break;
		}
	}
	if (!found) {
		errprintf(ERR_EXIT, "Never found msg <%s>", s);
		/* NOTREACHED */
	}
}

void
doaddsev(void)
{
	int i = rand() % nsevs;
	addsev(sevs[i].val, sevs[i].name);
}

void
dosetcat(void)
{
	int i = rand() % ncats;
	setcat(cats[i]);
}

void
dosetlabel(void)
{
	int i = rand() % nlabels;
	setlabel(labels[i]);
}

void
dostrcoll(void)
{
	strcoll("sdfjhkdfshjlkfdsa", "fjhkfdsjhkfdsfds");
}

void
dostrftime(void)
{
	char buf[512];
	struct tms t;
	clock_t c;

	c = times(&t);
	strftime(buf, sizeof(buf), "%A%B%p%X%Y%Z", localtime(&c));
}

void
dostrxfrm(void)
{
	char buf[128];
	strcoll(buf, "fjhkfdsjhkfdsfds");
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

/* ARGSUSED */
void
quitit(int sig)
{
	exit(0);
}

/* ARGSUSED */
void
abrt(int sig)
{
	write(2, "spin in abrt catcher\n",  21);
	for (;;) ;
}
