
#ident "$Id: exit_tp.s,v 1.2 1996/08/21 19:49:15 jwag Exp $"

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys.s>

/*
 * tracepoints for perf package
 */
	.weakext	__tp_exit, ___tp_exit
LEAF(___tp_exit)
	j	ra
	END(___tp_exit)
