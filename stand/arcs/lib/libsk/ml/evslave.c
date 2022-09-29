/*
 * evslave.c -- Contains the Everest multi-processor spin loop code.
 */

#include <sys/types.h>
#include <sys/sbd.h> 
#include <sys/cpu.h>
#include <sys/loaddrs.h>
#include <sys/EVEREST/evintr.h>
#include <sys/EVEREST/evmp.h>
#include <libsc.h>
#include <libsk.h>

extern int Debug;

#if (_MIPS_SZPTR == 64)

static int
convert_address(void *address)
{
	if (address == 0)
		return 0;
	if (((__psint_t)address & 0xff00000000000000L) == 0x9000000000000000L)
		return KPHYSTO32K1(address);
	else
		return KPHYSTO32K0(address);
}

#elif (_MIPS_SZPTR == 32)

static int
convert_address(void *address)
{
	return (int)address;
}

#endif /* _MIPS_SZPTR == 64 */

static void *
unconvert_address(int address)
{
	if (address == 0)
		return 0;
	if ((address & 0xe0000000) == 0xa0000000)
		/* synthesize an uncached, pointer-sized, kernel address */
		return (void *)K132TOKPHYS(address);
	else
		/* synthesize a cached, pointer-sized, kernel address */
		return (void *)K032TOKPHYS(address);
}


/*
 * void launch_slave()
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
	/* Check to see if virtual ID is current processor */
	if (virt_id == cpuid())	{
		/* Call launch and rendezvous parameters */
		lnch_fn(lnch_parm);
		if (rndvz_fn)
			rndvz_fn(rndvz_parm);
	} else {
		/* Make sure virt_id is valid */
		if (virt_id >= EV_MAX_CPUS || virt_id < 0) {
	    		printf("launch_slave: error: virt_id (0x%x) is too big.\n", 
				virt_id);
	    		return -1;
		}

		/* Set up the MP Block entry */
#ifdef LARGE_CPU_COUNT_EVEREST
		if (virt_id & 0x01)
			if (virt_id < REAL_EV_MAX_CPUS)
				return(-1);
			else
				virt_id = EV_MAX_CPUS - virt_id;
#endif
		MPCONF[virt_id].lnch_parm 	= lnch_parm;
		MPCONF[virt_id].rendezvous 	=
					convert_address((void *)rndvz_fn);
		MPCONF[virt_id].rndv_parm 	= rndvz_parm;
		MPCONF[virt_id].stack		= convert_address(stack);

/* In a 64-bit, store the whole 64-bit address.  In a 32-bit prom, store
 * a nice, sign-extended version of the address.
 */
#if (_MIPS_SZPTR == 32)
		MPCONF[virt_id].sign_fill = (int)stack < 0 ? -1 : 0;
#endif
		MPCONF[virt_id].real_sp		= stack;

		/* Do this last! - It will launch immediately. */
		MPCONF[virt_id].launch 		=
					convert_address((void *)lnch_fn);
	}

	return 0;
}

/*
 * void slave_loop(void)
 *	Slave processor Idle-loop.  The slaves periodically check to
 *	see if anyone has set a launch address.  If so, this routine 
 *	executes the launched code, passing the launch parameter.
 */

int 
slave_loop(void)
{
	uint		vpid = cpuid();	/* Virtual processor ID of this CPU */
	void 	 	(*launch)(int);	/* Launch function */
	void		(*rndvz)(int);	/* Rendezvous function */
	volatile mpconf_t *mp;		/* Pointer to cpu's MPCONF block */

	if  (Debug)
		printf("CPU 0x%x in IO4 slave_loop...\n", vpid);

#ifdef LARGE_CPU_COUNT_EVEREST
	if (vpid &0x01) {
		mp = (volatile mpconf_t *)&(MPCONF[EV_MAX_CPUS - vpid]);
	} else
#endif 
		mp = (volatile mpconf_t *)&(MPCONF[vpid]);

	for (;;) {
	        us_delay(1000);

		if (!mp->launch)
			continue;

		if (Debug)
			printf("\nReceived launch request (0x%x)\n",
								mp->launch);

		/* Make sure this is a valid MPCONF block */
		if (mp->mpconf_magic != MPCONF_MAGIC || mp->virt_id != vpid)
#ifdef LARGE_CPU_COUNT_EVEREST
		  if ((!(vpid & 0x01)) || ( mp->virt_id != (EV_MAX_CPUS-vpid)))
#endif
		{
			mp->launch = 0;
			printf("slave_loop: Error: Invalid MPCONF block\n");
			continue;
		}

		/* Call the launch routine */
		launch = (void (*)(int))unconvert_address(mp->launch);
		mp->launch = 0;
		if (IS_KSEG0((__psint_t)launch))
			flush_cache();
		if (mp->real_sp == (void *)0)
			launch(mp->lnch_parm);
		else
#if (_MIPS_SZPTR == 64)
			call_coroutine(launch, mp->lnch_parm, mp->real_sp);
#elif (_MIPS_SZPTR == 32)
			call_coroutine(launch, mp->lnch_parm,
					unconvert_address(mp->stack));
#endif

		/* Call the rendezvous routine */
		if (mp->rendezvous != 0) {
			rndvz = (void (*)(int))unconvert_address(mp->rendezvous);
			mp->rendezvous = 0;
			if (mp->real_sp == (void *)0)
				rndvz(mp->rndv_parm);
			else
				call_coroutine(rndvz, mp->rndv_parm,
						mp->real_sp);
		}
	}
}
