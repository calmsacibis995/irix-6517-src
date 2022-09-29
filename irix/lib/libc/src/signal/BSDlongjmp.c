/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.5 $"

#pragma	weak	BSDlongjmp = _BSDlongjmp

#include "synonyms.h"

#include <sys/types.h>
#include <signal.h>
#include <setjmp.h>
#include <stdio.h>
#include <ucontext.h>		/*prototype for setcontext() */

void
BSDlongjmp(jmp_buf env, int val)
{
        register ucontext_t *ucp = (ucontext_t *)env;
	/*
	 * for PIC code if folks want to change the PC and such
	 * we should also change 't9' for them. SInce t9
	 * is not part (officially) of a jmpbuf there should be
	 * no problem with this.
	 */
	ucp->uc_mcontext.gregs[CXT_T9] = ucp->uc_mcontext.gregs[CXT_EPC];
        if (val)
                ucp->uc_mcontext.gregs[CXT_V0] = (greg_t)val;
        else
                ucp->uc_mcontext.gregs[CXT_V0] = (greg_t)1;
        setcontext(ucp);
	/* NOTREACHED */
}
