#include <sys/asm.h>
#include <sys/regdef.h>	
#define	LEAF(x)						\
	.globl	x;					\
	.ent	x,0;					\
x:;							\
	.frame	sp,0,ra

#define	END(proc)					\
	.end	proc


LEAF(iter)
1:	subu	a0, 1
	bne	a0, zero, 1b
	j	ra
	END(iter)
	

	
