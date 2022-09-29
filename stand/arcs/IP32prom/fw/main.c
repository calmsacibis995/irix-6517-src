#ident	"IP32prom/main.c:  $Revision: 1.1 $"

/*
 * main.c -- main line prom code
 */

#include <arcs/restart.h>
#include <arcs/signal.h>
#include <sys/cpu.h>
#include <sys/mace.h>
#include <sys/IP32.h>
#include <parser.h>
#include <setjmp.h>
#include <menu.h>
#include <libsc.h>
#include <libsk.h>

#define PROMPT "> "

extern menu_t prom_menu;
extern jmp_buf restart_buf;

static void main_intr(void);
static int version(int, char **, char **, struct cmd_table *);

/*
 * commands
 */
extern int reboot_cmd(), single(), put(), get(), auto_cmd(), checksum();
extern int date_cmd(), dump(), _init(), gioinfo();
extern int play_cmd(), poweroff_cmd();
extern int readc0_cmd(), writec0_cmd();
extern void led_on(int), led_off(void);
extern char *getversion(void);
static int  IP32firmware_init();
#if defined(May28NinetySixSCSIFixed)
static int  IP32firmware_exit();
#else
extern int  exit_cmd();
#endif

/*
 * cmd_table -- interface between parser and command execution routines
 * Add new commands by making entry here.
 */
static struct cmd_table cmd_table[] = {
	{ "auto",	auto_cmd,	"autoboot:\tauto" },
	{ "boot",	boot,		"boot:\t\tboot [-f FILE] [-n] [ARGS]" },
	{ ".checksum",	checksum,	"checksum:\tchecksum RANGE" },
	{ "date",	date_cmd,	"date:\t\tdate [mmddhhmm[ccyy|yy][.ss]]"},
	{ ".dump",	dump,		"dump:\t\tdump [-(b|h|w)] [-(o|d|u|x|c|B)] RANGE" },
#if defined(May28NinetySixSCSIFixed)
	{ "exit",   IP32firmware_exit,	"exit:\t\texit" },
#else   
	{ "exit",	exit_cmd,	"exit:\t\texit" },
#endif        
	{ ".fill", CT_ROUTINE fill,	"fill:\t\tfill [-(b|h|w)] [-v VAL] RANGE" },
	{ "get",	get,		"get:\t\tg [-(b|h|w|d)] ADDRESS" },
	{ "put",	put,		"put:\t\tp [-(b|h|w|d)] ADDRESS VALUE" },
	{ ".g",		get,		"get:\t\tg [-(b|h|w|d)] ADDRESS" },
	{ ".p",		put,		"put:\t\tp [-(b|h|w|d)] ADDRESS VALUE" },
	{ ".go",	go_cmd,		"go:\t\tgo [INITIAL_PC]" },
	{ "help",	help,		"help:\t\thelp or ? [COMMAND]" },
	{ ".?",		help,		"help:\t\t? [COMMAND]" },
	{ "init",   IP32firmware_init,  "initialize:\tinit" },
	{ "hinv",	hinv,		"inventory:\thinv [-v] [-t [-p]]" },
	{ "ls",		ls,		"list files:\tls DEVICE" },
	{ "passwd",	passwd_cmd,	"passwd:\t\tpasswd" },
	{ ".play",	play_cmd,	"play <tune #>" },
	{ "off",	poweroff_cmd,	"power off machine:\toff" },
	{ "printenv",	printenv_cmd,	"printenv:\tprintenv [ENV_VAR_LIST]" },
	{ ".reboot",	reboot_cmd,	"reboot:\t\treboot" },
	{ "resetenv", CT_ROUTINE resetenv,	"resetenv:\tresetenv" },
	{ "resetpw",	resetpw_cmd,	"resetpw:\tresetpw" },
	{ "setenv",	setenv_cmd,	"setenv:\t\tsetenv ENV_VAR STRING" },
	{ "single",	single,		"single user:\tsingle" },
	{ "unsetenv",	unsetenv_cmd,	"unsetenv:\tunsetenv ENV_VAR" },
	{ "version",	version,	"version:\tversion" },
#ifdef DEBUG
	{ "readc0",	readc0_cmd,	"readc0:\t\tread_c0" },
	{ "writec0",	writec0_cmd,	"writec0:\t\twrite_c0" },
#endif
	{ 0,		0,		"" }
};

/*
 * main -- called from csu after prom data area has been cleared
 */
_main()
{
	char *diskless;
	extern int Verbose;

	/* PON ok, and (almost) ready to run system software */
	(void)rbsetbs(BS_PREADY);

	(void)init_prom_menu();
	Signal (SIGINT, main_intr);

        /* Turn on Green led */
        led_on(ISA_LED_GREEN);

	for(;;) {
		/* Mark point */
		setjmp(restart_buf);
		close_noncons();	/* close any left over fd's */

		if ( getenv("VERBOSE") == 0 )
			Verbose = 0;

		/* update menu */
		diskless = getenv("diskless");
		prom_menu.item[0].flags &= ~M_INVALID;
		prom_menu.item[1].flags &= ~M_INVALID;
		prom_menu.item[2].flags &= ~M_INVALID;
		prom_menu.item[3].flags &= ~M_INVALID;
		prom_menu.item[4].flags &= ~M_INVALID;
		prom_menu.item[5].flags &= ~M_INVALID;

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
    if (!isGuiMode()) printf ("\n");
    Signal (SIGALRM, SIGIgnore);
    longjmp (restart_buf, 1);
}

/*****************************************************************************
 *                       Find and get the version on flash                   *
 *****************************************************************************/

static int
version(int argc, char **argv, char **argp, struct cmd_table *xxx)
{
  printf("\n\nPROM Monitor (BE)\n%s\n", getversion());
  return 0;
}

/**************************************************************************
 *                 IP32 exit and init command
 **************************************************************************/

#if defined(May28NinetySixSCSIFixed)
/*
 * The SCSI driver will hang the system when re-enter the firmware
 * until it get fixed we'll have to take the long path.
 */
extern void fwStart(int,int);

/*******************
 * IP32firmware_init
 * -----------------
 * Implement the arcs init command.
 */
int
IP32firmware_init(void)
{
#if 1  
 fwStart(FW_INIT,0);
#else 
 fwStart(FW_EIM,0);
#endif
}

/*******************
 * IP32firmware_exit
 * -----------------
 * Implement the arcs prom exit command.
 */
int
IP32firmware_exit(void)
{
 fwStart(FW_EIM,0);
}

#else   /* defined(May28NinetySixSCSIFixed) */

/*******************
 * IP32firmware_init
 * -----------------
 * Implement the arcs init command.
 */
int
IP32firmware_init(void)
{
#if 1
  (*(void(*)(int,int))0xbfc00100)(FW_INIT,0);
#else 
  (*(void(*)(int,int))0xbfc00100)(FW_EIM,0);
#endif
}

#endif  /* defined(May28NinetySixSCSIFixed) */

/* END OF FILE */
