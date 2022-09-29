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

#ident	"$Revision: 1.10 $ $Author: tee $"
#include <stdio.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <signal.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <wait.h>
#include "stress.h"

char		*Cmd;
char		*filename;
struct stat	statbuf;
long		a_sh_0_518;
long		a_sh_8_4;
long		a_sh_2_16;
long		a_pr_0_20;
long		a_pr_2_16;
long		a_pr_512_end;
long		a_sh_0_end;
char		*pattern = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
char		*trailer = "This_is_the_end...";
char		*newtrailer = "THE_NEW_END_OF_THE_FILE";
int		pagesize;
int		verbose;

#define	NMAPPEDFILES	10
#define NPAGES		518
#define	NLINES		(NPAGES * pagesize/64)
#define SEGSIZE		(512 * pagesize)

struct mappedfiles {
	caddr_t	addr;		/* mapped virtual address */
	int	len;		/* length */
	int	prot;		/* protections */
	int	flags;		/* flags, of course */
	int	fo;		/* index into openfile[] array */
	int	off;		/* file offset of mapping */
} mappedfile[] = {
	{ 0, 0, 0, 0, 0, 0 } ,
	{ 0, 0, 0, 0, 0, 0 } ,
	{ 0, 0, 0, 0, 0, 0 } ,
	{ 0, 0, 0, 0, 0, 0 } ,
	{ 0, 0, 0, 0, 0, 0 } ,
	{ 0, 0, 0, 0, 0, 0 } ,
	{ 0, 0, 0, 0, 0, 0 } ,
	{ 0, 0, 0, 0, 0, 0 } ,
	{ 0, 0, 0, 0, 0, 0 } ,
	{ 0, 0, 0, 0, 0, 0 } ,
};

struct openfiles {
	int	fd;
	char	*name;
} openedfile[] = {
	{ -1, "rdonly" } ,
	{ -1, "wronly" } ,
	{ -1, "rdwr" } ,
	{ -1, "???" } ,
};

void
bail(void)
{
	unlink(filename);
	abort();
}

void
do_prot(int prot)
{
	printf("prot: ");
	if (prot & PROT_READ) printf("read, ");
	if (prot & PROT_WRITE) printf("write, ");
	if (prot & PROT_EXECUTE) printf("exec, ");
}

void
do_flags(int flags)
{
	printf("type: ");
	if ((flags & MAP_TYPE) == 0)
		printf("none, ");
	else {
		if (flags & MAP_SHARED) printf("shared, ");
		if (flags & MAP_PRIVATE) printf("private, ");
	}
	if (flags & MAP_FIXED) printf("map fixed, ");
	if (flags & MAP_RENAME) printf("map rename, ");
}

void
do_mapped(int mindx)
{
	printf("addr: 0x%x, len: 0x%x, foffset 0x%x, ", mappedfile[mindx].addr,
		mappedfile[mindx].len, mappedfile[mindx].off);
	do_prot(mappedfile[mindx].prot);
	do_flags(mappedfile[mindx].flags);
}

#define	address(resource)	mappedfile[resource].addr
#define	release(resource)	mappedfile[resource].addr = 0

int
map(caddr_t addr, int len, int prot, int flags, int fo, off_t off)
{
	register caddr_t vaddr;
	register int	i;

	for (i = 0; i < NMAPPEDFILES; i++) {
		if (mappedfile[i].addr == 0)
			break;
	}
	if (i == NMAPPEDFILES) {
		errprintf(ERR_EXIT, "mapped file test table overflow! quitting!\n");
		/* NOTREACHED */
	}

	if (verbose) {
		printf("m:%d:mapping %s(fd %d): addr 0x%x off 0x%x len 0x%x ",
			getpid(), openedfile[fo].name,
			openedfile[fo].fd, addr, off, len);
		do_prot(prot), do_flags(flags);
	}

	vaddr = mmap(addr, len, prot, flags, openedfile[fo].fd, off);
	if (vaddr == (caddr_t)-1L) {
		int serr = errno;
		if (verbose)
			printf("failed:%s\n", strerror(errno));
		errno = serr;
		return(-1);
	}
	if (verbose)
		printf("returned 0x%x\n", vaddr);
	mappedfile[i].addr = vaddr;
	mappedfile[i].len = len;
	mappedfile[i].prot = prot;
	mappedfile[i].flags = flags;
	mappedfile[i].fo = fo;
	mappedfile[i].off = off;
	return(i);
}

void
unmap(caddr_t addr, int len, int mindx)
{
	if (verbose) {
		printf("m:%d:munmap(0x%x, 0x%x), file mapped ",
			getpid(), addr, len);
		do_mapped(mindx);
	}

	if (munmap(addr, len)) {
		errprintf(ERR_ERRNO_EXIT, "munmap");
		/* NOTREACHED */
	}
	if (verbose)
		printf("returned OK\n");
}

void
unmapregion(int mindx)
{
	unmap(mappedfile[mindx].addr, mappedfile[mindx].len, mindx);
	release(mindx);
}

void
checksums(int mindx, int page)
{
	char *p;
	long off;
	char	a, b, c;

	p = address(mindx) + pagesize*page;
	off = ((mappedfile[mindx].off + pagesize - 1) / pagesize) + page;
	a = b = 0;
	/*
	 * Adjust offset for pagesize (the algorithm that writes to the pages
	 * writes an entry every 64 lines, each line being 64 bytes.)
	 */
	off *= (pagesize / (64*64));
	while (off >= 100) {
		a++;
		off -= 100;
	}
	while (off >= 10) {
		b++;
		off -= 10;
	}
	c = off;

	a += '0', b += '0', c += '0';

	if (verbose) {
		printf("m:%d:reading page %d checksums on file mapped ",
				getpid(), page);
		do_mapped(mindx);
	}
	if ((*p == a) && (*(p+1) == b) && (*(p+2) == c)) {
		if (verbose)
			printf("succeeded!\n");
	} else {
		errprintf(ERR_EXIT, "got %c %c %c instead of %c %c %c\n",
			*p, *(p+1), *(p+2), a, b, c);
		/* NOTREACHED */
	}
}

int
openfile(char *filename, int mode)
{
	int fd;

	if ((fd = open(filename, mode)) < 0) {
		errprintf(ERR_ERRNO_EXIT, "open");
		/* NOTREACHED */
	}
	return(fd);
}

void
catch_sig(int sig, int code)
{
	if (verbose)
		printf("m:got signal %d\n", sig);
	exit(0);
}


int
main(int argc, char **argv)
{
	FILE		*f;
	register int	d, i, j;
	char		a, b, c;
	char		*p;
	pid_t		forkid;
	auto int	status;
	pid_t		pid;

	pagesize = getpagesize();

	Cmd = errinit(argv[0]);
	while ((d = getopt(argc, argv, "v")) != EOF) {
		switch(d) {
		case 'v':
			verbose = 1;
			break;
		default:
			fprintf(stderr, "Usage:%s [-v]\n", Cmd);
			exit(1);
		}
	}
	filename = tempnam(NULL, "Tmap0");
	printf("m:Implicit i/o tests, using file %s\n", filename);

	if ((stat(filename, &statbuf) != 0) && errno != ENOENT) {
		errprintf(ERR_ERRNO_RET, "mmap stat");
		bail();
	}

	if (verbose)
		printf("m:Creating %s\n", filename);
	f = fopen(filename, "w+");
	if (f == NULL) {
		errprintf(ERR_RET, "fopen");
		bail();
	}

	/*
	 * Create a file of 518 pages + 16 bytes.
	 */
	for (i = 0 ; i < NLINES ; i++) {
		if (fwrite(pattern, 1, 64, f) != 64) {
			errprintf(ERR_RET, "fwrite");
			bail();
		}
	}
	fwrite(trailer, 16, 1, f);
	fclose(f);

	/*
	 * Now we'll open the file three ways,
	 * O_RDONLY, O_WRONLY, and O_RDWR.
	 */
	if (chmod(filename, 0777) != 0) {
		errprintf(ERR_ERRNO_RET, "chmod");
		bail();
	}
	openedfile[O_RDONLY].fd = openfile(filename, O_RDONLY);
	openedfile[O_WRONLY].fd = openfile(filename, O_WRONLY);
	openedfile[O_RDWR].fd = openfile(filename, O_RDWR);

	/*
	 * First, let's do simple reading and writing to create
	 * a nice set of patterns.  File is currently 518 pages of 'X'.
	 * Lets change everything in the second segment to '#',
	 * replace every 64th character with a newline, and change the
	 * first three characters of each page to be our page number.
	 */
	if ((a_sh_0_518 = map(0, NPAGES * pagesize, PROT_READ | PROT_WRITE,
			 MAP_SHARED, O_RDWR, 0)) < 0) {
		errprintf(ERR_ERRNO_RET, "map1");
		bail();
	}
	
	p = address(a_sh_0_518) + SEGSIZE;
	for (i = 0; i < 6*pagesize; i++)
		*p++ = '#';
		
	p = address(a_sh_0_518);
	a = b = c = '0';
	for (i = 0; i < NLINES; i++) {
		*(p + 63) = '\n';
		if ((i & 0x3f) == 0) {
			*(p+2) = a++;
			*(p+1) = b;
			*(p) = c;
			if (a > '9') {
				a = '0';
				if (++b > '9') {
					b = '0';
					++c;
				}
			}
		}
		p += 64;
	}
	unmapregion(a_sh_0_518);

	/* re-check */
	if ((a_sh_8_4 = map(0, pagesize * 4, PROT_READ | PROT_WRITE,
			 MAP_SHARED, O_RDWR, pagesize * 8)) < 0) {
		errprintf(ERR_ERRNO_RET, "map2");
		bail();
	}
	checksums(a_sh_8_4, 3);
	unmapregion(a_sh_8_4);

	/*
	 * Have child map a portion of the file (beginning at page 8,
	 * for 4 pages), and diddle with the pattern.  The changes should
	 * be seen by the parent after the child dies.
	 */
	forkid = fork();
	if (forkid == -1) {
		/*
		 * not really fatal .. just go away
		 */
		errprintf(ERR_ERRNO_RET, "can't fork");
		unlink(filename);
		exit(1);
	}
	if (forkid == 0) {	/* child */
		if ((a_sh_8_4 = map(0, pagesize * 4, PROT_READ | PROT_WRITE,
				 MAP_SHARED, O_RDWR, pagesize * 8)) < 0) {
			errprintf(ERR_ERRNO_RET, "map3");
			abort();
		}
		checksums(a_sh_8_4, 3);

		p = address(a_sh_8_4) + 128;
		/* leave a message for the parent */
		sprintf(p, "message in a bottle");
		release(a_sh_8_4);
		exit(0);
	}
	/* parent */
	for (;;) {
		pid = wait(&status);
		if (pid < 0 && errno == ECHILD)
			break;
		else if ((pid >= 0) && WIFSIGNALED(status)) {
			/*
			 * if anyone dies abnormally - get out
			 * we only want 1 core dump out of this - so don't
			 * abort if the failed child already dropped core.
			 */
			fprintf(stderr, "m:proc %d died of signal %d\n",
				pid, WTERMSIG(status));
			if (!WCOREDUMP(status))
				abort();
			/*
			 * Don't unlink on error - helps debugging
			unlink(filename);
			 */
			exit(1);
		}
	}

	if (fork() == 0) {
		/* child */
		a_pr_2_16 = map(0, pagesize*16, PROT_READ, MAP_PRIVATE, O_RDONLY, pagesize*2);
		if (a_pr_2_16 < 0) {
			errprintf(ERR_ERRNO_RET, "map4");
			abort();
		}
		p = address(a_pr_2_16) + pagesize * 6 + 128;
		if (strncmp(p, "message in a bottle", 19)) {
			errprintf(ERR_RET, "expected to find child's message ``message in a bottle'', but found %.19s\n", p);
			abort();
		} else if (verbose)
			printf("m:found child's message!\n");

		release(a_pr_2_16);
		/* yes -- this should blow us out of the water */
		signal(SIGBUS, catch_sig);
		signal(SIGSEGV, catch_sig);
		if (verbose)
			printf("m:We should get a bus error or segv here ... ");
		*(p + 33) = '=';
		errprintf(ERR_EXIT, "WE SHOULD NOT GET HERE!\n");
		/* NOTREACHED */
	}

	for (;;) {
		pid = wait(&status);
		if (pid < 0 && errno == ECHILD)
			break;
		else if ((pid >= 0) && WIFSIGNALED(status)) {
			/*
			 * if anyone dies abnormally - get out
			 * we only want 1 core dump out of this - so don't
			 * abort if the failed child already dropped core.
			 */
			fprintf(stderr, "m:proc %d died of signal %d\n",
				pid, WTERMSIG(status));
			if (!WCOREDUMP(status))
				abort();
			/*
			 * Don't unlink on error - helps debugging
			unlink(filename);
			 */
			exit(1);
		}
	}

	a_sh_2_16 = map(0, pagesize*16, PROT_READ, MAP_SHARED, O_RDONLY, pagesize*2);
	if (a_sh_2_16 < 0) {
		errprintf(ERR_ERRNO_RET, "map5");
		bail();
	}
	p = address(a_sh_2_16) + pagesize * 6 + 128;
	if (strncmp(p, "message in a bottle", 19)) {
		errprintf(ERR_RET, "expected to find child's message ``message in a bottle'', but found %.19s\n", p);
		bail();
	} else if (verbose)
		printf("m:found child's message2!\n");

	unmapregion(a_sh_2_16);

	/*
	 * Map in entire file -- NPAGES plus X bytes at the end.
	 */
	a_sh_0_end = map(0, NPAGES*pagesize+16, PROT_WRITE|PROT_READ,
			 MAP_SHARED, O_RDWR, 0);
	if (a_sh_0_end < 0) {
		errprintf(ERR_ERRNO_RET, "map6");
		bail();
	}
	p = address(a_sh_0_end) + NPAGES*pagesize;
	if (strncmp(p, trailer, 16)) {
		errprintf(ERR_RET, "Could not read trailer message in mapped file.\n");
		bail();
	} else if (verbose)
		printf("m:Found trailer message in mapped file!\n");

	if (verbose)
		printf("m:Writing through the end of the file, within last page...");
	sprintf(p, newtrailer);
	if (verbose)
		printf("succeeded!\n");
	unmapregion(a_sh_0_end);

	/*
	 * Map in second segment -- six pages and 16 bytes.
	 * Only the first 16 bytes of "newtrailer" had better be found.
	 */
	a_pr_512_end = map(0, 6*pagesize+16, PROT_READ,
			   MAP_PRIVATE, O_RDONLY, SEGSIZE);
	if (a_pr_512_end < 0) {
		errprintf(ERR_ERRNO_RET, "map7");
		bail();
	}

	checksums(a_pr_512_end, 2);

	p = address(a_pr_512_end) + 6*pagesize;

	if (strncmp(p, newtrailer, 16)) {
		errprintf(ERR_RET, "Could not read trailer message in mapped file.\n");
		bail();
	} else {
		if (verbose)
			printf("m:Found trailer message in mapped file!\n");

		if (*(p+16) != 0) {
			errprintf(ERR_RET, "Failure -- first byte beyond actual file is %c(%d), not zero!\n", *(p+16), *(p+16));
			bail();
		} else if (verbose)
			printf("m:OK reading past actual file, got zeros\n");
	}

	unmapregion(a_pr_512_end);

	/*
	 * Lets give mmap no maptype, then both maptypes.
	 * Should both fail.
	 */
	if (verbose)
		printf("m:The next two calls should return EINVAL\n");
	a_sh_2_16 = map(0, pagesize*16, PROT_READ, 0, O_RDONLY, pagesize*2);
	if (a_sh_2_16 >= 0) {
		errprintf(ERR_RET, "Should have failed, didn't!\n");
		unmapregion(a_sh_2_16);
	}
	a_sh_2_16 = map(0, pagesize*16, PROT_READ,
			MAP_SHARED|MAP_PRIVATE, O_RDONLY, pagesize*2);
	if (a_sh_2_16 >= 0) {
		errprintf(ERR_RET, "Should have failed, didn't!\n");
		unmapregion(a_sh_2_16);
	}
	/*
	 * Ok, lets give mmap() some fixed addresses.
	 */
	/*
	 * Try to map in on non-page boundary address.
	 */
	if (verbose)
		printf("m:The next call should return EINVAL\n");
	a_sh_2_16 = map((caddr_t)0x40000020L, pagesize*16, PROT_READ,
			MAP_SHARED|MAP_FIXED, O_RDONLY, pagesize*2);
	if (a_sh_2_16 >= 0) {
		errprintf(ERR_RET, "Should have failed, didn't!\n");
		unmapregion(a_sh_2_16);
	}

	a_sh_2_16 = map((caddr_t)0x40000000L, pagesize*16, PROT_READ,
			MAP_SHARED|MAP_FIXED, O_RDONLY, pagesize*2);
	if (a_sh_2_16 < 0) {
		errprintf(ERR_ERRNO_RET, "map8");
		bail();
	} else {
		checksums(a_sh_2_16, 5);
		unmapregion(a_sh_2_16);
	}
	unlink(filename);
	printf("%s:PASSED\n", Cmd);
	exit(0);
}
