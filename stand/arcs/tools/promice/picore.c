/*	PiCore.c - Edit 6

	LoadICE Version 3
	Copyright (C) 1990-95 Grammar Engine, Inc.
	All rights reserved
	
	NOTICE:  This software source is a licensed copy of Grammar Engine's
	property.  It is supplied to you as part of support and maintenance
	of some Grammar Engine products that you may have purchased.  Use of
	this software is strictly limited to use with such products.  Any
	other use constitutes a violation of this license to you.
*/

/*	 - LoadICE CORE functions:
		pcinit() - initialize the link to PROMICEs
		pcfill() - fill ROM
		pcedit() - edit ROM contents
		pcdump() - dump ROM contents
		pcsave() - save ROM contents to file
		pcksum() - checksum ROM etc.
		pcmove() - move ROM contents
		pcrstt() - reset target system
		pcexit() - shutdown PROMICE, unlink
*/

#include <stdio.h>
#include <fcntl.h>

#ifdef MSDOS
#include <io.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif

#include "piconfig.h"
#include "pistruct.h"
#include "piscript.h"
#include "pierror.h"
#include "pidata.h"
#include "pidriver.h"
#include "pisyn.h"

#ifdef ANSI
extern void (*psynf[])(void);
extern void pifixcfg(void);
extern void pifile(void);
extern void piconfig(short code);
extern void piread(void);
extern void piwrite(void);
extern void pidoedit(void);
extern void pidodump(void);
extern void pi_beep(void);
#ifdef FASTP
extern void pi_clear(void);
#endif

void pcedit(void);
void pcfill(void);
void pcxfill(void);
void pcxksum(void);
void pcxinit(void);
static void filler(short code);
#else
extern void (*psynf[])();
extern void pifixcfg();
extern void pifile();
extern void piconfig();
extern void piread();
extern void piwrite();
extern void pidoedit();
extern void pidodump();
extern void pi_beep();
#ifdef FASTP
extern void pi_clear();
#endif

void pcedit();
void pcfill();
void pcxfill();
void pcxksum();
void pcxinit();
static void filler();
#endif

/* `pcinit` - initialize the link to PROMICE */

void pcinit()
	{
	short i,j;
	short hicode = 1;
	PIROM *pr;
	
	if (piflags&PiUP)
		return;
	piflags &= ~PiFP;
	pxprom = 0;
	if (pxdisp&PXHI)
		printf("\n\nConnecting.. Please WAIT..");
	pxerror = pi_open();
	if (pxerror)
		return;
	if (pxdisp&PXHI)
		printf("\n Connection established");
	piflags |= PiUP;
	if (!(pxlink.flags&PLPQ))
		piflags |= PiSO;
	pxmode1 &= ~MO_LOAD;
	if (pxmodei)
		pxmode1 |= MO_LOAD;
	/* check PROMICE memory size */
	for (i=0; i<pxprom; i++)
		{
		pr = &pxrom[i];
		if (pxlink.flags&PLPB)
			picmd((char)i,PI_MO,1,pxmode1|MO_PPXO,0,0,0,0);
		else
			picmd((char)i,PI_MO,1,pxmode1,0,0,0,0);
		if (pxmodei)
			piflags &= ~PiMU;
		if (pxerror)
			return;
		pr->size = pxroms[pxrsp[PIDT]&0x0f];
		if (!(pr->flags & PRMI))
			{
			pr->mid = i;
			pr->sid = i;
			pr->flags |= PRMI;
			if (pxrsp[PIDT]&0x0f0)
				{
				pr->sid = i + 1;
				(pr+1)->sid = i + 1;
				(pr+1)->mid = i;
				(pr+1)->flags |= PRMI;
				}
			}
		if (!pr->size)		/* damn! size is zero */
			{
			pxerror = PGE_PWR;
			return;
			}
		if (pr->esize > pr->size)
			{
			if (pxdisp&PXHI)
				{
				pi_beep();
				printf("\n WARNING! unit size smaller than emulation size!");
				printf("\n  call 1-800-PROMICE for an upgrade!!");
				}
			pr->esize = pr->size;
			}
		if (!pr->esize)	/* default emulation size, when not given */
			pr->esize = pr->size;
		if (!pr->ssize)
			pr->ssize = pr->esize;
		pr->amask = PIC_MM - pr->ssize;
		picmd((char)i,PI_VS,1,0,0,0,0,0);	/* get MicroCode version# */
		if (pxerror)
			return;
		pxlink.flags &= ~PLST;
		for (j=0; j<4; j++)
			pr->ver[j] = pxrsp[PIDT+j];
		if (pr->ver[0] <= '2')
			pr->flags |= PRRB;				/* it's a boy! */
		if (pr->ver[0] >= '8')
			{
			pxmode1 |= MO_XTND;
			pr->res = pxrsp[PIDT+4];
			pr->flags |= PRHI;
			}
		if (pr->ver[0] < '6')
			{
			pxflags |= POSP;
			pr->flags |= PRLO;
			hicode = 0;
			piflags |= PiZZ;
			}
		if (pr->ver[0] < '8')
			piflags |= PiLO;
		if (pr->ver[0] < '7')
			{
			pr->smask = 0x000000;
			if (pxlink.flags&PLHI)
				pxmode1 |= MO_FAST;
			}
		else
			{ /* Rev 3.x unit */
			pxlink.flags &= ~PLHI;		/* don't need this ever */
			if (pxlink.flags&PLOW)
				pxmode1 |= MO_SLOW;
			if ((pr->ver[2] == '1') &&
					((pr->ver[3] == 'a') || (pr->ver[3] == 'A')))
				pr->smask = 0x000000;
			else
				{
				if (pr->size <= 131072)
					pr->smask = 0x060000;
				else
					{
					if (pr->size == 262144)
						pr->smask = 0x040000;
					else
						pr->smask = 0x000000;
					}
				}
			}
		}
	if (pxlink.flags&PLPB && piflags&PiLO)
		{
		pxerror = PGE_UNF;
		return;
		}
	if (pxnrom > pxprom)
		{
		pxerror = PGE_BCF;
		return;
		}
	pxnrom = pxprom;
	if (!pxcfg)	/* if no config */
		{
		sprintf(pxuline,"word=8");	/* fake a 'word' command */
		pisyn(pxuline,pcmfsyn,psynf);
		}
	else
		{
		if (piflags&PiNL)
			{
			piflags &= ~PiNL;
			sprintf(pxuline,"word=%d",pxcfg->words*8);
			pisyn(pxuline,pcmfsyn,psynf);
			}
		}
	for (i=0; i<pxnfile; i++)	/* fix any file configs */
		{
		pxcfg = pxfile[i].pfcfg;
		if (pxcfg)
			pifixcfg();
		}
	pxcfg = pxpcfg;
	pifixcfg();
	if (pxdisp&PXHI)
		piconfig(PcROM);
	if (pxerror)
		return;
	if (hicode && !(pxflags&POSP) && pxlink.flags&(PLPQ|PLPP))
		{
		for (i=0; i<pxprom; i++)
			picmd((char)i,PI_MO|CM_NORSP,1,pxmode1|MO_PPGO,0,0,0,0);
		piflags |= PiFP;
		}
#ifdef FASTP
	picmd(0,PI_MO,1,pxmode1,0,0,0,0);
#endif
	piflags &= ~PiSO;
	if ((!pxflags&POIX) && piflags&PiFM)	/* if fill reqired */
		pcfill();
	if ((!pxnfile) && (piflags&PiEL))	/* if no files then do edit - maybe */
		pcedit();
	pcxinit();
	}

/* `pcxinit` - initilaize target control signals now */

void pcxinit()
	{
	/* program the HSO interrupt polarity now, also REQ and ACK */
	if (piflags&PiHO)
		{
		switch(pxhso)
			{
			case 0:
				pxcmode &= ~MC_INTH;
				picmd(0,PI_CM|CM_PICOM|CM_CHANGE|CM_NORSP,1,pxcmode,0,0,0,0);
				break;
			case 1:
				pxcmode |= MC_INTH;
				picmd(0,PI_CM|CM_PICOM|CM_CHANGE|CM_NORSP,1,pxcmode,0,0,0,0);
				break;
			case 2:
				pxcmode |= MC_INTH;
				picmd(0,PI_CM|CM_PICOM|CM_CHANGE|CM_NORSP,1,pxcmode,0,0,0,0);
				pxcmode &= ~MC_INTH;
				picmd(0,PI_CM|CM_PICOM|CM_CHANGE|CM_NORSP,1,pxcmode,0,0,0,0);
				pxcmode |= MC_INTH;
				picmd(0,PI_CM|CM_PICOM|CM_CHANGE|CM_NORSP,1,pxcmode,0,0,0,0);
				break;
			case 5:
				pxcmode &= ~MC_INTH;
				picmd(0,PI_CM|CM_PICOM|CM_CHANGE|CM_NORSP,1,pxcmode,0,0,0,0);
				pxcmode |= MC_INTH;
				picmd(0,PI_CM|CM_PICOM|CM_CHANGE|CM_NORSP,1,pxcmode,0,0,0,0);
				pxcmode &= ~MC_INTH;
				picmd(0,PI_CM|CM_PICOM|CM_CHANGE|CM_NORSP,1,pxcmode,0,0,0,0);
				break;
			case 10:
				pxcmode |= MC_INTH;
				picmd(0,PI_CM|CM_PICOM|CM_CHANGE|CM_NORSP,1,pxcmode,0,0,0,0);
				pxcmode &= ~MC_INTH;
				picmd(0,PI_CM|CM_PICOM|CM_CHANGE|CM_NORSP,1,pxcmode,0,0,0,0);
				pi_sleep(1);
				pxcmode |= MC_INTH;
				picmd(0,PI_CM|CM_PICOM|CM_CHANGE|CM_NORSP,1,pxcmode,0,0,0,0);
				break;
			case 13:
				pxcmode &= ~MC_INTH;
				picmd(0,PI_CM|CM_PICOM|CM_CHANGE|CM_NORSP,1,pxcmode,0,0,0,0);
				pxcmode |= MC_INTH;
				picmd(0,PI_CM|CM_PICOM|CM_CHANGE|CM_NORSP,1,pxcmode,0,0,0,0);
				pi_sleep(1);
				pxcmode &= ~MC_INTH;
				picmd(0,PI_CM|CM_PICOM|CM_CHANGE|CM_NORSP,1,pxcmode,0,0,0,0);
				break;
			default:
				pxerror = PGE_BAA;
				return;
			}
		}	
	if (piflags&PiRQ)
		{
		if (pxreq)
			pxcmode |= MC_REQH;
		else
			pxcmode &= ~MC_REQH;
		if (pxack)
			pxcmode |= MC_ACKH;
		else
			pxcmode &= ~MC_ACKH;
		if (pxcmode&MC_EXINL)
			picmd(0,PI_CM|CM_PICOM|CM_CHANGE|CM_NORSP,2,pxcmode,pxtintl,0,0,0);
		else
			picmd(0,PI_CM|CM_PICOM|CM_CHANGE|CM_NORSP,1,pxcmode,0,0,0,0);
		pxcmode &= ~MC_EXINL;
		}
		if (piflags&PiMO)
			{
			if (pxrom[0].flags&PRHI)
				picmd(0,PI_MO|CM_NORSP,2,pxmode1|MO_MORE,pxmode2,0,0,0);
			else
				picmd(0,PI_MO|CM_NORSP,1,pxmode1,0,0,0,0);
			}
	if ((piflags&PiAB) && (pxrom[0].res&AIS31))
		{
		picmd(0,PI_MO|CM_NORSP|CM_AIREG,
			2,AI_MASTRW|AI_BITCHG,pxwrite&1,0,0,0);
		picmd(0,PI_MO|CM_NORSP|CM_AIREG,
			2,AI_SLAVEW|AI_BITCHG,pxwrite&2,0,0,0);
		picmd(0,PI_MO|CM_NORSP|CM_AIREG,
			2,AI_BURST0|AI_BITCHG,!(pxburst&1),0,0,0);
		picmd(0,PI_MO|CM_NORSP|CM_AIREG,
			2,AI_BURST1|AI_BITCHG,!(pxburst&2),0,0,0);
		picmd(0,PI_MO|CM_NORSP|CM_AIREG,
			2,AI_DELAY0|AI_BITCHG,!(pxaieco&1),0,0,0);
		picmd(0,PI_MO|CM_NORSP|CM_AIREG,
			2,AI_DELAY1|AI_BITCHG,!(pxaieco&2),0,0,0);
		}
	piflags &= ~(PiHO|PiRQ|PiMO|PiAB);
	}

/* `pcfill` - fill ROM space */

void pcfill()
	{
	long i,tct,w,b,d;

	if (pxcfg->flags & PCFL)	/* if filling config or partial ROM */
		{
		if ((pxfstart>pxfend) || (pxfstart<0) || (pxfend<0))
			{
			pxerror = PGE_BAA;
			return;
			}
		if ((pxfstart>pxmax) || (pxfend>pxmax))
			{
			pxerror = PGE_AOR;
			return;
			}
		if (pxdisp&PXHI)
			{
			printf("\nFilling config from 0x%lX to 0x%lX fill char = 0x%lX",
				pxfstart, pxfend, pxfdata);
			if (pxfsize > 4)
				printf(" %lX",pxfdata2);
			if (pxdisp&PXMH)
				piconfig(PcCFG);
			}
		tct = pxfend - pxfstart + 1;
		pxxloc = pxfstart;
		pxdlc = 0;
		while (tct && !pxerror)		/* keep going till done or error */
			{
			w = pxfsize;
			if (w < 5)
				d = 1;
			else
				d = w-3;
			if (tct > PIC_BS)
				pxxbc = PIC_BS;
			else
				pxxbc = tct;
			if (pxxbc > w)
				pxxbc -= pxxbc%w;
			tct -= pxxbc;
			for (i=0; i<pxxbc; i+=w)
				{
				for (b=0; b<w; b++)
					{
					if (b<4)
						pxxbf[i+b] =(char)(pxfdata >> (w-d-b)*8);
					else
						pxxbf[i+b] =(char)(pxfdata2 >> (w-b-1)*8);
					}
				}
			piwrite();
			pxxloc += pxxbc;
			}
		if (!pxerror && (pxdisp&PXHI))
			printf("\nTransferred %ld (0x%lX) fill characters",pxdlc,pxdlc);
		if (!(piflags&PiFM))
			pxcfg->flags &= ~PCFL;
		}
	else
		pcxfill();
	}

/* `pcxfill` - fill whole ROMs */

void pcxfill()
	{
	long i,j,tct,w,b,d;
	PIROM *rp;
	PICONFIG *tcfg;

	tcfg = pxcfg;
	for (i=0; i<pxprom; i++)
		{
		rp = &pxrom[i];
		if (rp->flags&PRFL)
			{
			if (pxdisp&PXHI)
				{
				if (rp->fsize == 1)
					printf("\nFilling ROM ID = %ld char = 0x%02X"
						,i,rp->fdata&0x0ff);
				else
					{
					printf("\nFilling ROM ID = %ld data = 0x%lX",i,rp->fdata);
					if (rp->fsize > 4)
						printf(" %lX",rp->fdata2);
					}
				}
			if (rp->ver[0] >= '5' && rp->fsize == 1 && 
				(rp->fstart == rp->fend)) /* can MicroCode do it? */
				{
				if (pxdisp&PXHI)
					printf(" (via uCode)");
				pxmode1 |= MO_LOAD;
				if (piflags&PiPH)
					{
					picmd((char)i,PI_MO,1,pxmode1,0,0,0,0);
					if (pxrsp[PIDT] == PIC_LOCK)
						{
						pxerror = PGE_LOK;
						return;
						}
					}
				else
					picmd((char)i,PI_MO|CM_NORSP,1,pxmode1,0,0,0,0);
				piflags |= PiNT;
				picmd((char)i,PI_TS|CM_FILLC,1,(char)pxrom[i].fdata,0,0,0,0);
				piflags &= ~PiNT;
				}
			else		/* else fill brute force */
				{
				pxcfg = &pxaltcfg;
				pxcfg->words = 1;
				pxcfg->uid[0] = (short)i;
				pifixcfg();
				if (rp->fstart == rp->fend)
					{
					tct = rp->esize;
					pxxloc = pxcfg->start;
					}
				else
					{
					tct = rp->fend - rp->fstart + 1;
					pxxloc = rp->fstart;
					}
				pxdlc = 0;
				while (tct && !pxerror)
					{
					w = rp->fsize;
					d = w - 3;
					if (w < 5)
						d = 1;
					else
						d = w-3;
					if (tct > PIC_BS)
						pxxbc = PIC_BS;
					else
						pxxbc = tct;
					if (pxxbc > w)
						pxxbc -= pxxbc%w;
					tct -= pxxbc;
					for (j=0; j<pxxbc; j+=w)
						{
						for (b=0; b<w; b++)
							{
							if (b<4)
								pxxbf[j+b] =(char)(rp->fdata >> (w-d-b)*8);
							else
								pxxbf[j+b] =(char)(rp->fdata2 >> (w-b-1)*8);
							}
						}
					piwrite();
					pxxloc += pxxbc;
					}
				if (!pxerror && (pxdisp&PXHI))
					printf("\nTransferred %ld (0x%lX) fill characters",
						pxdlc,pxdlc);
				}
			if (!(piflags&PiFM))
				rp->flags &= ~PRFL;
			}
		}
	pxcfg = tcfg;
	}
					
/* `pcedit` - edit ROM contents */

void pcedit()
	{
	extern void pidoedit();

	pidoedit();		/* in PIEDIT */
	}

/* `pcdump` - dump ROM contents */

void pcdump()
	{
	extern void pidodump();
	
	pidodump();		/* in PIEDIT */
	}

/* `pcsave` - save ROM contents */

void pcsave()
	{
	long sct;
	int ref,rc;
	
	if ((pxsstart<0) || (pxsstart>pxsend) || (pxsend<0) )
		{
		pxerror = PGE_BAA;
		return;
		}
	if ((pxsstart>pxmax) || (pxsend>pxmax))
		{
		pxerror = PGE_AOR;
		return;
		}
#ifdef MSDOS
	if ((ref = open(ps_str,O_WRONLY|O_CREAT|O_TRUNC|O_BINARY,S_IWRITE)) >= 0)
#else
	if ((ref = open(ps_str,O_WRONLY|O_CREAT|O_TRUNC,0xFFFF)) >= 0)
#endif
		{
		sct = pxsend - pxsstart + 1;
		if (pxdisp&PXHI)
			printf("\nSaving to file '%s' (data count = %ld)",ps_str,sct);
		pxyloc = pxsstart;
		while(sct)
			{
			if (sct > PIC_BS)
				pxybc = PIC_BS;
			else
				pxybc = sct;
			sct -= pxybc;
			piread();
			if (!pxerror)
				{
				rc = write(ref,pxybf,(unsigned int)pxybc);
				if (rc <0)
					{
					pxerror = PGE_IOE;
					break;
					}
				}
			else
				break;
			pxyloc += pxybc;
			}
		close(ref);
		}
	else
		pxerror = PGE_OPN;
	}

/* `pcksum` - checksum config */

void pcksum()
	{
	long kct,ksum,i;
	
	if ((pxkstart<0)||(pxkend<0)||(pxkend<pxkstart)||(pxkstore<0)||(pxksize>4))
		{
		pxerror = PGE_BAA;
		return;
		}
	if ((pxkstart>pxmax) || (pxkend>pxmax) || (pxkstore>pxmax))
		{
		pxerror = PGE_AOR;
		return;
		}
	if (pxdisp&PXHI)
		printf("\n\nChecksumming Config, Start = 0x%lX End = 0x%lX",
			pxkstart,pxkend);
	kct = pxkend - pxkstart + 1;
	ksum = 0;
	pxyloc = pxkstart;
	while(kct)
		{
		if (kct > PIC_BS)
			pxybc = PIC_BS;
		else
			pxybc = kct;
		kct -= pxybc;
		piread();
		if (!pxerror)
			{
			for (i=0; i<pxybc; i++)
				{
				if (pxflags&POKA)
					ksum += pxybf[i];
				else
					ksum ^= pxybf[i];
				}
			}
		else
			break;
		pxyloc += pxybc;
		}
	if (!pxerror)
		{
		pxxloc = pxkstore;
		pxxbc = pxksize;
		if (pxflags&POKA)
			{
			if (pxflags&POK1)
				ksum = ~ksum;
			else
				ksum = -ksum;
			}
		else
			{
			if (pxflags&POK1)
				ksum = ~ksum;
			}
		if (pxdisp&PXHI)
			{
			printf("\n Storing %ld Bit Checksum @ 0x%lX",pxksize*8,pxxloc);
			if (pxksize > 1)
				{
				if (pxflags&POKO)
					printf(" - LOW Byte First");
				else
					printf(" - HIGH Byte First");
				}
			if (pxflags&POKA)
				{
				printf("\n  Checksum via addition, ");
				if (pxflags&POK1)
					printf("storing 1's comp = ");
				else
					printf("storing 2's comp = ");
				}
			else
				{
				printf("\n  Checksum via exclusive OR, storing ");
				if (pxflags&POK1)
					printf("1's comp = 0x");
				else
					printf("as is = 0x");
				}
			}
		if (pxflags&POKO)
			{
			for (i=0; i<pxksize; i++)
				pxxbf[i] = (char)(ksum>>(i*8));
			}
		else
			{
			for (i=pxksize-1; i>=0; i--)
				pxxbf[pxksize-1-i] = (char)(ksum>>(i*8));
			}
		if (pxdisp&PXHI)
			for (i=0; i<pxksize; i++)
				printf("%02X",pxxbf[i]&0x0ff);
		piwrite();
		}
	}

/* `pcxksum` - checksum ROMs */

void pcxksum()
	{
	char mo;
	long i,j;
	long kct,ksum;
	PICONFIG *tcfg;

	ksum = 0;
	tcfg = pxcfg;
	for (i=0; i<pxprom; i++)
		{
		if (pxrom[i].flags&PRCK)
			{
			if (pxdisp&PXHI)
				printf("\n\nChecksuming ROM ID = %d Start = 0x%lX End = 0x%lX",
					(short)i,pxrom[i].kstart,pxrom[i].kend);
			pxcfg = &pxaltcfg;
			pxcfg->words = 1;
			pxcfg->uid[0] = (short)i;
			pifixcfg();
			if (pxerror)
				return;
			kct = pxrom[i].kend - pxrom[i].kstart + 1;
			while(kct)
				{
				if (kct > PIC_BS)
					pxybc = PIC_BS;
				else
					pxybc = kct;
				kct -= pxybc;
				piread();
				if (!pxerror)
					{
					for (j=0; j<pxybc; j++)
						{
						if (pxrom[i].flags&PRKA)
							ksum += pxybf[j];
						else
							ksum ^= pxybf[j];
						}
					}
				else
					break;
				pxyloc += pxybc;
				}
			if (!pxerror)
				{
				pxxloc = pxrom[i].kstore;
				pxxbc = pxrom[i].ksize;
				if (pxrom[i].flags&PRKA)
					{
					if (pxrom[i].flags&PRK1)
						ksum = ~ksum;
					else
						ksum = -ksum;
					}
				else
					{
					if (pxrom[i].flags&PRK1)
						ksum = ~ksum;
					}
				if (pxdisp&PXHI)
					{
					printf("\n Storing %ld Bit Checksum @ 0x%lX",
						pxxbc*8,pxxloc);
					if (pxrom[i].ksize > 1)
						{
						if (pxrom[i].flags&PRKO)
							printf(" - LOW Byte First");
						else
							printf(" - HIGH Byte First");
						}
					if (pxrom[i].flags&PRKA)
						{
						printf("\n  Checksum via addition, ");
						if (pxrom[i].flags&PRK1)
							printf("storing 1's comp = 0x");
						else
							printf("storing 2's comp = 0x");
						}
					else
						{
						printf("\n  Checksum via exclusive OR, storing ");
						if (pxrom[i].flags&PRK1)
							printf("1's comp = 0x");
						else
							printf("as is = 0x");
						}
					}
				if (pxrom[i].flags&PRKO)
					{
					for (j=0; j<pxrom[i].ksize; j++)
						pxxbf[j] = (char)(ksum>>(j*8));
					}
				else
					{
					for (j=pxrom[i].ksize-1; j>=0; j--)
						pxxbf[pxrom[i].ksize-1-j] = (char)(ksum>>(j*8));
					}
				if (pxdisp&PXHI)
					for (j=0; j<pxrom[i].ksize; j++)
						printf("%02X",pxxbf[j]&0x0ff);
				piwrite();
				}
			pxcfg = tcfg;
			}
		}
	}

/* `pcmove` - move ROM data */

void pcmove()
	{
	long mct,i;
	
	if ((pxmstart<0) || (pxmend<0) || (pxmend<pxmstart) || (pxmdest<0))
		{
		pxerror = PGE_BAA;
		return;
		}
	if ((pxmstart>pxmax) || (pxmend>pxmax) || (pxmdest>pxmax))
		{
		pxerror = PGE_AOR;
		return;
		}
	mct = pxmend - pxmstart + 1;
	pxxloc = pxmdest;
	pxyloc = pxmstart;
	while(mct && !pxerror)
		{
		if (mct > PIC_BS)
			pxybc = PIC_BS;
		else
			pxybc = mct;
		mct -= pxybc;
		piread();
		if (!pxerror)
			{
			for (i=0; i<pxybc; i++)
				pxxbf[i] = pxybf[i];
			pxxbc = pxybc;
			piwrite();
			}
		pxxloc += pxxbc;
		pxyloc += pxybc;
		}
	}

/* `pcfind` - find stuff in memory */

void pcfind()
	{
	long here,tct,i,j,k,l;

	tct = pxlend - pxlstart + 1;
	pxyloc = pxlstart;
	here = k = 0;
	while (tct && !pxerror)
		{
		if (tct > PIC_BS)
			pxybc = PIC_BS;
		else
			pxybc = tct;
		tct -= pxybc;
		piread();
		if (!pxerror)
			{
			for (i=0; i<pxybc;)
				{
				for (j=k; j<pxlsize; j++)
					{
					if (pxybf[i++] == pxldata[j])
						{
						if ((i==pxybc) && (j<(pxlsize-1)))
							{
							k = j+1;
							break;
							}
						if (!here)
							here = pxyloc + i - 1;
						}
					else
						{
						here = 0;
						k = 0;
						break;
						}
					}
				if (!k && here)
					{
					printf("\n%08lX: ",here);
					for (j=0; j<pxlsize; j++)
						printf("%02X",pxldata[j]&0x0ff);
					here = 0;
					}
				}
			}
		pxyloc += pxybc;
		}
	}


/* `pcstring` - find string in memory */

void pcstring()
	{
	long here,tct,i,j,k,l;
	char *tsp;

	tct = pxlend - pxlstart + 1;
	pxyloc = pxlstart;
	here = k = 0;
	pxlsize = strlen(pxlstr);
	tsp = pxlstr;
	while (tct && !pxerror)
		{
		if (tct > PIC_BS)
			pxybc = PIC_BS;
		else
			pxybc = tct;
		tct -= pxybc;
		piread();
		if (!pxerror)
			{
			for (i=0; i<pxybc;)
				{
				for (j=k; j<pxlsize; j++)
					{
					if (pxybf[i++] == *tsp++)
						{
						if ((i==pxybc) && (j<(pxlsize-1)))
							{
							k = j+1;
							break;
							}
						if (!here)
							here = pxyloc + i - 1;
						}
					else
						{
						here = 0;
						k = 0;
						tsp = pxlstr;
						break;
						}
					}
				if (!k && here)
					{
					printf("\n%08lX: %s",here,pxlstr);
					here = 0;
					tsp = pxlstr;
					}
				}
			}
		pxyloc += pxybc;
		}
	}


/* `pcrstt` - reset target system */

#ifdef ANSI
void pcrstt(short id, short tm)
#else
void pcrstt(id, tm)
short id,tm;
#endif
	{
	picmd((char)id,PI_RT,1,(char)tm,0,0,0,0);
	}

/* `pcexit` - shutdown the link to PROMICE */

void pcexit()
	{
	short i;
	char bc=0;
	unsigned long aip;
	
	if (!(piflags&PiUP))
		{
		pi_close();
		return;
		}
#ifdef FASTP
	pi_clear();
#endif
	pxmode1 &= ~MO_LOAD;
	if (pxlink.flags&PLPB)
		piflags |= PiSO;
	if (pxmodeo)
		{
		piflags &= ~PiMU;
		pxmode1 |= MO_LOAD;
		}
	if (piflags&PiBC)
		bc = 1;
	if (pxrom[0].ver[0] < '6' && piflags&PiAI)
		{
		piflags &= ~PiAI;
		if (pxdisp&PXHI)
			{
			printf("\n\7Unit is too old to do transparant mode AI link");
			printf("\n call 1-800-PROMICE for an upgrade!");
			}
		}
	if (piflags&PiAI && !(piflags&PiER))
		{
		if (!pxaiws)
			pxaiws = pxcfg->words;
		aip = (pxailoc/pxaiws) | 0x03 |
				pxrom[pxaiid].smask | pxrom[pxaiid].amask;
		for (i=pxprom-1; i>=0; i--)	/* make sure emulating and set params */
			{
			picmd((char)i,PI_MO,1,pxmode1,0,0,0,0);
			picmd((char)i,PI_MO|CM_AITTY,3,
				bc,pxaibchr,(char)pxhints,0,0);
			}
		for (i=pxprom-1; i>=0; i--)
			{
			if (i==pxaiid)	/* this one get the AI paramters */
				{
				if (pxdisp&PXHI)
					{
					printf("\n PROMICE ID-%d putting AI in transparant mode",i);
					printf("\n AILOC = 0x%lX (0x%lX)\n HostLink - ",pxailoc,aip);
					if (piflags&PiXP)
						printf("parallel port");
					else
						printf("serial port @ %ld baud",pxaibr);
					if (bc)
						printf("\n BreakCharacter = 0x%02X",pxaibchr);
					else
						printf("\n Binary transparency - no breakCharacter");
					printf("\n Host interrupts (DTR/INIT toggles) to ignore: ");
					if (pxhints == 0x0ff)
						printf("ALL");
					else
						printf("%d",pxhints);
					}
				picmd((char)pxaiid,PI_RS|CM_NORSP|CM_CINIT,
					5,(char)PIC_XCODE,(char)(aip>>16),
					(char)(aip>>8),(char)aip,(char)pxaibrc);
				}
			else			/* others just pass thru data */
				picmd((char)i,PI_RS|CM_NORSP,1,(char)(pxaibrc|0x080),0,0,0,0);
			}
		}
	else		/* standard stuff, just emulate and restore units */
		{
		for (i=pxprom-1; i>=0; i--)
			{
			picmd((char)i,PI_MO | CM_NORSP,1,pxmode1,0,0,0,0);
			picmd((char)i,PI_RS | CM_NORSP,1,0,0,0,0,0);
			}
		}
	if (pxerror)
		pierror();
	pxprom = 0;
	pxerror = pi_close();
	}
