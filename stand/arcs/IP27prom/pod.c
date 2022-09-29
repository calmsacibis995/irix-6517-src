/***********************************************************************\
 *       File:           pod.c                                         *
 *                                                                     *
 \**********************************************************************/

#ident "$Revision: 1.127 $"

#include <setjmp.h>
#include <stdarg.h>

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/immu.h>
#include <sys/cpu.h>
#include <sys/inst.h>
#include <sys/mips_addrspace.h>

#include <sys/SN/intr.h>
#include <sys/SN/SN0/IP27.h>
#include <sys/SN/agent.h>
#include <sys/SN/gda.h>

#include <libkl.h>
#include <sys/SN/SN0/ip27log.h>
#include <ksys/elsc.h>

#include "junkuart.h"
#include "pod.h"
#include "pod_failure.h"
#include "ip27prom.h"
#include "prom_leds.h"
#include "prom_error.h"
#include "libasm.h"
#include "cache.h"
#include "tlb.h"
#include "hub.h"
#include "mdir.h"
#include "parse.h"
#include "symbol.h"
#include "kdebug.h"

void	store_gprs(struct reg_struct *);
void	scroll_n_print(unsigned char);

#define NOPRINT 1

char *cop0_names[] = {
    "Index", "Random", "EntryLo0", "EntryLo1", "Context", "PageMask",
    "Wired", "Cop0_7", "BadVAddr", "Count", "EntryHi", "Compare",
    "Status", "Cause", "EPC", "PRId", "Config", "LLAddr", "WatchLo",
    "WatchHi", "XContext", "FrameMask", "BrDiag", "Cop0_23", "Cop0_24",
    "PerfCount", "ECC", "CacheErr", "TagLo", "TagHi", "ErrorEPC", "NMISR",
};

int cop0_count = sizeof (cop0_names) / sizeof (cop0_names[0]);

int cop0_lookup(const char *name)
{
    int		i, j, c1, c2;

    for (i = 0; i < cop0_count; i++)
	for (j = 0; ; j++) {
	    c1 = name[j];
	    c2 = LBYTE(&cop0_names[i][j]);

	    if (c1 == 0 && c2 == 0)
		return i;

	    if (toupper(c1) != toupper(c2) || c1 == 0)
		break;
	}

    return -1;
}

char *gpr_names[] = {
    "r0", "at", "v0", "v1", "a0", "a1", "a2", "a3",
    "a4", "a5", "a6", "a7", "t0", "t1", "t2", "t3",
    "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7",
    "t8", "t9", "k0", "k1", "gp", "sp", "fp", "ra",
};

char *exc_names[32] =
{
    "Interrupt", "Page Modified", "Load TLB Miss",
    "Store TLB Miss", "Load Addr Err", "Store Addr Err",
    "Inst Bus Err", "Data Bus Err", "System Call",
    "Breakpoint", "Reserved Inst", "Cop. Unusable",
    "Overflow", "Trap", "-", "Float Pt. Exc", "-", "-",
    "-", "-", "-", "-", "-", "Watchpoint", "-", "-", "-",
    "-", "-", "-", "-", "-"
};

/*
 * hubprintf
 *
 *   If in manufacturing mode, prints to the Junk UART if present,
 *   otherwise ELSC.  If not manufacturing mode, doesn't print anywhere
 *   (doing so screws up initialization of ioc3uart in ioc3uart.c since
 *   ioc3uart.c calls hubprintf).
 */

/*VARARGS1*/
int
hubprintf(const char *fmt, ...)
{
    va_list		ap;
    libc_device_t      *old_dev, *dev;

    old_dev	= libc_device(0);
    dev		= &dev_nulluart;

    if (get_BSR() & BSR_MANUMODE) {
	if (junkuart_probe())
	    dev = &dev_junkuart;
	else if (hub_have_elsc())
	    dev = &dev_elscuart;
    }

    libc_device(dev);

    va_start(ap, fmt);
    vprintf(fmt, ap);

    libc_device(old_dev);

    return 0;
}

void
xlate_cause(__scunsigned_t value, char *buf)
{
    __scunsigned_t	exc_code, int_bits;
    int			i;

    exc_code = (value & CAUSE_EXCMASK) >> CAUSE_EXCSHIFT;
    int_bits = (value & CAUSE_IPMASK ) >> CAUSE_IPSHIFT;

    if (value & CAUSE_BD) {
	sprintf(buf, "BDSLOT ");
	buf += strlen(buf);
    }

    if (value & CAUSE_CEMASK) {
	sprintf(buf, "COP%ld ", (value & CAUSE_CEMASK) >> CAUSE_CESHIFT);
	buf += strlen(buf);
    }

    strcpy(buf, "INT:");
    buf += strlen(buf);

    for (i = 7; i >= 0; i--)
	*buf++ = (int_bits & 1 << i) ? '1' + i : '-';

    *buf = 0;

    if (exc_code) {
	sprintf(buf, " <%s>", exc_names[exc_code]);
	buf += strlen(buf);
    }
}

/*
 * dbe_address
 *
 *   Should only be called if CAUSE indicates one of the following:
 *     Load Addr Err, Store Addr Err, Data Bus Err
 */

static __uint64_t dbe_address(struct reg_struct *gprs)
{
    jmp_buf			fault_buf;
    void		       *old_buf;
    union mips_instruction	inst;

    if (setfault(fault_buf, &old_buf)) {
	restorefault(old_buf);
	return 0;
    }

    inst.word = *(unsigned *) (gprs->cause & CAUSE_BD ?
			       gprs->epc + 4 : gprs->epc);

    restorefault(old_buf);

    if (inst.i_format.opcode < 0x1a)
	return 0;

    return gprs->gpr[inst.i_format.rs] + inst.i_format.simmediate;
}

/*
 * print_exception
 */

void print_exception(struct reg_struct *gprs, struct flag_struct *flags)
{
    char		tmp[80];
    __uint64_t		busadr;

    printf(" EPC    : %y (%P)\n", gprs->epc, gprs->epc);
    printf(" ERREPC : %y (%P)\n", gprs->error_epc, gprs->error_epc);
    printf(" CACERR : %y\n", gprs->cache_err);
    printf(" Status : %y\n", gprs->sr);
    printf(" BadVA  : %y (%P)\n", gprs->badva, gprs->badva);
    printf(" RA     : %y (%P)\n", gprs->gpr[31], gprs->gpr[31]);
    printf(" SP     : %y\n", gprs->gpr[29]);
    printf(" A0     : %y\n", gprs->gpr[4]);

    xlate_cause(gprs->cause, tmp);
    printf(" Cause  : %y (%s)\n", gprs->cause, tmp);

    switch ((gprs->cause & CAUSE_EXCMASK) >> CAUSE_EXCSHIFT) {
    case 4:		/* Load Addr Err  */
    case 5:		/* Store Addr Err */
    case 7:		/* Data Bus Err   */
	if ((busadr = dbe_address(gprs)) != 0)
	    printf(" BusAdr : %y\n", busadr);
    }

    decode_diagval(tmp, flags->diagval);
    printf(" Reason : %d (%s)\n", flags->diagval, tmp);
}

/*
 * pod_console
 *
 *   Determines which console to use, in a specified priority order.
 *   Probes the listed devices, and selects and initializes the first one
 *   that's responding.  Dev_list is a string of letters listing the
 *   devices according to the following table.  Net UART always ought to be
 *   included at the end.
 *
 *	'j'	Junk UART
 *	'i'	IOC3 UART
 *	'e'	ELSC UART
 *	'n'	Net UART
 *
 *   The Junk and IOC3 UARTs can only be used by a local master.
 *
 *   NOTE:  It is very important that this routine CANNOT crash.
 *	    The junkuart and elscuart probe routines do not access
 *	    memory, and/or have timeouts, and the ioc3uart probing
 *	    traps exceptions.
 */

void pod_console(void *console, char *dev_list, int cons_dipsw, int flag,
	uchar_t *nvram_buffer)
{
    int			elsc_r, r;
    libc_device_t      *dev = 0;

    /*
     * Set the default UART to NULL.  This is necessary because the i2c
     * routines, get_local_console routines, etc. try to print stuff.
     */

    libc_device(&dev_nulluart);

#ifdef SABLE

    if (hub_local_master())
	dev = &dev_hubuart;

#endif /* SABLE */

    /*
     * Initialize I2C.  This is done on both CPUs because we don't
     * know which is going to be requesting a console.
     *
     * Re-read DIP switches for DIAG_MODE.
     */

#if 0
    hub_lock(HUB_LOCK_I2C);

    hub_led_set(PLED_I2CINIT);
    elsc_r = i2c_init(get_nasid());
    hub_led_set(PLED_I2CDONE);

    hub_unlock(HUB_LOCK_I2C);
#else
    elsc_r = 0;
#endif

    read_dip_switches(nvram_buffer);

    /*
     * Probe devices in order.
     */

    while (dev == 0 && (r = LBYTE(dev_list)) != 0) {
	switch (r) {
	case 'j':	/* Junk UART (local master only) */
	    if (hub_local_master() && junkuart_probe() > 0)
		dev = &dev_junkuart;
	    break;
	case 'e':	/* ELSC UART */
	    if (elsc_r >= 0) {
		hub_led_set(PLED_ELSCPROBE);
		elsc_r = elscuart_probe();
		hub_led_set(PLED_DONEPROBE);
		if (elsc_r > 0)
		    dev = &dev_elscuart;
	    }
	    break;
	case 'i':	/* IOC3 UART (local master only) */
	    if (hub_local_master() && ! hub_io_disable()) {
		hub_led_set(PLED_CONSOLE_GET);
		/* XXX passing in a module value of -1. Need to
		   determine what it should be on re-entry */
		r = get_local_console(console, DIAG_MODE_NONE, cons_dipsw, -1, flag);
		hub_led_set(PLED_CONSOLE_GET_OK);

		if (r >= 0) {
		    set_ioc3uart_base(((console_t *) console)->uart_base);
		    dev = &dev_ioc3uart;
		}
	    }
	    break;
	}

	dev_list++;
    }

    if (dev)
	libc_device(dev);

    hub_led_set(PLED_UARTINIT);
    libc_init(console);
    hub_led_set(PLED_UARTINITDONE);
}

/*
 * pod_loop
 *
 *  mode: Of POD_MODE_xxx and specifies memory/cache setup to use
 *  diagval: Contains a code explaining why we're here (see pod_failure.h)
 *  message: Message to display on entry.
 */

static char RunStrings[] = "Dex\000Unc\000Cac\000Bad\000";

void pod_loop(int mode, int diagval, char *message, __uint64_t pod_ra)
{
    char		buf[LINESIZE], lastcomm[LINESIZE];
    struct reg_struct	gprs;
    struct flag_struct	flags;
    char		elsc, epc, bad;
    console_t		console;
    int			cpusl;
    nasid_t		mynid;
    nmi_t		*nmi_addr;
    nasid_t		*nid_address;

#if 0
    char manu_mode;
#endif

    hub_led_set(PLED_PODLOOP);

	/*
	 * Do not consider an error the case when the second NMI is
	 * received but the nmi vector has the nmi_addr->call_parm set.
	 * This is an artificial case aiming to solve the corrupted
	 * core dump problem. The second NMI is issued by vmdump.c
	 * on purpose (in most cases it will not be seen as a second NMI,
	 * only cases when the operator stops the machine by issuing
	 * the NMI himself are relevant).
	 */

    mynid = get_nasid();
    cpusl = hub_cpu_get();
    nmi_addr = (nmi_t *)NMI_ADDR(mynid, cpusl);

	if (diagval == KLDIAG_NMISECOND && nmi_addr->call_parm) {
		epc = 0;
		bad = 0;
	} else {
	    epc = (diagval == KLDIAG_NMI ||
		   diagval == KLDIAG_NMIPROM ||
		   diagval == KLDIAG_NMIBADMEM ||
		   diagval == KLDIAG_NMICORRUPT ||
		   diagval == KLDIAG_NMISECOND ||
		   diagval == KLDIAG_EXC_CACHE);

   	 	bad = (diagval != KLDIAG_DEBUG && diagval != KLDIAG_PASSED &&
	   		diagval != KLDIAG_FRU_LOCAL &&  diagval != KLDIAG_FRU_ALL);
	}

    /* Do the local node fru analysis */
    if (diagval == KLDIAG_FRU_LOCAL) {
	    extern void sn0_fru_node_entry(nasid_t nasid,
					   int (*print)(const char *,...));
	    sn0_fru_node_entry(get_nasid(),printf);
    }
    /* Do the fru analysis on all nodes */
    if (diagval == KLDIAG_FRU_ALL) {
	    extern int	sn0_fru_entry(int (*print)(const char *,...));
	    sn0_fru_entry(printf);
    }



#if 0
    manu_mode = 1;	/* XXX fixme according to dip switch */
#endif

    pod_console(&console, bad ? "jein" : "jien", 0, 1, (uchar_t *)NULL);

    elsc = (libc_device(0) == &dev_elscuart);

    /*
     * Get the GPR values from the GPRs and FPRs (FPRs contain the
     * values that were in the GPRs when an exception was taken).
     */

    store_gprs(&gprs);

    flags.mode = mode;
    flags.diagval = diagval;
    flags.selected = 0xff;		/* No one's selected */
    flags.silent = 0;			/* Normal verbosity level */
    flags.slot = hub_slot_get();
    flags.slice = hub_cpu_get();
    flags.maxerr = 32;
    flags.pod_ra = pod_ra;
    flags.alt_regs = 0;

    set_BSR(get_BSR() & ~(BSR_GDASYMS | BSR_KREGS));

    if (get_BSR() & BSR_KDEBUG)
	kdebug_setup(&flags);

    /*
     * Display a brief message indicating why we're here.
     * The 'why' command provides the detailed info.
     */

    printf("\n");

	if (diagval == KLDIAG_NMISECOND && nmi_addr->call_parm) {
#if 0
		printf("*** THIS IS A CONTROLLED NMI ISSUED BY VMDUMP\n");
#endif
	} else {
	    if (! elsc)
		printf("%d%C %03d: ", hub_slot_get(), get_nasid());

    	printf("*** %s on node %d\n", message, get_nasid());

    	if (bad) {
			if (! elsc)
	    		printf("%d%C %03d: ", hub_slot_get(), get_nasid());

			if (epc)
	    		printf("*** Error EPC: 0x%lx (%P)\n",
		   			gprs.error_epc, gprs.error_epc);
			else
		    	printf("*** EPC: 0x%lx (%P)\n",
			   		gprs.epc, gprs.epc);
    	}

    	sc_disp(diagval);

    	/*
    	 * Scroll the diag string on the system controller display
    	 * until a key is pressed.
    	 */

    	flags.scroll_msg = 1;

    	if (bad) {
			if (! elsc)
    			printf("%d%C %03d: ", hub_slot_get(), get_nasid());
			printf("*** Press ENTER to continue.\n");
			scroll_n_print(diagval);
			getc();
    	}
	}

	/*
	 * When vmdump.c sends the NMI request to pod to park the CPU to
	 * make sure it is out of sight when the dump is being generated,
	 * it specifies in the nmi vector the address in which it wants the
	 * CPU to write something to acknowledge it has seen the request
	 * and is getting out of the way. It will write it's own nid into
	 * this address (anything could have been chosen). Vmdump spins on
	 * that address and continues only when it finds something in it.
	 * For more comments see vmdump.c.
	 * This fixes occassional dump buffer overwriting by the CPU
	 * whose node memory is being borrowed by vmdump to increase its
	 * buffer when generating the core dump. (PV 694136).
	 */

    if (nmi_addr->call_parm) {
		*(__psunsigned_t *)BDPRT_ENTRY(nmi_addr->call_parm, mynid) = MD_PROT_RW;
		*((nasid_t *)nmi_addr->call_parm) = mynid;
		*(__psunsigned_t *)BDPRT_ENTRY(nmi_addr->call_parm, mynid) = MD_PROT_RO;
#if DUMPDEBUG
		printf("*** pod.c: %d wrote into address: 0x%llx value: %d\n",
			mynid, nmi_addr->call_parm, *((nasid_t *)nmi_addr->call_parm));
	} else {
		printf("*** ***nmi_addr->call_parm is 0x%llx.\n", nmi_addr->call_parm);
#endif
    }
	 
    lastcomm[0] = 0;

    hub_led_set(PLED_PODPROMPT);

    for (;;) {
	parse_data_t	pd;
	char	       *bb, *bd;
	int		c, mod;
	int		prompt_col;
	char		altregs[32];
	char		id[16];

	/*
	 * to supress messages when we have the 'artificial second NMI' case
	 */
	if (diagval == KLDIAG_NMISECOND && nmi_addr->call_parm) {
		elsc = 0;
	} else {
		elsc = (libc_device(0) == &dev_elscuart);	/* May change */
	}

	if (elsc)
	    id[0] = 0;
	else
	    sprintf(id, "%d%C %03d: ", hub_slot_get(), get_nasid());

	/*
	 * to supress messages when we have the 'artificial second NMI' case
	 */
	if (! (diagval == KLDIAG_NMISECOND && nmi_addr->call_parm)) {
	sprintf(altregs, " AltRegs 0x%lx", flags.alt_regs);

	sprintf(buf,
		"%sPOD %s %s%s%s> ",
		id,
		device(),
		&RunStrings[(flags.mode & 3) * 4],
		mdir_error_check(get_nasid(), 0) ? " MDerr" : "",
		flags.alt_regs ? altregs : "");

	prompt_col = strlen(buf);

	puts(buf);
	}

	if (gets(buf, LINESIZE) == 0)
	    continue;

	mod = 0;

	/*
	 * Process command line substitution and history,
	 * consisting of:  !!, !$, and ^^, and `envvar`
	 */

	while ((bb = strstr(buf, "!!")) != 0 &&
	       strlen(buf) - 2 + strlen(lastcomm) < LINESIZE) {
	    strrepl(buf, bb - buf, 2, lastcomm);
	    mod = 1;
	}

	bd = lastcomm + strlen(lastcomm);
	while (bd > lastcomm && ! isspace(bd[-1]))
	    bd--;

	while ((bb = strstr(buf, "!$")) != 0 &&
	       strlen(buf) - 2 + strlen(bd) < LINESIZE) {
	    strrepl(buf, bb - buf, 2, bd);
	    mod = 1;
	}

	if (buf[0] == '^' && (bb = strchr(buf + 1, '^')) != 0) {
	    *bb++ = 0;
	    if ((bd = strstr(lastcomm, buf + 1)) != 0 &&
		strlen(lastcomm) - strlen(buf + 1) + strlen(bb) < LINESIZE) {
		strrepl(lastcomm, bd - lastcomm, strlen(buf + 1), bb);
		strcpy(buf, lastcomm);
		mod = 1;
	    }
	}

	while ((bb = strchr(buf, '`')) != 0 &&
	       (bd = strchr(bb + 1, '`')) != 0) {
	    *bd = 0;
	    /* Can use lastcomm as a temp since it's overwritten below */
	    if (ip27log_getenv(get_nasid(), bb + 1, lastcomm, 0,
			       IP27LOG_VERBOSE) < 0 ||
		strlen(buf) - (bd + 1 - bb) + strlen(lastcomm) >= LINESIZE)
		lastcomm[0] = 0;
	    strrepl(buf, bb - buf, bd + 1 - bb, lastcomm);
	    mod = 1;
	}

	if (mod) {
	    printf("%s\n", buf);
	    prompt_col = 0;
	}

	if (buf[0] != 0)
	    strcpy(lastcomm, buf);

	/*
	 * Call the parser
	 */

	switch (flags.alt_regs) {
	case 0:
	    pd.regs	= &gprs;
	    break;
	default:
	    pd.regs	= (struct reg_struct *)flags.alt_regs;
	    break;
	}

	pd.flags	= &flags;
	pd.next		= (rtc_time_t) 0;

	parse(&pd, buf, prompt_col);
    }
}


/* decode_diagval-
 *	This is _very_ preliminary code for coming up with real
 *		messages to display on the system controller.
 *	We need to do _much_ better.
 *	POD will need another parameter to describe where the error
 *		occurred.
 */
void
decode_diagval(char *diag_string, int diagval)
{
    /* We need to get FRU info, location, etc. */

    strcpy(diag_string, get_diag_string(KLDIAG_DIAGCODE(diagval)));

}

/*
 * set_unit_enable
 *
 *   Enables or disables a CPU, temporarily or permanently.
 *   Actually, you can't temporarily enable a CPU.
 */

int
set_unit_enable(nasid_t nasid, int slice, int enable, int temp)
{
    int		r;

    printf("%s %sabling NASID %d CPU %c\n",
	   temp ? "Temporarily" : "Permanently",
	   enable ? "en" : "dis",
	   nasid,
	   'A' + slice);

    if (temp) {
	if (enable)
	    r = -1;
	else {
	    SD(REMOTE_HUB(nasid, slice == 0 ?
			  PI_CPU_ENABLE_A : PI_CPU_ENABLE_B), 0);
	    r = 0;
	}
    } else {
	char	       *key, reason[64];

	key = (slice == 0) ? "DisableA" : "DisableB";

        sprintf(reason, "%sabled from IP27 POD", (enable ? "En" : "Dis"));

	if (enable)
            r = ed_cpu_mem(nasid, key, NULL, reason, 0, enable);
	else {
	    char	buf[64];

	    sprintf(buf, "%d: CPU disabled from IP27 POD",
			KLDIAG_CPU_DISABLED);
            r = ed_cpu_mem(nasid, key, buf, reason, 0, 0);
	}
    }

    return r;
}

/* set_mem_enable:
 *
 * enables or disables mem. if there is some mem disabled as per the log,
 * adds to or subtracts from the mask in the log
 */

int
set_mem_enable(nasid_t nasid, char *mask, int enable)
{
   char		mem[MD_MEM_BANKS+1], mem_dis_reason[MD_MEM_BANKS+1];
   char		dis_mem_sz[MD_MEM_BANKS+1], reason[64];
   char		*chr;
   int		j, i, r, len;
   __uint64_t	md_mem_cfg = translate_hub_mcreg(REMOTE_HUB_L(nasid, 
						      MD_MEMORY_CONFIG));

   bzero(mem, MD_MEM_BANKS+1);

   ip27log_getenv(nasid, DISABLE_MEM_SIZE, dis_mem_sz, "00000000", 0);
   ip27log_getenv(nasid, DISABLE_MEM_REASON, mem_dis_reason, "00000000", 0);
   r = ip27log_getenv(nasid, DISABLE_MEM_MASK, mem, 0, 0);

   len = (strlen(mask) > MD_MEM_BANKS ? MD_MEM_BANKS : strlen(mask));

   if (r < 0) {
      if (!enable) {
	 j = 0;
	 for (i = 0; i < len; i++) {
            int	bank = mask[i] - '0';

            if (bank >= MD_MEM_BANKS)
                continue;

	    if (isdigit(mask[i])) {
	       mem[j++] = mask[i];
               dis_mem_sz[bank] = '0' + ((md_mem_cfg & MMC_BANK_MASK(bank)) >>
					MMC_BANK_SHFT(bank));
               mem_dis_reason[bank] = '0' + MEM_DISABLE_USR;
            }
         }
      }
   }
   else {
      if (enable) {
	 for (i = 0; i < len; i++) {
	    if (chr = strchr(mem, mask[i])) {
	       while (*chr) {
		  *chr = *(chr + 1);
		  chr++;
	       }
	    }
	 }
      }
      else {
	 for (i = 0; i < len; i++) {
            int	bank = mask[i] - '0';

            if (bank >= MD_MEM_BANKS)
                continue;

	    if (isdigit(mask[i]) && !strchr(mem, mask[i])) {
	       mem[strlen(mem)] = mask[i];
               dis_mem_sz[bank] = '0' + ((md_mem_cfg & MMC_BANK_MASK(bank)) >>
					MMC_BANK_SHFT(bank));
               mem_dis_reason[bank] = '0' + MEM_DISABLE_USR;
            }
	 }
      }
   }

   if (enable) {
        if (!strchr(mem, '0'))
	    ip27log_unsetenv(nasid, SWAP_BANK, 0) ;
   }
   else { /* disable */
      if (strchr(mem, '0') && (strchr(mem, '1') || 
		((md_mem_cfg & MMC_BANK_MASK(1)) >> MMC_BANK_SHFT(1) == 
		MD_SIZE_EMPTY))) {
         char	gbuf[8];

         printf("Disabling banks 0 and 1 will result in the system being "
		"unable to boot. Disable [n] ?");

         gets_timeout(gbuf, sizeof (gbuf), 5000000, "n");

         if ((gbuf[0] == 'n') || (gbuf[0] == 'N')) {
            printf("No bank(s) disabled.\n");
            return 0;
         }
      }
   }
 	
   ip27log_setenv(nasid, DISABLE_MEM_SIZE, dis_mem_sz, 0);
   ip27log_setenv(nasid, DISABLE_MEM_REASON, mem_dis_reason, 0);

   sprintf(reason, "%sabled from IP27 POD", (enable ? "En" : "Dis"));
   r = ed_cpu_mem(nasid, DISABLE_MEM_MASK, mem, reason, 0, enable);

   printf("mem banks %s %sabled. Reset to make it functional\n",
		mem, (enable ? "en" : "dis"));

   return r;
}

void
dump_entry(int index)
{
    __uint64_t lo0, lo1, hi;

    hi = tlb_get_enhi(index);
    lo0 = tlb_get_enlo(index, 0);
    lo1 = tlb_get_enlo(index, 1);

    printf("hi: %016lx, lo0: %016lx, lo1: %016lx\n",
		hi, lo0, lo1);
    printf("%02x: ASID %02x, VPN %x -> PFN %x, %c%c%c; PFN %x, %c%c%c\n",
	   index,
	   hi & 0xff,
	   (hi >> 13) << 1,
	   lo0 >>6,
	   lo0 & TLBLO_D ? (int)'D' : 0x20,
	   lo0 & TLBLO_V ? (int)'V' : 0x20,
	   lo0 & TLBLO_G ? (int)'G' : 0x20,
	   lo1 >>6,
	   lo1 & TLBLO_D ? (int)'D' : 0x20,
	   lo1 & TLBLO_V ? (int)'V' : 0x20,
	   lo1 & TLBLO_G ? (int)'G' : 0x20);
}

void
dump_tlb(int index)	/* -1 for all */
{
    int i;

    if (index < 0) {
	for (i=0; i < NTLBENTRIES; i++)
	    dump_entry(i);
    } else if (index >= 0 && index < NTLBENTRIES) {
	dump_entry(index);
    } else
	printf("*** TLB slot out of range.\n");
}

#define	ICACHE_ADDR(line, way)	(((line) * CACHE_ILINE_SIZE) + K0BASE + (way))
#define	DCACHE_ADDR(line, way)	(((line) * CACHE_DLINE_SIZE) + K0BASE + (way))
#define	SCACHE_ADDR(line, way)	(((line) * CACHE_SLINE_SIZE) + K0BASE + (way))

static	void
printPrimaryCacheLine(__uint64_t tag, __uint64_t addr, __uint64_t *data, int len)
/*
 * Function: printCacheLine
 * Purpose: To print out a primary cache line
 * Parameters:	tag - the primary cache line tag
 *              addr- address used
 *		data- pointer to the data
 *		len - length of data
 * Returns: nothing
 */
{
    /* XXX fixme: CTP_STATEMOD_MASK in sys/R10k.h can't be used since */
    /* shift count is too large */
#undef	CTP_STATEMOD_MASK
#define	CTP_STATEMOD_MASK	(0xfL<<CTP_STATEMOD_SHFT)

#   define	STATEMOD(t) (((t) & CTP_STATEMOD_MASK) >> CTP_STATEMOD_SHFT)
    static char * stateMod[] = {
	"invalid",
	"normal ",
	"incon  ",
	"invalid",
	"refill ",
	"invalid",
	"invalid"
    };

#   define	STATE(t) (((t) & CTP_STATE_MASK) >> CTP_STATE_SHFT)
    static char * state[] = {
	"invalid  ",
	"shared   ",
	"clean-ex ",
	"dirty-ex "
    };
    __uint32_t	*d;

    printf("Tag 0x%x address= 0x%x s=(%d)%s sm=(%d)%s ",
	   tag & CTP_TAG_MASK,
           ((__uint64_t) ((tag & CTP_TAG_MASK) >> CTP_TAG_SHFT) << 12)  
           | (addr & ( len/ 2 -1)),
	   STATE(tag), state[STATE(tag)],
	   STATEMOD(tag), stateMod[STATEMOD(tag)]);
    printf("sp(%d) scw(%d) lru(%d)\n",
	   (tag & CTP_STATEPARITY_MASK) >> CTP_STATEPARITY_SHFT,
	   (tag & CTP_SCW) ? 1 : 0, (tag & CTP_LRU) ? 1 : 0);

    d = (__uint32_t *)data;
    while (len && d) {
	printf("\t0x%x 0x%x 0x%x 0x%x\n",
	       (__uint64_t) d[0], (__uint64_t)d[1],
	       (__uint64_t) d[2], (__uint64_t)d[3]);
	len -= 16;
	d   += 16/sizeof(*d);
    }
#   undef	STATEMOD
#   undef	STATE
}

static	void
printSecondaryCacheLine(__uint64_t tagT5, __uint64_t addr, __uint64_t *data)
/*
 * Function: printSecondaryCacheLine
 * Purpose: To print out a secondary cache line
 * Parameters:	tagT5 - the secondary cache line tagfrom T5
 *		data- pointer to the data
 * Returns: nothing
 */
{
    int		len;
    __uint32_t	*d;
    __uint64_t  tmpAddr;
    __uint64_t  maskAddr = ~((CACHE_SLINE_SIZE / 2) - 1);

#   define	STATE(t) (((t) & CTS_STATE_MASK) >> CTS_STATE_SHFT)
    static char * state[] = {
	"invalid ",
	"shared  ",
	"clean-ex",
	"dirty-ex",
    };

    tmpAddr = ((__uint64_t) ((tagT5 & CTS_TAG_MASK) >> CTS_TAG_SHFT) << 18) & maskAddr;
    tmpAddr += addr & ~maskAddr;

    /* Print the tag information only for the tag specfic commands */
    if (!data)
	printf("T5 0x%x addr 0x%x state=(%d)%s Vidx(%d) ECC(0x%02x) MRU(%d)\n",
	       tagT5 & CTS_TAG_MASK, tmpAddr,
	       STATE(tagT5), state[STATE(tagT5)],
	       (tagT5 & CTS_VIDX_MASK) >> CTS_VIDX_SHFT,
	       tagT5 & CTS_ECC_MASK, (tagT5 & CTS_MRU) ? 1 : 0);

    d = (__uint32_t *)data;
    for (len = CACHE_SLINE_SIZE; len && data; len -= 16, d += 4) {
	printf("\t0x%x 0x%x 0x%x 0x%x\n",
	       (__uint64_t)d[0], (__uint64_t)d[1],
	       (__uint64_t)d[2], (__uint64_t)d[3]);
    }
#   undef	STATE
}

void
dumpSecondaryLine(int line, int way, int contents)
/*
 * Function: dumpSecondaryLine
 * Purpose: To display a secondary cache line contents.
 * Parameters:  line - line # (Index) to dump
 *		way - cahe line way
 *		contents - if true, dump contexts of line with tags.
 * Returns: nothing
 */
{
    __uint64_t	tagT5;
    __uint64_t	b[CACHE_SLINE_SIZE / sizeof(__uint64_t)];

    cache_get_s(SCACHE_ADDR(line, way), &tagT5, NULL, contents ? b : NULL);
    printSecondaryCacheLine(tagT5, SCACHE_ADDR(line, way), contents ? b : NULL);
}

void
dumpSecondaryLineAddr(__uint64_t addr, int way, int contents)
{
    __uint64_t	tagT5;
    __uint64_t	b[CACHE_SLINE_SIZE / sizeof(__uint64_t)];

    addr |= K0BASE;

    cache_get_s((addr & ~(CACHE_SLINE_SIZE-1)) + way, &tagT5,NULL,
		contents ? b : NULL);
    printSecondaryCacheLine(tagT5, addr, contents ? b : NULL);
}

int
dumpPrimaryInstructionLine(int line, int way, int contents)
/*
 * Function: dumpPrimaryLine
 * Purpose: To retireve and dump a primary cache line.
 * Parameters:  line- line number to dump
 *		way - which cache way 0/1
 *		contents - 0 - tags only, 1 - data too.
 * Returns: 0 OK, !0 failed
 */
{
    __uint64_t	b[CACHE_ILINE_SIZE/sizeof(__uint64_t)];
    __uint64_t	tag;

    if (line >= cache_size_i() / CACHE_ILINE_SIZE) {
	return(1);
    }

    tag = cache_get_i(ICACHE_ADDR(line, way), contents ? (int *)b : 0);
    printPrimaryCacheLine(tag, ICACHE_ADDR(line, way), contents ? b : 0, CACHE_ILINE_SIZE);
    return(0);
}
int
dumpPrimaryInstructionLineAddr(__uint64_t addr, int way, int contents)
/*
 * Function: dumpPrimaryLine
 * Purpose: To retireve and dump a primary cache line.
 * Parameters:  line- line number to dump
 *		way - which cache way 0/1
 *		contents - 0 - tags only, 1 - data too.
 * Returns: 0 OK, !0 failed
 */
{
    __uint64_t	b[CACHE_ILINE_SIZE/sizeof(__uint64_t)];
    __uint64_t	tag;

    addr |= K0BASE;
    addr &= ~(CACHE_ILINE_SIZE - 1);
    tag = cache_get_i(addr + way, contents ? (int *)b : 0);
    printPrimaryCacheLine(tag,addr,contents ? b : 0, CACHE_ILINE_SIZE);
    return(0);
}

int
dumpPrimaryDataLine(int line, int way, int contents)
/*
 * Function: dumpPrimaryDataLine
 * Purpose: To retireve and dump a primary cache line.
 * Parameters:  line- line number to dump
 *		way - which cache way 0/1
 *		contents - 0 - tags only, 1 - data too.
 * Returns: 0 OK, !0 failed
 */
{
    __uint64_t	b[CACHE_DLINE_SIZE/sizeof(__uint64_t)];
    __uint64_t	tag;

    if (line >= cache_size_d() / CACHE_DLINE_SIZE) {
	return(1);
    }

    tag = cache_get_d(DCACHE_ADDR(line, way), contents ? (int *)b : 0);
    printPrimaryCacheLine(tag, DCACHE_ADDR(line, way), contents ? b : 0, CACHE_DLINE_SIZE);
    return(0);
}
int
dumpPrimaryDataLineAddr(__uint64_t addr, int way, int contents)
/*
 * Function: dumpPrimaryDataLine
 * Purpose: To retireve and dump a primary cache line.
 * Parameters:  line- line number to dump
 *		way - which cache way 0/1
 *		contents - 0 - tags only, 1 - data too.
 * Returns: 0 OK, !0 failed
 */
{
    __uint64_t	b[CACHE_DLINE_SIZE/sizeof(__uint64_t)];
    __uint64_t	tag;

    addr |= K0BASE;
    addr &= ~(CACHE_DLINE_SIZE - 1);

    tag = cache_get_d(addr + way, contents ? (int *)b : 0);
    printPrimaryCacheLine(tag, addr, contents ? b : 0, CACHE_DLINE_SIZE);
    return(0);
}

void
dumpPrimaryCache(int contents)
/*
 * Function: dumpPrimaryCache
 * Purpose: Display the entire primary data Cache
 * Parameters: contents - if true, display cache contents
 * Returns: nothing
 */
{
    int	lines, i;

    lines = (cache_size_d() / CACHE_DLINE_SIZE) / 2;

    for (i = 0; i < lines; i++) {
	dumpPrimaryDataLine(i, 0, contents);
	dumpPrimaryDataLine(i, 1, contents);
    }
}

void
dumpSecondaryCache(int contents)
/*
 * Function: dumpSecondaryCache
 * Purpose: To dump the secondary cache tags and contents
 * Parameters: contents - true--> dump tags + DATA,
 * Returns: nothing
 */
{
    int	lines, i;

    lines  = (cache_size_s() / CACHE_SLINE_SIZE) / 2;

    for (i = 0; i < lines; i++) {
	dumpSecondaryLine(i, 0, contents); /* way 0 */
	dumpSecondaryLine(i, 1, contents); /* way 1 */
    }
}

/*
 * Version of sload for downloading code via RS232
 * Assume RS232 port has been initialized
 */

#define	ACK	0x6
#define	NAK	0x15

#define DIGIT(c)	((c)>'9'?((c)>='a'?(c)-'a':(c)-'A')+10:(c)-'0')

__scunsigned_t jump_addr(__psunsigned_t address,
			 __scunsigned_t parm1, __scunsigned_t parm2,
			 struct flag_struct *flags)
{
    __scunsigned_t ret_val;
    __uint64_t sregs[10];

    if (! flags->silent)
	printf("Jump address is %lx\n", address);

    save_sregs(sregs);

    /*
     * Jump to the address passing the appropriate parameters.
     * If running from cached memory, flush and invalidate the
     * caches first.
     */

    if (flags->mode == POD_MODE_CAC)
	cache_flush();

    ret_val = jump_inval(address, parm1, parm2,
			 (flags->mode == POD_MODE_CAC) ?
			 (JINV_ICACHE | JINV_SCACHE) : 0,
			 0); /* No new sp */

    restore_sregs(sregs);
    return ret_val;
}

int
call_asm(uint (*function)(__scunsigned_t), __scunsigned_t parm)
{

    __uint64_t sregs[10];
    int result;

    result = save_and_call(sregs, function, parm);

    return result;
}

/* Write to the Everest System Reset register. */

void reset_system(void)
{
    delay(0x40000);

    /* Reset this hub and the network */
    SD(LOCAL_HUB(NI_PORT_RESET), NPR_PORTRESET | NPR_LOCALRESET);
}

/*
 * fled_die
 *
 *   Flashes a FLED_* led value.
 *   Waits for a CTRL(C) to be pressed.
 *   If pod is true, jumps to POD mode; else returns.
 *
 *   Flags:
 *	FLED_NOTTY	Do not attempt to poll UART; it's unavailable
 *	FLED_POD	Go to POD mode if ^C is pressed on UART
 *	FLED_CONT	Return if ^C is pressed on UART
 */

#define CTRL(x)		((x) & 0x1f)
#define FLASH_PERIOD	500000	/* microseconds */

void fled_die(__uint64_t leds, int flags)
{
   rtc_time_t	expire;
   int		i;

   expire = rtc_time();

   hub_alive_set(hub_cpu_get(), 0);

   /* If the elsc display for this node is not already holding an error
      value, take set the taken bit and display the error code. */ 
   if (!hub_display_taken()) {
       /* Setting this prevents the error code from being overwritten by the
       other processor. */
       hub_set_display_taken();

       hub_display_digits(hub_slot_get(),leds);
   }

   for (i = 0;
	(flags & FLED_NOTTY) || ! poll() || readc() != CTRL('C');
	i ^= leds) {
       hub_led_set(i);
       expire += FLASH_PERIOD / 2;
       while (rtc_time() < expire) ;
   }

   if (flags & FLED_POD)
       pod_mode(-POD_MODE_DEX, KLDIAG_DEBUG, "Failure LEDs interrupted");

   if ((flags & FLED_CONT) == 0)
       hub_halt();
}

static char *
cpu_inv_str(int cpu_inv_state)
{
    if (cpu_inv_state == IP27_INV_ENABLE)
        return "Present & Enabled";
    else if (cpu_inv_state == IP27_INV_PRESENT)
        return "Present & Disabled";
    else
        return "Absent";
}

void
ip27_die(__uint64_t leds)
{
    __uint64_t	sr1;

    SET_PROGRESS(leds);
    SET_PROGRESS_OTHER(leds);

    
    LOCAL_HUB_S(NI_SCRATCH_REG0, 0);
    sr1 = LOCAL_HUB_L(NI_SCRATCH_REG1);
    sr1 |= (0x3ULL << (ADVERT_CPUMASK_SHFT + 2));
    LOCAL_HUB_S(NI_SCRATCH_REG1, sr1);
    LOCAL_HUB_S((LOCAL_HUB_L(PI_CPU_NUM) ? PI_CPU_ENABLE_A : PI_CPU_ENABLE_B),
	0);
    LOCAL_HUB_S((LOCAL_HUB_L(PI_CPU_NUM) ? PI_CPU_ENABLE_B : PI_CPU_ENABLE_A),
	0);
}

void
ip27_inventory(nasid_t nasid)
{
    int         bank;
    __uint64_t  mdir_config;
    char        old_cpu_mask[4], cpu_mask[4];
    char        mem_dis[16], mem_mask[16], old_mem_mask[16];
    char        mem_dis_sz[16], mem_sz[16], old_mem_sz[16];
    char	swap_bank[4];

    bzero(cpu_mask, 4);
    bzero(old_cpu_mask, 4);

    if (REMOTE_HUB_L(nasid, PI_CPU_PRESENT_A)) {
        if (REMOTE_HUB_L(nasid, PI_CPU_ENABLE_A))
            cpu_mask[0] = '0' + IP27_INV_ENABLE;
        else
            cpu_mask[0] = '0' + IP27_INV_PRESENT;
    }

    if (REMOTE_HUB_L(nasid, PI_CPU_PRESENT_B)) {
        if (REMOTE_HUB_L(nasid, PI_CPU_ENABLE_B))
            cpu_mask[1] = '0' + IP27_INV_ENABLE;
        else
            cpu_mask[1] = '0' + IP27_INV_PRESENT;
    }

    if (ip27log_getenv(nasid, INV_CPU_MASK, old_cpu_mask, 0, 0) >= 0) {
        if (cpu_mask[0] != old_cpu_mask[0]) {
            ip27log_printf_nasid(nasid, IP27LOG_INFO, "CPU A was previously %s "                "but is now %s \n", cpu_inv_str(old_cpu_mask[0] - '0'),
                cpu_inv_str(cpu_mask[0] - '0'));
            printf("*** Nasid %d: CPU A was previously %s but is now %s\n",
                nasid, cpu_inv_str(old_cpu_mask[0] - '0'),
                cpu_inv_str(cpu_mask[0] - '0'));
        }

        if (cpu_mask[1] != old_cpu_mask[1]) {
            ip27log_printf_nasid(nasid, IP27LOG_INFO, "CPU B was previously %s "                "but is now %s\n", cpu_inv_str(old_cpu_mask[1] - '0'),
                cpu_inv_str(cpu_mask[1] - '0'));
            printf("*** Nasid %d: CPU B was previously %s but is now %s\n",
                nasid, cpu_inv_str(old_cpu_mask[1] - '0'),
                cpu_inv_str(cpu_mask[1] - '0'));
        }
    }

    ip27log_setenv(nasid, INV_CPU_MASK, cpu_mask, 0);

    mdir_config = translate_hub_mcreg(REMOTE_HUB_L(nasid, MD_MEMORY_CONFIG));

    bzero(mem_dis, 16);
    bzero(mem_mask, 16);
    bzero(old_mem_mask, 16);

    ip27log_getenv(nasid, DISABLE_MEM_MASK, mem_dis, 0, 0);

    for (bank = 0; bank < MD_MEM_BANKS; bank++) {
        if ((mdir_config & MMC_BANK_MASK(bank)) >> 
		MMC_BANK_SHFT(bank) != MD_SIZE_EMPTY)
            mem_mask[bank] = '0' + IP27_INV_ENABLE;
        else if (strchr(mem_dis, '0' + bank))
            mem_mask[bank] = '0' + IP27_INV_PRESENT;
        else
            mem_mask[bank] = '0';
    }

    if (ip27log_getenv(nasid, INV_MEM_MASK, old_mem_mask, 0, 0) >= 0) {
        for (bank = 0; bank < MD_MEM_BANKS; bank++)
            if (mem_mask[bank] != old_mem_mask[bank]) {
                ip27log_printf_nasid(nasid, IP27LOG_INFO, "Memory bank %d was "
                        "previously %s but is now %s\n", bank,
                        cpu_inv_str(old_mem_mask[bank] - '0'),
                        cpu_inv_str(mem_mask[bank] - '0'));
                printf("*** Nasid %d: Memory bank %d was previously %s but is "
                        "now %s\n", nasid, bank,
                        cpu_inv_str(old_mem_mask[bank] - '0'),
                        cpu_inv_str(mem_mask[bank] - '0'));
            }
    }

    ip27log_setenv(nasid, INV_MEM_MASK, mem_mask, 0);

    bzero(mem_dis_sz, 16);
    bzero(mem_sz, 16);
    bzero(old_mem_sz, 16);

    ip27log_getenv(nasid, DISABLE_MEM_SIZE, mem_dis_sz, 0, 0);

    for (bank = 0; bank < MD_MEM_BANKS; bank++) {
        if ((mdir_config & MMC_BANK_MASK(bank)) >> 
		MMC_BANK_SHFT(bank) != MD_SIZE_EMPTY)
            mem_sz[bank] = '0' + ((mdir_config & MMC_BANK_MASK(bank)) >>
                MMC_BANK_SHFT(bank));
        else if (strchr(mem_dis, '0' + bank))
            mem_sz[bank] = mem_dis_sz[bank];
        else
            mem_sz[bank] = '0';
    }

    if (ip27log_getenv(nasid, INV_MEM_SZ, old_mem_sz, 0, 0) >= 0) {
        for (bank = 0; bank < MD_MEM_BANKS; bank++)
            if (mem_sz[bank] != old_mem_sz[bank]) {
                ip27log_printf_nasid(nasid, IP27LOG_INFO, "Memory bank %d was "
                        "previously had %d MB but now has %d MB\n", bank,
                        MD_SIZE_MBYTES(old_mem_sz[bank] - '0'),
                        MD_SIZE_MBYTES(mem_sz[bank] - '0'));
                printf("*** Nasid %d: Memory bank %d was previously had %d MB "
                        "but now has %d MB\n", nasid, bank,
                        MD_SIZE_MBYTES(old_mem_sz[bank] - '0'),
                        MD_SIZE_MBYTES(mem_sz[bank] - '0'));
            }
    }

    ip27log_setenv(nasid, INV_MEM_SZ, mem_sz, 0);
}
