#ifndef _STANDALONE	/* whole file */
#include "fx.h"
#include <signal.h>
#include <sys/termio.h>

/* Routines and variables used only in the version running under IRIX.
 * Gets rid of some ifdefs, and with 3 versions, it was getting too confusing.
 * See also standalone.c and arcs.c
 * Dave Olson, 7/92
*/

static int save_interrupt = -1;

/*
 * make the current level interruptible or not according to flag.
 *	 1 - interruptible
 *	 0 - non-interruptible
 *	-1 - killable
 * return previous flag value.
 */
int
setintr(int flag)
{
	register int oldintr;

	oldintr = save_interrupt;
	if( flag == 0 ) {
		save_interrupt = flag;
		signal(SIGINT, SIG_IGN);
	}
	else if( flag > 0 ) {
		save_interrupt = flag;
		signal(SIGINT, mpop);
	}
	else {
		save_interrupt = flag;
		signal(SIGINT, SIG_DFL);
	}

	return(oldintr);
}

/*
 * make the current level interruptible, and return the previous
 * interruptibility.  if the current level was killable, no change.
 */
int
mkintr(void)
{
	register int oldintr;

	oldintr = save_interrupt;
	signal(SIGINT, SIG_IGN);
	save_interrupt = 0;
	if( oldintr == -1 ) {
		save_interrupt = -1;
		signal(SIGINT, SIG_DFL);
	}
	else {
		save_interrupt = 1;
		signal(SIGINT, mpop);
	}

	return(oldintr);
}


/* get the screenwidth.  Return 80 (texport width) for standalone,
 * and if the ioctl fails, as it does for regular tty lines on your
 * system.
*/
uint
getscreenwidth(void)
{
	struct winsize win;
	if(ioctl(1, TIOCGWINSZ, &win) == -1)
		return 80;
	return (uint)win.ws_col > 1 ? (uint)win.ws_col : 80;
}
#endif /* _STANDALONE (whole file) */
