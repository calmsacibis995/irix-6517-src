#include <sys/asm.h>
#include <sys/regdef.h>

#if (_MIPS_ISA == _MIPS_ISA_MIPS2 || _MIPS_ISA == _MIPS_ISA_MIPS3)
	.align 4
LEAF(strlen)
	.set noreorder
	PTR_SUBU    v0,a0,0
1:
        lbu	v1,0(v0)	 # 1
	PTR_ADDIU   v0,v0,4      # 1
        beql    v1,zero,2f       # 2
	PTR_ADDIU   a0,a0,4      # 2
        lbu     t0,-3(v0)        # 3
	beql    t0,zero,2f       # 4
	PTR_ADDIU   a0,a0,3      # 4
	lbu     t1,-2(v0)        # 4
        beql    t1,zero,2f       # 5
	PTR_ADDIU   a0,a0,2      # 5
        lbu     t3,-1(v0)        # 5
	bne     t3,zero,1b       # 6
        nop                      # 6
	PTR_ADDIU   a0,a0,1
2:
        j       ra
        PTR_SUB     v0,v0,a0
	.set reorder
        .end    strlen
#endif

#if (_MIPS_ISA == _MIPS_ISA_MIPS4)
/* Obtained the theoretical min: 1 cycle per iteratin */
	.align 4
LEAF(strlen)
	.set noreorder
	PTR_SUBU    v0,a0,0      # 0
        lbu     v1,0(a0)	 # 0
        lbu     t0,1(a0)         # 0
	nada                     # 0  /* alignment */
1:
	lbu     t1,2(a0)         # 1
        lbu     t3,3(a0)         # 1
        beq     v1,zero,3f       # 1
	PTR_ADDIU   a0,a0,4      # 1
	beql    t0,zero,2f       # 2
	PTR_ADDIU   v0,v0,3      # 2
        beql    t1,zero,2f       # 3
	PTR_ADDIU   v0,v0,2      # 3
        lbu     v1,0(a0)	 # 4
        lbu     t0,1(a0)         # 4
	bne     t3,zero,1b       # 4
        nop                      # 4
	PTR_ADDIU   v0,v0,1
2:
        j       ra
        PTR_SUB     v0,a0,v0
3:
	PTR_ADDIU   a0,a0,-4
        j       ra
        PTR_SUB     v0,a0,v0
	.set reorder
        .end    strlen
#endif
