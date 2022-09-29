 
#ident "$Revision: 1.1 $"

#include "asm.h"
#include "regdef.h"

/* heart_byte_blast(char *,data) -- in assembly for a tight loop and so caller
 * can make sure the code is cached easily.
 */
LEAF(heart_byte_blast)
	li	t0,128
1:	sb	a1,0(a0)
	addi	t0,-1
	daddiu	a0,1
	addi	a1,1
	bnez	t0,1b
	j	ra
	END(heart_byte_blast)
