#include <asm.h>
#include <regdef.h>

LEAF(LOAD_FPU)
	.extern	o_buf	128
	.text
/* load from memory to fpu
 */
	beq	a0,zero,ZERO
	beq	a0,1,1f
	beq	a0,2,2f
	beq	a0,3,3f
	beq	a0,4,4f
	beq	a0,5,5f
	beq	a0,6,6f
	beq	a0,7,7f
	beq	a0,8,8f
	beq	a0,9,9f
	beq	a0,10,10f
	beq	a0,11,11f
	beq	a0,12,12f
	beq	a0,13,13f
	beq	a0,14,14f
	beq	a0,15,15f
	beq	a0,16,16f
	beq	a0,17,17f
	beq	a0,18,18f
	beq	a0,19,19f
	beq	a0,20,20f
	beq	a0,21,21f
	beq	a0,22,22f
	beq	a0,23,23f
	beq	a0,24,24f
	beq	a0,25,25f
	beq	a0,26,26f
	beq	a0,27,27f
	beq	a0,28,28f
	beq	a0,29,29f
	beq	a0,30,30f
	beq	a0,31,31f	

ZERO:
	lwc1	$f0,(a1)
	j	RET
1:
	lwc1	$f1,(a1)
	j	RET
2:
	lwc1	$f2,(a1)
	j	RET
3:
	lwc1	$f3,(a1)
	j	RET
4:
	lwc1	$f4,(a1)
	j	RET
5:
	lwc1	$f5,(a1)
	j	RET
6:
	lwc1	$f6,(a1)
	j	RET
7:
	lwc1	$f7,(a1)
	j	RET
8:
	lwc1	$f8,(a1)
	j	RET	
9:
	lwc1	$f9,(a1)
	j	RET
10:
	lwc1	$f10,(a1)
	j	RET
11:
	lwc1	$f11,(a1)
	j	RET
12:
	lwc1	$f12,(a1)
	j	RET
13:
	lwc1	$f13,(a1)
	j	RET
14:
	lwc1	$f14,(a1)
	j	RET
15:
	lwc1	$f15,(a1)
	j	RET
16:
	lwc1	$f16,(a1)
	j	RET
17:
	lwc1	$f17,(a1)
	j	RET
18:
	lwc1	$f18,(a1)
	j	RET
19:
	lwc1	$f19,(a1)
	j	RET
20:
	lwc1	$f20,(a1)
	j	RET
21:
	lwc1	$f21,(a1)
	j	RET
22:
	lwc1	$f22,(a1)
	j	RET
23:
	lwc1	$f23,(a1)
	j	RET
24:	
	lwc1	$f24,(a1)
	j	RET
25:
	lwc1	$f25,(a1)
	j	RET
26:
	lwc1	$f26,(a1)
	j	RET
27:
	lwc1	$f27,(a1)
	j	RET
28:
	lwc1	$f28,(a1)
	j	RET
29:
	lwc1	$f29,(a1)
	j	RET
30:
	lwc1	$f30,(a1)
	j	RET
31:
	lwc1	$f31,(a1)
	
RET:
	j	ra
	END(LOAD_FPU)
