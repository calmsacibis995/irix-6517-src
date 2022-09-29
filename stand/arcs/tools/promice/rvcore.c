/*	RvCore.c - Edit 0.2

	RomView Version 1.1a
	Copyright (C) 1990-2 Grammar Engine, Inc.
	All rights reserved
	
	NOTICE:  This software source is a licensed copy of Grammar Engine's
	property.  It is supplied to you as part of support and maintenance
	of some Grammar Engine products that you may have purchased.  Use of
	this software is strictly limited to use with such products.  Any
	other use constitutes a violation of this license to you.
*/

/*	 - RomView CORE functions:
		rcinit() - initialize the link to target
		rcedit() - edit target contents
		rcdump() - dump target contents
		rcstak() - dump target stack
		rcbreak() - save ROM contents to file
		rcexit() - shutdown PROMICE, unlink
*/
#ifdef PIRV
#include <stdio.h>
#include <fcntl.h>
#ifdef MSDOS
#include <io.h>
#endif

#include "piconfig.h"
#include "rvconfig.h"
#include "pistruct.h"
#include "rvstruct.h"
#include "rvscript.h"
#include "pierror.h"
#include "pidata.h"
#include "pidriver.h"
#include "pisyn.h"

#ifdef ANSI
extern void (*rvsynf[])(void);
extern void pi_sleep(short time);
extern void pi_beep(void);
extern void rvwrite(short rvcm);
extern void rvread(short rvcm);
extern void rvdodump(void);
extern void rvdodmpi(void);
extern void rvdoedit(void);
extern void rvdostak(void);
extern void rvdoregs(void);
#else
extern void (*rvsynf[])();
extern void pi_sleep();
extern void pi_beep();
extern void rvwrite();
extern void rvread();
extern void rvdodump();
extern void rvdodmpi();
extern void rvdoedit();
extern void rvdostak();
extern void rvdoregs();
#endif

/* `rcinit` - initialize the link to target */

void rcinit()
	{
	if (rvflags&RvUP)
		return;
	picmd(0,(char)(rxproto|PI_CX|CM_CINIT),3,
		(char)(rxlink>>16),(char)(rxlink>>8),(char)rxlink,0,0);
	if (pxrsp[PICM]&CM_FAIL)
		{
		pxerror = PGE_XNO;
		return;
		}

	if (rvflags&RvAS)
		{
		picmd(0,(char)(rxproto|PI_CW|CM_ASYNC|CM_NORSP),1,RV_HI,0,0,0,0);
		pirsp();
		}
	else
		picmd(0,(char)(rxproto|PI_CW),1,RV_HI,0,0,0,0);
			
	if (!(pxrsp[PICM]&CM_FAIL))
		{
		while (!pxerror)
			{
			if (rvflags&RvAS)
				{
				picmd(0,(char)(rxproto|PI_CR|CM_ASYNC|CM_NORSP),1,1,0,0,0,0);
				pirsp();
				}
			else
				picmd(0,(char)(rxproto|PI_CR),1,1,0,0,0,0);
			if (!(pxrsp[PICM]&CM_FAIL))
				{
				if (pxrsp[PIDT] != RV_EH)
					pxerror = PGE_XXF;
				else
					{
					rvflags |= RvUP;
					break;
					}
				}
			else
				{
				pi_sleep(1);
				continue;
				}
			}
		}
	else
		pxerror = PGE_XXF;
	}

/* `rcedit` - edit target contents */

void rcedit()
	{
	if (rvflags&RvUP)
		rvdoedit();
	else
		pxerror = PGE_XXX;
	}

/* `rcdump` - dump target contents */

void rcdump()
	{
	if (rvflags&RvUP)
		rvdodump();
	else
		pxerror = PGE_XXX;
	}

/* `rcdumpi` - dump target contents */

void rcdumpi()
	{
	if (rvflags&RvUP)
		rvdodmpi();
	else
		pxerror = PGE_XXX;
	}

/* `rcreadi` - Read PROMICE internal buffer */

void rcreadi()
	{
	picmd(0,(char)(rxproto|PI_CR),1,1,0,0,0,0);
	}
	
/* `rcstak` - dump stack */

void rcstak()
	{
	if (rvflags&RvUP)
		{
		rvdostak();
		}
	else
		pxerror = PGE_XXX;
	}

/* `rcregs` - dump or load target registers */

void rcregs()
	{
	short j,k;

	switch(pxtcpu)
		{
		case 68000:
			rxybc = 32;
			rvread(RV_DR);
			if (!pxerror)
				{
				for (j=0; j<8; j++)
					{
					for (k=0; k<4; k++)
						printf("%02x",rxybf[j*4+k]&0xFF);
					printf(" ");
					}
				}
			printf("\n");
			rxybc = 32;
			rvread(RV_AR);
			if (!pxerror)
				{
				for (j=0; j<8; j++)
					{
					for (k=0; k<4; k++)
						printf("%02x",rxybf[j*4+k]&0xFF);
					printf(" ");
					}
				}
			break;
		case 8051:
			rxybc = 14;
			rvread(RV_DR);
			if (!pxerror)
				{
				printf("\na-%02X b-%02X dptr-%02X%02X sp-%02X psw-%02X",
					rxybf[2]&0x0FF,rxybf[6]&0x0FF,rxybf[3]&0x0FF,rxybf[4]&0x0FF,
					rxybf[0]&0x0FF,rxybf[1]&0x0FF);
				printf("\nr0-%02X r1-%02X r2-%02X r3-%02X r4-%02X r5-%02X r6-%02X r7-%02X",
					rxybf[5]&0x0FF,rxybf[7]&0x0FF,rxybf[8]&0x0FF,rxybf[9]&0x0FF,
					rxybf[10]&0x0FF,rxybf[11]&0x0FF,rxybf[12]&0x0FF,rxybf[13]&0x0FF);
				}
			break;
		}
	}

/* `rcexit` - shutdown the link to PROMICE */

void rcexit()
	{
	if (rvflags&RvUP)
		picmd(0,(char)(rxproto|PI_CM|CM_CHANGE),1,0,0,0,0,0);
	rvflags &= ~RvUP;
	}
#endif
