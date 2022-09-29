
#ident "$Id: execve_tp.s,v 1.2 1996/08/21 19:49:08 jwag Exp $"

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys.s>

/*
 * tracepoints for perf package
 */
	.weakext	__tp_execve, ___tp_execve
	.weakext	__tp_execve_error, ___tp_execve_error
LEAF(___tp_execve)
	li	v0, 0
	j	ra
	END(___tp_execve)

LEAF(___tp_execve_error)
	j	ra
	END(___tp_execve_error)
