/* textport button utilities
 */

#ident "$Revision: 1.5 $"

#include <arcs/io.h>
#include <saioctl.h>
#include <setjmp.h>
#include <libsc.h>
#include <libsc_internal.h>

static jmp_buf tpbuf;
void (*tpfunc)(void);
static char *buttontext[] = {
	"Done",
	"Continue",
	"Return"
};

/* Set-up textport button:
 *	- func is the routine called when the button is pressed.
 *	- msgnum is the message to be on the button.
 *	- wantbutton is immediate, or delayed button.
 */
void
setTpButtonAction(void (*func)(void), int msgnum, int wantbutton)
{
	extern void _tpbutton(void);
	ULONG rc;

	if (!isgraphic(StandardOut))
		return;

	setjmp(tpbuf);
	tpbuf[JB_PC] = (__psunsigned_t)_tpbutton;
	tpfunc = func;

	/* set button string, set jump position, turn button on. */
	(void)ioctl(StandardOut,GIOCSETBUTSTR,(long)buttontext[msgnum]);
	(void)ioctl(StandardOut,GIOCSETJMPBUF,(long)tpbuf);
	Write(StandardOut, wantbutton ? "\033[40h" : "\033[39h",5,&rc);
}
