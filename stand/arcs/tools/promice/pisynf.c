/*	PiSynF.c - Edit 6

	LoadICE Version 3
	Copyright (C) 1990-95 Grammar Engine, Inc.
	All rights reserved
	
	NOTICE:  This software source is a licensed copy of Grammar Engine's
	property.  It is supplied to you as part of support and maintenance
	of some Grammar Engine products that you may have purchased.  Use of
	this software is strictly limited to use with such products.  Any
	other use constitutes a violation of this license to you.
*/

/*	 - PROMICE/functions called from the parser
	global variables are initialized by the parser
*/

#include <stdio.h>
#include <fcntl.h>
#include <string.h>

#ifdef MSDOS
#include <process.h>
#include <conio.h>
#include <sys/stat.h>
#include <io.h>
#endif

#include "piconfig.h"
#include "piscript.h"
#include "pistruct.h"
#include "pierror.h"
#include "pidriver.h"
#include "pidata.h"
#include "pihelp.h"
#include "pisyn.h"

#ifdef ANSI
extern void (*psynf[])(void);
extern void piconfig(short cd);
extern void picline(void);
extern void pidohlp(void);
extern void pifixcfg(void);
extern void piinitf(void);
extern void pcdump(void);
extern void pcedit(void);
extern void pcfill(void);
extern void pcxfill(void);
extern void pcksum(void);
extern void pcxksum(void);
extern void pcinit(void);
extern void pcmove(void);
extern void pcsave(void);
extern void pcfind(void);
extern	void pcstring(void);
extern void pcrstt(short id, short time);
extern void pilod(void);
extern void piemu(void);
extern void pi_put(char data);
extern long pi_get(char *dp);
extern void pi_sleep(short time);
static void geitest(void);
static void filler(short code);
static PICONFIG *pigetcfg(void);
static void piload(void);
static void pifname(void);
static void pifmore(PIFILE *cf);

#else
extern void (*psynf[])();
extern void piconfig();
extern void picline();
extern void pidohlp();
extern void pifixcfg();
extern void piinitf();
extern void pcdump();
extern void pcedit();
extern void pcfill();
extern void pcxfill();
extern void pcksum();
extern void pcxksum();
extern void pcinit();
extern void pcmove();
extern void pcsave();
extern void pcfind();
extern void pcstring();
extern void pcrstt();
extern void pilod();
extern void piemu();
extern long pi_get();
extern void pi_put();
extern void pi_sleep();

static void geitest();
static void filler();
static PICONFIG *pigetcfg();
static void piload();
static void pifname();
static void pifmore();
#endif

short pftype = PFHEX;
static PICONFIG *pftcfg;
extern char *liver;

/* `pcload` - down load user data files */

void pcload()
	{
	if ((!(pxflags&POCP)) && (piflags&PiFM)) /* if not comparing and do fill */
		pcfill();
	if (pxerror)
		return;
	for (pxcfile=0,pxdlc=0; pxcfile<pxnfile; pxcfile++)	/* load all files */
		{
		pifile();
		if (pxerror)
			break;
		}
	if (!pxerror)
		{
		pxcfg = pxpcfg;
		if ((!(pxflags&POCP)) && (piflags&PiEL))
			pcedit();		/* if not comparing and have an edit list */
		if (pxdisp&PXHI)
			{
			if (pxflags&POCP)
				printf("\nVerified");
			else
				printf("\nTransferred");
			printf(" %ld (0x%lx) data bytes",pxdlc,pxdlc);
			}
		if (!(pxflags&POCP) && piflags&PiCK)
			if (pxflags&POKR)
				pcxksum();
			else
				if (pxflags&POKC)
					pcksum();
		}
	}

/* `piailoc` - 1 - set ailoc address */

static void piailoc()
	{
	short i,id=0;

	if (pxdisp&PXMH)
		printf(" >PiAiloc()");
	if (ps_id>=0)
		id = ps_id;
	if (ps_i<2)
		{
		pxerror = PGE_BAA;
		return;
		}
	pxailoc = ps_num[0];
	pxaiid = id;
	piflags |= PiAI;		/* so we know */
	pxaibr = ps_num[1];
	pxaiws = 0;
	pxaibrc = 0;
	switch(ps_num[1])
		{
		case 0:
			piflags |= PiXP;
			pxaibrc = 0xff;
			break;
		default:
			for (i=0; pxbauds[i]; i+=2)	/* look for baud rate */
				{
				if (pxaibr == pxbauds[i])
					{
					pxaibrc = pxbauds[i+1];
					break;
					}
				}	
			if (!pxaibrc)			/* did not find it */
				pxerror = PGE_BRT;
			piflags &= ~PiXP;
			break;
		}
	ps_i -= 2;
	if (ps_i)		/* if more arguments then its a break character */
		{
		ps_i--;
		if (ps_num[2] >= 0)
			{
			piflags |= PiBC;
			pxaibchr = (char)ps_num[2];
			}
		}
	else
		{
		piflags &= ~PiBC;
		pxaibchr = 0;
		}
	if (ps_i)		/* yet more is number of reset intterrupt to ignore */
		pxhints = (short)ps_num[3];
	else
		pxhints = 0xff;
	}

/* `pibrate` - 2 - set baud rate */

static void pibrate()
	{
	short i;
	
	if (pxdisp&PXMH)
		printf(" >PiBrate()");
	pxlink.brate = 0;
	for (i=0; pxbauds[i]; i+=2)
		{
		if (ps_num[0] == pxbauds[i])
			{
			pxlink.brate = ps_num[0];
			break;
			}
		}
	if (!pxlink.brate)
		pxerror = PGE_BRT;
	}

/* `picompare` - 3 - compare PROMICE data */

static void picompare()
	{
	if (pxdisp&PXMH)
		printf(" >PiCompare()");
	if (piflags&PiMU && !(pxflags&PORQ))
		pxerror = PGE_EMU;
	else
		{
		pxflags |= POCP;		/* set the compare flag and do load */
		piload();
		pxflags &= ~POCP;		/* clear the flag now */
		}
	}

/* `pidump` - 4 - dump PROMICE contents */

static void pidump()
	{
	long tmp;

	if (pxdisp&PXMH)
		printf(" >PiDump()");
	
	if (piflags&PiCR)			/* if just CR typed then dump next range */
		{
		if (!(piflags&PiDF))
			{
			tmp = pxdend - pxdstart;
			pxdstart = pxdend + 1;
			pxdend = pxdstart + tmp;
			if (pxdend > pxmax)
				pxdend = pxmax;
			}
		}
	else	/* depending on the number the arguments vary */
		{
		if (ps_li--)
			{
			pxdstart = ps_numl[0];
			if (ps_li)
				pxdend = ps_numl[1];
			else
				{
				pxdend = pxdstart + PIC_DS;
				if (pxdend > pxmax)
					pxdend = pxmax;
				}
			}
		else
			{
			pxdstart = 0;
			pxdend = PIC_DS;
			}
		}
	if (piflags&PiMU && !(pxflags&PORQ))		/* hey! we are emulating */
		{
		pxerror = PGE_EMU;
		piflags |= PiDF;
		}
	else
		{
		if (pigetcfg())		/* set up the config to dump from */
			{
			pcdump();		/* dump - in PICORE */
			pxcfg = pftcfg;	/* restore config */
			pifixcfg();
			}
		}
	}

/* `piedit` - 5 - edit PROMICE or IMAGE contents */

static void piedit()
	{
	short i;

	if (ps_bchrc != ' ' && ps_bchrc != '\0')
		{
		pxerror = PGE_CMD;
		return;	
		}
	if (pxdisp&PXMH)
		printf(" >PiEdit()");
	if (ps_i)					/* if data already given */
		{
		pxestart = ps_num[0];
		for (i=0; i<ps_li; i++)
			pxelist[i] = ps_numl[i];
		pxelcnt = ps_li;
		}
	else
		{
		pxestart = 0;
		pxelcnt = 0;
		}
	if (piflags&(PiiX|PiII))	/* if in dialog mode or '.' in 'ini' */
		{
		if (piflags&PiMU && !(pxflags&PORQ))		/* emulating? */
			pxerror = PGE_EMU;
		else
			{
			if (pigetcfg())
				pcedit();		/* go to editor */
			pxcfg = pftcfg;
			pifixcfg();
			}
		}
	else
		piflags |= PiEL;		/* not interactive - edit after load */
	}

/* `piundef` - 6  - mt slot */

/* `pifill` - 7  - fill ROM space */

static void pifill()
	{
	PIROM *rp;

	if (pxdisp&PXMH)
		printf(" >PiFill()");

	if (ps_id >= 0)				/* if ID given */
		{
		if (ps_id >= PIC_NR)
			{
			pxerror = PGE_BIG;
			return;
			}
		rp = &pxrom[ps_id];
		switch (ps_li)			/* number or args determine their type */
			{
			case 0:
				break;
			case 1:
				rp->fdata = ps_numl[0];
				break;
			case 2:
				rp->fstart = ps_numl[0];
				rp->fend = ps_numl[1];
				break;
			case 3:
				rp->fstart = ps_numl[0];
				rp->fend = ps_numl[1];
				rp->fdata = ps_numl[2];
				break;
			case 4:
				rp->fstart = ps_numl[0];
				rp->fend = ps_numl[1];
				rp->fdata = ps_numl[2];
				rp->fsize = ps_numl[3];
				break;
			case 5:
				rp->fstart = ps_numl[0];
				rp->fend = ps_numl[1];
				rp->fdata = ps_numl[2];
				rp->fdata2 = ps_numl[3];
				rp->fsize = ps_numl[4];
				break;
			default:
				pxerror = PGE_NUM;
				break;
			}
		rp->flags |= PRFL;
		pxflags |= POFR;
		pxflags &= ~POFC;
		}
	else
		{
		if (pigetcfg())	/* setup config - sets pxmax */
			{
			pxfstart = pxcfg->start;
			pxfend = pxmax;
			pxfdata = 0xffffffff;
			pxfsize = pxcfg->words;
			pxcfg->flags |= PCFL;
			}
		else
			{
			pxerror = PGE_BAF;
			return;
			}
		switch (ps_li)			/* number or args determine their type */
			{
			case 0:
				pxfdata = pxrom[0].fdata;
				break;
			case 1:
				pxfdata = ps_numl[0];
				break;
			case 2:
				pxfstart = ps_numl[0];
				pxfend = ps_numl[1];
				break;
			case 3:
				pxfstart = ps_numl[0];
				pxfend = ps_numl[1];
				pxfdata = ps_numl[2];
				break;
			case 4:
				pxfstart = ps_numl[0];
				pxfend = ps_numl[1];
				pxfdata = ps_numl[2];
				pxfsize = ps_numl[3];
				break;
			case 5:
				pxfstart = ps_numl[0];
				pxfend = ps_numl[1];
				pxfdata = ps_numl[2];
				pxfdata2 = ps_numl[3];
				pxfsize = ps_numl[4];
				break;

			default:
				pxerror = PGE_NUM;
				break;
			}
		pxflags |= POFC;
		pxflags &= ~POFR;
		}
	if (!pxerror && piflags&(PiII|PiiX))
		{
		if (pigetcfg())
			{
			if (piflags&PiMU && !(pxflags&PORQ))		/* if not emulating */
				pxerror = PGE_EMU;
			else
				{
				pcfill();
				}
			pxcfg = pftcfg;	/* restore config */
			pifixcfg();
			}
		}
	else
		piflags |= PiFM;
	}

/* `pihighx` - 8  - high speed response */

static void pihigh()
	{
	if (pxdisp&PXMH)
		printf(" >PiHigh()");
	pxlink.flags |= PLHI;
	pxlink.flags &= ~PLOW;
	if (piflags&(PiII|PiiX))
		{
		if (pxrom[0].ver[0] < '7')
			pxmode1 |= MO_FAST;
		else
			pxmode1 &= ~MO_SLOW;
		}
	piflags |= PiMO;
	}

/* `pihso` - 9 - set HSO polarity */

static void pihso()
	{
	if (pxdisp&PXMH)
		printf(" >PiHso()");
	piflags |= PiHO;
	if (ps_i--)
		{
		pxhso = ps_num[0];
		if (ps_i--)
			{
			piflags |= PiRQ;
			pxreq = ps_num[1];
			if (ps_i)
				pxack = ps_num[2];
			}
		}
	}

/* `pisearch` - 10 - find something */

static void pisearch()
	{
	long i,j;

	if (pxdisp&PXMH)
		printf(" >PiSearch()");
	
	if (piflags&PiMU)
		{
		pxerror = PGE_EMU;
		return;
		}
	if (pigetcfg())
		{
		pxlstart = pxcfg->start;
		pxlend = pxmax;

		if (ps_str)			/* searching for a string */
			{
			if (ps_i--)
				pxlstart = ps_num[0];
			if (ps_i)
				pxlend = ps_num[1];
			piflags |= PiSS;
			pxlstr = ps_str;
			if (pxlend < pxlstart)
				pxerror = PGE_BAA;
			else
				pcstring();
			}
		else				/* else binary search */
			{
			piflags &= ~PiSS;
			if (ps_i<3)
				pxerror = PGE_BAA;
			else
				{
				pxlstart = ps_num[0];
				pxlend = ps_num[1];
				pxlsize = ps_num[2];
				if ((pxlend < pxlstart) || (ps_li < pxlsize)
										|| (pxlsize > PIC_SS))
					pxerror = PGE_BAA;
				else
					{
					for (i=0; i<pxlsize; i++)
						pxldata[i] = (char)ps_numl[i];
					pcfind();
					}
				}
			}
		}
	pxcfg = pftcfg;
	pifixcfg();
	}

/* `pichksum` - 11 - compute checksum */

static void pichksum()
	{
	short i;

	if (pxdisp&PXMH)
		printf(" >PiChksum()");
	if (ps_i<3)
		{
		pxerror = PGE_BAA;
		return;
		}
	else
		if (ps_i>3)
			if(ps_num[3]<8 || ps_num[3]>32 || ps_num[3]%8)
				{
				pxerror = PGE_BAA;
				return;	
				}
	if (ps_id>=0)			/* one ROM */
		{
		if (ps_id >= PIC_NR)
			{
			pxerror = PGE_BIG;
			return;
			}
		pxrom[ps_id].kstart = ps_num[0];
		pxrom[ps_id].kend = ps_num[1];
		pxrom[ps_id].kstore = ps_num[2];
		if (ps_i>3)
			pxrom[ps_id].ksize = ps_num[3]/8;
		else
			pxrom[ps_id].ksize = 1;
		pxrom[ps_id].flags |= PRCK;
		switch (ps_char)
			{
			case 'a':
				pxrom[ps_id].flags |= PRKA;
				pxrom[ps_id].flags &= ~PRK1;
				break;
			case 'A':
				pxrom[ps_id].flags |= PRKA|PRK1;
				break;
			case 'X':
				pxrom[ps_id].flags &= ~PRKA;
				pxrom[ps_id].flags |= PRK1;
				break;
			default:
				pxrom[ps_id].flags &= ~(PRKA|PRK1);
				break;
			}
		if (ps_i == 5)
			{
			if (ps_num[4])
				pxrom[ps_id].flags |= PRKO;
			else
				pxrom[ps_id].flags &= ~PRKO;
			}
		else
			pxrom[ps_id].flags &= ~PRKO;
		pxflags |= POKR;	/* checksum ROMs later */
		pxflags &= ~POKC;	/* no checksumming config */
		}
	else
		{
		pxkstart = ps_num[0];
		pxkend = ps_num[1];
		pxkstore = ps_num[2];
		if (ps_i>3)
			pxksize = ps_num[3]/8;
		else
			pxksize = 1;
		switch (ps_char)
			{
			case 'a':
				pxflags |= POKA;
				pxflags &= ~POK1;
				break;
			case 'A':
				pxflags |= POKA|POK1;
				break;
			case 'X':
				pxflags &= ~POKA;
				pxflags |= POK1;
				break;
			default:
				pxflags &= ~(POKA|POK1);
				break;
			}
		if (ps_i == 5)
			{
			if (ps_num[4])
				pxflags |= POKO;
			else
				pxflags &= ~POKO;
			}
		else
			pxflags &= ~POKO;
		pxflags |= POKC;	/* checksum config later */
		pxflags &= ~POKR;	/* no cheksum indvidual ROMs */
		}
	if (piflags&(PiiX|PiII))	/* right away */
		{
		if (pxflags&POKR)
			pcxksum();
		else
			{
			if (pigetcfg())
				{
				if (piflags&PiMU && !(pxflags&PORQ))
					pxerror = PGE_EMU;
				else
					pcksum();
				pxcfg = pftcfg;
				pifixcfg();
				}
			}
		}
	else
		piflags |= PiCK;
	}


/* `piload` - 12  - load PROMICE */

static void piload()
	{
	if (pxdisp&PXMH)
		printf(" >PiLoad()");
	if (ps_str)					/* file name is given with command */
		{
		piflags |= PiII;
		pifname();				/* do file name processing */
		piflags &= ~PiII;
		}
	if (!pxerror)
		{
		if (piflags&(PiiX|PiII))
			{
			if (!pxprom)		/* if link is not up */
				{
				pcinit();		/* initilize the link */
				pilod();		/* put units in load mode */
				}
			if (!pxerror)
				{
				if (piflags&PiII)	/* from 'ini' file '.' command */
					pcload();
				else				/* from dialog mode */
					{
					if (piflags&PiMU && !(pxflags&PORQ))
						pxerror = PGE_EMU;
					else
						pcload();
					}
				}
			}
		else
			{
			pxflags |= POLO;		/* else load later */
			}
		}
	}

/* `pimove` - 13 - move image data around */

static void pimove()
	{
	if (pxdisp&PXMH)
		printf(" >PiMove()");

	if (piflags&PiMU && !(pxflags&PORQ))
		pxerror = PGE_EMU;
	else
		{
		if (ps_i > 2)
			{
			if (pigetcfg())
				{
				pxmstart = ps_num[0];
				pxmend = ps_num[1];
				pxmdest = ps_num[2];
				pcmove();
				pxcfg = pftcfg;
				pifixcfg();
				}
			}
		else
			pxerror = PGE_BAA;
		}
	}

/* `pinumber` - 14  - set number of PROMICE units */

static void pinumber()
	{
	if (pxdisp&PXMH)
		printf(" >PiNumber()");
	if (ps_i)
		pxnpiu = (short)ps_num[0];
	}

/* `piouput` - 15  - set output serial port */

static void piouput()
	{
	int l;
	
	if (pxdisp&PXMH)
		printf(" >PiOuput()");
#ifdef MSDOS			/* for DOS its port# */
	ps_str = "COM0";
	if (ps_emb<=0)
		ps_emb = 1;
	*(ps_str+3) = (char)((char)ps_emb+'0');
#endif
	l = strlen(ps_str);	/* for others its device name */
	if (l < PIC_SN)
		strcpy(pxlink.name,ps_str);
	else
		pxerror = PGE_NTL;
	if (ps_i)			/* port address is also given */
		{
		pxlink.saddr = (int)ps_num[0];
		pxlink.flags |= PLSU;
		}
	pxlink.flags &= ~PLPQ;	/* definitely not pponly */
	if (piflags&PiNE)
		{
		piflags &= ~PiNE;
		pxlink.flags &= ~(PLPP|PLPB);
		}
	}

/* `pistdpp` - 16  - set standard parallel port */

static void pistdpp()
	{
	int l;
	
	if (pxdisp&PXMH)
		printf(" >PiStdpp()");
#ifndef UNIX
	ps_str = "LPT0";
	if (ps_emb<=0)
		ps_emb = 1;
	*(ps_str+3) = (char)((char)ps_emb+'0');
#endif
	l = strlen(ps_str);
	if (l < PIC_SN)
		strcpy(pxlink.pname,ps_str);
	else
		pxerror = PGE_NTL;
	if (ps_i)				/* port address given */
		{
		pxlink.paddr = (int)ps_num[0];
		pxlink.flags |= PLPU;
		}
	pxlink.flags |= PLPP;
	}

/* `pibidipp` - 17  - set bidirectional parallel port */

static void pibidipp()
	{
	int l;
	
	if (pxdisp&PXMH)
		printf(" >PiBidipp()");
#ifndef UNIX
	ps_str = "LPT0";
	if (ps_emb<=0)
		ps_emb = 1;
	*(ps_str+3) = (char)((char)ps_emb+'0');
#endif
	l = strlen(ps_str);
	if (l < PIC_SN)
		strcpy(pxlink.pname,ps_str);
	else
		pxerror = PGE_NTL;
	if (ps_i)				/* port address */
		{
		pxlink.paddr = (int)ps_num[0];
		pxlink.flags |= PLPU;
		}
	pxlink.flags |= PLPQ;
	}

/* `piromsize` - 18  - set emulation ROM size */

static void piromsize()
	{
	PICONFIG *tcfg;
	register short i;

	if (pxdisp&PXMH)
		printf(" >PiRomsize()");
	
	for (i=0; pxromx[i]; i+=2)		/* look for a valid size */
		{
		if (ps_emb == pxromx[i])
			{
			ps_emb = pxromx[i+1];
			break;
			}
		}
	if (!pxromx[i])					/* none found */
		pxerror = PGE_ROM;
	else
		{
		if (ps_id >= 0)				/* only for one unit */
			{
			pxrom[ps_id].esize = ps_emb;
			if (!(piflags&PiSK))
				pxrom[ps_id].ssize = ps_emb;
			}
		else
			{
			for (i=0; i<PIC_NR; i++)	/* for all units */
				{
				pxrom[i].esize = ps_emb;
				if (!(piflags&PiSK))
					pxrom[i].ssize = ps_emb;
				}
			}
		}
	if (pxcfg)		/* if a current config */
		pifixcfg();	/* fix up config parameters */
	tcfg = pxcfg;
	for (i=0; i<(short)pxnfile; i++)	/* also if any file configs */
		{
		pxcfg = pxfile[i].pfcfg;
		if (pxcfg)
			pifixcfg();
		}
	pxcfg = tcfg;
	}

/* `pistop` - 19 - stop emulation */

static void pistop()
	{
	static short once=0;

	if (pxdisp&PXMH)
		printf(" >PiStop()");
	pilod();
	if (pxdisp&PXHI)
		printf("\nNow in Load Mode!");
	if (!once)			/* tell user first time only */
		{
		once = 1;
		printf("\nUse 'go' later to emulate");
		}
	}

/* `pitest` - 20  - test unit */

static void pitest()
	{
	short id=0,ct=1;
	long ad,i;

	if (pxdisp&PXMH)
		printf(" >PiTest()");
	
	if (piflags&PiMU)	/* not if emulating */
		{
		pxerror = PGE_EMU;
		return;
		}
	if (ps_id >= 0)		/* unit ID given */
		{
		id = ps_id;
		if (ps_i)		/* pass count also */
			ct = (short)ps_num[0];
		}
	else
		{
		if (ps_i--)		/* no : but a number is an ID */
			{
			id = (short)ps_num[0];
			if (id >= pxnrom)
				{
				ct = id;
				id = 0;
				}
			if (ps_i)	/* and may be a passcout too */
				ct = (short)ps_num[1];
			}
		}
	if (id >= pxnrom)
		{
		pxerror = PGE_BIG;
		return;
		}
	if (ct > 1)
		{
		geitest();
		printf("\n Test complete");
		}
	else
		{
		piflags |= PiNT;		/* don't timeout */
		picmd((char)id,PI_TS,1,(char)ct,0,0,0,0);
		piflags &= ~PiNT;
		if (pxrsp[PICT] != 1)
			{
			ad = ~pxrom[id].amask & 
				(((long)(pxrsp[PIDT]&0xFF)<<16)|((long)(pxrsp[PIDT+1]&0xFF)<<8)
												|((long)(pxrsp[PIDT+2]&0xFF)));
			printf("\n Unit %d failed test @loc %lX",id,ad);
			}
		else
			printf("\n Unit %d passes test",id);
		}
	}

/* `geitest` - test the PROMICE via GEItester */

static void geitest()
	{
	pilod();
	if (!pxerror)
		{
		printf("\nDown loading pattern:");
		filler(0);
		}
	if (pxerror)
		return;

	if (!pxerror)
		{
		printf("\nUploading and checking data:");
		filler(1);
		}
	}

#ifdef ANSI
static void filler(short code)
#else
static void filler(code)
short code;
#endif
	{
	unsigned long spat = 0x51237D1F;
	unsigned long pinc = 0xBAD1FEED;
	long tct = pxmax + 1;
	unsigned long tvalue;
	long *pats;
	long i;

	if (code)
		{
		pxyloc = 0;
		while (tct && !pxerror)
			{
			pxybc = PIC_BS;
			piread();
			if (pxerror)
				break;
			pats = (long *)&pxybf[0];
			for (i=0; i<PIC_BS; i+=4)
				{
				tvalue = *pats++;
				if (tvalue != spat)
					{
					printf("\nError @ %lX Got-%lX Want-%lX SUM-%lX",
						pxyloc+i,tvalue,~spat,tvalue+~spat);
					}
				spat += pinc;
				}
			pxyloc += PIC_BS;
			tct -= PIC_BS;
			pi_kbd();
			}
		return;
		}

	pxxloc = 0;
	while(tct && !pxerror)
		{
		pats = (long *)&pxxbf[0];
		for (i=0; i<PIC_BS; i+=4)
			{
			*pats++ = spat;
			spat += pinc;
			}
		pxxbc = PIC_BS;
		piwrite();
		pxxloc += PIC_BS;
		tct -= PIC_BS;
		}
	}

/* `piserial` - 21 - return serial# */

static void piserial()
	{
	short id;

	if (pxdisp&PXMH)
		printf(" >PiSerial()");
	if (ps_id >= 0)
		id = ps_id;
	else
		{
		if (ps_i)
			id = (short)ps_num[0];
		else
			id = 0;
		}
	if (id >= pxprom)
		pxerror = PGE_BIG;
	else					/* modified version command */
		{
		picmd((char)id,PI_VS| CM_SERN,1,0,0,0,0,0);		
		printf("\nserial#=%02X%02X%02X%02X",
		 pxrsp[PIDT]&0x0FF,pxrsp[PIDT+1]&0x0FF,
		 pxrsp[PIDT+2]&0x0FF,pxrsp[PIDT+3]&0x0FF);
		}
	}

/* `piverify` - 22 - set verify option */

static void piverify()
	{
	short f=0;

	if (pxdisp&PXMH)
		printf(" >PiVerify()");
	if (ps_i)
		f = (short)ps_num[0];
	if (f)
		pxflags |= POVF;
	else
		pxflags &= ~POVF;
	}

/* `piword` - 23 - set word size */

static void piword()
	{
	PICONFIG *cx,*cy;
	short u,w,b,n;

	if (pxdisp&PXMH)
		printf(" >PiWord()");
	if (!ps_i)			/* if no args then 8-bit */
		ps_num[0] = 8;
	if (!(ps_num[0]%8))	/* better be multiple of 8 */
		{
		w = (short)ps_num[0]/8;	/* word size in bytes */
		if (ps_li)				/* if ID list given */
			{
			if (ps_li%w)		/* better be right number of units */
				pxerror = PGE_IDL;
			}
		else					/* no ID list given */
			{
			n = pxnrom;
			if ((!n) || (!pxprom))	/* link is not up so assume unit count */
				{
				n = w;
				pxnrom = w;
				piflags |= PiNL;
				}
			else
				{
				if (n<w)			/* else enough ROMs to do the word? */
					{
					pxerror = PGE_BCF;
					return;
					}
				}
			for (b=0; b<(n/w); b++) /* no ID list so make it up */
				{
				for (u=w*b; u<(w*(b+1)); u++)
					ps_numl[u] = u;
				}
			ps_li = n;
			}
		if (piflags&(PiII|PiiX))
			{
			for (b=0; b<ps_li; b++)
				if (ps_numl[b] >= pxnrom)
					pxerror = PGE_BIG;
			}
		if (!pxerror)		/* if no errors then build config */
			{
			b = ps_li/w;	/* number of banks */
			cx = pxpcfg;
			cy = (PICONFIG *)0;
			if (cx)	/* free any previous configs */
				{
				while (cx->next)
					cx = cx->next;
				cx->next = pxfcfg;
				pxfcfg = pxpcfg;
				pxpcfg = (PICONFIG *)0;
				}
			for (u=0; u<b; u++)
				{
				cx = pxfcfg;	/* get a free config structure */
				if (!pxpcfg)	/* if no physical config set */
					{
					pxpcfg = cx;
					pxcfg = cx;
					}
				if (cx)			/* got one */
					{
					pxfcfg = cx->next;	/* remove from freeList */
					cx->next = (PICONFIG *)0;
					cx->words = w;		/* set word size */
					for (n=0; n<w; n++)	/* and ID list */
						cx->uid[n] = (short)ps_numl[w*u+n];
					if (cy)				/* if chaining banks */
						cy->next = cx;
					cy = cx;
					}
				else			/* no config structures left?!! */
					{
					pxerror = PGE_CFG;
					break;
					}
				}
			pifixcfg();	/* fix all config parameters */
			}
		}
	else
		pxerror = PGE_WDS;
	}

/* `pinocksm` - 24 - no-check checksum */

static void pinocksm()
	{
	if (pxdisp&PXMH)
		printf(" >PiNoCksm()");
	if (ps_i)
		{
		if (ps_num[0])
			pxflags |= PODK;
		else
			pxflags &= ~PODK;
		}
	pxflags |= PONK;
	}

/* `delay` - 25 - set delay time out */

static void pidelay()
	{
	if (pxdisp&PXMH)
		printf(" >PiDelay()");

	pxnotot = 0;
	if (ps_i)
		{
		if (ps_num[0])
			pxdelay = ps_num[0];
		else
			pxnotot = 1;
		}
	else
		{
		pxdelay = 1;
		}
	}

/* `pinoadder` - 26 - set to ignore address errors */

static void pinoaddr()
	{
	if (pxdisp&PXMH)
		printf(" >PiNoaddr()");
	if (ps_i)
		{
		if (ps_num[0])
			pxflags |= PODO;
		else
			pxflags &= ~PODO;
		}
	pxflags |= PONO;
	}

/* `piaisw` - 27 - LoadICE is talking through the AISwitch */

static void piaisw()
	{
	extern char *pxaicstr;

	if (pxdisp&PXMH)
		printf(" >PiAisw()");

	if ((!ps_i) || (ps_num[0] < 0))
		{
		pxerror = PGE_BAA;
		return;
		}
	if (ps_num[0] > PIC_NR)
		pxerror = PGE_BIG;
	else					/* store Switch port# in connect string */
		{
		*(pxaicstr+9) = (char)((char)(ps_num[0]/100)+'0');
		*(pxaicstr+10) = (char)((char)((char)((ps_num[0]%100)/10)+'0'));
		*(pxaicstr+11) = (char)((char)((char)((ps_num[0]%100)%10)+'0'));
		}
	piflags |= PiSW|PiPH;	/* remember the Switch */
	}

/* `piaitty` - 28 - operate transparent tty mode - does not work */

static void piaitty()
	{
	char c;

	if (pxdisp&PXMH)
		printf(" >PiAITTY()");

	if (piflags&PiAI)
		{
		picmd((char)pxaiid,PI_MO|CM_NORSP|CM_AITTY,3,1,0,(char)0xff,0,0);
		pxailoc |= pxrom[pxaiid].amask;
		picmd((char)pxaiid,PI_RS|CM_NORSP|CM_CINIT,5,(char)PIC_XCODE,
			(char)(pxailoc>>16),(char)(pxailoc>>8),(char)pxailoc,(char)pxaibr);
		}
	else
		{
		pxerror = PGE_IOE;
		return;
		}
	for (;;)
		{
#ifdef MSDOS
		if (kbhit())
			{
			c = (char)getch();
			pi_put(c);
			}
#endif
		if (pi_get(&c))
			continue;
		else
			printf("%c",c);
		}
	}
	
/* `pidiscfg` - 29 - display configuration */

static void pidiscfg()
	{
	short cd;
	
	if (pxdisp&PXMH)
		printf(" >PiDisCfg()");
	if (ps_str == NULL)
		ps_str = "a";
	switch (*ps_str)
		{
		case 'a':
			cd = PcALL;
			break;
		case 'l':
			cd = PcLNK;
			break;
		case 'f':
			cd = PcFLE;
			break;
		case 'r':
			cd = PcROM;
			break;
		case 'p':
			cd = PcPCF;
			break;
		case 'c':
			cd = PcCFG;
			break;
		default:
			printf("Unknown config type - doing all");
			cd = PcALL;
		}
	piconfig(cd);
	}

/* `pidisply` - 30 - change display level */

static void pidisply()
	{
	if (pxdisp&PXMH)
		printf(" >PiDisply()");
	if (ps_i)
		pxdisp = (short)ps_num[0];
	else
		pxdisp = 0xC0;
	}

/* `pisystem` - 31 - escape command to system */

static void pisystem()
	{
	if (pxdisp&PXMH)
		printf(" >PiSystem()");
#ifdef VMS
	pxerror = PGE_UNF;
#else
#ifndef MAC
	(void)system(ps_str);
#endif
#endif
	}

/* `pifname` - 32 - specifies file name - do whole buch of stuff */

static void pifname()
	{
	PICONFIG *cx;
	PIFILE *cf;
	int ref,l;
	char htype=0;
	
	if (pxdisp&PXMH)
		printf(" >PiFname()");
	if (pftype == PFHEX)			/* if hexfile then read one byte */
		if ((ref = open(ps_str, O_RDONLY)) >= 0)	/* does it exits? */
			{
			(void)read(ref,&htype,1);
			(void)close(ref);
			}
		else
			htype = 'h';			/* generic */
	if (piflags&PiII)		/* 'ini' '.' command */
		pxnfile = 0;
	pxcfile = pxnfile;
	piinitf();				/* clear out file structure */
	cf = &pxfile[pxnfile];	/* start filling in stuff */
	cf->type = pftype;
	cf->htype = htype;
	l = strlen(ps_str);
	if (l < PIC_FN)			/* if name not too long */
		{
		strcpy(cf->name,ps_str);
		if (pftype == PFBIN)
			{
			if (ps_i)
				{
				ps_i--;
				cf->skip = ps_num[0];
				}
			if (ps_i)
				cf->offset = ps_num[1];
			}
		else
			{
			if (ps_i)
				{
				ps_i--;
				cf->offset = ps_num[0];
				}
			if (ps_i)
				cf->offset = ps_num[1] - ps_num[0];
			}
		cx = cf->pfcfg;
		if (cx && (cx != pxpcfg))	/* free any previous file configs */
			{
			while (cx->next)
				cx = cx->next;
			cx->next = pxfcfg;
			pxfcfg = cf->pfcfg;
			}
		cf->pfcfg = (PICONFIG *)0;
		pifmore(cf);				/* split for stupid optimizer */
		}
	else
		pxerror = PGE_NTL;
	}
#ifdef ANSI
static void pifmore(PIFILE *cf)
#else
static void pifmore(cf)
PIFILE *cf;
#endif
	{
	PICONFIG *cx,*cy=0;
	short u,w,b,n;

	if (ps_li)	/* if we have a config */
		{
		ps_li--;
		if (ps_numl[0]%8)
			{
			pxerror = PGE_IDL;
			return;	
			}
		w = (short)ps_numl[0]/8;	/* word size */
		if (ps_li && !(ps_li%w) && (ps_li <= PIC_NR))
			{
			b = ps_li/w;	/* number of banks */
			if (b<=0)
				b = 1;
			for (u=0; u<b; u++)
				{
				cx = pxfcfg;
				if (!cf->pfcfg)
					cf->pfcfg = cx;
				if (cx)
					{
					pxfcfg = cx->next;
					cx->next = (PICONFIG *)0;
					cx->words = w;
					for (n=0; n<w; n++)
						if (ps_li)
							cx->uid[n] = (short)ps_numl[u*w+n+1];
						else
							cx->uid[n] = ps_id++;
					if (cy)
						cy->next = cx;
					cy = cx;
					}
				else
					{
					pxerror = PGE_CFG;
					break;
					}
				}
			}
		else
			pxerror = PGE_IDL;
		ps_li++;
		}
	else	/* no config given */
		{
		if (ps_id >= 0)
			{
			cx = pxpcfg;	/* check if existing one would work */
			while (cx)
				{
				if (cx->uid[0] == ps_id)
					{
					cf->pfcfg = cx;
					break;
					}
				cx = cx->next;
				}
			if (!cx)	/* if none found make a 8-bit config */
				{
				cx = pxfcfg;
				cf->pfcfg = cx;
				if (cx)
					{
					pxfcfg = cx->next;
					cx->next = 0;
					cx->words = 1;
					cx->uid[0] = ps_id;
					}
				}
			}
		}
		
	if (ps_sli && !pxerror)	/* partial transfer addresses */
		{
		if (!(ps_sli%2))
			{
			cf->saddr = ps_numl[ps_li++] + cf->offset;
			cf->eaddr = ps_numl[ps_li] + cf->offset;
			cf->flags |= PFPL;
			if (cf->saddr > cf->eaddr)
				pxerror = PGE_BAA;
			}
		else
			pxerror = PGE_INP;
		}
	if (!pxerror && pxprom)		/* if link is up then update config data */
		{
		if (cf->pfcfg)
			{
			pxcfg = cf->pfcfg;
			pifixcfg();
			}
		}
	if (!pxerror)
		pxnfile++;
	if (pxprom)
		{
		pxcfg = pxpcfg;
		pifixcfg();
		}
	}

/* `pigo` - 33 - unitinandoutofemulation */

static void pigo()
	{
	if (pxdisp&PXMH)
		printf(" >PiGo()");
	piemu();
	if (pxdisp&PXHI)
		printf("\nNow Emulating!");
	}

/* `pihelp` - 34 - display help file */

static void pihelp()
	{
	char *is,*ts,*t,**hs;
	PIHELP *h;
	
	is = ps_str;
	if (is == NULL)
		is = "?";
	while (*is == ' ' || *is == '\t')
		is++;
	h = pxhelp;
	while (h->topic)
		{
		t = h->topic;
		ts = is;
		while (*ts)
			{
			if (*ts++ == *t++)
				continue;
			else
				{
				h++;
				--ts;
				break;
				}
			}
		if (!*ts)
			break;
		}
	if (h->topic)
		{
		hs = h->help;
		while (*hs)
			{
			printf("\n%s",*hs);
			hs++;
			}
		}
	else
		printf("\nNo help available on topic '%s'",is);
	}

/* `piimage` - 35  - binary file specification */

static void piimage()
	{
	if (pxdisp&PXMH)
		printf(" >PiImage()");
	pftype = PFBIN;
	pifname();
	pftype = PFHEX;
	}

/* `pisocket` - 36  - set rom socket size */

static void pisocket()
	{
	register short i;

	if (pxdisp&PXMH)
		printf(" >PiSocket()");

	for (i=0; pxromx[i]; i+=2)		/* look for a valid size */
		{
		if (ps_emb == pxromx[i])
			{
			ps_emb = pxromx[i+1];
			break;
			}
		}
	if (!pxromx[i])					/* none found */
		pxerror = PGE_ROM;
	else
		{
		if (ps_id >= 0)				/* only for one unit */
			{
			pxrom[ps_id].ssize = ps_emb;
			}
		else
			{
			for (i=0; i<PIC_NR; i++)	/* for all units */
				{
				pxrom[i].ssize = ps_emb;
				}
			}
		piflags |= PiSK;
		}
	}

/* `pifkeys` - 37 - assign function keys */

static void pifkeys()
	{
	if (pxdisp&PXMH)
		printf(" >PiFkeys()");
	if (!ps_i)
		pxerror = PGE_BAA;
	else
		{
		if (ps_str == NULL)
			pxerror = PGE_BAA;
		else
			{
			ps_num[0]--;
			pxkeys[ps_num[0]] = (char *)calloc(strlen(ps_str)+1,1);
			if (pxkeys[ps_num[0]])
				strcpy(pxkeys[ps_num[0]],ps_str);
			}
		}
	}
			
/* `piiload` - 38  - load binary file */

static void piiload()
	{
	if (pxdisp&PXMH)
		printf(" >PiiLoad()");
	if (piflags&PiMU && !(pxflags&PORQ))
		{
		pxerror = PGE_EMU;
		return;
		}
	if (ps_str)
		{
		piflags |= PiII;
		pftype = PFBIN;
		pifname();
		pftype = PFHEX;
		piflags &= ~PiII;
		}
	if (!pxerror)
		{
		if (!pxprom)
			pcinit();
		if (!pxerror)
			pcload();
		}
	}

/* `piulock` - 39 - set flag for locked units (only when talking directly) */

static void piulock()
	{
	if (pxdisp&PXMH)
		printf(" >PiUlock()");
	piflags |= PiPH;
	}

/* `pinofil` - 40 - don't do filling */

static void pinofil()
	{
	if (pxdisp&PXMH)
		printf(" >PiNofil()");
	piflags &= ~PiFM;
	}

/* `pimemap` - 41 - change memory map display */

static void pimemap()
	{
	if (pxdisp&PXMH)
		printf(" >PiMeMap()");
	if (ps_i)
		{
		if (ps_num[0])
			{
			pxflags |= POMP;
			return;
			}
		}
	pxflags &= ~POMP;
	}

/* `pirecover` - 42 - restart the link */

static void pirecover()
	{
	if (pxdisp&PXMH)
		printf(" >PiRecover()");

	pxprom = 0;
	piflags &= ~PiUP;
	pcexit();
	pcinit();
	}
	
/* `pireset` - 43 - reset the target */

static void pireset()
	{
	short id, dt;

	if (pxdisp&PXMH)
		printf(" >PiReset()");
	if (ps_i)			/* time period specified */
		{
		dt = (short)ps_num[0]/9;
		}
	else
		{
		dt = pxrtime;
		}
	id = (ps_id <0) ? 0 : ps_id;
	if (piflags&(PiiX|PiII))
		pcrstt(id,dt);
	pxrtime = dt;
	}

/* `pisave` - 45 - save PROMICE contents to a file */

static void pisave()
	{
	if (pxdisp&PXMH)
		printf(" >PiSave()");
	if (ps_bchrc != ' ' && ps_bchrc != '\0')
		{
		pxerror = PGE_CMD;
		return;
		}
	if (piflags&PiMU && !(pxflags&PORQ))
		{
		pxerror = PGE_EMU;
		return;
		}
	if (pigetcfg())
		{
		pxsstart = pxcfg->start;
		pxsend = pxmax;
		if (ps_li--)
			{
			pxsstart = ps_numl[0];
			if (ps_li)
				pxsend = ps_numl[1];
			else
				pxsend = pxmax;
			}
		pcsave();
		pxcfg = pftcfg;
		pifixcfg();
		}
	}

/* `pippmode` - 46 - set parallel fast transfer */

static void pippmode()
	{
	if (pxdisp&PXMH)
		printf(" >PiPpmode()");
	if (ps_i)
		{
		switch (ps_num[0])
			{
			case 0:
				pxflags |= POSP;
				break;
			case 1:
				pxflags &= ~POSP;
				break;
			case 2:
				pxflags &= ~(POSP|POVF);
				break;
			default:
				pxerror = PGE_BAA;
			}
		}
	else
		pxflags |= POSP;
	}	

/* `piautor` - 47 - recover from error automatically */

static void piautor()
	{
	if (pxdisp&PXMH)
		printf(" >PiAutoRecover()");

	if (!ps_i)
		pxflags &= ~POAR;
	else
		if (ps_num[0])
			pxflags |= POAR;
		else
			pxflags &= ~POAR;
	}

/* `piversion` - 48 - report PROMICE uC version# */

static void piversion()
	{
	short id;

	if (pxdisp&PXMH)
		printf(" >PiVersion()");
	if (ps_id >= 0)
		{
		id = ps_id;
		}
	else
		{
		if (ps_i)
			{
			id = (short)ps_num[0];
			}
		else
			{
			id = 0;
			}
		}
	if (id >= pxprom)
		pxerror = PGE_BIG;
	else
		printf("\n%s / PROMICE microCode version %s",liver, pxrom[id].ver);
	}

/* `piwrenb` - 49 - enable write from target (only on some units) */

static void piwrenb()
	{

	if (pxdisp&PXMH)
		printf(" >PiWrenb()");
	if (piflags&PiiX)
		{
		if (piflags&PiLO)
			{
			pxerror = PGE_UNF;
			return;
			}
		if (!(pxrom[0].res&AIS31))
			{
			pxerror = PGE_N31;
			return;
			}
		}
	if (ps_i)
		{
		pxwrite = ps_num[0];
		}
	else
		pxerror = PGE_BAA;
	if (pxwrite < 0 || pxwrite > 3)
		pxerror = PGE_BAA;
	if (!pxerror)
		piflags |= PiAB;
	}

/* `piexit` - 50 - Set Exit Code */

static void piexit()
	{
	if (pxdisp&PXMH)
		printf(" >PiExit()");
	pxflags |= POXX;
	if (ps_i)
		pxcode = ps_num[0];
	pxexitv = (int)pxcode;
	}

/* `pifast` - 51 - fast host */

static void pifast()
	{
	if (pxdisp&PXMH)
		printf(" >PiFast()");
	piflags |= PiZZ;
	if (ps_li)
		ppxdl0 = ps_numl[0];
	if (ps_li>1)
		ppxdl1 = ps_numl[1];
	else
		ppxdl1 = ppxdl0;
	if (ps_li>2)
		ppxdl2 = ps_numl[2];
	else
		ppxdl2 = ppxdl0;
	}

/* `pidialog` - 52 - enter dialog mode */

static void pidialog()
	{
	if (pxdisp&PXMH)
		printf(" >PiDialog()");
	pxflags |= POIX;
	}

/* `pippbus` - 53 - parallel port is bussed */

static void pippbus()
	{
	int l;

	if (pxdisp&PXMH)
		printf(" >PiBus()");
#ifndef UNIX
	ps_str = "LPT0";
	if (ps_emb<=0)
		ps_emb = 1;
	*(ps_str+3) = (char)((char)ps_emb+'0');
#endif
	l = strlen(ps_str);
	if (l < PIC_SN)
		strcpy(pxlink.pname,ps_str);
	else
		pxerror = PGE_NTL;
	if (ps_i)				/* port address */
		{
		pxlink.paddr = (int)ps_num[0];
		pxlink.flags |= PLPU;
		}
	pxlink.flags |= PLPB | PLPP;
	}	

/* `pislows` - 54 - set  the serial port to respond slowly */

static void pislows()
	{
	if (pxdisp&PXMH)
		printf(" >PiSlow()");
	pxlink.flags |= PLOW;
	pxlink.flags &= ~PLHI;
	if (piflags&(PiII|PiiX))
		{
		if (pxrom[0].ver[0] < '7')
			pxmode1 &= ~MO_FAST;
		else
			pxmode1 |= MO_SLOW;
		}
	piflags |= PiMO;
	}

/* `pirlink` - 55 - establish link with remote */

static void pirlink()
	{
	pxerror = PGE_NYI;
	}

/* `picurse` - 56 - show or hide the spinning cursor */

static void picurse()
	{
	if (pxdisp&PXMH)
		printf(" >PiCursor()");
	if (ps_i)
		if (ps_num[0])
			piflags &= ~PiHC;
		else
			piflags |= PiHC;
	else
		piflags |= PiHC;
	}
/* `pilite` - 57 - turn off lights blinking */

static void pilite()
	{
	if (pxdisp&PXMH)
		printf(" >PiLite()");
	if (piflags&PiiX)
		if (piflags&PiLO)
			{
			pxerror = PGE_UNF;
			return;
			}
	if (ps_i)
		{
		if (ps_num[0])
			pxmode2 |= M2_LIGHT;
		else
			pxmode2 &= ~M2_LIGHT;
		}
	else
		pxmode2 |= M2_LIGHT;
	piflags |= PiMO;
	}

/* `pitimer` - 58 - disable PromICE timer */

static void pitimer()
	{
	if (pxdisp&PXMH)
		printf(" >PiTimer()");
	if (piflags&PiiX)
		if (piflags&PiLO)
			{
			pxerror = PGE_UNF;
			return;
			}
	if (ps_i)
		{
		if (ps_num[0])
			pxmode2 |= M2_NOTIM;
		else
			pxmode2 &= ~M2_NOTIM;
		}
	else
		pxmode2 |= M2_NOTIM;
	piflags |= PiMO;
	}

/* `pistatus` - 59 - report target status */

static void pistatus()
	{
	if (pxdisp&PXMH)
		printf(" >PiStatus()");
	if (piflags&PiiX)
		if (piflags&PiLO)
			{
			pxerror = PGE_UNF;
			return;
			}
	if (piflags&PiMU)
		{
		picmd(0,PI_VS,1,0,0,0,0,0);
		printf("\nSTATUS: Target power is ");
		if (pxrsp[PICM]&CM_TPOW)
			printf("ON");
		else
			{
			printf("OFF");
			return;
			}
		printf (" - Target is ");
		if (!(pxrsp[PICM]&CM_TACT))
			printf("NOT ");
		printf("accessing ROM");
		}
	else
		printf("\nSTATUS: Unit(s) in LOAD mode");
	}

/* `pinet` - 60 - fastport network */

static void pinet()
	{
	int l;

	if (pxdisp&PXMH)
		printf(" >PiNet()");

	if (ps_str == NULL)
		pxerror = PGE_BAA;
	else
		{
		l = strlen(ps_str);
		if (l < PIC_HN)
			{
			strcpy(pxhost,ps_str);
			piflags |= PiNE;
			}
		else
			pxerror = PGE_NTL;
		}
	}

/* `pitint`- 61 - int target on hostint */

static void pitint()
	{
	if (pxdisp&PXMH)
		printf(" >PiTaint()");
	if (piflags&PiiX)
		{
		if (piflags&PiLO)
			{
			pxerror = PGE_UNF;
			return;
			}
		if (!(pxrom[0].res&AIS31))
			{
			pxerror = PGE_N31;
			return;
			}
		}
	if (ps_i)
		{
		if (ps_num[0])
			pxmode2 |= M2_TAINT;
		else
			pxmode2 &= ~M2_TAINT;
		}
	else
		pxmode2 |= M2_TAINT;
	piflags |= PiMO;
	}

/* `pitrst` - 62 - reset target on hostint */

static void pitrst()
	{
	if (pxdisp&PXMH)
		printf(" >PiTarst()");
	if (piflags&PiiX)
		{
		if (piflags&PiLO)
			{
			pxerror = PGE_UNF;
			return;
			}
		if (!(pxrom[0].res&AIS31))
			{
			pxerror = PGE_N31;
			return;
			}
		}
	if (ps_i)
		{
		if (ps_num[0])
			pxmode2 |= M2_TARST;
		else
			pxmode2 &= ~M2_TARST;
		}
	else
		pxmode2 |= M2_TARST;
	piflags |= PiMO;
	}

/* `piairst` - 63 - reset aitty on hostint */

static void piairst()
	{
	if (pxdisp&PXMH)
		printf(" >PiAirst()");
	if (piflags&PiiX)
		{
		if (piflags&PiLO)
			{
			pxerror = PGE_UNF;
			return;
			}
		if (!(pxrom[0].res&AIS31))
			{
			pxerror = PGE_N31;
			return;
			}
		}
	if (ps_i)
		{
		if (ps_num[0])
			pxmode2 |= M2_AIRST;
		else
			pxmode2 &= ~M2_AIRST;
		}
	else
		pxmode2 |= M2_AIRST;
	piflags |= PiMO;
	}

/* `piaifst` - 64 - use aitty fast mode */

static void piaifst()
	{
	if (pxdisp&PXMH)
		printf(" >PiAifast()");
	if (piflags&PiiX)
		{
		if (piflags&PiLO)
			{
			pxerror = PGE_UNF;
			return;
			}
		if (!(pxrom[0].res&AIS31))
			{
			pxerror = PGE_N31;
			return;
			}
		}
	if (ps_i)
		{
		if (ps_num[0])
			pxmode2 |= M2_AIGOF;
		else
			pxmode2 &= ~M2_AIGOF;
		}
	else
		pxmode2 |= M2_AIGOF;
	piflags |= PiMO;
	}

/* `pinorci` - 65 - no rcv char int on aitty */

static void pinorci()
	{
	if (pxdisp&PXMH)
		printf(" >PiAinorci()");
	if (piflags&PiiX)
		{
		if (piflags&PiLO)
			{
			pxerror = PGE_UNF;
			return;
			}
		if (!(pxrom[0].res&AIS31))
			{
			pxerror = PGE_N31;
			return;
			}
		}
	if (ps_i)
		{
		if (ps_num[0])
			pxmode2 |= M2_AIRCI;
		else
			pxmode2 &= ~M2_AIRCI;
		}
	else
		pxmode2 |= M2_AIRCI;
	piflags |= PiMO;
	}

/* `piairci` - 66 - rcv char int on ai (non-tty) */

static void piairci()
	{
	if (pxdisp&PXMH)
		printf(" >PiAirci()");
	if (piflags&PiiX)
		{
		if (piflags&PiLO)
			{
			pxerror = PGE_UNF;
			return;
			}
		if (!(pxrom[0].res&AIS31))
			{
			pxerror = PGE_N31;
			return;
			}
		}
	if (ps_i)
		{
		if (ps_num[0])
			pxcmode |= MC_AIRCI;
		else
			pxcmode &= ~MC_AIRCI;
		}
	else
		pxcmode |= MC_AIRCI;
	piflags |= PiRQ;
	}

/* `pitintl` - 67 - set traget int length */

static void pitintl()
	{
	if (pxdisp&PXMH)
		printf(" >PiTintl()");
	if (ps_i)
		pxtintl = ps_num[0];
	pxcmode |= MC_EXINL;
	piflags |= PiRQ;
	}

/* `pigasrd` - 68 - set global async read (AiCOM or PiCOM) */

static void pigasrd()
	{
	if (pxdisp&PXMH)
		printf(" >PiGasrd()");
	if (piflags&PiiX)
		{
		if (piflags&PiLO)
			{
			pxerror = PGE_UNF;
			return;
			}
		}
	if (ps_i)
		{
		if (ps_num[0])
			pxcmode |= MC_ASYNC;
		else
			pxcmode &= ~MC_ASYNC;
		}
	else
		pxcmode |= MC_ASYNC;
	piflags |= PiRQ;
	}

/* `pigxint` - 69 - set global command completion int */

static void pigxint()
	{
	if (pxdisp&PXMH)
		printf(" >PiGxint()");
	if (piflags&PiiX)
		{
		if (piflags&PiLO)
			{
			pxerror = PGE_UNF;
			return;
			}
		}
	if (ps_i)
		{
		if (ps_num[0])
			pxcmode |= MC_GXINT;
		else
			pxcmode &= ~MC_GXINT;
		}
	else
		pxcmode |= MC_GXINT;
	piflags |= PiRQ;
	}

/* `pilog` - 70 - open a log file for all command traffic */

static void pilog()
	{
	int l;

	if (pxdisp&PXMH)
		printf(" >PiLog()");
	if (ps_str != NULL || pclog<0)
		{
		if (ps_str)
			{
			l = strlen(ps_str);
			if (l < PIC_FN)
				strcpy(pxlog,ps_str);
			else
				{
				pxerror = PGE_NTL;
				return;
				}
			}
		else
			strcpy(pxlog,"loadice.log");
#ifdef MSDOS
		pclog = open(pxlog,O_BINARY|O_CREAT|O_RDWR|O_TRUNC,S_IREAD|S_IWRITE);
#else
		pclog = open(pxlog,O_CREAT|O_RDWR|O_TRUNC,0xFFFF);
#endif
		if (pclog<0)
			{
			perror("Logfile:");
			return;
			}
		pxdisp |= PLOG;
		}
	else
		{
		if (ps_i)
			{
			if (ps_num[0])
				pxdisp |= PLOG;
			else
				pxdisp &= ~PLOG;
			}
		else
			{
			if (pxdisp&PLOG)
				pxdisp &= ~PLOG;
			else
				pxdisp |= PLOG;
			}
		}
	if (pxdisp&PLOG)
		printf("\nLog is ON");
	else
		printf("\nLog is OFF");
	}

/* `pibank` - 71 - load multiple banks */

static void pibank()
	{
	if (pxdisp&PXMH)
		printf(" >PiBank()");
	if (!ps_i)
		{
		pxerror = PGE_BAA;
		return;
		}
	if (ps_num[0] < 0)
		{
		pxbanks = 0;
		if (piflags&(PiII|PiiX))
			{
			pifixcfg();
			if (pxdisp&PXHI)
				printf("\nBanking is OFF");
			}
		return;
		}
	if (!pxbanks)
		{
		switch(ps_num[0])
			{
			case 2:
			case 4:
			case 8:
			case 16:
				break;
			default:
				pxerror = PGE_BAA;
				return;
			}
		if (ps_id)
			{
			pxrom[ps_id].banks = ps_num[0];
			pxrom[ps_id].flags |= PRBA;
			}
		pxbanks = ps_num[0];
		pxbank = 0;
		if (piflags&(PiII|PiiX))
			{
			pifixcfg();
			if (pxdisp&PXHI)
				printf("\nBanking is ON with %ld banks",pxbanks);
			}
		}
	else
		{
		if (ps_num[0] < 0 || ps_num[0] >= pxbanks)
			pxerror = PGE_BAA;
		else
			pxbank = ps_num[0];
		}
	}

/* `pinetr` - 72 - reset fastport */

static void pinetr()
	{
	if (pxdisp&PXMH)
		printf(" >PiNetr()");
	if (ps_i)
		if (!ps_num[0])
			{
			piflags &= ~PiRF;
			return;
			}
		piflags |= PiRF;
	}

/* `pinoar` - 73 - no auto reset */

static void pinoar()
	{
	if (pxdisp&PXMH)
	if (ps_i)
		{
		if (ps_num[0])
			pxmode2 |= M2_LIGHT;
		else
			pxmode2 &= ~M2_LIGHT;
		}
	else
		pxmode2 |= M2_LIGHT;
		printf(" >PiNoar()");
	}

/* `piburst - 74 - set burst mode */

static void piburst()
	{
	if (pxdisp&PXMH)
		printf(" >PiBurst()");
	if (piflags&PiiX)
		{
		if (piflags&PiLO)
			{
			pxerror = PGE_UNF;
			return;
			}
		if (!(pxrom[0].res&AIS31))
			{
			pxerror = PGE_N31;
			return;
			}
		}
	if (ps_i)
		pxburst = ps_num[0];
	else
		pxerror = PGE_BAA;
	switch (pxburst)
		{
		case 0:
			break;
		case 4:
			pxburst = 1;
			break;
		case 8:
			pxburst = 2;
			break;
		case 16:
			pxburst = 3;
			break;
		default:
			pxerror = PGE_BAA;
		}
	if (!pxerror)
		piflags |= PiAB;
	}

/* `piaieco` - 75 - set ai control options */

static void piaieco()
	{
	if (pxdisp&PXMH)
		printf(" >PiAieco()");
	if (piflags&PiiX)
		{
		if (piflags&PiLO)
			{
			pxerror = PGE_UNF;
			return;
			}
		if (!(pxrom[0].res&AIS31))
			{
			pxerror = PGE_N31;
			return;
			}
		}
	if (ps_i)
		pxaieco = ps_num[0];
	else
		pxerror = PGE_BAA;
	if (pxaieco < 0 || pxaieco > 3)
		pxerror = PGE_BAA;
	if (!pxerror)
		piflags |= PiAB;
	}

/* `pimodei` - 76 - define mode at connect */

static void pimodei()
	{
	if (pxdisp&PXMH)
		printf(" >PiModei()");
	if (ps_str)
		{
		if (*ps_str == 's')
			pxmodei = MO_LOAD;
		else
			if (*ps_str == 'g')
				pxmodei = MO_EMU;
			else
				pxerror = PGE_BAA;
		}
	else
		pxerror = PGE_BAA;
	}

/* `pimodeo` - 77 - define mode at exit */

static void pimodeo()
	{
	if (pxdisp&PXMH)
		printf(" >PiModeo()");
	if (ps_str)
		{
		if (*ps_str == 's')
			pxmodeo = MO_LOAD;
		else
			if (*ps_str == 'g')
				pxmodeo = MO_EMU;
			else
				pxerror = PGE_BAA;
		}
	else
		pxerror = PGE_BAA;
	}

/* `piaimode` - 78 - set AI operating options */

static void piaimode()
	{
	if (pxdisp&PXMH)
		printf(" >PiAiMode()");
	if (piflags&PiiX)
		{
		if (piflags&PiLO)
			{
			pxerror = PGE_UNF;
			return;
			}
		if (!(pxrom[0].res&AIS31))
			{
			pxerror = PGE_N31;
			return;
			}
		}
	if (ps_i)
		{
		pxmode2 = (char)ps_num[0];
		if (ps_i>1)
			pxcmode = (char)ps_num[1];
		piflags |= PiMO;
		}
	else
		pxerror = PGE_BAA;
	}

/* `pinoload` - 79 - do not down-load file before dialog mode */

static void pinoload()
	{
	if (pxdisp&PXMH)
		printf(" >PiNoLoad()");
	pxflags &= ~POLO;
	}

/* `piaddrerr` - 80 - address out of range errors */

static void piaddrerr()
	{
	if (pxdisp&PXMH)
		printf(" >PiAddrErr()");
	if (ps_i)
		{
		if (ps_num[0])
			pxflags &= ~PONO;
		else
			pxflags |= PONO;
		if (ps_i > 1)
			{
			if (ps_num[1])
				pxflags |= PODO;
			else
				pxflags &= ~PODO;
			}
		}
	else
		pxflags |= PONO;
	}

/* `picserr` - 81 - checksum checking */

static void picserr()
	{
	if (pxdisp&PXMH)
		printf(" >PiCserr()");
	if (ps_i)
		{
		if (ps_num[0])
			pxflags &= ~PONK;
		else
			pxflags |= PONK;
		if (ps_i > 1)
			{
			if (ps_num[1])
				pxflags |= PODK;
			else
				pxflags &= ~PODK;
			}
		}
	else
		pxflags |= PONK;
	}

/* `pireqack` - 82 - use req/ack for read/write */

static void pireqack()
	{
	if (pxdisp&PXMH)
		printf(" >PiReqAck()");

	if (pxflags&PORQ)
		pxflags &= ~PORQ;
	else
		{
		if (ps_i--)
			{
			piflags |= PiHO;
			pxhso = ps_num[0];
			if (ps_i--)
				{
				piflags |= PiRQ;
				pxreq = ps_num[1];
				if (ps_i)
					pxack = ps_num[2];
				}
			}
		pxflags |= PORQ;
		}
	if (pxflags&PORQ)
		printf("\n Hold/HoldAck mode ON");
	else
		printf("\n Hold/HoldAck mode OFF");
	}

/* `piafkeys` - 83 - assign alt-function keys */

static void piafkeys()
	{
	if (pxdisp&PXMH)
		printf(" >PiFkeys()");
	if (!ps_i)
		pxerror = PGE_BAA;
	else
		{
		if (ps_str == NULL)
			pxerror = PGE_BAA;
		else
			{
			ps_num[0] += 11;
			pxkeys[ps_num[0]] = (char *)calloc(strlen(ps_str)+1,1);
			if (pxkeys[ps_num[0]])
				strcpy(pxkeys[ps_num[0]],ps_str);
			}
		}
	}

/* `pisleep` - 84 - waste some time */

static void pisleep()
	{
	if (pxdisp&PXMH)
		printf(" >PiSleep()");
	if (!ps_i || !ps_num[0])
		ps_num[0] = 1;
	if (pxdisp&PXHI)
		printf("\nsleeping  %d units",(short)ps_num[0]);
	pi_sleep((short)ps_num[0]);
	}

/* `piundef` - UNDEFINED */

static void piundef()
	{
	if (pxdisp&PXMH)
		printf(" >PiUndef()");
	}

/* `pigetcfg` - find an existing one or build an alternate config */

static PICONFIG *pigetcfg()
	{
	pftcfg = pxcfg;
	if (ps_id >= 0)
		{
		if (ps_id >= pxprom)
			{
			pxerror = PGE_BIG;
			return(0);
			}
		pxcfg = &pxaltcfg;
		pxcfg->words = 1;
		pxcfg->uid[0] = ps_id;
		}
	pifixcfg();
	return(pxcfg);
	}

/*	table of function pointers -
	- this table is used by the input parser
	- to call the appropriate routine as
	- specied in the current script
*/
void (*psynf[])() = {
	piailoc,	/* 1 - set up ailoc for AITTY */
	pibrate,	/* 2 - baud rate */
	picompare,	/* 3 - comapre */
	pidump,		/* 4 - dump */
	piedit,		/* 5 - edit */
	piundef,	/* 6 - Undefined function */
	pifill,		/* 7 - fill now */
	pihigh,		/* 8 - high speed */
	pihso,		/* 9 - hso polarity */
	pisearch,	/* 10 - search for stuff */
	pichksum,	/* 11 - checksum */
	piload,		/* 12 - load*/
	pimove,		/* 13 - move ROM data around */
	pinumber,	/* 14 - number of unit */
	piouput,	/* 15 - serial device */
	pistdpp,	/* 16 - parallel port */
	pibidipp,	/* 17 - bidirectional pp */
	piromsize,	/* 18 - rom size */
	pistop,		/* 19 - go in to load mode */
	pitest,		/* 20 - test unit */
	piserial,	/* 21 - return unit's serial number */
	piverify,	/* 22 - verify */
	piword,		/* 23 - word size */
	pinocksm,	/* 24 - no-check checksum */
	pidelay,	/* 25 - set timeout delay */
	pinoaddr,	/* 26 - ignore address out of range errors */
	piaisw,		/* 27 - loadice talking through the AISwitch */
	piaitty,	/* 28 - go into transparent mode*/
	pidiscfg,	/* 29 - display config */
	pidisply,	/* 30 - change display level */
	pisystem,	/* 31 - escape command to system */
	pifname,	/* 32 - hex file name */
	pigo,		/* 33 - go in to emulation */
	pihelp,		/* 34 - give out help */
	piimage,	/* 35 - image file */
	pisocket,	/* 36 - set socket size */
	pifkeys,	/* 37 - assign function keys */
	piiload,	/* 38 - load one binary file */
	piulock,	/* 39 - locked units */
	pinofil,	/* 40 - don't do filling */
	pimemap,	/* 41 - change map display status */
	pirecover,	/* 42 - recover from failure */
	piundef,	/* 43 - Undefined function */
	pireset,	/* 44 - reset target */
	pisave,		/* 45 - save to file */
	pippmode,	/* 46 - set fast parallel transfer mode */
	piautor,	/* 47 - autorecover */
	piversion,	/* 48 - report uCode ver# */
	piwrenb,	/* 49 - enable or disable write signal */
	piexit,		/* 50 - set exit code */
	pifast,		/* 51 - fast host */
	pidialog,	/* 52 - enter dialog mode */
	pippbus,	/* 53 - parallel port is bussed */
	pislows,	/* 54 - turn down the serial rate */
	pirlink,	/* 55 - establish link with remote/ROMview */
	picurse,	/* 56 - show or hide spinning cursor */
	pilite,		/* 57 - lights or no lights */
	pitimer,	/* 58 - timer or no timer */
	pistatus,	/* 59 - report target status */
	pinet,		/* 60 - fast port ethernet */
	pitint,		/* 61 - int target on hostint */
	pitrst,		/* 62 - reset target on hostint */
	piairst,	/* 63 - reset aitty on hostint */
	piaifst,	/* 64 - use aitty fast mode */
	pinorci,	/* 65 - no rcv char int on aitty */
	piairci,	/* 66 - rcv char int on ai (non-tty) */
	pitintl,	/* 67 - set target interrupt length */
	pigasrd,	/* 68 - set global async read */
	pigxint,	/* 69 - set global command completion int */
	pilog,		/* 70 - initiate loadice comm log */
	pibank,		/* 71 - load multiple banks */
	pinetr,		/* 72 - reset fastport */
	pinoar,		/* 73 - no auto reset */
	piburst,	/* 74 - set burst mode control */
	piaieco,	/* 75 - set ai control options */
	pimodei,	/* 76 - define default mode at connect */
	pimodeo,	/* 77 - define mode at exit */
	piaimode,	/* 78 - set AI mode all at once */
	pinoload,	/* 79 - do not load files */
	piaddrerr,	/* 80 - ignore address out of range error control */
	picserr,	/* 81 - ignore checksum error control */
	pireqack,	/* 82 - use req/ack for read/write */
	piafkeys,	/* 83 - alt function key */
	pisleep		/* 84 - sleep a while */
	};
