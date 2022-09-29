/**************************************************************************
 *									  *
 *  Copyright (C) 1986-1994 Silicon Graphics, Inc.			  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.95 $"

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/param.h>
#include <sys/reg.h>
#include <sys/pda.h>
#include <sys/systm.h>
#include <sys/cpu.h>
#include <sys/sysinfo.h>
#include <sys/cmn_err.h>
#include <sys/callo.h>
#include <sys/schedctl.h>
#include <sys/invent.h>
#include <sys/cmn_err.h>
#include <sys/edt.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/addrs.h>
#include <sys/EVEREST/evintr.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/evmp.h>
#include <sys/debug.h>
#include <sys/time.h>
#include <sys/arcs/spb.h>	/* ARCS System Parameter Block */
#include <sys/arcs/debug_block.h>/* ARCS debug block */
#include <sys/arcs/kerncb.h>	/* ARCS printf prototype */
#include <ctype.h>
#include <sys/errno.h>
#include <sys/syssgi.h>
#include <sys/strsubr.h>
#include <sys/sema.h>
#include <sys/fpu.h>

#if SABLE
#include <sys/i8251uart.h>
#endif

void	mem_init(void);
void	evio_init(void);
void	he_arcs_set_vectors(void);

int	maxcpus = EV_MAX_CPUS;
#if IP19
short	cputype = CPU_IP19;
#endif
#if IP21
short	cputype = CPU_IP21;
#endif
#if IP25
short	cputype = CPU_IP25;
#endif

/* processor-dependent data area */
pdaindr_t	pdaindr[EV_MAX_CPUS];

/* Convert logical cpuid to physical slot */
int cpuid_to_slot[EV_MAX_CPUS];

/* Convert logical cpuid to physical cpu# on board */
int cpuid_to_cpu[EV_MAX_CPUS];

/* convert physical slot/cpu# to logical cpu id */
cpuid_t logical_cpuid[EV_MAX_SLOTS][EV_CPU_PER_BOARD];

/*
 * Band-aids for a circulating program that re-writes the in-core copy of
 * a system id to allow s/w licensed for one machine to run on many.
 * 1. Hide ev_serial symbol by using ld -x in Makefile
 * 2. Introduce a new symbol id_number for crackers to waste their time on.
 * 3. Scatter the real serial number throughout the ev_serial array: Bytes from
 *    the real serial number are stored in the 3rd byte of every 4 in ev_serial.
 * 4. Alter the real serial number to make it harder to pick out of a crash dump.
 * 5. The other 3 bytes of ev_serial are filled with junk that will tend to
 *    make the array look more like a set of pointers to functions.
 */
static char ev_serial[SERIALNUMSIZE * 4];	/* Store system serial number here */
#define EV_SER_FUDGE	0x1b	/* must be less than 0x20 for validity test to work */
#define EV_SER_BAD_VAL	0xff
#define EV_SER_BYTE_1	0x80	/* somewhere in kernel text */
#define EV_SER_BYTE_2	0x35
/*#define EV_SER_BYTE_3	real ev_serial - EV_SER_FUDGE or NULL*/
/*#define EV_SER_BYTE_4	varies*/

/* logical cpuid of master cpu */
extern cpuid_t master_procid;

extern int cachewrback;	/* 1 if cache is writeback */
extern int valid_icache_reasons;
extern int valid_dcache_reasons;

/*
 * Everest-specific initialization functions.
 */

#ifdef SABLE
static void
everest_sable_init()
{
	/* Sable does not execute prom code, so we construct prom
	 * information here, using knowledge of HW config which it
	 * supports.
	 */

	int slot, cpu, slice, vid;
	evreg_t	cpumask;

	/* Memory configuration */

#ifdef IP25
	EVCFGINFO->ecfg_memsize = 65536;	/* 16 MB (65536 x 256 bytes) */
#else
	EVCFGINFO->ecfg_memsize = 32768;	/* 8 MB (32768 x 256 bytes) */
#endif

#ifndef SABLE_HW
	/* CPU configuration */

	cpumask = (EV_GET_LOCAL(EV_SYSCONFIG) & EV_CPU_MASK) >> EV_CPU_SHFT;

	/* Which slot is CPU in ? */
	if (cpumask & 0x01)
	  	slot = 0;
	else if (cpumask & 0x04)
	  	slot = 2;
	for (cpu=0; cpu<EV_CPU_PER_BOARD; cpu++) {
	  	slice = cpu;
		if (cpu == 0) {
			vid = 0;
			MPCONF[vid].phys_id = (slot<<EV_SLOTNUM_SHFT) | slice;
			EVCFGINFO->ecfg_board[slot].eb_type = EVCLASS_CPU;
			EVCFG_CPUSTRUCT(slot, slice).cpu_vpid = vid;
			EVCFG_CPUSTRUCT(slot, slice).cpu_enable = 1;
		} else {
		  	/* Mark other cpus as failing the diags */
			EVCFGINFO->ecfg_board[(slot)].eb_cpu.eb_cpus[(cpu)].cpu_diagval = -1;
		}
	}
	/*
	 * Kernel ASSUMES that the proms have setup the uart.
	 * So perform setup here so UART is usable.
	 * Arbitrarily pick any of the ASYNC mode for sable -- as long as
	 * one of the two LSB is set, uart will operate.
	 */
	EV_SET_LOCAL(EV_UART_CMD,
		     I8251_ASYNC64X|I8251_8BITS|I8251_EVENPAR|I8251_STOPB2);

	/* For now we don't enable RX until we hit idle loop - it makes
	 * sable execute about 5 times slower, continuously checking for
	 * input characters.
	 */
/****	EV_SET_LOCAL(EV_UART_CMD, I8251_TXENB|I8251_RXENB); ****/
	EV_SET_LOCAL(EV_UART_CMD, I8251_TXENB); 

	/* arcs info setup  */

	__TV = (FirmwareVector *)PHYS_TO_K1(0x1800);
	SPB->DebugBlock = (LONG *)PHYS_TO_K1(0x85fd0);
#endif
}

#endif


/*
 * Probe everest hardware to find out where cpu boards are,
 * and set up mappings between physical CPU IDs and logical
 * CPU IDs according to PROM mappings.
 *
 * Some cpus may have been disabled by the PROM, but they
 * may be enabled later under operator control.
 *
 * Returns number of cpu's, including disabled cpu's.
 */
static int
cpu_probe(void)
{
	int slot, cpu;
	cpuid_t cpuid, max_cpuid = 0;
#if R4000 && IP19
	extern int has_250;	/* flag checked in hook_exc.c */
#endif

	evreg_t cpumask =
	    (EV_GET_SYSCONFIG() & EV_CPU_MASK) >> EV_CPU_SHFT;

	for (slot = 0; slot < EV_MAX_SLOTS; slot++) {
		if (cpumask & 1) { /* Found a slot with a processor board */
			for (cpu=0; cpu<EV_CPU_PER_BOARD; cpu++) {
				if (EVCFG_CPUDIAGVAL(slot,cpu) ==
							EVDIAG_NOTFOUND) {
					logical_cpuid[slot][cpu] = EV_CPU_NONE;
					continue;
				}
				cpuid = EVCFG_CPUID(slot,cpu);
#if LARGE_CPU_COUNT_EVEREST
				/* Modify EVCFG structure so that odd numbered
				 * cpus are numbered as a negative offset from
				 * EV_MAXCPUS (i.e. cpu 1 == cpu 127, 3 == 125)
				 */
				if (cpuid & 0x01) {
					cpuid = EV_MAX_CPUS - cpuid;
					EVCFG_CPUSTRUCT(slot, 1).cpu_vpid =
						cpuid;
				}
#endif 
				cpuid_to_slot[cpuid] = slot;
				cpuid_to_cpu[cpuid] = cpu;
				logical_cpuid[slot][cpu] = cpuid;
				if (cpuid > max_cpuid)
					max_cpuid = cpuid;
#if R4000 && IP19
				/* Note any 250MHz modules present */
				if ((EVCFG_CPUSPEED(slot, cpu) * 2) == 250)
					has_250++;
#endif
			}
		} else {	/* No processor board in this slot */
			for (cpu=0; cpu<EV_CPU_PER_BOARD; cpu++)
				logical_cpuid[slot][cpu] = EV_CPU_NONE;
		}
		cpumask = cpumask >> 1;
	}
	return(max_cpuid+1);
}

extern void cpuintr(eframe_t*, void *);
extern void groupintr(eframe_t*, void *);
extern void tlbintr(eframe_t*, void *);
extern void slave_loop();
extern void nvram_init(void);

/* 
 * This array is when start_slave_loop is called by the master CPU
 * during boot. A CPU is associated with an index using the same
 * mapping as MPCONF array. When a CPU has entered the kernel slave
 * loop and is ready to continue, it sets its entry to 1. This is used 
 * as a positive ack to indicate the IO4 image is no longer required.
 */
char	slave_loop_ready[EV_MAX_CPUS] = {0}; /* Force part of data segment */

#define	READYTIMEOUT	50000

/* Temp copy of system id number - crackers can play with this one all they like */
char	id_number[SERIALNUMSIZE + 1] = {0};

/*
 * mlreset - very early machine reset - at this point NO interrupts have been
 * enabled; nor is memory, tlb, p0, etc setup
 */
void
mlreset(int first)
{
	int i, t, bad_serial = 0, saw_null = 0;

	if (first == 0) {
#ifdef SABLE
		everest_sable_init();
#endif

		/*
		 * Only the "master" or "first" processor does this
		 * global initialization.
		 */

		/* probe for all CPUs */
		maxcpus = cpu_probe();

		master_procid = getcpuid();

		/*
		 * Wait for all processors to indicate they are ready. We
		 * do not print any messages at this point, we do that later
		 * in allowboot().
		 */
#ifndef SABLE

		for (t = READYTIMEOUT*maxcpus, i = 0; i < maxcpus; i++) {
		    if (cpu_enabled(i)) {
			while (!slave_loop_ready[i] && t) {
			    t--;
			    DELAY(1);
			}
		    }
		}
#endif /* SABLE */

		/* init interrupts */
		evintr_init();

		/* init memory subsystem */
		mem_init();

		/* setup IO subsystem */
		evio_init();

		/* init nvram spinlock */
		nvram_init();
		
		/*
		 * if _check_dbg() did not turn kdebug on, turn it off here.
		 */
		if (kdebug == -1)
			kdebug = 0;

#if R4000
		/*
		 * cachecolormask is set from the bits in a vaddr/paddr
		 * which the r4k looks at to determine if a VCE should
		 * be raised.
		 * Despite what some documents may say, these are
		 * bits 12..14
		 * These are shifted down to give a value between 0..7
		 * NOTE: VCE not an issue for TFP.
		 */
		cachecolormask = R4K_MAXPCACHESIZE/NBPP - 1;
#endif
		cachewrback = 0;	/* No writeback (I/O coherent) */
		valid_icache_reasons &= ~CACH_IO_COHERENCY;
		valid_dcache_reasons &= ~CACH_IO_COHERENCY;
#if TFP
		/* TFP dcache is write-through, so no reason to flush
		 * the dcache for ICACHE_COHERENCY (though icache needs
		 * flushing).
		 */
		valid_dcache_reasons &= ~(CACH_ICACHE_COHERENCY|CACH_AVOID_VCES);
		valid_icache_reasons &= ~CACH_AVOID_VCES;
#endif		
#if R10000
		valid_dcache_reasons &= ~CACH_AVOID_VCES;
		valid_icache_reasons &= ~CACH_AVOID_VCES;
#endif
		evintr_connect(0, EVINTR_LEVEL_CPUACTION, SPLMAX, 
			EVINTR_DEST_CPUACTION, cpuintr, 0);
	
		evintr_connect(0, EVINTR_LEVEL_GROUPACTION, SPLMAX, 
			       EVINTR_DEST_GROUPACTION, groupintr, 0);

#if EVINTR_LEVEL_TLB != EVINTR_LEVEL_CPUACTION
		/* This should be executed on all systems except MUTLIKERNEL
		 * CELL where we don't actually use a different level.
		 * On that system we still use CPUACTION.
		 * NOTE: Could use SPLTLB except our EVINTR_LEVEL is
		 * greater than cpuintr which uses SPLMAX.
		 */
		evintr_connect(0, EVINTR_LEVEL_TLB, SPLMAX, 
			       0, tlbintr, 0);
#endif
		
		/*
		 * Since we've wiped out memory at this point, we
	    	 * need to reset the ARCS vector table so that it
		 * points to appropriate functions in the kernel
		 * itself.  In this way, we can maintain the ARCS
		 * vector table conventions without having to actually
		 * keep redundant PROM code in memory.
		 */
		he_arcs_set_vectors();

		/*
		 * Enable all Everest local interrupts on master.
		 */
		EV_SET_LOCAL(EV_ILE,    /* EV_EBUSINT_MASK */
					EV_CMPINT_MASK |
					EV_UARTINT_MASK |
					EV_ERTOINT_MASK |
					EV_WGINT_MASK);
		
		/* Clear Interrupt Group Mask */
		EV_SET_LOCAL(EV_IGRMASK, 0);
		
		/* Copy in the system serial number */
/*		bcopy((char *)EVCFGINFO->ecfg_snum, ev_serial, SERIALNUMSIZE);*/
		bcopy((char *)EVCFGINFO->ecfg_snum, id_number, SERIALNUMSIZE);
		/* Obfuscate it */
		for (i = 0; i < SERIALNUMSIZE; i++) {
		     t = i << 2;
		     ev_serial[t] = EV_SER_BYTE_1;
		     ev_serial[t+1] = EV_SER_BYTE_2;
		     /* NULL is special - terminates valid bytestring */
		     if (id_number[i] == '\0') {
			  ev_serial[t+2] = '\0';
			  saw_null = 1;
		     } else {
			  /* chars < 0x20 or > 0x7f are bad unless after a NULL */
			  if (!saw_null
			      && (((id_number[i] & 0xe0) == 0) || (id_number[i] & 0x80))) {
			       bad_serial = 1;
			  }
			  /* Make it harder to spot the real data in a dump */
			  ev_serial[t+2] = id_number[i] - EV_SER_FUDGE;
		     }
		     ev_serial[t+3] = (id_number[i] + id_number[i+1]) & 0xfc;
		}
		if (bad_serial || !saw_null) {
		     ev_serial[1] = EV_SER_BAD_VAL;	/* force ENODEV on subsequent reads */
		}
	} else {

		/*
		 * This code is performed ONLY by slave processors.
		 */

		/*
		 * Enable Everest local interrupts on slaves.
		 * (Leave CCUART interrupt disabled.)
		 */
		EV_SET_LOCAL(EV_ILE,    /* EV_EBUSINT_MASK */
					EV_CMPINT_MASK |
					EV_ERTOINT_MASK |
					EV_WGINT_MASK);
		
		/* Clear Interrupt Group Mask */
		EV_SET_LOCAL(EV_IGRMASK, 0);
	}

	/*
	 * ALL processors do this initialization
	 */

#if !SABLE  	/* Was !SABLE_RTL */
	/* RTL simulator inits both the tlbs & the cache for us */
	flush_tlb(TLBFLUSH_NONPDA);		/* flush all BUT pda */
	flush_cache(); 				/* flush all caches */
#endif
}

/*
 * Allow interrupts to reach cpus.
 */
void
allowintrs(void)
{
	evreg_t ile;
	extern int ducw_intrs_enabled;

	ile = EV_GET_LOCAL(EV_ILE);
	EV_SET_LOCAL(EV_ILE, ile | EV_EBUSINT_MASK);
	ducw_intrs_enabled = 1;
}

/*
 * Return the logical cpu ID for the calling processor.
 * Usually, the value private.p_cpuid (accessed via the
 * "cpuid()" macro is sufficient for this purpose; however, 
 * this routine can be called early on, before the pda is 
 * fully set up.
 */
cpuid_t
getcpuid(void)
{
	register evreg_t slotproc;
	register int slot; 
	register int proc; 
#if SABLE_RTL
	return(0);
#endif /* SABLE_RTL */

	slotproc = EV_GET_LOCAL(EV_SPNUM);
	slot = (slotproc & EV_SLOTNUM_MASK) >> EV_SLOTNUM_SHFT;
	proc = (slotproc & EV_PROCNUM_MASK) >> EV_PROCNUM_SHFT;

	return(logical_cpuid[slot][proc]);
}


/*
 * On EVEREST, unix overlays the prom's bss area.  Therefore, only
 * those prom entry points that do not use prom bss or are 
 * destructive (i.e. reboot) may be called.  There is however,
 * a period of time before the prom bss is overlayed in which
 * the prom may be called.  After unix wipes out the prom bss
 * and before the console is inited, there is a very small window
 * in which calls to dprintf will cause the system to hang.
 *
 * If symmon is being used, it will plug a pointer to its printf
 * routine into the debug block which the kernel will then use
 * for dprintfs.  The advantage of this is that the kp command
 * will not interfere with the kernel's own print buffers.
 *
 */
# define ARGS	 a0,a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15
void
dprintf(fmt,ARGS)
char		*fmt;
__psint_t	ARGS;
{
	/*
	 * Check for presence of symmon
	 * Check the SPB magic number - there are several things
	 * going on here that make this check important. The 64
	 * bit IP19 kernel is started by a 32 bit prom, which builds
	 * a 32 bit spb. However, symmon, when initialized, rebuilds
	 * a 64 bit spb. In checking the magic number here, we are
	 * looking for the magic number in a 64 bit field - if that
	 * is present we assume there is a 64-bit symmon out there.
	 */
	if (SPB->Signature == SPBMAGIC && 
	    SPB->DebugBlock &&
	    ((db_t *)SPB->DebugBlock)->db_magic == DB_MAGIC &&
	    ((db_t *)SPB->DebugBlock)->db_printf)
		(*((db_t *)SPB->DebugBlock)->db_printf)(fmt, ARGS);
	else
	/*
	 * cn_is_inited() implies that PROM bss has been wiped out
	 */
	if (cn_is_inited())
		cmn_err(CE_CONT,fmt,ARGS);
	else
	/*
	 * try printing through the prom
	 */
		arcs_printf (fmt,ARGS);
}


/*
 * fp_find - probe for floating point chip
 */
void
fp_find(void)
{
	private.p_fputype_word = get_fpc_irr();
}


/*
 * fp chip initialization
 */
void
fp_init(void)
{
	set_fpc_csr(0);
}

/*
 * get_except_norm
 *
 *	return address of exception handler for all exceptions other
 *	then utlbmiss.
 */
inst_t *
get_except_norm(void)
{
	extern inst_t exception[];

	return exception;
}



int
cpuboard(void)
{
#if R4000
	ASSERT(cputype == CPU_IP19);
	return(INV_IP19BOARD);
#endif
#if TFP
	ASSERT(cputype == CPU_IP21);
	return(INV_IP21BOARD);
#endif
#if R10000
	ASSERT(cputype == CPU_IP25);
	return(INV_IP25BOARD);
#endif
}


/*
 * add_ioboard
 *	Adds EVEREST-specific I/O information to the hardware inventory
 *	structure.  We add the UARTS here because the duart driver is
 *	initialized before the dynamic memory allocater is.
 */

void
add_ioboard(void)
{
	extern int ev_num_serialports;
	int slot, my_slot;
	evbrdinfo_t *brd;
	int serial_found, ether_found;
	int num_serial = 0;
	ibus_t *ibus;

#if defined(MULTIKERNEL)
	/* only the golden cell has I/O bpoards */
	if (evmk_cellid != evmk_golden_cellid)
		return;
#endif /* MULTIKERNEL */
	/* Add the DUART information to the hardware inventory */	
	add_to_inventory(INV_SERIAL, INV_EPC_SERIAL, 0, 0, ev_num_serialports);

	/* Add the I/O boards to the hardware inventory */
	for (slot = EV_MAX_SLOTS; slot >= 0; slot--) {
		brd = &(EVCFGINFO->ecfg_board[slot]);	

		/* Skip non-I/O boards */
		if ((brd->eb_type & EVCLASS_MASK) != EVCLASS_IO)
			continue;

		add_to_inventory(INV_IOBD, INV_EVIO, 0, (char) slot, 
				 brd->eb_type);

		/* Make allowances for the master IO4 */		
		if (BOARD(slot)->eb_io.eb_winnum == 1)
			my_slot = 0;
		else
			my_slot = slot;

#ifndef SABLE
		/* Check to see if the serial ports or ethernet for
		 * this board are configured.
		 */
		ibus = ibus_adapters;


		while (ibus->ibus_module) {
			if ((ibus->ibus_adap >> IBUS_SLOTSHFT) == my_slot) {
				if (ibus->ibus_module == EPC_SERIAL) {
					serial_found++;
					num_serial++;
				}
	
				if (ibus->ibus_module == EPC_ETHER)
					ether_found++;
			}
		
			ibus++;
		} 
#endif

		if (!serial_found && (num_serial < 4))
		 	cmn_err(CE_NOTE, 
	"No system file entry for the IO4 slot %d serial ports was found.\n"
	"Consult the serial(7) man page for information on configuring\n"
	"these ports.\n", slot);

		if (!ether_found)
			cmn_err(CE_NOTE,
	"No system file entry for the IO4 slot %d ethernet port was found.\n"
	"Consult the ethernet(7) man page for information on configuring\n"
	"this port.\n", slot); 
		
		serial_found = 0;
		ether_found = 0;
	}
}

/*
 * timer_freq is the frequency of the 32 bit counter timestamping source.
 * timer_high_unit is the XXX
 * Routines that references these two are not called as often as
 * other timer primitives. This is a compromise to keep the code
 * well contained.
 * Must be called early, before main().
 */
void
timestamp_init(void)
{
	timer_freq = CYCLE_PER_SEC;	/* RTC time source */
	timer_unit = NSEC_PER_SEC/timer_freq;
	timer_high_unit = timer_freq;	/* don't change this */
	timer_maxsec = TIMER_MAX/timer_freq;
}


/*
 *  Pass back the serial number associated with the system ID in the
 *  system controller's NVRAM.  Return zero if the ID has been set to an
 *  appropriate string else zero it out and return ENODEV as clover2.c does.
 *  There really isn't a more appropriate already defined error number.
 */
int
getsysid(char *hostident)
{
	int i;
	char c;

	/*
	 * serial number is up to 10 bytes long on Everest.  Left justify
	 * in memory passed in...  Zero out the balance.
	 */
	i = 0;

	bzero(hostident, MAXSYSIDSIZE);

	if (ev_serial[1] == EV_SER_BAD_VAL) {
	     return ENODEV;
	}

	/* Pull out the interesting bytes and reconstruct the serial number */
	for (i = 0; i <= SERIALNUMSIZE; i++) {
	     if (i == SERIALNUMSIZE) {
		  return ENODEV;
	     }
	     c = ev_serial[(i << 2) + 2];
	     if (c == '\0') {
		  hostident[i] = c;
		  break;
	     }
	     hostident[i] = c + EV_SER_FUDGE;
	}
	return 0;
}

