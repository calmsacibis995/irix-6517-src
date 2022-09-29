#if defined(_STANDALONE) && !defined(ARCS_SA)	/* whole file */
#include "fx.h"
#include <saioctl.h>

/* routines needed for old (pre-arcs) standalone.
 * Also see arcs.c and irix.c
 * Dave Olson, 7/92
*/

extern int *sig_jmp;	/* from input.c */

/* nop stub */
fflush(void *t)
{
}


/*
 * make the current level interruptible or not according to flag.
 *	 1 - interruptible
 *	 0 - non-interruptible
 *	-1 - killable
 * return previous flag value.  ioctl returns
 *	 1 - non-interruptible
 *	 0 - killable
 *	xx - interruptible
 */
int
setintr(int flag)
{
    int oldintr;

    if( flag == 0 )
	oldintr = ioctl(0, TIOCINTR, 1);
    else
    if( flag > 0 )
	oldintr = ioctl(0, TIOCINTR, sig_jmp);
    else
	oldintr = ioctl(0, TIOCINTR, 0);

    return oldintr==0 ?-1 : oldintr==1 ?0 :1;
}

/*
 * make the current level interruptible, and return the previous
 * interruptibility.  if the current level was killable, no change.
 */
int
mkintr(void)
{
    int oldintr;

    oldintr = ioctl(0, TIOCINTR, 1);
    if( oldintr == 0 )
	(void)ioctl(0, TIOCINTR, oldintr);
    else
	(void)ioctl(0, TIOCINTR, sig_jmp);

    return oldintr==0 ?-1 : oldintr==1 ?0 :1;
}


/* get the screenwidth.  Return 80 (texport width) for standalone */
uint
getscreenwidth(void)
{
	return 80;
}

chkmounts(char *file)
{
	return 0;
}
#endif /* defined(_STANDALONE) && !defined(ARCS_SA) (whole file) */
