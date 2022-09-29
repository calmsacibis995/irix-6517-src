/*	rvscript.h - Edit 0.2

	RomView Version 1.1a
	Copyright (C) 1990-2 Grammar Engine, Inc.
	All rights reserved
	
	NOTICE:  This software source is a licensed copy of Grammar Engine's
	property.  It is supplied to you as part of support and maintenance
	of some Grammar Engine products that you may have purchased.  Use of
	this software is strictly limited to use with such products.  Any
	other use constitutes a violation of this license to you.
*/

/*	 - This file contains the syntax for all parsing operations 
	
	Format: break characters are 'bt,/:='
	literal command &
	$ - read a string
	# - read a decimal number
	* - read a hex number
	: - read unit id
	% - read embedded number
	@ - read a list of zero or more hex numbers
	& - read a list of zero or more decimal numbers
*/
#ifdef PIRV
#ifdef ANSI
void pisyn(char *in, char *scripts[], void (*psynf[])(void));
#else
void pisyn();
#endif

#ifdef PDGLOB
char	*rvfsyn[] = {
	0
	};

char	*rvusyn[] = {

/*	interactive mode commands: */

/* test - test promice unit */
	"test&#(T",
/* ai - init AiCOM link */
	"ai&@(a",
/* b - set break point */
	"b&***(b",
/* clear - clear break points */
	"c&*(c",
/* dump code - display code space */
	"di&@(C",
/* dump external - display external data space */
	"dx&@(D",
/* dump - dump memory */
	"d&@(d",
/* edit instructions - edit code space */
	"ei&*@(I",
/* edit external - edit external data space */
	"ex&*@(E",
/* edit - edit memory locations */
	"e&*@(e",
/* go - start user program */
	"g&*(g",
/* help - give help */
	"h&$(h",
	"?&$(h",
/* instruction dump - dump code (either i space or d space) */
	"i&@(i",
/* pi - init PiCOM link */
	"pi&@(p",
/* p - proceed from breakpoint */
	"p&#(P",
/* q - xequte instruction */
	"q&*(q",
/* registers - display target registers */
	"r&&(r",
/* Read - read Promice Buffer */
	"Q&&(R",
/* stack dump - shows the stack */
	"sp&**(s",
/* step - step some instructions */
	"s&#(X",
/* trap - test hardware trap */
	"t&@(t",
/* word - set word size */
	"w&#(w",
/* default is like a special command */
	"~$@(S",
	0
	};
#else
extern char *rvfsyn[];
extern char *rvusyn[];
#endif
#endif
