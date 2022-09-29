#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <bstring.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <stdio.h>
#include <getopt.h>
#include <sys/prctl.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/syssgi.h>
#include "stress.h"

/*
 * Test for sproc local regions.  Here we create a group of shared address
 * addresses and do a variety of process local mappings to make sure 
 * the shadow code in the kernel maps the correct things into each shared
 * process's address space.  We do unit tests for some specific cases and
 * then do a bunch of random mappings as a stress tests.
 *
 * This program uses knowledge of when and how the kernel creates shadows
 * (otherwise we wouldn't know which kinds of local mappings need testing).
 * This programs knows the following things about the kernel's handling
 * of sproc local mappings:
 *
 *	shadows are created when there is a non-fully overlapping local
 *	region in the same segment as a shared region.
 *
 *	a segment is a page of page table entries - currently we
 *	assume 4 bytes per pte.
 */

#define PASS	1
#define FAIL	0

int pagesz;
long segsz;
int Verbose;
int debugger;
int Silent;
int Iterations = 5;
int NumChildren = 5;
#define NUMTRIALS	5

#define Message if (Verbose) printf
#define Printf if (!Silent) printf

char *ArenaStart;
extern int _end;
char *Cmd;

typedef unsigned int GenMap_t;
#define GENMAPSIZE ((8*segsz / pagesz) * sizeof(GenMap_t))
GenMap_t *ShrdGenMap;

void SetUp(void);
int UnitTests(void);
int StressTests(void);
char *mapit(char *, int, int, int, int, off_t);
void unmapit(char *, int);
void DoUnitTests(void *);
GenMap_t *SetUpGenMap(void);
void SetGen(int *, GenMap_t *, char *, int);
void ClrGen(GenMap_t *, char *, int);
void ChkGen(GenMap_t *, GenMap_t *, char *, int);
int inlocal(char *);

int
main(int argc, char *argv[])
{
	int c;
	int errflg = 0;

	Cmd = errinit(argv[0]);
	while ((c = getopt(argc, argv, "dvsi:c:")) != -1)
		switch (c) {
		case 'd':
			debugger++;
			break;
		case 'v':
			Verbose++;
			break;
		case 's':
			Silent++;
			break;
		case 'i':
			Iterations = atoi(optarg);
			break;
		case 'c':
			NumChildren = atoi(optarg);
			break;
		case '?':
			errflg++;
			break;
		}

	if (errflg) {
		fprintf(stderr, "Usage: shadow [-v] [-i # of iterations] [-c # of children]\n");
		exit(1);
	}

	/*
	 * We use a two segment arena for our tests.  This is big enough
	 * to have some interested boundary cases while being small enough
	 * to run the tests in a reasonable amount of time.  The arena will
	 * start at the segment boundary past the end of bss.  There better
	 * not be anything mapped here or these tests won't work.
	 */
	pagesz = getpagesize();
	segsz = (pagesz / 4) * pagesz;

	ArenaStart = (char *)(((long)&_end & ~(segsz - 1)) + segsz);
	Message("Arena start:0x%x\n", ArenaStart);
	SetUp();

	/*
	 * Do the tests
	 */
	while (Iterations--) {
		if (UnitTests() == FAIL)
			exit(1);

		if (StressTests() == FAIL)
			exit(2);
	}
	printf("shadows:%d:PASSED\n", getpid());

	exit(0);
}

/*
 * Get some mappings in interesting places.  These will become
 * shared mappings over which we'll put local mappings to create
 * shadows.
 *
 * ShrdMapping[0] - 4 pages starting on segment boundary
 * ShrdMapping[1] - 4 pages starting in middle of segment
 * ShrdMapping[2] - 4 pages straddling segment boundary
 * ShrdMapping[3] - 4 pages ending on segment boundary
 * ShrdMapping[4] - 4 pages in middle of 3rd segment
 * ...
 *
 * The following are offsets from ArenaStart which will be relocated at
 * runtime.
 */

struct MapEntry {
	char	*Addr;		/* Starting address of mapping */
	int	Len;		/* size of mapping in bytes */
	char	comment[80];
};

struct MapEntry ShrdMapping[9];
int NumShrdMappings = sizeof(ShrdMapping) / sizeof(struct MapEntry);

/*
 * This is the set of local mappings that will overlay the shared ones
 * defined above.
 */
struct MapEntry LclMapping[] = {
	0, 0, "Case 1: maplocal in middle of non-boundary shared mapping\n",
	0, 0, "Case 2: maplocal in middle of seg boundary shared mapping\n",
	0, 0, "Case 3: maplocal at start of seg boundary shared mapping\n",
	0, 0, "Case 4: maplocal at end of seg boundary shared mapping\n",
	0, 0, "Case 5: maplocal over beginning of shared mapping\n",
	0, 0, "Case 6: maplocal over beginning of shared mapping at end of seg\n",
};
int NumLclMappings = sizeof(LclMapping) / sizeof(struct MapEntry);

/*
 * Perform one-time mapping relocations.
 */
void
SetUp(void)
{
	int i;

	for (i = 0; i < NumShrdMappings; i++) {
		ShrdMapping[i].Len = 4*pagesz;
	}
	ShrdMapping[0].Addr = ArenaStart + 0;
	ShrdMapping[1].Addr = ArenaStart + (32*pagesz);
	ShrdMapping[2].Addr = ArenaStart + (segsz - 2*pagesz);
	ShrdMapping[3].Addr = ArenaStart + (2*segsz - 4*pagesz);
	ShrdMapping[4].Addr = ArenaStart + (3*segsz + 4*pagesz);
	ShrdMapping[5].Addr = ArenaStart + (4*segsz + 40*pagesz);
	ShrdMapping[6].Addr = ArenaStart + (5*segsz + 50*pagesz);
	ShrdMapping[7].Addr = ArenaStart + (6*segsz + 77*pagesz);
	ShrdMapping[8].Addr = ArenaStart + (7*segsz + 83*pagesz);

	for (i = 0; i < NumLclMappings; i++)
		LclMapping[i].Len += 2*pagesz;

	LclMapping[0].Addr = ArenaStart + 33*pagesz;
	LclMapping[1].Addr = ArenaStart + pagesz;
	LclMapping[2].Addr = ArenaStart + 0;
	LclMapping[3].Addr = ArenaStart + 2*pagesz;
	LclMapping[4].Addr = ArenaStart + segsz-3*pagesz;
	LclMapping[5].Addr = ArenaStart + segsz-2*pagesz;
}

/*
 * Perform a set of unit tests for some interesting boundary cases.
 */
int
UnitTests(void)
{
	int fd;
	int cpid;
	int i;
	int status;
	int errors = 0;
	int Generation = getpid()<<8 | 1;

	Message("Starting Unit Tests...\n");

	if ((fd = open("/dev/zero", O_RDWR)) == -1) {
		perror("/dev/zero");
		exit(-1);
	}

	ShrdGenMap = SetUpGenMap();

	for (i = 0; i < NumShrdMappings; i++) {
		Message("Set up ShrdMapping %d @0x%x\n", i, ShrdMapping[i].Addr);
		(void) mapit(ShrdMapping[i].Addr, ShrdMapping[i].Len, 
			PROT_READ | PROT_WRITE,
			MAP_SHARED | MAP_FIXED, fd, 0);
		SetGen(&Generation, ShrdGenMap, ShrdMapping[i].Addr, ShrdMapping[i].Len);
	}

	/*
	 * Sproc a group of children to run the tests.
	 */

	for (i = 0; i < NumChildren; i++)
		if ((cpid = sproc(DoUnitTests, PR_SADDR, fd)) == -1) {
			perror("UnitTest sproc");
			exit(-1);
		}

	for (i = 0; i < NumChildren; i++) {
		pid_t pid;

		while (((pid = wait(&status)) < 0) || errno == EINTR)
			;

		if (!WIFEXITED(status) || WEXITSTATUS(status) != PASS) {
			Printf("Child Failed Running Unit Tests pid %d status 0x%x\n",
				pid, status);
			errors++;
		}
	}

	/*
	 * Make sure shared mappings are still OK.
	 */
	Message("Checking shrd mapping gen numbers\n");

	for (i = 0; i < NumShrdMappings; i++)
		ChkGen(ShrdGenMap, NULL, ShrdMapping[i].Addr, ShrdMapping[i].Len);

	/*
	 * Tear down shared mappings.
	 */
	for (i = 0; i < NumShrdMappings; i++) {
		Message("Unmap ShrdMapping %d\n", i);
		unmapit(ShrdMapping[i].Addr, ShrdMapping[i].Len);
	}

	close(fd);

	if (errors) {
		Printf("Unit Tests Failed\n");
		return FAIL;
	} else {
		Message("Unit Tests Complete.  All Tests Passed.\n");
		return PASS;
	}
}


/*
 * Perform unit tests.  This is run as an sproc child.  It makes private
 * mappings that will create shadows of the global mappings the parent
 * set up.
 */

void
DoUnitTests(void *arg)
{
	int fd = (int)(long) arg;
	int i, shd, trials;
	char *Local;
	GenMap_t *LclGenMap;
	int Generation;

	/*
	 * Set up a gen map for our local mappings.
	 */

	LclGenMap = SetUpGenMap();

	Generation = getpid() << 16 | 1;

	for (trials = 0; trials < NUMTRIALS; trials++) {
		Message("Unit Test Trial %d...\n", trials);

		/*
		 * Test 'em.
		 */
		for (i = 0; i < NumLclMappings; i++) {
			Message(LclMapping[i].comment);
			Local = mapit(LclMapping[i].Addr, LclMapping[i].Len, 
				PROT_READ | PROT_WRITE,
				MAP_LOCAL | MAP_PRIVATE | MAP_FIXED, fd, 0);
			Message("%d:local mapping at 0x%x len 0x%x\n",
					getpid(),
					LclMapping[i].Addr,
					LclMapping[i].Len);
				
	
			SetGen(&Generation, LclGenMap, LclMapping[i].Addr, LclMapping[i].Len);
	
			for (shd = 0; shd < NumShrdMappings; shd++)
				ChkGen(ShrdGenMap, LclGenMap, ShrdMapping[shd].Addr, ShrdMapping[shd].Len);
	
			ChkGen(ShrdGenMap, LclGenMap, LclMapping[i].Addr, LclMapping[i].Len);
	
			/*
			 * Note that munmap here will unmap BOTH
			 * the local mapping and the shared mapping underneath
			 */
			unmapit(LclMapping[i].Addr, LclMapping[i].Len);
			ClrGen(LclGenMap, LclMapping[i].Addr, LclMapping[i].Len);
		}
	}
	exit(PASS);
}

/*
 * Initial the shared gen map
 */
GenMap_t *
SetUpGenMap()
{
	GenMap_t *map;

	map = (GenMap_t *)malloc(GENMAPSIZE);
	bzero((void *)map, GENMAPSIZE);
	return map;
}


/*
 * Write a unique generation number into each of the given pages.
 */

void
SetGen(int *Generation, GenMap_t *map, char *addr, int len)
{
	int pgno = (addr - ArenaStart) / pagesz;

	while (len) {
		*(GenMap_t *)addr = *Generation;
		map[pgno] = *Generation;
		*Generation++;
		addr += pagesz;
		pgno++;
		len -= pagesz;
	}
}


/*
 * Clear gen map entries
 */
void
ClrGen(GenMap_t *map, char *addr, int len)
{
	int pgno = (addr - ArenaStart) / pagesz;

	while (len) {
		map[pgno] = 0;
		addr += pagesz;
		pgno++;
		len -= pagesz;
	}
}

/*
 * Check the generation numbers on the given set of shared pages.
 * Since the underlying shared map gets unmapped when any thread
 * unmaps a local map, we can't really check anything in the shared
 * map that MIGHT be in a local map
 */
void
ChkGen(GenMap_t *shrdmap, GenMap_t *lclmap, char *StartAddr, int Size)
{
	int len = Size;
	char *addr = StartAddr;
	int pgno = (addr - ArenaStart) / pagesz;
	int tgen;

	while (len) {
		if (lclmap && (tgen = lclmap[pgno])) {
			if (*(GenMap_t *)addr != tgen) {
				errprintf(ERR_RET, "ChkGen Failed (lclmap): addr 0x%x, found 0x%x, expected 0x%x\n(when checking block at 0x%x, len 0x%x)",
				    addr, *(GenMap_t *)addr,
				    lclmap[pgno], StartAddr, Size);
				if (debugger)
					syssgi(SGI_IDBG, 3);
				abort();
			}
		} else if (!inlocal(addr) && *(GenMap_t *)addr != shrdmap[pgno]) {
			errprintf(ERR_RET, "ChkGen Failed (shrdmap): addr 0x%x, found 0x%x, expected 0x%x\n(when checking block at 0x%x, len 0x%x)",
				addr, *(GenMap_t *)addr,
				shrdmap[pgno], StartAddr, Size);
			if (debugger)
				syssgi(SGI_IDBG, 3);
			abort();
		}

		addr += pagesz;
		pgno++;
		len -= pagesz;
	}
}

int
inlocal(char *addr)
{
	int i;

	for (i = 0; i < NumLclMappings; i++) {
		if (addr >= LclMapping[i].Addr &&
		    addr < (LclMapping[i].Addr + LclMapping[i].Len))
		return 1;
	}
	return 0;
}

int
StressTests()
{
	return PASS;
}


/*	
 * Mmap with error checking.
 */

char *
mapit(char *addr, int len, int prot, int flags, int fd, off_t off)
{
	char *result;

	if ((result = (char *)mmap((void *)addr, len, prot, flags, fd, off)) ==
		(char *) -1L) {
		errprintf(ERR_ERRNO_EXIT, "when trying to map addr 0x%x, len 0x%x, flags 0x%x",
			addr, len, flags);
		/* NOTREACHED */
	}

	if ((flags & MAP_FIXED) && result != addr) {
		errprintf(ERR_EXIT, "mmap MAP_FIXED failed to map at 0x%x, returned 0x%x\n",
			addr, result);
		/* NOTREACHED */
	}

	return result;
}


/*
 * Munmap with error checking.
 */
void
unmapit(char *addr, int len)
{
	if (munmap((void *)addr, len) == -1) {
		errprintf(ERR_ERRNO_EXIT, "when trying to unmap addr 0x%x, len 0x%x\n",
			addr, len);
		/* NOTHREACHED */
	}
}
