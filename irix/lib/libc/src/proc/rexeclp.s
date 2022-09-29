#ident "$Id: rexeclp.s,v 1.1 1996/03/16 01:51:27 jwag Exp $"

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

/*
 *	rexeclp(cell, name, arg,...,0)	(like rexecl, but does path search)
 */
	.weakext	rexeclp, _rexeclp

NESTED(_rexeclp, FRAMESZ, ra)
        .mask   M_RA, RAOFF-FRAMESZ
	SETUP_GP
	PTR_SUBU sp,FRAMESZ
	SETUP_GP64(GPOFF,_rexeclp)
	SAVE_GP(GPOFF)

	REG_S	ra,RAOFF(sp)
	/* rexecvp is basically a varargs so need to home all args */
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
	/* We will call rexecvp - so we need to roll up the doubleword
	 * aligned pointers to be only word aligned, since this is what
	 * rexecvp (and the kernel) expects. Since these are pointers,
	 * and pointers are 32bits, there is no data loss.
	 * The idea is to compact the doubleword args to be a vector
	 * of char *, in place.
	 */
	move	a3,a2		# a3 points to our source area. (A2HOME)
	move	t0,a2		# t0 points to our target area. (A2HOME)
	addu	a3,4		# a3 points to A2HOME + 4
1:	PTR_L	v0,0(a3)
	PTR_ADDU a3,SZREG	# Step to next doubleword aligned arg.
	PTR_S	v0,0(t0)	# store arg as singleword
	addu	t0,4		# step to next singleword alignment.
	bne	v0,zero,1b
#endif	/* _MIPS_SIM_NABI32 */
	jal	_rexecvp	# _rexecvp(cell, file, argv)
	REG_L	ra,RAOFF(sp)
	RESTORE_GP64
	PTR_ADDU sp,FRAMESZ
	j	ra
	END(_rexeclp)		# _rexeclp(cell, file, arg1, arg2, ..., 0);
