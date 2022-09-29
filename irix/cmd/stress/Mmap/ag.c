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
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "stress.h"

#define NL 164
#define BPL 64

char	*pattern = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
char	buf[NL*BPL];
char	*Cmd;
static void segcatch(int sig, int code, struct sigcontext *sc);
int delay;

int
main(int argc, char **argv)
{
	int verbose = 0, c;
	char *m1, *m2;
	int fd, i;
	FILE *f;
	struct stat statbuf;
	char *filename;

	Cmd = errinit(argv[0]);
	while ((c = getopt(argc, argv, "vp:")) != EOF) {
		switch(c) {
		case 'v':
			verbose = 1;
			break;
		case 'p':
			delay = atoi(optarg);
			break;
		default:
			fprintf(stderr, "Usage:%s [-v]\n", Cmd);
			exit(1);
		}
	}
	filename = tempnam(NULL, "agYY");
	sigset(SIGSEGV, segcatch);

	f = fopen(filename, "w+");
	if (f == NULL) {
		errprintf(ERR_EXIT, "fopen");
		/* NOTREACHED */
	}

	/*
	 * Create a file of 64 bytes
	 */
	if (fwrite(pattern, 1, BPL, f) != BPL) {
		errprintf(ERR_RET, "fwrite");
		goto bad;
	}
	fclose(f);

	if (chmod(filename, 0777)) {
		errprintf(ERR_ERRNO_RET, "chmod");
		goto bad;
	}

	fd = open(filename, O_RDWR);
	if (fd < 0) {
		errprintf(ERR_ERRNO_RET, "open");
		goto bad;
	}
	if (unlink(filename)) {
		errprintf(ERR_ERRNO_RET, "unlink");
		goto bad;
	}

	m1 = mmap(0, NL*BPL, PROT_READ|PROT_WRITE,
		MAP_SHARED|MAP_AUTOGROW, fd, 0);
	if (m1 == (caddr_t) -1L) {
		errprintf(ERR_ERRNO_EXIT, "mmap");
		/* NOTREACHED */
	} else if (verbose)
		printf("%s:map returned 0x%x\n", Cmd, m1);

	for (i = BPL*2; i < NL*BPL; i++)
		m1[i] = i & 0xff;

	if (delay)
		sleep(delay);

	m2 = mmap(m1, NL*BPL, PROT_READ|PROT_WRITE, MAP_FIXED|MAP_SHARED, fd,0);
	if (m2 == (caddr_t) -1L) {
		errprintf(ERR_ERRNO_EXIT, "map fixed (second mapping)");
		/* NOTREACHED */
	}
	if (lseek(fd, 0, 0)) {
		errprintf(ERR_ERRNO_EXIT, "lseek");
		/* NOTREACHED */
	}
	i = read(fd, buf, NL*BPL);
	if (i != NL*BPL) {
		errprintf(ERR_RET, "read returned %d chars, not %d\n",
						i, NL*BPL);
		abort();
	}
	for (i = 0; i < BPL; i++) {
		if (buf[i] != 'X') {
			errprintf(ERR_EXIT, "overwrite valid data (read)!\n");
			/* NOTREACHED */
		}
		if (m2[i] != 'X') {
			errprintf(ERR_EXIT, "overwrite valid data (mapped)!\n");
			/* NOTREACHED */
		}
	}
	for ( ; i < BPL*2; i++) {
		if (buf[i] != '\0') {
			errprintf(ERR_EXIT, "gap (%d) was %d, not 0 (read)!\n",
					i, buf[i]);
			/* NOTREACHED */
		}
		if (m2[i] != '\0') {
			errprintf(ERR_EXIT, "gap (%d) was %d, not 0 (mapped)!\n",
					i, buf[i]);
			/* NOTREACHED */
		}
	}
	for ( ; i < NL*BPL; i++) {
		if ((buf[i] != (i & 0xff)) || m2[i] != (i & 0xff)) {
			errprintf(ERR_RET, "did not find mapped data\n");
			errprintf(ERR_EXIT, "buf[%d] == %d, m2[%d] == %d not %d\n",
					i, buf[i], i, m2[i], i & 0xff);
			/* NOTREACHED */
		}
	}

	printf("%s:%d:PASSED -- remapped at 0x%p\n", Cmd, getpid(), m2);
	exit(0);
bad:
	unlink(filename);
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
