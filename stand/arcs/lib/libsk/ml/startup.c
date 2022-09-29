/* startup auto boot code.
 */

#ident "$Revision: 1.13 $"

#include <arcs/errno.h>
#include <arcs/io.h>
#include <libsc.h>
#include <gfxgui.h>
#include <style.h>

#define AUTO_DELAY 5		/* wait 5 sec before booting */
#define AUTO_MESSAGE1 0		/* default start message */
#define TTY_AUTO_MESSAGE2 \
	"To perform system maintenance instead, press <Esc>\n"

/*
 * startup - boot system, autobooting if AutoLoad is set to 'Y'
 */
void
startup(void)
{
	char *cp, bootchar = (cp = getenv ("AutoLoad")) ? *cp : 0;
	char *stopboot = getenv("NoAutoLoad");
	int ab;

	/*
	 * XXX here it should check to see if there is a valid
	 * restart block, and if so, reboot from it.
	 */
	if (!stopboot && (bootchar == 'y' || bootchar == 'Y')) {
		char *msg2;

		if (!doGui()) {
			msg2 = TTY_AUTO_MESSAGE2;
			p_panelmode();
			p_center();
			p_cursoff();
		}
		else {
			/* make sure screen is up (not done on Restart) */
			if (!isGuiMode())
				setGuiMode(1,0);
			msg2 = 0;
		}

		ab = autoboot(AUTO_DELAY, AUTO_MESSAGE1, msg2);

		if (ab != ESUCCESS) {
			extern void EnterInteractiveMode();

			putchar('\n');
			setTpButtonAction(EnterInteractiveMode,
					  TPBCONTINUE,WBNOW);
			p_curson();
			p_printf("Unable to boot; press any key to continue:");
			getchar();
			putchar('\n');
		}
	}
	/* goto main() via EnterInteractiveMode() which will re-init
	 * if needed.
	 */
#if !defined(IP32) /* In IP32 this will get handler by fw_dispatcher */
	EnterInteractiveMode();
#endif
}
