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

#ident	"$Revision: 1.11 $ $Author: tee $"
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/param.h>
#include <sys/sysmacros.h>

extern int openfile(char *filename, int mode);

static int pagesize;
int bytes_per_seg;
int nlines;
int verbose;
#undef _PAGESZ
#define _PAGESZ		pagesize

char		*filename = "/dev/zero";
struct stat	statbuf;
int		a_sh_0_518;
int		a_sh_8_4;
int		a_sh_2_16;
int		a_pr_0_20;
int		a_pr_2_16;
int		a_pr_512_end;
int		a_sh_0_end;
char		*pattern = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
char		*trailer = "This_is_the_end...";
char		*newtrailer = "THE_NEW_END_OF_THE_FILE";

#define	NMAPPEDFILES	10
#define NPAGES		518

struct mappedfiles {
	caddr_t	addr;		/* mapped virtual address */
	size_t	len;		/* length */
	int	prot;		/* protections */
	int	flags;		/* flags, of course */
	int	fo;		/* index into openfile[] array */
	off_t	off;		/* file offset of mapping */
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
	if ((flags & MAP_TYPE) == 0) printf("none, ");
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
map(
	char *addr,
	int len,
	int prot,
	int flags,
	int fo,
	off_t off)
{
	register caddr_t vaddr;
	register int	i;

	for (i = 0; i < NMAPPEDFILES; i++) {
		if (mappedfile[i].addr == 0)
			break;
	}
	if (i == NMAPPEDFILES) {
		printf("mapped file test table overflow! quitting!\n");
		exit(1);
	}

	printf("mapping %s(fd %d): addr 0x%x, off: 0x%x, len: 0x%x ",
		openedfile[fo].name, openedfile[fo].fd, addr, off, len);
	do_prot(prot), do_flags(flags);

	errno = 0;
	vaddr = mmap(addr, len, prot, flags, openedfile[fo].fd, off);
	if (errno) {
		printf("failed\n");
		perror("mmap");
		return(-1);
	}
	printf("returned 0x%x\n", vaddr);
	mappedfile[i].addr = vaddr;
	mappedfile[i].len = len;
	mappedfile[i].prot = prot;
	mappedfile[i].flags = flags;
	mappedfile[i].fo = fo;
	mappedfile[i].off = off;
	return(i);
}

#ifdef not_used
sync(addr, len, flags, mindx)
register caddr_t addr;
register int	len, flags, mindx;
{
	printf("msync(0x%x, 0x%x), file mapped ", addr, len);
	do_mapped(mindx);

	errno = 0;
	msync(addr, len, flags);
	if (errno) {
		perror("msync");
		return(-1);
	}
	printf("returned OK\n");
	return(0);
}

void
unmapregion(int mindx)
{
	unmap(mappedfile[mindx].addr, mappedfile[mindx].len, mindx);
	release(mindx);
}
#endif

int
unmap(
	char *addr,
	int len,
	int mindx)
{
	printf("munmap(0x%x, 0x%x), file mapped ", addr, len);
	do_mapped(mindx);

	errno = 0;
	if (munmap(addr, len)) {
		perror("munmap");
		return(-1);
	}
	printf("returned OK\n");
	return(0);
}

checksums(mindx, page)
{
	register char *p = address(mindx) + pagesize*page;
	register off_t off = (off_t)(btoc(mappedfile[mindx].off) + page);
	char	a, b, c;

	a = b = 0;
	while (off >= 100) {
		a++;
		off -= 100;
	}
	while (off >= 10) {
		b++;
		off -= 10;
	}
	c = (char)off;
	a += '0', b += '0', c += '0';

	printf("reading page %d checksums on file mapped ", page);
	do_mapped(mindx);
	if ((*p == a) && (*(p+1) == b) && (*(p+2) == c)) {
		printf("succeeded!\n");
		return(0);
	} else {
		printf("failed! -- got %c %c %c instead of %c %c %c\n",
			*p, *(p+1), *(p+2), a, b, c);
		return(-1);
	}
}

int
openfile(char *filename, int mode)
{
	int fd;

	errno = 0;
	fd = open(filename, mode, 0777);
	if (errno) {
		perror("open");
		exit(1);
	}
	return(fd);
}

void
catch_sig(sig)
{
	printf("got signal %d\n", sig);
	exit(1);
}

int
main(int argc, char **argv)
{
	register int	i;
	char		a, b, c;
	char		*p, *q;
	int		forkid;

	extern char *optarg;
	extern int optind;
	int opts;

	while ((opts = getopt(argc, argv, "vf:")) != EOF)
		switch (opts) {
			case 'v':
				verbose = 1;
				break;
			case 'f':
				filename = optarg;
				break;
		}

	pagesize = getpagesize();
	if (pagesize > 0x1000)
		bytes_per_seg = ((pagesize/8) * pagesize);
	else
		bytes_per_seg = ((pagesize/4) * pagesize);
	nlines = NPAGES * pagesize/64;

	printf("Implicit i/o tests, using file %s\n", filename);

#ifdef notdef
	errno = 0;
	stat(filename, &statbuf);
	if (errno) {
		perror("mmap stat");
		exit(1);
	}
	if ((statbuf.st_mode & S_IFMT) != S_IFCHR) {
		fprintf(stderr, "%s not a char special file\n", filename);
		exit(1);
	}
#endif

	/*
	 * Now we'll open the file three ways,
	 * O_RDONLY, O_WRONLY, and O_RDWR.
	 */
	errno = 0;
	openedfile[O_WRONLY].fd = openfile(filename, O_WRONLY|O_CREAT);
	openedfile[O_RDWR].fd = openfile(filename, O_RDWR|O_CREAT);
	if (errno) {
		perror("mmap test, open");
		exit(1);
	}

	if ((a_sh_0_518 = map((char *)0, bytes_per_seg + 6*pagesize,
				PROT_READ | PROT_WRITE,
				MAP_SHARED, O_RDWR, 0)) < 0)
		exit(1);
	/*
	 * First, let's do simple reading and writing to create
	 * a nice set of patterns.  File is currently 518 pages of zeros.
	 * Lets change everything in the first segment to '+',
	 * Lets change everything in the second segment to '#',
	 * replace every 64th character with a newline, and change the
	 * first three characters of each page to be our page number.
	 */
	
	p = address(a_sh_0_518);
	if (*p == 0) {
		fprintf(stderr, "shared address space zeros!\n");
	} else {
		fprintf(stderr, "shared address space not zeros!\n");
		exit(1);
	}
	fprintf(stderr, "writing over first segment (p == 0x%x)\n", p);
	for (i = 0; i < bytes_per_seg; i++)
		*p++ = '+';

	p = address(a_sh_0_518) + bytes_per_seg;
	fprintf(stderr, "writing over second segment (p == 0x%x)\n", p);
	for (i = 0; i < 6*pagesize; i++)
		*p++ = '#';
		
	p = address(a_sh_0_518);
	a = b = c = '0';
	for (i = 0; i < nlines; i++) {
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

	/*
	 * Similar stuff for a private region (20 pages)
	 */
	if ((a_pr_0_20 = map(0, 20*pagesize, PROT_READ | PROT_WRITE,
				 MAP_PRIVATE, O_RDWR, 0)) < 0)
		exit(1);

	p = address(a_pr_0_20);
	if (*p != 0) {
		fprintf(stderr, "address space not zeros!\n");
		exit(1);
	}
	for (i = 0; i < 20*pagesize; i++)
		*p++ = '*';

	p = address(a_pr_0_20);
	a = b = c = '0';
	for (i = 0; i < 20 * pagesize / 64; i++) {
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

	/* do it again, for kicks */

	if ((a_pr_2_16 = map((char *)(address(a_pr_0_20) + 22*pagesize),
				16*pagesize, PROT_READ | PROT_WRITE,
				MAP_PRIVATE, O_RDWR, 2*pagesize)) < 0)
		exit(1);

	q = address(a_pr_2_16);
	if (*q != 0) {
		fprintf(stderr, "address space not zeros!\n");
		exit(1);
	}
	for (i = 0; i < 16*pagesize; i++)
		*q++ = '*';

	q = address(a_pr_2_16);
	p = address(a_pr_0_20);
	a = b = c = '0';
	for (i = 0; i < 16 * pagesize / 64; i++) {
		*(p + 63) = '\n';
		*(q + 63) = '\n';
		p += 64;
		q += 64;
	}

	/*
	 * Have child map a portion of the file (beginning at page 8,
	 * for 4 pages), and diddle with the pattern.  The changes should
	 * be seen by the parent after the child dies.
	 */
	forkid = fork();
	if (forkid == -1) {
		printf("fork failed, exiting!\n");
		exit(1);
	}
	if (forkid == 0) {	/* child */
		/* leave messages for the parent */
		p = address(a_sh_0_518) + 10;
		sprintf(p, "message in a bottle");
		p = address(a_pr_0_20) + 10;
		sprintf(p, "message in a bottle");
		exit(0);
	}
	/* parent */
	wait((int *)0);

	if (fork() == 0) {
		p = address(a_sh_0_518) + 10;
		if (strncmp(p, "message in a bottle", 19)) {
			printf("expected to find child's message ``message in a bottle'', but found %.19s\n", p);
		} else
			printf("found child's message!\n");

		p = address(a_pr_0_20) + 10;
		if (strncmp(p, "message in a bottle", 19)) {
			printf("did not find evidence of tampering in private region\n");
		} else
			printf("unexpectedly found child's message ``message in a bottle' in private region!\n");

		p = address(a_pr_0_20) + 21 * pagesize;
		release(a_pr_0_20);
		/* yes -- this should blow us out of the water */
		signal(SIGBUS, catch_sig);
		signal(SIGSEGV, catch_sig);
		printf("We should get a bus error or segv here ... ");
		*(p + 33) = '=';
		printf("WE SHOULD NOT GET HERE!\n");
		exit(0);
	}

	wait((int *)0);
	p = address(a_sh_0_518) + 10;
	if (strncmp(p, "message in a bottle", 19)) {
		printf("expected to find child's message ``message in a bottle'', but found %.19s\n", p);
	} else
		printf("found child's message!\n");
	return 0;
}
