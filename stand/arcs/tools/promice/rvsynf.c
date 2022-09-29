/*      RVSynF.c - Edit 0.2

	RomView Version 1.1a
	Copyright (C) 1990-2 Grammar Engine, Inc.
	All rights reserved
	
	NOTICE:  This software source is a licensed copy of Grammar Engine's
	property.  It is supplied to you as part of support and maintenance
	of some Grammar Engine products that you may have purchased.  Use of
	this software is strictly limited to use with such products.  Any
	other use constitutes a violation of this license to you.
*/

/*       - RomView/functions called from the parser
	global variables are initialized by the parser
*/
#ifdef PIRV
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#ifdef MSDOS
#include <process.h>
#include <io.h>
#endif

#include "piconfig.h"
#include "pistruct.h"
#include "rvconfig.h"
#include "rvstruct.h"
#include "pierror.h"
#include "pidriver.h"
#include "rvdata.h"
#include "rvhelp.h"
#include "pisyn.h"

#ifdef ANSI
extern void rxinit(void);
extern void rcinit(void);
extern void rcdump(void);
extern void rcedit(void);
extern void rcregs(void);
extern void rcstak(void);
extern void rcload(void);
extern void rcreadi(void);
extern void rcdumpi(void);
extern void rvdosfr(void);
extern void rvdoxeq(void);
extern void rvsetbrk(void);
extern void rvclrbrk(void);
extern void rvgouser(char code);
extern void rvgoon(void);
extern void rvdostep(void);
static void rvdumpx(void);
static void rlcinit(void);
#else
extern void rxinit();
extern void rcinit();
extern void rcdump();
extern void rcedit();
extern void rcregs();
extern void rcstak();
extern void rcload();
extern void rcreadi();
extern void rcdumpi();
extern void rvdosfr();
extern void rvdoxeq();
extern void rvsetbrk();
extern void rvclrbrk();
extern void rvgouser();
extern void rvgoon();
extern void rvdostep();
static void rvdumpx();
static void rlcinit();
#endif

/* `rvailink` - 'a' - init AiCOM link */

static void rvailink()
	{
	if (pxdisp&PXMH)
		printf(" >RvAiLink()");

	if (rvflags&RvUP)
		return;

	if (ps_li != 7)
		{
		pxerror = PGE_INP;
		return;
		}
	rvflags |= RvAI;
	rxlink = pxrom[0].amask | 3 | ps_numl[1];
	rxproto = 0;
	rlcinit();
	}

/* `rvbreak` - 'b' - set a breakpoint */

static void rvbreak()
	{
	long i;
	RVBREAK *bp;
	
	if (pxdisp&PXMH)
		printf(" >RvBreak()");
	if (!ps_i)
		{
		for (i=0; i<RVC_NBRK; i++)
			{
			bp = &rxbreak[i];
			if (bp->flags&RBSY)
				{
				printf("\nBreakPoint %d @ 0x%lX",i,bp->bad);
				if (bp->flags&RBCT)
					printf(" SkipCount=%d",bp->count);
				if (bp->flags&RBVL)
					printf(" Condition: Addr=%lX Value=%lX",bp->vad,bp->count);
				}
			}
		}
	else
		{
		for (i=0; i<RVC_NBRK; i++)
			{
			bp = &rxbreak[i];
			if (bp->flags&RBSY)
				continue;
			bp->bad = ps_num[0];
			bp->flags |= RBIN;
			if (ps_i == 2)
				{
				bp->count = ps_num[1];
				bp->flags |= RBCT;
				}
			if (ps_i == 3)
				{
				bp->vad = ps_num[1];
				bp->count = ps_num[2];
				bp->flags |= RBVL;
				}
			break;
			}
		if (i == RVC_NBRK)
			pxerror = PGE_TMB;
		else
			rvsetbrk();
		}
	}

/* `rvclear` - 'c' - clear one or more break point */

static void rvclear()
	{
	long i;
	RVBREAK *bp;
	
	if (pxdisp&PXMH)
		printf(" >RvClear()");

	for (i=0; i<RVC_NBRK; i++)
		{
		bp = &rxbreak[i];
		if (!(bp->flags&RBSY))
			continue;
		if (bp->bad == ps_num[0])
			{
			bp->flags |= RBEX;
			break;
			}
		}
	if (i == RVC_NBRK)
		pxerror = PGE_BNF;
	else
		rvclrbrk();
	}

/* `rcdumpc - 'C' - dump code space */

static void rvdumpc()
	{

	if (pxdisp&PXMH)
		printf(" >RvDumpc()");
	rxflags |= RxIS;
	rvdumpx();
	rxflags &= ~RxIS;
	}

/* `rcdumpx - 'D' - dump external space */

static void rvdumpx()
	{
	long tmp;

	if (pxdisp&PXMH)
		printf(" >RvDumpx()");
	if (piflags&PiCR)
		{
		tmp = rxdend - rxdstart;
		rxdstart = rxdend + 1;
		rxdend = rxdstart + tmp;
		}
	else
		{
		if (ps_li--)
			{
			rxdstart = ps_numl[0];
			if (ps_li)
				rxdend = ps_numl[1];
			else
				rxdend = rxdstart + RVC_DS;
			}
		else
			{
			rxdstart = 0;
			rxdend = RVC_DS;
			}
		}
	rcdump();
	}

/* `rvdump` - 'd' - dump internal data space */

static void rvdump()
	{

	if (pxdisp&PXMH)
		printf(" >RvDump()");

	rxflags |= RxIR;
	rvdumpx();
	rxflags &= ~RxIR;
	}

/* `rveditx` - 'E' - edit target address space */

static void rveditx()
	{
	short i;

	if (pxdisp&PXMH)
		printf(" >RvEditx()");
	if (ps_i)
		{
		rxestart = ps_num[0];
		for (i=0; i<ps_li; i++)
			rxelist[i] = ps_numl[i];
		rxelcnt = ps_li;
		}
	else
		{
		rxestart = 0;
		rxelcnt = 0;
		}
	rcedit();
	}

/* `rvedit` - 'e' - edit target address space */

static void rvedit()
	{
	if (pxdisp&PXMH)
		printf(" >RvEdit()");

	rxflags |= RxIR;
	rveditx();
	rxflags &= ~RxIR;
	}

/* `rvgoman` - 'g' - run user program */

static void rvgoman()
	{
	if (pxdisp&PXMH)
		printf(" >RvGo()");
	
	if (!ps_i)
		pxerror = PGE_NOG;
	else
		{
		rxstart = ps_num[0];
		rvflags |= RvGO;
		rvflags &= ~RvBP;
		piflags |= PiNT;
		rvgouser(0);
		}
	}

/* `rvhelp` - 'H' - display help file */

static void rvhelp()
	{
	char *is,*ts,*t,**hs;
	RVHELP *h;
	
	if (pxdisp&PXMH)
		printf(" >RvHelp()");
		
	is = ps_str;
	if (is == NULL)
		is = "?";
	while (*is == ' ' || *is == '\t')
		is++;
	h = rxhelp;
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
			printf("\n %s",*hs);
			hs++;
			}
		}
	else
		printf("\nNo help available on topic '%s'",is);
	}

/* `rvediti` - 'e' - edit target instruction space */

static void rvediti()
	{
	if (pxdisp&PXMH)
		printf(" >RvEditi()");

	rxflags |= RxIS;
	rveditx();
	rxflags &= ~RxIS;
	}

/* `rvdumpi` - 'i' - dissassemble target instruction space */

static void rvdumpi()
	{
	long tmp;

	if (pxdisp&PXMH)
		printf(" >RvDumpi()");
	if (piflags&PiCR)
		{
		tmp = rxiend - rxistart;
		rxistart = rxiend + 1;
		rxiend = rxistart + tmp;
		}
	else
		{
		if (ps_li--)
			{
			rxistart = ps_numl[0];
			if (ps_li)
				rxiend = ps_numl[1];
			else
				rxiend = rxistart + RVC_DS;
			}
		else
			{
			rxistart = 0;
			rxiend = RVC_DS;
			}
		}
	rxflags |= RxIS;
	rcdumpi();
	rxflags &= ~RxIS;
	}

/* `rvproced` - 'P' - proceed from a break point */

static void rvproced()
	{
	if (pxdisp&PXMH)
		printf(" >RvProced()");
	if (rvflags&RvGO)
		{
		if (ps_i)
			{
			rxbreak[rxnbrk].count = ps_num[0];
			rxbreak[rxnbrk].flags |= RBCT;
			}
		rvflags &= ~RvBP;
		rvgoon();
		}
	else
		pxerror = PGE_NUP;
	}

/* `rvpilink` - 'p' - init PiCOM link */

static void rvpilink()
	{
	if (pxdisp&PXMH)
		printf(" >RvPiLink()");

	if (rvflags&RvUP)
		return;

	if (ps_li != 7)
		{
		pxerror = PGE_INP;
		return;
		}
	rvflags |= RvPI;
	rxlink = pxrom[0].amask | ps_numl[1];
	rxproto = CM_PICOM&0x0FF;
	rlcinit();
	}

static void rlcinit()
	{
	if (ps_numl[0])
		rvflags |= RvAS;
	else
		rvflags &= ~RvAS;
	rvseva = ps_numl[2];
	rvmons = ps_numl[3];
	rvmone = ps_numl[4];
	rvbrkc = ps_numl[5] | RVC_CALL;
	rvxqa = ps_numl[6];
	rxinit();
	rcinit();
	}

/* `rvxequte` - 'q' - execute an instruction */

static void rvxequte()
	{
	if (pxdisp&PXMH)
		printf(" >RvXequte()");

	if (ps_i)
		{
		rxuseri = ps_num[0];
		rxflags |= RxUI;
		}
	rvdoxeq();
	rxflags &= ~RxUI;
	}

/* `rvregs` - 'r' - dump or load target registers */

static void rvregs()
	{
	if (pxdisp&PXMH)
		printf(" >RvRegs()");

	rxdmask = 0xFF;
	rxamask = 0xFF;
	if (ps_li--)
		{
		rxdmask = (short)ps_num[0];
		if (ps_li)
			rxamask = (short)ps_num[1];
		}
	rcregs();
	}

/* `rvreadi` - 'R' - Read PROMICE internal buffer */

static void rvreadi()
	{
	if (pxdisp&PXMH)
		printf(" >RvReadi()");
	
	rcreadi();
	}

/* `rvstack` - 's' - dump stack */

static void rvstack()
	{
	if (pxdisp&PXMH)
		printf(" >RvStack()");

	rxstkp = RVC_STKP;
	rxstks = RVC_STKS;

	if (ps_i)
		{
		rxstkp = ps_num[0];
		ps_i--;
		}
	if (ps_i)
		rxstks = ps_num[1];
	rcstak();
	}

/* `rvstep` - 'S' - step a few instructions */

static void rvstep()
	{
	if (pxdisp&PXMH)
		printf(" >RvStep()");
	if (!ps_i)
		rxstep = 1;
	else
		rxstep = ps_num[0];
	rvdostep();
	}
	
/* `rvhtrap` - 't' - test hardware trap */

static void rvhtrap()
	{
	if (pxdisp&PXMH)
		printf(" >RvHtrap()");
	pxerror = PGE_NYI;
	}

/* `testu` - 'T' - test PROMICE unit */

static void rvtestu()
	{
	if (pxdisp&PXMH)
		printf(" >RvTestU()");

	}

/* `rvwords` - 'w' - set word size */

static void rvwords()
	{
	if (pxdisp&PXMH)
		printf(" >RvWords()");
	if (ps_i)
		rxwords = ps_num[0]/8;
	else
		rxwords = 1;
	}

/* `rvsymbol` - 'S' - SFRs or special symbols */

static void rvsymbol()
	{
	if (pxdisp&PXMH)
		printf(" >RvSymbol()");
	if (*ps_str == '.')
		{
		rxreg = &ps_str[1];
		if (ps_li)
			{
			rxrval = ps_numl[0];
			rxflags |= RxWR;
			}
		rvdosfr();
		rxflags &= ~RxWR;
		}
	else
		pxerror = PGE_CMD;
	}

/* `rvundef` - UNDEFINED */

static void rvundef()
	{
	if (pxdisp&PXMH)
		printf(" >RVUndef()");
	}

/*      table of function pointers -
	- this table is used by the input parser
	- to call the appropriate routine as
	- specied in the current script
*/
void (*rvsynf[])() = {
	rvailink,       /* a - init AiCOM link */
	rvbreak,        /* b - set a break point */
	rvclear,        /* c - clear a break point */
	rvdump,         /* d - dump target address space */
	rvedit,         /* e - edit target address space */
	rvundef,        /* f - */
	rvgoman,        /* g - run user program */
	rvhelp,         /* h - give user some help */
	rvdumpi,        /* i - dump instructions */
	rvundef,        /* j - */
	rvundef,        /* k - */
	rvundef,        /* l - */
	rvundef,        /* m - */
	rvundef,        /* n - */
	rvundef,        /* o - */
	rvpilink,       /* p - init PiCOM link */
	rvxequte,       /* q - execute an instruction */
	rvregs,         /* r - read or write registers */
	rvstack,        /* s - dump stack */
	rvhtrap,        /* t - test hardware trap */
	rvundef,        /* u - */
	rvundef,        /* v - */
	rvwords,        /* w - set word size */
	rvundef,        /* x - */
	rvundef,        /* y - */
	rvundef,        /* z - */
	rvundef,        /* A - */
	rvundef,        /* B - */
	rvdumpc,        /* C - dump instruction space */
	rvdumpx,        /* D - dump external data space */
	rveditx,        /* E - edit external data space */
	rvundef,        /* F - */
	rvundef,        /* G - */
	rvundef,        /* H - */
	rvediti,        /* I - edit instruction space */
	rvundef,        /* J - */
	rvundef,        /* K - */
	rvundef,        /* L - */
	rvundef,        /* M - */
	rvundef,        /* N - */
	rvundef,        /* O - */
	rvproced,       /* P - proceed from a break point */
	rvundef,        /* Q - */
	rvreadi,        /* R - Read PROMICE internal buffer */
	rvsymbol,       /* S - SFRs or special symbols */
	rvtestu,        /* T - test the PROMICE */
	rvundef,        /* U - */
	rvundef,        /* V - */
	rvundef,        /* W - */
	rvstep,         /* X - step through some instructions */
	rvundef,        /* Y - */
	rvundef         /* Z - */
	};
#endif
