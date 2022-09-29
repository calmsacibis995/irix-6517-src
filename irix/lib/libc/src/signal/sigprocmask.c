/*************************************************************************
 *                                                                       *
 *              Copyright (C) 1995, Silicon Graphics, Inc.               *
 *                                                                       *
 * These coded instructions, statements, and computer programs  contain  *
 * unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 * are protected by Federal copyright law.  They  may  not be disclosed  *
 * to  third  parties  or copied or duplicated in any form, in whole or  *
 * in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                       *
 *************************************************************************/

#ident "$Revision: 1.11 $"

#ifdef __STDC__
        #pragma weak sigprocmask = _sigprocmask
#endif

#include "synonyms.h"
#include <sys/signal.h>
#include "sig_extern.h"
#include <sys/prctl.h>
#include <sys/syssgi.h>
#include <errno.h>
#include <assert.h>
#include "mplib.h"

#define	KSIG_O32(P)	(((P)->t_sys.t_flags & T_HOLD_KSIG_O32) != 0)

static int __t_hold_valid = 0;

/* ARGSUSED */
int
__sgi_prda_procmask(int level)
{
	if (syssgi(SGI_PROCMASK_LOCATION, level) == 0) {
		if (level == USER_LEVEL)
			__t_hold_valid = 1;
		return 0;
	}
	return -1;
}

int
sigprocmask(int operation, const sigset_t *newset, sigset_t *oldset)
{
	MTLIB_RETURN( (MTCTL_SIGPROCMASK, operation, newset, oldset) );

	if (__t_hold_valid && PRDA->t_sys.t_flags & T_HOLD_VALID) {
		/* User Level */
		
		k_sigset_t *hold = &PRDA->t_sys.t_hold;
		k_sigset_t new;
	
		if (newset
		    && operation != SIG_NOP
		    && operation != SIG_BLOCK
		    && operation != SIG_UNBLOCK
		    && operation != SIG_SETMASK
		    && operation != SIG_SETMASK32) {
			setoserror(EINVAL);
			return -1;
		}

		/*
		 * Return old set if requested.
		 */
#if	(_MIPS_SIM != _ABIO32)
		if (oldset) {
			if (!KSIG_O32(PRDA)) {
				/* n32 or 64 library, n32 or 64 kernel */
				oldset->__sigbits[0] = (__uint32_t)*hold;
				oldset->__sigbits[1] = (__uint32_t)(*hold>>32);
			} else {
				/* n32 or 64 library, o32 kernel */
				oldset->__sigbits[0] = (__uint32_t)(*hold>>32);
				oldset->__sigbits[1] = (__uint32_t)*hold;
			}
			oldset->__sigbits[2] = 0;
			oldset->__sigbits[3] = 0;
		}
#else	/* (_MIPS_SIM == _ABIO32) */
		if (oldset) {
			if (KSIG_O32(PRDA)) {
				/* o32 library, o32 kernel */
				oldset->__sigbits[0] = hold->sigbits[0];
				oldset->__sigbits[1] = hold->sigbits[1];
			} else {
				/* o32 library, n32 or 64 kernel */
				oldset->__sigbits[0] = hold->sigbits[1];
				oldset->__sigbits[1] = hold->sigbits[0];
			}
			oldset->__sigbits[2] = 0;
			oldset->__sigbits[3] = 0;
		}
#endif	/* (_MIPS_SIM == _ABIO32) */

		/*
		 * If new set is NULL, we're done.
		 */
		if (newset == NULL) {
			return 0;
		}

		/*
		 * convert sigset to k_sigset
		 */

#if	(_MIPS_SIM != _ABIO32)

		if (!KSIG_O32(PRDA)) {
			/*
			 * n32 or 64 library, n32 or 64 kernel
			 */
			new = ((k_sigset_t)newset->__sigbits[1] << 32) |
				(k_sigset_t)newset->__sigbits[0];
			/*
			 * Remove KILL and STOP from new set.
			 */
			new &= ~(sigmask(SIGKILL) | sigmask(SIGSTOP));
		} else {
			/*
			 * n32 or 64 library, o32 kernel
			 */

			__uint32_t lowbits = newset->__sigbits[0];

			/*
			 * Remove KILL and STOP from new set.
			 */
			lowbits &= ~(sigmask(SIGKILL) | sigmask(SIGSTOP));

			new = ((k_sigset_t)lowbits << 32) |
				(k_sigset_t)newset->__sigbits[1];
		}

#else	/* (_MIPS_SIM == _ABIO32) */

		if (KSIG_O32(PRDA)) {
			/*
			 * o32 library, o32 kernel
			 */
			new.sigbits[0] = newset->__sigbits[0];
			new.sigbits[1] = newset->__sigbits[1];
			/*
			 * Remove KILL and STOP from new set.
			 */
			new.sigbits[0] &=
					~(sigmask(SIGKILL) | sigmask(SIGSTOP));
		} else {
			/*
			 * o32 library, n32 or 64 kernel
			 */
			new.sigbits[0] = newset->__sigbits[1];
			new.sigbits[1] = newset->__sigbits[0];
			/*
			 * Remove KILL and STOP from new set.
			 */
			new.sigbits[1] &=
					~(sigmask(SIGKILL) | sigmask(SIGSTOP));
		}

#endif	/* (_MIPS_SIM == _ABIO32) */

		/*
		 * Depending on the operation, change 'hold' appropriately.
		 */
		switch(operation) {
		    case SIG_BLOCK:
			    __ksigorset(hold, &new);
			    break;
			    
		    case SIG_UNBLOCK:
			    __ksigdiffset(hold, &new);
			    break;

		    case SIG_SETMASK:
			    *hold = new;
			    break;

		    case SIG_SETMASK32:
			    /*
			     * if they want to block everything - let them
			     */
			    if (newset->__sigbits[1] == -1)
				    *hold = new;
			    else {
				    /* 
				     * preserves the upper 32 signals from
				     * sigsetmask since it's not supposed
				     * to know about them.
				     */
#if	(_MIPS_SIM != _ABIO32)
				if (!KSIG_O32(PRDA))
				    /* n32 or 64 library, n32 or 64 kernel */
				    *(((__uint32_t *)hold) + 1) = 
						(__uint32_t)(new & 0x00000000FFFFFFFF);
				else
				    /* n32 or 64 library, o32 kernel */
				    *((__uint32_t *)hold) =
						(__uint32_t)(new >> 32);
#else	/* (_MIPS_SIM == _ABIO32) */
				if (KSIG_O32(PRDA))
				    /* o32 library, o32 kernel */
				    hold->sigbits[0] = new.sigbits[0];
				else
				    /* o32 library, n32 or 64 kernel */
				    hold->sigbits[1] = new.sigbits[1];
#endif	/* (_MIPS_SIM == _ABIO32) */
			    }
			    break;
		}
		
		return 0;
	} else {
		/* Kernel Level */
		return (_ksigprocmask(operation, newset, oldset));
	}
}
