
#ident "$Id: pcreateve_tp.s,v 1.1 1996/08/21 19:50:52 jwag Exp $"

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys.s>

/*
 * tracepoints for perf package
 */
	.weakext	__tp_pcreateve, ___tp_pcreateve
LEAF(___tp_pcreateve)
	li	v0, 0
	j	ra
	END(___tp_pcreateve)
