/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident "$Revision: 1.7 $"

#include "uucp.h"

#define USER	1

#include "pk.h"
#include <sys/buf.h>


/*
** Code added to allow translation of states from numbers to
** letters, to be done in such a way as to be meaningful to 
** John Q. Public
*/
struct {
	int state;
	char *msg;
} st_trans[] = {
	DEAD,	"Dead!",
	INITa,	"INIT code a",
	INITb,	"INIT code b",
	LIVE,	"O.K.",
	RXMIT,	"Rcv/Xmit",
	RREJ,	"RREJ?",
	PDEBUG,	"PDEBUG?",
	DRAINO,	"Draino...",
	WAITO,	"Waiting",
	DOWN,	"Link down",
	RCLOSE,	"RCLOSE?",
	BADFRAME,	"Bad frame",
	-1,	"End of the line",
};



#define PKMAXSTMSG 40
extern int Errorrate;
int Connodata = 0;		/* Continuous Non Valid Data Count */
int Ntimeout = 0;
#define CONNODATA	20	/* Max Continuous Non Valid Data Count */
#define NTIMEOUT	50	/* This is not currently used, but maybe future */

/*
 * packet driver support routines
 *
 */
extern struct pack *pklines[];

/*
 * start initial synchronization.
 */
struct pack *
pkopen(ifn, ofn)
int ifn, ofn;
{
	register struct pack *pk;
	register char **bp;
	register int i;
#ifndef sgi
	char *malloc();
#endif

	if (++pkactive >= NPLINES)
		return(NULL);
	if ((pk = (struct pack *) malloc(sizeof (struct pack))) == NULL)
		return(NULL);
	pkzero((caddr_t) pk, sizeof (struct pack));
	pk->p_ifn = ifn;
	pk->p_ofn = ofn;
	pk->p_xsize = pk->p_rsize = PACKSIZE;
	pk->p_rwindow = pk->p_swindow = WINDOWS;

	/*
	 * allocate input window
	 */
	for (i = 0; i < pk->p_rwindow; i++) {
		if ((bp = (char **) malloc((unsigned) pk->p_xsize)) == NULL)
			break;
		*bp = (char *) pk->p_ipool;
		pk->p_ipool = bp;
	}
	if (i == 0)
		return(NULL);
	pk->p_rwindow = i;

	/*
	 * start synchronization
	 */
	pk->p_msg = pk->p_rmsg = M_INITA;
	for (i = 0; i < NPLINES; i++) {
		if (pklines[i] == NULL) {
			pklines[i] = pk;
			break;
		}
	}
	if (i >= NPLINES)
		return(NULL);
	pkoutput(pk);

	for (i = 0; i < PKMAXSTMSG; i++) {
		pkgetpack(pk);
		if ((pk->p_state & LIVE) != 0)
			break;
	}
	if (i >= PKMAXSTMSG)
		return(NULL);

	pkreset(pk);
	return(pk);
}

/*
 * input framing and block checking.
 * frame layout for most devices is:
 *	
 *	S|K|X|Y|C|Z|  ... data ... |
 *
 *	where 	S	== initial synch byte
 *		K	== encoded frame size (indexes pksizes[])
 *		X, Y	== block check bytes
 *		C	== control byte
 *		Z	== XOR of header (K^X^Y^C)
 *		data	== 0 or more data bytes
 *
 */
#define GETRIES 10

/*
 * Pseudo-dma byte collection.
 */
void
pkgetpack(ipk)
register struct pack *ipk;
{
	register char *p;
	register struct pack *pk;
	register struct header *h;
	unsigned short sum;
	int i, ret, k, tries, ifn;
	char **bp, hdchk, msgline[80], delimc;

	pk = ipk;
	if ((pk->p_state & DOWN) ||
	  Connodata > CONNODATA  /* || Ntimeout > NTIMEOUT */ )
		pkfail();
	ifn = pk->p_ifn;

	/*
	 * find HEADER
	 */
	for (tries = 0; tries < GETRIES; ) {
		p = (caddr_t) &pk->p_ihbuf;
		if ((ret = pkcget(ifn, p, 1)) < 0) {

			/*
			 * set up retransmit or REJ
			 */
			tries++;
			pk->p_msg |= pk->p_rmsg;
			if (pk->p_msg == 0)
				pk->p_msg |= M_RR;
			if ((pk->p_state & LIVE) == LIVE)
				pk->p_state |= RXMIT;
			pkoutput(pk);
			continue;
		}
		if (*p != SYN)
			continue;
		p++;
		ret = pkcget(ifn, p, HDRSIZ - 1);
		if (ret == -1)
			continue;
		break;
	}
	if (tries >= GETRIES) {
		DEBUG(4, "tries = %d\n", tries);
		pkfail();
	}

	Connodata++;
	DEBUG(9, "pkgetpack: Connodata=%d\n", Connodata);
	h = (struct header * ) &pk->p_ihbuf;
	p = (caddr_t) h;
	hdchk = p[1] ^ p[2] ^ p[3] ^ p[4];
	p += 2;
	sum = (unsigned) *p++ & 0377;
	sum |= (unsigned) *p << 8;
	h->sum = sum;
	DEBUG(9, "rec h->cntl %o\n", (unsigned) h->cntl);
	k = h->ksize;
	if (hdchk != h->ccntl) {

		/*
		 * bad header
		 */
		DEBUG(7, "bad header %o,", hdchk);
		DEBUG(7, "h->ccntl %o\n", h->ccntl);
		return;
	}
	if (k == 9) {
		if (h->sum + h->cntl == CHECK) {
			pkcntl(h->cntl, pk);
			/* 
			** New code added to make the state diagnostics
			** meaningful to human beings that can't figure
			** out bizarre, silly, cryptic numbers.
			** 
			** Exhibit A: DEBUG(7, "state - %o\n", pk->p_state);
			*/
			if(Debug >= 7) {
			    sprintf(msgline,"state -");
			    delimc = ' ';
			    for(i=0;st_trans[i].state!= -1;i++) {
				if(pk->p_state&st_trans[i].state){
				    sprintf(msgline,"%s%c[%s]",msgline,delimc,
					st_trans[i].msg);
				    delimc = '&';
				}
			    }
			    sprintf(msgline,"%s (%o)\n",msgline,pk->p_state);
			    DEBUG(7,"%s",msgline);
			}
					
		} else {

			/*
			 * bad header
			 */
			DEBUG(7, "bad header %o\n", h->cntl);
			pk->p_state |= BADFRAME;
		}
		return;
	}
	if (k && pksizes[k] == pk->p_rsize) {
		pk->p_rpr = h->cntl & MOD8;
		pksack(pk);
		bp = pk->p_ipool;
		if ((bp = pk->p_ipool) == NULL) {
			DEBUG(7, "bp NULL, all buffers lost!%s\n", "");
			return;
		}
		pk->p_ipool = (char **) *bp;
		ret = pkcget(pk->p_ifn, (char *) bp, pk->p_rsize);
		if (ret == 0)
			pkdata(h->cntl, h->sum, pk, bp);
		else {
			*bp = (char *)pk->p_ipool;
			pk->p_ipool = bp;
		}
	}
}


void
pkdata(char c, unsigned short sum, struct pack *pk, char **bp)
{
	register x;
	int t;
	char m;

	if (pk->p_state & DRAINO || !(pk->p_state & LIVE)) {
		pk->p_msg |= pk->p_rmsg;
		pkoutput(pk);
		goto drop;
	}
	t = next[pk->p_pr];
	for(x=pk->p_pr; x!=t; x = (x-1)&7) {
		if (pk->p_is[x] == 0)
			goto slot;
	}
drop:
	*bp = (char *)pk->p_ipool;
	pk->p_ipool = bp;
	return;

slot:
	m = mask[x];
	pk->p_imap |= m;
	pk->p_is[x] = c;
	pk->p_isum[x] = sum;
	pk->p_ib[x] = (char *)bp;
}

#define PKMAXBUF 128

/*
 * Start transmission on output device 
 * device associated with pk.
 * For asynch devices (t_line==1) framing is
 * imposed.  For devices with framing and crc
 * in the driver (t_line==2) the transfer is
 * passed on to the driver.
 */
void
pkxstart(register struct pack *pk, char cntl, int x)
{
	register char *p;
	register short checkword;
	register char hdchk;
	int ret;

	p = (caddr_t) &pk->p_ohbuf;
	*p++ = SYN;
	if (x < 0) {
		*p++ = hdchk = 9;
		checkword = cntl;
	} else {
		*p++ = hdchk = pk->p_lpsize;
		checkword = pk->p_osum[x] ^ (unsigned)(cntl & 0377);
	}
	checkword = CHECK - checkword;
	*p = checkword;
	hdchk ^= *p++;
	*p = checkword>>8;
	hdchk ^= *p++;
	*p = cntl;
	hdchk ^= *p++;
	*p = hdchk;

 /*
  * writes
  */
DEBUG(9, "send %o\n", (unsigned) cntl);
	p = (caddr_t) & pk->p_ohbuf;
	if (x < 0) {
		ret = (*Write)(pk->p_ofn, p, HDRSIZ);
		PKASSERT(ret == HDRSIZ, "PKXSTART ret", "", ret);
	} else {
		register char *b, *q;
		register int i;
		char buf[PKMAXBUF + HDRSIZ]; 


/*
		for (i = 0, b = buf; i < HDRSIZ; i++) 
			*b++ = *p++;
		for (i = 0, p = pk->p_ob[x]; i < pk->p_xsize; i++)
			*b++ = *p++;
*/
		i = HDRSIZ;
		b = buf;
		q = p;
		do
			*b++ = *q++;
		while(--i);
		if(i = pk->p_xsize){
			q = pk->p_ob[x];
			do
				*b++ = *q++;
			while(--i);
		}
		ret = (*Write)(pk->p_ofn, buf, pk->p_xsize + HDRSIZ);
		PKASSERT(ret == pk->p_xsize + HDRSIZ,
		  "PKXSTART ret", "", ret);
	}
	if (pk->p_msg)
		pkoutput(pk);
}




/*
 * get n characters from input
 *	b	-> buffer for characters
 *	fn	-> file descriptor
 *	n	-> requested number of characters
 * return: 
 *	n	-> number of characters returned
 *	0	-> end of file
 */
jmp_buf Getjbuf;
void 
cgalarm() { longjmp(Getjbuf, 1); }
int npkccall, npkctry, npkcnap, npkcchar;

pkcget(fn, b, n)
register int n;
register char *b;
register int fn;
{
	register int ret;
	register int donap;

	if (n == 0)
		return(0);
	npkccall++;
	donap = (linebaudrate > 0 && linebaudrate < 4800);
	if (setjmp(Getjbuf)) {
		Ntimeout++;
		DEBUG(4, "alarm %d\n", Ntimeout);
		return(-1);
	}
	(void) signal(SIGALRM, cgalarm);

	(void) alarm((unsigned) (n < HDRSIZ ? 10 : 20));
	for (;;) {
		if ((ret = (*Read)(fn, b, n)) == 0) {
			DEBUG(4, "pkcget, read failed, errno %d\n", errno);
			(void) alarm(0);
			return(-1);
		}
		npkctry++;
		npkcchar += ret;
		PKASSERT(ret > 0, "PKCGET READ", "", ret);
		if ((n -= ret) <= 0)
			break;
#ifdef PKSPEEDUP
		if (donap) {
#ifdef sgi
			sginap(HZ/6);
#else
			sleep(1);
#endif
			npkcnap++;
#ifdef BSD4_2
		} else {
			nap(HZ/6);
#endif /*  BSD4_2  */
		}
#endif /*  PKSPEEDUP  */
		b += ret;
	}
	(void) alarm(0);
	return(0);
}
