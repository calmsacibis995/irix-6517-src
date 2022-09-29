#include <stdio.h>
#include <getopt.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/procfs.h>
#include <signal.h>
#include <errno.h>
#include "stress.h"

/*
 * test some particular features of /dev/zero
 * 1) that mapping a new /dev/zero next to an old one simply extends the old one
 * 2) that MAP_FIXED works
 * 3) that the addr hint works (and fails when it should)
 * 4) that elfmap() of /dev/zero works (we set this program up for speculative
 *	execution
 */
#define MAXMAPS 100
char *Cmd;
int verbose;
int zipit[100];

int
main(int argc, char **argv)
{
	int pagesize, pfd, fd, c;
	char *v1, *v2, *v3, *v4;
	struct prmap_sgi_arg pma;
	int nmaps, nmaps1;
	char pname[20];
	prmap_sgi_t pmap[MAXMAPS];
	char *zfile = "/dev/zero";

	Cmd = errinit(argv[0]);
	pagesize = getpagesize();

	while ((c = getopt(argc, argv, "vf:")) != EOF)
		switch (c) {
		case 'v':
			verbose++;
			break;
		case 'f':
			zfile = optarg;
			break;
		}

	if ((fd = open(zfile, O_RDWR)) < 0)
		errprintf(ERR_ERRNO_EXIT, "open");

	if ((v1 = mmap(0, pagesize*4, PROT_WRITE, MAP_SHARED, fd, 0)) == (char *)MAP_FAILED) {
		errprintf(ERR_ERRNO_EXIT, "mmap1");
		/* NOTREACHED */
	}

	/* find out how many maps we have */
	sprintf(pname, "/proc/%05d", getpid());
        if ((pfd = open(pname, O_RDWR)) < 0) {
		errprintf(ERR_ERRNO_EXIT, "open proc");
		/* NOTREACHED */
	}

	pma.pr_vaddr = (caddr_t)&pmap;
	pma.pr_size = MAXMAPS * sizeof(prmap_sgi_t);
	if ((nmaps = ioctl(pfd, PIOCMAP_SGI, &pma)) < 0) {
		errprintf(ERR_ERRNO_EXIT, "PIOCPMAP1");
		/* NOTREACHED */
	}

	if (verbose)
		printf("%s:1st vaddr 0x%x nmaps %d\n", Cmd, v1, nmaps);
	
	/* check that allocating addr we want works */
	if ((v2 = mmap(v1 + 10 * pagesize, pagesize*4, PROT_WRITE,
					MAP_SHARED, fd, 0)) == (char *)MAP_FAILED) {
		errprintf(ERR_ERRNO_EXIT, "mmap2");
		/* NOTREACHED */
	}
	if (v2 != (v1 + 10 * pagesize)) {
		errprintf(ERR_EXIT, "addr 0x%x should be 0x%x\n",
			v2, v1 + 10 * pagesize);
		/* NOTREACHED */
	}
	pma.pr_vaddr = (caddr_t)&pmap;
	pma.pr_size = MAXMAPS * sizeof(prmap_sgi_t);
	if ((nmaps1 = ioctl(pfd, PIOCMAP_SGI, &pma)) < 0) {
		errprintf(ERR_ERRNO_EXIT, "PIOCPMAP2");
		/* NOTREACHED */
	}
	if (nmaps1 != nmaps + 1) {
		errprintf(ERR_EXIT, "nmaps %d should be %d\n", nmaps1, nmaps + 1);
		/* NOTREACHED */
	}

	/* now check for grow of /dev/zero does a grow, not a new map
	 * this certainly relies on the current implementation of mmap and
	 * PIOCMAP_SGI
	 */
	if ((v3 = mmap(v1 + 4 * pagesize, pagesize*6, PROT_WRITE,
					MAP_SHARED, fd, 0)) == (char *)MAP_FAILED) {
		errprintf(ERR_ERRNO_EXIT, "mmap3");
		/* NOTREACHED */
	}
	if (v3 != (v1 + 4 * pagesize)) {
		errprintf(ERR_EXIT, "grow:addr 0x%x should be 0x%x\n",
			v3, v1 + 4 * pagesize);
		/* NOTREACHED */
	}
	pma.pr_vaddr = (caddr_t)&pmap;
	pma.pr_size = MAXMAPS * sizeof(prmap_sgi_t);
	if ((nmaps1 = ioctl(pfd, PIOCMAP_SGI, &pma)) < 0) {
		errprintf(ERR_ERRNO_EXIT, "PIOCPMAP3");
		/* NOTREACHED */
	}
	/* we should not have gotten another region, just grown the
	 * first one
	 */
	if (nmaps1 != nmaps + 1) {
		errprintf(ERR_EXIT, "grow:nmaps %d should be %d\n", nmaps1, nmaps + 1);
		/* NOTREACHED */
	}

	/*
	 * now do a FIXED mapping over portions of what we've just done
	 */
	if ((v4 = mmap(v1 + 3 * pagesize, pagesize*8, PROT_WRITE,
			MAP_FIXED|MAP_SHARED, fd, 0)) == (char *)MAP_FAILED) {
		errprintf(ERR_ERRNO_EXIT, "mmap4");
		/* NOTREACHED */
	}
	if (v4 != (v1 + 3 * pagesize)) {
		errprintf(ERR_EXIT, "fixed:addr 0x%x should be 0x%x\n",
			v4, v1 + 3 * pagesize);
		/* NOTREACHED */
	}
	pma.pr_vaddr = (caddr_t)&pmap;
	pma.pr_size = MAXMAPS * sizeof(prmap_sgi_t);
	if ((nmaps1 = ioctl(pfd, PIOCMAP_SGI, &pma)) < 0) {
		errprintf(ERR_ERRNO_EXIT, "PIOCPMAP4");
		/* NOTREACHED */
	}
	/* should have a total of 3 maps of zero now since we split one */
	if (nmaps1 != nmaps + 2) {
		errprintf(ERR_EXIT, "fixed:nmaps %d should be %d\n", nmaps1, nmaps + 2);
		/* NOTREACHED */
	}

	/*
	 * now make sure that asking for an address that is in use
	 * gives us a new address
	 */
	if ((v4 = mmap(v1 + 1 * pagesize, pagesize*8, PROT_WRITE,
			MAP_SHARED, fd, 0)) == (char *)MAP_FAILED) {
		errprintf(ERR_ERRNO_EXIT, "mmap5");
		/* NOTREACHED */
	}
	if (v4 == (v1 + 1 * pagesize)) {
		errprintf(ERR_EXIT, "bad-hint:addr 0x%x should not be!\n", v4);
		/* NOTREACHED */
	}
	printf("%s:SUCCESS\n", Cmd);

	return 0;
}
