#ident "$Id: uname.s,v 1.12 1994/10/03 04:33:40 jwag Exp $"

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys.s>
#include "sys/syscall.h"

	.weakext	uname, _uname

LEAF(_uname)
	li	a2,0
	li	a3,0
	.set	noreorder
	li	v0,SYS_utssys
	syscall
	bne	a3,zero,9f
	nop
	move	v0,zero
	RET(uname)	/* defines 9f, etc */
