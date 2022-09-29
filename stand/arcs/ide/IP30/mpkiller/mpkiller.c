/*
 * Main program for SpeedRacer (IP30) standalone mpkiller tests
 */
/* $Id: mpkiller.c,v 1.4 1996/11/18 21:57:29 kkn Exp $ */

#include <sys/cpu.h>
#include <sys/sbd.h>
#include <libsk.h>
#include <libsc.h>

struct return_status {
    long long status;	/* 0x00 */
    long long count;	/* 0x08 */
    long long address;	/* 0x10 */
    long long expect;	/* 0x18 */
    long long actual;	/* 0x20 */
};

typedef struct argdesc_s {
	int argc;
	char **argv;
} argdesc_t;

extern void MPK_TLB_Refill();
extern void MPK_XTLB_Refill();
extern void MPK_CacheError();
extern void MPK_Others_Excs();

extern int mpk_test(volatile struct return_status *result);

volatile struct return_status cpu0_status = { 0UL, 0UL, 0UL, 0UL, 0UL };
volatile struct return_status cpu1_status = { 0UL, 0UL, 0UL, 0UL, 0UL };

static long long stack[128];

extern int reboot_cmd(int argc, char **argv); 
extern int _setenv (char *name, char *value, int override);

void
lame(void* notused) {
  printf("slave: going into oo loop\n");
  while(1) ; 
}

/*
 * entry pt
 */
int
main(int argc, char** argv) {

    int num_cpus = 0;
    int cpu_id = 0;
    unsigned status = 0;
    long long x = 12345678;

    printf("*******************\n");
    printf("** MPKiller Test **\n");
    printf("*******************\n");

    cpu0_status.status = 0xdeadbeef0;
    cpu1_status.status = 0xdeadbeef1;

    /*
     * Need more than one CPU.
     */
    if ((num_cpus = cpu_probe()) < 2) {
	printf("Only %d CPU detected!\n", num_cpus);
    }
    else
    	printf("Number of cpus found: %d\n", num_cpus);
    printf("Master cpu id: %d\n", cpuid());

    /*
     * Start the slave test.
     */
    if (num_cpus > 1) {
	printf("======================================================\n");
	printf("Launching slave - ");
	cpu_id = cpuid() + 1;
	launch_slave(cpu_id,
     		 (void (*)(void *))mpk_test, (void *)&cpu1_status,
                     (void (*)(void *))lame, NULL, &stack[128]);
    }

    /* wait until slave is started */
    while (cpu1_status.status == 0xdeadbeef1) {
		x = x / (long long)cpu1_status.status;
		x = x * (long long)cpu0_status.status;
    }
    printf("slave is alive.\n");

    /*
     * Install the mpkiller handlers.
     */
    printf("Installing mpkiller handlers.\n");
    _save_exceptions(); /* saves 30 instructions for each vector location */

    bcopy((char *)MPK_TLB_Refill,  (char *)UT_VEC, 0x80);
    bcopy((char *)MPK_CacheError,  (char *)XUT_VEC,0x80);
    bcopy((char *)MPK_Others_Excs, (char *)ECC_VEC,0x80);
    bcopy((char *)MPK_XTLB_Refill, (char *)E_VEC,  0x80);
    clear_cache((void *)UT_VEC,  0x80);
    clear_cache((void *)XUT_VEC, 0x80);
    clear_cache((void *)ECC_VEC, 0x80);
    clear_cache((void *)E_VEC,   0x80);

    printf("Starting master and slave.\n");
    cpu1_status.status = 0xbeefbeef1;
    status = mpk_test(&cpu0_status);

    printf(" CPU 0 - loop cnt: %d addr: %llx xor: %llx act: r%-2d\n",
	cpu0_status.count, cpu0_status.address, 
	cpu0_status.expect, cpu0_status.actual);
    printf(" CPU 1 - loop cnt: %d addr: %llx xor: %llx act: r%-2d\n",
	cpu1_status.count, cpu1_status.address,
	cpu1_status.expect, cpu1_status.actual);

    /* printf("Restore handlers.\n-----------------\n"); */
    _restore_exceptions();

    /* release slave to return */
    cpu1_status.status = 0xcafebeef1;

    /* wait until slave returns */
    while (cpu1_status.status == 0xcafebeef1) {
		x = x / (long long)cpu1_status.status;
		x = x * (long long)cpu0_status.status;
    }

    if (!status) {
	printf("MPKiller Test result - PASSED \n");
    	setenv_nvram("AutoLoad", "Yes");
    	{ /* add some delay */
		int i;
		for(i=0; i<0x100000; i++);
    	}
    	setenv_nvram("diagmode", "");
    	{ /* add some delay */
		int i;
		for(i=0; i<0x100000; i++);
    	}
    	_setenv("idebinary", "", 2);
    	{ /* add some delay */
		int i;
		for(i=0; i<0x100000; i++);
    	}
    } else {
	printf("MPKiller Test result - FAILED @ addr=0x%08x\n", status);
    }

    reboot_cmd(1, NULL);

    return(0); /* SHOULD NEVER GET HERE */

}
