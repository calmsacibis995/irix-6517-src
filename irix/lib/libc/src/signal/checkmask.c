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

#ident "$Revision: 1.14 $"

#include "synonyms.h"

#include <sys/types.h>
#include <signal.h>
#include <setjmp.h>
#include <stdio.h>
#include <ucontext.h>		/* prototype for getcontext() */

int
_sigstat_save(ucontext_t *ucp, int savemask, machreg_t ra, machreg_t sp,
							 machreg_t gp)
{
        register greg_t *reg;

        if (!savemask)
                ucp->uc_flags &= ~UC_SIGMASK;

        reg = ucp->uc_mcontext.gregs;
        reg[CXT_RA] = ra;
        reg[CXT_EPC] = ra;
        reg[CXT_SP] = sp;
        reg[CXT_GP] = gp;

        return 0;
}
