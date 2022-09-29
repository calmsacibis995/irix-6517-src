/*
 * single_cmd.c - Boot to single user mode
 */

#ident "$Revision: 1.15 $"

#include <libsc.h>
#include <gfxgui.h>

static char *automsg = "\n\nStarting up the system in single user mode...\n\n";

/*ARGSUSED*/
int
single(int argc, char **argv)
{
	setenv("initstate","s");

	if (!Verbose && doGui()) {
		setGuiMode(1,0);
	}
	else {
		p_panelmode();
		p_cursoff();
	}

	autoboot(0, automsg, 0);

	putchar('\n');
	setTpButtonAction(EnterInteractiveMode,TPBCONTINUE,WBNOW);
	p_curson();
	printf("Unable to boot; press any key to continue: ");
	getchar();
	putchar('\n');

	EnterInteractiveMode();
	/*NOTREACHED*/
}
