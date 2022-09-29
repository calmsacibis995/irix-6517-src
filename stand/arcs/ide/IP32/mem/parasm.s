#ident	"IP22diags/mem/parasm.s:  $Revision: 1.1 $"

/*
 * parasm.s - diagnostic assembler subroutines for checking parity logic
 */

#include "asm.h"
#include "regdef.h"
#include "sys/cpu.h"
#include "sys/sbd.h"

/*
 * This file defines the following C-callable functions:
 *	void parwbad(addr,value,size);
 *	void _parwrite(addr,value,size);
 *	void _parread(addr,size);
 */

#define WBFLUSHM	\
	.set	noreorder ; \
	.set	noat	;\
	li	AT,PHYS_TO_K1(CPUCTRL0) ;\
	lw	AT,0(AT) ;\
	.set	at ; \
	.set	reorder

/* parwbad --
 *	write a byte/halfword/word with bad parity
 *	a0 - address to be written
 *	a1 - value to be written
 *	a2 - size of value to be written
 *	This is a level 0 routine (C-callable).
 *
 *	void parwbad(addr,value,size);
 */
LEAF(parwbad)
	li	t0, PHYS_TO_K1(CPUCTRL0)
	lw	t1, 0(t0)
	or	t2, t1, CPUCTRL0_BAD_PAR
	and	t2,~CPUCTRL0_RFE
	sw	t2, 0(t0)
	WBFLUSHM

	bne	a2,1,1f
	WBFLUSHM
	sb	a1,0(a0)
	sw	t1, 0(t0)
	WBFLUSHM
	j	ra

1: 	bne	a2,2,2f
	WBFLUSHM
	sh	a1,0(a0)
	sw	t1, 0(t0)
	WBFLUSHM
	j	ra

2:	WBFLUSHM
	sw	a1,0(a0)
	sw	t1, 0(t0)
	WBFLUSHM
	j	ra
	END(parwbad)

/* 
 * _parwrite (addr, value, size)
 */
LEAF(_parwrite)
	bne	a2,1,1f
	sb	a1,0(a0)
	j	ra
1: 	bne	a2,2,2f
	sh	a1,0(a0)
	j	ra
2:	sw	a1,0(a0)
	j	ra
	END(_parwrite)

/* 
 * _parread (addr, size)
 */
LEAF(_parread)
	/*
	 * without the WBFLUSHM, byte parity check will come back with the
	 * wrong address (hardare bug ?)
	 */
	WBFLUSHM

	bne	a1,1,1f
	lbu	v0,0(a0)
	j	ra
1: 	bne	a1,2,2f
	lhu	v0,0(a0)
	j	ra
2:	lw	v0,0(a0)
	j	ra
	END(_parread)

