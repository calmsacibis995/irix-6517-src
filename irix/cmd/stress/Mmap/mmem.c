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

#ident	"$Revision: 1.5 $ $Author: jwag $"
#include <stdio.h>
#include <sys/types.h>
#include <sys/syssgi.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <unistd.h>

/*
 * trivial check for mmaped devices
 */
static unsigned int cycleval;
static int fd = -1;
static unsigned long long lastts;

#define READCYCLE(x) (cycleis32 ? *(unsigned long *)(x) : *(x))
static int cycleis32 = 0;

volatile unsigned long long *
initcc(void)
{
	__psunsigned_t phys_addr, raddr;
	int poffmask;
	volatile unsigned long long *p;
	int fd;

	poffmask = getpagesize() - 1;
	phys_addr = syssgi(SGI_QUERY_CYCLECNTR, &cycleval);
	if (phys_addr == -1)
		return NULL;
	raddr = phys_addr & ~poffmask;
	printf("phys 0x%lx raddr 0x%lx\n", phys_addr, raddr);
	fd = open("/dev/mmem", O_RDONLY);
	p = (volatile unsigned long long *)mmap(0, poffmask, PROT_READ,
			MAP_PRIVATE, fd, (off_t)raddr);
	if (p == (volatile unsigned long long *)MAP_FAILED) {
		fprintf(stderr, "mmem:map failed:%s\n", strerror(errno));
		abort();
		/* NOTREACHED */
	}

	p = (volatile unsigned long long *)
		((__psunsigned_t)p + (phys_addr & poffmask));
	/* HACK! */
	if ((unsigned long long)p & 0xf) cycleis32 = 1;

	lastts = READCYCLE(p);
	close(fd);
	return p;
}

unsigned long long
ccdelta(unsigned long long cc)
{
	unsigned long long t = cc - lastts;
	lastts = cc;
	return (t * cycleval) / (1000 * 1000);
}

int
main()
{
	volatile unsigned long long *cp;

	if ((cp = initcc()) == NULL)
		return 0; /* no cycle counter */
	printf("mmem:delta %lld\n", ccdelta(READCYCLE(cp)));
	return 0;
}
