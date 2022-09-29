#include <sys/cpu.h>
#include <sys/sbd.h>
#include <sys/regdef.h>
#include <sys/fpregdef.h>
#include <asm.h>

#define PARITY_ERR_MASK		PAR_SYSAD_SYND_TCC | PAR_DATA_SYND_DB0 | \
				PAR_DATA_SYND_DB1

/* void *fp_ls(pattern, addr, len_in_longs)
 */
LEAF(fp_ls)
        .set    noreorder
	move	t1,a1				# test address
	daddu	t0,t1,a2			# ending address

	dli	a3,PHYS_TO_K1(TCC_PARITY)
	dmtc1	a0,ft2

	/*  Write a cache-line out, trying to do two stores per cycle, for
	 * 8 cycles.
	 */
	.align	4
1:	sdc1	ft2,0(t1)			# even
	sdc1	ft2,8(t1)			# odd
	sdc1	ft2,16(t1)			# even
	sdc1	ft2,24(t1)			# odd
	sdc1	ft2,32(t1)			# even
	sdc1	ft2,40(t1)			# odd
	sdc1	ft2,48(t1)			# even
	sdc1	ft2,56(t1)			# odd
	sdc1	ft2,64(t1)			# even
	sdc1	ft2,72(t1)			# odd
	sdc1	ft2,80(t1)			# even
	sdc1	ft2,88(t1)			# odd
	sdc1	ft2,96(t1)			# even
	sdc1	ft2,104(t1)			# odd
	sdc1	ft2,112(t1)			# even
	sdc1	ft2,120(t1)			# odd

	/* Check if we got a parity error.
	 */
	ld	a4,0(a3)			# get TCC_PARITY
	and	a4,PARITY_ERR_MASK
	bnez	a4,fpls_error
	nada

	daddiu	t1,128				# next cache line
	bltu	t1,t0,1b
	nada

	/* Read data with two loads per cycle.
	 */
	move	t1,a1				# test address
	.align	4
1:	ldc1	ft4,0(t1)			# even
	ldc1	ft5,0(t1)			# odd
	dmfc1	t3,ft4				# check even
	nada
	bne	a0,t3,fpls_error
	nada
	dmfc1	t3,ft5				# check odd
	nada
	bne	a0,t3,fpls_error
	daddiu	t1,8				# BDSLOT: correct address
	daddiu	t1,8				# next quad
	bltu	t1,t0,1b
	nada

	b	done				# no errors
	move	v0,zero				# BDSLOT

fpls_error:
	move	v0,t1				# return error address

done:
        j      ra
	nada
	.set	reorder
END(fp_ls)
