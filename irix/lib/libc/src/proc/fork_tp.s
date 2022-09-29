
#ident "$Id: fork_tp.s,v 1.2 1996/08/21 19:49:28 jwag Exp $"

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys.s>

/*
 * tracepoints for perf package
 */
	.weakext	__tp_fork_pre, ___tp_fork_pre
	.weakext	__tp_fork_parent, ___tp_fork_parent
	.weakext	__tp_fork_child, ___tp_fork_child
LEAF(___tp_fork_pre)
	j	ra
	END(___tp_fork_pre)

LEAF(___tp_fork_parent)
	j	ra
	END(___tp_fork_parent)

LEAF(___tp_fork_child)
	j	ra
	END(___tp_fork_child)
