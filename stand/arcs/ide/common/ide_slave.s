#ident "ide/ide_slave.s: $Revision: 1.8 $"

/*
 * ide_slave.s -- startup code for ide slaves
 */

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys/sbd.h>
#include <ml.h>
#include <fault.h>
#include "ide_mp.h"

/* callshared(argc,argv,func):
 *
 * For modular ide -- need to set-up t9 (fp) to be set to the address of
 * the calling function as shared code requires this register to be set.
 */
CALLSHAREDSIZE=(2*SZREG)
NESTED(callshared,CALLSHAREDSIZE,zero)
	PTR_SUBU sp,CALLSHAREDSIZE	# set-up stack frame
	REG_S	t9,SZREG(sp)		# save caller t9
	REG_S	ra,0(sp)		# save caller ra
	move	t9,a2			# put addr in t9 for the callee
	jalr	a2			# call function
	REG_L	t9,SZREG(sp)		# restore t9
	REG_L	ra,0(sp)		# restore ra
	PTR_ADDU sp,CALLSHAREDSIZE	# restore stack frame
	j	ra
	END(callshared);

#if MULTIPROCESSOR
/*
 * only MP architectures use this slave_startup routine
 */
/*
 * slave_start expects to be called with pda->initial_stack
 * pointing to the top of its normal stack (EXSTKSZ < the end of
 * its stack area).  This code therefore relies on the PDAs already
 * being initialized by the master thread.  It uses the initial_stack
 * value to calculate a per-processor-unique exception SP, builds a fault
 * stack, and saves that TOS addr in pda->fault_sp.
 */
STARTFRM=	EXSTKSZ			# leave room for fault stack
NESTED(slave_startup, STARTFRM, zero)
	.set noreorder
	li	t0,IDE_SLAVE_SR
	mtc0	t0,C0_SR
	nop
	.set reorder
	move	v0,sp
	.set noreorder
	_get_spda			# t1: <-- slave's pda
	.set reorder

	# sp is passed in from call_coroutine()
	PTR_ADDU	sp,EXSTKSZ
	PTR_SUBU	sp,NARGSAVE*SZREG	# leave room for argsaves
	sreg	sp,GPDA_FAULT_SP(t1)	# this is slave's fault-handling stack

	PTR_SUBU	sp,STARTFRM	# fault stack can grow to here
	sreg	zero,STARTFRM-SZREG*1(sp)	# keep debuggers happy
	sreg	ra,STARTFRM-SZREG*2(sp)
	sreg	v0,STARTFRM-SZREG*3(sp)	# save caller's sp

	sreg	sp,GPDA_INITIAL_SP(t1)

	# assume slave caches contain garbage: invalidate, don't flush lines
#if IP21
	jal	ide_invalidate_caches
#elif !IP30
	jal	invalidate_caches
#endif

	jal	ide_slave_boot

	END(slave_startup)

#endif /* MULTIPROCESSOR */

