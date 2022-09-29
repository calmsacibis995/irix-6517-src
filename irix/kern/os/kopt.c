/* ------------------------------------------------------------------ */
/* | Copyright Unpublished, MIPS Computer Systems, Inc.  All Rights | */
/* | Reserved.  This software contains proprietary and confidential | */
/* | information of MIPS and its suppliers.  Use, disclosure or     | */
/* | reproduction is prohibited without the prior express written   | */
/* | consent of MIPS.                                               | */
/* ------------------------------------------------------------------ */
#ident "$Revision: 3.102 $"

#include "sys/debug.h"
#include "sys/types.h"
#include "sys/sysmacros.h"
#include "sys/param.h"
#include "sys/systm.h"
#include "sys/signal.h"
#include "sys/errno.h"
#include "sys/sbd.h"
#include "sys/immu.h"
#include "sys/pcb.h"
#include "sys/kopt.h"
#include "sys/cpu.h"
#include <string.h>

#include "sys/conf.h"

#ifdef _K64PROM32
#include <sys/EVEREST/evaddrmacros.h>
#define STR_ADDR(_x)	(char *)K032TOKPHYS(_x)
#else
#define	STR_ADDR(_x)	(_x)
#endif

/*
 * We need to declare static storage for the args to be recognized
 * by the kernel. It turns out that the only time these args are
 * guaranteed to be good is at boot time so they must be copied into
 * local storage. The sizes of these local strings are setup here.
 */
char arg_dbaud[6];		/* max value is 38400 */
char arg_tapedevice[200];	/* Must be big enough to handle remote dirs */
char arg_root[MAXDEVNAME];	/* hwgraph locator strings are longer */
char arg_swap[MAXDEVNAME];
char arg_showconfig[10] = FALSE_VALUE;
char arg_initfile[40];
char arg_initstate[10];
char arg_swplo[20];
char arg_nswap[20];
char arg_console[10];
char arg_gfx[10];
char arg_keybd[10];
char arg_nogfxkbd[2];
char arg_cpufreq[4];
char arg_monitor[9];			/* 8 chars on IP30 */
char arg_sync_on_green[2];
char arg_diskless[2];
char arg_netaddr[17];
char arg_srvaddr[17];
char arg_hostname[20];
char arg_dlserver[17];
char arg_dlif[4];
char arg_dlgate[17];
char arg_dllogin[20];
char arg_maxpmem[7];
char arg_debug_bigmem[2];

#if MP
char arg_splockmeteron[10];
#endif

char arg_pagecolor[7];
char arg_screencolor[7];
char arg_logocolor[7];

#if IP20 || IP22 || IP26 || IP28 || IP30 || IP32 || IPMHSIM
char arg_eaddr[18];
char arg_rdebug[4];
char arg_dbgmon[4];
char arg_nodbgmon[4];
char arg_wd93retry[4];
char arg_wd93hostid[2];
#endif

#if IP20 || IP22 || IP26 || IP28 || IP30 || IP32 || IPMHSIM || IP27
char arg_volume[4];
#endif

char arg_autoload[2];
char arg_oslpart[64];
char arg_osloader[19];
char arg_osfile[20];
char arg_osopts[13];
char arg_syspart[64];
char arg_sysfile[30];
char arg_diagmode[3];
char arg_sgilogo[2];
char arg_rbaud[6];

#if IP22 || IP26 || IP28 || EVEREST || IP30
char arg_rebound[2];
#endif

#if IP22 || EVEREST
char arg_wd95hostid[2];
#endif
#ifdef EVEREST
/*
 * @@: For NVRAM Fru logging; see #519323 
 *     - Need to log panic output to NVRAM on Challenge
 */
#include <sys/EVEREST/nvram.h>
char arg_evfru_outp[NVLEN_FRUDATA];
#endif

#if IP27 || IP30 || IP33
char arg_qlhostid[2];
#endif

#if SN
char arg_maxnodes[4];
char arg_probeallscsi[2] ;
char arg_probewhichscsi[128] ;
#endif

#ifdef IP22
char arg_prompoweroff[2];
#ifdef DEBUG
char arg_ic_debugflags[11];	/* hex */
#endif
#endif

char arg_wd93_syncenable[6];

#ifdef _SYSTEM_SIMULATION
char arg_sablediskpart[10]; 
char arg_sableexectrace[20];
char arg_sableio[10];
#endif 

#ifdef TRITON
char arg_triton_invall[10];
#endif /* TRITON */

#if IP26 || IP28 || IP30
char arg_boottune[2];
#endif

#if IP30
char arg_bootmaster[2];
char arg_ef0mode[7];
#endif

#ifdef HEART_COHERENCY_WAR
char arg_heart_read_war[2];
char arg_heart_write_war[2];
#endif
#ifdef HEART_INVALIDATE_WAR
char arg_heart_invalidate_war[2];
#endif

char pci_no_auto[2];
char arg_mrmode[13];     /* inst mode in miniroot */
char arg_mrconfig[128];
char arg_netmask[17];

char arg_kernname[100];		/* arcs pathnames can be very long */	

#ifdef IP32
char arg_ec0mode[5];
char arg_bump_leds[4];
char arg_reset_dump[4];
char arg_ecc_enabled[6];
char arg_videotiming[16];
char arg_videooutput[16];
char arg_videoinput[16];
char arg_videostatus[16];
char arg_videogenlock[16];
char arg_audiopass[2];
char arg_ip32nokp[2];
char arg_crt_option[10];
char arg_mfgsite[20];
char arg_proctype[20];
char arg_testmod[20];
#endif
#if defined(EVEREST) && defined(MULTIKERNEL)
char arg_initmkcell_serial[7];
char arg_evmk_numcells[4];
#endif
#if CELL_IRIX
char arg_golden_cell[4];
char arg_firewall[2];
#endif

#if CELL
char arg_num_cells[4];
#endif /* CELL */

#if defined (CELL_IRIX) && defined (LOGICAL_CELLS)
char arg_subcells[4];
char arg_max_subcells[4];
#endif /*  (CELL_IRIX) && (LOGICAL_CELLS) */

#ifdef _R5000_CVT_WAR
char arg_r5000_cvt_war[4];
#endif /* _R5000_CVT_WAR */

#ifdef _R5000_BADVADDR_WAR
char arg_rm5271_badvaddr_war[4];
#endif /* _R5000_BADVADDR_WAR */



#if defined(SN0) && defined(SN0_HWDEBUG)
char arg_disable_bte[32];
char arg_disable_bte_pbcopy[32];
char arg_disable_bte_pbzero[32];
char arg_disable_bte_poison_range[32];
char arg_iio_bte_crb_count[32];
char arg_iio_icmr_c_cnt[32];
char arg_iio_ictp[32];
char arg_iio_icto[32];
char arg_iio_icmr_precise[32];

char arg_la_trigger_nasid1[32];
char arg_la_trigger_nasid2[32];
char arg_la_trigger_val[32];
#endif


/*
 * The following is a table of symbolic names and addresses of kernel
 * variables which can be tuned to alter the performance of the system.
 * They can be modified at boot time as a boot parameter or by the sgikopt
 * system call.  Variables marked as readonly can't be modifed after system
 * boot time (i.e. through the sgikopt call).
 * maxpmem does the same thing as changing MAXPMEM in master.d/kernel,
 * but doesn't require reconfiguring the kernel; it is specified the same
 * way, in pages.
 */
static struct kernargs kernargs[] = {
	{ "dbaud",	arg_dbaud,	sizeof(arg_dbaud),	1, },
	{ "tapedevice",	arg_tapedevice,	sizeof(arg_tapedevice),	1, },
	{ "root",	arg_root,	sizeof(arg_root),	1, },
	{ "swap",	arg_swap,	sizeof(arg_swap),	1, },
	{ "showconfig",	arg_showconfig,	sizeof(arg_showconfig),	1, },
	{ "initfile",	arg_initfile,	sizeof(arg_initfile),	1, },
	{ "initstate",	arg_initstate,	sizeof(arg_initstate),	1, },
	{ "swaplo",	arg_swplo,	sizeof(arg_swplo),	1, },
	{ "nswap",	arg_nswap,	sizeof(arg_nswap),	1, },
#ifndef EVEREST
	{ "sync_on_green", arg_sync_on_green, sizeof(arg_sync_on_green), 1, },
#endif
#if MP
	{ "splockmeteron", arg_splockmeteron, sizeof(arg_splockmeteron), 0, },
#endif
	{ "console",	arg_console,	sizeof(arg_console),	1, },
	{ "gfx",	arg_gfx,	sizeof(arg_gfx),	1, },
	{ "keybd",	arg_keybd,	sizeof(arg_keybd),	1, },
	{ "nogfxkbd",	arg_nogfxkbd,	sizeof(arg_nogfxkbd),	1, },
	{ "cpufreq",	arg_cpufreq,	sizeof(arg_cpufreq),	1, },
	{ "monitor",	arg_monitor,	sizeof(arg_monitor),	1, },
	{ "pagecolor",	arg_pagecolor,	sizeof(arg_pagecolor),	1, },
	{ "screencolor",arg_screencolor,sizeof(arg_screencolor),1, },
	{ "logocolor",	arg_logocolor,	sizeof(arg_logocolor),	1, },
	{ "diskless",	arg_diskless,	sizeof(arg_diskless),	1, },
	{ "srvaddr",	arg_srvaddr,	sizeof(arg_srvaddr),	1, },
	{ "netaddr",	arg_netaddr,	sizeof(arg_netaddr),	1, },
	{ "diagmode",	arg_diagmode,	sizeof(arg_diagmode),	1, },
	{ "hostname",	arg_hostname,	sizeof(arg_hostname),	1, },
	{ "dlserver",	arg_dlserver,	sizeof(arg_dlserver),	1, },
	{ "dlgate",	arg_dlgate,	sizeof(arg_dlgate),	1, },
	{ "dlif",	arg_dlif,	sizeof(arg_dlif),	1, },
	{ "dllogin",	arg_dllogin,	sizeof(arg_dllogin),	1, },
	{ "maxpmem",	arg_maxpmem,	sizeof(arg_maxpmem),	1, },
	{ "debug_bigmem", arg_debug_bigmem, sizeof(arg_debug_bigmem),	1, },
#if IP20 || IP22 || IP26 || IP28 || IP30 || IP32 || IPMHSIM
	{ "eaddr",	arg_eaddr,	sizeof(arg_eaddr),	0, },
	{ "rdebug",	arg_rdebug,	sizeof(arg_rdebug),	1, },
	{ "dbgmon",	arg_dbgmon,	sizeof(arg_dbgmon),	1, },
	{ "nodbgmon",	arg_nodbgmon,	sizeof(arg_nodbgmon),	1, },
#ifndef IP30
	{ "scsiretries",	arg_wd93retry,	sizeof(arg_wd93retry),	1, },
#endif
#endif
#if IP20 || IP22 || IP26 || IP28 || IP30 || IP32 || IPMHSIM || IP27
	{ "volume",       arg_volume,       sizeof(arg_volume), 1, },
#endif
	{ "scsi_syncenable", arg_wd93_syncenable, sizeof(arg_wd93_syncenable), 1},

	{ "SystemPartition", arg_syspart, sizeof(arg_syspart),  1, },
	{ "OSLoadPartition", arg_oslpart, sizeof(arg_oslpart),	1, },
	{ "OSLoader",	arg_sysfile,	sizeof(arg_sysfile),	1, },
	{ "OSLoadFilename", arg_osfile,	sizeof(arg_osfile), 	1, },
	{ "OSLoadOptions", arg_osopts,	sizeof(arg_osopts),	1, },
	{ "AutoLoad",	arg_autoload,	sizeof(arg_autoload),	1, },
	{ "sgilogo",	arg_sgilogo,	sizeof(arg_sgilogo),	1, },
        { "rbaud",      arg_rbaud,      sizeof(arg_rbaud),      1, },
	{ "passwd_key",	"",	1,	0, }, /* can only be cleared */

#if EVEREST || IP22 || IP26 || IP28 || IP30
	{ "rebound", arg_rebound, sizeof(arg_rebound), 1, },
#endif

#if EVEREST || IP22
	{ "scsihostid", arg_wd95hostid, sizeof(arg_wd95hostid), 1, },
#endif

#if IP20 || IP22 || IP26 || IP28
	{ "scsihostid", arg_wd93hostid, sizeof(arg_wd93hostid), 1, },
#endif

#if IP27 || IP30
	{ "scsihostid", arg_qlhostid, sizeof(arg_qlhostid), 1, },
#endif

#if SN
	{ "maxnodes",	arg_maxnodes,	sizeof(arg_maxnodes),	1, },
	{ "ProbeAllScsi", arg_probeallscsi, sizeof(arg_probeallscsi), 1, },
	{ "ProbeWhichScsi", arg_probewhichscsi, sizeof(arg_probewhichscsi), 1,},
#endif

#if defined(EVEREST) && defined(MULTIKERNEL)
	{ "initmkcell_serial", arg_initmkcell_serial, sizeof(arg_initmkcell_serial),1, },
	{ "evmk_numcells", arg_evmk_numcells, sizeof(arg_evmk_numcells),1, },
#endif
#if CELL_IRIX
	{ "golden_cell", arg_golden_cell, sizeof(arg_golden_cell),1, },
	{ "firewall", arg_firewall, sizeof(arg_firewall), 1, },
#endif
#if CELL
	{ "numcells", arg_num_cells, sizeof(arg_num_cells), 1, },
#endif
#if defined (CELL_IRIX) && defined (LOGICAL_CELLS)
	{ "subcells", arg_subcells, sizeof(arg_subcells), 1, },
	{ "max_subcells", arg_max_subcells, sizeof(arg_subcells), 1, },
#endif /*  (CELL_IRIX) && (LOGICAL_CELLS) */

#ifdef IP22
	{ "prompoweroff", arg_prompoweroff, sizeof(arg_prompoweroff), 1, },
#ifdef DEBUG
	{ "ic_debugflags", arg_ic_debugflags, sizeof(arg_ic_debugflags), 1, },
#endif
#endif	
#ifdef _SYSTEM_SIMULATION
	{ "sablediskpart", arg_sablediskpart, sizeof(arg_sablediskpart), 0, },
	{ "sableexectrace", arg_sableexectrace, sizeof(arg_sableexectrace), 0, },
	{ "sableio", arg_sableio, sizeof(arg_sableio), 0, },
#endif 
#ifdef TRITON
	{ "triton_invall", arg_triton_invall, sizeof(arg_triton_invall), 0, },
#endif /* TRITON */
#if IP26 || IP28
	{ "boottune", arg_boottune, sizeof(arg_boottune), 1, },
#endif
#ifdef IP30
	{ "bootmaster", arg_bootmaster, sizeof(arg_bootmaster), 1, },
	{ "ef0mode", arg_ef0mode, sizeof(arg_ef0mode), 1, },
#endif
	{ "pci_no_auto", pci_no_auto, 1, 1, },
	{ "mrmode", arg_mrmode, sizeof(arg_mrmode), 1, },
	{ "mrconfig", arg_mrconfig, sizeof(arg_mrconfig), 1, },
	{ "netmask", arg_netmask, sizeof(arg_netmask), 1, },
	{ "kernname", arg_kernname, sizeof(arg_kernname), 1, },
#ifdef IP32
	{ "ec0mode",	 arg_ec0mode,	  sizeof(arg_ec0mode),	   1, },
	{ "bump_leds", arg_bump_leds, sizeof(arg_bump_leds), 0, },
	{ "reset_dump", arg_reset_dump, sizeof(arg_reset_dump), 0, },
	{ "ecc_enabled", arg_ecc_enabled, sizeof(arg_ecc_enabled), 0, },
	{ "videotiming", arg_videotiming, sizeof(arg_videotiming), 0, },
	{ "videooutput", arg_videooutput, sizeof(arg_videooutput), 0, },
	{ "videoinput",  arg_videoinput,  sizeof(arg_videoinput),  0, },
	{ "videostatus", arg_videostatus, sizeof(arg_videostatus), 0, },
	{ "videogenlock", arg_videogenlock, sizeof(arg_videogenlock), 0, },
	{ "audiopass", arg_audiopass, sizeof(arg_audiopass), 0, },
	{ "ip32nokp",	arg_ip32nokp,	sizeof(arg_ip32nokp),	1, },
	{ "crt_option", arg_crt_option, sizeof(arg_crt_option), 0, },
	{ "mfgsite", arg_mfgsite, sizeof(arg_mfgsite), 0, },
	{ "proctype", arg_proctype, sizeof(arg_proctype), 0, },
	{ "testmod", arg_testmod, sizeof(arg_testmod), 0, },
#endif
#ifdef _R5000_CVT_WAR
	{ "_R5000_CVT_WAR", arg_r5000_cvt_war, sizeof(arg_r5000_cvt_war), 0, },
#endif /* _R5000_CVT_WAR */
#ifdef _R5000_BADVADDR_WAR
	{ "_RM5271_BADVADDR_WAR", arg_rm5271_badvaddr_war, sizeof(arg_rm5271_badvaddr_war), 0, },
#endif /* _R5000_BADVADDR_WAR */

#if defined(SN0) && defined(SN0_HWDEBUG)

	/*
	 * SN0 debugging variables :
	 */

	/*
	 *
 	 * disable_bte 		    : cpu count, disables BTE SW usage.
	 *
	 * disable_bte_pbcopy 	    : cpu count, don't use BTE for bcopy
	 *
	 * disable_bte_pbzero	    : cpu count, don't use BTE for bzero
	 *
	 * disable_bte_poison_range : cpu count, use backdoor poisoning.
	 *			      Attention, there are known problems
 	 *			      with this since backdoor poisoning is
 	 *			      not an atomic operation
	 */
        { "disable_bte"       ,arg_disable_bte                     ,
                        sizeof (arg_disable_bte)              , 0 },
        { "disable_bte_pbcopy",arg_disable_bte_pbcopy              ,
                        sizeof (arg_disable_bte_pbcopy)       , 0 },
        { "disable_bte_pbzero",arg_disable_bte_pbzero         ,
                        sizeof (arg_disable_bte_pbzero)       , 0 },
        { "disable_bte_poison_range", arg_disable_bte_poison_range ,
                        sizeof (arg_disable_bte_poison_range) , 0 },

	/*
	 * changing of hub configuration registers in order to
	 * debug outstanding issues as the IVACK problem or DMA timeouts.
	 * These values must be set at boot time, attempts to set those
	 * on the fly will result in hangs etc, so these are marked
	 * read-only
	 */
        { "iio_bte_crb_count" ,arg_iio_bte_crb_count               ,
                        sizeof (arg_iio_bte_crb_count)        , 1 },
        { "iio_icmr_c_cnt"    ,arg_iio_icmr_c_cnt                  ,
                        sizeof (arg_iio_icmr_c_cnt)           , 1 },
        { "iio_ictp"          ,arg_iio_ictp                        ,
                        sizeof (arg_iio_ictp)                 , 1 },
        { "iio_icto"          ,arg_iio_icto                        ,
                        sizeof (arg_iio_icto)                 , 1 },
        { "iio_icmr_precise"  ,arg_iio_icmr_precise                ,
                        sizeof (arg_iio_icmr_precise)         , 1 },

	/*
	 * the following values have been used in the PCI bridge
	 * error interrupt to trigger a logic anaylzer by writing
	 * a magic value (la_trigger_val) to a read-only register.
	 * Note that another magic values is added to avoid false
	 * triggers when the values showed up in a cache line as data
	 * A NASID of -1 means that the trigger is not active (see
	 * code in pcibr.c)
	 */
        { "la_trigger_nasid1"  ,arg_la_trigger_nasid1              ,
                        sizeof (arg_la_trigger_nasid1)        , 0 },
        { "la_trigger_nasid2"  ,arg_la_trigger_nasid2              ,
                        sizeof (arg_la_trigger_nasid2)        , 0 },
        { "la_trigger_val"     ,arg_la_trigger_val                 ,
                        sizeof (arg_la_trigger_val)           , 0 },
#endif
	{ 0 },
};

#if IP30
#define BACKDOOR_KERNARGS
static struct kernargs backdoor_kernargs[] = {
#ifdef HEART_COHERENCY_WAR
	{ "heart_read_war", arg_heart_read_war,
	  sizeof(arg_heart_read_war), 1},
	{ "heart_write_war", arg_heart_write_war,
	  sizeof(arg_heart_write_war), 1},
#endif
#ifdef HEART_INVALIDATE_WAR
	{ "heart_invalidate_war", arg_heart_invalidate_war,
	  sizeof(arg_heart_invalidate_war), 1},
#endif
	{ 0 },
};
#endif

/*
 * Return non-zero if the variable was defined (with no value)
 */
int
is_true(char *s)
{
	return (strcmp(s, TRUE_VALUE) == 0);
}

/*
 * Return non-zero if the given variable was specified
 */
int
is_specified(char *s)
{
	return (strlen(s) != 0);
}

/*
 * Return non-zero if variable is true or non-zero
 */
int
is_enabled(char *s)
{
	return(is_true(s) ? -1 : atoi(s));
}

static void
parseopts(register __promptr_t *argp)
{
	register struct kernargs *kp;
	register char *cp;
	register int i;
	
	for (; *argp; argp++) {
		for (kp = kernargs; kp->name; kp++) {
			i = strlen(kp->name);
			if (strncmp(kp->name, STR_ADDR(*argp), i) == 0) {
				cp = &((STR_ADDR(*argp))[i]);
				if (*cp == 0) {
					strncpy(kp->str_ptr, TRUE_VALUE,
						(kp->strlen - 1));
#if defined(EVEREST) && defined(MULTIKERNEL)
					/* replicate value into all cells */
					evmk_replicate(kp->str_ptr,
						kp->strlen-1);
#endif
				} else
				if (*cp == '=') {
					cp++;
					while ((*cp == ' ') || (*cp == '\t'))
						cp++;
					strncpy(kp->str_ptr, cp,
						(kp->strlen - 1));
#if defined(EVEREST) && defined(MULTIKERNEL)
					/* replicate value into all cells */
					evmk_replicate(kp->str_ptr,
						kp->strlen-1);
#endif
				} else
					continue;
				break;
			}
		}
#ifdef BACKDOOR_KERNARGS
		/* For variables which we do not want showing up in via
		 * the nvram command.  Basically for controlling new system
		 * wars under the covers.
		 */
		for (kp = backdoor_kernargs; kp->name; kp++) {
			i = strlen(kp->name);
			if (strncmp(kp->name, STR_ADDR(*argp), i) == 0) {
				cp = &((STR_ADDR(*argp))[i]);
				if (*cp == 0)
					strncpy(kp->str_ptr, TRUE_VALUE,
						(kp->strlen - 1));
				else
				if (*cp == '=') {
					cp++;
					while ((*cp == ' ') || (*cp == '\t'))
						cp++;
					strncpy(kp->str_ptr, cp,
						(kp->strlen - 1));
				} else
					continue;
				break;
			}
		}
#endif
	}
}

/*
 * Parse kernel arguments.  Kernel arguments have the following syntax:
 *	"arg=value", where value is a C style number (decimal, octal, hex)
 *	"arg=string", where string is a simple alpha-numeric sequence
 *	"arg", which just defines arg to have a value of "1"
 * For string arguments, the string cannot begin with a "-" or a
 * "0" throuh "9"
 */
/*ARGSUSED*/
void
getargs(
	int argc,
	__promptr_t *argv,
	__promptr_t *environ)
{

#if _MIPS_SIM == _ABI64
#define KXBASE		K1BASE		/* IP26/28 passes args in K1 */
#else
#define KXBASE		K0BASE
#endif

	if ((__psunsigned_t)environ >= KXBASE) {
		parseopts(environ);
	}

	if ((__psunsigned_t)argv >= KXBASE) {
		parseopts(argv);
	}

	showconfig = is_enabled(arg_showconfig);

}

char *
kopt_find(char *name)
{
	register struct kernargs *kp;

	for (kp = kernargs; kp->name; kp++) {
		if (!strcmp(name, kp->name))
			return(kp->str_ptr);
	}
	return((char *)0);
}

int
kopt_index(int idx, char **npp, char **vpp)
{
	register struct kernargs *kp;
	register int i;

	for (kp = kernargs, i = 0; kp->name; kp++, i++) {
		if (i == idx) {
			*npp = kp->name;
			*vpp = kp->str_ptr;
			return(0);
		}
	}
	return(-1);
}

int
kopt_set(char *name, char *value)
{
	register struct kernargs *kp;

	for (kp = kernargs; kp->name; kp++) {
		if (!strcmp(name, kp->name)) {
			strncpy (kp->str_ptr, value, kp->strlen - 1);
			return(0);
		}
	}
	return(-1);
}

