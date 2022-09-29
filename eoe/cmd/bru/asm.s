#include	<sys/regdef.h>
#include	<sys/asm.h>

/*
 * Do this since t5 is defined as a5 when
 * using _MIPS_SIM_ABI32 ... otherwise you will 
 * get an error during compile time.
 */
#if (_MIPS_SIM == _MIPS_SIM_ABI32)
#define TEMP_REG5	t5
#else
#define TEMP_REG5	a5
#endif 

LEAF(sumblock)
/*
 * Wipe out any leading bytes in the buffer
 */
	move	v0,zero
	andi	t0,a0,0x3
	beq	t0,zero,2f

	li	t3,0x4
	sub	t0,t3,t0
1:
	ble	a1,zero,9f
	sub	a1,a1,1
	lb	t1,(a0)
	add	v0,v0,t1
	addiu	a0,a0,1
	sub	t0,t0,1
	bne	t0,zero,1b
2:
	li	TEMP_REG5,3
3:
	ble	a1,TEMP_REG5,4f

	lb	t1,(a0)
	add	v0,v0,t1
	lb	t2,1(a0)
	add	v0,v0,t2
	lb	t1,2(a0)
	add	v0,v0,t1
	lb	t2,3(a0)
	add	v0,v0,t2

	addu	a0,a0,4
	sub	a1,a1,4
	b	3b

/*
 * Finally, finish by cleaning up any remaining bytes.
 */
4:
	ble	a1,zero,9f
	sub	a1,a1,1
	lb	t1,(a0)
	add	v0,v0,t1
	addiu	a0,a0,1
	subu	t0,t0,1
	bne	t0,zero,4b

9:
	j	ra

END(sumblock)
