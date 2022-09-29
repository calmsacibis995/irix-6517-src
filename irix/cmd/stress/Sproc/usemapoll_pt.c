/**************************************************************************
 *									  *
 * 		 Copyright (C) 1990-1995 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.1 $"

#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <bstring.h>
#include <getopt.h>
#include <wait.h>
#include <signal.h> 
#include <ulocks.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/usync.h>
#include <pthread.h>
#include "stress.h"

int nusers = 8;
int nsemas = 16;
int nacquires = 16;
int debug = 0;
int verbose = 0;

struct ss {
	usema_t *sp;
	int fd;
	dev_t dev;
} *s;

pid_t mpid;
char *filename;
char *Cmd;
usptr_t *handle;

usema_t *sema_alive;

void simple1(void *);

int
main(int argc, char **argv)
{
	int		c;
	int		err = 0;
	int		i;
	struct stat	sb;
	pthread_t	pthread_handle;

	setlinebuf(stdout);
	setlinebuf(stderr);
	Cmd = errinit(argv[0]);
	while ((c = getopt(argc, argv, "vdn:u:s:")) != EOF) {
		switch (c) {
		case 'v':
			verbose++;
			break;
		case 'd':
			debug++;
			break;
		case 'u':
			nusers = atoi(optarg);
			break;
		case 's':
			nsemas = atoi(optarg);
			break;
		case 'n':
			nacquires = atoi(optarg);
			break;
		case '?':
			err++;
			break;
		}
	}
	mpid = get_pid();
	if (err) {
		fprintf(stderr,
			"Usage: %s [-v][-d][-u users][-n acquires][-s semas]\n",
			Cmd);
		exit(1);
	}
	if (debug) {
		usconfig(CONF_LOCKTYPE, US_DEBUGPLUS);
	}

	printf("%s: # users %d # semas %d # acquires %d\n",
		Cmd, nusers, nsemas, nacquires);
	if (usconfig(CONF_INITUSERS, nusers) < 0) {
		errprintf(1, "INITUSERS");
		/* NOTREACHED */
	}
	usconfig(CONF_ARENATYPE, US_SHAREDONLY);
	usconfig(CONF_INITSIZE, 512*1024);

	filename = tempnam(NULL, "usemapoll");
	if ((handle = usinit(filename)) == NULL) {
		errprintf(1, "master usinit");
		/* NOTREACHED */
	}
	s = malloc(nsemas * sizeof(*s));

	/*
	 * allocate semaphores
	 */

	if ((sema_alive = usnewsema(handle, 1)) == NULL) {
		errprintf(1, "newsema (sema_alive) failed\n");
		/* NOTREACHED */
	}

	if (debug)
		usconfig(CONF_HISTON, handle);

	for (i = 0; i < nsemas; i++) {
		if ((s[i].sp = usnewpollsema(handle, 1)) == NULL) {
			errprintf(1, "usnewpollsema failed i:%d", i);
			/* NOTREACHED */
		}
		if ((s[i].fd = usopenpollsema(s[i].sp, 0644)) < 0) {
			errprintf(1, "usopenpollsema failed i:%d", i);
			/* NOTREACHED */
		}
		if (fstat(s[i].fd, &sb) != 0) {
			errprintf(1, "stat on fd %d", s[i].fd);
			/* NOTREACHED */
		}
		s[i].dev = sb.st_rdev;
		if (debug) {
			usctlsema(s[i].sp, CS_DEBUGON);
		}
	}

	prctl(PR_SETSTACKSIZE, 64*1024);

	/*
	 * Spawn all user threads
	 */
	for (i = 0; i < nusers-1; i++) {
		if (pthread_create(&pthread_handle,
				   NULL,
				   (void *) simple1,
				   NULL)) {
			errprintf(3,
				  "usemapoll_pt: pthread_create failed i:%d",
				  i);
			exit(-1);
		}
	}
	simple1(0);

	/*
	 * Wait for children to complete
	 */
	while (1) {
		uspsema(sema_alive);
		if (nusers == 0)
			break;
		usvsema(sema_alive);
		sleep(1);
	}

	return 0;
}

/* ARGSUSED */
void
simple1(void *a)
{
	register int i, j;
	register int gotton;
	fd_set lfd, tfd;
	register int rv;
	char buf[5120], sfd[20];
	pthread_t me;

	me = pthread_self();

	if ((handle = usinit(filename)) == NULL) {
		errprintf(1, "%d slave usinit", me);
		/* NOTREACHED */
	}
	for (j = 0; j < nacquires; j++) {
		FD_ZERO(&lfd);
		gotton = 0;
		for (i = 0; i < nsemas; i++) {
			if ((rv = uspsema(s[i].sp)) > 0) {
				/* got it */
				gotton++;
				sginap(1);
				if (usvsema(s[i].sp) < 0) {
					errprintf(1, "%d slave usvsema", me);
					/* NOTREACHED */
				}
			} else if (rv < 0) {
				errprintf(1, "%d slave uspsema", me);
				/* NOTREACHED */
			} else {
				FD_SET(s[i].fd, &lfd);
			}
		}

		/* now wait for any I didn't get */
		while (gotton != nsemas) {
			if (verbose) {
				sprintf(buf, "%s:%d pass %d selecting on %d semas (",
						Cmd,
						me, j+1,
						nsemas - gotton);
				for (i = 0; i < nsemas; i++)
					if (FD_ISSET(s[i].fd, &lfd)) {
						sprintf(sfd, "(%d-%d)",
							s[i].fd, minor(s[i].dev));
						strcat(buf, sfd);
					}
				strcat(buf, ")\n");
				write(1, buf, strlen(buf));
			}
			tfd = lfd;
			rv = select(FD_SETSIZE, &tfd, NULL, NULL, NULL);
			if (rv < 0) {
				errprintf(1, "%d select", me);
				/* NOTREACHED */
			}
			if (verbose)
				sprintf(buf, "%s:%d pass %d got (",
							Cmd, me, j+1);
			for (i = 0; i < nsemas; i++) {
				if (FD_ISSET(s[i].fd, &tfd)) {
					gotton++;
					FD_CLR(s[i].fd, &lfd);
					if (verbose) {
						sprintf(sfd, "%d ", s[i].fd);
						strcat(buf, sfd);
					}
					sginap(1);
					if (usvsema(s[i].sp) < 0) {
						errprintf(1, "%d slave usvsema",
								me);
						/* NOTREACHED */
					}
				}
			}
			if (verbose) {
				strcat(buf, ")\n");
				write(1, buf, strlen(buf));
			}
		}

		if (verbose)
			printf("%s:%d pass:%d\n", Cmd, me, j+1);
	}
	printf("%s:%d complete\n", Cmd, me);

	uspsema(sema_alive);
	nusers--;
	usvsema(sema_alive);
}
