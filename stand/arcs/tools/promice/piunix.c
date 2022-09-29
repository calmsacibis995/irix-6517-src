/*	PiUNIX.c - Edit 6

	LoadICE Version 3
	Copyright (C) 1990-95 Grammar Engine, Inc.
	All rights reserved
	
	NOTICE:  This software source is a licensed copy of Grammar Engine's
	property.  It is supplied to you as part of support and maintenance
	of some Grammar Engine products that you may have purchased.  Use of
	this software is strictly limited to use with such products.  Any
	other use constitutes a violation of this license to you.
*/

/*	S Y S T E M  S P E C I F I C   D E V I C E  I/O  R O U T I N E S
	For UNIX systems
	- 4/23/92 - added stuff for BSD derivatives, different IOCTLs
	- 12/5/94 - added Ethernet support via FastPort (print server)
	- 12/5/94 - added parallel port support for HP workstations
	- 4/03/95 - added stuff to compile under solaris (define SOLARIS)
*/

#ifdef UNIX
#include <stdio.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/termio.h>
#include <sgtty.h>
#include <sys/types.h>
#include <sys/time.h>
#ifdef HP
#include <sys/iomap.h>
#endif
#include <sys/errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "piconfig.h"
#include "pistruct.h"
#include "pierror.h"
#include "pidriver.h"
#include "pidata.h"
#include "pidev.h"

#ifdef ANSI
long pi_raw(void);
long pi_toggle(void);
long pi_rcv(void);
void pi_cook(void);
long pi_switch(void);
void pi_delay(void);
void pi_sleep(short time);
void pi_setime(time_t time);
long pi_chktime();
void pi_put(char data);
void pi_putp(char data);
void pi_puts(char *str);
long pi_get(char *data);
long pi_getp(char *data);
void pi_putp(char data);
void pi_putp2(char data);
void pi_ccw(void);
void pi_cccw(void);
void do_outp(char *, int b);
char do_inp(char *a);
#else
long pi_raw();
long pi_toggle();
long pi_rcv();
void pi_cook();
long pi_switch();
void pi_setime();
long pi_chktime();
void pi_delay();
void pi_sleep();
void pi_put();
void pi_putp();
void pi_puts();
long pi_get();
long pi_getp();
void pi_putp();
void pi_putp2();
void pi_ccw();
void pi_cccw();
void do_outp();
char do_inp();
#endif
char *pxaicstr = "MCON 000 000";

extern short pptwo;
extern int pclog;
static time_t pithen;
static time_t ttime;
static long to;
int umodem;
int picuct;
int fpsock;
struct sockaddr_in server;
struct hostent *hp;
static unsigned char ppon[4] = {0,PI_MO,1,MO_PPXN};
static unsigned char ppoff[4] = {0,PI_MO,1,MO_PPXO};
static char idon = 0;
static int pl_once=0;

/* `pi_raw` - set serial port to raw mode or mess with the parallel port */

long pi_raw()
	{
	int flags, stype, ssize;

	ssize = sizeof(stype);
	if (piflags&PiNE)
		{
		/* reset fastport */
		char one;

		if (piflags&PiRF)
			{
			if ((pxlink.saddr = socket(AF_INET, SOCK_STREAM, 0)) < 0)
				perror("SocketMonitor"), exit(1);
			server.sin_family = AF_INET;

			if (hp = gethostbyname(pxhost))
				{
#ifdef SOLARIS
				memcopy((char *)&server.sin_addr,(char *)hp->h_addr,hp->h_length);
#else
				bcopy((char *)hp->h_addr,(char *)&server.sin_addr,hp->h_length);
#endif
				server.sin_port = htons(2002);
				}
			else
				{
				printf("HOST=%s ",pxhost);
				perror("FastPort hostname");
				exit(1);
				}
			if (connect(pxlink.saddr,(struct sockaddr *)&server,sizeof(server))<0)
				perror("ConnectMonitor"), exit(1);
			for (;;)
				{
				while (pi_get(&one));
				if (one == 'e')
					{
					while (pi_get(&one));
					if (one == '.')
						break;
					}
				}
			pi_put(0x79);
			pi_put(0x0a);
			for (;;)
				{
				while (pi_get(&one));
				if (one == '>')
					break;
				}
			while (pi_get(&one));
			while (pi_get(&one));
			pi_put(0x72);
			pi_put(0x0a);
			close(pxlink.saddr);
			printf("S0-");
			pi_sleep(9);
			}

		/* serial */
		printf("S1-");
		if ((pxlink.saddr = socket(AF_INET, SOCK_STREAM, 0)) < 0)
			perror("SocketSerial"), exit(1);
		server.sin_family = AF_INET;

		if (!(piflags&PiRF))
			{
			if (hp = gethostbyname(pxhost))
				{
#ifdef SOLARIS
				memcopy((char *)&server.sin_addr,(char *)hp->h_addr,hp->h_length);
#else
				bcopy((char *)hp->h_addr,(char *)&server.sin_addr,hp->h_length);
#endif
				server.sin_port = htons(2001);
				}
			else
				{
				printf("HOST=%s ",pxhost);
				perror("FastPort hostname");
				exit(1);
				}
			}
		else
			server.sin_port = htons(2001);

		printf("S2-");
		if (connect(pxlink.saddr,(struct sockaddr *)&server,sizeof(server))<0)
			perror("ConnectSerial"), exit(1);

		if (pxlink.flags&(PLPP|PLPQ))
			{
			/* parallel */
			printf("S3-");
			if ((pxlink.paddr = socket(AF_INET, SOCK_STREAM, 0)) < 0)
				perror("SocketParalell"), exit(1);
			server.sin_family = AF_INET;

			server.sin_port = htons(2000);

			printf("S4-");
			if (connect(pxlink.paddr,(struct sockaddr *)&server,sizeof(server))<0)
				perror("ConnectParallel"), exit(1);
			printf("S5-");
			}
		if ((flags = fcntl(pxlink.saddr, F_GETFL, 0)) == -1)
			return(PGE_IOE);
		if (fcntl(pxlink.saddr, F_SETFL, flags | O_NDELAY) == -1)
			return(PGE_IOE);
		}
	else		/* else we are serial and or parallel connection */
		{
#ifdef BSD
		struct sgttyb arg;
		int flags, baud;
#else
		struct termio arg;
		char baud;
		short tflag = 1;
#endif

		if (!(pxlink.flags&PLPQ) || (piflags&PiAI && !(piflags&PiXP)))
			{
#ifdef	HP
			if ((pxlink.saddr = open(pxlink.name,O_RDWR)) < 0)
				return(PGE_BAA);
			close(pxlink.saddr);
#endif
			switch (pxlink.brate)
				{
				case 1200:
					baud = B1200;
					break;
				case 2400:
					baud = B2400;
					break;
				case 4800:
					baud = B4800;
					break;
				case 9600:
					baud = B9600;
					break;
				case 19200:
					baud = B19200;
					break;
				default:
					printf("\nInvalid baud rate %ld changing to 19200",pxlink.brate);
					baud = B19200;
					pxlink.brate = 19200;
					break;
				}


			printf("\nOpening serial device '%s' @BR-%ld",pxlink.name,pxlink.brate);
			if ((pxlink.saddr = open(pxlink.name,O_RDWR)) < 0)
				return(PGE_IOE);
			if ((flags = fcntl(pxlink.saddr, F_GETFL, 0)) == -1)
				return(PGE_IOE);
			if (fcntl(pxlink.saddr, F_SETFL, flags | O_NDELAY) == -1)
				return(PGE_IOE);
#ifdef BSD
			if (ioctl(pxlink.saddr, TIOCGETP, &arg) < 0)
				return(PGE_IOE);
			arg.sg_ispeed = baud;
			arg.sg_ospeed = baud;
			arg.sg_flags = RAW;
			if (ioctl(pxlink.saddr, TIOCSETP, &arg) <0)
				return(PGE_IOE);
#else
			if (ioctl(pxlink.saddr, TCGETA, &arg) < 0)
				return(PGE_IOE);
			arg.c_iflag = 0;
			arg.c_oflag = 0;
#ifdef HP
			arg.c_cflag = baud | CREAD | CS8 | CSTOPB;
#else
			arg.c_cflag = baud | CREAD | CS8 | CSTOPB | CLOCAL;
#endif
			arg.c_lflag = 0;
			arg.c_cc[VMIN] = 1;
			arg.c_cc[VTIME] = 1;
			if (ioctl(pxlink.saddr, TCSETA, &arg) < 0)
				return(PGE_IOE);

			pi_toggle();
			if (piflags&PiAI)
				{
				/*pi_put((char)pxaibchr); */
				pi_toggle();
				pi_toggle();
				pi_toggle();
				}
			tflag = 0;
#ifdef	AISWITCH
			if (piflags&PiSW)
				if (ioctl(pxlink.saddr, TCSBRK, 0) < 0)
					return(PGE_IOE);
			if (ioctl(pxlink.saddr, TCFLSH, TCIOFLUSH) < 0)
				return(PGE_IOE);
#endif
			}
#endif

#ifdef HP
		if (pxlink.flags&(PLPP|PLPQ))
			{
			printf("\nOpening parallel device '%s'",pxlink.pname);
			if ((pxlink.paddr = open(pxlink.pname,O_RDWR)) < 0)
				return(PGE_IOE);
			if (ioctl(pxlink.paddr,IOMAPMAP,&pxlink.paddr) < 0)
				return(PGE_IOE);
			pxlink.paddr += 0x800;
			pxlink.ppin = pxlink.paddr + PP_STS;
			pxlink.ppout = pxlink.paddr + PP_CTL;
			pxlink.ppdat = pxlink.paddr + PP_DAT;
			}
		if (pxlink.flags&PLPQ)
			{
			if (tflag)
				{
				pi_toggle();
				if (piflags&PiAI)
					{
					pi_toggle();
					pi_toggle();
					pi_toggle();
					}	
				}
			}
#endif
		}
	return(PGE_NOE);
	}

/* `pi_toggle` - toggle the PROMICE reset interrupt line */

long pi_toggle()
	{
	if (piflags&PiNE)
		return;
	if (!(pxlink.flags&PLPQ) || (piflags&PiAI && !(piflags&PiXP)))
		{
#ifdef BSD

		if (pxdisp&PXVL)
			printf("^");
		if (ioctl(pxlink.saddr, TIOCSDTR, 0) < 0)
			return(PGE_IOE);
		pi_sleep(1);
		if (ioctl(pxlink.saddr, TIOCCDTR, 0) < 0)
			return(PGE_IOE);
#endif
#ifdef	SUN
		if (pxdisp&PXVL)
			printf("^");
		if (ioctl(pxlink.saddr, TIOCMGET, &umodem) < 0)
			return(PGE_IOE);
		umodem |= TIOCM_DTR;
		if (ioctl(pxlink.saddr, TIOCMSET, &umodem) < 0)
			return(PGE_IOE);
		/*pi_sleep(1); */
		umodem &= ~TIOCM_DTR;
		if (ioctl(pxlink.saddr, TIOCMSET, &umodem) < 0)
			return(PGE_IOE);
#endif
		}
#ifdef HP
	else
		{
		if (pxdisp&PXVL)
			printf("|");
		do_outp(pxlink.ppout, PP_INITS);
		pi_sleep(1);
		do_outp(pxlink.ppout, PP_INITC);
		}
#endif
	}

/* `pi_cook` - restore the serial port */

void pi_cook()
	{
	if (piflags&PiNE)
		{
		shutdown(pxlink.paddr,1);
		shutdown(pxlink.saddr,1);
		close(pxlink.paddr);
		close(pxlink.saddr);
		}
	else
		{
		if (pxlink.flags&(PLPQ|PLPP))
			(void)close(pxlink.paddr);
		if (!(pxlink.flags&PLPQ))
			(void)close(pxlink.saddr);
		}
	}

/* `pi_xmt` - send a block of data over the i/o device */

#ifdef ANSI
long pi_xmt(char *buf, long ct)
#else
long pi_xmt(buf, ct)
char *buf;
long ct;
#endif
	{
	int i, cnt, offset, n_write;
	unsigned char id;
	unsigned char *po;

	pi_setime(PIC_TOT);
	if (pxlink.flags&(PLPP|PLPQ) && !(piflags&PiSO))
		{
		id = *(buf+PIID);
		if ((pxlink.flags&PLPB) || (pxrom[id].mid == 0))
			pptwo = 0;
		else
			pptwo = 1;
		if (pxlink.flags&PLPB)
			{
			if (!(pxrom[id].flags&PRPP))
				{
				ppon[0] = id;
				ppoff[0] = idon;
				if (piflags&PiMU)
					{
					ppon[3] &= ~MO_LOAD;
					ppoff[3] &= ~MO_LOAD;
					}
				else
					{
					ppon[3] |= MO_LOAD;
					ppoff[3] |= MO_LOAD;
					}
				if (pl_once)
					{
					if (pxdisp&PLOG)
						write(pclog,ppoff,4);
					if (piflags&PiNE)
						{
						write(pxlink.paddr,ppoff,4);
						}
					else
						for (po=ppoff,i=0; i<4; i++)
							pi_putp(*po++); 
					pi_rcv();
					}
				if (pxdisp&PLOG)
					write(pclog,ppon,4);
				write(pxlink.saddr,ppon,4);
				pi_rcv();
				if (pl_once)
					{
					pxrom[pxrom[idon].mid].flags &= ~PRPP;
					pxrom[pxrom[idon].sid].flags &= ~PRPP;
					}
				pxrom[pxrom[id].mid].flags |= PRPP;
				pxrom[pxrom[id].sid].flags |= PRPP;
				idon = id;
				pl_once = 1;
				}
			}
		if (piflags&PiNE)
			{
			cnt = ct;
			offset = 0;
			while ((n_write = write(pxlink.paddr, buf+offset, cnt)) != cnt)
				{
				if (n_write > 0)
					{
					pi_chktime();
					offset += n_write;
					cnt -= n_write;
					}
				}
			}
		else
			while (ct--)
				pi_putp(*buf++);
		}
	else
		{
		cnt = ct;
		offset = 0;
		while ((n_write = write(pxlink.saddr, buf+offset, cnt)) != cnt)
			{
			if (n_write > 0)
				{
				pi_chktime();
				offset += n_write;
				cnt -= n_write;
				}
			}
		}
	return(PGE_NOE);
	}

/* `pi_rcv` - receive a standard response from the PROMICE */

long pi_rcv()
	{
	long i,j=0,k,l;

	pi_setime(PIC_TOT);
	if (pxlink.flags&PLPQ || piflags&PiLO)
		{
		if (piflags&PiFP)
			{
			while(!((char)do_inp(pxlink.ppin) & BUSY));
			do_outp(pxlink.ppout, B_ACK);
			pi_delay();
			do_outp(pxlink.ppout, STROFF);
			}
		for (i=0; i<4; i++)
			while (pi_getp(&pxrsp[i]));
		if (pxrsp[PICT] == 0)
			pxrspl = PIC_MP;
		else
			pxrspl = (pxrsp[PICT]&0xff) + 3;
		for (i=4; i<pxrspl; i++)
			while (pi_getp(&pxrsp[i]));
		}
	else
		{
		i=l=0;
		k=4;
		while(k)
			{
			while ((i=read(pxlink.saddr,&pxrsp[l],k)) < 0)
				if (pxerror || (j++>pxtout))
					{
					if (!pxerror)
						pi_chktime();
					if (pxtout)
						return(PGE_TMO);
					}
			l+=i;
			k-=i;
			}
		if (pxrsp[PICT] == 0)
			pxrspl = PIC_MP;
		else
			pxrspl = (pxrsp[PICT]&0xff) + 3;
		i=l=4;
		k=pxrspl-4;
		while(k)
			{
			while ((i=read(pxlink.saddr,&pxrsp[l],k)) < 0)
				if (pxerror || (j++>PIC_DLY))
					{
					if (!pxtout)
						pi_chktime(PIC_TOT);
					if (pxtout)
						return(PGE_TMO);
					}
			l+=i;
			k-=i;
			}
		}
	return(PGE_NOE);
	}

/* pi_clear - clear the parallel serial interface */

void pi_clear()
	{
	int i;
	unsigned char *po;

	ppoff[0] = idon;
	if (pxlink.flags&PLPB)
		{
		if (piflags&PiNE)
			{
			write(pxlink.paddr,ppoff,4);
			}
		else
			for (po=ppoff,i=0; i<4; i++)
				pi_putp(*po++); 
		pi_rcv();
		}
	}

/* `pi_setime`  - set timeout time */

void pi_setime(ttime)
	time_t ttime;
	{
	time_t t;
	pxtout = 0;
	t = time(NULL);
	pithen = t+ttime;
	}

/* `pi_chktime`  - check if timeout happened */	


long pi_chktime()
	{
	time_t t;
	
	if (pxnotot || piflags&PiNT)
		return(1);


	t = time(NULL);
	if (pithen > t)
		return(1);
	pxtout = 1;
	return(0);
	}


/* `pi_put` - send byte over serial port */

#ifdef ANSI
void pi_put(char data)
#else
void pi_put(data)
char data;
#endif
	{
	if (pxlink.flags&PLPQ)
		{
		pi_putp(data);
		}
	else
		{
		if (pxdisp&PXVL)
			printf(" %02x",data&0xff);
		while (write(pxlink.saddr,&data,1)<0);
		if (pxdisp&PXVL)
			printf("+");
		}
	}

#ifdef ANSI
void pi_putp(char data)
#else
void pi_putp(data)
char data;
#endif
	{
	if (pxdisp&PXVL)
		printf(" %02x",data&0xff);

	if (pptwo)
		{
		pi_putp2(data);
		return;
		}

	while(!((char)do_inp(pxlink.ppin) & BUSY));
	do_outp(pxlink.ppdat, data);
	do_outp(pxlink.ppout, STRON);
	pi_delay();
	do_outp(pxlink.ppout, STROFF);
	if (!(piflags&PiFP))
		{
		while(!((char)do_inp(pxlink.ppin) & BUSY));
		do_outp(pxlink.ppout, B_ACK);
		pi_delay();
		do_outp(pxlink.ppout, STROFF);
		}
	if (pxdisp&PXVL)
		printf(">");
	}

void pi_delay()
	{
	long i;
	for (i=0; i<ppxdl0; i++);
	}

 /* use Autofeed as strobe and Out of paper as busy */

#ifdef ANSI
void pi_putp2(char data)
#else
void pi_putp2(data)
char data;
#endif
	{
	if (pxdisp&PXVL)
		printf(" %02x",data&0xff);
	while((char)do_inp(pxlink.ppin) & PAPER);
	do_outp(pxlink.ppdat, data);
	do_outp(pxlink.ppout, AUTOON);
	pi_delay();
	do_outp(pxlink.ppout, AUTOOFF);
	if (!(piflags&PiFP))
		{
		while(((char)do_inp(pxlink.ppin) & PAPER));
		do_outp(pxlink.ppout, B_ACK);
		do_outp(pxlink.ppout, AUTOOFF);
		}
	if (pxdisp&PXVL)
		printf("*");
	}

/* `pi_getp` - read possible byte from the parallel port */

#ifdef ANSI
long pi_getp(char *cp)
#else
long pi_getp(cp)
char *cp;
#endif
	{
	if (pxlink.flags&PLPQ)
		{
		if (!((char)do_inp(pxlink.ppin) & BUSY))
			{
			do_outp(pxlink.ppout, B_ACK);
			*cp = (char)(((char)do_inp(pxlink.ppin)) >> 3) & (char)0x0f;
			do_outp(pxlink.ppout, STROFF);
			while(!((char)do_inp(pxlink.ppin) & BUSY));
			do_outp(pxlink.ppout, B_ACK);
			do_outp(pxlink.ppout, STROFF);
			while(((char)do_inp(pxlink.ppin) & BUSY));
			do_outp(pxlink.ppout, B_ACK);
			*cp = *cp|(char)(((char)do_inp(pxlink.ppin)<<(char)1)&(char)0xf0);
			do_outp(pxlink.ppout, STROFF);
			while(!((char)do_inp(pxlink.ppin) & BUSY));
			do_outp(pxlink.ppout, B_ACK);
			do_outp(pxlink.ppout, STROFF);
			if (pxdisp&PXVL)
				printf("<%02X",*cp&0xff);
			return(PGE_NOE);
			}
		return(PGE_EOF);
		}
	else
		return(PGE_IOE);
	}

/* `pi_get` - read possible byte from the serial or parallel port */

#ifdef ANSI
long pi_get(char *cp)
#else
long pi_get(cp)
char *cp;
#endif
	{
	if (pxlink.flags&PLPQ)
		{
		return(pi_getp(cp));
		}
	else
		{
		if (read(pxlink.saddr,cp,1)>0)
			{
			if (pxdisp&PXVL)
				printf("-%02X",*cp&0xff);
			return(PGE_NOE);
			}
		return(PGE_EOF);
		}
	}

void do_outp(a,b)
char *a, b;
	{
	*a = b;
	}
 
char do_inp(a)
char *a;
	{
	return(*a);
	}

/* `pi_sleep` - waste some time */

#ifdef ANSI
void pi_sleep(short t)
#else
void pi_sleep(t)
short t;
#endif
	{
	sleep(1);
	}

/* `pi_ccw` - make the cursor spin */

void pi_ccw()
	{
	if (piflags&PiHC)
		{
		if (picuct++ % 128)
			return;
		printf(".");
		}
	else
		{
		switch (picuct++ % 4)
			{
			case 0:
				printf("|\b");
				break;
			case 1:
				printf("/\b");
				break;
			case 2:
				printf("-\b");
				break;
			case 3:
				printf("\\\b");
				break;
			}
		}
	}

/* `pi_cccw` - and now the other way */

void pi_cccw()
	{
	if (piflags&PiHC)
		{
		if (picuct++ % 128)
			return;
		printf(".");
		}
	else
		{
		switch (picuct++ % 4)
			{
			case 0:
				printf("|\b");
				break;
			case 1:
				printf("\\\b");
				break;
			case 2:
				printf("-\b");
				break;
			case 3:
				printf("/\b");
				break;
			}
		}
	}

/* `pi_beep` - make a beep sound */

void pi_beep()
	{
	printf("\7");
	}

/* `pi_switch` - connect thru the switch */

long pi_switch()
	{
#ifdef AISWITCH
	char c;
	char aiport[PIC_CL];
	short i,j;

	pi_put(13);
	pi_sleep(1);
	ioctl(pxlink.saddr, TCFLSH, TCOFLUSH);
	do
		{
		while (pi_get(&c));
		} while (c != '>');
	pi_puts("ai");
	i = 0;
	j = 0;
	do
		{
		while (pi_get(&c));
		aiport[i++] = c;
		} while (c != '>');

	*(pxaicstr+5) = aiport[i-6];
	*(pxaicstr+6) = aiport[i-5];
	*(pxaicstr+7) = aiport[i-4];
	if (pxdisp&PXHI)
		printf("\nConnecting via AISwitch, command '%s'",pxaicstr);
	pi_puts(pxaicstr);
	return(PGE_NOE);
#endif
	}

#ifdef ANSI
static void pi_puts(char *str)
#else
static void pi_puts(str)
char *str;
#endif
	{
	char c,ec;

	while (*str)
		{
		c = *str++;
		pi_put(c);
		while(pi_get(&c));
		}
	pi_put(13);
	while (pi_get(&c));
	}
#endif
