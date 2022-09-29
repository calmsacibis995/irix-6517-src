#include <asm.h>
#include <early_regdef.h>
#include <sys/sbd.h>
#include <sys/cpu.h>

/*
 * this tests the data path from the CPU to the MC/HPC by walking a 1
 * through one of the registers.  if the test fails, loop forever
 */
LEAF(pon_mc)
	move	RA3,ra			# save return address

	/* must turn refresh off before we run the MC portion of the test */
	LI	T31,PHYS_TO_K1(CPUCTRL0)
	lw	T32,0(T31)
	and	T33,T32,~CPUCTRL0_RFE
	sw	T33,0(T31)

	/*
	 * test the data path to the MC by walking a 1 through the
	 * main memory access configuration parameters register
	 */
	LI	T34,PHYS_TO_K1(CPU_MEMACC)
	lw	T30,0(T34)		# save old value
	li	a1,1<<27		# bits 28..31 are reserved
1:
	sw	a1,0(T34)
	lw	a2,0(T34)
	bne	a1,a2,loop		# loop if data is bad

	srl	a1,1
	bne	a1,zero,1b

	sw	T30,0(T34)		# restore the old value
	sw	T32,0(T31)

	/*
	 * test the data path (all 64 bits) to the HPC by walking a 1 through
	 * two different ethernet registers.  the two registers are accessed
	 * through different halves of the GIO bus
	 */
	LI	T34,PHYS_TO_K1(HPC3_ETHER_RX_NBDP)
	li	a1,1<<31
2:
	sw	a1,0(T34)
	lw	a2,0(T34)
	bne	a1,a2,loop		# loop if data is bad

	srl	a1,1
	bne	a1,zero,2b

	LI	T34,PHYS_TO_K1(HPC3_ETHER_RX_CBP)
	li	a1,1<<31
2:
	sw	a1,0(T34)
	lw	a2,0(T34)
	bne	a1,a2,loop		# loop if data is bad

	srl	a1,1
	bne	a1,zero,2b
	j	RA3

loop:
	move	a0,T34
	jal	pon_memerr		# this may not show up on screen
4:
	li	a1,1<<27		# bits 28..31 may be reserved
5:
	sw	a1,0(T34)
	lw	a2,0(T34)
	srl	a1,1
	bne	a1,zero,5b
	b	4b

	END(pon_mc)
