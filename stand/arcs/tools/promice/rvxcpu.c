/*	Rvxcpu.c - Edit 0.2

	RomView Version 1.0
	Copyright (C) 1990-2 Grammar Engine, Inc.

	NOTICE:  This software source is a licensed copy of Grammar Engine's
	property.  It is supplied to you as part of support and maintenance
	of some Grammar Engine products that you may have purchased.  Use of
	this software is strictly limited to use with such products.  Any
	other use constitutes a violation of this license to you.
*/

/*	 - Target space - Edit/Display module
*/
#ifdef PIRV
#include "rvxcpu.h"

/* `rvgetb` - get a byte from internal RAM */

#ifdef ANSI
char rvgetb(long tlp)
#else
char rvgetb(tlp)
long tlp;
#endif
	{
	return(0);
	}

/* `rvdodmpi` - dump target instruction space per user request */

void rvdodmpi()
	{
	}

/* `rvdosfr` - process a special display request */

void rvdosfr()
	{
	}

/* `rvdostep` - step a few instructions */

void rvdostep()
	{
	}

/* `rvdoxeq` - execute an instruction */

void rvdoxeq()
	{
	}

/* `rvsetbrk` - set a break point */

void rvsetbrk()
	{
	}

/* `rvclrbrk` - clr a break point */

void rvclrbrk()
	{
	}

/* `rvgouser` - execute user program */

#ifdef ANSI
void rvgouser(char jump)
#else
void rvgouser(jump)
char jump;
#endif
	{
	}

/* `rvgoon` - proceed from where ever we are */

void rvgoon()
	{
	}

/* `rvatbpt` - process a break point stop */

void rvatbpt()
	{
	}

/* `rxinit` - initialize specific things */

void rxinit()
	{
	}

/* `rviwrite` - write to the instruction space */

#ifdef ANSI
void rviwrite(long loc, char byte)
#else
void rviwrite(loc, byte)
long loc;
char byte;
#endif
	{
	}
#endif
