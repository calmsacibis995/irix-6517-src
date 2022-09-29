/***********************************************************************\
*	File:		main.c						*
*									*
*	Contains the main-line system PROM monitor loop.		*
*									*
\***********************************************************************/

#ident	"I04prom/main.c:  $Revision: 1.32 $"

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/loaddrs.h>
#include <arcs/restart.h>
#include <arcs/signal.h>
#include <sys/cpu.h>
#include <sys/EVEREST/evmp.h>
#include <parser.h>
#include <setjmp.h>
#include <menu.h>
#include <libsc.h>
#include <libsk.h>

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

int version(int, char **, char **, struct cmd_table *);

static void main_intr (void);
static int pod_mode(void);

void init_prom_menu(void);
void menu_parser(menu_t *pmenu);
char *getversion(void);

/*
 * commands
 */
extern int auto_cmd(), checksum();
extern int disable_cmd(), do_cmd(), dump();
extern int enable_cmd(), exit_cmd(), flash_cmd();
extern int get(), goto_cmd();
extern int _init(), put();
extern int serial_cmd();
extern int single(), update_cmd(), dump_earoms();
extern int mcmp(), mcopy();
extern int board_revision_cmd(),margin_voltage_cmd(),pod_mode();
#if IP25
extern int ip25mon_cmd();
#endif


/*
 * cmd_table -- interface between parser and command execution routines
 * Add new commands by making entry here.
 */
static struct cmd_table cmd_table[] = {
	{ "auto",	auto_cmd,	
	  "auto:\t\t\t\tAutoboot the machine" },
#ifdef IP21
	{ "board_revision", board_revision_cmd,
	  "board_revision:\t\tDisplay Revision of each Cpu Board" },
#endif
	{ "boot",	boot,		
	  "boot [-f FILE] [-n] [ARGS]:\tLoad (and execute) the specified file" },
	{ ".checksum",	checksum,	
	  "checksum:\tchecksum RANGE" },
	{ "disable", disable_cmd,
	  "disable SLOT [UNIT]:\t\tDisable a particular board or component" }, 
	{ ".docmd",     do_cmd,         
	  "docmd:\t docmd cmdname parameters"},
	{ ".dump",	dump,		
	  "dump:\t\tdump [-(b|h|w)] [-(o|d|u|x|c|B)] RANGE" },
	{ ".earom",	dump_earoms,	""},
	{ "enable",	enable_cmd,
	  "enable SLOT [UNIT]:\t\tRe-enable a previously disabled board or unit"},
	{ "exit",	exit_cmd, "exit:\t\t\t\tReturn to PROM menu" },
	{ ".flash",	flash_cmd,	
	  "flash [-s SLOT] [-e] FILE\tWrite the given file into the flash PROM"},
	{ ".g",		get,		
	  "g [-(b|h|w|d)] ADDRESS\tGet a byte, half, word or doubleword from memory" },
	{ ".go",	go_cmd,		
	  "go [INITIAL_PC]\t\tGoto the given address in a loaded executable" },
	{ ".goto", 	goto_cmd,	
	  "goto INITIAL_PC [ARGS]\tCall subroutine at address with given args"},
	{ "help",	help,		
	  "help [COMMAND]:\t\t\tPrint descriptions for one or all commands" },
	{ ".?",		help, "help:\t\t\t? [COMMAND]" },
	{ "init",	_init,		
	  "init:\t\t\t\tReinitialize the PROM" },
	{ "hinv",	hinv,		
	  "hinv [-v] [-t] [-b]:\t\tDisplay hardware inventory" },
#if IP25
	{ "ip25mon", 	ip25mon_cmd, "Testing"},
#endif
	    
	{ "ls",		ls,		
	  "ls [DEVICE]:\t\t\tDisplay a directory listing for device" },
#ifdef IP21
	{ ".margin",	margin_voltage_cmd,
	  "margin:\t\t\tVoltage Margining Utility" },
#endif
	{ ".mcmp",	mcmp,		
          "mcmp:\tmcmp [-b|-h|-w] RANGE1 RANGE2"},
	{ ".mcopy",	mcopy,		"" },
	{ ".mfind",	mfind,		"" },
	{ "passwd",	passwd_cmd,	
	   "passwd:\t\t\t\tSet the prom's password" },
	{ "ping",	ping,	
	   "ping [-f] [-i sec] [-c #] target:\tPing server at target" },
	{ ".pod",	pod_mode,	
	  "pod\t\t\tSwitch back into POD mode"},
	{ "printenv",	printenv_cmd,	
	  "printenv [ENV_VAR_LIST]:\tDisplay selected environment variables" },
	{ ".p",		put,		
#if _MIPS_SIM == _ABI64
	  "p [-(b|h|w|d)] ADDRESS VALUE\tWrite a " },
#else
	  "p [-(b|h|w)] ADDRESS VALUE\tWrite a " },
#endif
	{ "reset",	(int (*)())cpu_hardreset,	"reset:\t\t\t\tReset the machine" },
	{ "resetenv",	CT_ROUTINE resetenv,	"resetenv:\t\t\tReset environment to standard values" },
	{ "resetpw",	resetpw_cmd,	"resetpw:\t\t\tReset the prom password" },
	{ ".serial",	serial_cmd,	"serial: \tserial NEW_SERIAL_NUM" },
	{ "setenv",	setenv_cmd,	
	  "setenv ENV_VAR STRING:\t\tSet the value of an environment variable" },
	{ "single",	single,		
          "single:\t\t\t\tBoot unix to single-user mode" },
	{ "unsetenv",	unsetenv_cmd,
	  "unsetenv ENV_VAR:\t\tRemoves a variable from the environment" },
	{ "update",	update_cmd,
	  "update:\t\t\t\tUpdate the stored hardware inventory info." },
	{ "version",	version,	"version:\t\t\tDisplay prom version" },
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

	printf("IO4 PROM Monitor %s (%s%s)\n", getversion(), endian, size);
	return 0;
}


/*
 * pod_mode -- Jump back into POD mode.
 */

static int
pod_mode(void)
{
    unsigned cpu;

    /* Kick all of the slave processors back into the slave loop */
    for (cpu = 0; cpu < EV_MAX_CPUS; cpu++) {

	/* Deal with the master CPU last */
	if (cpu == cpuid())
	    continue;

	/* Skip MPCONF fields which are obviously bogus */
	if (MPCONF[cpu].mpconf_magic != MPCONF_MAGIC ||
	    MPCONF[cpu].virt_id != cpu)
	    continue;

	/* Try pushing all processors back into slave loop */
	launch_slave(cpu, (void (*)(int)) EV_PROM_RESLAVE, 0, 0, 0, 0);
    }

    /* Now handle the master processor */
    printf("\nSwitching into Power-On Diagnostics mode...\n\n");
    ((void (*)()) EV_PROM_EPCUARTPOD)();

    return 0;
}
