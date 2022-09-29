
/*	RVHelp.h - Edit 0.2

	RomView Version 1.1a
	Copyright (C) 1990-2 Grammar Engine, Inc.
	All rights reserved
	
	NOTICE:  This software source is a licensed copy of Grammar Engine's
	property.  It is supplied to you as part of support and maintenance
	of some Grammar Engine products that you may have purchased.  Use of
	this software is strictly limited to use with such products.  Any
	other use constitutes a violation of this license to you.
*/

/*	 - help strings
*/

#ifdef PIRV
typedef struct
	{
	char	*topic;
	char	*help[8];
	}RVHELP;

#ifdef	PDGLOB
RVHELP rxhelp[] = {
"?",
{"ROMview command help: USE '? topic' for more help;",
"	topics are:",
"	break point, clear (break point), go, proceed, step,",
"	dump, edit, instructions , registers",
"	",
"	you may use ? with just one or two letters to select a topic",
"	",0},
/*	set break point */
"breakpoint",
{"set break points and their conditions",
" syntax:	b address condition",
"	sets breakpoint at the specified address",
"	condition may be a simple count or addr=value",
"	breakpoint is hit when condition is met",
"	if no arguments given then display all break points",
"	",0},
/*	clear break point */
"clear",
{"clear break points",
" syntax:	c address",
"	clears breakpoint at the specified address",
"	",
"	",
"	",
"	",0},
/*	execute user program */
"go",
{"execute user program",
" syntax:	g address",
"	execute user program at address",
"	a subroutine call is made to the program",
"	user program should do a return",
"	",
"	",0},
/*	proceed */
"proceed",
{"continue from a break point",
" syntax:	p [#]",
"	continue the user program",
"	if # then proceed that many times",
"	from this breakpoint",
"	",
"	",0},
/*	step */
"step",
{"step one or more instructions",
" syntax:	s [#]",
"	single step user program",
"	starts at the current pc",
"	does one or more steps",
"	",
"	",0},
/*	dump target space */
"dump",
{"dump target space",
" syntax:	d[x|i] address1 address2",
"	dump target system space",
"	dump the range or if only one address",
"	given then dump small block",
"	dx - dump external RAM if applicable",
"	di - dump instruction space",0},
/*	edit */
"edit",
{"edit target memory",
" syntax:	e[x|i] address [data..]",
"	enter edit mode (unless data given) use 'x' to exit edit mode",
"	display current value and allow editing",
"	display next (CR) or previous(^) value",
"	ex - edit external RAM if applicable",
"	ei - edit instruction space",0},
/*	display instruction space */
"inst",
{"display memory as instructions",
" syntax:	i address1 address2",
"	operates just like dump",
"	except dumps memory as",
"	instructions",
"	generally fetches data from",
"	any target instruction space",0},
/*	registers */
"registers",
{"display registers",
" syntax:	r",
"	dump target processor registers",
"	show all the registers defined",
"	the individual registers may be",
"	displayed and edited by using the",
"	.name [value] - syntax",0},
0};

#else
extern RVHELP rxhelp[];
#endif
#endif
