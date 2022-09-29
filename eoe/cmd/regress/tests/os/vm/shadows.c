#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdio.h>
#include <getopt.h>
#include <sys/prctl.h>
#include <sys/signal.h>
#include <sys/wait.h>
#include <sys/syssgi.h>

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
 *	a segment is 4 Mb and is aligned on a 4 Mb boundary.
 *
 *	pages are 4K
 */

#define PASS	1
#define FAIL	0

#define SEGSZ	(4*1024*1024)
#define PAGESZ	(4*1024)

int Verbose = 0;
int Silent = 0;
int Iterations = 5;
int NumChildren = 5;
#define NUMTRIALS	5

#define Message if (Verbose) printf
#define Printf if (!Silent) printf

char *ArenaStart;
extern int _end;

typedef unsigned int	GenMap_t;
#define GENMAPSIZE ((2*SEGSZ / PAGESZ) * sizeof(GenMap_t))
GenMap_t	*ShrdGenMap;

void SetUp();
int UnitTests();
int StressTests();
char *mapit(char *, int, int, int, int, off_t);
void unmapit(char *, int);
void DoUnitTests(void *);
GenMap_t *SetUpGenMap();
void SetGen(int *, GenMap_t *, char *, int);
void ClrGen(GenMap_t *, char *, int);
int ChkGen(GenMap_t *, GenMap_t *, char *, int);


main(int argc, char *argv[])
{
	int c;
	int errflg = 0;

	while ((c = getopt(argc, argv, "vsi:c:")) != -1)
		switch (c) {
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

	ArenaStart = (char *)(((int)&_end & ~(SEGSZ - 1)) + SEGSZ);
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
 *
 * The following are offsets from ArenaStart which will be relocated at
 * runtime.
 */

struct MapEntry {
	char	*Addr;		/* Starting address of mapping */
	int	Len;		/* size of mapping in bytes */
	char	comment[80];
};

struct MapEntry ShrdMapping[] = {
	(char *) 0,			4*PAGESZ,	"",
	(char *) (32*PAGESZ),		4*PAGESZ,	"",
	(char *) (SEGSZ - 2*PAGESZ),	4*PAGESZ,	"",
	(char *) (2*SEGSZ - 4*PAGESZ),	4*PAGESZ,	"",
};

int NumShrdMappings = sizeof(ShrdMapping) / sizeof(struct MapEntry);


/*
 * This is the set of local mappings that will overlay the shared ones
 * defined above.
 */

struct MapEntry LclMapping[] = {
	(char *)(33*PAGESZ),	2*PAGESZ,
		"Case 1: maplocal in middle of non-boundary shared mapping\n",

	(char *)(PAGESZ),	2*PAGESZ,
		"Case 2: maplocal in middle of seg boundary shared mapping\n",

	(char *)0,		2*PAGESZ,
		"Case 3: maplocal at start of seg boundary shared mapping\n",

	(char *)(2*PAGESZ),	2*PAGESZ,
		"Case 4: maplocal at end of seg boundary shared mapping\n",

	(char *)(SEGSZ-3*PAGESZ), 2*PAGESZ,
		"Case 5: maplocal over beginning of shared mapping\n",

	(char *)(SEGSZ-2*PAGESZ), 2*PAGESZ,
		"Case 6: maplocal over beginning of shared mapping at end of seg\n",

};

int NumLclMappings = sizeof(LclMapping) / sizeof(struct MapEntry);


/*
 * Perform one-time mapping relocations.
 */

void
SetUp()
{
	int i;

	for (i = 0; i < NumShrdMappings; i++)
		ShrdMapping[i].Addr += (int) ArenaStart;

	for (i = 0; i < NumLclMappings; i++)
		LclMapping[i].Addr += (int) ArenaStart;
}

/*
 * Perform a set of unit tests for some interesting boundary cases.
 */

int
UnitTests()
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
		Message("Set up ShrdMapping %d\n", i);
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
		wait(&status);

		if (!WIFEXITED(status) || WEXITSTATUS(status) != PASS) {
			Printf("Child Failed Running Unit Tests\n");
			errors++;
		}
	}

	/*
	 * Make sure shared mappings are still OK.
	 */

	Message("Checking shrd mapping gen numbers\n");

	for (i = 0; i < NumShrdMappings; i++)
		if (ChkGen(ShrdGenMap, NULL, ShrdMapping[i].Addr, ShrdMapping[i].Len) == FAIL)
			errors++;

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
DoUnitTests(void *fd)
{
	int i, shd, trials;
	char *Local;
	int errors = 0;
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
				MAP_LOCAL | MAP_PRIVATE | MAP_FIXED, (int)fd, 0);
	
			SetGen(&Generation, LclGenMap, LclMapping[i].Addr, LclMapping[i].Len);
	
			for (shd = 0; shd < NumShrdMappings; shd++)
				if (ChkGen(ShrdGenMap, LclGenMap, ShrdMapping[shd].Addr, ShrdMapping[shd].Len) == FAIL)
					errors++;
	
			if (ChkGen(ShrdGenMap, LclGenMap, LclMapping[i].Addr, LclMapping[i].Len) == FAIL)
				errors++;

#if never

			/* Unfortunately, we can't use this code anymore.
			 * The old definition of munmap would remove only
			 * private mappings and leave underlying shared
			 * ones alone.  The new defintion causes it to
			 * remove everything.
			 */

			unmapit(LclMapping[i].Addr, LclMapping[i].Len);
			ClrGen(LclGenMap, LclMapping[i].Addr, LclMapping[i].Len);
#endif
		}
	
		/*
		 * Do all mappings at together. 
		 */
	
	/*******************************************************************
		This doesn't really work too well since they overlap.
		Maybe just leave all this to the stress part of the test
	
		Message("All Together Now..\n");
	
		for (i = 0; i < NumLclMappings; i++) {
			Message(LclMapping[i].comment);
			(void) mapit(LclMapping[i].Addr, LclMapping[i].Len, 
				PROT_READ | PROT_WRITE,
				MAP_LOCAL | MAP_PRIVATE | MAP_FIXED, fd, 0);
	
			SetGen(LclGenMap, LclMapping[i].Addr, LclMapping[i].Len);
		}
	
		for (i = 0; i < NumLclMappings; i++) 
			if (ChkGen(ShrdGenMap, LclGenMap, LclMapping[i].Addr, LclMapping[i].Len) == FAIL)
				errors++;
	
		if (ChkGen(ShrdGenMap, LclGenMap, LclMapping[i].Addr, LclMapping[i].Len) == FAIL)
			errors++;
	
		Message("Unmapping all...\n");
	
		for (i = 0; i < NumLclMappings; i++) {
			Message(LclMapping[i].comment);
			unmapit(LclMapping[i].Addr, LclMapping[i].Len);
			ClrGen(LclGenMap, LclMapping[i].Addr, LclMapping[i].Len);
		}
		
		if (ChkGen(ShrdGenMap, LclGenMap, LclMapping[i].Addr, LclMapping[i].Len) == FAIL)
			errors++;
	**************************************************************************/

		if (errors)
			break;
	}

	if (errors)
		exit(FAIL);
	else
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
	int pgno = (addr - ArenaStart) / PAGESZ;

	while (len) {
		*(GenMap_t *)addr = *Generation;
		map[pgno] = *Generation;
		(*Generation)++;
		addr += PAGESZ;
		pgno++;
		len -= PAGESZ;
	}
}


/*
 * Clear gen map entries
 */

void
ClrGen(GenMap_t *map, char *addr, int len)
{
	int pgno = (addr - ArenaStart) / PAGESZ;

	while (len) {
		map[pgno] = 0;
		addr += PAGESZ;
		pgno++;
		len -= PAGESZ;
	}
}

/*
 * Check the generation numbers on the given set of shared pages.
 */

int
ChkGen(GenMap_t *shrdmap, GenMap_t *lclmap, char *StartAddr, int Size)
{
	int len = Size;
	char *addr = StartAddr;
	int errors = 0;
	int pgno = (addr - ArenaStart) / PAGESZ;

	while (len) {
		if (lclmap && lclmap[pgno]) {
			if (*(GenMap_t *)addr != lclmap[pgno]) {
			    Printf("ChkGen Failed (lclmap): addr 0x%x, found 0x%x, expected 0x%x\n(when checking block at 0x%x, len 0x%x)\n",
				    addr, *(GenMap_t *)addr, lclmap[pgno], StartAddr, Size);
			    errors++;
			    syssgi(SGI_IDBG, 3);
			}

		} else if (*(GenMap_t *)addr != shrdmap[pgno]) {
			Printf("ChkGen Failed (shrdmap): addr 0x%x, found 0x%x, expected 0x%x\n(when checking block at 0x%x, len 0x%x)\n",
				addr, *(GenMap_t *)addr, shrdmap[pgno], StartAddr, Size);
			errors++;
			syssgi(SGI_IDBG, 3);
		}

		addr += PAGESZ;
		pgno++;
		len -= PAGESZ;
	}

	if (errors)
		return FAIL;
	else
		return PASS;
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
		(char *) -1) {
		perror("mmap");
		fprintf(stderr, "when trying to map addr 0x%x, len 0x%x, flags 0x%x\n",
			addr, len, flags);
		exit(-1);
	}

	if ((flags & MAP_FIXED) && result != addr) {
		Printf("mmap MAP_FIXED failed to map at 0x%x, returned 0x%x\n",
			addr, result);
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
		perror("munmap");
		fprintf(stderr, "when trying to unmap addr 0x%x, len 0x%x\n",
			addr, len);
		exit(-1);
	}
}
