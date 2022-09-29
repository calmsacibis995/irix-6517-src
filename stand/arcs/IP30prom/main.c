#ident	"IP30prom/main.c:  $Revision: 1.27 $"

/*
 * main.c -- main line prom code
 */

#include <arcs/restart.h>
#include <arcs/signal.h>
#include <sys/cpu.h>
#include <sys/immu.h>
#include <parser.h>
#include <setjmp.h>
#include <gfxgui.h>
#include <menu.h>
#include <libsc.h>
#include <libsk.h>

#include <sys/RACER/IP30nvram.h>
#include <sys/RACER/sflash.h>

extern menu_t prom_menu;

int init_prom_menu(void);
int getversion(void);

extern jmp_buf restart_buf;

/*
 * PROMPT is different for debugging version so
 * it's easy to tell which monitor you're talking to
 * when debugging monitor with monitor
 */

#ifdef ENETBOOT
#define	PROMPT	">>> "
#ifdef SYMMON
int _prom=3;            /* bake _prom flag with true, 3 == dprom.DBG */
#else
int _prom=2;            /* bake _prom flag with true, 2 == dprom */
#endif /* SYMMON */
#else
#define	PROMPT	">> "
int _prom=1;            /* bake _prom flag with true. 1 == prom */
#endif /* ENETBOOT */

static int version(int, char **, char **, struct cmd_table *);
static void main_intr(void);

/*
 * commands
 */
extern int reboot_cmd(), single(), put(), get(), auto_cmd(), checksum();
extern int dump(), exit_cmd(), _init(), system_cmd(), memlist();
extern int play_cmd();

static int who_cmd(int, char **, char **, struct cmd_table *);

extern int disable_cmd(int, char **, char **, struct cmd_table *);
extern int enable_cmd(int, char **, char **, struct cmd_table *);

#if defined(MFG_DBGPROM) && defined(MFG_MAKE)
extern int poncmd(int, char **, char **, struct cmd_table *);
#endif

extern int flash_cmd();
#ifdef SA_DIAG
extern int xb_diag();
#endif /* SA_DIAG */

#ifdef DEBUG
extern int qldebug_cmd();
#endif

#if 0
extern int mcopy(), mcmp(), mfind(), spin();
#endif

/*
 * cmd_table -- interface between parser and command execution routines
 * Add new commands by making entry here.
 */
static struct cmd_table cmd_table[] = {
	{ "auto",	auto_cmd,	"autoboot:\tauto" },
	{ "boot",	boot,		"boot:\t\tboot [-f FILE] [-n] [ARGS]" },
	{ ".checksum",	checksum,	"checksum:\tchecksum (RANGE | -f FILE)" },
	{ ".dump",	dump,		"dump:\t\tdump [-(b|h|w|d)] [-(o|d|u|x|c|B)] RANGE" },
	{ "disable",	disable_cmd,	"disable processor: disable [cpu num]"},
	{ "enable",	enable_cmd,	"enable processor: enable [cpu num]" },
	{ "exit",	exit_cmd,	"exit:\t\texit" },
	{ ".fill", CT_ROUTINE fill,	"fill:\t\tfill [-(b|h|w|d)] [-v VAL] RANGE" },
        { "flash",	flash_cmd,	"flash:\t\tflash (?|...)" },
	{ ".fl",        flash_cmd,	"fl:\t\tflash (?|...)" },
	{ ".g",		get,		"get:\t\tg [-(b|h|w|d)] ADDRESS" },
	{ ".go",	go_cmd,		"go:\t\tgo [INITIAL_PC]" },
	{ ".?",		help,		"help:\t\t? [COMMAND]" },
	{ "help",	help,		"help:\t\thelp or ? [COMMAND]" },
	{ "hinv",	hinv,		"inventory:\thinv [-v] [-t [-p]]" },
	{ "init",	_init,		"initialize:\tinit" },
	{ "ls",		ls,		"list files:\tls DEVICE" },
	{ ".mlist",	memlist,	"mlist:\t\tmlist" },
	{ "ping",	ping,
		"ping [-f] [-i sec] [-c #] target:\tPing server at target" },
#ifdef ENETBOOT
	{ "cat",	cat,		"cat:\t\tcat FILE_LIST" },
	{ "cp",		copy,
		"copy:\t\tcp [-b BLOCKSIZE] [-c BCOUNT] SRC_FILE DST_FILE" },
	{ ".play",	play_cmd,	"play [-f <tune file>] <tune #>"},
#endif
	{ "passwd",	passwd_cmd,	"passwd:\t\tpasswd" },
	{ "printenv",	printenv_cmd,	"printenv:\tprintenv [ENV_VAR_LIST]" },
	{ ".p",		put,		"put:\t\tp [-(b|h|w|d)] ADDRESS VALUE"},
#ifdef DEBUG
	{ ".qldebug",	qldebug_cmd,	"qldebug:\t\tqldebug [level]"},
#endif

	/* some peole are used to the reset name */
	{ ".reboot",	reboot_cmd,	"reboot:\t\treboot" },
	{ ".reset",	reboot_cmd,	"reset:\t\treset" },

	{ "resetenv", CT_ROUTINE resetenv,	"resetenv:\tresetenv" },
	{ "resetpw",	resetpw_cmd,	"resetpw:\tresetpw" },
	{ "setenv",	setenv_cmd,	"setenv:\t\tsetenv ENV_VAR STRING" },
	{ "single",	single,		"single user:\tsingle" },
	{ ".system",	system_cmd,	"System info:\tsystem [-d]\n"},
	{ "unsetenv",	unsetenv_cmd,	"unsetenv:\tunsetenv ENV_VAR" },
	{ "version",	version,	"version:\tversion" },
	{ ".who",	who_cmd,	"who:\twho" },
#ifdef SA_DIAG
	{ ".xb",        xb_diag, 	   "xb:\t\txb [...]" },
#endif /* SA_DIAG */
#ifdef DEBUG
	{ "mcopy",	mcopy,		"mcopy:\t\tmcopy [-(b|h|w)] FROMRANGE TO" },
	{ "mcmp",	mcmp,		"mcmp:\t\tmcmp [-(b|h|w)] FROMRANGE TO" },
	{ "mfind",	mfind,		"mfind:\t\tmfind [-(b|h|w)] [-n] RANGE VALUE" },
	/* XXX add support for double word XXX */
	{ "spin",	spin,
		"spin:\t\tspin [[-c COUNT] [-v VAL] [-(r|w)[+](b|h|w) ADDR]]*"},
#endif /* DEBUG */

#if defined(MFG_DBGPROM) && defined(MFG_MAKE)
	{ ".pon",	poncmd,		"pon:\t\tpon <test> [argument values]" },
#endif

	{ 0,		0,		"" }
};

/*
 * main -- called from csu after prom data area has been cleared
 */
void
main(void)
{
	char *diskless;
	extern int Verbose;

#if !ENETBOOT && !NOGFX
	/* if FPROM was just reprogrammed, clear the FVP
	 * (Fprom Validate Pending) bits
	 */
       flash_set_nv_rpromflg(flash_get_nv_rpromflg() & ~RPROM_FLG_FVP);

#endif /* !ENETBOOT && !NOGFX */

	/* PON ok, and (almost) ready to run system software */
	(void)rbsetbs(BS_PREADY);

	(void)init_prom_menu ();
	Signal (SIGINT, main_intr);

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
		prom_menu.item[6].flags &= ~M_INVALID;

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
main_intr(void)
{
    if (!isGuiMode()) printf ("\n");
    Signal (SIGALRM, SIGIgnore);
    longjmp (restart_buf, 1);
}

/*ARGSUSED*/
static int
version(int argc, char **argv, char **argp, struct cmd_table *xxx)
{
	printf("PROM Monitor %s - 64 Bit\n", getversion());
	return 0;
}

/*ARGSUSED*/
static int
who_cmd(int argc, char **argv, char **argp, struct cmd_table *xxx)
{
#ifdef BUILDER
	printf("PROM built by %s\n",BUILDER);
#endif
	return 0;
}
