/***********************************************************************\
*	File:		launch.s					*
*									*
*	Common slave loop/launcher					*
*									*
*	This code is accessible from special vectors defined in		*
*	sys/SN/launch.h and is used by IP27prom, IO6PROM, MDK,		*
*	Unix, etc.							*
*									*
*	Important notes:						*
*									*
*	This code must be able to run directly out of the PROM at	*
*	0xffffffffbfc00000 even though it is linked at			*
*	0xc00000001fc00000.  Hence it must not use 'jr' or		*
*	otherwise use labels naively.  It also has no stack.		*
*									*
\***********************************************************************/

#ident "$Revision: 1.19 $"

#include <asm.h>

#include <sys/regdef.h>
#include <sys/sbd.h>

#include <sys/SN/agent.h>
#include <sys/SN/launch.h>
#include <sys/SN/kldiag.h>

#include "cache.h"
#include "hub.h"
#include "prom_leds.h"
#include "pod.h"
#include "ip27prom.h"

	.text
	.set	at
	.set	noreorder

#if SABLE
#define FLASH_PERIOD	100
#define POLL_PERIOD	25
#else
#define FLASH_PERIOD	20000
#define POLL_PERIOD	10000
#endif

/*
 * launch_t *launch_get(int nasid, int cpu);
 *
 *   Returns the address of a node/CPU's launch area by looking
 *   it up in KLDIR.
 *
 *   Arguments:
 *	nasid: address space identifier of launch area node
 *	cpu: which CPU on node, 0 or 1
 *   Returns:
 *	v0 pointing to the CPU's LAUNCH structure in K1 space
 *   Prerequisites:
 *	KLDIR must already be initialized.
 *   Uses:
 *	t1-t2, v0-v1
 */

LEAF(launch_get)
	/* Change NASID in a0 to node address space bits in t1 */

	dsll	t1, a0, NASID_SHFT

	/* Get K1 address of KLDIR entry for appropriate LAUNCH into v0 */

	dli	v1, K1BASE
	or	v0, t1, v1
	dli	v1, KLDIR_OFFSET + (KLI_LAUNCH * KLDIR_ENT_SIZE)
	or	v0, v1

	/* Multiply CPU number by stride */

	move	t2, zero
	beqz	a1, 1f
	 nop
	ld	t2, KLDIR_OFF_STRIDE(v0)
1:

	/* Get offset to LAUNCH in v0 appropriate to CPU */

	ld	v0, KLDIR_OFF_OFFSET(v0)
	daddu	v0, t2

	/* Convert offset to K1 address in correct node address space */

	dli	v1, K1BASE
	or	v0, v1
	or	v0, t1

	j	ra
	 nop
	END(launch_get)

/*
 * __psunsigned_t launch_get_current(void);
 *
 *   Returns the address of the calling CPU's launch area by looking
 *   it up in KLDIR.
 *
 *   Returns:
 *	v0 pointing to the LAUNCH structure (in K1 space)
 *   Prerequisites:
 *	KLDIR must already be initialized.
 *	NASID must be programmed in the calling node's NI_STATUS_REV_ID.
 *   Uses:
 *	a0-a1, t1-t2, v0-v1
 */

LEAF(launch_get_current)
	/* Get current NASID in a0 */

	dli	a0, LOCAL_HUB(NI_STATUS_REV_ID)
	ld	a0, 0(a0)
	dsrl	a0, NSRI_NODEID_SHFT
	and	a0, (NASID_MASK >> NASID_SHFT)

	/* Get current CPU number in a1 */

	dli	a1, LOCAL_HUB(PI_CPU_NUM)
	ld	a1, 0(a1)

	/* Args. set up; continue in launch_get */

	b	launch_get
	 nop
	END(launch_get_current)

/*
 * void launch_loop(void);
 *
 *   Initializes the launch structure for the current CPU.
 *   Enters the launch loop and does not return.
 *   Slaves must already be in launch loop when master calls launch_slave.
 */

LEAF(launch_loop)
	jal	launch_get_current
	 nop
	move	t3, v0

	/* Figure out which interrupt to use and clear it */

	HUB_CPU_GET()
	add	t0, v0, IP27PROM_INT_LAUNCH

	HUB_INT_CLEAR(t0)

	sync

	/* Initialize the launch area, including turning off busy flag */

	dli	v0, LAUNCH_MAGIC
	sd	v0, LAUNCH_OFF_MAGIC(t3)
	sd	zero, LAUNCH_OFF_BUSY(t3)
	sd	zero, LAUNCH_OFF_CALL(t3)
	sd	zero, LAUNCH_OFF_CALLC(t3)
	sd	zero, LAUNCH_OFF_CALLPARM(t3)
	sd	zero, LAUNCH_OFF_STACK(t3)
	sd	zero, LAUNCH_OFF_GP(t3)
	sd	zero, LAUNCH_OFF_BEVUTLB(t3)
	sd	zero, LAUNCH_OFF_BEVNORMAL(t3)
	sd	zero, LAUNCH_OFF_BEVECC(t3)

	/* Wait for interrupt, with blinking LEDs */

	li	t2, PLED_LAUNCHLOOP	/* LED value */

1:	li	t1, FLASH_PERIOD	/* LED counter */
	HUB_LED_SET(t2)

2:	HUB_INT_TEST(t0)
	bnez	v0, 5f
	 nop
	add	t1, -1
	bnez	t1, 2b
	 nop

	 /* Check flag so we soon launch even if interrupts are broken */

	ld	v0, LAUNCH_OFF_BUSY(t3)
	bnez	v0, 5f
	 nop

	b	1b
	 xori	t2, PLED_LAUNCHLOOP

5:	/*
	 * Interrupt seen.
	 * Verify the integrity of the launch structure.
	 */

	HUB_LED_SET(PLED_LAUNCHINTR)

	ld	v0, LAUNCH_OFF_MAGIC(t3)
	dli	v1, LAUNCH_MAGIC
	bne	v0, v1, skip_call
	 nop

	ld	v0, LAUNCH_OFF_BUSY(t3)
	beqz	v0, skip_call
	 nop

	ld	t2, LAUNCH_OFF_CALL(t3)
	ld	v0, LAUNCH_OFF_CALLC(t3)
	sd	zero, LAUNCH_OFF_CALLC(t3)
	daddu	v0, t2
	daddu	v0, 1
	bnez	v0, skip_call
	 nop

	/* Flush data caches and call routine */

#if 0
	HUB_LED_SET(PLED_FLUSHCACHES)

	jal	cache_flush
	 nop
#endif

	HUB_LED_SET(PLED_LAUNCHCALL)

	ld	a0, LAUNCH_OFF_STACK(t3)
	beqz	a0, 6f
	move	sp, a0
6:

	ld	a0, LAUNCH_OFF_GP(t3)
	beqz	a0, 7f
	move	gp, a0
7:

	jalr	t2
	 ld	a0, LAUNCH_OFF_CALLPARM(t3)

	HUB_LED_SET(PLED_LAUNCHDONE)

skip_call:

	j	launch_loop	/* Turns off busy flag */
	 nop
	END(launch_loop)

/*
 * void launch_slave(int nasid, int cpu,
 *		     launch_proc_t call_addr,
 *		     __int64_t call_parm,
 *		     void *stack_addr,
 *		     void *gp_addr);
 *
 *   Stores the appropriate values into the launch structure for a
 *   specified NASID/cpu.  Sends an interrupt to launch the slave.
 *   Should be called once per slave to be launched.  Does not wait
 *   until the slaves have completed (see launch_wait).
 *
 *   Arguments:
 *	nasid: address space identifier of node to be launched
 *	cpu: which CPU on node, 0 or 1
 *	call_addr: address of subroutine for remote node to call
 *	call_parm: parameter passed to subroutine on remote node
 *	stack_addr: if non-zero, remote node sets sp before launching
 *	gp_addr: if non-zero, remote node sets gp before launching
 *   Uses:
 *	t0-t3, v0-v1
 */

LEAF(launch_slave)
	move	t0, ra

	jal	launch_get		/* Does not trash a0-a1 */
	 nop
	move	t3, v0

	sd	a2, LAUNCH_OFF_CALL(t3)
	not	v0, a2
	sd	v0, LAUNCH_OFF_CALLC(t3)
	sd	a3, LAUNCH_OFF_CALLPARM(t3)

	sd	a4, LAUNCH_OFF_STACK(t3)
	sd	a5, LAUNCH_OFF_GP(t3)

	dli	v0, 1
	sd	v0, LAUNCH_OFF_BUSY(t3)

	/* Figure out which interrupt to use and send it */

	add	t1, a1, IP27PROM_INT_LAUNCH

	HUB_INT_SET_REMOTE(a0, t1)

	j	t0
	 nop
	END(launch_slave)

/*
 * int launch_wait(int nasid, int cpu, int timeout_msec);
 *
 *   Waits until the specified CPU returns to the launch loop.
 *
 *   Arguments:
 *	nasid: address space identifier of launch to wait on
 *	cpu: which CPU on node, 0 or 1
 *	timeout_msec: timeout value, approximately in milliseconds
 *   Returns:
 *	0 if successful
 *	-1 if timed out
 *   Uses:
 *	t0-t3, v0-v1
 */

LEAF(launch_wait)
	move	t0, ra

	jal	launch_get		/* Does not trash a0-a1 */
	 nop
	move	t3, v0

1:
	ld	v0, LAUNCH_OFF_BUSY(t3)
	beqz	v0, 3f
	 nop

	li	v0, POLL_PERIOD
2:	bnez	v0, 2b
	 sub	v0, 1

	sub	a2, 1
	bnez	a2, 1b
	 nop

	j	t0
	 li	v0, -1

3:
	j	t0
	 li	v0, 0
	END(launch_wait)

/*
 * launch_state_t launch_poll(int nasid, int cpu);
 *
 *   Polls the state of a CPU that has been launched.
 *
 *   Returns:
 *	LAUNCH_STATE_SENT if slave has not yet seen the launch.
 *	LAUNCH_STATE_RECD if slave has seen the launch but not returned.
 *	LAUNCH_STATE_DONE if slave has returned.
 *
 *   Uses: t0-t3, v0-v1
 */

LEAF(launch_poll)
	move	t0, ra

	jal	launch_get		/* Does not trash a0-a1 */
	 nop
	move	t3, v0

	ld	v1, LAUNCH_OFF_CALLC(t3)
	bnez	v1, 1f
	 li	v0, LAUNCH_STATE_SENT

	ld	v1, LAUNCH_OFF_BUSY(t3)
	bnez	v1, 1f
	 li	v0, LAUNCH_STATE_RECD

	li	v0, LAUNCH_STATE_DONE
1:
	j	t0
	 nop
	END(launch_poll)
