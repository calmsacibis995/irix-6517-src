/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1996, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident  "$Revision: 1.8 $"

#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/loaddrs.h>
#include <sys/regdef.h>
#include <asm.h>
#include <sys/SN/agent.h>

#define SLAVESTACK_SIZE 4096


#ifdef SABLE_SYMMON
LEAF(sable_symmon_slave_launch)
	.set noreorder

	# a0 contains a CPU number.
        li	t0, SLAVESTACK_SIZE
	move	t1, zero

	LA	a2, slave_boot_stacks

	# Add SLAVESTACK_SIZE to the current stack pointer
1:	PTR_ADD	a2, SLAVESTACK_SIZE

	# If we're not to the current CPU number, try again.
	bne	t1, a0, 1b
	add	t1, 1		# (BD)

	# Now we've made it to out CPU number.

	PTR_SUB	a2, 16		# Back us up one long double

	LA	a1, bootstrap

	/* a0 == cpu number
	 * a1 == launch address
	 * a2 == stack pointer
	 */
	LI	t0,SABLE_SYMMON_INITPC+16
	lw	t1,(t0)
	beq	t1,zero,1f
	nop
	jr	t0
	nop
1:	
	j	ra
	nop	
	
	END(sable_symmon_slave_launch)
#endif /* SABLE_SYMMON */

#if 0	
/*
 * void slave_loop(launch_t *)
 *	slave_loop watches this CPU's MPCONF launch address and jumps
 *		to any address stored into it.
 */
LEAF(slave_loop)
	.set noreorder

#ifdef SABLE
	# a0 contains a CPU number.
        li	t0, SLAVESTACK_SIZE
	move	t1, zero

	LA	sp, slave_boot_stacks

	# Add SLAVESTACK_SIZE to the current stack pointer
1:	PTR_ADD	sp, SLAVESTACK_SIZE

	# If we're not to the current CPU number, try again.
	bne	t1, a0, 1b
	add	t1, 1		# (BD)

	# Now we've made it to out CPU number.

	PTR_SUB	sp, 16		# Back us up one long double

#endif
#ifdef SABLE_SYMMON
	/* If symmon is loaded, then jump to "slave loop" */
	/* a0 == cpu number (not currently used)
	 * sp == stack pointer for this cpu
	 */
	LI	t0,SABLE_SYMMON_INITPC+8
	lw	t1,(t0)
	beq	t1,zero, 1f
	nop
	jr	t0
	nop
1:		
#endif /* SABLE_SYMMON */		

	# Load an address
	# Wait for a launch address to be stored.

	# for now, just wait for a release
	LA	t0, slave_release

1:	lw	t1, 0(t0)
	beqz	t1, 1b
	nop

	LA	t1, bootstrap

	jr	t1
	nop			# (BD)

	END(slave_loop)

	.align 7
LEAF(slave_entry)
	LA	t0,slave_loop
	j	t0
	nop
	END(slave_entry)
	.align 7

LEAF(start_slave_loop)
	.set	noreorder
#ifdef notyet
	/* 
	 *Launch slaves into kernel slave loop, in a stackless function.
	 */
	LA	t2,slave_entry
	
	KPHYSTO32K1(t2);

	LI	t0,MPCONF_ADDR
	li	t1,EV_MAX_CPUS
1:
	/*
	 * Zero out stuff that should be zero (for saftey).
	 */
	sw	zero,MP_STACKADDR(t0)
#if _MIPS_SIM == _ABI64
	PTR_S	zero, MP_REAL_SP(t0)
#else
	sw	zero,FILLER(t0)
	sw	zero,FILLER+4(t0)
#endif
	move	t3,t0			/* MPCONF pointer */
	KPHYSTO32K1(t3)
	sw	t3,MP_LPARM(t0)
	sw	t2,MP_LAUNCHOFF(t0)

	sub	t1,1
	bgtz	t1,1b
	PTR_ADDU t0,MPCONF_SIZE

	/* Wait a while to let them get going .... */

	li	t0,0xffffff
2:	bgtz	t0,2b
	sub	t0,1

#endif /* notyet */

	j	ra
	nop
	END(start_slave_loop)


#ifdef SABLE

	.data
	.align	7
	.globl slave_boot_stacks
slave_boot_stacks:
	.space	SLAVESTACK_SIZE * MAXCPUS
#endif

#endif

#if defined (CELL_IRIX) && defined (LOGICAL_CELLS)	
#include <sys/SN/addrs.h>
	
LEAF(slave_cell_start)
	LA	t1, start
	dli	v0, LOCAL_HUB_ADDR(NI_STATUS_REV_ID);
	ld	v0, 0(v0)
	dli	v1, NSRI_NODEID_MASK
	and	v0, v1
	dli	v1, NSRI_NODEID_SHFT
	dsrl	v0, v1
	dsll32	v0, 0
	dli	v1, IO6_PROM_OFFSET
	or	v0, v1
	dli	v1, K0BASE
	or	v0, v1
	ld	a0, 0(v0)
	ld	a1, 8(v0)
	ld	a2, 16(v0)
	jal	start
	END(slave_cell_start)
#endif /* CELL_IRIX && LOGICAL_CELLS */
