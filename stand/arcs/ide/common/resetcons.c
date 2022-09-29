/*
 *  reset console
 *
 *  If the graphics console is being used, reinit the graphics.
 *  If any argument is passed, then just reopen the keyboard.
 */

#ident "$Revision: 1.7 $"

#include <saioctl.h>
#include <libsc.h>
#include <libsk.h>

/*ARGSUSED*/
void
resetcons(int argc, char **argv)
{
	extern int gfx_testing;

	if ((argc <= 1) && console_is_gfx()) {
#if IP20 || IP22 || IP26 || IP28
		extern void gfxreset(void);

		gfxreset();
#endif
		ioctl(1, TIOCREOPEN, 0);
		htp_init();
	}
	gfx_testing = 0;
	ioctl(0, TIOCREOPEN, 0);		/* reopen keyboard */
}
