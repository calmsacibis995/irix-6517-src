/*
 * bb_cacheasm.s -- blackbird(tfp) Cache manipulation primitives
 */

#include <regdef.h>
#include <sys/cpu.h>
#include <sys/sbd.h>
#include <asm.h>
#include "ip21.h"

LEAF(read_tag)
		### just stubbed in for now!!
	j ra
	END(read_tag)

/*
 * unsigned GetCacheErr()
 *
 * Returns the cp0 Cache_Err register (read only).
 */
LEAF(GetCacheErr)
        .set noreorder
#ifndef TFP
        mfc0    v0, C0_CACHEERR
        nop
#endif
        j       ra
        nop
        .set reorder
END(GetCacheErr)



/* pd_hwbinv(caddr): Primary Data cache Hit Writeback Invalidate.
 * a0: K0-seg virtual address */
LEAF(pd_hwbinv)
#ifndef TFP
        li      t0, WD_ALIGN_MASK       # cacheops must be wd aligned
        not     t0
        and     a0, a0, t0
        .set    noreorder
        cache   CACH_PD|C_HWBINV, 0(a0)
        nop
        nop
        nop 
	.set    reorder
#endif
        j       ra
        END(pd_hwbinv)

