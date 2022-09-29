/* fabs - floating absolute value */

#include <regdef.h>
#include <sys/asm.h>
#include <errno.h>
.rdata
.align 3
_local_qnan:	.word	0x7ff10000,0

	.text
LEAF(fabs)
	SETUP_GP
	USE_ALT_CP(t2)
	SETUP_GP64(t2,fabs)
	c.un.d	$f12, $f12
	abs.d $f0,$f12
	bc1f	1f
	/* set errno to EDOM */
        li      a0, EDOM
        PTR_L   t1, __errnoaddr
	sw	a0, errno
        sw      a0, 0(t1)
	/* return QNAN */
	l.d	$f0, _local_qnan
1:	j	ra
	END(fabs)

