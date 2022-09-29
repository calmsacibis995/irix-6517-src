#ident	"lib/libsc/lib/clientasm.s: $Revision: 1.14 $"

/*
 * clientasm.s -- assembler routines for transfering to and from client code
 */

#include <ml.h>
#include <regdef.h>
#include <asm.h>

/*
 * client_start(argc, argv, environ, client_pc, client_sp);
 *
 * WARNING: dbgmon expects to be transfered to as:
 *	dbgmon(argc, argv, environ, client_pc)
 * In this case, the prom doesn't know an appropriate client_pc, so
 * client_start is currently written to pass the entry pc of the dbgmon
 * itself to the dbgmon.  This suffices to allow the dbgmon to come up
 * where the user can change the initial pc by hand.  (The dbgmon receives
 * the correct pc when invoked normally via check_dbg() in the saio library.)
 * If you change this, make sure a3 contains a accessable word address so
 * that a manually loaded dbgmon won't crap out when attempting to disassemble
 * the entry pc.
 */

ARGSAVES= (6*BPREG)
CLIENTFRM=(2*BPREG)
NESTED(client_start, CLIENTFRM, zero)
	PTR_SUBU	sp,ARGSAVES
	PTR_SUBU	sp,CLIENTFRM
	sreg	ra,CLIENTFRM-BPREG(sp)
	sreg	a0,CLIENTFRM(sp)	# flush_cache modifies a0, so, we
	sreg	a1,CLIENTFRM+BPREG(sp)	# save a0..a3 on stack
	sreg	a2,CLIENTFRM+(2*BPREG)(sp)
	sreg	a3,CLIENTFRM+(3*BPREG)(sp)
#if (_MIPS_SIM != _ABIO32)
	sreg	a4,CLIENTFRM+(4*BPREG)(sp)
#endif
#ifndef LIBSL
	/* libsl does not know flush_cache symbol and uses arcs flush */
	jal	flush_cache		# make sure i cache is consistent
#endif
	lreg	a0,CLIENTFRM(sp)
	lreg	a1,CLIENTFRM+BPREG(sp)
	lreg	a2,CLIENTFRM+(2*BPREG)(sp)
	lreg	a3,CLIENTFRM+(3*BPREG)(sp)
	lreg	v1,CLIENTFRM+(4*BPREG)(sp)	# client_sp

	bnez	v1,1f			# run client on current sp
	move	v1,sp			# if client_sp is zero
1:	PTR_SUBU	v1,4*BPREG
	sreg	sp,(v1)			# store cur sp on new sp
	PTR_SUBU	v1,4*BPREG
	move	sp,v1			# switch stacks
	jal	a3			# call client
	PTR_ADDU	sp,4*BPREG
	lreg	v1,(sp)
	move	sp,v1			# get sp back
	lreg	ra,CLIENTFRM-BPREG(sp)
	PTR_ADDU	sp,CLIENTFRM
	PTR_ADDU	sp,ARGSAVES
	j	ra
	END(client_start)
