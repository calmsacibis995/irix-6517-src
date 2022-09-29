
#ident "$Id: csu_tp.s,v 1.2 1996/08/21 19:49:01 jwag Exp $"

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys.s>

/*
 * tracepoints for perf package
 */
	.weakext	__tp_main, ___tp_main
LEAF(___tp_main)
	j	ra
	END(___tp_main)
