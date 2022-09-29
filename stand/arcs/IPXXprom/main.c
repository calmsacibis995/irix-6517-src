#ident	"IP12prom/main.c:  $Revision: 1.12 $"

/*
 * main.c -- main line prom code
 */

#include <arcs/restart.h>
#include <arcs/signal.h>
#include <sys/cpu.h>
#include <gfxgui.h>
#include <parser.h>
#include <setjmp.h>
#include <menu.h>
#include <libsk.h>
#include <libsc.h>

#include "ipxx.h"

extern menu_t prom_menu;

extern jmp_buf restart_buf;

int errno;

#define	PROMPT	">>> "

int _prom=1;		/* bake _prom flag with true. */

static int version(int, char **);
static void main_intr (void);

/*
 * commands
 */
extern int boot(), get(), printenv_cmd(), unsetenv_cmd();
extern int cat(), put(), setenv_cmd(), go_cmd(), resetenv();
extern int auto_cmd(), spin(), fill(), dump(), checksum();
int hinv(), ls();
#ifdef	DEBUG
extern int mcopy(), mcmp(), mfind(), memlist();
#endif	/* DEBUG */
extern int reboot_cmd();

extern int _init();
extern int exit_cmd();
extern int single();
extern int passwd_cmd(), resetpw_cmd();
extern int cat(), copy();

#ifdef INCLUDE_TESTS
extern int opendev(), exit_test(), allchars(), progress();
#endif
extern int kernd(int, char **);

int
quit()
{
	exit(0);
}

/*
 * cmd_table -- interface between parser and command execution routines
 * Add new commands by making entry here.
 */
static struct cmd_table cmd_table[] = {
#ifdef INCLUDE_TESTS
	{ "allchars",	allchars,	"allchars:\tprint all characters" },
#endif
	{ "auto",	auto_cmd,	"autoboot:\tauto" },
	{ "boot",	boot,		"boot:\t\tboot [-f FILE] [-n] [ARGS]" },
	{ "cat",	cat,		"cat:\t\tcat FILE_LIST"},
	{ "cp",		copy,		"copy:\t\tcp [-b BLOCKSIZE] [-c BCOUNT] SRC_FILE DST_FILE"},
	{ ".checksum",	checksum,	"checksum:\tchecksum RANGE" },
	{ ".dump",	dump,		"dump:\t\tdump [-(b|h|w)] [-(o|d|u|x|c|B)] RANGE" },
	{ "kernd",	kernd,		"kernd: kernd up|down|restart" },
	{ "exit",	exit_cmd,	"exit:\t\texit" },
#ifdef INCLUDE_TESTS
	{ "exit_test",	exit_test,	"exit_test:\ttest exit functions"},
#endif
	{ ".fill",	fill,		"fill:\t\tfill [-(b|h|w)] [-v VAL] RANGE" },
	{ ".g",		get,		"get:\t\tg [-(b|h|w)] ADDRESS" },
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
#ifdef INCLUDE_TESTS
	{ "open",	opendev,	"open test:\topen mode file"},
#endif
	{ "passwd",	passwd_cmd,	"passwd:\t\tpasswd" },
	{ "printenv",	printenv_cmd,	"printenv:\tprintenv [ENV_VAR_LIST]" },
#ifdef INCLUDE_TESTS
	{ "progress",	progress,	"test progress:\tprogress" },
#endif
	{ ".p",		put,		"put:\t\tp [-(b|h|w)] ADDRESS VALUE" },
	{ "quit",	quit,		"quit prom sim:\tquit"},
#ifdef ENETBOOT
	{ ".reboot",	reboot_cmd,	"reboot:\t\treboot" },
#endif /* ENETBOOT */
	{ "resetenv",	resetenv,	"resetenv:\tresetenv" },
	{ "resetpw",	resetpw_cmd,	"resetpw:\tresetpw" },
	{ "setenv",	setenv_cmd,	"setenv:\t\tsetenv ENV_VAR STRING" },
	{ "single",	single,		"single user:\tsingle" },
#ifdef DEBUG
	{ "spin",	spin,		"spin:\t\tspin [[-c COUNT] [-v VAL] [-(r|w)[+](b|h|w) ADDR]]*" },
#endif /* DEBUG */
	{ "unsetenv",	unsetenv_cmd,	"unsetenv:\tunsetenv ENV_VAR" },
	{ "version",	version,	"version:\tversion" },
	{ 0,		0,		"" }
};

/*
 * main -- called from csu after prom data area has been cleared
 */
real_main()
{
	char *diskless;
	extern int Verbose;

	/* PON ok, and (almost) ready to run system software */
	(void)rbsetbs(BS_PREADY);

	(void)init_prom_menu();
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

static int
version(int argc, char **argv)
{
	printf("PROM Simulator Monitor %s\n", getversion());
	return 0;
}

int
kernd(int argc, char **argv)
{
	char *p;

	setGuiMode(1,GUI_NOLOGO);

	if (!argv[1]) return 1;

	if (!strcmp(argv[1],"down"))
		p = "The system is shutting down.\nPlease wait.\n";
	else if (!strcmp(argv[1],"up"))
		p = "The system is coming up.";
	else if (!strcmp(argv[1],"restart"))
		p = "The system is being restarted.";
	else
		p = argv[1];

	popupDialog(p, 0, DIALOGPROGRESS, DIALOGBIGFONT);

	return 0;
}
