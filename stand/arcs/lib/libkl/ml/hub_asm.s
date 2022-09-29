/***********************************************************************\
*	File:		hub_asm.s					*
*									*
\***********************************************************************/

#ident "$Revision: 1.15 $"

#include <asm.h>

#include <sys/mips_addrspace.h>
#include <sys/cpu.h>
#include <sys/regdef.h>

#include <sys/SN/agent.h>
#include <sys/SN/SN0/IP27.h>
#include <sys/SN/SN0/ip27config.h>		/* For hub clock frequency */

#include <hub.h>

	.text
	.set	reorder

/*
 * int hub_cpu_get(void)
 *
 *   Returns 0 on CPU A, 1 on CPU B.
 */

LEAF(hub_cpu_get)
	ld	v0, LOCAL_HUB(PI_CPU_NUM)
	j	ra
	END(hub_cpu_get)

/*
 * int hub_kego(void)
 *
 *   Returns TRUE if the current node card is in a KEGO machine.
 */

LEAF(hub_kego)
        lw      v0, CONFIG_FLAGS_ADDR
        and     v0, CONFIG_FLAG_12P4I
        beqz    v0, 2f
        li      v0, 0
        j       ra
2:	lw	v0, IP27CONFIG_SN00_ADDR
	beqz	v0, 1f
	li	v0, 0
	j	ra
1:	ld	v0, LOCAL_HUB(MD_SLOTID_USTAT)
	and	v0, MSU_SLOTID_MASK
	slt	v0, v0, 3
	j	ra
	END(hub_kego)

/*
 * void hub_led_set(__uint64_t leds)
 *
 *   Sets the current CPU's LEDs
 */

LEAF(hub_led_set)
	.set	noreorder
	HUB_LED_SET(a0)
	.set	reorder
	j	ra
	END(hub_led_set)

/*
 * void hub_led_set_remote(nasid_t nasid, int cpu, __uint64_t leds)
 *
 *   Sets another CPU's LEDs
 */

LEAF(hub_led_set_remote)
	.set	noreorder
	HUB_LED_SET_REMOTE(a0, a1, a2)
	.set	reorder
	j	ra
	END(hub_led_set_remote)

/*
 * get_nasid
 */

LEAF(get_nasid)
	GET_NASID_ASM(v0)
	j	ra
	END(get_nasid)
