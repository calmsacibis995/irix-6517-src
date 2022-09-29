/*	RVShell.c - Edit 0.2

	RomView Version 1.1a
	Copyright (C) 1990-2 Grammar Engine, Inc.
	All rights reserved
	
	NOTICE:  This software source is a licensed copy of Grammar Engine's
	property.  It is supplied to you as part of support and maintenance
	of some Grammar Engine products that you may have purchased.  Use of
	this software is strictly limited to use with such products.  Any
	other use constitutes a violation of this license to you.
*/

/*	 - RemoteView shell: This shell uses either PiCOM or AiCOM protocols
	as implemented in the PROMICE units to talk to the RemoteView minimal
	ROM monitor.  It accomplishes several debugging commands to help debug
	target systems of various types.
*/
#ifdef PIRV
#include <stdio.h>
#include <string.h>

#include "piconfig.h"
#include "pistruct.h"
#include "piscript.h"
#include "rvconfig.h"
#include "rvstruct.h"
#include "rvscript.h"
#include "pierror.h"
#include "rvhelp.h"

#ifdef ANSI
extern void (*rvsynf[])(void);
extern void (*psynf[])(void);
extern char *getenv(char *var);
extern void rcexit(void);
extern void rvinit(void);
extern void rvatbpt(void);

void rvshell(void)
#else
extern void (*rvsynf[])();
extern void (*psynf[])();
extern char *getenv();
extern void rcexit();
extern void rvinit();
extern void rvatbpt();

void rvshell()
#endif
	{
	char *cstr;
	FILE *cf;
	char first = '\0';
	
	rvinit();
	cstr = getenv("ROMVIEW");
	if (cstr == NULL)
		cstr = "romview.ini";
#ifdef MACAPPL
		cstr = "\0";
#endif
	if (pxdisp&PXHI)
			printf("\nOpening ROMview config file '%s'",cstr);
	if ((cf = fopen(cstr,"r")) != NULL)
		{
		while (fgets(pxuline,PIC_CL,cf) != NULL)
			{
			if (pxuline[0] == '*')
				continue;
			pisyn(pxuline,rvusyn,rvsynf);
			if (pxerror)
				{
				pierror();
				(void)fclose(cf);
				}
			}
		(void)fclose(cf);
		}
	else
		{
		if (pxdisp&PXHI)
			printf("\nFile not found '%s' - proceeding",cstr);
		}
	for(;;)
		{
		if (rvflags&RvBP)
			{
			if (pxdisp&PXVH)
				printf("\nROMView: ");
			piflags &= ~PiCR;
			if (gets(pxuline))
				{
#ifdef MAC
				if (pxuline[0] == 'R' && pxuline[7] == ':')
					{
					if (!strlen(&pxuline[9]))
						{
						pxuline[9] = first;
						piflags |= PiCR;
						}
					strcpy(pxuline, &pxuline[9]);
					}
#endif
				if (!strlen(pxuline))
					{
					pxuline[0] = first;
					piflags |= PiCR;
					}
				first = pxuline[0];
				if (pxuline[0] == 'x')
					{
					break;
					}
				if (pxuline[0] == '!')
					{
					piflags |= PiII;
					pxuline[0] = ' ';
					pisyn(pxuline,pusrsyn,psynf);
					}
				else
					{
					piflags &= ~PiII;
					pisyn(pxuline,rvusyn,rvsynf);
					}
				if (pxerror)
					pierror();
				}
			}
		if ((rvflags&RvGO) && !(rvflags&RvBP))
			{
			rvatbpt();
			if (!(rvflags&RvGO))
				printf("\nUser program terminated");
			}
		if (!(rvflags&RvGO))
			piflags &= ~PiNT;
		}
	rcexit();
	}
#endif
