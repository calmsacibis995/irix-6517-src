#include "asm.h"
#include "regdef.h"

.text
.set noreorder
.set noat

/* int Lock(int *aLock) */

LEAF(Lock)
   beqz		a0,	2f
   move		t1,	zero
1:
   ll		t0,	0(a0)
   beqz		t0,	1b
   nop
   sc		t1,	0(a0)
#ifdef R10000	/* up till 2.6 at least have trouble with some sc sequences */
   nop;nop;nop;nop;nop;nop;nop;nop
   nop;nop;nop;nop;nop;nop;nop;nop
   nop;nop;nop;nop;nop;nop;nop;nop
   nop;nop;nop;nop;nop;nop;nop;nop
#endif
   beqz		t1,	1b
   nop
   j		ra
   li		v0,	1
2:
   j		ra
   li		v0,	0
END(Lock)

/* int Unlock(int *aLock) */

LEAF(Unlock)
   beqz         a0,     2f
   li           t1,     1
1:
   sw           t1,     0(a0)
   j            ra
   li           v0,     1
2:
   j            ra
   li           v0,     0
END(Unlock)

