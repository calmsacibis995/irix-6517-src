/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 1.9 $"

#include "uucp.h"


#define PACKSIZE	64
#define SNDFILE	'S'
#define RCVFILE 'R'
#define RESET	'X'

int	linebaudrate = 0;	/* for speedup hook in pk (unused in ATTSV) */
static int Saved_line;		/* was savline() successful?	*/


static struct termio Savettyb;
/*
 * set speed/echo/mode...
 *	tty	-> terminal name
 *	spwant	-> speed
 *	type	-> type
 *
 *	if spwant == 0, speed is untouched
 *	type is unused, but needed for compatibility
 *
 * return:
 *	none
 */
/* ARGSUSED */
void
fixline(tty, spwant, type)
int	tty, spwant, type;
{
	struct termio		ttbuf;

	DEBUG(6, "fixline(%d, ", tty);
	DEBUG(6, "%d)\n", spwant);
	if ((*Ioctl)(tty, TCGETA, &ttbuf) != 0)
		return;
	if (spwant > 0)
		ttbuf.c_ospeed = (speed_t)spwant;
	ttbuf.c_iflag = ttbuf.c_oflag = ttbuf.c_lflag = (ushort)0;

	ttbuf.c_cflag &= ~CLOCAL;
	ttbuf.c_cflag |= (CS8 | CREAD | (spwant ? HUPCL : 0));
	ttbuf.c_cc[VMIN] = 1;
	ttbuf.c_cc[VTIME] = 1;

	ASSERT((*Ioctl)(tty, TCSETAW, &ttbuf) >= 0,
	    "RETURN FROM fixline ioctl", "", errno);
}

void
sethup(dcf)
int	dcf;
{
	struct termio ttbuf;

	if ((*Ioctl)(dcf, TCGETA, &ttbuf) != 0)
		return;
	if (!(ttbuf.c_cflag & HUPCL)) {
		ttbuf.c_cflag |= HUPCL;
		(void) (*Ioctl)(dcf, TCSETAW, &ttbuf);
	}
}

void
ttygenbrk(fn)
register int	fn;
{
	if (isatty(fn))
		(void) (*Ioctl)(fn, TCSBRK, 0);
}


/*
 * optimize line setting for sending or receiving files
 * return:
 *	none
 */
void
setline(register char	type)
{
	static struct termio tbuf;

	if ((*Ioctl)(Ifn, TCGETA, &tbuf) != 0)
		return;
	DEBUG(2, "setline - %c\n", type);
	switch (type) {
	case RCVFILE:
		if (tbuf.c_cc[VMIN] != PACKSIZE/2) {
		    tbuf.c_cc[VMIN] = PACKSIZE/2;
		    (void) (*Ioctl)(Ifn, TCSETAW, &tbuf);
		}
		break;

	case SNDFILE:
	case RESET:
		if (tbuf.c_cc[VMIN] != 1) {
		    tbuf.c_cc[VMIN] = 1;
		    (void) (*Ioctl)(Ifn, TCSETAW, &tbuf);
		}
		break;
	}
}

savline()
{
	if ( (*Ioctl)(0, TCGETA, &Savettyb) != 0 )
		Saved_line = FALSE;
	else {
		Saved_line = TRUE;
		Savettyb.c_cflag = (Savettyb.c_cflag & ~CS8) | CS7;
		Savettyb.c_oflag |= OPOST;
		Savettyb.c_lflag |= (ISIG|ICANON|ECHO);
	}
	return(0);
}

#ifdef SYTEK

/***
 *	sytfixline(tty, spwant)	set speed/echo/mode...
 *	int tty, spwant;
 *
 *	return codes:  none
 */

sytfixline(tty, spwant)
int tty, spwant;
{
	struct termio ttbuf;
	struct sg_spds *ps;
	int speed = -1;
	int ret;

	if ( (*Ioctl)(tty, TCGETA, &ttbuf) != 0 )
		return;
	for (ps = spds; ps->sp_val >= 0; ps++)
		if (ps->sp_val == spwant)
			speed = ps->sp_name;
	DEBUG(4, "sytfixline - speed= %d\n", speed);
	ASSERT(speed >= 0, "BAD SPEED", "", spwant);
	ttbuf.c_iflag = (ushort)0;
	ttbuf.c_oflag = (ushort)0;
	ttbuf.c_lflag = (ushort)0;
	ttbuf.c_cflag = speed;
	ttbuf.c_cflag |= (CS8|CLOCAL);
	ttbuf.c_cc[VMIN] = 6;
	ttbuf.c_cc[VTIME] = 1;
	ret = (*Ioctl)(tty, TCSETAW, &ttbuf);
	ASSERT(ret >= 0, "RETURN FROM sytfixline", "", ret);
	return;
}

sytfix2line(tty)
int tty;
{
	struct termio ttbuf;
	int ret;

	if ( (*Ioctl)(tty, TCGETA, &ttbuf) != 0 )
		return;
	ttbuf.c_cflag &= ~CLOCAL;
	ttbuf.c_cflag |= CREAD|HUPCL;
	ret = (*Ioctl)(tty, TCSETAW, &ttbuf);
	ASSERT(ret >= 0, "RETURN FROM sytfix2line", "", ret);
	return;
}

#endif /* SYTEK */

restline()
{
	if ( Saved_line == TRUE )
		return((*Ioctl)(0, TCSETAW, &Savettyb));
	return(0);
}

