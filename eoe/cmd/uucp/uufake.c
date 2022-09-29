/* plaster over holes in the code ripped out of UUCP for PPP and SLIP
 */

#include "uucp.h"

extern int debug;

extern void kludge_stderr(void);

jmp_buf Sjbuf;


#ident "$Revision: 1.11 $"

void
sethup(int dcf)
{
	struct termio ttbuf;

	if (0 > ioctl(dcf, TCGETA, &ttbuf))
		return;
	if (!(ttbuf.c_cflag & HUPCL)) {
		ttbuf.c_cflag |= HUPCL;
		(void)ioctl(dcf, TCSETAW, &ttbuf);
	}
}


void
ttygenbrk(int fn)
{
	if (isatty(fn))
		(void)ioctl(fn, TCSBRK, 0);
}


#define HEADERSIZE	6
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
fixline(int tty, int spwant, int type)
{
#if defined(PPP_IRIX_62) || defined(PPP_IRIX_53)
	register struct sg_spds	*ps;
	static struct sg_spds {
		int	sp_val,
		sp_name;
	} spds[] = {
		{ 300,  B300},
		{ 600,  B600},
		{1200, B1200},
		{2400, B2400},
		{4800, B4800},
		{9600, B9600},
		{19200,	B19200},
		{38400,	B38400},
		{0,    0}
	};
	int			speed = -1;
#endif
	struct termio		ttbuf;

	DEBUG(6, "fixline(%d, ", tty);
	DEBUG(6, "%d)\n", spwant);
	if ((*Ioctl)(tty, TCGETA, &ttbuf) != 0)
		return;
	if (spwant > 0) {
#if defined(PPP_IRIX_62) || defined(PPP_IRIX_53)
		for (ps = spds; ps->sp_val; ps++)
			if (ps->sp_val == spwant) {
				speed = ps->sp_name;
				break;
			}
		ASSERT(speed >= 0, "BAD SPEED", "", spwant);
#ifdef CNEW_RTSCTS
		ttbuf.c_cflag = (ttbuf.c_cflag & CNEW_RTSCTS) | speed;
	} else
		ttbuf.c_cflag &= (CBAUD | CNEW_RTSCTS);
#else
		ttbuf.c_cflag = speed;
	} else
		ttbuf.c_cflag &= CBAUD;
#endif
#else
		ttbuf.c_ospeed = (speed_t)spwant;
	}
#endif
	ttbuf.c_iflag = ttbuf.c_oflag = ttbuf.c_lflag = (ushort)0;


	ttbuf.c_cflag &= ~CLOCAL;
	ttbuf.c_cflag |= (CS8 | CREAD);
#if defined(PPP_IRIX_62) || defined(PPP_IRIX_53)
	ttbuf.c_cflag |= (speed ? HUPCL : 0);
#else
	ttbuf.c_cflag |= (spwant ? HUPCL : 0);
#endif
	ttbuf.c_cc[VMIN] = HEADERSIZE;
	ttbuf.c_cc[VTIME] = 1;

	ASSERT((*Ioctl)(tty, TCSETAW, &ttbuf) >= 0,
	    "RETURN FROM fixline ioctl", "", errno);
	return;
}


int
restline(void)
{
	return(ioctl(0, TCSETAW, &Savettyb));
}


void
assert(char *s1, char *s2, int i1, char *file, int lineno)
{
	kludge_stderr();
	(void)fprintf(stderr, "%s %s (%d) [FILE: %s, LINE: %d]\n",
		      s1, s2, i1, file, lineno);
}


void
logent(char *text, char *status)
{
	if (debug) {
		kludge_stderr();
		(void)fprintf(stderr, "%s (%s)\n", status, text);
	}
}
