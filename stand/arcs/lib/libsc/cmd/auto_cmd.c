/*
 * auto_cmd.c - Auto-boot command that just calls autoboot in libsc/lib
 */

#ident "$Revision: 1.31 $"

#include <libsc.h>
#include <gfxgui.h>

/*
 * auto_cmd - command wrapper for autobooting
 */
/*ARGSUSED*/
int
auto_cmd(int argc, char **argv, char **envp)
{
    if (!Verbose && doGui()) {
	    setGuiMode(1,0);
    }
    else {
	    p_panelmode();
	    p_cursoff();
    }

    autoboot (0, 0, 0);

    putchar('\n');
    setTpButtonAction(EnterInteractiveMode,TPBCONTINUE,WBNOW);
    p_curson();
    printf("Unable to boot; press any key to continue: ");
    getchar();
    putchar('\n');

    EnterInteractiveMode();
    /*NOTREACHED*/
}
