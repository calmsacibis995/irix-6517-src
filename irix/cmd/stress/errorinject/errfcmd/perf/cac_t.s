#include <sys/asm.h>
#include <sys/regdef.h>	
#define	LEAF(x)						\
	.globl	x;					\
	.ent	x,0;					\
x:;							\
	.frame	sp,0,ra

#define	END(proc)					\
	.end	proc

 
	
LEAF(catte)
	.set noreorder	
1:	
	ld	t0,   0(a0)
	ld 	t1, 128(a0)
	ld	t2, 256(a0)
	ld	t3, 384(a0)
	ld	t0, 512(a0)
	ld	t1, 640(a0)
	ld	t2, 768(a0)
	ld	t3, 896(a0)
	ld	t0, 1024(a0)
	ld	t1, 1152(a0)
	ld	t2, 1280(a0)
	ld	t3, 1408(a0)
	ld	t0, 1536(a0)
	ld	t1, 1664(a0)
	ld	t2, 1792(a0)
	ld	t3, 1920(a0)
	ld	t0, 2048(a0)
	ld	t1, 2176(a0)
	ld	t2, 2304(a0)
	ld	t3, 2432(a0)
	ld	t0, 2560(a0)	
	ld	t1, 2688(a0)
	ld	t2, 2816(a0)
	ld	t3, 2944(a0)
	ld	t0, 3072(a0)
	ld	t1, 3200(a0)
	ld	t2, 3328(a0)
	ld	t3, 3456(a0)
	ld	t0, 3584(a0)
	ld	t1, 3712(a0)
	ld	t2, 3840(a0)
	ld	t3, 3968(a0)
	subu	a1, 1
	bnez	a1, 1b
	addiu	a0, 4096
	j	ra
	 nop
	.set reorder
	END(catte)		


	
