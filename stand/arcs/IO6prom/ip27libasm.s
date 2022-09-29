/***********************************************************************\
*       File:           libasm.s                                        *
*                                                                       *
*       Contains subroutines which allow C functions to manipulate      *
*       the machine's low-level state.  In some cases, these            *
*       functions do little more than provide a C interface to          *
*       the machine's native assembly language.                         *
*                                                                       *
\***********************************************************************/


#ident "$Revision: 1.4 $"

#include <asm.h>

#include <sys/mips_addrspace.h>
#include <sys/regdef.h>
#include <sys/cpu.h>
#include <sys/loaddrs.h>
#include <sys/sbd.h>

#include <sys/SN/SN0/IP27.h>

#include "ip27prom.h"
#include "pod.h"

	.set	noreorder

LEAF(set_BSR)
	DMTBR(a0, BR_BSR)
	j	ra
	 nop
	END(set_BSR)

LEAF(get_BSR)
	DMFBR(v0, BR_BSR)
	j	ra
	 nop
	END(get_BSR)

/* ---------------------------IO6PROM Stuff------------------------- */

LEAF(ip27prom_asminit)
	.set noreorder

        lui     k0,1                    /* Turn off branch prediction */
        MTC0(k0, C0_BRDIAG)             /*  (BPmode = 1, all not taken) */

        li      v0, PROM_SR             /* Our expected SR */
        MTC0(v0, C0_SR)                 /* In known state */

        /*
         * The first access to the hub chip causes this CPU's PRESENT
         * bit to be set.  Might as well do that right now.
         */

        dli     v0, LOCAL_HUB(0)
        ld      v0, PI_CPU_NUM(v0)

        /*
         * Put CPU number in first bit of BSR for use by hub_cpu_get()
         */

        /* HUB_CPU_GET() */
        dli     v0, LOCAL_HUB(0)
        ld      v0, PI_CPU_NUM(v0)

        DMTBR(v0, BR_BSR)
	nop

	END(ip27prom_asminit)

/*
 * Delay uses the smallest possible loop to loop a specified number of times.
 * For accurate micro-second delays based on the hub clock, see rtc.s.
 */

LEAF(ip27_delay)
#ifndef SABLE
1:
        bnez    a0, 1b
         daddu  a0, -1
#endif /* SABLE */
        j       ra
         nop
        END(ip27_delay)

