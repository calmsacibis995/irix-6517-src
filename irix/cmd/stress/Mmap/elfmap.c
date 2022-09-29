#include <stdio.h>
#include <getopt.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/procfs.h>
#include <sys/syssgi.h>
#include <signal.h>
#include <errno.h>
#include "stress.h"
#include <elf.h>
#include <sys/elf.h>

/*
 * test elfmap() a bit
 */
char *Cmd;
int verbose;
#if _MIPS_SIM == _ABI64
#define phdr_t Elf64_Phdr
#else
#define phdr_t Elf32_Phdr
#endif

#define MAXMAPS 100
int
getnmaps(int pfd)
{
	prmap_sgi_t pmap[MAXMAPS];
	int nmaps;
	struct prmap_sgi_arg pma;

	pma.pr_vaddr = (caddr_t)&pmap;
	pma.pr_size = MAXMAPS * sizeof(prmap_sgi_t);
	if ((nmaps = ioctl(pfd, PIOCMAP_SGI, &pma)) < 0) {
		errprintf(ERR_ERRNO_EXIT, "PIOCPMAP1");
		/* NOTREACHED */
	}
	return nmaps;
}

void
unmapit(char *vaddr, size_t len)
{
	if (munmap(vaddr, len) != 0) {
		errprintf(ERR_ERRNO_EXIT, "MUNMAP failed for addr 0x%x len %d",
				vaddr, len);
		/* NOTREACHED */
	}
}

int
main(int argc, char **argv)
{
	int pagesize, pfd, fd, i, j, c;
	char *v1;
	int nmaps, nmaps1;
	char pname[20];
	int buf[1024];
	phdr_t hdrs[100];
	char *mapfile;

	Cmd = errinit(argv[0]);
	pagesize = getpagesize();

	while ((c = getopt(argc, argv, "v")) != EOF)
		switch (c) {
		case 'v':
			verbose++;
			break;
		}

	mapfile = tempnam(NULL, "elfmap");
	if ((fd = open(mapfile, O_RDWR|O_CREAT|O_TRUNC, 0666)) < 0)
		errprintf(ERR_ERRNO_EXIT, "open");

	/*
	 * set up a pattern - write a 32 bit word giving the word offset in
	 * in the file
	 */
	for (i = 0; i < 100; i++) {
		for (j = 0; j < sizeof(buf) / sizeof(buf[0]); j++)
			buf[j] = j + i * (int)sizeof(buf);
		if (write(fd, buf, sizeof(buf)) != sizeof(buf)) {
			errprintf(ERR_ERRNO_RET, "bad write");
			goto bad;
		}
	}

	/* find out how many maps we have */
	sprintf(pname, "/proc/%05d", getpid());
        if ((pfd = open(pname, O_RDWR)) < 0) {
		errprintf(ERR_ERRNO_EXIT, "open proc");
		/* NOTREACHED */
	}

	nmaps = getnmaps(pfd);

	/*
	 * set up for basic 2 partition mapping
	 * everything aligned
	 */
	hdrs[0].p_type = PT_LOAD;
	hdrs[0].p_flags = PF_R;
	hdrs[0].p_offset = 0;
	hdrs[0].p_vaddr = 0x20000000;
	hdrs[0].p_filesz = 4 * pagesize;
	hdrs[0].p_memsz = 4 * pagesize;
	hdrs[0].p_align = pagesize;
	hdrs[1].p_type = PT_LOAD;
	hdrs[1].p_flags = PF_W|PF_R;
	hdrs[1].p_offset = 4 * pagesize;
	hdrs[1].p_vaddr = hdrs[0].p_vaddr + 0x100000;
	hdrs[1].p_filesz = 2 * pagesize;
	hdrs[1].p_memsz = 4 * pagesize;
	hdrs[1].p_align = pagesize;

	if ((v1 = (char *)syssgi(SGI_ELFMAP, fd, hdrs, 2)) == (char *)MAP_FAILED) {
		errprintf(ERR_ERRNO_EXIT, "ELFMAP1 failed");
		/* NOTREACHED */
	}
	if (v1 != (char *)hdrs[0].p_vaddr) {
		errprintf(ERR_ERRNO_EXIT, "ELFMAP1 returned 0x%x rather than 0x%x",
				v1, hdrs[0].p_vaddr);
		/* NOTREACHED */
	}

	if ((nmaps1 = getnmaps(pfd)) != (nmaps + 2)) {
		errprintf(ERR_ERRNO_EXIT, "ELFMAP1 should have %d maps but have %d ",
				nmaps + 2, nmaps1);
		/* NOTREACHED */
	}

	/* now check contents XX */

	/* now remove them via munmap and check that we got rid of them all */
	unmapit((char *)hdrs[0].p_vaddr, (size_t)hdrs[0].p_memsz);
	unmapit((char *)hdrs[1].p_vaddr, (size_t)hdrs[1].p_memsz);
	if ((nmaps1 = getnmaps(pfd)) != nmaps) {
		errprintf(ERR_ERRNO_EXIT, "ELFMAP1 should have %d maps but have %d ",
				nmaps, nmaps1);
		/* NOTREACHED */
	}

	/*
	 * now, do it again twice to make sure it will pick an alternate
	 * address
	 */
	if ((v1 = (char *)syssgi(SGI_ELFMAP, fd, hdrs, 2)) == (char *)MAP_FAILED) {
		errprintf(ERR_ERRNO_EXIT, "ELFMAP2 failed");
		/* NOTREACHED */
	}
	if (v1 != (char *)hdrs[0].p_vaddr) {
		errprintf(ERR_ERRNO_EXIT, "ELFMAP2 returned 0x%x rather than 0x%x",
				v1, hdrs[0].p_vaddr);
		/* NOTREACHED */
	}
	if ((v1 = (char *)syssgi(SGI_ELFMAP, fd, hdrs, 2)) == (char *)MAP_FAILED) {
		errprintf(ERR_ERRNO_EXIT, "ELFMAP2 failed");
		/* NOTREACHED */
	}
	/* should have gotton DIFFERENT address */
	if (v1 == (char *)hdrs[0].p_vaddr) {
		errprintf(ERR_ERRNO_EXIT, "ELFMAP2 returned 0x%x!", v1);
		/* NOTREACHED */
	}
	/* remove them all */
	unmapit((char *)hdrs[0].p_vaddr, (size_t)hdrs[0].p_memsz);
	unmapit((char *)hdrs[1].p_vaddr, (size_t)hdrs[1].p_memsz);
	unmapit(v1, (size_t)hdrs[0].p_memsz);
	unmapit(v1 + 0x100000, (size_t)hdrs[1].p_memsz);
	if ((nmaps1 = getnmaps(pfd)) != nmaps) {
		errprintf(ERR_ERRNO_EXIT, "ELFMAP2 should have %d maps but have %d ",
				nmaps, nmaps1);
		/* NOTREACHED */
	}

	/* now try to test some error cases by having the 2 sections
	 * map to the same place
	 */
	hdrs[1].p_vaddr = hdrs[0].p_vaddr;
	if ((v1 = (char *)syssgi(SGI_ELFMAP, fd, hdrs, 2)) != (char *)MAP_FAILED) {
		errprintf(ERR_ERRNO_EXIT, "ELFMAP3 worked");
		/* NOTREACHED */
	}
	/* and nothing should have been added */
	if ((nmaps1 = getnmaps(pfd)) != nmaps) {
		errprintf(ERR_ERRNO_EXIT, "ELFMAP3 should have %d maps but have %d ",
				nmaps, nmaps1);
		/* NOTREACHED */
	}

	unlink(mapfile);
	return 0;
bad:
	unlink(mapfile);
	exit(1);
	/* NOTREACHED */
}
