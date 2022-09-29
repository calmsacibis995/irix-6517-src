/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* NOTE that this file, for the 64-bit kernel, is compiled
 * twice: Once with ELF64 defined, and once with ELF64
 * left undefined. os/Makefile directs this compilation with
 * and without the additional ELF64 flag.
 *
 * The reason is that some structure sizes/types differ, but otherwise,
 * the code is identical. The differences are handled by the following
 * definitions and typedefs in the ELF32/ELF64 ifdefs. Thus, we have
 * one copy of the source and 2 copies of the object - one
 * object providing support for 32-bit exec's, the other providing
 * support for 64-bit exec's.
 *
 * If the compile mode is !_ABI64 this file is
 * compiled one time only - for exec support for o32 and n32
 * binaries.
 *
 * The main difference between the 2 objects is the typedef userstack_t.
 * For 32 bit elf exec, userstack_t is an __int32_t. For 64
 * bit elf execs, userstack_t is an __int64_t
 * userstack_t is defined in the file sys/exec.h
 * This allows the kernel to build up the arguments on the
 * user stack properly for 32 and 64bit user environments.
 */

#ident	"$Revision: 1.29 $"

/* For !_ABI64 kernels, produce only 1 fuexarg module.
 * For 64-bit kernels, produce a module for 32-bit support
 * and a module for 64-bit support.
 */
#if !ELF64 || (ELF64 && _MIPS_SIM == _ABI64)		/* Whole file */

#include <sys/types.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/exec.h>
#include <sys/param.h>
#include <sys/pda.h>
#include <sys/sysmacros.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include "os/proc/pproc_private.h"	/* XXX bogus */

#if SN0 && SN0_USE_BTE
#include <sys/SN/SN0/bte.h>
#endif


#if ELF64
#define fuexarg		fuexarg64
typedef __int64_t	userstack_t;
#else
typedef __int32_t	userstack_t;
#endif

extern int ncargs;		/* defined in kernel master file */

/*
 * This code assumes that vaddr points to an area that is exactly
 * (3*ncargs + args->auxsize) bytes from the start of the user stack.
 */
int
fuexarg(struct uarg *args, caddr_t vaddr, uvaddr_t *newsp, int sourceabi)
{
	register char *cp = (char *)vaddr + 2*ncargs + args->ua_auxsize;
	register char **ap;
	register char *sp;
	register userstack_t *statep = (userstack_t *)vaddr;
	register userstack_t ucp;
	register int np = 0;
	int nc = 0;
	size_t c;
	int na = 0;
	int nac = 0;
	int env = 0;
	userstack_t *newp;
	int error;
	int incr;
	__psint_t stackptr;

	/*
	 * Handle any prefix args first.  All prefix ptrs are in
	 * kernel space.  Strings may be either in kernel or user
	 * space.
	 */

#if _MIPS_SIM == _ABI64
	if (sourceabi == ABI_IRIX5_64)
		incr = sizeof(__int64_t);
	else
#endif
		incr = sizeof(__int32_t);

	stackptr = (__psint_t)args->ua_stackaddr;

	ucp = stackptr - ncargs;

	if (ap = args->ua_prefixp)
		while (sp = *ap) {
			*statep++ = ucp;
			np++;

			if (ncargs-nc <= 1)   /* a string is at least 2 bytes */
				return E2BIG;

			if (IS_KUSEG(sp))
				error = copyinstr(sp, cp, ncargs-nc, &c);
			else
				error = copystr(sp, cp, ncargs-nc, &c);
			
			/*
			 * Ugh.  If this is an suid script and this is
			 * the argument for the file descriptor, we cache
			 * the location on the stack that we're going
			 * to need.  This will be used in remove_proc()
			 * when we are single-threaded and can open
			 * the descriptor.
			 */
			if (args->ua_intpvp && (sp == args->ua_fname))
				args->ua_intpstkloc = cp;

			if (error)
				if (error == ENAMETOOLONG)
					return E2BIG;
				else
					return error;

			ASSERT(c != 0);
			nc += c;
			ucp += c;
			cp += c;
			ap++;
		}

	/* collect arglist */

	ap = args->ua_argp;
	for (;;) {
		if (ap) {
			for (;;) {
#if _MIPS_SIM == _ABI64
				if (sourceabi == ABI_IRIX5_64)
					sp = (char *)fulong((caddr_t)ap);
				else
#endif
					sp = (char *)(__psint_t)
							fuword((caddr_t)ap);
				if (sp == 0)
					break;
				*statep++ = ucp;
				if (++np > (ncargs/2))
					return E2BIG;
				ap = (char **)((__psint_t)ap + incr);

				if (ncargs-nc <= 1)   /* a string is at least */
					return E2BIG; /* 2 bytes              */

				if (error = copyinstr(sp, cp, ncargs-nc, &c))
					if (error == ENAMETOOLONG)
						return E2BIG;
					else
						return error;
				ASSERT(c);
				nc += c;
				ucp += c;
				cp += c;
			}
		}
		*statep++ = NULL;	/* add terminator */
		if (!env++) {
			na = np;	/* save argc */
			nac = nc;
			ap = args->ua_envp; /* switch to env */
			continue;
		}
		break;
	}

	/*
	 * copy back arglist -- i.e.
	 *	set up argument and environment pointers
	 *	arg list is built by pushing onto stack.
	 *	a picture is worth a thousand words:
	 *
	 *	The first area is used for floating point emulation
	 *	code space in the mips implementation
	 *
	 * 	At this point the picture looks like this:
	 *
					high
	 userstack(0x7ffff000) ->	|_______________| ___
					|               |  ^
					| WASTED SPACE  |  |
					|               |  |
					| "e_strn\0"    |  |
			strings		|    ***        |  |
					| "e_str0\0"    |  | ncargs
					| "a_strn\0"    |  |
					|    ***        |  |
					| "a_str0\0"    |  |
	 		newp->		|_______________| _v_
					|               |  ^
					|               |  |
					|               |  |
					|               |  |
			statep->	|               |  |
					| 0             |  |
					| ENVn          |  |
					| ...           |  |
					| ENV0          |  |
					| 0             |  | ("holding area")
					| ARGn          |  |
					| ...           |  | 2*ncargs
					| ARG0          |  |
	 		vaddr->		|_______________| _v_
					|               |  ^
					| Shared library|  |
					| data (temp).  |  |
	 		vwinadr(exece)->|_______________| _v_
					low



	 * 	We will make it look like this
	 * XXX - Rewrite to not waste the space above the strings
					 _______________
	 empty page for kernel copy	|               |
					|               |
	 userstack(0x7ffff000) ->	|_______________| ___
					|               |  ^
					| WASTED SPACE  |  |
					|   (zeroed)    |  |
					| "e_strn\0"    |  |
					|    ***        |  |
					| "e_str0\0"    |  | ncargs
					| "a_strn\0"    |  |
					|    ***        |  |
					| "a_str0\0"    |  |
	 		strings		|_______________| _v_
					| Space reserved|  ^
					| for aux args  |  | args->auxsize
					| (for rld)	|  v
					|---------------| ---
					| 0             |
					| env[n]        |  ^
					| ...           |  |
					| env[0]        |  | (real area)
					| 0             |  |
					| argv[n]       |  |
					| ...           |  | 2*ncargs
					| argv[0]       |  |
			sp ->		| argc          |  |
					|               |  |
					|               |  |
					|               |  |
					|               |  |
					|               |  |
	 		vaddr->		|_______________| _v_
					|               |  ^
					| Shared library|  |
					| data (temp).  |  |
	 		vwinadr(exece)->|_______________| _v_
					low

	 */
	
	/*
	 * set up the users stackpointer 1 + 2 is for argc & 2 nulls
	 */
	*newsp = (uvaddr_t)(stackptr - ncargs -
			(np + 1 + 2) * sizeof(userstack_t)
			- args->ua_auxsize);
	newp = (userstack_t *)(vaddr + 2*ncargs);
	cp = (char *)newp + args->ua_auxsize;

	/*
	 * Clear unused space on "bottom" of stack (everything between the
	 * last env string and the highest stack address).
	 */

#if SN0_USE_BTE
	/*
	 * For now, just wait for the transfer to finish.
	 * We may want to do it asynchronously in the future.
	 * What about cache coloring?
	 *
	 * Clear up to next cache line first. We do it inline
	 * since calling bcopy for such a small size (less than
	 * a cache line) is too expensive.
	 */
	{
		caddr_t start;
		int total_cnt, cnt;

		start = cp + nc;
		total_cnt = ncargs - nc;
		while ((__psint_t)start & BTE_LEN_ALIGN) {
			*start++ = 0;
			total_cnt--;
		}
		while (total_cnt >= BTE_LEN_MINSIZE) {
			cnt = MIN(NBPP - poff(start), total_cnt);
			if (cnt < BTE_LEN_MINSIZE)
				break;
			bte_pbzero(kvtophys(start), cnt, 0);
			start += cnt;
			total_cnt -= cnt;
		}
		if (total_cnt)
			bzero(start, total_cnt);
	}
#else
	bzero(cp + nc, ncargs - nc);
#endif

	/*
	 * Stack needs to be double word aligned for floating point stuff.
	 * There's talk of needing quad word alignment so in anticipation
	 * of that need forcing the stack to quad word alignment now.
	 */
	if ((__psint_t)*newsp & 0xf) {
		userstack_t fudge;

		fudge = (__psint_t)*newsp & 0xf;
		*newsp -= fudge;
		newp -= fudge / sizeof (userstack_t);
	}
	args->ua_stackend = (caddr_t) newp;

	/* copy from holding area to real area */
	for ( c = 0; c < np+2; c++)	/* +2 for two null ptrs */
		*--newp = *--statep;

	*--newp = na;	 /*  argc  	*/

	/* do psargs */
	sp = curprocp->p_psargs;
	np = MIN(nac-1, PSARGSZ-1);	/* nac-1 to avoid trailing ' ' */
	if (!args->ua_intpvp) {
		/* NB np can start out with a value < 0, since nac man be 0 */
		while (np-- > 0) {
			if ( (*sp = *cp++) == '\0' )
				*sp = ' ';
			sp++;
		}
		*sp = '\0';
		return 0;
	} else {
		/*
		 * We need to cache the location of the psarg for
		 * /dev/fd so that we can substitute the file descriptor
		 * when known.
		 * NB np can start out with a value < 0, since nac man be 0.
		 */
		while (np-- > 0) {
			if (cp == args->ua_intpstkloc)
				args->ua_intppsloc = sp;
			if ( (*sp = *cp++) == '\0' )
				*sp = ' ';
			sp++;
		}
		*sp = '\0';
		return 0;
	}
}	
#endif	/* !ELF64 || (ELF64 && _MIPS_SIM == _ABI64) */
