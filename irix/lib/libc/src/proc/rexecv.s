#ident "$Id: rexecv.s,v 1.1 1996/03/16 01:51:29 jwag Exp $"

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys.s>
#include "sys/syscall.h"


LOCALSZ=	2		# save ra, gp
FRAMESZ=	(((NARGSAVE+LOCALSZ)*SZREG)+ALSZ)&ALMASK
RAOFF=		FRAMESZ-(1*SZREG)
GPOFF=		FRAMESZ-(2*SZREG)

	.text
	.weakext	rexecv, _rexecv

NESTED(_rexecv, FRAMESZ, ra)
        .mask   M_RA, RAOFF-FRAMESZ
	SETUP_GP
	PTR_SUBU sp,FRAMESZ
	SETUP_GP64(GPOFF,_rexecv)
	SAVE_GP(GPOFF)

	REG_S	ra,RAOFF(sp)
	PTR_L	a3,_environ
	jal	_rexecve
	REG_L	ra,RAOFF(sp)
	RESTORE_GP64
	PTR_ADDU sp,FRAMESZ
	j	ra
	END(_rexecv)			# _rexecv(cell, file, argv)
