#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/pckm.h>

#include <uif.h>
#include <pon.h>

#ifdef PROM

#define KEYBOARD_PORT	0
#define MOUSE_PORT	1
#define CTRL_PORT	2

static char *ctrl_testname = "PC keyboard/mouse controller";
static char *kbd_testname = "PC keyboard";
static char *ms_testname = "PC mouse";

static int kbdbatrc;
static int kbdstat;

extern int _prom;

int
pon_pcctrl(void)
{
	int data, errcnt = 0;

	msg_printf(VRB,"%s test.\n",ctrl_testname);

	/*  Status of mouse or keyboard BAT test will be left in
	 * register 60.
	 */
	kbdstat = inp(KB_REG_64);
	kbdbatrc = inp(KB_REG_60);

	data = pckm_pollcmd(CTRL_PORT,CMD_RSTCTL);
	errcnt = (data != KBD_NOERR);

	if (errcnt)
		sum_error(ctrl_testname);
	else
		okydoky();

	return(errcnt);
}

int
pon_pckbd(void)
{
	int errcnt = 0;

	msg_printf(VRB,"%s test.\n",kbd_testname);

	/*  If in prom (not dprom), check if saved BAT return code is
	 * for the keyboard, and if it matches the failed code.
	 */
	if ((_prom == 1) && ((kbdstat & (SR_OBF|SR_MSFULL)) == SR_OBF) &&
	    (kbdbatrc == KBD_BATFAIL)) {
		sum_error(kbd_testname);
		return(1);
	}

	okydoky();
	return(errcnt);
}

int
pon_pcms(void)
{
	msg_printf(VRB,"%s test.\n",ms_testname);

	/*  If in prom (not dprom), check if saved BAT return code is
	 * for the mouse, and if it matches the failed code.
	 */
	if ((_prom == 1) && (kbdstat & SR_MSFULL) &&
	    (kbdbatrc == KBD_BATFAIL)) {
		sum_error(ms_testname);
		return(1);
	}

	okydoky();
	return(0);
}
#endif
