#ident	"IP22prom/main.c:  $Revision: 1.28 $"

/*
 * main.c -- main line prom code
 */

#include <arcs/restart.h>
#include <arcs/signal.h>
#include <sys/cpu.h>
#include <parser.h>
#include <setjmp.h>
#include <gfxgui.h>
#include <menu.h>
#include <libsc.h>
#include <libsk.h>

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
int _prom=2;            /* bake _prom flag with true. */
#else
#define	PROMPT	">> "
int _prom=1;            /* bake _prom flag with true. */
#endif

static int version(int, char **, char **, struct cmd_table *);
static void main_intr(void);

/*
 * commands
 */
extern int reboot_cmd(), single(), put(), get(), auto_cmd(), checksum();
extern int date_cmd(), dump(), exit_cmd(), _init(), gioinfo();

#if SFLASH
extern int flash_cmd();
#endif

#if	defined(DEBUG) || defined(IP24)
extern int play_cmd(), poweroff_cmd();
#endif

#ifdef ENETBOOT
extern int showfont_cmd();
#endif
#ifdef DEBUG
extern int endian_cmd(), mcopy(), mcmp(), memlist(), spin();
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
#ifdef DEBUG
	{ ".endian",	endian_cmd,	"endian:\t\tendian -(b|l)" },
#endif
	{ "exit",	exit_cmd,	"exit:\t\texit" },
	{ ".fill", CT_ROUTINE fill,	"fill:\t\tfill [-(b|h|w)] [-v VAL] RANGE" },
	{ ".g",		get,		"get:\t\tg [-(b|h|w|d)] ADDRESS" },
	{ ".go",	go_cmd,		"go:\t\tgo [INITIAL_PC]" },
	{ "help",	help,		"help:\t\thelp or ? [COMMAND]" },
	{ ".?",		help,		"help:\t\t? [COMMAND]" },
	{ "init",	_init,		"initialize:\tinit" },
	{ "hinv",	hinv,		"inventory:\thinv [-v] [-t [-p]]" },
	{ "ls",		ls,		"list files:\tls DEVICE" },
#ifdef DEBUG
	{ "mcopy",	mcopy,		"mcopy:\t\tmcopy [-(b|h|w)] FROMRANGE TO" },
	{ "mcmp",	mcmp,		"mcmp:\t\tmcmp [-(b|h|w)] FROMRANGE TO" },
	{ "mfind",	mfind,		"mfind:\t\tmfind [-(b|h|w)] [-n] RANGE VALUE" },
	{ "mlist",	memlist,	"mlist:\t\tmlist" },
#endif /* DEBUG */
#ifdef ENETBOOT
	{ "cat",	cat,		"cat:\t\tcat FILE_LIST" },
	{ "cp",		copy,		"copy:\t\tcp [-b BLOCKSIZE] [-c BCOUNT] SRC_FILE DST_FILE" },
#endif
	{ ".gio",	gioinfo,	"gio info:\tgio\n"},
	{ "passwd",	passwd_cmd,	"passwd:\t\tpasswd" },
#if	defined(DEBUG) || defined(IP24)
	{ ".play",	play_cmd,	"play <tune #>" },
#endif
#ifdef	NOTYET
 	{ "ping",	ping,		"ping [-r | TARGET COUNT]" },
#endif
#if IP24 || defined(DEBUG)
	{ "off",	poweroff_cmd,	"power off machine:\toff" },
#endif
	{ "printenv",	printenv_cmd,	"printenv:\tprintenv [ENV_VAR_LIST]" },
#if _MIPS_SIM == _ABI64
	{ ".p",		put,		"put:\t\tp [-(b|h|w|d)] ADDRESS VALUE" },
#else
	{ ".p",		put,		"put:\t\tp [-(b|h|w)] ADDRESS VALUE" },
#endif
#if	defined(ENETBOOT) || defined(IP24)
	{ ".reboot",	reboot_cmd,	"reboot:\t\treboot" },
#endif /* ENETBOOT */
	{ "resetenv", CT_ROUTINE resetenv,	"resetenv:\tresetenv" },
	{ "resetpw",	resetpw_cmd,	"resetpw:\tresetpw" },
	{ "setenv",	setenv_cmd,	"setenv:\t\tsetenv ENV_VAR STRING" },
#ifdef ENETBOOT
	{ ".showfont",	showfont_cmd,	"fonts:\t\tshowfont index" },
#endif
	{ "single",	single,		"single user:\tsingle" },
#ifdef DEBUG
	{ "spin",	spin,		"spin:\t\tspin [[-c COUNT] [-v VAL] [-(r|w)[+](b|h|w) ADDR]]*" },
#endif /* DEBUG */
	{ "unsetenv",	unsetenv_cmd,	"unsetenv:\tunsetenv ENV_VAR" },
	{ "version",	version,	"version:\tversion" },
#ifdef SFLASH
	{ ".flash",	flash_cmd, "flash:\t\tflash [...]" },
	{ ".fl",	flash_cmd, "fl:\t\tflash [...]" },
#endif
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

#ifdef ENETBOOT
int
showfont_cmd(int ac, char **av)
{
	int i,c;

	for (i=1; i < ac; i++) {
		c = atoi(av[i]);
		printf("0x%x 0%o -> '%c'\n",c,c,c);
	}
	return(0);
}
#endif

/*ARGSUSED*/
static int
version(int argc, char **argv, char **argp, struct cmd_table *xxx)
{
#ifdef	_MIPSEB
	printf("PROM Monitor %s (BE)\n", getversion());
#else	/* _MIPSEL */
	printf("PROM Monitor %s (LE)\n", getversion());
#endif	/* _MIPSEL */
	return 0;
}
