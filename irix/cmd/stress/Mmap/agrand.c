/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"$Revision: 1.6 $ $Author: jwag $"
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <getopt.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/types.h>
#include "stress.h"

#define FSZ	0x20000
#define FSTART	0x08000

char	*Cmd;
char	*agfile;
int	agfd;
int	nreps = 25;
int	nbppmask;
int	verbose;
static void segcatch(int sig, int code, struct sigcontext *sc);

int
main(int argc, char **argv)
{
	int c;
	register i;

	nbppmask = getpagesize() - 1;

	Cmd = errinit(argv[0]);
	while ((c = getopt(argc, argv, "l:v")) != EOF) {
		switch (c) {
		case 'v':
			verbose = 1;
			break;
		case 'l':
			nreps = atoi(optarg);
			break;
		default:
			fprintf(stderr, "Usage:%s [-v][-l reps]\n", Cmd);
			exit(1);
		}
	}
	printf("%s:%d: doing %d reps\n", Cmd, getpid(), nreps);

	srand(time(NULL));
	agfile = tempnam(NULL, "Tmagr");
	sigset(SIGSEGV, segcatch);

	for (i = 0; i < nreps; i++) {
		register caddr_t	addr, s;
		int	start = (rand() % FSTART) & ~(nbppmask);
		int	len = FSZ - start;
		int	tlen, plen;
		register	j;

		agfd = open(agfile, O_RDWR | O_CREAT, 0666);
		if (agfd < 0) {
			errprintf(ERR_ERRNO_EXIT, "can't create %s\n",
				agfile);
			/* NOTREACHED */
		}

		if (verbose) {
			printf("map 0x%x bytes from 0x%x..", len, start);
			fflush(stdout);
		}
		s = addr = mmap(0, len, PROT_READ|PROT_WRITE,
				MAP_SHARED|MAP_AUTOGROW, agfd, start);
		if (addr == (caddr_t) -1L) {
			errprintf(ERR_ERRNO_RET, "mmap");
			goto bad;
		}

		plen = rand() % len;

		if (verbose) {
			printf("poke 0x%x..", plen);
			fflush(stdout);
		}

		for (j = 0; j < plen; j++)
			*s++ = 'x';

		tlen = rand() % FSZ;
		if (verbose) {
			printf("trunc to 0x%x..", tlen);
			fflush(stdout);
		}
		if (ftruncate(agfd, tlen)) {
			errprintf(ERR_ERRNO_RET, "ftruncate");
			goto bad;
		}

		if (verbose) {
			printf("umap 0x%x to 0x%x\n", addr, len);
			fflush(stdout);
		}
		if (munmap(addr, len)) {
			errprintf(ERR_ERRNO_RET, "munmap");
			goto bad;
		}

		close(agfd);
	}

	unlink(agfile);
	printf("%s:%d:PASSED\n", Cmd, getpid());
	exit(0);

bad:
	unlink(agfile);
	abort();
}

static void
segcatch(int sig, int code, struct sigcontext *sc)
{
	fprintf(stderr, "%s:%d:ERROR: caught signal %d at pc 0x%llx code %d badvaddr %llx\n",
			Cmd, getpid(), sig, sc->sc_pc, code,
			sc->sc_badvaddr);
	abort();
	/* NOTREACHED */
}
