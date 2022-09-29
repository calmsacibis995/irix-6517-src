#include	"ml.h"
#include	"sys/asm.h"
#include	"sys/regdef.h"

#ident "saio/lib/swbcopy.s: $Revision: 1.2 $"

/*
 * swap bytes while copying them.  Used by scsi tape driver.
 *
 * This code ASSUMES the following:
 *	- count is even
 *	- from & to do **not** overlap
 *
 * void swbcopy(from, to, count);
 *	unsigned char *from, *to;
 *	unsigned long count;
 */
/* registers used */
#define	from	a0
#define	to	a1
#define	count	a2

LEAF(swbcopy)
	beq	count,zero,2f		# Test for zero count
	beq	from,to,2f		# Test for from == to
	/*
	 * Copy bytes, two at a time.
	 */
1:	lb	v0,0(from)
	lb	v1,1(from)
	PTR_ADDU	from,2
	sb	v1,0(to)
	sb	v0,1(to)
	PTR_ADDU	to,2
	PTR_SUBU	count,2
	bgt	count,zero,1b

2:	j	ra
	END(swbcopy)
