#include <stdio.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/immu.h>
#include <sys/mman.h>
#include <sys/sysmp.h>
#include <sys/pda.h>

/*
 * Unit test for executing instructions out of data areas.
 *
 */

#define PASS	1
#define FAIL	0

#define Message if (Verbose) printf
#define Debug	if (Dbg) printf

int Verbose = 0;
int No_mprotect = 0;
int Dbg = 0;
int Iterations = 5;

int func1();

static int pagesize;
#undef NBPP
#undef NBPC
#define NBPP	pagesize
#define NBPC	pagesize

main(int argc, char *argv[])
{
	int c;
	int errflg = 0;

	pagesize = getpagesize();
	while ((c = getopt(argc, argv, "vndi:")) != -1)
		switch (c) {
		case 'v':
			Verbose++;
			break;
		case 'n':
			No_mprotect++;
			break;
		case 'd':
			Dbg++;
			break;
		case 'i':
			Iterations = atoi(optarg);
			break;
		case '?':
			errflg++;
			break;
		}

	if (errflg) {
		fprintf(stderr, "Usage: mexec [-v] [-n]\n");
		exit(1);
	}

	if (Verbose)
		fprintf(stderr, "WARNING: running with verbose on tends to invalidate the results since\ncalling into the kernel for the writes can invalidate the i cache\n\n");


	while (Iterations--) {
		if (Unit_tests() == FAIL) {
			printf("\n*** UNIT TESTS FAILED ***\n\n");
			exit(1);
		}
	}

	printf("mexec: All tests passed\n");
	exit(0);
}


/*
 * buffer in bss where instructions will be stored.
 */

char buf[512];

#define FUNC_SIZE 100	/* can't do sizeof(func), so we guess */
#define FUNC1_VALUE 42
#define FUNC2_VALUE 24

int func1();
int func2();

Unit_tests()
{
	int errors = 0;
	int (*func_ptr)();
	int num_cpus, cpu;
	struct pda_stat *pdap;

	num_cpus = do_sysmp(MP_NPROCS);
	pdap = (struct pda_stat *)malloc(sizeof(struct pda_stat) * num_cpus);
	do_sysmp(MP_STAT, pdap);

	/*
	 * Go to each cpu, one at a time, and try a simple test
	 */

	for (cpu = 0; cpu < num_cpus; cpu++) {
		if ((pdap[cpu].p_flags & PDAF_ENABLED) == 0)
			continue;

		do_sysmp(MP_MUSTRUN, cpu);
		Message("Running on cpu %d now...\n", cpu);

		Message("Executing func1 out of bss...\n");
	
		memcpy(buf, func1, FUNC_SIZE);
		func_ptr = (int(*)())buf;
	
		do_mprotect(buf, FUNC_SIZE, PROT_READ|PROT_WRITE|PROT_EXEC);
	
		if (func_ptr() != FUNC1_VALUE) {
			fprintf(stderr, "case 1: func1 returned wrong value on cpu %d\n",
				cpu);
			errors++;
		}
	
		Message("Executing func2 out of bss...\n");
		memcpy(buf, func2, FUNC_SIZE);
	
		do_mprotect(buf, FUNC_SIZE, PROT_READ|PROT_WRITE|PROT_EXEC);
	
		if (func_ptr() != FUNC2_VALUE) {
			fprintf(stderr, "case 1: func2 returned wrong value on cpu %d\n",
				cpu);
			errors++;
		}
	}

	/*
	 * Now try dirty'ing the i-cache on each cpu, then changing it one.
	 * Then go back and the others to make sure they got flushed as
	 * well.
	 */

	Message("Dirty all caches, then change on one...\n");

	memcpy(buf, func1, FUNC_SIZE);
	func_ptr = (int(*)())buf;

	do_mprotect(buf, FUNC_SIZE, PROT_READ|PROT_WRITE|PROT_EXEC);
	
	for (cpu = 0; cpu < num_cpus; cpu++) {
		if ((pdap[cpu].p_flags & PDAF_ENABLED) == 0)
			continue;

		do_sysmp(MP_MUSTRUN, cpu);
		Message("Running on cpu %d now...\n", cpu);

		if (func_ptr() != FUNC1_VALUE) {
			fprintf(stderr, "case 2: func1 returned wrong value on cpu %d\n",
				cpu);
			errors++;
		}
	}

	Message("Changing to func2...\n");

	memcpy(buf, func2, FUNC_SIZE);
	func_ptr = (int(*)())buf;

	do_mprotect(buf, FUNC_SIZE, PROT_READ|PROT_WRITE|PROT_EXEC);

	Message("Checking all cpus for func2 code...\n");

	for (cpu = 0; cpu < num_cpus; cpu++) {
		if ((pdap[cpu].p_flags & PDAF_ENABLED) == 0)
			continue;

		do_sysmp(MP_MUSTRUN, cpu);
		Message("Running on cpu %d now...\n", cpu);

		if (func_ptr() != FUNC2_VALUE) {
			fprintf(stderr, "case 2: func2 returned wrong value on cpu %d\n",
				cpu);
			errors++;
		}
	}

	if (errors)
		return FAIL;
	else
		return PASS;

}

/*
 * Round addr to page boundary and adjust len as a convenience to the
 * caller.  Also check for errors.
 */

do_mprotect(caddr_t addr, size_t len, int prot)
{
	int offset;

	/*
	 * To see whether the tests are really generating stale i-cache
	 * entries, it's useful to shut off the mprotects and make sure
	 * the tests fail.
	 */

	if (No_mprotect)
		return;

	offset = (ulong)addr & POFFMASK;
	addr = (caddr_t)((ulong)addr & ~POFFMASK);
	len += offset;

	if (mprotect(addr, len, prot) == -1) {
		fprintf(stderr, "mprotect(0x%x, 0x%x, 0x%x) failed\n",
			addr, len, prot);
		perror("mprotect");
	}
}


do_sysmp(cmd, arg1)
int cmd, arg1;
{
	int rval;

	if ((rval = sysmp(cmd, arg1)) == -1) {
		fprintf(stderr, "sysmp failed, cmd 0x%x, arg 0x%x\n",
			cmd, arg1);
		perror("sysmp");
	}

	return rval;
}

func1()
{
	return FUNC1_VALUE;

}

func2()
{
	return FUNC2_VALUE;
}
