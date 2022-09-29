#include <sys/types.h>
#include <rpc/rpc.h>
#include <sys/termio.h>
#include "rexioctl.h"
#include "rex.h"
#include <syslog.h>

test_fd(p)
register fd_set	*p;
{
    register int tmp, count;

    count = howmany(FD_SETSIZE, NFDBITS);

    for ( tmp = 0; tmp < count; tmp++)
	if ( p->fds_bits[tmp] )
	    return(1);
    return (0);
}

void
fd_and(a, b)
register fd_set	*a;
register fd_set	*b;
{
    register int tmp, count;

    count = howmany(FD_SETSIZE, NFDBITS);

    for ( tmp = 0; tmp < count; tmp++ )
	a->fds_bits[tmp] &= b->fds_bits[tmp];
}

void
fd_or(a, b)
register fd_set	*a;
register fd_set	*b;
{
    register int tmp, count;

    count = howmany(FD_SETSIZE, NFDBITS);

    for ( tmp = 0; tmp < count; tmp++ )
	a->fds_bits[tmp] |= b->fds_bits[tmp];
}

/*
* translate struct termio to struct rex_ttymode.
*/
void
trans_tty_mode(mode, flags)
register struct rex_ttymode	*mode;
register struct termio	*flags;
{
    mode->basic.sg_ispeed = termio_to_wire(flags->c_ispeed);
    mode->basic.sg_ospeed = termio_to_wire(flags->c_ospeed);
    mode->basic.sg_erase = flags->c_cc[VERASE];
    mode->basic.sg_kill = flags->c_cc[VKILL];
    mode->basic.sg_flags = 0;
    if ( flags->c_iflag & IXOFF )
	mode->basic.sg_flags |= TANDEM;
    if ( !(flags->c_lflag & ICANON) && (flags->c_lflag & ISIG) )
	mode->basic.sg_flags |= CBREAK;
    if ( flags->c_iflag & IUCLC )
	mode->basic.sg_flags |= LCASE;
    if ( flags->c_lflag & ECHO )
	mode->basic.sg_flags |= B_ECHO;
    if ( flags->c_oflag & ONLCR )
	mode->basic.sg_flags |= CRMOD;
    if ( !(flags->c_lflag & ICANON) && !(flags->c_lflag & ISIG) )
	mode->basic.sg_flags |= RAW;
    if ( (flags->c_iflag & INPCK) || (flags->c_cflag & PARENB) ) {
	if (flags->c_cflag & PARODD)
	    mode->basic.sg_flags |= ODDP;
	else
	    mode->basic.sg_flags |= EVENP;
    }
    if ( flags->c_oflag & NL1 )
	mode->basic.sg_flags |= B_NL1;
    if ( flags->c_oflag & CR1 )
	mode->basic.sg_flags |= B_CR1;
    if ( flags->c_oflag & CR2 )
	mode->basic.sg_flags |= B_CR2;
    if ( flags->c_oflag & TAB1 )
	mode->basic.sg_flags |= B_TAB1;
    if ( flags->c_oflag & TAB2 )
	mode->basic.sg_flags |= B_TAB2;
    if ( flags->c_oflag & BS1 )
	mode->basic.sg_flags |= B_BS1;
    if ( flags->c_oflag & VT1 )
	mode->basic.sg_flags |= B_FF1;

    mode->more.t_intrc = flags->c_cc[VINTR];
    mode->more.t_quitc = flags->c_cc[VQUIT];
    mode->more.t_eofc = flags->c_cc[VEOF];
    mode->more.t_brkc = CDEL;

    if (flags->c_line == LDISC1) {
	mode->more.t_startc = flags->c_cc[VSTART];
	mode->more.t_stopc = flags->c_cc[VSTOP];
	mode->yetmore.t_suspc  = flags->c_cc[VSUSP];
	mode->yetmore.t_rprntc = flags->c_cc[VRPRNT];
	mode->yetmore.t_flushc = flags->c_cc[VFLUSHO];
	mode->yetmore.t_werasc = flags->c_cc[VWERASE];
	mode->yetmore.t_lnextc = flags->c_cc[VLNEXT];
    } else {
	mode->more.t_startc = CSTART;
	mode->more.t_stopc = CSTOP;
	mode->yetmore.t_suspc  = CSUSP;
	mode->yetmore.t_rprntc = CRPRNT;
	mode->yetmore.t_flushc = CFLUSH;
	mode->yetmore.t_werasc = CWERASE;
	mode->yetmore.t_lnextc = CLNEXT;
    }
    mode->yetmore.t_dsuspc = CDSUSP;

    mode->andmore = 0;
    if ( flags->c_lflag & ECHOE )
	mode->andmore |= (CRTBS | CRTERA);
    if ( flags->c_lflag & ECHOK )
	mode->andmore |= CRTKIL;
    if ( !(flags->c_iflag & IXANY) )
	mode->andmore |= DECCTQ;
    if ( flags->c_lflag & NOFLSH )
	mode->andmore |= B_NOFLSH;
    if ( flags->c_cflag & LOBLK )
	mode->andmore |= B_TOSTOP;
    mode->andmore >>= 16;
}

void
Term_BsdToSysV(mode, term)
    register struct rex_ttymode	*mode;
    register struct termio	*term;
{
    term->c_iflag = BRKINT;
    if ( !(mode->basic.sg_flags & ANYP) )
	term->c_iflag |= IGNPAR;
    else
	term->c_iflag |= INPCK;

    term->c_iflag |= ISTRIP;
    term->c_iflag |= ICRNL;
    if ( mode->basic.sg_flags & LCASE )
	term->c_iflag |= IUCLC;
    term->c_iflag |= IXON;
    if ( mode->basic.sg_flags & TANDEM )
	term->c_iflag |= IXOFF;
    if ( mode->andmore & (DECCTQ>>16) )
	term->c_iflag |= IXANY;


    term->c_oflag = OPOST;
    if ( mode->basic.sg_flags & CRMOD )
	term->c_oflag |= ONLCR;
    if ( mode->basic.sg_flags & B_NL1 )
	term->c_oflag |= NL1;
    if ( mode->basic.sg_flags & B_CR1 )
	term->c_oflag |= CR1;
    if ( mode->basic.sg_flags & B_CR2 )
	term->c_oflag |= CR2;
    if ( mode->basic.sg_flags & B_TAB1 )
	term->c_oflag |= TAB1;
    if ( mode->basic.sg_flags & B_TAB2 )
	term->c_oflag |= TAB2;
    if ( mode->basic.sg_flags & B_BS1 )
	term->c_oflag |= BS1;
    if ( mode->basic.sg_flags & B_FF1 )
	term->c_oflag |= VT1;

    term->c_ispeed = wire_to_termio(mode->basic.sg_ispeed);
    term->c_ospeed = wire_to_termio(mode->basic.sg_ospeed);
    term->c_cflag = (CS8 | CREAD | CLOCAL);
    if ( mode->basic.sg_flags & ANYP )
	term->c_cflag |= PARENB;
    if ( mode->basic.sg_flags & ODDP )
	term->c_cflag |= PARODD;

    term->c_lflag = 0;
    if ( !(mode->basic.sg_flags & RAW) ) {
	term->c_lflag |= ISIG;
	if ( !(mode->basic.sg_flags & CBREAK) )
	    term->c_lflag |= ICANON;
    }
    if ( mode->basic.sg_flags & B_ECHO )
	term->c_lflag |= ECHO ;
    if ( mode->andmore & ( (CRTBS|CRTERA) >> 16 ))
	term->c_lflag |= ECHOE;
    if ( mode->andmore & (CRTKIL>>16) )
	term->c_lflag |= ECHOK;
    if ( mode->andmore & (B_NOFLSH>>16) )
	term->c_lflag |= NOFLSH;
    if ( mode->andmore & (B_TOSTOP>>16) )
	term->c_cflag |= LOBLK;
	    

    term->c_line = LDISC1;

    term->c_cc[VINTR] = mode->more.t_intrc;
    term->c_cc[VQUIT] = mode->more.t_quitc;
    term->c_cc[VERASE] = mode->basic.sg_erase;
    term->c_cc[VKILL] = mode->basic.sg_kill;
    term->c_cc[VEOF] = CEOF;
    term->c_cc[VEOL] = CNUL;
    term->c_cc[VEOL2] = CNUL;
    term->c_cc[VSUSP] = mode->yetmore.t_suspc;
    term->c_cc[VRPRNT] = mode->yetmore.t_rprntc;
    term->c_cc[VFLUSHO] = mode->yetmore.t_flushc;
    term->c_cc[VWERASE] = mode->yetmore.t_werasc;
    term->c_cc[VLNEXT] = mode->yetmore.t_lnextc;
    term->c_cc[VSTART] = mode->more.t_startc;
    term->c_cc[VSTOP] = mode->more.t_stopc;

}

struct speed_ent {
	int wire;
	int local;
};

/*
 * I'm not sure that more values are legal here
 */
struct speed_ent speed_map[] = {
	{ 0, 0 },
	{ 1, 50 },
	{ 2, 75 },
	{ 3, 110 },
	{ 4, 134 },
	{ 5, 150 },
	{ 6, 200 },
	{ 7, 300 },
	{ 8, 600 },
	{ 9, 1200 },
	{ 0xa, 1800 },
	{ 0xb, 2400 },
	{ 0xc, 4800 },
	{ 0xd, 9600 },
	{ 0xe, 19200 },
	{ 0xf, 38400 }
};

int nspeeds = sizeof(speed_map) / sizeof(struct speed_ent);

int
termio_to_wire(int local, int wire)
{
	int i;

	for (i = 0; i < nspeeds; i++) {
		if (speed_map[i].local == local) {
			return speed_map[i].wire;
		}
	}
	return speed_map[nspeeds - 1].wire;
}

int
wire_to_termio(int wire, int local)
{
	int i;

	for (i = 0; i < nspeeds; i++) {
		if (speed_map[i].wire == wire) {
			return speed_map[i].local;
		}
	}
	return speed_map[nspeeds - 1].local;
}
