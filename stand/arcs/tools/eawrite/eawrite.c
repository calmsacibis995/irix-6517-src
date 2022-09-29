#include <sys/types.h>
#include <sys/sbd.h> 
#include <sys/cpu.h>
#include <sys/loaddrs.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/evintr.h>
#include <sys/EVEREST/evmp.h>
#include <sys/EVEREST/evconfig.h>

#define DELAY	delay(50)

extern void sa_store_double(char *, int);
extern unsigned int sa_load_double(char *);
int launch_slave(int, void (*)(int), int, void (*)(int), int, void *);
void fix_earom();

main() {
	int cpu;

	for (cpu = 0; cpu < EV_MAX_CPUS; cpu++) {
		
		if (MPCONF[cpu].mpconf_magic != MPCONF_MAGIC ||
					   MPCONF[cpu].virt_id != cpu)
			continue;
		launch_slave(cpu, (void (*)(int)) fix_earom,
			0, 0, 0, 0);
	}

	fix_earom();
}


/*
 * int launch_slave()
 *	Sets up the MPCONF entry for the given cpu and launches
 *	it.  Note that the routine checks to see if the vpid 
 *	is equal to the vpid of the processor this function is
 *	running on.  If it is, it just calls the launch and 
 *	rendezvous function.
 */

int
launch_slave(int virt_id, void (*lnch_fn)(int), int lnch_parm, 
	     void (*rndvz_fn)(int), int rndvz_parm, void *stack)
{
	/* Make sure virt_id is valid */
	if (virt_id >= EV_MAX_CPUS || virt_id < 0) {
		return -1;
	}

	/* Set up the MP Block entry */
	MPCONF[virt_id].lnch_parm 	= lnch_parm;
	MPCONF[virt_id].rendezvous 	= rndvz_fn;
	MPCONF[virt_id].rndv_parm 	= rndvz_parm;
	MPCONF[virt_id].stack		= stack;
	MPCONF[virt_id].launch 		= lnch_fn; /* do this last! */

	return 0;
}


void fix_earom() {
	int prid;
	int speed;

	unsigned int write_count;

	/* Tell user we're doing something. */
	sa_store_double(EV_LED_BASE, 0xaa);

	prid = get_prid();

#ifdef WRITE_COUNT
	write_count = sa_load_double(EV_WCOUNT1_LOC) << 8;
	write_count |= sa_load_double(EV_WCOUNT0_LOC);
#else
	write_count = 1;
#endif

	sa_store_double(EV_EBUSRATE0_LOC, 0xe8);
	DELAY;
	sa_store_double(EV_EBUSRATE1_LOC, 0x9b);
	DELAY;
	sa_store_double(EV_EBUSRATE2_LOC, 0xd6);
	DELAY;
	sa_store_double(EV_EBUSRATE3_LOC, 0x02);
	DELAY;

	sa_store_double(EV_RTCFREQ0_LOC, 0xe8);
	DELAY;
	sa_store_double(EV_RTCFREQ1_LOC, 0x9b);
	DELAY;
	sa_store_double(EV_RTCFREQ2_LOC, 0xd6);
	DELAY;
	sa_store_double(EV_RTCFREQ3_LOC, 0x02);
	DELAY;

	sa_store_double(EV_EPROCRATE0_LOC, 0x80);
	DELAY;
	sa_store_double(EV_EPROCRATE1_LOC, 0xf0);
	DELAY;
	sa_store_double(EV_EPROCRATE2_LOC, 0xfa);
	DELAY;
	sa_store_double(EV_EPROCRATE3_LOC, 0x02);
	DELAY;

	sa_store_double(EV_PGBRDEN_LOC, 0);
	DELAY;

	sa_store_double(EV_CACHE_SZ_LOC, 0x14);
	DELAY;

	sa_store_double(EV_IW_TRIG_LOC, 0);
	DELAY;

	sa_store_double(EV_RR_TRIG_LOC, 0);
	DELAY;
	
	if ((prid & C0_REVMASK) >= 0x40)
		sa_store_double(EV_ECCENB_LOC, 1);
	else
		sa_store_double(EV_ECCENB_LOC, 0);
	DELAY;
#ifdef WRITE_COUNT
	write_count++;
#endif

	sa_store_double(EV_WCOUNT0_LOC, write_count & 0xff);
	DELAY;

	sa_store_double(EV_WCOUNT1_LOC, (write_count >> 8) & 0xff);
	DELAY;

	sa_store_double(EV_LED_BASE, 0xff);
}
