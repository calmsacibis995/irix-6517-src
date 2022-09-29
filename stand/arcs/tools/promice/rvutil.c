/*	RvUtil.c - Edit 0.2

	RomView Version 1.1a
	Copyright (C) 1990-2 Grammar Engine, Inc.
	All rights reserved
	
	NOTICE:  This software source is a licensed copy of Grammar Engine's
	property.  It is supplied to you as part of support and maintenance
	of some Grammar Engine products that you may have purchased.  Use of
	this software is strictly limited to use with such products.  Any
	other use constitutes a violation of this license to you.
*/

/*	 - RomView utility functions:
		rvwrite() - write data to target
		rvread() - read data from target
		rvlop() - load new pointers
*/
#ifdef PIRV
#include <stdio.h>

#include "piconfig.h"
#include "pistruct.h"
#include "rvconfig.h"
#include "rvstruct.h"
#include "pierror.h"
#include "pidriver.h"
#include "rvdata.h"

#ifdef ANSI
extern void pi_cccw(void);
extern void pi_ccw(void);
void rvlop(long lptr);
#else
extern void pi_cccw();
extern void pi_ccw();
void rvlop();
#endif

/* `rvwrite` - write `rxxbf` per `rxxloc` and `rxxbc` */

#ifdef ANSI
void rvwrite(short rvcm)
#else
void rvwrite(rvcm)
short rvcm;
#endif
	{
	long tct,i,boff=0;

	if (pxdisp&PXMI)
		printf("\nRvWrite: loc=%08lX ct=%ld",rxxloc,rxxbc);

	tct = rxxbc;
	if ((rvcm&0x0F) == RV_WR)
		rvlop(rxxloc);
	while (tct && !pxerror)
		{
		pxcmd[PIID] = 0;
		if (rvflags&RvAS)
			pxcmd[PICM] = (char)(rxproto|PI_CW|CM_ASYNC|CM_NORSP);
		else
			pxcmd[PICM] = (char)(rxproto|PI_CW);
		pxcmd[PIDT] = (char)rvcm;
		if (rxxbc > RVC_XC)
			rxxbc = RVC_XC;
		tct -= rxxbc;
		for (i=0; i<rxxbc; i++)
			pxcmd[i+4] = rxxbf[i+boff];
		pxcmd[PICT] = (char)(rxxbc+1);
		pxcmdl = rxxbc+4;
		if (pi_cmd() == PGE_NOE)
			{
			if (rvflags&RvAS)
				pirsp();
			if (pxrsp[PICM]&CM_FAIL)
				pxerror = PGE_XXF;
			}
		boff += rxxbc;
		rxxbc = tct;
		pi_ccw();
		}
	}

/* `rvread` - read `rxybf` per `rxyloc` and `rxybc` */

#ifdef ANSI
void rvread(short rvcm)
#else
void rvread(rvcm)
short rvcm;
#endif
	{
	long tct,i,boff=0;
			
	if (pxdisp&PXMI)
		printf("\nRvRead: loc=%08lX ct=%ld",rxyloc,rxybc);

	rvflags &= ~RvIB;
	tct = rxybc;
	if ((rvcm&0x0F) == RV_RD)
		rvlop(rxyloc);
	while (tct && !pxerror)
		{
		rxybc = tct;
		if (rxybc > RVC_XC)
			rxybc = RVC_XC;
		if (rvflags&RvAS)
			{
			picmd(0,(char)(rxproto|PI_CW|CM_ASYNC|CM_NORSP),2,(char)rvcm,(char)rxybc,0,0,0);
			pirsp();
			}
		else
			picmd(0,(char)(rxproto|PI_CW),2,(char)rvcm,(char)rxybc,0,0,0);
		if (pxrsp[PICM]&CM_FAIL)
			pxerror = PGE_XXF;
		else
			{
			while (!pxerror)
				{
				if (rvflags&RvAS)
					{
					picmd(0,(char)(rxproto|PI_CR|CM_ASYNC|CM_NORSP),1,0,0,0,0,0);
					pirsp();
					}
				else
					picmd(0,(char)(rxproto|PI_CR),1,0,0,0,0,0);
				if (pxrsp[PICM]&CM_FAIL)
					{
					if (pxrsp[PIDT] == (char)(0xFB))
							{
							pi_sleep(10);
							continue;
							}
					else
						pxerror = PGE_XXF;
					}
				else
					{
					for (i=0; i<(long)(pxrsp[PICT]); i++)
						rxybf[i+boff] = pxrsp[PIDT+i];
					rxybc = (long)pxrsp[PICT];
					tct -= rxybc;
					boff += rxybc;
					break;
					}		
				}
			}
		pi_cccw();
		}
	rxybc = boff;
	}

/* `rvlop` - load target's pointer */

#ifdef ANSI
void rvlop(long tlp)
#else
void rvlop(tlp)
long tlp;
#endif
	{
	if (rvflags&RvAS)
		{
		picmd((char)0,(char)(rxproto|PI_CW|CM_ASYNC|CM_NORSP),4,RV_LP,
			(char)(tlp>>16),(char)(tlp>>8),(char)tlp,0);
		pirsp();
		}
	else
		picmd((char)0,(char)(rxproto|PI_CW),4,RV_LP,
			(char)(tlp>>16),(char)(tlp>>8),(char)tlp,0);
	if (pxrsp[PICM]&CM_FAIL)
		pxerror = PGE_XXF;
	}

/* `rvinit` - init RomView globals */

void rvinit()
	{
	long i;
	
	rxwords = 1;
	rxflags = 0;
	rxnbrk = 0;

	for (i=0; i<RVC_NBRK; i++)
		rxbreak[i].flags = 0;

	rvflags = RvBP;
	rvjump = RVC_JUMP;
	rvcall = RVC_CALL;
	rvpcts = RVC_PCTS;
	rvstpc = RVC_STPC;
	rvmtac = RVC_MTAC;
	rvactm = RVC_ACTM;
	}
#endif
