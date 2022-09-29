#include <asm.h>
#include <regdef.h>

#if (_MIPS_SIM == _MIPS_SIM_ABI32)	/* do not compiler for 64-bit */

LEAF(dbl_load)
	ld  t0,0(a0)
	sd  t0,0(a1)
	j   ra
	END(dbl_load)


/* store_double(high,low,vaddr); */


.set noreorder


LEAF(ide_store_double)
          mtc1    a0,$f5
          mtc1    a1,$f4
          jr      ra
          sdc1    $f4, 0(a2)
END(ide_store_double)

#endif
