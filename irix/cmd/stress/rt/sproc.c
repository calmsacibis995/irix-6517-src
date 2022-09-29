#define	_KMEMUSER
#include <fcntl.h>
#include <stdio.h>
#include <symconst.h>
#include <sys/types.h>
#include <sys/sysmp.h>
#include <sys/prctl.h>
#include <sys/proc.h>
#include <sys/mman.h>
#include <sys/cachectl.h>
#include <sys/lock.h>
#include <signal.h>
#include <errno.h>
#include <aio.h>
#include <sys/sema.h>
#include <sys/rte.h>
#include <unistd.h>
#include <ulocks.h>

/*
 * Command line options
 */
int	pcpu = 3;		/* parent cpu */
int	ccpu = 1;		/* child (sproc) cpu */
int	verbose = 0;
int	newptes_opt = 0;	/* kernel has additional newptes optimization */
int	swaptst = 0;		/* execute swap test */
/*
 * Variables for controlling synchronization
 */
usptr_t	*handle;		/* handle to user semaphore arena */
sema_t	*sprocsema;		/* semaphore to indicate sproc is done with an operation */
sema_t	*parentsema;		/* semaphore to indicate parent is done with an operation */
/*
 * Variables for doing address space operations
 */
long	pagesize;
char	*vaddr;
int	mapfd;

#define	LENGTH	(1024*pagesize)

int rtefd;		/* /dev/rte file desc. */
/*
 * Data structures for logging rte statistics
 */
rtedat_t rtedat1;
rtedat_t rtedat2;

#define	DONT_CARE	-1
/*
 * Routines to fetch rte statistics
 */
/*
 * getstat
 *
 * pull a copy of the rtedat data structure from the kernel.
 */
void
getstat(rtedat_t *rtedatp)
{
	ioctl(rtefd, RTGET, rtedatp);
}
/*
 * dumpstat
 *
 * dump the delta between the original copy of the statistics and
 * the current copy of statistics
 */
void
dumpstat(rtedat_t *rtedatp1, rtedat_t *rtedatp2)
{
	printf("Isolated CPU pfaults: %d\n",
		rtedatp2->stat.iso_pfault - rtedatp1->stat.iso_pfault);
	printf("Isolated CPU vfaults: %d\n",
		rtedatp2->stat.iso_vfault - rtedatp1->stat.iso_vfault);
	printf("Isolated CPU tlbsyncs: %d\n",
		rtedatp2->stat.iso_tlbsync - rtedatp1->stat.iso_tlbsync);
	printf("Isolated CPU tlbcleans: %d\n",
		rtedatp2->stat.iso_tlbclean -  rtedatp1->stat.iso_tlbclean);
	printf("Isolated CPU delayiflushs: %d\n",
		rtedatp2->stat.iso_delayiflush -  rtedatp1->stat.iso_delayiflush);
	printf("Isolated CPU delaytlbflushs: %d\n",
		rtedatp2->stat.iso_delaytlbflush -  rtedatp1->stat.iso_delaytlbflush);
	printf("Isolated CPU cleanicaches: %d\n",
		rtedatp2->stat.iso_cleanicache -  rtedatp1->stat.iso_cleanicache);
}
/*
 * expected
 *
 * compare the delta between two statistics snapshots against expected values.
 * 
 */
expected(rtedat_t *rtedatp1, rtedat_t *rtedatp2, rtedat_t *rtedatex)
{
	int errors = 0;

	if (rtedatex->stat.iso_pfault != DONT_CARE) {
		if (rtedatex->stat.iso_pfault !=
		    (rtedatp2->stat.iso_pfault - rtedatp1->stat.iso_pfault)) {
			printf("pfault:expected %d, got %d\n",
				rtedatex->stat.iso_pfault,
				rtedatp2->stat.iso_pfault - rtedatp1->stat.iso_pfault);
			errors++;
		}
	}
	if (rtedatex->stat.iso_vfault != DONT_CARE) {
		if (rtedatex->stat.iso_vfault !=
		    (rtedatp2->stat.iso_vfault - rtedatp1->stat.iso_vfault)) {
			printf("vfault:expected %d, got %d\n",
				rtedatex->stat.iso_vfault,
				rtedatp2->stat.iso_vfault - rtedatp1->stat.iso_vfault);
			errors++;
		}
	}
	if (rtedatex->stat.iso_tlbsync != DONT_CARE) {
		if (rtedatex->stat.iso_tlbsync !=
		    (rtedatp2->stat.iso_tlbsync - rtedatp1->stat.iso_tlbsync)) {
			printf("tlbsync:expected %d, got %d\n",
			    rtedatex->stat.iso_tlbsync,
			    rtedatp2->stat.iso_tlbsync - rtedatp1->stat.iso_tlbsync);
			errors++;
		}
	}
	if (rtedatex->stat.iso_tlbclean != DONT_CARE) {
		if (rtedatex->stat.iso_tlbclean !=
		    (rtedatp2->stat.iso_tlbclean - rtedatp1->stat.iso_tlbclean)) {
			printf("tlbclean:expected %d, got %d\n",
			    rtedatex->stat.iso_tlbclean,
			    rtedatp2->stat.iso_tlbclean - rtedatp1->stat.iso_tlbclean);
			errors++;
		}
	}
	if (rtedatex->stat.iso_delaytlbflush != DONT_CARE) {
		if (rtedatex->stat.iso_delaytlbflush !=
		    (rtedatp2->stat.iso_delaytlbflush - rtedatp1->stat.iso_delaytlbflush)) {
			printf("delaytlbflush:expected %d, got %d\n",
				rtedatex->stat.iso_delaytlbflush,
				rtedatp2->stat.iso_delaytlbflush -
					rtedatp1->stat.iso_delaytlbflush);
			errors++;
		}
	}
	if (rtedatex->stat.iso_delayiflush != DONT_CARE) {
		if (rtedatex->stat.iso_delayiflush !=
		    (rtedatp2->stat.iso_delayiflush - rtedatp1->stat.iso_delayiflush)) {
			printf("Pfaults:expected %d, got %d\n",
				rtedatex->stat.iso_delayiflush,
				rtedatp2->stat.iso_delayiflush -
					rtedatp1->stat.iso_delayiflush);
			errors++;
		}
	}
	if (rtedatex->stat.iso_cleanicache != DONT_CARE) {
		if (rtedatex->stat.iso_cleanicache !=
		    (rtedatp2->stat.iso_cleanicache - rtedatp1->stat.iso_cleanicache)) {
			printf("Pfaults:expected %d, got %d\n",
				rtedatex->stat.iso_cleanicache,
				rtedatp2->stat.iso_cleanicache -
						rtedatp1->stat.iso_cleanicache);
			errors++;
		}
	}
	return errors;
}
/*
 * stackhack
 *
 * force stack to be grown
 */
void
stackhack()
{
	volatile int sp;	

        sp = *(int *)((int)&sp - 4096);
}

/*
 * sbrk_tst_sproc
 *
 * do an sbrk that crosses a segment boundary
 */
void
sbrk_tst_sproc(void *arg)
{
	unsigned memaddr;

	if (sysmp(MP_MUSTRUN, ccpu) < 0) {
		perror("sproc:mustrun");
		exit(-1);
	}
	/*
	 * Do an sbrk and try these steps over
	 */
	if (verbose) {
		printf("sproc running on cpu %d!\n", ccpu);
		printf("Sproc sbrk'ing %x bytes\n", LENGTH);
	}
	getstat(&rtedat1);
	memaddr = (unsigned)sbrk(LENGTH);
	getstat(&rtedat2);
	/*
	 * Clean up
	 */
	(void)sbrk(-LENGTH);
	exit (0);
}

/*
 * sbrk_tst
 *
 * sproc does an sbrk that crosses a segment boundary
 * if newptes optimization is not enabled we expect
 * to see a tlbsync operation
 */
int
sbrk_tst()
{
	int spid;
	int stat;
	rtedat_t rte_expected;
	/*
	 * sbrk test
	 */
	if ((spid = sproc(sbrk_tst_sproc, PR_SALL, 0, 0)) < 0) {
		perror("sproc");
		return (-1);
	}
	/*
	 * Test for untouched memory area (should get tlbsync ops)
	 */
	waitpid(spid, &stat, 0);
	if (stat < 0)
		return (stat);

	/*
	 * Check results.  Expect 1 tlbsync operation
	 */
	rte_expected.stat.iso_pfault = DONT_CARE;
	rte_expected.stat.iso_vfault = DONT_CARE;
	rte_expected.stat.iso_tlbsync = (newptes_opt)? 0 : 1;
	rte_expected.stat.iso_tlbclean = 0;
	rte_expected.stat.iso_delaytlbflush = DONT_CARE;
	rte_expected.stat.iso_delayiflush = DONT_CARE;
	rte_expected.stat.iso_cleanicache = 0;

	return (expected(&rtedat1, &rtedat2, &rte_expected));
}
/*
 * cacheflush_tst_sproc
 *
 * sproc sbrks a memory area then issues a cacheflush
 */
void
cacheflush_tst_sproc(void *arg)
{
	unsigned memaddr;

	if (sysmp(MP_MUSTRUN, ccpu) < 0) {
		perror("sproc:mustrun");
		exit(-1);
	}
	/*
	 * Do an sbrk and try these steps over
	 */
	if (verbose) {
		printf("sproc running on cpu %d!\n", ccpu);
		printf("Sproc sbrk'ing/cacheflushing %x bytes\n", LENGTH);
	}
	getstat(&rtedat1);
	memaddr = (unsigned)sbrk(LENGTH);
	cacheflush((void *)memaddr, LENGTH, BCACHE);
	getstat(&rtedat2);
	/*
	 * Clean up
	 */
	(void)sbrk(-LENGTH);
	exit (0);
}

/*
 * cacheflush_tst_sproc
 *
 * sproc sbrks a memory area then issues a cacheflush
 * We expect to see a tlbsync (from the allocation) and
 * a cleanicache (from the cacheflush)
 */
int
cacheflush_tst()
{
	int spid;
	int stat;
	rtedat_t rte_expected;
	/*
	 * cacheflush test
	 */
	if ((spid = sproc(cacheflush_tst_sproc, PR_SALL, 0, 0)) < 0) {
		perror("sproc");
		return (-1);
	}
	/*
	 * Test for untouched memory area (should get tlbsync ops)
	 */
	waitpid(spid, &stat, 0);
	if (stat < 0)
		return (stat);

	/*
	 * Check results.  Expect 1 tlbsync op (from sbrk)
	 * and 1 cleanicache (from cacheflush)
	 */
	rte_expected.stat.iso_pfault = DONT_CARE;
	rte_expected.stat.iso_vfault = DONT_CARE;
	rte_expected.stat.iso_tlbsync = (newptes_opt)? 0 : 1;
	rte_expected.stat.iso_tlbclean = 0;
	rte_expected.stat.iso_delaytlbflush = DONT_CARE;
	rte_expected.stat.iso_delayiflush = DONT_CARE;
	rte_expected.stat.iso_cleanicache = 1;

	return (expected(&rtedat1, &rtedat2, &rte_expected));
}

/*
 * mprotect_tst_sproc
 *
 * make a protection change that requires cleanicache
 */
void
mprotect_tst_sproc(void *arg)
{
	unsigned memaddr;

	if (sysmp(MP_MUSTRUN, ccpu) < 0) {
		perror("sproc:mustrun");
		exit(-1);
	}
	/*
	 * Do an sbrk and try these steps over
	 */
	if (verbose) {
		printf("sproc running on cpu %d!\n", ccpu);
		printf("Sproc sbrk'ing/mprotecting %x bytes\n", LENGTH);
	}

	getstat(&rtedat1);
	memaddr = (unsigned)sbrk(LENGTH);
	memaddr = (memaddr + pagesize - 1) & ~(pagesize - 1);
	mprotect((void *)memaddr, LENGTH-pagesize, PROT_EXEC);
	getstat(&rtedat2);
	/*
	 * Clean up
	 */
	(void)sbrk(-LENGTH);
	exit (0);
}

/*
 * mprotect_tst
 *
 * sproc makes a change in memory protection that forces an icache
 * operation.
 */
int
mprotect_tst()
{
	int spid;
	int stat;
	rtedat_t rte_expected;
	/*
	 * mprotect test
	 */
	if ((spid = sproc(mprotect_tst_sproc, PR_SALL, 0, 0)) < 0) {
		perror("sproc");
		return (-1);
	}
	/*
	 * Test for untouched memory area (should get tlbsync ops)
	 */
	waitpid(spid, &stat, 0);
	if (stat < 0)
		return (stat);

	/*
	 * Check results.  Expect 1 tlbsync op (from sbrk)
	 * and 1 cleanicache (from cacheflush)
	 */
	rte_expected.stat.iso_pfault = DONT_CARE;
	rte_expected.stat.iso_vfault = DONT_CARE;
	rte_expected.stat.iso_tlbsync = (newptes_opt)? 1 : 2;
	rte_expected.stat.iso_tlbclean = 0;
	rte_expected.stat.iso_delaytlbflush = DONT_CARE;
	rte_expected.stat.iso_delayiflush = DONT_CARE;
	rte_expected.stat.iso_cleanicache = 1;

	return (expected(&rtedat1, &rtedat2, &rte_expected));
}
/*
 * map_local_tst_sproc
 *
 * memory map and reference a map local area.  This should have no impact
 * on the parent
 */
void
map_local_tst_sproc(void *arg)
{
	char *mapaddr;
	char *cp;

	if (sysmp(MP_MUSTRUN, ccpu) < 0) {
		perror("sproc:mustrun");
		exit(-1);
	}
	/*
	 * Allocate a memory mapped area and make references
	 */
	if (verbose) {
		printf("sproc running on cpu %d!\n", ccpu);
		printf("Sproc mapping a local memory area\n");
	}

	getstat(&rtedat1);
	mapaddr = mmap(0, LENGTH, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_LOCAL, mapfd, 0);
	if (mapaddr == (char *)-1) {
		perror("mmap");
		exit(-1);
	}
	for (cp = mapaddr; cp < mapaddr + LENGTH; cp += pagesize)
		*cp = '\0';

	munmap(mapaddr, LENGTH);
	getstat(&rtedat2);
	exit (0);
}
/*
 * map_local_tst
 *
 * Sproc allocates and reference MAP_LOCAL memory.  Should get no cpuactions
 * of any kind
 */
int
map_local_tst()
{
	int spid;
	int stat;
	rtedat_t rte_expected;
	
	if ((spid = sproc(map_local_tst_sproc, PR_SALL, 0, 0)) < 0) {
		perror("sproc");
		return (-1);
	}
	/*
	 * Test for untouched memory area (should get tlbsync ops)
	 */
	waitpid(spid, &stat, 0);

	if (stat < 0)
		return (stat);

	/*
	 * Check results.  
	 */
	rte_expected.stat.iso_pfault = 0;
	rte_expected.stat.iso_vfault = 0;
	rte_expected.stat.iso_tlbsync = 0;
	rte_expected.stat.iso_tlbclean = 0;
	rte_expected.stat.iso_delaytlbflush = 0;
	rte_expected.stat.iso_delayiflush = 0;
	rte_expected.stat.iso_cleanicache = 0;

	return (expected(&rtedat1, &rtedat2, &rte_expected));
}
/*
 * plock_tst_sproc
 *
 * do a sbrk after parent has locked memory
 */
void
plock_tst_sproc(void *arg)
{
	char *cp;

	if (sysmp(MP_MUSTRUN, ccpu) < 0) {
		perror("sproc:mustrun");
		exit(-1);
	}
	/*
	 * Allocate a memory area (already datalock'd)
	 */
	if (verbose) {
		printf("sproc running on cpu %d!\n", ccpu);
		printf("Sproc sbrk'ing with datalock\n");
	}

	getstat(&rtedat1);
	vaddr = sbrk(LENGTH);
	/*
	 * Signal that memory has been allocated and wait for parent
	 */
	usvsema(sprocsema);
	uspsema(parentsema);

	getstat(&rtedat2);

	sbrk(-LENGTH);

	exit (0);
}
/*
 * plock_tst
 *
 * -Parent plocks()
 * -sproc sbrks
 * -parent touches sbrk memory.  should get no p/v faults
 */
int
plock_tst()
{
	int spid;
	int stat;
	rtedat_t rte_expected;
	char *cp;
	
	if (plock(PROCLOCK) < 0) {
		perror("plock");
		return (-1);
	}
	/*
	 * Fault in semaphores right now
	 */
	usvsema(sprocsema);
	uspsema(sprocsema);
	usvsema(parentsema);
	uspsema(parentsema);

	if ((spid = sproc(plock_tst_sproc, PR_SALL, 0, 0)) < 0) {
		perror("sproc");
		return (-1);
	}

	/* wait for sproc */
	uspsema(sprocsema);	/* wait */
 
	for (cp = vaddr; cp < vaddr + LENGTH; cp += pagesize)
			*cp = '\0';
	usvsema(parentsema);
	/*
	 * Now wait for sproc to terminate
	 */
	waitpid(spid, &stat, 0);

	plock(UNLOCK);

	if (stat < 0)
		return (stat);

	/*
	 * Check results.  
	 */
	rte_expected.stat.iso_pfault = 0;
	rte_expected.stat.iso_vfault = 0;
	rte_expected.stat.iso_tlbsync = (newptes_opt)? 0 : 1;
	rte_expected.stat.iso_tlbclean = 0;
	rte_expected.stat.iso_delaytlbflush = DONT_CARE;
	rte_expected.stat.iso_delayiflush = DONT_CARE;
	rte_expected.stat.iso_cleanicache = 0;

	return (expected(&rtedat1, &rtedat2, &rte_expected));
}
/*
 * mpin_tst_sproc
 *
 * map and mpin a memory area
 */
void
mpin_tst_sproc(void *arg)
{
	char *cp;

	if (sysmp(MP_MUSTRUN, ccpu) < 0) {
		perror("sproc:mustrun");
		exit(-1);
	}
	/*
	 * Allocate a memory area (already datalock'd)
	 */
	if (verbose) {
		printf("sproc running on cpu %d!\n", ccpu);
		printf("Sproc mmaping/mpining a memory area\n");
	}

	getstat(&rtedat1);
	vaddr = mmap(0, LENGTH, PROT_READ|PROT_WRITE, MAP_SHARED, mapfd, 0);
	if (vaddr == (char *)-1) {
		usvsema(sprocsema);
		perror("mmap");
		exit(-1);
	}
	if (mpin(vaddr, LENGTH) < 0) {
		usvsema(sprocsema);
		perror("mpin");
		exit(-1);
	}
	/*
	 * Signal that memory has been allocated and wait for parent
	 */
	usvsema(sprocsema);
	uspsema(parentsema);

	getstat(&rtedat2);

	munpin(vaddr, LENGTH);
	munmap(vaddr, LENGTH);

	exit (0);
}
/*
 * mpin_tst
 *
 * -sproc mmaps and mpins
 * -parent touches mmap memory.  should get no p/v faults
 */
int
mpin_tst()
{
	int spid;
	int stat;
	rtedat_t rte_expected;
	char *cp;
	
	if ((spid = sproc(mpin_tst_sproc, PR_SALL, 0, 0)) < 0) {
		perror("sproc");
		return (-1);
	}
	/* wait for sproc */
	uspsema(sprocsema);	/* wait */
 
	for (cp = vaddr; cp < vaddr + LENGTH; cp += pagesize)
		*cp = '\0';

	usvsema(parentsema);
	/*
	 * Now wait for sproc to terminate
	 */
	waitpid(spid, &stat, 0);

	if (stat < 0)
		return (stat);

	/*
	 * Check results.  
	 */
	rte_expected.stat.iso_pfault = 0;
	rte_expected.stat.iso_vfault = 0;
	rte_expected.stat.iso_tlbsync = (newptes_opt)? 0 : 1;;
	rte_expected.stat.iso_tlbclean = 0;
	rte_expected.stat.iso_delaytlbflush = DONT_CARE;
	rte_expected.stat.iso_delayiflush = DONT_CARE;
	rte_expected.stat.iso_cleanicache = 0;

	return (expected(&rtedat1, &rtedat2, &rte_expected));
}


void
mmap_fault_tst_sproc(void *arg)
{
	char *cp;

	if (sysmp(MP_MUSTRUN, ccpu) < 0) {
		perror("sproc:mustrun");
		exit(-1);
	}
	if (verbose) {
		printf("sproc running on cpu %d!\n", ccpu);
		printf("Sproc mmapping a memory area\n");
	}

	getstat(&rtedat1);
	vaddr = mmap(0, LENGTH, PROT_READ|PROT_WRITE, MAP_SHARED, mapfd, 0);
	if (vaddr == (char *)-1) {
		usvsema(sprocsema);
		perror("mmap");
		exit(-1);
	}
	/*
	 * Signal that memory has been mapped and wait for parent
	 */
	usvsema(sprocsema);
	uspsema(parentsema);

	getstat(&rtedat2);

	munmap(vaddr, LENGTH);

	exit (0);
}
/*
 * mmap_fault_tst
 *
 * -sproc mmaps and does not mpin
 * -parent touches mmap memory.  should get a fault for each page
 */
int
mmap_fault_tst()
{
	int spid;
	int stat;
	rtedat_t rte_expected;
	char *cp;
	
	if ((spid = sproc(mmap_fault_tst_sproc, PR_SALL, 0, 0)) < 0) {
		perror("sproc");
		return (-1);
	}
	/* wait for sproc */
	uspsema(sprocsema);	/* wait */
 
	for (cp = vaddr; cp < vaddr + LENGTH; cp += pagesize)
		*cp = '\0';

	usvsema(parentsema);
	/*
	 * Now wait for sproc to terminate
	 */
	waitpid(spid, &stat, 0);

	if (stat < 0)
		return (stat);

	/*
	 * Check results.  
	 */
	rte_expected.stat.iso_pfault = LENGTH/pagesize;
	rte_expected.stat.iso_vfault = LENGTH/pagesize;
	rte_expected.stat.iso_tlbsync = (newptes_opt)? 0 : 1;
	rte_expected.stat.iso_tlbclean = 0;
	rte_expected.stat.iso_delaytlbflush = DONT_CARE;
	rte_expected.stat.iso_delayiflush = DONT_CARE;
	rte_expected.stat.iso_cleanicache = 0;

	return (expected(&rtedat1, &rtedat2, &rte_expected));
}

void
sbrk_fault_tst_sproc(void *arg)
{
	char *cp;

	if (sysmp(MP_MUSTRUN, ccpu) < 0) {
		perror("sproc:mustrun");
		exit(-1);
	}
	if (verbose) {
		printf("sproc running on cpu %d!\n", ccpu);
		printf("Sproc sbrk'ing a memory area\n");
	}

	getstat(&rtedat1);
	vaddr = sbrk(LENGTH);
	/*
	 * Signal that memory has been mapped and wait for parent
	 */
	usvsema(sprocsema);
	uspsema(parentsema);

	getstat(&rtedat2);

	sbrk(-LENGTH);

	exit (0);
}
/*
 * sbrk_fault_tst
 *
 * -sproc sbrks with no datalock
 * -parent touches sbrk memory.  should get a fault for each page
 */
int
sbrk_fault_tst()
{
	int spid;
	int stat;
	rtedat_t rte_expected;
	volatile char *cp;
	volatile char c;
	
	if ((spid = sproc(sbrk_fault_tst_sproc, PR_SALL, 0, 0)) < 0) {
		perror("sproc");
		return (-1);
	}
	/* wait for sproc */
	uspsema(sprocsema);	/* wait */
 	/*
	 * Set cp to offset of pagesize - 1 so that we touch every page of the
	 * allocation just in case vaddr is not page aligned
	 */
	for (cp = vaddr + pagesize - 1; cp < vaddr + LENGTH; cp += pagesize)
		c = *cp;

	for (cp = vaddr + pagesize - 1; cp < vaddr + LENGTH; cp += pagesize)
		*cp = '\0';

	usvsema(parentsema);
	/*
	 * Now wait for sproc to terminate
	 */
	waitpid(spid, &stat, 0);

	if (stat < 0)
		return (stat);

	/*
	 * Check results.  
	 */
	rte_expected.stat.iso_pfault = LENGTH/pagesize;
	rte_expected.stat.iso_vfault = LENGTH/pagesize;
	rte_expected.stat.iso_tlbsync = (newptes_opt)? 0 : 1;
	rte_expected.stat.iso_tlbclean = 0;
	rte_expected.stat.iso_delaytlbflush = DONT_CARE;
	rte_expected.stat.iso_delayiflush = DONT_CARE;
	rte_expected.stat.iso_cleanicache = 0;

	return (expected(&rtedat1, &rtedat2, &rte_expected));
}

/*
 * Allocate a bunch of space and run around using it
 */
swap_tst_child()
{
	char	*memaddr;
	volatile char	*cp;
	volatile char	c;

	memaddr = sbrk(LENGTH);

	while (1) {
		for (cp = memaddr; cp < memaddr + LENGTH; cp++)
			c = *cp;
	}
}

void
swap_tst_sproc(void *arg)
{
	volatile char *cp;
	volatile char c;

	while (1) {
		sysmp(MP_RUNANYWHERE, 0);
		for (cp = vaddr; cp < vaddr + LENGTH; cp++)
			c = *cp;
	}
}

swap_tst()
{
	int i;
	int spid;
	int fpid[32];
	int seconds;
	volatile char c;
	volatile char *cp;
	rtedat_t rte_expected;

	/*
	 * Kick off a bunch of children
	 */
	for (i = 0; i < sizeof(fpid)/sizeof(int); i++) {
		if ((fpid[i] = fork()) < 0)
			break;
		if (fpid[i] == 0) {
			sysmp(MP_RUNANYWHERE, 0);
			swap_tst_child();
		}
	}
	vaddr = sbrk(LENGTH);

	if ((spid = sproc(swap_tst_sproc, PR_SALL, 0, 0)) < 0) {
		perror("sproc");
		sbrk(-LENGTH);
		return (-1);
	}
	getstat(&rtedat1);
	for (seconds = 0; seconds < swaptst; seconds++) {
		sleep(1);
	}
	getstat(&rtedat2);
	/*
	 * Kill all the children
	 */
	while (--i >= 0)
		kill(fpid[i], SIGTERM);

	kill(spid, SIGTERM);

	sbrk(-LENGTH);

	rte_expected.stat.iso_pfault = 0;
	rte_expected.stat.iso_vfault = 0;
	rte_expected.stat.iso_tlbsync = 0;
	rte_expected.stat.iso_tlbclean = 0;
	rte_expected.stat.iso_delaytlbflush = DONT_CARE;
	rte_expected.stat.iso_delayiflush = DONT_CARE;
	rte_expected.stat.iso_cleanicache = 0;

	return (expected(&rtedat1, &rtedat2, &rte_expected));
}
	
main_loop()
{
	int spid;
	int status;
	int errors = 0;

	setbuf(stdout, NULL);
	/*
	 * Fault the stack in right now
	 */
	stackhack();
	/*
	 * test sbrk
	 */
	if ((status = sbrk_tst()) < 0) {
		fprintf(stderr, "Fatal error in sbrk test\n");
		return (-1);
	}
	errors += status;

	if (status > 0 || verbose) {
		dumpstat(&rtedat1, &rtedat2);
		if (status != 0)
			printf("sbrk test failed!\n");
		else
			printf("sbrk test successful\n");
	}
	/*
	 * test cache flush
	 */
	if ((status = cacheflush_tst()) < 0) {
		fprintf(stderr, "Fatal error in cacheflush test\n");
		return (-1);
	}
	errors += status;

	if (status > 0 || verbose) {
		dumpstat(&rtedat1, &rtedat2);
		if (status != 0)
			printf("cacheflush test failed!\n");
		else
			printf("cacheflush test successful\n");
	}
	/*
	 * test mprotect
	 */
	if ((status = mprotect_tst()) < 0) {
		fprintf(stderr, "Fatal error in mprotect test\n");
		return (-1);
	}
	errors += status;

	if (status > 0 || verbose) {
		dumpstat(&rtedat1, &rtedat2);
		if (status != 0)
			printf("mprotect test failed!\n");
		else
			printf("mprotect test successful\n");
	}
	/*
	 * test map local
	 */
	if ((status = map_local_tst()) < 0) {
		fprintf(stderr, "Fatal error in map local test\n");
		return (-1);
	}
	errors += status;

	if (status > 0 || verbose) {
		dumpstat(&rtedat1, &rtedat2);
		if (status != 0)
			printf("map local test failed!\n");
		else
			printf("map local test successful\n");
	}
	/*
	 * test plock
	 */
	if ((status = plock_tst()) < 0) {
		fprintf(stderr, "Fatal error in plock test\n");
		return (-1);
	}
	errors += status;

	if (status > 0 || verbose) {
		dumpstat(&rtedat1, &rtedat2);
		if (status != 0)
			printf("plock test failed!\n");
		else
			printf("plock test successful\n");
	}
	/*
	 * test mpin
	 */
	if ((status = mpin_tst()) < 0) {
		fprintf(stderr, "Fatal error in mpin test\n");
		return (-1);
	}
	errors += status;

	if (status > 0 || verbose) {
		dumpstat(&rtedat1, &rtedat2);
		if (status != 0)
			printf("mpin test failed!\n");
		else
			printf("mpin test successful\n");
	}
	/*
	 * test mmap faulting
	 */
	if ((status = mmap_fault_tst()) < 0) {
		fprintf(stderr, "Fatal error in mmap fault test\n");
		return (-1);
	}
	errors += status;

	if (status > 0 || verbose) {
		dumpstat(&rtedat1, &rtedat2);
		if (status != 0)
			printf("mmap fault test failed!\n");
		else
			printf("mmap fault test successful\n");
	}
	/*
	 * test sbrk faulting
	 */
	if ((status = sbrk_fault_tst()) < 0) {
		fprintf(stderr, "Fatal error in sbrk fault test\n");
		return (-1);
	}
	errors += status;

	if (status > 0 || verbose) {
		dumpstat(&rtedat1, &rtedat2);
		if (status != 0)
			printf("sbrk fault test failed!\n");
		else
			printf("sbrk fault test successful\n");
	}
	/*
	 * test sbrk faulting
	 */
	if (swaptst) {
		if ((status = swap_tst()) < 0) {
			fprintf(stderr, "Fatal error in swap test\n");
			return (-1);
		}
		errors += status;

		if (status > 0 || verbose) {
			dumpstat(&rtedat1, &rtedat2);
			if (status != 0)
				printf("swap test failed!\n");
			else
				printf("swap test successful\n");
		}
	}
	return (errors);
}

main(int argc, char *argv[])
{
	int i;
	int ofd;
	int dfd;
   	extern int		optind;
   	extern char		*optarg;
	int			c;
	int 			err = 0;
	int			exit_stat;
	char *tmpfile;
	/*
	 * Parse arguments.
	 */
	setbuf(stdout, NULL);
	while ((c = getopt(argc, argv, "os:p:c:v")) != EOF) {
	  	switch (c) {
		case 'o':
			/*
			 * Kernel implements newptes opt
			 */
			newptes_opt = 1;
			break;
		case 'c':
			/*
			 * Child cpu
			 */
			ccpu = atoi(optarg);
			break;
		case 'p':
			/*
			 * Parent cpu
			 */
			pcpu = atoi(optarg);
			break;
		case 's':
			/*
			 * Execute swap test
			 */
			swaptst = atoi(optarg);
			break;
		case 'v':
			verbose = 1;
			break;
		default:
			err++;
			break;
	 	}
     	}
	if (err) {
		fprintf(stderr,
			"Usage -- %s [-ov][-c child_cpu][-p parent_cpu][-s seconds]\n",
				argv[0]);
		exit (1);
	}
	if (verbose) {
		printf("parent cpu %d, child cpu %d\n", pcpu, ccpu);
	}
	/*
	 * Initialize the statistics
	 */
	if ((rtefd = open("/dev/rte", O_RDONLY)) < 0) {
		perror("/dev/rte");
		exit (1);
	}
	if (sysmp(MP_MUSTRUN, pcpu) < 0) {
		perror("mustrun");
		goto done;
	}
	if (sysmp(MP_ISOLATE, pcpu) < 0) {
		perror("isolate");
		exit (1);
	}
	if (sysmp(MP_NONPREEMPTIVE, pcpu) < 0) {
		perror("isolate");
		exit (1);
	}
	/*
	 * Initialize lock/semaphore arena
	 */
	if ((handle = usinit(tempnam(NULL, "sysmp"))) == NULL) {
		perror("usinit");
		goto done;
	}
	(void)usconfig(CONF_INITUSERS, 100);
	sprocsema = usnewsema(handle, 0);
	parentsema = usnewsema(handle, 0);
	/*
	 * Get manifest system constants
	 */
	pagesize = sysconf(_SC_PAGESIZE);
	/*
	 * Create a file for memory mapping
	 */
	tmpfile = tempnam(NULL, "mmap");
	mapfd = open(tmpfile, O_RDWR|O_CREAT);
	if (mapfd < 0) {
		perror("open");
		goto done;
	}
	if (lseek(mapfd, LENGTH-1, SEEK_SET) < 0) {
		perror("lseek");
		goto done;
	}
	if (write(mapfd, &mapfd, 1) != 1) {
		perror("write");
		goto done;
	}
	exit_stat = main_loop();

	sysmp(MP_PREEMPTIVE, pcpu);
	sysmp(MP_UNISOLATE, pcpu);

	exit (exit_stat);
done:
	(void)sysmp(MP_UNISOLATE, pcpu);
	(void)unlink(tmpfile);
	exit(-1);
}
