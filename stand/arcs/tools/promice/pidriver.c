/*	PiDriver.c - Edit 6

	LoadICE Version 3
	Copyright (C) 1990-95 Grammar Engine, Inc.
	All rights reserved
	
	NOTICE:  This software source is a licensed copy of Grammar Engine's
	property.  It is supplied to you as part of support and maintenance
	of some Grammar Engine products that you may have purchased.  Use of
	this software is strictly limited to use with such products.  Any
	other use constitutes a violation of this license to you.
*/

/*	 - PROMICE driver module -
	 - Provides open, close, read, write, cmd, and resp services
	
*/
#include <stdio.h>
#include <fcntl.h>

#include "piconfig.h"
#include "pistruct.h"
#include "pierror.h"
#include "pidriver.h"

#ifdef MSDOS
#include "pidev.h"
#include <dos.h>
#include <sys/stat.h>
#include <io.h>
#endif

#ifdef ANSI
long pi_raw(void);
void pi_toggle(void);
void pi_cook(void);
long pi_switch(void);
void pi_sleep(short time);
void pi_setime(long time);
long pi_xmt(char *buf, long ct);
long pi_haxmt(char *data, long count);
long pi_rcv(void);
void pi_put(char data);
void pi_putp(char data);
void pi_puts(char *str);
long pi_get(char *data);
long pi_getp(char *data);
void pi_putp(char data);
void pi_putp2(char data);
void pi_putpf(char data);
void pi_putp2f(char data);
void pi_ccw(void);
void pi_cccw(void);
void pi_kbd(void);
static void pi_dbuf(char *buf, long ct);

#else
long pi_raw();
void pi_toggle();
void pi_cook();
long pi_switch();
void pi_sleep();
long pi_xmt();
long pi_haxmt();
long pi_rcv();
void pi_put();
void pi_putp();
void pi_puts();
long pi_get();
long pi_getp();
void pi_putp();
void pi_putp2();
void pi_putpf();
void pi_putp2f();
void pi_ccw();
void pi_cccw();
void pi_kbd();
static void pi_dbuf();
#endif

short pptwo;	/* send data to 2nd unit on the parallel port  */
char xbuf[260];

/* `pi_cmd` - send a command to a PROMICE */

#ifdef ANSI
long pi_cmd(void)
#else
long pi_cmd()
#endif
	{
	if (pxdisp&PLOG)
		write(pclog,pxcmd,pxcmdl);
	if (pxdisp&(PXLO|PXLL))	/* diagnostic output */
		{
		printf("\nCMD:");
		pi_dbuf(pxcmd,pxcmdl);
		}
	pxerror = pi_xmt(pxcmd,pxcmdl);	/* send to PROMICEs */
	if (!pxerror)
		if (!(pxcmd[PICM] & CM_NORSP))	/* if response expected */
			return(pi_rsp());			/* get response */
	return(pxerror);
	}

/* `pi_rsp` - return response from some PROMICE */

#ifdef ANSI
long pi_rsp(void)
#else
long pi_rsp()
#endif
	{
	pxerror = pi_rcv();	/* read response from the PROMICEs */
	if (!pxerror)
		if (pxdisp&(PXLO|PXLL))	/* diagnostic output */
			{
			printf("\nRSP:");
			pi_dbuf(pxrsp,pxrspl);
			}
	if (pxdisp&PLOG)
		write(pclog,pxrsp,pxrspl);
	if (!pxerror && pxrsp[PICM]&CM_FAIL)
		pxerror = -pxrsp[PIDT];
	return(pxerror);
	}

/* `pi_write` - write a block to the PROMICEs */

#ifdef ANSI
long pi_write(void)
#else
long pi_write()
#endif
	{
	long tct,xct,rxct,tws;
	long i,ui,ck,hiv;
	char *xptr;
			
	pxdlc += pxdbc;	/* up the grand transfer count */
	if (pxdisp&(PXLO|PXLL))	/* diagnostic output */
		{
		printf("\nPI_WRITE:BigBuf/Size=%d",pxdbc);
		if (pxdisp&PXLL)
			{
			printf("\n    ");
			pi_dbuf(pxdbf,pxdbc);
			}
		}
	tws = pxacfg->words;
	if (pxflags & POVF)		/* verifying or not ? */
		xbuf[PICM] = PI_WR;
	else
		xbuf[PICM] = PI_WR | CM_NORSP;
	rxct = pxdbc%tws;		/* if transfer count not multiple of wordsize */
	for (ui=0,xct=0; ui<tws; ui++)
		{
		tct = pxdbc/tws;	/* per unit transfer count */
		if (rxct)			/* adjust for odd transfer count */
			tct++,rxct--;
		xptr = pxdbf+ui;	/* data for this unit starts here */
		xbuf[PIID] =(char)pxacfg->uid[(ui+pxsrom)%tws];	/* get unit ID */
		hiv = 0;
		if (pxrom[xbuf[PIID]].ver[0] >= '5')	/* verify supported? */
			if (!(piflags&PiFP))
				hiv = 1;
			
		while (tct && !pxerror)	/* while data to send and no error */
			{
			if (tct>PIC_MD)
				xct = PIC_MD;	/* max data in single command packet */
			else
				xct = tct;
			tct -= xct;

			/* fill xbuf - transfer buffer - with command and data */

			xbuf[PICT] = (char)xct;
			xct += PIDT;
#ifdef MSDOS
			if (piflags&PiFP && !(pxflags&POVF) && !(pxlink.flags&PLPB))
				{
				if (pxrom[xbuf[PIID]].mid == 0)
					{
					while (!(inp(pxlink.ppin)&BUSY));
					outp(pxlink.ppdat, xbuf[PIID]);
					outp(pxlink.ppout, STRON);
					outp(pxlink.ppout, STROFF);
					while (!(inp(pxlink.ppin)&BUSY));
					outp(pxlink.ppdat, xbuf[PICM]);
					outp(pxlink.ppout, STRON);
					outp(pxlink.ppout, STROFF);
					while (!(inp(pxlink.ppin)&BUSY));
					outp(pxlink.ppdat, xbuf[PICT]);
					outp(pxlink.ppout, STRON);
					outp(pxlink.ppout, STROFF);
					for (i=PIDT; i<xct; i++)
						{
						while (!(inp(pxlink.ppin)&BUSY));
						outp(pxlink.ppdat, *xptr);
						outp(pxlink.ppout, STRON);
						outp(pxlink.ppout, STROFF);
						xptr += tws;
						}
					}
				else
					{
					while (inp(pxlink.ppin)&PAPER);
					outp(pxlink.ppdat, xbuf[PIID]);
					outp(pxlink.ppout, AUTOON);
					outp(pxlink.ppout, AUTOOFF);
					while (inp(pxlink.ppin)&PAPER);
					outp(pxlink.ppdat, xbuf[PICM]);
					outp(pxlink.ppout, AUTOON);
					outp(pxlink.ppout, AUTOOFF);
					while (inp(pxlink.ppin)&PAPER);
					outp(pxlink.ppdat, xbuf[PICT]);
					outp(pxlink.ppout, AUTOON);
					outp(pxlink.ppout, AUTOOFF);
					for (i=PIDT; i<xct; i++)
						{
						while (inp(pxlink.ppin)&PAPER);
						outp(pxlink.ppdat, *xptr);
						outp(pxlink.ppout, AUTOON);
						outp(pxlink.ppout, AUTOOFF);
						xptr += tws;
						}
					}
				pi_ccw();
				continue;
				}
#endif
			for (i=PIDT,ck=0; i<xct; i++)
				{
				xbuf[i] = *xptr;
				if (pxflags & POVF)	/* compute checksum if verifying */
					ck ^= *xptr;
				xptr += tws;
				}
			if (pxdisp&(PXLO|PXLL))	/* diagnostic output */
				{
				printf("\nCMD:");
				pi_dbuf(xbuf,xct);
				}
			if (pxdisp&PLOG)
				write(pclog,xbuf,xct);
			if (piflags&PiMU && pxflags&PORQ)
				{
				if (pxrom[0].ver[0] == '8' && pxrom[0].ver[2] == '0')
					pxerror = PGE_UNF;
				else
					pxerror = pi_haxmt(xbuf,xct);
				}
			else
				{
				pxerror = pi_xmt(xbuf,xct);	/* send to PROMICE */
				if (!pxerror && (pxflags & POVF))	/* if verifying */
					{
					pxerror = pi_rsp();
					if (!pxerror && hiv)
						{
						if (pxdisp&PXLL)
							printf(" (%02X)",ck&0xff);
						if (pxrsp[PIDT] != (char)ck)
							pxerror = PGE_VFE;		/* bad news */
						}
					}
				}
			if (pxdisp&PXHI)
				pi_ccw();			/* spin cursor */
			pi_kbd();	/* check for keyboard activity */
			}
		if (pxerror)
			return(pxerror);
		}

	return(pxerror);
	}

/* `pi_haxmt` - do a write by using hold/holdack on PromICE PiCOM */
#ifdef ANSI
long pi_haxmt(char *data, long count)
#else
long pi_haxmt(data, count)
char *data;
long count;
#endif
	{
	long i,tlp;
	char tid;

	tid = *(data+PIID);
	tlp = pxrom[tid].cramp;
	for (i=3; i<count && !pxerror; i++,tlp++)
		{
		picmd(tid,PI_LP|CM_NORSP,3,
			(char)(tlp>>16),(char)(tlp>>8),(char)tlp,0,0);
		picmd(tid,PI_MB,1,*(data+i),0,0,0,0);
		}
	pxrom[tid].cramp = tlp;
	return(pxerror);
	}

/* `pi_read` - read a block from the PROMICEs */

#ifdef ANSI
long pi_read(void)
#else
long pi_read()
#endif
	{
	long tct,xct,rxct,tws;
	long i,ui,tid,tlp;
	char *xptr;

	pxdlc += pxdbc;
	tws = pxacfg->words;
	rxct = pxdbc%tws;
	pxcmd[PICM] = PI_RD;
	pxcmd[PICT] = 1;
	pxcmdl = PIMI;
	for (ui=0,xct=0; ui<tws; ui++) /* once for each ID in word */
		{
		tct = pxdbc/tws; /* adjust transfer count for multi-byte words */
		if (rxct)
			tct++,rxct--;
		xptr = pxdbf+ui;
		pxcmd[PIID] = (char)pxacfg->uid[(ui+pxsrom)%tws];
		while (tct && !pxerror)	/* do transfer and merge */
			{
			if (piflags&PiMU && pxflags&PORQ)
				{
				if (pxrom[0].ver[0] == '8' && pxrom[0].ver[2] == '0')
					pxerror = PGE_UNF;
				else
					{
					tid = pxcmd[PIID];
					tlp = pxrom[tid].cramp;
					for (i=0; i<tct && !pxerror; i++,tlp++)
						{
						picmd(tid,PI_LP|CM_NORSP,3,
							(char)(tlp>>16),(char)(tlp>>8),(char)tlp,0,0);
						picmd(tid,PI_MB|CM_MBREAD,1,0,0,0,0,0);
						*xptr = pxrsp[PIDT];
						xptr += tws;
						if (!(i%256))
							{
							if (pxdisp&PXHI)
								pi_cccw();
							pi_kbd();
							}
						}
					pxrom[tid].cramp = tlp;
					tct = 0;
					}
				}
			else
				{
				if (tct>PIC_MD)
					xct = PIC_MD;
				else
					xct = tct;
				pxcmd[PIDT] = (char)xct;
				pxerror = pi_cmd();
				if (!pxerror)
					{
					tct -= xct;
					for (i=PIDT; i<xct+PIDT; i++)
						{
						*xptr = pxrsp[i];
						xptr += tws;
						}
					}
				}
			if (pxdisp&PXHI)
				pi_cccw();		/* spin cursor */
			pi_kbd();		/* check for user activity */
			}
		if (pxerror)
			return(pxerror);
		} /* done reading requested data from all units */
	if (pxdisp&(PXLO|PXLL))
		printf("\nPI_READ:BigBuf/Size=%d",pxdbc);
	if (pxdisp&PXLL)
		{
		pi_dbuf(pxdbf,pxdbc);
		}
	return(pxerror);
	}

/* `pi_open` - open device and establish link with PROMICEs */

#ifdef ANSI
long pi_open(void)
#else
long pi_open()
#endif
	{
	long i;
	long j;
	char tc,td;
	long giveup = pxctry;
		
	pxerror = pi_raw();				/* set port in RAW mode (open device) */
	if (!pxerror && piflags&PiSW)
		pxerror = pi_switch();		/* if talking through AiSwitch */
	if (pxerror)
		return(pxerror);
	pxprom = 0;
	pptwo = 0;
	i = 0;
	while (!pxprom)	/* till we connect */
		{
		pi_toggle();
		pi_setime(PIC_TOT);

#ifdef	UNIX
		if (!(pxlink.flags&PLPQ))
			for (j=0; j<pxnpiu*3; j++)
				{
				pi_put(PI_BR);
				pi_sleep(1);
				}
#endif
		/* send auto-baud character till we get one back */
		do
			{
			tc = 0;
			pi_put(PI_BR);		/* send auto-baud character */
			pi_sleep(1);
			while (!pxtout && pi_get(&tc) && i<(PIC_TRY*pxnpiu))
				{
				i++;
				pi_put(PI_BR);
				pi_sleep(1);
				}
#ifdef	MSDOS
			kbhit();
#endif
			printf(".");
			if ((tc != PI_BR) || (i == (PIC_TRY*pxnpiu)))
				pi_toggle();	/* try reseting the unit again */
			i = 0;
			} while ((tc != PI_BR) && !pxtout);
		if (pxtout)	/* kept timing out */
			{
			if (giveup>=0)
				{
				if (giveup)
					giveup--;
				else
					return(PGE_TMO);	
				}
			continue;
			}
		
		pi_setime(PIC_TOT);	/* reset timout */

		/* send '00 00' */
		pi_put(PI_ID);	/* first '00' */
		while (pi_get(&tc) && !pxtout);	/* should come back to us */
		if (pxtout)
			continue;
		pi_setime(PIC_TOT);
		if (tc == PI_BR)
			{
			while ((tc == PI_BR) && !pxtout)	/* ignore auto-baud char */
				{
				while (pi_get(&tc));
				}
			if (pxtout)
				continue;
			}
		if (tc != PI_ID)
			continue;
		pi_setime(PIC_TOT);	/* reset timeout */
		pi_put(PI_ID);		/* second '00' - the ID */
		while(pi_get(&td) && !pxtout);
		if (pxtout)
			continue;
		if (td == PI_ID)	/* can't get '00' now */
			continue;
		pxprom = td;		/* number of physical modules found */
		}
	if (tc != PI_ID)
		return(PGE_COM);
	return(PGE_NOE);
	}

/* `pi_close` - close the device to PROMICE */

#ifdef ANSI
long pi_close(void)
#else
long pi_close()
#endif
	{
	pi_cook();
	if (pclog>=0)
		close(pclog);
	return(PGE_NOE);
	}

/* `pi_kbd` - process user keyboard activity */

#ifdef MSDOS
void pi_kbd()
	{
	char c;

	if (!kbhit())	/* no keys hit */
		return;
	c = (char)getch();	/* else get key */
	if (c == 'l')
		{
		if (pxdisp&PLOG)
			pxdisp &= ~PLOG;
		else
			pxdisp |= PLOG;
		return;
		}
	if (c == ' ')		/* space = pause */
		{
		c = (char)getch();	/* pause till another space typed */
		if (c == ' ')
			return;
		}
	pxerror = PGE_NOP;		/* any other key - terminate operation */
	}
#else
void pi_kbd()
	{
	}
#endif
	
/* `pi_dbuf` - display buffer contents to user */

#ifdef ANSI
static void pi_dbuf(char *buf, long ct)
#else
static void pi_dbuf(buf, ct)
char *buf;
long ct;
#endif
	{
	long i;
	
	for (i=0; i<ct;)
		{
		printf(" %02X",(*buf++)&0xff);
		if (!((++i)%16))
			{
			if (pxdisp&PXLL)
				printf("\n    ");
			else
				{
				printf("...");
				break;
				}
			pi_kbd();
			if (pxerror)
				break;
			}
		}
	}
