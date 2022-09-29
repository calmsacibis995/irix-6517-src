/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992-1995 Silicon Graphics, Inc.           *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/***********************************************************************\
*	File:		main.c						*
*									*
*	Contains the main-line system PROM monitor loop.		*
*									*
\***********************************************************************/

#ident	"I06prom/main.c:  $Revision: 1.61 $"

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/loaddrs.h>
#include <arcs/restart.h>
#include <arcs/signal.h>
#include <sys/cpu.h>
#include <parser.h>
#include <setjmp.h>
#include <menu.h>
#include <libsc.h>
#include <libsk.h>
#include <sys/SN/gda.h>
#include <sys/SN/launch.h>
#include "io6prom_private.h"
#include <sys/SN/klconfig.h>
#include <libkl.h>
#include <sys/SN/kldiag.h>

extern menu_t prom_menu;

extern jmp_buf restart_buf;

/*
 * PROMPT is different for debugging version so
 * it's easy to tell which monitor you're talking to
 * when debugging monitor with monitor
 */

#ifdef ENETBOOT
#define	PROMPT	">>> "
#else
#define	PROMPT	">> "
#endif

int _prom=1;            /* bake _prom flag with true. */
int _symmon=0;		/* Senator, I know symmon and you are no symmon. */

/*
 * Due to known hub II problems, we can take data bus errors when
 * reading/writing IOC3 registers. We don't want to go into an
 * infinite loop when trying to print stuff out. Catch the exception
 * so we can switch to alternate console.
 */
int cons_hw_faulted = 0;


int version(int, char **, char **, struct cmd_table *);

static void main_intr(void);
static int opts();

void init_prom_menu(void);
void menu_parser(menu_t *pmenu);
char *getversion(void);
int pod_mode();

/*
 * commands
 */
extern int auto_cmd(), checksum();
extern int disable_cmd(), dump();
extern int enable_cmd(), exit_cmd(), flash_cmd(),
				flashio_cmd(), flashcpu_cmd() ;
extern int ed_cmd() ;
extern int enableall_cmd() ;
extern int get(), goto_cmd();
extern int _init(), put();
extern int serial_cmd();
extern int single() ;
extern int update_cmd() ;
extern int dumpklcfg_cmd() ;
extern int checkclkmod_cmd() ;
extern int erasenvram_cmd() ;
extern int modnum_cmd() ;
extern int mcmp(), mcopy();
extern int slvnvram() ;
extern int diag_enet_cmd() ;
extern int diag_scsi_cmd() ;
extern int setpart_cmd() ;
extern int setdomain_cmd() ;
extern int setcluster_cmd() ;
extern int mvmodule_cmd() ;

/*
 * cmd_table -- interface between parser and command execution routines
 * Add new commands by making entry here.
 */
static struct cmd_table cmd_table[] = {
	{ "auto",	auto_cmd,	
	  "auto:\t\t\t\tAutoboot the machine" },
	{ "boot",	boot,		
	  "boot [-f FILE] [-n] [ARGS]:\tLoad (and execute) specified file" },
	{ ".checksum",	checksum,	
	  "checksum:\t\t\tchecksum RANGE" },
	{ ".diag_enet",	diag_enet_cmd,
          "diag_enet [-N NASID] [-m MODID] [-s SLOTID] [-p PCIDEV] [-h] [-e] "
	  "[-v] [-t TESTNAME] [-r COUNT]\n"
	  "\t\t\t\t\tRun enet diags on a single IOC3" },
	{ ".diag_scsi",	diag_scsi_cmd,
          "diag_scsi [-N NASID] [-m MODID] [-s SLOTID] [-p PCIDEV] [-h] [-e] "
	  "[-v] [-t TESTNAME] [-r COUNT]\n"
	  "\t\t\t\t\tRun SCSI diags on a single IOC3" },
	{ "disable", disable_cmd,
          "disable -m MODID -s SLOTID [-cpu a|b|ab] "
          "[-mem [1234567]] [-pci [01234567]]\n"
	  "\t\t\t\t\tDisable hardware" },
	{ ".dump",	dump,		
	  "dump:\t\t\t\tdump [-(b|h|w)] [-(o|d|u|x|c|B)] RANGE" },
	{ "enable",	enable_cmd,
          "enable -m MODID -s SLOTID [-cpu a|b|ab] "
          "[-mem [1234567]] [-pci [01234567]]\n"
	  "\t\t\t\t\tEnable hardware" },
	{ "exit",	exit_cmd, "exit:\t\t\t\tReturn to PROM menu" },
        { ".flashcpu", flashcpu_cmd,
	  "flashcpu [-N NASID] [-m MODID -s SLOTID] "
	  "[-f] [-F] [-v] [-e] [-l] [-C] FILE\t\n"
	  "\t\t\t\t\tWrite FILE to Flash PROMs on IP27\n"
	  "\t\t\t\t\tUse ip27prom.img file in build directory."},
        { ".flashio",  flashio_cmd,
          "flashio [-m MODID -s SLOTID] [-f] [-F] [-v] [-e] FILE\n"
	  "\t\t\t\t\tWrite FILE to Flash PROMs on IO6\n"
	  "\t\t\t\t\tUse io6prom.img file in build directory."},
	{ ".flash",	flash_cmd,	
	  "flash [-c|n] [-i] [-e] [-m MODID -s SLOTID] [-N NASID] "
	  "[-f] [-F] [-y] [-v] [-e] [-l] [-C] FILE\n"
	  "\t\t\t\t\tFlash all appropriate PROMs with FILE"},
	{ ".g",		get,		
	  "g [-(b|h|w|d)] ADDRESS\t\t\t"
	  "Get a byte, half, word or doubleword from memory" },
	{ ".go",	go_cmd,		
	  "go [INITIAL_PC]\t\t\t"
	  "Goto the given address in a loaded executable" },
	{ ".goto", 	goto_cmd,	
	  "goto INITIAL_PC [ARGS]\t\t\tCall subroutine at address with args" },
	{ "help",	help,		
	  "help [COMMAND]:\t\t\tPrint descriptions for one or all commands" },
	{ ".?",		help,
	  "help:\t\t\t\t? [COMMAND]" },
	{ "init",	_init,		
	  "init:\t\t\t\tReinitialize the PROM" },
	{ "hinv",	klhinv,		
	  "hinv [-v] [-m] [-mvvv] [-g PATH]:\n"
	  "\t\t\t\t\tDisplay hardware inventory" },
	{ "opts",	opts,		
	  "opts:\t\t\t\tWhich options were we built with?" },
	{ ".slvnvram",	slvnvram,		
	  "slvnvram:\t\t\tDump contents of slave nvrams." },
	{ "ls",		ls,		
	  "ls [DEVICE]:\t\t\tDisplay a directory listing for device" },
	{ ".mcmp",	mcmp,		
          "mcmp:\t\t\tmcmp [-b|-h|-w] RANGE1 RANGE2"},
	{ ".mcopy",	mcopy,		"" },
	{ ".mfind",	mfind,		"" },
	{ "passwd",	passwd_cmd,	
	  "passwd:\t\t\t\tSet the PROM's password" },
        { "ping",       ping,
	  "ping [-f] [-i sec] [-c #] IPADDR:\n"
	  "\t\t\t\t\tPing server at IPADDR" },
        { "enableall",	enableall_cmd,
	  "enableall [-y] [-list]:\tEnable all disabled components" },
	{ ".pod",	pod_mode,	
	  "pod\t\t\t\tSwitch back into POD mode"},
	{ "printenv",	printenv_cmd,	
	  "printenv [ENV_VAR_LIST]:\tDisplay selected environment variables" },
	{ ".p",		put,		
#if _MIPS_SIM == _ABI64
	  "p [-(b|h|w|d)] ADDRESS VALUE\t"
	  "Write a byte, half, word or doubleword to memory" },
#else
	  "p [-(b|h|w)] ADDRESS VALUE\t"
	  "Write a byte, half, word or doubleword to memory" },
#endif
	{ "reset",	(int (*)())cpu_hardreset,
	  "reset:\t\t\t\tReset the machine" },
	{ "resetenv",	CT_ROUTINE resetenv,
	  "resetenv:\t\t\tReset environment to standard values" },
	{ "resetpw",	resetpw_cmd,
	  "resetpw:\t\t\tReset the prom password" },
	{ ".rstat",	rstat_cmd,
	  "rstat vector:\t\t\tGet router status" },
	{ ".serial",	serial_cmd,
	  "serial:\t\t\tserial NEW_SERIAL_NUM" },
	{ "setenv",	setenv_cmd,	
	  "setenv ENV_VAR STRING:\t\tSet value of an environment variable" },
	{ "single",	single,		
          "single:\t\t\t\tBoot unix to single-user mode" },
	{ "unsetenv",	unsetenv_cmd,
	  "unsetenv ENV_VAR:\t\tRemoves a variable from the environment" },
        { "update",     update_cmd,
          "update:\t\t\t\tUpdate the stored hardware inventory info." },
        { ".erasenvram",	erasenvram_cmd,
          "erasenvram [-check] [-help]:\t\tCompletely erase all NVRAMs." },
        { ".dumpklcfg",		dumpklcfg_cmd,
          "dumpklcfg:\t\t\tDump klconfig on a node to node basis." },
        { ".checkclk",		checkclkmod_cmd,
          "checkclk <modid>:\t\tCheck the Clock on Module <modid>." },
        { "modnum",     modnum_cmd,
          "modnum:\t\t\t\tList all current modules in the system." },
        { ".erasenvram",	erasenvram_cmd,
          "erasenvram [-check] [-help]:\tCompletely erase all NVRAMs." },
	{ "version",	version,
	  "version:\t\t\tDisplay prom version" },
	{ "setpart",	setpart_cmd,
	  "setpart:\t\t\tMake partitions" },
	{ "setdomain",	setdomain_cmd,
	  "setdomain [-h] [-l] [-s]:\t\t\tGet or Set Domain ID" },
	{ "setcluster",	setcluster_cmd,
	  "setcluster [-h] [-l] [-s]:\t\t\tGet or Set Cluster ID" },
	{ "mvmodule",	mvmodule_cmd,
	  "mvmodule OLD_MODID NEW_MODID:\tChange module ID" },
	{ 0,		0,		"" }
};

/*
 * main -- called from csu after prom data area has been cleared
 */
main()
{
	char *diskless;
	extern int Verbose;

	/* PON ok, and (almost) ready to run system software */
	(void)rbsetbs(BS_PREADY);

	init_prom_menu ();
	Signal (SIGINT, main_intr);

	/* Turn off diags when we run it once. We do
	   not want to run it for every io6prom re-entry. */

	diag_set_mode(DIAG_MODE_NONE) ;

	for(;;) {
		/* Mark point */
		setjmp(restart_buf);

		if ( getenv("VERBOSE") == 0 )
			Verbose = 0;

		/* update menu */
		diskless = getenv("diskless");

		prom_menu.item[0].flags &= ~M_INVALID;
		prom_menu.item[1].flags &= ~M_INVALID;
		prom_menu.item[2].flags &= ~M_INVALID;
		prom_menu.item[3].flags &= ~M_INVALID;
		prom_menu.item[4].flags &= ~M_INVALID;

		if (diskless && (*diskless == '1')) {
			prom_menu.item[1].flags |= M_INVALID;
			prom_menu.item[3].flags |= M_INVALID;
		}

		/* Menu parser returns for manual mode */
		menu_parser(&prom_menu);

		/* re-Mark point */
		setjmp(restart_buf);
		close_noncons();	/* close any left over fd's */
		_scandevs();

		/* Verbose defaults to 1 in manual mode if not in environ. */
		if ( getenv("VERBOSE") == 0 )
			Verbose = 1;

		/* Manual mode */
		command_parser(cmd_table, PROMPT, 1, 0);

		/* on leaving manual mode, return to menu mode */
	}
}

static void
main_intr (void)
{
    printf ("\n");
    Signal (SIGALRM, SIGIgnore);
    longjmp (restart_buf, 1);
}

/*ARGSUSED*/
int
version(int argc, char **argv, char **argp, struct cmd_table *xxx)
{
#ifdef	_MIPSEB
	char *endian = "BE";
#else
	char *endian = "LE";
#endif
#if _MIPS_SIM == _ABI64
	char *size = "64";
#else
	char *size = "";
#endif
	extern _ftext();

	printf("BASEIO PROM Monitor %s (%s%s)\n", getversion(), endian, size);
	return 0;
}


/*
 * pod_mode -- Jump back into POD mode.
 */

int
pod_mode()
{
    cpuid_t 	cpu;

    /* Kick all the slave processors back into the slave loop */
    for (cpu = 0; cpu < MAXCPUS; cpu++) {
        cnodeid_t 	cnode;
        nasid_t	nasid;
	int slice;

	get_cpu_cnode_slice(cpu, &cnode, &slice);

        if ((nasid = GDA->g_nasidtable[cnode]) == INVALID_NASID)
           break;

	if (slice == -1)
	    continue;

        /* Skip all CPUs which are current cpu or aren't available */
        if ((cpu == cpuid()) || !get_cpuinfo(cpu))
           continue;

	/* Pushing all processors back into slave loop */
        LAUNCH_SLAVE(nasid, slice, (void (*)(__int64_t)) IP27PROM_SLAVELOOP, 0, 0, 0);
    }

    /* Now handle the master processor */
    printf("\nSwitching into Power-On Diagnostics mode...\n\n");
    ((void (*)()) IP27PROM_IOC3UARTPOD)();

    /* Never reached */
    return 0;
}

extern char *getversion(void);
extern char *libkl_opts, *libsk_opts, *prom_opts;

int
opts()
{
    puts(getversion());
    printf("\nLibkl was built as follows: ");
    puts(libkl_opts);
    printf("\n\nLibsk was built as follows: ");
    puts(libsk_opts);
    printf("\n\nThe IO6prom was built as follows: ");
    puts(prom_opts);
    printf("\n");
    return 0;
}


