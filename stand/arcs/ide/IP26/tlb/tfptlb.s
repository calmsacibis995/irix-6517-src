/*
 * tlb.s -- tlb code
 */

#include "ml.h"
#include <sys/sbd.h>
#include <sys/immu.h>
#include <regdef.h>
#include <asm.h>


#if IP26
#include <sys/cpu.h>
#endif


#define NOINTS(x,y)                     \
        li      x,(SR_PAGESIZE>>32);    \
        dsll    x,32;                   \
        MTC0_NO_HAZ(x,C0_SR);           \
        NOP_MTC0_HAZ

.data
crlf:
.asciiz	"\n"



        .text
/*
 * tfptlbfill(indx, tlbpid, vaddr, pte) -- 
 * a0 -- set
 * a1 -- tlbpid -- context number to use (0..TLBHI_NPID-1)
 * a2 -- vaddr -- virtual address (could have offset bits)
 * a3 -- pte -- contents of pte
 */
LEAF(tfptlbfill)
        .set    noreorder
/*        DMFC0(t0,C0_TLBHI)               save current TLBPID*/
        DMFC0(v0,C0_SR)                 # save SR and disable interrupts
        NOINTS(t1,C0_SR)
        DMTC0(a2,C0_BADVADDR)
        .set    reorder


/*        dsll    a3,TLBLO_HWBITSHIFT  */   # shift pfn field to correct position
/*	dsll	a3, 7			# temporary substitute*/

        sll     a1,TLBHI_PIDSHIFT       # line up pid bits

        dsrl    a2,PNUMSHFT		# chop offset bits
        dsll    a2,PNUMSHFT

/*	dli	t2, 0xc000fffffff80000*/

	dli	t2, (TLBHI_REGIONMASK | TLBHI_VPNMASK)
	and	a2, t2			# VPN mask for EntryHi

/*       move    a0,a2
        jal     pon_puthex64
        dla     a0, crlf
        jal     pon_puts

*/
        or      a1,a2                   # formatted tlbhi entry


        .set    noreorder
        DMTC0(a1,C0_TLBHI)              # set VPN and TLBPID
        DMTC0(a3,C0_TLBLO)              # set PPN and access bits
        DMTC0(a0,C0_TLBSET)	        # choose desireable set 
        TLB_WRITER                      # drop it in
/*        DMTC0(t0,C0_TLBHI)              # restore TLBPID*/
        DMTC0(v0,C0_SR)                 # restore SR
	nop
	nop
        j       ra
        nop                             # BDSLOT
        .set    reorder
        END(tfptlbfill)

