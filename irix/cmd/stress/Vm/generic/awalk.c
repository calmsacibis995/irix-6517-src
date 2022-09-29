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

#ident	"$Revision: 1.6 $ $Author: tee $"

#include <stdio.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/procfs.h>
#include <malloc.h>

void 	do_prot(), do_flags(), do_mapped(), unmapregion(), makesums(),
	catch_sig(), bail(), d_printf();
char		*filename;
struct stat	statbuf;
int		quiet = 0; 	
int		a_sh_0_end;
int		a_sh_8_4;
int		a_sh_2_16;
int		a_pr_0_end;
int		a_pr_2_16;
char		*pattern = "XXXXXXX XXXXXXX XXXXXXX XXXXXXX XXXXXXX XXXXXXX XXXXXXX XXXXXXX\n";

static int pagesize;
#undef _PAGESZ
#define _PAGESZ		pagesize
#undef	NBPC
#undef	btoc
#define	NBPC	pagesize
#define	btoc(X)	((((unsigned)(X)) + pagesize - 1)/pagesize)

#define	NMAPPEDFILES	10
#define NPAGES		20
#define LINESIZE	64
#define	NLINES		NPAGES * NBPC / LINESIZE

char *rbuf;

#define LOUD	0
#define SOFT	1
#define QUIET	2

struct mappedfiles {
	caddr_t	Address;		/* mapped virtual address */
	int	Len;		/* length */
	int	Prot;		/* protections */
	int	Flags;		/* flags, of course */
	int	Fo;		/* index into openfile[] array */
	int	Off;		/* file offset of mapping */
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

void do_prot(prot)
{
	d_printf("prot: ");
	if (prot & PROT_READ) d_printf("read, ");
	if (prot & PROT_WRITE) d_printf("write, ");
	if (prot & PROT_EXECUTE) d_printf("exec, ");
}

void do_flags(flags)
{
	d_printf("type: ");
	if ((flags & MAP_TYPE) == 0) d_printf("none, ");
	else {
		if (flags & MAP_SHARED) d_printf("shared, ");
		if (flags & MAP_PRIVATE) d_printf("private, ");
	}
	if (flags & MAP_FIXED) d_printf("map fixed, ");
	if (flags & MAP_RENAME) d_printf("map rename, ");
}

void do_mapped(mindx)
{
	d_printf("addr: 0x%x, len: 0x%x, foffset 0x%x, ",
		mappedfile[mindx].Address,
		mappedfile[mindx].Len, mappedfile[mindx].Off);
	do_prot(mappedfile[mindx].Prot);
	do_flags(mappedfile[mindx].Flags);
}

ismapped(caddr_t address, int length, prmap_t *prmappings, int nmappings)
{
	int i;

	for (i = 0; i < nmappings; i++) {
		caddr_t vaddr;
		int	size;

		vaddr = prmappings[i].pr_vaddr;
		size = prmappings[i].pr_size;
		if (((vaddr <= address)&&(vaddr + size) >= address)||
		    (address <= vaddr)&&((address + length) >= vaddr))
			return 1;

	}
	return 0;
}

#define	addr(resource)		mappedfile[resource].Address
#define	len(resource)		mappedfile[resource].Len
#define	pglen(resource)		btoc(mappedfile[resource].Len)
#define	off(resource)		mappedfile[resource].Off
#define	release(resource)	mappedfile[resource].Address = 0


map(address, length, prot, flags, fo, offset)
caddr_t address;
int	length, prot, flags, fo;
off_t	offset;
{
	register caddr_t vaddr;
	register int	i;

	for (i = 0; i < NMAPPEDFILES; i++) {
		if (mappedfile[i].Address == 0)
			break;
	}
	if (i == NMAPPEDFILES) {
		printf("awalk:ERROR:mapped file test table overflow! quitting!\n");
		exit(1);
	}

	d_printf("awalk:%d:mapping %s(fd %d): addr 0x%lx, off: 0x%x, len: 0x%x ",
		getpid(), openedfile[fo].name, openedfile[fo].fd, address, offset,length);
	do_prot(prot), do_flags(flags);

	errno = 0;
	vaddr = mmap(address, length,  prot, flags|MAP_FIXED, openedfile[fo].fd, offset);
	if (errno) {
		perror("awalk:ERROR:mmap");
		return(-1);
	}
	mappedfile[i].Address = vaddr;
	mappedfile[i].Len = length;
	mappedfile[i].Prot = prot;
	mappedfile[i].Flags = flags;
	mappedfile[i].Fo = fo;
	mappedfile[i].Off = offset;
	return(i);
}

sync(address, length, flags, mindx)
register caddr_t address;
register int	length, flags, mindx;
{
	d_printf("awalk:msync(0x%x, 0x%x, ", address, length);

	if (flags & MS_ASYNC) {
		d_printf("MS_ASYNC");
		if (flags & MS_INVALIDATE)
			d_printf("|MS_INVALIDATE");
		d_printf(")");
	} else if (flags & MS_INVALIDATE)
		d_printf("MS_INVALIDATE)");
	else
		d_printf("0x%x)", flags);

	d_printf(" -- file mapped ");
	do_mapped(mindx);

	errno = 0;
	msync(address, length, flags);
	if (errno) {
		perror("awalk:ERROR:awalk");
		return(-1);
	}
	d_printf("returned OK\n");
	return(0);
}

void unmapregion(mindx)
{
	unmap(mappedfile[mindx].Address, mappedfile[mindx].Len, mindx);
	release(mindx);
}

unmap(address, length, mindx)
register caddr_t address;
register int	length, mindx;
{
	d_printf("awalk:munmap(0x%x, 0x%x), file mapped ", address, length);
	do_mapped(mindx);

	errno = 0;
	if (munmap(address, length)) {
		perror("awalk:ERROR:munmap");
		return(-1);
	}
	d_printf("returned OK\n");
	return(0);
}


void makesums(p, npages, key)
char *p;
{
	int	i, j;
	char	a;	/* msb */
	char	b;	/*     */
	char	c;	/* lsb */

	d_printf("awalk:Making checksums: %d pages from 0x%x, key %d\n", npages, p,key);

	a = b = 0;
	while (key >= 100) {
		a++;
		key -= 100;
	}
	while (key >= 10) {
		b++;
		key -= 10;
	}
	c = key;
	a += '0', b += '0', c += '0';

	for (i = 0; i < npages; i++, p += NBPC) {
		*(p+2) = c++;
		*(p+1) = b;
		*(p) = a;
		if (c > '9') {
			c = '0';
			if (++b > '9') {
				b = '0';
				++a;
			}
		}
	}
}

checksums(p, npages, key, volume)
char *p;
{
	char	a;	/* msb */
	char	b;	/*     */
	char	c;	/* lsb */
	int	j;
	int	nerrors = 0;

	if (volume != QUIET)
		d_printf("awalk:Doing checksums: %d pages from 0x%x, key %d\n",
			npages, p, key);

	a = b = 0;
	while (key >= 100) {
		a++;
		key -= 100;
	}
	while (key >= 10) {
		b++;
		key -= 10;
	}
	c = key;
	a += '0', b += '0', c += '0';

	for (j = 0; j < npages; j++, p += NBPC) {
		if ((*p != a) || (*(p+1) != b) || (*(p+2) != c)) {
			nerrors++;
			if (volume == LOUD)
				printf("awalk:page %d failed! -- got %c %c %c instead of %c %c %c\n",
					j, *p, *(p+1), *(p+2), a, b, c);
		}
		c++;
		if (c > '9') {
			c = '0';
			if (++b > '9') {
				b = '0';
				++a;
			}
		}
	}
	return(nerrors);
}

openfile(filename, mode)
char	*filename;
{
	int fd;

	errno = 0;
	fd = open(filename, mode);
	if (errno) {
		perror("awalk:ERROR:open");
		abort();
	}
	return(fd);
}

void catch_sig(sig)
{
	d_printf("awalk:got signal %d", sig);
	exit(0);
}


main(argc, argv)
char **argv;
{
	FILE	*f;
	int	i, j;
	int	nmappings;
	int	keyout1, keyin1, child1;
	int	keyout2, keyin2, child2;
	int	keyout3, keyin3, child3;
	off_t	offset1, offset2, offset3;
	int	size, cid, prot, playchar;
	char	*p;
	prmap_t	*prmappings;
	char	procpath[16];
	int	procfd;
	caddr_t	address;

	pagesize = getpagesize();
	printf("Starting %s\n", argv[0]);
	setlinebuf(stdout);
#if 0
	if (argc > 1)
		filename = argv[1];
	else
		filename = tempnam(NULL, "Tmap1");
#else
	filename = tempnam(NULL, "Tmap1");
	for (i = 1 ; i < argc ; i++){
		switch (argv[i][1]) {
		case 'f':
			i++;
			filename = tempnam(NULL, argv[i]);
			break;
		case 'q':
			quiet++;
			break;
		case 'h':
		case '?':
			printf("usage: %s -{df} [filename]\n", argv[0]);
			printf("\t%s -f filename (for tmp file)\n", argv[0]);
			printf("\t%s -q =  quiet mode\n", argv[0]);
			exit(0);
			break;
		}
	}
#endif
	d_printf("awalk:(more) Implicit i/o tests, using file %s\n", filename);

	d_printf("awalk:Creating %s\n", filename);
	f = fopen(filename, "w+");
	if (f == NULL) {
		perror("awalk:ERROR:mmap test, fopen");
		exit(1);
	}

	/*
	 * Create a file of NPAGES pages
	 */

	rbuf = (char *)malloc(NPAGES*NBPC);
	if (rbuf == NULL) {
		perror("awalk:ERROR:mmap test, malloc");
		bail();
	}
	for (i = 0 ; i < NLINES ; i++) {
		if (fwrite(pattern, 1, 64, f) != 64) {
			perror("awalk:ERROR:mmap test, fwrite");
			bail();
		}
	}
	fclose(f);

	/*
	 * Now we'll open the file three ways,
	 * O_RDONLY, O_WRONLY, and O_RDWR.
	 */
	errno = 0;
	chmod(filename, 0777);
	if (errno) {
		perror("awalk:ERROR:mmap test, chmod");
		bail();
	}
	openedfile[O_RDONLY].fd = openfile(filename, O_RDONLY);
	openedfile[O_WRONLY].fd = openfile(filename, O_WRONLY);
	openedfile[O_RDWR].fd = openfile(filename, O_RDWR);
	if (errno) {
		perror("awalk:ERROR:mmap test, open");
		bail();
	}
	/*
	 * Figure out where we've already got stuff mapped
	 */
	sprintf(procpath, "/proc/%d", getpid());
	if ((procfd = open(procpath, O_RDONLY)) < 0) {
		perror("awalk:ERROR:mmap test, proc open");
		bail();
	}
	if (ioctl(procfd, PIOCNMAP, (void *)&nmappings) < 0) {
		perror("awalk:ERROR:mmap test, piocnmap");
		bail();
	}
	prmappings = (prmap_t *)malloc(nmappings * sizeof(prmap_t));
	if (prmappings == NULL) {
		perror("awalk:ERROR:mmap test, malloc");
		bail();
	}
	if (ioctl(procfd, PIOCMAP, (void *)prmappings) < 0) {
		perror("awalk:ERROR:mmap test, piocmap");
		bail();
	}
	for (address = (caddr_t)(long)getpagesize();
		address < (caddr_t)&address;
			address = (caddr_t)((unsigned long)address << 1)) {

		if (ismapped(address, 20*NBPC, prmappings, nmappings))
			continue;

		if ((a_sh_0_end = map(address, 20*NBPC, PROT_READ | PROT_WRITE,
				 MAP_SHARED|MAP_FIXED, O_RDWR, (off_t)0)) < 0)
			bail();
		
		/*
		 * Sync the file then have child read file.
		 * Child should be able to see our changes.
		 *	pg 0 0 0	chksum 0 0 9
		 *	pg 0 0 1	chksum 0 1 0
		 *	 ...		  ...
		 *	pg 0 1 9	chksum 0 2 8
		 */
		keyout1 = 9, keyin1 = 0;
		makesums(addr(a_sh_0_end), pglen(a_sh_0_end), keyout1);
		sync(addr(a_sh_0_end), len(a_sh_0_end), 0, a_sh_0_end);
		child1 = child(off(a_sh_0_end), pglen(a_sh_0_end), keyout1, keyin1);
		wait((int *)0);
		checksums(addr(a_sh_0_end), pglen(a_sh_0_end),
			 keyin1 == 0 ? keyout1 : keyin1, LOUD);
	
		/*
		 * Write new checksums.
		 *	pg 0 0 0	chksum 0 0 2
		 *	pg 0 0 1	chksum 0 0 3
		 *	 ...		  ...
		 *	pg 0 1 9	chksum 0 2 1
		 */
		keyout1 = 2, keyin1 = 4;
		makesums(addr(a_sh_0_end), pglen(a_sh_0_end), keyout1);
		/*
		 * Sync the file w/ MS_INVALIDATE, and
		 * have child write back a new checksum key.
		 *	pg 0 0 0	chksum 0 0 4
		 *	pg 0 0 1	chksum 0 0 5
		 *	 ...		  ...
		 *	pg 0 1 9	chksum 0 2 3
		 * Check checksums based on new checksum key.
		 */
		sync(addr(a_sh_0_end), len(a_sh_0_end), MS_INVALIDATE, a_sh_0_end);
		child1 = child(off(a_sh_0_end), pglen(a_sh_0_end), keyout1, keyin1);
		wait((int *)0);
		checksums(addr(a_sh_0_end), pglen(a_sh_0_end), keyin1, LOUD);
	
	
		keyout1 = 13, keyin1 = 11, playchar = 24;
		offset1 = 8 * NBPC, size = 10 * NBPC;
		prot = PROT_READ | PROT_WRITE;
		/*
		 * Now spawn off another child that maps the same file,
		 * reading and writing.  This creates a separate region.
		 * Child writes checksums based on keyin.
		 *	pg 0 0 8	chksum 0 1 9	(was 0 1 2)
		 *	pg 0 0 9	chksum 0 2 0
		 *	 ...		  ...
		 *	pg 0 1 7	chksum 0 2 3
		 *
		 * Child doesn't return until keyout checksum is found in offset page:
		 *	pg 0 0 8	chksum 0 2 1	(was 0 1 9)
		 */
		cid = cmapsum(address, size, prot, MAP_SHARED|MAP_FIXED,
			O_RDWR, offset1, keyin1, keyout1, playchar);
	
		keyout2 = 20, keyin2 = 3, playchar = 31;
		offset2 = 6 * NBPC, size = 9 * NBPC;
		prot = PROT_READ | PROT_WRITE;
		/*
		 * Now spawn off another child that maps the same file,
		 * reading and writing.  This creates a separate region.
		 * Child writes checksums based on keyin.
		 *	pg 0 0 6	chksum 0 0 9	(was 0 1 7)
		 *	pg 0 0 7	chksum 0 1 0
		 *	 ...		  ...
		 *	pg 0 1 4	chksum 0 1 7
		 *
		 * Child doesn't return until keyout checksum is found in offset page:
		 *	pg 0 0 6	chksum 0 2 6	(was 0 ? ?)
		 */
		cid = cmapsum(address, size, prot, MAP_SHARED|MAP_FIXED,
			O_RDWR, offset2, keyin2, keyout2, playchar);
	
		p = addr(a_sh_0_end) + offset1;
		/* ``keyout1'' checksum lets child return
		 *	pg 0 0 8	chksum 0 2 1	(was 0 1 1)
		 */
		makesums(p, 1, keyout1);
		wait((int *)0);
	
		p = addr(a_sh_0_end) + offset2;
		/* ``keyout2'' checksum lets child return
		 *	pg 0 0 6	chksum 0 2 6	(was 0 1 1)
		 */
		makesums(p, 1, keyout2);
		wait((int *)0);
		wait((int *)0);
	
		/* NOW, do our awalk(.... MS_INVALIDATE)  test */
		keyout3 = 2, keyin3 = 5;
		/*
		 *	pg 0 0 0	chksum 0 0 2	(was ? ? ?)
		 *	pg 0 0 1	chksum 0 0 3
		 *	 ...		  ...
		 *	pg 0 1 9	chksum 0 2 1
		 */
		makesums(addr(a_sh_0_end), pglen(a_sh_0_end), keyout3);
		sync(addr(a_sh_0_end), len(a_sh_0_end), MS_INVALIDATE, a_sh_0_end);
		child3 = child(off(a_sh_0_end), pglen(a_sh_0_end), keyout3, keyin3);
		wait((int *)0);
		/*
		 * Look for our new checksums...
		 *
		 *	pg 0 0 0	chksum 0 0 5	(was 0 0 2)
		 *	pg 0 0 1	chksum 0 0 6
		 *	 ...		  ...
		 *	pg 0 1 9	chksum 0 2 4
		 */
		checksums(addr(a_sh_0_end), 20, keyin3, LOUD);
		unmapregion(a_sh_0_end);
	}
	unlink(filename);
	printf("%s completed\n", argv[0]);
	exit(0);
}

/*
 * Fork child.
 * Child maps fd beginning at offset for length bytes, etc.
 * We return child's pid to parent.
 * Child makes checksums based on keyout value; parent spins until
 * it sees those checksums in its mapping of the file.
 * Now child spins until it sees keyin checksums in its mapping.
 * While spinning, child does reads and writes on non-checksum areas,
 * performing sanity checks.
 */
cmapsum(address, length, protections, flags, fd, offset, keyout, keyin, playc)
char	*address;
off_t	offset;
{
	int	mindx, i;
	char	*p, b, c;
	int	forkid;
	int	iteration = 0;
	int	nerrors = 0;

	forkid = fork();

	if (forkid == -1) {
		printf("awalk:ERROR:fork failed, exiting!\n");
		bail();
	}

	/* parent */
	if (forkid != 0) {
		p = addr(a_sh_0_end) + offset;
		/* spin until child has written new checksums */
		while (checksums(p, btoc(length), keyout, SOFT)) {
			sleep(1);
		}
		if ((checksums(p, btoc(length), keyin, QUIET)) == 0) {
			fprintf(stderr, "awalk:OUCH -- child got ahead of us!\n");
			kill(forkid, 9);
			bail();
		}
		return(forkid);
	}

	if ((mindx = map(address, length, protections, flags, fd, offset)) < 0){
		fprintf(stderr, "awalk:OUCH: child quitting early!!\n");
		exit(1);
	}
	p = addr(mindx);
	b = 0x20;
	for (i = 0; i < btoc(length); i++) {
		*(p+playc) = b;
		p += ctob(1);
	}
	makesums(addr(mindx), pglen(mindx), keyout);

	while (checksums(addr(mindx), 1, keyin, QUIET)) {
		p = addr(mindx);
		for (i = 0; i < btoc(length); i++) {
			c = *(p+playc);
			if (c != b) {
				nerrors++;
			}
			if (++c > 0x7e)
				c = 0x20;
			*(p+playc) = c;
			p += ctob(1);
		}
		iteration++;
		if (++b > 0x7e)
			b = 0x20;
	}
	sleep(1);
	if (nerrors)
	    fprintf(stderr,"awalk:cmapsum: got %d errors in %d iterations\n",
		nerrors, iteration);
	exit(1);
}


child(offset, npages, key, writeback)
{
	int	i, j;
	int	forkid;
	int	retval;
	int	fd = openedfile[O_RDWR].fd;

	forkid = fork();
	if (forkid == -1) {
		printf("awalk:fork failed, exiting!\n");
		bail();
	}
	/* parent */
	if (forkid)
		return(forkid);
	/* child */
	if (forkid == 0) {
		errno = 0;
		lseek(fd, offset, 0);
		if (errno) {
			perror("awalk:ERROR:child lseek");
			exit(1);
		}
		retval = read(fd, rbuf, npages * NBPC);
		if (errno) {
			perror("awalk:ERROR:child read");
			exit(1);
		}
		if (retval != npages * NBPC) {
			fprintf(stderr, "awalk:OUCH! read returned 0x%x, not 0x%x\n",
				retval, NPAGES*NBPC);
		}
		/*
		 * fake up a mapped file structure so we can call checksums
		 */
		for (i = 0; i < NMAPPEDFILES; i++) {
			if (mappedfile[i].Address == 0)
				break;
		}
		mappedfile[i].Address = rbuf;
		mappedfile[i].Len = NPAGES;
		mappedfile[i].Prot = 0;
		mappedfile[i].Flags = 0;
		mappedfile[i].Fo = O_RDWR;
		mappedfile[i].Off = 0;

		checksums(rbuf, npages, key, 0);

		if (writeback) {
			lseek(fd, offset, 0);
			makesums(rbuf, npages, writeback);
			retval = write(fd, rbuf, npages * NBPC);
			if (errno) {
				perror("awalk:ERROR:child write");
				exit(1);
			}
			if (retval != npages * NBPC) {
				fprintf(stderr, "awalk:OUCH! write returned 0x%x, not 0x%x\n",
					retval, NPAGES*NBPC);
			}
		}
		mappedfile[i].Address = 0;

		exit(0);
	}

	/*  Only here to make the compiler happy about returning a value.  It
	    cannot see the return(forkid) earlier.  */

	return(forkid);
}

void bail()
{
	unlink(filename);
	abort();
}

void d_printf(s1, s2, s3, s4, s5,s6)
char *s1;
int s2, s3,s4,s5,s6;
{
	if (!quiet) 
        	printf(s1,s2,s3,s4,s5,s6);
}
