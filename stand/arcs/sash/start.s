/* needed to pull in the csu.s from libsc */

#include <asm.h>
#include <regdef.h>

LEAF(__start)
	j	startsc
	END(__start)
