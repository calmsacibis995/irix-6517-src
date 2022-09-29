#include <ml.h>
#include <asm.h>
#include <regdef.h>

/* set up backtrace parameters, and jump to real code.
 *	backtrace(levels) -> _brace(levels,pc,sp,0)
 */
LEAF(backtrace)
	.set	noreorder
	move	a1,ra		/* a0 already passed in */
	PTR_ADDI	a1,-8
	LA	k0,_btrace
	move	a2,sp		/* ld delay */
	jr	k0		
	move	a3,zero		/* branch delay */
	END(backtrace)
