/* copy to or from VME devices that understand only 16-bit words.
 *
 *	hwcpout is going from 32 bit bus to 16 bit bus, hwcpin is opposite.
 *	a0 is the source. a1 is the destination. a2 is the byte-count.
 *	No return value.
 *
 * The caller must ensure that the length is not zero.
 *
 *	Basic Algorithm:
 *		Try to align the 32 bit side on a word bit boundary.
 *		If you can't, just byte copy. Then do as many
 *		16 byte copy loops as you can, then do as many 2 byte copy
 *		iterations as you can, then pick up the dregs with byte
 *		copies.
 */

#ident	"$Revision: 3.6 $"

#include "sys/asm.h"
#include "sys/regdef.h"


/*
 *	The CPU is fast, the VME bus and card are very slow.  Anything
 *	that reduces the number of VME cycles is good.
 *
 *	Things are almost always aligned.  Things are almost always large
 *	The VME bus is very slow, so do not worry much about speed.
 *
 *	The assembler does a good job filling delay slots in the following
 *	code, so let it.
 */

#define CAT(x)t

#define OUTW(n) lw CAT(t)n,4*n(a0); \
	srl	t8,CAT(t)n,16;  \
	sh t8,4*n(a1);		\
	sh CAT(t)n,4*n+2(a1)

#define INW(n) lhu CAT(t)n,4*n(a0); \
	lhu	t8,4*n+2(a0);	\
	sll	CAT(t)n,16;	\
	or	CAT(t)n,t8;	\
	sw	CAT(t)n,4*n(a1)


LEAF(hwcpout)
	xor	v0,a0,a1
	ble	a2,3,bytc	# if too short, just byte copy.
	and	v0,1
	bne	v0,zero,bytc	# source and destination not alignable

	and	v1,a0,1		# do an odd initial byte
	beq	v1,zero,ockw

	lb	t0,0(a0)
	sb	t0,0(a1)
	PTR_ADDU a0,1
	PTR_ADDU a1,1
	subu	a2,1

ockw:	and	v0,a0,2		# make source word aligned
	beq	v0,zero,oblk
	lhu	t0,0(a0)
	sh	t0,0(a1)
	PTR_ADDU a0,2
	PTR_ADDU a1,2
	subu	a2,2


/* try to copy blocks of 16 bytes
 */
oblk:	and	a3,a2,~(16-1)
	beq	a3,zero,8f
16:	OUTW(0)
	OUTW(1)
	OUTW(2)
	OUTW(3)
	PTR_ADDU a0,16
	PTR_ADDU a1,16
	subu	a3,16
	bne	a3,zero,16b

/* here we know we have < 16 bytes to finish */
8:	and	v1,a2,8
	beq	v1,zero,4f
	OUTW(0)
	OUTW(1)
	PTR_ADDU a0,8
	PTR_ADDU a1,8

4:	and	v0,a2,4
	beq	v0,zero,2f
	OUTW(0)
	PTR_ADDU a0,4
	PTR_ADDU a1,4

2:	and	v1,a2,2
	beq	v1,zero,1f
	lhu	t0,0(a0);  sh t0,0(a1)
	PTR_ADDU a0,2
	PTR_ADDU a1,2

1:	and	v0,a2,1
	beq	v0,zero,odone
	lbu	t0,0(a0)
	sb	t0,0(a1)

odone:	j	ra



/* copy bytes
 *	Because the VME bus is so slow, do not worry about speed.
 */
bytc:
	blt	a2,4,bytfin
4:	lbu	t0,0(a0);  sb t0,0(a1)
	lbu	t1,1(a0);  sb t1,1(a1)
	lbu	t2,2(a0);  sb t2,2(a1)
	lbu	t3,3(a0);  sb t3,3(a1)
	PTR_ADDU a0,4
	PTR_ADDU a1,4
	subu	a2,4
	bge	a2,4,4b

bytfin:	beq	a2,zero,bdone
1:	lbu	t0,0(a0);  sb t0,0(a1)
	PTR_ADDU a0,1
	PTR_ADDU a1,1
	subu	a2,1
	bne	a2,zero,1b

bdone:	j	ra


	END(hwcpout)



LEAF(hwcpin)
	xor	v0,a0,a1
	ble	a2,3,bytc	# if too short, just byte copy.
	and	v0,1
	bne	v0,zero,bytc	# source and destination not alignable

	and	v1,a0,1		# do an odd initial byte
	beq	v1,zero,ickw

	lb	t0,0(a0)
	sb	t0,0(a1)
	PTR_ADDU a0,1
	PTR_ADDU a1,1
	subu	a2,1

ickw:	and	v0,a1,2		# make destination word aligned
	beq	v0,zero,iblk
	lhu	t0,0(a0);  sh t0,0(a1)
	PTR_ADDU a0,2
	PTR_ADDU a1,2
	subu	a2,2

/* try to copy blocks of 16 bytes
 */
iblk:	and	a3,a2,~(16-1)
	beq	a3,zero,8f
16:	INW(0)
	INW(1)
	INW(2)
	INW(3)
	PTR_ADDU a0,16
	PTR_ADDU a1,16
	subu	a3,16
	bne	a3,zero,16b

/* here we know we have < 16 bytes to finish */
8:	and	v1,a2,8
	beq	v1,zero,4f
	INW(0)
	INW(1)
	PTR_ADDU a0,8
	PTR_ADDU a1, 8

4:	and	v0,a2,4
	beq	v0,zero,2f
	INW(0)
	PTR_ADDU a0,4
	PTR_ADDU a1,4

2:	and	v1,a2,2
	beq	v1,zero,1f
	lhu	t0,0(a0)
	sh	t0,0(a1)
	PTR_ADDU a0,2
	PTR_ADDU a1,2

1:	and	v0,a2,1
	beq	v0,zero,idone
	lbu	t0,0(a0)
	sb	t0,0(a1)

idone:	j	ra
	
	END(hwcpin)
