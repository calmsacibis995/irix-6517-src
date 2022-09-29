/*	PiUtil.c - Edit 6

	LoadICE Version 3
	Copyright (C) 1990-95 Grammar Engine, Inc.
	All rights reserved
	
	NOTICE:  This software source is a licensed copy of Grammar Engine's
	property.  It is supplied to you as part of support and maintenance
	of some Grammar Engine products that you may have purchased.  Use of
	this software is strictly limited to use with such products.  Any
	other use constitutes a violation of this license to you.
*/

/*	 - LoadICE utility functions:
		piwrite() - write data to promice
		piread() - read data from promice
		pilod() - set current config to load mode
		pilop() - load new pointers
		piemu() - set all units to emulate
		picmd() - issue a short command, data length < 5
		pirsp() - read a response from PROMICEs
		pifixcfg() - update current config's range values
		piinit() - init globals at startup
		piinitr() - init current ROM structure
		piinitf() - init current file structure
*/
#include <stdio.h>

#include "piconfig.h"
#include "pistruct.h"
#include "pierror.h"
#include "pidriver.h"
#include "pidata.h"

#ifdef ANSI
void pifixcfg(void);
void piinitr(void);
void piinitf(void);
void pilop(void);
#else
void pifixcfg();
void piinitr();
void piinitf();
void pilop();
#endif

/* `piwrite` - write `pxxbf` per `pxxloc` and `pxxbc` */

void piwrite()
	{
	long boff = 0;
	long tloc = pxxloc;
	long tct = pxxbc;
		
	if (pxdisp&PXMI)
		printf("\nPiWrite: loc=%08lX ct=%ld",pxxloc,pxxbc);

	while (tct && !pxerror)		/* while data count and no error */
		{
		pxacfg = pxcfg;
		while (pxacfg)			/* find the config to which to write */
			{
			if (tloc <= pxacfg->end) /* transfer loc in this config? */
				break;
			pxacfg = pxacfg->next;
			}
		pxcfg->lct = 0;			/* initilaize load count */
		if (pxacfg)				/* found one */
			{
			if (tloc+tct > pxacfg->end)	/* only as much as would fit here */
				pxdbc = pxacfg->end - tloc + 1;
			else
				pxdbc = tct;
			if ((pxcfg->lptr != tloc) || !pxcfg->lct)
				{
				pxcfg->lptr = tloc;
				pilop();		/* load pointers - piutil */
				}
			if (pxdbc > pxcfg->lct)
				pxdbc = pxcfg->lct;
			pxdbf = &pxxbf[boff];
			pxerror = pi_write();	/* pidriver call */

			/* adjust all the counts etc. for next transfer */

			boff += pxdbc;
			tct -= pxdbc;
			tloc += pxdbc;
			pxcfg->lptr += pxdbc;
			pxcfg->lct -= pxdbc;
			}
		else
			pxerror = PGE_AOR;	/* no config found! AddressOutOfRange */
		}
	}

/* `piread` - read `pxybf` per `pxyloc` and `pxybc` */

void piread()
	{
	long boff = 0;
	long tloc = pxyloc;
	long tct = pxybc;
			
	if (pxdisp&PXMI)
		printf("\nPiRead: loc=%08lX ct=%ld",pxyloc,pxybc);

	while (tct && !pxerror)		/* till count exhausted or error */
		{
		pxacfg = pxcfg;
		while (pxacfg)			/* find the config to read from */
			{
			if (tloc <= pxacfg->end)
				break;
			pxacfg = pxacfg->next;
			}
		pxcfg->lct = 0;
		if (pxacfg)				/* if found a config */
			{
			if (tloc+tct > pxacfg->end)
				pxdbc = pxacfg->end - tloc + 1;
			else
				pxdbc = tct;
			if ((pxcfg->lptr != tloc) || !pxcfg->lct)
				{
				pxcfg->lptr = tloc;
				pilop();		/* need to load unit pointers - piutil */
				}
			if (pxdbc > pxcfg->lct)
				pxdbc = pxcfg->lct;
			pxdbf = &pxybf[boff];
			pxerror = pi_read();	/* pidriver call */

			/* adjust all the counts etc. for next read */

			boff += pxdbc;
			tct -= pxdbc;
			tloc += pxdbc;
			pxcfg->lptr += pxdbc;
			pxcfg->lct -= pxdbc;
			}
		else
			pxerror = PGE_AOR;	/* no config found, AddressOutOfRange */
		}
	pxybc -= tct;
	}

/* `pilod` - put current config in load mode */

void pilod()
	{
	PICONFIG *cx;
	long pitflag = piflags;
	short u;
	
	piflags &= ~PiMU;		/* we are not gonna be emulating */
	pxmode1 |= MO_LOAD;
	cx = pxcfg;
	while(cx && !pxerror)
		{
		for (u=0; u<cx->words; u++)
			{
			if (piflags&PiPH)	/* check for LOCKED units */
				{
				picmd((char)cx->uid[u],PI_MO,1,pxmode1,0,0,0,0);
				if (pxrsp[PIDT] == PIC_LOCK)
					{
					pxerror = PGE_LOK;
					piflags = pitflag;
					return;
					}
				}
			else	/* else put units in load mode */
				picmd((char)cx->uid[u],PI_MO|CM_NORSP,1,pxmode1,0,0,0,0);
			}
		cx = cx->next;
		}
	pxcfg->flags |= PCLO;
	}

/* `pilop` - load data pointer to PROMICE's in active config */

void pilop()
	{
	short i;
	long maxct,tlp;

	/* at anytime we can only transfer 64KxWordSize bytes only */

	maxct = PIC_MX * pxacfg->words;
	tlp = pxcfg->lptr;
	while (tlp >= maxct)
		tlp -= maxct;
	pxcfg->lct = maxct - tlp;	/* must switch config after this many bytes */

	/* pxsrom is the starting ROM# in multi-word config. This will happen 
	 when a record starts at address that is not a multiple of word size */

	pxsrom = (short)((pxcfg->lptr-pxacfg->start)%pxacfg->words);

	for (i=0; i<pxacfg->words; i++)
		{
		tlp = ((pxcfg->lptr-pxacfg->start)/pxacfg->words) |
			pxrom[pxacfg->uid[i]].amask;	/* OR the high bits */
		if (pxbanks)
			tlp |= (pxmax+1)*pxbank;
		if (i<pxsrom)	/* adjust pointer if address was not word multiple */
			tlp++;
		pxrom[pxacfg->uid[i]].cramp = tlp;
		picmd((char)pxacfg->uid[i],PI_LP|CM_NORSP,3,
			(char)(tlp>>16),(char)(tlp>>8),(char)tlp,0,0);
		}
	}

/* `piemu` - set all unit's to emulate */

void piemu()
	{
	PICONFIG *cx;
	short u;
	
	if (pxerror)
		pierror();
#ifdef UNIX
	piflags |= PiMU | PiHC;
#else
	piflags |= PiMU;
#endif
	pxmode1 &= ~MO_LOAD;
	cx = pxcfg;
	while(cx)
		{
		for (u=0; u<cx->words; u++)
			{
			picmd((char)cx->uid[u],PI_MO|CM_NORSP,1,pxmode1,0,0,0,0);
			if (pxerror)
				return;
			}
		cx = cx->next;
		}
	pxcfg->flags &= ~PCLO;
	}

void pikick()
	{
	picmd(0,PI_MO,1,pxmode1,0,0,0,0);
	}

/* `picmd` - for issuing short commands */

#ifdef ANSI
void picmd(char id,char cmd,char ct,char d0,char d1,char d2,char d3,char d4)
#else
void picmd(id, cmd, ct, d0, d1, d2, d3, d4)
char id,cmd,ct,d0,d1,d2,d3,d4;
#endif
	{
	register char *tcp;

	if (piflags&PiUP)
		{
		if (!pxerror)
			{
			if (ct > 5)
				pxerror = PGE_CFT;
			else
				{
				tcp = pxcmd;
				pxcmdl = ct + 3;
				*tcp++ = id;
				*tcp++ = cmd;
				*tcp++ = ct;
				*tcp++ = d0;
				*tcp++ = d1;
				*tcp++ = d2;
				*tcp++ = d3;
				*tcp++ = d4;
				pxerror = pi_cmd();	/* pidriver call */
				}
			}
		}
	else
		pxerror = PGE_XXX;
	}

/* `pirsp` - a companion routine to the above */

void pirsp()
	{
	if (!pxerror)
		pxerror = pi_rsp();	/* pidriver call */
	}

/* `pifixcfg` - fix the parameters in current configuration
	Initialize config per real information about the units	*/

void pifixcfg()
	{
	PICONFIG *cx;
	long ad;
	short i;
	
	if (pxprom)
		if (pxnrom > pxprom)
			{
			pxerror = PGE_BIG;
			return;
			}
	if (!pxcfg)
		return;
	cx = pxcfg;
	ad = 0;
	while (cx)
		{
		cx->start = ad;				/* init start address for config */
		for (i=0; i<cx->words; i++)
			{
			if (pxprom)
					if (cx->uid[i] >= pxprom)
					{
					pxerror = PGE_BCF;
					return;	
					}
			ad += pxrom[cx->uid[i]].esize;	/* up by emulation size */
			pxrom[cx->uid[i]].amask = PIC_MM - pxrom[cx->uid[i]].ssize;
			}
		cx->end = ad - 1;
		cx = cx->next;
		}
	/* init all config variables */
	pxmax = ad - 1;
	pxcfg->lptr = -1;
	pxcfg->lct = 0;
	pxcfg->max = pxmax;

	if (pxbanks)
		{
		pxmax = ((pxmax+1)/pxbanks) - 1;
		pxcfg->max = pxmax;
		}

	}

/* `piinit` - initialize the globals */

void piinit()
	{
	short i;
	extern int errno;

	errno = 0;	
	pxhso = 0;
	pxreq = 0;
	pxack = 0;
	pclog = -1;
	pibusy = 1;
	pxmode1 = 0;
	pxmode2 = 0;
	pxcmode = 0;
	pxcode = 0;
	pxctry = 5;
	pxcloc = 0;
	pxnpiu = 1;
	pxtcpu = 0;
	pxprom = 0;
	pxbanks = 0;
	pxbank = 0;
	pxnrom = 0;
	ppxdl0 = PIC_PPD0;
	ppxdl1 = PIC_PPD1;
	ppxdl2 = PIC_PPD2;
	pxrtime = PIC_RT;
	pxtintl = PIC_TI;
	pxnfile = 0;
	pxcfile = 0;
	pxaiid = 0;
	pxaibchr = 0;
	pxhints = 0;
	pxnotot = 0;
	pxdelay = 1;
	pxexitv = PI_SUCCESS;
	pxmodei = MO_EMU;
	pxmodeo = MO_EMU;
	piflags = PiMU;
	pxflags = POVF | POMP | POAR;
	pxcline[0] = '\0';
	for (i=0; i<PIC_NC; i++)
		{
		pxconfig[i].start = 0;
		pxconfig[i].end = 0;
		pxconfig[i].max = 0;
		pxconfig[i].lptr = -1;
		pxconfig[i].lct = 0;
		pxconfig[i].words = 0;
		pxconfig[i].flags = 0;
		pxconfig[i].next = &pxconfig[i+1];
		}
	pxconfig[PIC_NC-1].next = (PICONFIG *)0;
	pxfcfg = &pxconfig[0];
	pxcfg = (PICONFIG *)0;
	pxpcfg = (PICONFIG *)0;
	pxaltcfg.start = 0;
	pxaltcfg.end = 0;
	pxaltcfg.max = 0;
	pxaltcfg.lptr = -1;
	pxaltcfg.lct = 0;
	pxaltcfg.words = 0;
	pxaltcfg.flags = 0;
	pxaltcfg.uid[0] = 0;
	pxaltcfg.next = (PICONFIG *)0;
	for (pxcrom=0; pxcrom<PIC_NR; pxcrom++)
		piinitr();
	pxcrom = 0;
	for (pxcfile=0; pxcfile<PIC_NF; pxcfile++)
		piinitf();
	pxdisp = PXVH | PXHI;
	for (i=0; i<PIC_KK; i++)
		pxkeys[i] = (char *)0;
	}

/* `piinitr` - initialize the current ROM structure */

void piinitr()
	{
	PIROM *rp;
	
	rp = &pxrom[pxcrom];
	rp->size = 0;
	rp->esize = 0;
	rp->ssize = 0;
	rp->amask = 0;
	rp->smask = 0;
	rp->fstart = 0;
	rp->fend = 0;
	rp->fsize = 1;
	rp->fdata = 0xFF;
	rp->flags = PRLP;
	rp->ver[0] = '\0';
	}

/* `piinitf` - initialize the current file structure */

void piinitf()
	{
	PIFILE *fp;
	
	fp = &pxfile[pxcfile];
	fp->type = 0;
	fp->skip = 0;
	fp->offset = 0;
	fp->saddr = 0;
	fp->eaddr = 0;
	fp->flags = 0;
	fp->pfcfg = (PICONFIG *)0;
	fp->name[0] = '\0';
	}
