/*	Copyright (c) 1990, 1991, 1992 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-i386:gen/makectxt.c	1.7"

#ifdef __STDC__
	#pragma weak makecontext = _makecontext
#endif

#include "synonyms.h"
#include <stdarg.h>
#include <string.h>
#include <ucontext.h>
#include <errno.h>
#include <sys/types.h>
#include "sig_extern.h"

/*
	makecontext - Has several short comings, here is what they are ... 
	so far.

	1) Only can handle longs and pointers.
*/

void
makecontext(ucontext_t *ucp, void (*func)(), int argc, ...)
{
	register greg_t *greg;
	va_list ap;
	machreg_t *sp;
	int i;

	greg = ucp->uc_mcontext.gregs; 		/* General CPU registers */

	greg[ CTX_EPC ] = (greg_t) func;

	/* For "Position Independent" functions T9 must contain
	   the address of the function to be called */

	greg[ CTX_T9 ] = (greg_t) func;

	/* Argument for setcontext function, will be used in setlinkcontext */

	greg[CTX_S0] = (greg_t)(ucp->uc_link);

	/* Assembler call to get the current value of the $gp. Will be used 
	   later in setlinkcontext (see setlinkctxt.s) */

	greg[CTX_S1] = (greg_t)__getgp();

	sp = (machreg_t *)ucp->uc_stack.ss_sp;

	greg[ CTX_SP ] = (machreg_t) sp ;	/* set the signal stack to sp */

#if (_MIPS_SIM == _MIPS_SIM_ABI32)
	sp = &sp[4];			/* first arg entry in arg. block */
#endif

	va_start (ap, argc);

	/* The first 4 args go in register A0 - A3, ... */

#if (_MIPS_SIM == _MIPS_SIM_ABI32)
	for (i=CTX_A0; argc && (i <= CTX_A3); argc--, i++)	
#else	/* _MIPS_SIM == _MIPS_SIM_ABI64 || _MIPS_SIM == _MIPS_SIM_NABI32 */
	for (i=CTX_A0; argc && (i <= CTX_A7); argc--, i++)	
#endif
		greg[ i ] = va_arg(ap, machreg_t);

	/* all other arguments will go on the stack */

	while (argc--)
		*sp++ = va_arg(ap, machreg_t);

	va_end (ap);

	/* Set the return address to __setlinkcontext so that the 
	   _setlinkcontext can move the values from s0 (uc_link) 
	   and s1 ($gp) to the correct values and then jump to setcontext */

	greg[ CTX_RA ] = (greg_t) __setlinkcontext;
}
