#ident	"IP12prom/csu.s:  $Revision: 1.8 $"

/*
 * csu.s -- prom startup code
 */

#include <fault.h>
#include <asm.h>
#include <regdef.h>
#include <sys/cpu.h>
#include <sys/sbd.h>

#define BSSMAGIC 0xfeeddead

/*******************************************************************/
/* 
 * ARCS Prom entry points
 */

#define ARCS_HALT	1
#define ARCS_POWERDOWN	2
#define ARCS_RESTART	3
#define ARCS_REBOOT	4
#define ARCS_IMODE	5

LEAF(Halt)
	li	s0,ARCS_HALT
	la	k0,arcs_common
	jr	k0
	END(Halt)

LEAF(PowerDown)
	li	s0,ARCS_POWERDOWN
	la	k0,arcs_common
	jr	k0
	END(PowerDown)

LEAF(Restart)
	li	s0,ARCS_RESTART
	la	k0,arcs_common
	jr	k0
	END(Restart)

LEAF(Reboot)
	li	s0,ARCS_REBOOT
	la	k0,arcs_common
	jr	k0
	END(Reboot)

/*  EnterInteractive mode goes to prom menu.  If called from outside the
 * prom or bssmagic, or envdirty is true, do full clean-up, else just
 * re-wind the stack to keep the prom responsive.
 */
LEAF(EnterInteractiveMode)
	la	t0, _ftext
	la	t1, _etext
	sgeu	t2,ra,t0
	sleu	t3,ra,t1
	and	t0,t2,t3
	beq	t0,zero,1f

	# check if envdirty true (program was loaded)
	lw	v0,envdirty
	bne	v0,zero,1f

	# light clean-up
	jal	real_main
	# if main fails fall through and re-init
1:
	# go whole hog
	li	s0,ARCS_IMODE
	la	k0,arcs_common
	jr	k0
	END(EnterInteractiveMode)

LEAF(arcs_common)
	jal	clear_prom_ram		# clear_prom_ram initializes sp
	move	a0,zero
	jal	init_prom_soft		# initialize saio and everything else

	bne	s0,ARCS_IMODE,1f
	jal	real_main
	j	never			# never gets here
1:
	bne	s0,ARCS_HALT,1f
	jal	halt
	j	never			# never gets here
1:
	bne	s0,ARCS_POWERDOWN,1f
	jal	powerdown
	j	never			# never gets here
1:
	bne	s0,ARCS_RESTART,1f
	jal	restart
	j	never			# never gets here
1:
	bne	s0,ARCS_REBOOT,never
	jal	reboot

never:	la	k0,_exit		# never gets here
	jr	k0
	END(arcs_common)

/*
 * _exit()
 *
 * Re-enter prom command loop without initialization
 * Attempts to clean-up things as well as possible while
 * still maintaining things like current enabled consoles, uart baud
 * rates, and environment variables.
 */
LEAF(_exit)

	lw	v0,bssmagic		# Check to see if bss valid
	bne	v0,BSSMAGIC,_init	# If not, do a hard init

	sw	sp,_fault_sp
	subu	sp,EXSTKSZ		# top 1/4 of stack for fault handling
	jal	config_cache		# determine cache sizes
	jal	flush_cache		# flush cache
	jal	enter_imode
1:	b	1b			# never gets here
	END(_exit)

/*
 * _init -- reinitialize prom and reenter command parser
 */
LEAF(_init)
	jal	clear_prom_ram		# clear_prom_ram initializes sp
	move	a0,zero
	jal	init_prom_soft
	jal	real_main
	jal	EnterInteractiveMode	# shouldn't get here
	END(_init)

LEAF(init_prom_ram)
	j	ra
	END(init_prom_ram)

	BSS(bssmagic,4)
