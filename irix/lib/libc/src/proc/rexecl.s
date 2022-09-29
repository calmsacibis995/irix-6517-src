#ident "$Id: rexecl.s,v 1.1 1996/03/16 01:51:23 jwag Exp $"

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys.s>
#include "sys/syscall.h"


#if (_MIPS_SIM == _MIPS_SIM_ABI32)
LOCALSZ=	2		# save ra, gp
#else	/* _MIPS_SIM_ABI64 or _MIPS_SIM_NABI32 */
LOCALSZ=	10		# save ra, gp, home 8 args
#endif
FRAMESZ=	(((NARGSAVE+LOCALSZ)*SZREG)+ALSZ)&ALMASK

#if (_MIPS_SIM == _MIPS_SIM_ABI32)
RAOFF=		FRAMESZ-(1*SZREG)
GPOFF=		FRAMESZ-(2*SZREG)
A0HOME=		FRAMESZ+(0*SZREG)
A1HOME=		FRAMESZ+(1*SZREG)
A2HOME=		FRAMESZ+(2*SZREG)
A3HOME=		FRAMESZ+(3*SZREG)
#else	/* _MIPS_SIM_ABI64 or _MIPS_SIM_NABI32 */
A7HOME=		FRAMESZ-(1*SZREG)
A6HOME=		FRAMESZ-(2*SZREG)
A5HOME=		FRAMESZ-(3*SZREG)
A4HOME=		FRAMESZ-(4*SZREG)
A3HOME=		FRAMESZ-(5*SZREG)
A2HOME=		FRAMESZ-(6*SZREG)
A1HOME=		FRAMESZ-(7*SZREG)
A0HOME=		FRAMESZ-(8*SZREG)
RAOFF=		FRAMESZ-(9*SZREG)
GPOFF=		FRAMESZ-(10*SZREG)
#endif
	.weakext	rexecl, _rexecl

NESTED(_rexecl, FRAMESZ, ra)
        .mask   M_RA, RAOFF-FRAMESZ
	SETUP_GP
	PTR_SUBU sp,FRAMESZ
	SETUP_GP64(GPOFF,_rexecl)
	SAVE_GP(GPOFF)

	REG_S	ra,RAOFF(sp)
	/* rexecv is basically a varargs so need to home all args */
	REG_S	a2,A2HOME(sp)
	REG_S	a3,A3HOME(sp)
#if (_MIPS_SIM != _MIPS_SIM_ABI32)
	REG_S	a4,A4HOME(sp)
	REG_S	a5,A5HOME(sp)
	REG_S	a6,A6HOME(sp)
	REG_S	a7,A7HOME(sp)
#endif
	PTR_ADDIU a2,sp,A2HOME
#if _MIPS_SIM == _MIPS_SIM_NABI32
	/* We will call rexecv - so we need to roll up the doubleword
	 * aligned pointers to be only word aligned, since this is what
	 * rexecv (and the kernel) expects. Since these are pointers,
	 * and pointers are 32bits, there is no data loss.
	 * The idea is to compact the doubleword args to be a vector
	 * of char *, in place.
	 */
	move	t1,a1		# t1 points to our source area. (A2HOME)
	move	t0,a1		# t0 points to our target area. (A2HOME)
	addu	t1,4		# t1 points to A2HOME + 4
1:	PTR_L	v0,0(a2)
	PTR_ADDU t1,SZREG	# Step to next doubleword aligned arg.
	PTR_S	v0,0(t0)	# store arg as singleword
	addu	t0,4		# step to next singleword alignment.
	bne	v0,zero,1b
#endif	/* _MIPS_SIM_NABI32 */
	jal	_rexecv		# _rexecv(cell, file, argv)
	REG_L	ra,RAOFF(sp)
	RESTORE_GP64
	PTR_ADDU sp,FRAMESZ
	j	ra
	END(_rexecl)		# _rexecl(cell, file, arg1, arg2, ..., 0);
