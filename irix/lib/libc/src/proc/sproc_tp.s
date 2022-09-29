
#ident "$Id: sproc_tp.s,v 1.2 1996/08/21 19:50:24 jwag Exp $"

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys.s>

/*
 * tracepoints for perf package
 */
	.weakext	__tp_sproc_pre, ___tp_sproc_pre
	.weakext	__tp_sproc_parent, ___tp_sproc_parent
	.weakext	__tp_sproc_child, ___tp_sproc_child
LEAF(___tp_sproc_pre)
	j	ra
	END(___tp_sproc_pre)

LEAF(___tp_sproc_parent)
	j	ra
	END(___tp_sproc_parent)

LEAF(___tp_sproc_child)
	j	ra
	END(___tp_sproc_child)
