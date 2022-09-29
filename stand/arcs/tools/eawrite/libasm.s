#include <asm.h>
#include <regdef.h>
#include <sys/sbd.h>
#include <sys/cpu.h>


LEAF(get_prid)
        .set    noreorder
        mfc0    v0, C0_PRID
        j       ra
        nop
        END(get_prid)

LEAF(sa_store_double)
        .set    noreorder
        j       ra
        sd      a1,0(a0)        # (BD)
        END(sa_store_double)

LEAF(sa_load_double)
        .set    noreorder
        j       ra
        ld      v0,0(a0)        # (BD)
        END(sa_load_double)

LEAF(delay)
        .set    noreorder
	sll	t8, a0, 17
99:     bne     t8, zero, 99b
        sub     t8, 1		# (BD)
	j	ra
	END(delay)

