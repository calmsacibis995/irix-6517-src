#ifdef	MP
#include <sys/asm.h>
#include <sys/regdef.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/RACER/racermp.h>

/*
 * void slave_loop(mpconf_t *)
 *	slave_loop watches this CPU's MPCONF launch address and jumps
 *		to any address stored into it.
 */
	.align 7
LEAF(slave_loop)
	.set	noreorder

	/*
	 * indicate that we are ready ....
	 * a0 is passed from launch_slave() in arcs/lib/libsk/ml/IP30.c
	 */
	lw	t0,MP_VIRTID(a0)
	LA	t1,slave_loop_ready
	PTR_ADDU	t0,t1		# address of ready flag
	li	t1,1
	sb	t1,0(t0)		# set ready flag
1:
	li	t0,0xffffff		# down counter

	# avoid hogging the bus by spinning in a tight cached loop
2:	bne	t0,zero,2b
	subu	t0,1			# BDSLOT

	PTR_L	t0,MP_LAUNCHOFF(a0)
	beq	t0,zero,1b
	nop				# BDSLOT

	# load stack pointer
	PTR_L	sp,MP_STACKADDR(a0)	# pointer to top of the stack
	PTR_S	zero,MP_LAUNCHOFF(a0)	# clear the launch address
	PTR_L	a0,MP_LPARM(a0)		# argument to the launch function

	jal	t0
	nop				# BDSLOT

	/* NOTREACHED */

	.set	reorder
	END(slave_loop)

	.align 7
LEAF(start_slave_loop)
	.set	noreorder

	/* launch slaves into kernel slave loop, in a stackless function */
	LA	t2,slave_loop
	
	LI	t0,MPCONF_ADDR
	li	t1,MAXCPU
1:
	/* zero out stuff that should be zero (for saftey) */
	PTR_S	zero,MP_STACKADDR(t0)

	PTR_S	t0,MP_LPARM(t0)
	PTR_S	t2,MP_LAUNCHOFF(t0)	# must be last

	sub	t1,1
	bne	t1,zero,1b
	PTR_ADDU	t0,MPCONF_SIZE	# BDSLOT

	j	ra
	nop				# BDSLOT

	.set	reorder
	END(start_slave_loop)
#endif	/* MP */
