/***********************************************************************\
*       File:           diagasm.s                                       *
*                                                                       *
\***********************************************************************/

#ident "$Revision: 1.4 $"

#include <asm.h>
#include <sys/cpu.h>
#include <sys/regdef.h>
#include <sys/sbd.h>

LEAF(diag_get_epc)
        .set noreorder
	DMFC0(v0, C0_EPC)
        j       ra
         nop
        .set reorder
        END(diag_get_epc)

LEAF(diag_get_badvaddr)
        .set noreorder
	DMFC0(v0, C0_BADVADDR)
        j       ra
         nop
        .set reorder
        END(diag_get_badvaddr)

LEAF(diag_get_cause)
        .set noreorder
	DMFC0(v0, C0_CAUSE)
        j       ra
         nop
        .set reorder
        END(diag_get_cause)

LEAF(diag_get_lladdr)
        .set noreorder
	DMFC0(v0, C0_LLADDR)
        j       ra
         nop
        .set reorder
        END(diag_get_lladdr)

LEAF(diag_set_lladdr)
        .set noreorder
	DMTC0(a0, C0_LLADDR)
        j       ra
         nop
        .set reorder
        END(diag_set_lladdr)

LEAF(diag_get_tlbextctxt)
        .set noreorder
	DMFC0(v0, C0_EXTCTXT)
        j       ra
         nop
        .set reorder
        END(diag_get_tlbextctxt)

LEAF(diag_set_tlbextctxt)
        .set noreorder
	DMTC0(a0, C0_EXTCTXT)
        j       ra
         nop
        .set reorder
        END(diag_set_tlbextctxt)

