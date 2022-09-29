/* pon_cache.s - GCache power on diagnostics.
 */
#ident "$Revision: 1.38 $"

#include <sys/cpu.h>
#include <sys/sbd.h>
#include <early_regdef.h>
#include <ml.h>
#include <asm.h>

#define PTAG_ST_READ_MASK 0x1f900000
#define	PTAG_ST_WRITE	GCACHE_STATE_EN|GCACHE_VSYN_EN|GCACHE_DIRTY|0x1e000000
#define PTAG_ST_READ	PTAG_ST_WRITE & PTAG_ST_READ_MASK

#define NICEADDR_SHIFT	(GCACHE_TAG_SHIFT-TAGADDR_SET_SHIFT)

#define	EVEN2ODDTAGDA		TCC_OTAG_DA-TCC_ETAG_DA
#define EVEN2ODDTAGST		TCC_OTAG_ST-TCC_ETAG_ST
#define	NEXT_SET		0x1 << TAGADDR_SET_SHIFT

#define SCACHE_TESTADDR		PHYS_TO_K0(0x19000000)	/* use reserved GIO */

/* IO macros
 */
#define puts(STR)			\
	dla	a0,STR;			\
	jal	pon_puts;		\
	nada
#define newline()			\
	puts(crlf)
#define puthex64(reg)			\
	move	a0,reg;			\
	jal	pon_puthex64;		\
	nada
#define puthex(reg)			\
	move	a0,reg;			\
	jal	pon_puthex;		\
	nada
#define	putdec1(reg)			\
	jal	pon_putc;		\
	addi	v1,reg,0x30;

#define SCT_DEBUG	0
#define SCT2_DEBUG	0
#define SCT3_DEBUG	0
#define SCD_DEBUG	0
#define SCD_DEBUG3	0
#define SCD2_DEBUG	0

#if SCT_DEBUG||SCT2_DEBUG||SCT3_DEBUG||SCD_DEBUG||SCD_DEBUG3||SCD2_DEBUG
#define DEBUG 1
#define dputs(STR)			\
	puts(STR);			\
	newline();
#define	showdata(II,XX,YY,SS,DD)	\
	li	a0,DD;			\
	beqz	a0,171f;		\
	nada;				\
	puts(index_msg);		\
	puthex(II);			\
	puts(set_msg);			\
	putdec1(SS);			\
	puts(expect_msg);		\
	puthex64(XX);			\
	puts(actual_msg);		\
	puthex64(YY);			\
	newline();			\
171:
#else
#define dputs(STR)
#define showdata(II,XX,YY,SS,DD)
#endif

/*  Update GCache error register -- Increment the bits for the current
 * error counter, and flag the set as bad.
 */
#define incr_gerror(SHIFT)			\
	dmfc0	a0,TETON_GERROR;		\
	daddu	a0,(1<<SHIFT);			\
	dli	a1,GERROR_SET0;			\
	dsll	a1,k0;				\
	or	a0,a1;				\
	dmtc0	a0,TETON_GERROR;		\
	ssnop
#if PROM
#define check_gerror(SHIFT)			\
	dsll	a0,(64-SHIFT-16);		\
	dsrl	a0,(64-16);			\
	sub	a0,GERROR_MAXPONERR-1;		\
	blt	a0,zero,99f;			\
	nada;					\
	dmfc0	v0,TETON_GERROR;		\
	ssnop;					\
	j	RA2;				\
99:	nada
#else
#define check_gerror(SHIFT)
#endif

/* Size_2nd_cache returns the cache size in MB, so to convert this
 * to indicies:
 *	(cache size / sets) / linesize
 * or
 *	(2MB/4)/128 == (512KB/128) == 4096 == 2MB >> 9
 */
#define	gsize2index(dst,src)	srl	dst,src,9

#if !PROM
FRAMESIZE=(SZREG*12)+(SZREG*12)
#define	 FUNCTION_SETUP				\
	PTR_SUBU sp, FRAMESIZE;			\
	dmtc0	zero,TETON_GERROR;		\
	sd	T20, FRAMESIZE-1*SZREG(sp);	\
	sd	T21, FRAMESIZE-2*SZREG(sp);	\
	sd	T22, FRAMESIZE-3*SZREG(sp);	\
	sd	T23, FRAMESIZE-4*SZREG(sp);	\
	sd	T30, FRAMESIZE-5*SZREG(sp);	\
	sd	T31, FRAMESIZE-6*SZREG(sp);	\
	sd	T32, FRAMESIZE-7*SZREG(sp);	\
	sd	T33, FRAMESIZE-8*SZREG(sp);	\
	sd	T34, FRAMESIZE-9*SZREG(sp);	\
	sd	T40, FRAMESIZE-10*SZREG(sp);	\
	sd	T41, FRAMESIZE-11*SZREG(sp);	\
	sd	ra,  FRAMESIZE-12*SZREG(sp)
#define	FUNCTION_TAKEDOWN			\
	dmfc0	v0,TETON_GERROR;		\
	ld	T20, FRAMESIZE-1*SZREG(sp);	\
	ld	T21, FRAMESIZE-2*SZREG(sp);	\
	ld	T22, FRAMESIZE-3*SZREG(sp);	\
	ld	T23, FRAMESIZE-4*SZREG(sp);	\
	ld	T30, FRAMESIZE-5*SZREG(sp);	\
	ld	T31, FRAMESIZE-6*SZREG(sp);	\
	ld	T32, FRAMESIZE-7*SZREG(sp);	\
	ld	T33, FRAMESIZE-8*SZREG(sp);	\
	ld	T34, FRAMESIZE-9*SZREG(sp);	\
	ld	T40, FRAMESIZE-10*SZREG(sp);	\
	ld	T41, FRAMESIZE-11*SZREG(sp);	\
	ld	ra,  FRAMESIZE-12*SZREG(sp);	\
	PTR_ADDU sp, FRAMESIZE
#else
#define FUNCTION_SETUP		\
	move	RA2,ra;
#define FUNCTION_TAKEDOWN	\
	move	ra,RA2;
#endif

/*  Write each tag (both state and address) and read it back.  We cycle
 * through address based data (to test addressing), even bits on (all 'a'),
 * and odd bits on (all '5').
 *
 *	- Test should be run in Chandra mode to minimize conflicts in
 *	  set 0 when fetching instructions.
 *
 *	- Test sets 1+2+3 first since they arn't affected by uncached
 *	  instruction fetches.  Set 0 writes then reads back immediately
 *	  since set 0 may get affected by other uncached reads.
 *
 *  Register map:
 *	T20	- number of indicies in cache set
 *	T21	- index counter
 *	T22	- pattern control
 *	T23	- expected data
 *	T30	- working tag address
 *	T31	- working state address
 *	T32	- cache prime flag
 *	T33	- GCACHE_TAG
 *	T34	- actual data
 *	T40	- PTAG_ST_READ_MASK
 *	T41	- free
 *	k0	- set number
 *	k1	- error address
 *	a0-a3	- scratch
 */
LEAF(scache_tag)
	.set	noreorder
	FUNCTION_SETUP

	jal	size_2nd_cache			# get cache size
	nada
	gsize2index(T20,v0)			# number of GCache indicies

	dli	T33,GCACHE_TAG
	move	T22,zero			# start with addresses
	move	T32,zero			# run counter.

sct_top:
	move	T21,zero			# index count
	li	k0,1				# init set counter

	/*  We only write the even state and tag since both even and odd will
	 * actually be written.
	 */
sct_next_pass:
	dli	T30,PHYS_TO_K1(TCC_ETAG_DA)	# tag base
	dli	T31,PHYS_TO_K1(TCC_ETAG_ST)	# state base
	sll	a0,k0,TAGADDR_SET_SHIFT		# shift set into position
	or	T30,a0				# add set to address
	or	T31,a0				# add set to address
	li	a3,PTAG_ST_WRITE		# tag state init value

sct_init_loop:					# Begin of set loop 
	/*  Calculate test data.  if T22==0 derive the data from the address,
	 * else use T22 directly.
	 */
	bnez	T22,1f				# generate or use data?
	move	T23,T30				# BDSLOT: ok for target
	not	T23
	b	2f
	sll	T23,NICEADDR_SHIFT		# BDSLOT: get good bits
1:	move	T23,T22				# use T22 directly
2:	and	T23,T33				# isolate tag

	sd	T23,0(T30)			# write scache addr tag
	sd	a3,0(T31)			# write scache tag state

	addu	T21,1				# incr index
	daddiu	T30,(1<<TAGADDR_INDEX_SHIFT)	# incr tag ptr
	daddiu	T31,(1<<TAGADDR_INDEX_SHIFT)	# incr tag ptr
	bltu	T21,T20,sct_init_loop		# loop through scache indexes
	nada

	/* tags are now initialized, now readback and compare values.
	 */
	li	T40,PTAG_ST_READ_MASK
	move	T21,zero			# index count
	dli	T30,PHYS_TO_K1(TCC_ETAG_DA)	# tag base
	dli	T31,PHYS_TO_K1(TCC_ETAG_ST)	# state base
	sll	a0,k0,TAGADDR_SET_SHIFT		# shift set into position
	or	T30,a0				# add set to address
	or	T31,a0				# add set to address

sct_readback_loop:				# Beginning of index loop
	/* Regenerate the data stored at these tags.
	 */
	bnez	T22,1f				# generate or use data?
	move	T23,T30				# BDSLOT:
	not	T23
	b	2f
	sll	T23,NICEADDR_SHIFT		# BDSLOT: get good bits
1:	move	T23,T22				# use data
2:	and 	T23,T33				# isolate tag

	/* Check even tag data.
	 */
	ld	T34,0(T30)			# check tag
	beqz	T32,1f				# first run primes the cache
	and	T34,T33				# BDSLOT: isolate tag
	beq	T34,T23,2f			# Even tag read back ok
	move	k1,T30				# BDLOT: error addr in k1
	jal	sct_error			# goto error handler
	nada
2:	showdata(T21,T23,T34,k0,SCT_DEBUG)

	/* Check odd tag data.
	 */
1:	daddu	a0,T30,EVEN2ODDTAGDA		# convert addr to odd map
	ld	T34,0(a0)			# read odd tag address
	beqz	T32,1f				# first run primes the cache
	and	T34,T33				# BDSLOT: isolate tag
	beq	T34,T23,1f			# Odd tag read back ok
	move	k1,a0				# BDLOT: error addr in k1
	jal	sct_error			# goto error handler
	nada
2:	showdata(T21,T23,T34,k0,SCT_DEBUG)

	/* Check even tag state.
	 */
1:	ld	T34,0(T31)			# Check the even tag state
	and	T34,T40				# isolate important data
	li	T23,PTAG_ST_READ
	beqz	T32,1f				# first run primes the cache
	nada
	beq	T34,T23,2f			# Even proc state read ok
	move	k1,T31				# BDSLOT: error addr in k1
	jal	sct_error			# goto error handler
	nada
2:	showdata(T21,T23,T34,k0,SCT_DEBUG)

	/* Check odd tag state [T23 still == PTAG_ST_READ].
	 */
1:	daddu	a0,T31,EVEN2ODDTAGST		# convert addr to odd map
	ld	T34,0(a0)			# read tag state
	and	T34,T40				# isolate important data
	beqz	T32,1f				# first run primes the cache
	nada
	beq	T34,T23,2f			# Odd state read ok
	move	k1,a0				# BDSLOT: error addr in k1
	jal	sct_error			# goto error handler
	nada
2:	showdata(T21,T23,T34,k0,SCT_DEBUG)

	beqz	T32,sct_top			# start over after prime job
	ori	T32,zero,1			# BDSLOT: done with first run

	/* Loop through all index entries.
	 */
1:	daddu	T21,1				# incr set
	daddiu	T30,(1<<TAGADDR_INDEX_SHIFT)	# incr tag ptr
	daddiu	T31,(1<<TAGADDR_INDEX_SHIFT)	# incr tag ptr
	bltu	T21,T20,sct_readback_loop
	nada

	addiu	k0,1				# incr set counter
	li	a0,3				# only do sets 1,2,3 here
	ble	k0,a0,sct_next_pass		# next full set pass
	move	T21,zero			# BDSLOT: reset index counter

	/* Loop through all test patterns
	 */
	dli	a0,0x5555555555555555
	bnez	T22,1f
	nada
	b	sct_top				# goto second pass
	move	T22,a0				# BDSLOT: use 5's
1:	bne	T22,a0,1f			# branch if done
	nada
	dli	T22,0xaaaaaaaaaaaaaaaa
	b	sct_top				# goto last pass
	nada
1:

	/* Test has completed.
	 */
	b	sct_done			# jump to takedown code
	nop

	/* Print an error message:
	 *	Tag RAM test FAILED @ 0x900000018xxxxxx TYPE
	 *		expected: 0x123456789abcdef actual: 0x1234567891bcdef
	 *		index: 0x0000xxxx set: x
	 *
	 * TYPE can be:	[even state], [even tag], [odd state], [odd tag].
	 */
sct_error:
	move	RA3,ra				# save callers RA
	incr_gerror(GERROR_TAG_SHIFT)
	check_gerror(GERROR_TAG_SHIFT)
	puts(scache_tag_fail)
index_cmn_err:					# common report for index tests
	puthex64(k1)				# which tag addr failed
	lui	a0,0x0010			# even base has bit 20 set
	and	a0,k1,a0
	beqz	a0,1f
	nada
	puts(even_msg)
	b	2f
	nada
1:	puts(odd_msg)
2:	lui	a0,0x0070			# figure if state or tag
	and	a0,k1,a0
	lui	a1,0x0030
	blt	a0,a1,1f
	nada
	puts(state_msg)
	b	2f
	nada
1:	puts(tag_msg)
2:	newline();

	puts(expect_msg)
	puthex64(T23)
	puts(actual_msg)
	puthex64(T34)
	newline();

	puts(index_msg)
	puthex(T21)
	puts(set_msg)
	putdec1(k0)
	newline();
	newline();

	jr	RA3				# continue with test
	nada

sct_done:
	FUNCTION_TAKEDOWN
	j	ra
	nada
	.set	reorder
	END(scache_tag)

/*  Run tests similar to scache_tag on set 0.  We repeat this test twice
 * so we can test all set 0 w/o interference from instruction fetches.
 *
 *  Register map:
 *	T20	- number of indicies in cache set
 *	T21	- index counter
 *	T22	- pattern control
 *	T23	- expected data
 *	T30	- working tag address
 *	T31	- working state address
 *	T32	- PTAG_ST_WRITE
 *	T33	- GCACHE_TAG
 *	T34	- actual data
 *	T40	- start index
 *	T41	- end index
 *	k0	- set number
 *	k1	- error address
 *	a0-a3	- scratch
 *	
 */
LEAF(scache_tag2)
	.set	noreorder
	FUNCTION_SETUP

	jal	size_2nd_cache			# get cache size
	nada
	gsize2index(T20,v0)			# number of GCache indicies

	dli	T33,GCACHE_TAG
	move	T22,zero			# start with addresses
	li	T32,PTAG_ST_WRITE		# tag state init value
	li	k0,0				# do set 0

	dla	T40,sct2_loop			# top of loop
	dla	T41,sct2_bottom			# bottom of loop
	li	a0,GCACHE_INDEX
	and	T40,a0
	and	T41,a0
	srl	T40,GCACHE_INDEX_SHIFT
	srl	T41,GCACHE_INDEX_SHIFT
	bgt	T41,T40,sct2_top		# If range wraps then start
	nada					# at ending index plus one.
	move	T21,T41				# The scache_tag test will
	addi	T21,1				# cover the skipped indicies.

sct2_top:
	dli	T30,PHYS_TO_K1(tcc_tagaddr(TCC_ETAG_DA,0,3))
	dli	T31,PHYS_TO_K1(tcc_tagaddr(TCC_ETAG_ST,0,3)) 
	move	T21,zero			# init index counter

sct2_loop:
	blt	T21,T40,1f			# ok if less than 1st index
	nada
	bgt	T21,T41,1f			# ok if greather than last index
	nada
	b	sct2_skip2			# skip this index
	nada

1:	bnez	T22,1f				# generate or constant?
	move	T23,T30				# BDSLOT: construct test tag
	not	T23
	b	2f
	sll	T23,NICEADDR_SHIFT		# BDSLOT
1:	move	T23,T22				# use constant
2:	and	T23,T33				# isolate tag

	/* Check even tag data
	 */
	sd	T23,0(T30)			# write tag
	ld	T34,0(T30)			# read tag
	and	T34,T33				# isolate tag
	beq	T34,T23,2f			# Even tag data ok
	move	k1,T30				# BDSLOT: error addr in k1
	jal	sct_error			# goto error handler
	nada
2:	showdata(T21,T23,T34,k0,SCT2_DEBUG)

	/* Check odd tag data.
	 */
	daddu	a0,T30,EVEN2ODDTAGDA		# convert addr to odd map
	sd	T23,0(a0)			# write tag
	ld	T34,0(a0) 			# read tag
	and	T34,T33				# isolate tag
	beq	T34,T23,2f			# Odd tag data ok
	move	k1,a0				# BDSLOT: error addr in k1
	jal	sct_error			# goto error handler
	nada
2:	showdata(T21,T23,T34,k0,SCT2_DEBUG)

	/* Check even tag state
	 */
	li	T23,PTAG_ST_READ
	sd	T32,0(T31)			# store tag
	ld	T34,0(T31)			# load tag
	and	T34,PTAG_ST_READ_MASK		# pick imporant bits
	beq	T34,T23,2f			# Even tag state ok
	move	k1,T31				# BDSLOT: error addr in k1
	jal	sct_error			# goto error handler
	nada
2:	showdata(T21,T23,T34,k0,SCT2_DEBUG)

	/* Check odd tag state
	 */
	daddu	a0,T31,EVEN2ODDTAGST		# convert addr to odd map
	sd	T32,0(a0)			# store tag
	ld	T34,0(a0)			# load tag
	and	T34,PTAG_ST_READ_MASK
	beq	T34,T23,2f			# Odd tag state ok
	move	k1,a0				# BDSLOT: error addr in k1
	jal	sct_error			# goto error handler
	nada
2:	showdata(T21,T23,T34,k0,SCT2_DEBUG)

	/* Loop through all index entries
	 */
sct2_skip2:
	addu	T21,1				# incr index
	daddiu	T30,(1<<TAGADDR_INDEX_SHIFT)	# incr tag ptr
	daddiu	T31,(1<<TAGADDR_INDEX_SHIFT)	# incr tag ptr
	bltu	T21,T20,sct2_loop		# loop until done
	nada

sct2_bottom:

	/* Loop through all test patterns
	 */
	dli	a0,0x5555555555555555
	bnez	T22,1f
	nada
	b	sct2_top			# goto second pass
	move	T22,a0				# BDSLOT: use 5's
1:	bne	T22,a0,1f			# branch if done
	nada
	dli	T22,0xaaaaaaaaaaaaaaaa
	b	sct2_top			# goto last pass
	nada
1:


	/* Test completed.
	 */
	b	sct_done			# jump to takedown code
	nop
	.set	reorder
	END(scache_tag2)

/*  Run tests similar to scache_tag on set 0.  We repeat this test twice
 * so we can test all set 0 w/o interference from instruction fetches.
 *
 *  Register map:
 *	T20	- number of indicies in cache set
 *	T21	- index counter
 *	T22	- pattern control
 *	T23	- expected data
 *	T30	- working tag address
 *	T31	- working state address
 *	T32	- PTAG_ST_WRITE
 *	T33	- GCACHE_TAG
 *	T34	- actual data
 *	T40	- start index
 *	T41	- end index
 *	k0	- set number
 *	k1	- error address
 *	a0-a3	- scratch
 *	
 */
.align 7
LEAF(scache_tag3)
	.set	noreorder
	FUNCTION_SETUP

	jal	size_2nd_cache			# get cache size
	nada
	gsize2index(T20,v0)			# number of GCache indicies

	dli	T33,GCACHE_TAG
	move	T22,zero			# start with addresses
	li	T32,PTAG_ST_WRITE		# tag state init value
	li	k0,0				# do set 0

	dla	T40,sct3_loop			# top of loop
	dla	T41,sct3_bottom			# bottom of loop
	li	a0,GCACHE_INDEX
	and	T40,a0
	and	T41,a0
	srl	T40,GCACHE_INDEX_SHIFT
	srl	T41,GCACHE_INDEX_SHIFT
	bgt	T41,T40,sct3_top		# If range wraps then start
	nada					# at ending index plus one.
	move	T21,T41				# The scache_tag test will
	addi	T21,1				# cover the skipped indicies.

sct3_top:
	dli	T30,PHYS_TO_K1(tcc_tagaddr(TCC_ETAG_DA,0,3))
	dli	T31,PHYS_TO_K1(tcc_tagaddr(TCC_ETAG_ST,0,3)) 
	move	T21,zero			# init index counter

sct3_loop:
	blt	T21,T40,1f			# ok if less than 1st index
	nada
	bgt	T21,T41,1f			# ok if greather than last index
	nada
	b	sct3_skip2			# skip this index
	nada

1:	bnez	T22,1f				# generate or constant?
	move	T23,T30				# BDSLOT: construct test tag
	not	T23
	b	2f
	sll	T23,NICEADDR_SHIFT		# BDSLOT
1:	move	T23,T22				# use constant
2:	and	T23,T33				# isolate tag

	/* Check even tag data
	 */
	sd	T23,0(T30)			# write tag
	ld	T34,0(T30)			# read tag
	and	T34,T33				# isolate tag
	beq	T34,T23,2f			# Even tag data ok
	move	k1,T30				# BDSLOT: error addr in k1
	jal	sct_error			# goto error handler
	nada
2:	showdata(T21,T23,T34,k0,SCT3_DEBUG)

	/* Check odd tag data.
	 */
	daddu	a0,T30,EVEN2ODDTAGDA		# convert addr to odd map
	sd	T23,0(a0)			# write tag
	ld	T34,0(a0) 			# read tag
	and	T34,T33				# isolate tag
	beq	T34,T23,2f			# Odd tag data ok
	move	k1,a0				# BDSLOT: error addr in k1
	jal	sct_error			# goto error handler
	nada
2:	showdata(T21,T23,T34,k0,SCT3_DEBUG)

	/* Check even tag state
	 */
	li	T23,PTAG_ST_READ
	sd	T32,0(T31)			# store tag
	ld	T34,0(T31)			# load tag
	and	T34,PTAG_ST_READ_MASK		# pick imporant bits
	beq	T34,T23,2f			# Even tag state ok
	move	k1,T31				# BDSLOT: error addr in k1
	jal	sct_error			# goto error handler
	nada
2:	showdata(T21,T23,T34,k0,SCT3_DEBUG)

	/* Check odd tag state
	 */
	daddu	a0,T31,EVEN2ODDTAGST		# convert addr to odd map
	sd	T32,0(a0)			# store tag
	ld	T34,0(a0)			# load tag
	and	T34,PTAG_ST_READ_MASK
	beq	T34,T23,2f			# Odd tag state ok
	move	k1,a0				# BDSLOT: error addr in k1
	jal	sct_error			# goto error handler
	nada
2:	showdata(T21,T23,T34,k0,SCT3_DEBUG)

	/* Loop through all index entries
	 */
sct3_skip2:
	addu	T21,1				# incr index
	daddiu	T30,(1<<TAGADDR_INDEX_SHIFT)	# incr tag ptr
	daddiu	T31,(1<<TAGADDR_INDEX_SHIFT)	# incr tag ptr
	bltu	T21,T20,sct3_loop		# loop until done
	nada

sct3_bottom:

	/* Loop through all test patterns
	 */
	dli	a0,0x5555555555555555
	bnez	T22,1f
	nada
	b	sct3_top			# goto second pass
	move	T22,a0				# BDSLOT: use 5's
1:	bne	T22,a0,1f			# branch if done
	nada
	dli	T22,0xaaaaaaaaaaaaaaaa
	b	sct3_top			# goto last pass
	nada
1:

	/* Test completed.
	 */
	b	sct_done			# jump to takedown code
	nop
	.set	reorder
	END(scache_tag3)

/*  Secondary cache address and data test.  We take 3 passes  writing the
 * negation of an address '5's and 'a's to the cache.
 *
 *  We need to ignore CPU detected GCache parity errors when run as a
 * power-on test (we should be called this way) since the cache data
 * has not been initialized and we will be playing with it.  We do not
 * need to worry about TCC detected errors since we run with writebacks
 * inhibited.
 *
 *  For set 0 we run this same test compiled at a different index
 * make sure we get full coverage due to PIOs and ICache misses changing
 * the GCache state.  The (prototype-only BP tags used set 3 for this).
 * the BQ tags use set 0.
 *
 *  Register map:
 *	T20	- number of cache indicies
 *	T21	- current index
 *	T22	- data to write
 *	T23	- expected data
 *	T30	- address to write to
 *	T31	- address of tag data
 *	T32	- address of tag state
 *	T33	- GCACHE_TAG mask
 *	T34	- actual data
 *	T40	- scratch in error handler
 *	T41	- scratch in error handler
 *	k0	- current set
 *	k1	- error address
 *	a0-a3	- scratch/args
 */
LEAF(scache_data)
	.set	noreorder
	FUNCTION_SETUP

	jal	size_2nd_cache
	nada
	gsize2index(T20,v0)

	dli	T33,GCACHE_TAG			# cache tag mask
	move	T22,zero			# data to write

scd_top:
	li	k0,1				# start with set 1 (not 0)

scd_set_top:
	move	T21,zero			# init index counter

	/*  Set-up TCC_GCACHE for this pass.  Writeback disabled, default
	 * timing, set 0 (for instructions/PIOs) and current set enabled.
	 */
	dli	a0,PHYS_TO_K1(TCC_GCACHE)	# GCache control
	li	a1,GCACHE_WBACK_INH|GCACHE_RR_FULL|GCACHE_WB_RESTART| \
		   GCACHE_SET0
	li	a2,1				# 1<<set is enable bit
	sll	a2,a2,k0
	or	a1,a2				# or in set enable
	sd	a1,0(a0)			# config cache

	/* Get even tag base for this set
	 */
	dli	T31,PHYS_TO_K1(TCC_ETAG_ST)	# tag/state bases
	dli	T32,PHYS_TO_K1(TCC_ETAG_DA)
	sll	a0,k0,TAGADDR_SET_SHIFT		# shift set into position
	or	T31,a0				# set shift bits
	or	T32,a0				# set shift bits

	/*  Figure out base address for this set (base, base+16MB, base+32,
	 * base+48MB).
	 */
	dli	T30,SCACHE_TESTADDR		# base address we access
	dli	a0,0x1000000			# 16MB
	multu	a0,k0				# set * 16MB
	mflo	a0				# 0MB, 16MB, 32MB, 48MB
	daddu	T30,a0				# final address

scd_write_loop:
	/* Initialize the tag to a valid clean line in reserved GIO space.
	 */
	li	a1,GCACHE_INITSTATE
	sd	a1,0(T31)			# invalidate what is there
	and	a0,T30,T33			# isolate tag (T33==GCACHE_TAG)
	sd	a0,0(T32)			# write tag
	li	a1,GCACHE_INITSTATE|(GCACHE_VALID<<GCACHE_STATE_SHIFT)
	sd	a1,0(T31)			# validate new tag

	/*  Line is 16 doubles (128 bytes).  Fill line with data.  If T22
	 * is 0, write negation of address to the line, otherwise use T22
	 */
	bnez	T22,1f				# use T22 if non zero
	not	a0,T30				# else address based pattern
	b	2f
	nada					# BDSLOT
1:	move	a0,T22
2:	sd	a0,0(T30)
	sd	a0,8(T30)
	sd	a0,16(T30)
	sd	a0,24(T30)
	sd	a0,32(T30)
	sd	a0,40(T30)
	sd	a0,48(T30)
	sd	a0,56(T30)
	sd	a0,64(T30)
	sd	a0,72(T30)
	sd	a0,80(T30)
	sd	a0,88(T30)
	sd	a0,96(T30)
	sd	a0,104(T30)
	sd	a0,112(T30)
	sd	a0,120(T30)

	addu	T21,1				# incr index
	daddiu	T31,(1<<TAGADDR_INDEX_SHIFT)	# incr index in tag ptr
	daddiu	T32,(1<<TAGADDR_INDEX_SHIFT)	# incr index in set ptr
	bltu	T21,T20,scd_write_loop		# loop through scache indicies
	daddiu	T30,TCC_LINESIZE		# BDSLOT: increment address

	/*  Now readback this set of the cache.  We do some sanity checking
	 * on the tags, and check the data in the cache line.
	 */
	dli	T31,PHYS_TO_K1(TCC_ETAG_ST)	# state base
	dli	T32,PHYS_TO_K1(TCC_ETAG_DA)	# data base
	sll	a0,k0,TAGADDR_SET_SHIFT		# shift set into position
	or	T31,a0				# set shift bits
	or	T32,a0				# set shift bits
	move	T21,zero			# reinit index counter
	dli	T30,SCACHE_TESTADDR		# base address we access
	dli	a0,0x1000000			# 16MB
	multu	a0,k0				# set * 16MB
	mflo	a0				# 0MB, 16MB, 32MB, 48MB
	daddu	T30,a0				# final address

scd_read_loop:
	/*  Read the tag and state and do some sanity checking -- the
	 * tag should match what we expect and the line should be dirty.
	 */
	ld	T34,0(T32)			# check tag data
	and	T34,T33				# isolate tag
	and	T23,T30,T33			# isolate tag
	beq	T34,T23,2f			# unexpected tag?
	move	k1,T32				# BDSLOT: error addr in k1
	jal	scd_tag_error			# goto error handler
	nada
2:	showdata(T21,T23,T34,k0,SCD_DEBUG)

	ld	T34,0(T31)			# check tag state
	li	T23,GCACHE_DIRTY|(GCACHE_VALID<<GCACHE_STATE_SHIFT)
	and	T34,T34,T23			# only care about this bits
	beq	T34,T23,2f			# unexpected state?
	move	k1,T31				# BDSLOT: error addr in k1
	jal	scd_tag_error			# goto error handler
	nada
2:	showdata(T21,T23,T34,k0,SCD_DEBUG)

	bnez	T22,1f				# use T22 if non zero
	not	T23,T30				# else address based pattern
	b	2f
	nada					# BDSLOT
1:	move	T23,T22
2:

#define checkdata(off,dbg)			\
	ld	T34,off(T30);			\
	beq	T34,T23,99f;			\
	move	k1,T30;				\
	daddi	k1,off;				\
	jal	scd_error;			\
	nada;					\
99:	showdata(T21,T23,T34,k0,dbg);

	checkdata(0,SCD_DEBUG)			# check each double word
	checkdata(8,SCD_DEBUG)			# in this line
	checkdata(16,SCD_DEBUG)
	checkdata(24,SCD_DEBUG)
	checkdata(32,SCD_DEBUG)
	checkdata(40,SCD_DEBUG)
	checkdata(48,SCD_DEBUG)
	checkdata(56,SCD_DEBUG)
	checkdata(64,SCD_DEBUG)
	checkdata(72,SCD_DEBUG)
	checkdata(80,SCD_DEBUG)
	checkdata(88,SCD_DEBUG)
	checkdata(96,SCD_DEBUG)
	checkdata(104,SCD_DEBUG)
	checkdata(112,SCD_DEBUG)
	checkdata(120,SCD_DEBUG)

	/* Invalidate current line now that we are done with it.
	 */
	li	a0,GCACHE_INITSTATE
	sd	a0,0(T31)

scd_loop:
	addu	T21,1				# incr index
	daddiu	T31,(1<<TAGADDR_INDEX_SHIFT)	# incr index in tag ptr
	daddiu	T32,(1<<TAGADDR_INDEX_SHIFT)	# incr index in set ptr
	bltu	T21,T20,scd_read_loop		# loop through scache indicies
	daddiu	T30,TCC_LINESIZE		# BDSLOT: increment address

	/* Bump to next set
	 */
	addu	k0,1				# incr set
	li	a0,3				# test sets 1+2+3
	ble	k0,a0,scd_set_top
	nada

	/* Test sets 0 seperately since fetches and PIOs affect GCache.
	 */
	li	k0,0				# set 0 first

scd_setX_next:
	dli	T31,PHYS_TO_K1(TCC_OTAG_ST)	# odd state base address
	dli	T32,PHYS_TO_K1(TCC_OTAG_DA)	# odd tag base address
	sll	a0,k0,TAGADDR_SET_SHIFT		# shift set into position
	or	T31,a0				# turn on set info
	or	T32,a0				# turn on set info
	dli	T30,SCACHE_TESTADDR		# base address we access
	dli	a0,0x1000000			# 16MB
	multu	a0,k0				# set * 16MB
	mflo	a0				# 0MB, 16MB, 32MB, 48MB
	daddu	T30,a0				# final address
	move	T21,zero			# reinit index counter

	/* Figure GCache indicies execution will touch so we can skip them.
	 */
	dla	a2,scd_setX_top			# top of loop
	dla	a3,scd_setX_bottom		# bottom of loop
	li	a0,GCACHE_INDEX
	and	a2,a0				# get top index
	and	a3,a0				# get bottom index
	srl	a2,GCACHE_INDEX_SHIFT
	srl	a3,GCACHE_INDEX_SHIFT
	bgt	a3,a2,scd_setX_top		# If range wraps then start
	nada					# at ending index plus one.
	move	T21,a3				# The scache_data2 test will
	addi	T21,1				# cover the skipped indicies.

scd_setX_top:
	/* Skip program indicies.
	 */
	blt	T21,a2,1f			# ok if less than 1st index
	nada
	bgt	T21,a3,1f			# ok if greater than last index
	nada
	b	scd_setX_loop			# skip this index
	
	/* Initialize the tag to a valid clean line in reserved GIO space.
	 */
1:	li	a1,GCACHE_INITSTATE
	sd	a1,0(T31)			# invalidate what is there
	and	a0,T30,T33			# isolate tag (T33==GCACHE_TAG)
	sd	a0,0(T32)			# write tag
	li	a1,GCACHE_INITSTATE|(GCACHE_VALID<<GCACHE_STATE_SHIFT)
	sd	a1,0(T31)			# validate new tag

	/*  Line is 16 doubles (128 bytes).  Fill line with data.  If T22
	 * is 0, write negation of address to the line, otherwise use T22
	 */
	bnez	T22,1f				# use T22 if non zero
	not	a0,T30				# else address based pattern
	b	2f
	nada					# BDSLOT
1:	move	a0,T22
2:	sd	a0,0(T30)
	sd	a0,8(T30)
	sd	a0,16(T30)
	sd	a0,24(T30)
	sd	a0,32(T30)
	sd	a0,40(T30)
	sd	a0,48(T30)
	sd	a0,56(T30)
	sd	a0,64(T30)
	sd	a0,72(T30)
	sd	a0,80(T30)
	sd	a0,88(T30)
	sd	a0,96(T30)
	sd	a0,104(T30)
	sd	a0,112(T30)
	sd	a0,120(T30)

	/*  For setX we need to check the data first.  We must be careful
	 * about PIOs to tag rams since they unfortunately invalidate the
	 * current index.  Since we have done no PIOs, yet, it should
	 * be ok to access the line.  We will be able to check the tag
	 * after the data, but we cannot check the state...
	 *
	 *  We also can't add any other PIOs (like accessing the duart)
	 * here since we may invalidate the current tag.
	 */
	bnez	T22,1f				# use T22 if non zero
	not	T23,T30				# else address based pattern
	b	2f
	nada					# BDSLOT
1:	move	T23,T22
2:
	checkdata(0,0)				# check each double word
	checkdata(8,0)				# in this line
	checkdata(16,0)
	checkdata(24,0)
	checkdata(32,0)
	checkdata(40,0)
	checkdata(48,0)
	checkdata(56,0)
	checkdata(64,0)
	checkdata(72,0)
	checkdata(80,0)
	checkdata(88,0)
	checkdata(96,0)
	checkdata(104,0)
	checkdata(112,0)
	checkdata(120,0)

	/*  Read the tag and make sure the address matches.  The PIO will
	 * not affect the tag, but will zero the state, so we cannot
	 * check the state on set 0.
	 */
	ld	T34,0(T32)			# check tag data
	and	T34,T33				# isolate tag
	beq	a3,T34,scd_setX_loop		# skip if tag matches program.
	and	T23,T30,T33			# BDSLOT: isolate tag
	beq	T34,T23,2f			# unexpected tag?
	move	k1,T32				# BDSLOT: error addr in k1
	jal	scd_tag_error			# goto error handler
	nada
2:	showdata(T21,T23,T34,k0,SCD_DEBUG3)

	/* Invalidate current line now that we are done with it.
	 */
	li	a0,GCACHE_INITSTATE
	sd	a0,0(T31)

scd_setX_loop:
	addu	T21,1				# incr index
	daddiu	T31,(1<<TAGADDR_INDEX_SHIFT)	# incr index in tag ptr
	daddiu	T32,(1<<TAGADDR_INDEX_SHIFT)	# incr index in set ptr
	bltu	T21,T20,scd_setX_top		# loop through scache indicies
	daddiu	T30,TCC_LINESIZE		# BDSLOT: increment address

	
scd_setX_bottom:

	/* Bump to next pattern and redo entire loop.
	 */
	dli	a0,0x5555555555555555
	bnez	T22,1f				
	nada					# done with first pass
	b	scd_top				# run with 5 pattern
	move	T22,a0				# BDSLOT
1:	bne	T22,a0,1f
	nada					# done with second pass
	dli	T22,0xaaaaaaaaaaaaaaaa		# run with a pattern
	b	scd_top
	nada
1:

	/* Test completed.
	 */
	b	scd_done
	nop

	/* Failure detected.  Reports look like:
	 *
	 *	GCache data tag FAILED @ 0x900000018xxxxxx TYPE
	 *		expected: 0x123456789abcdef actual: 0x1234567891bcdef
	 *		index: 0x12345678 set: 0x0000000x
	 *
	 * TYPE can be:	[even state], [even tag], [odd state], [odd tag].
	 *
	 *	GCache data comparison FAILED @ 0x900000019xxxxxx TYPE
	 *		expected: 0x123456789abcdef actual: 0x1234567891bcdef
	 *		index: 0x0000xxxx set: x
	 *		byte: x SRAM: Sxx
	 *		<potentially multiple byte errors>
	 *
	 * TYPE can be: [even] or [odd].
	 */
scd_tag_error:
	move	RA3,ra				# save callers RA
	incr_gerror(GERROR_TAG_SHIFT)
	check_gerror(GERROR_TAG_SHIFT)
	puts(scache_data_tag_fail)
	b	index_cmn_err
	nada
scd_error:
	move	RA3,ra				# save callers RA
	incr_gerror(GERROR_DATA_SHIFT)
	check_gerror(GERROR_DATA_SHIFT)
	puts(scache_data_fail)
	puthex64(k1)
	and	a0,k1,0x8
	beqz	a0,1f
	nada
	puts(odd2_msg)
	b	2f
	nada
1:	puts(even2_msg)
2:	newline();

	puts(expect_msg)
	puthex64(T23)
	puts(actual_msg)
	puthex64(T34)
	newline();

	puts(index_msg)
	puthex(T21)
	puts(set_msg)
	putdec1(k0)
	newline()

	li	T40,0xff			# start with byte 0
	li	T41,0				# byte 0
1:
	and	a0,T23,T40			# mask current byte
	and	a1,T34,T40			# mask current byte
	beq	a0,a1,2f			# no error if bytes match
	nada
	puts(byte_msg)
	putdec1(T41)
	puts(sram_msg)
	andi	a0,k1,0x8			# even or odd side of cache
	bnez	a0,3f				# bit set on odd side
	nada
	dla	a0,even_srams
	b	4f
	nada
3:	dla	a0,odd_srams
4:	sll	a1,T41,2			# calculate offset (byte*4)
	jal	pon_puts			# print SRAM string
	daddu	a0,a1				# BDSLOT: base+offset == str
	newline()
2:	dsll	T40,8				# next byte
	bnez	T40,1b				# loop through all bytes
	addi	T41,1				# incr byte number
	newline()
	newline()

	jr	RA3				# continue with tests

scd_done:
	FUNCTION_TAKEDOWN
	j	ra
	nada
	.set	reorder
	END(scache_data)

/*  Second run of set 0 part of scache_data so we can get the tags we missed
 * in the first run.  We need to start on a different GCache line (128 byte
 * alignment) so we interfere with different sets.
 */
.align 7
LEAF(scache_data2)
	.set	noreorder
	FUNCTION_SETUP

	jal	size_2nd_cache
	nada
	gsize2index(T20,v0)

	dli	T33,GCACHE_TAG			# cache tag mask
	li	k0,0				# do set 0

	/* Test sets 0 seperately since fetches and PIOs affect GCache.
	 */
scd2_next:
	move	T22,zero			# data to write

scd2_top:
	dli	T31,PHYS_TO_K1(TCC_OTAG_ST)	# odd state base address
	dli	T32,PHYS_TO_K1(TCC_OTAG_DA)	# odd tag base address
	sll	a0,k0,TAGADDR_SET_SHIFT		# shift set intto position
	dli	T30,SCACHE_TESTADDR		# base address we access
	dli	a0,0x1000000			# 16MB
	multu	a0,k0				# set * 16MB
	mflo	a0				# 0MB, 16MB, 32MB, 48MB
	daddu	T30,a0				# final address
	move	T21,zero			# reinit index counter

	/*  Figure GCache indicies set0 execution will touch so we
	 * can skip them.
	 */
	dla	a2,scd2_setX_top		# top of loop
	dla	a3,scd2_setX_bottom		# bottom of loop
	li	a0,GCACHE_INDEX
	and	a2,a0				# get top index
	and	a3,a0				# get bottom index
	srl	a2,GCACHE_INDEX_SHIFT
	srl	a2,GCACHE_INDEX_SHIFT
	bgt	a3,a2,scd2_setX_top		# If range wraps then start
	nada					# at ending index plus one.
	move	T21,a3				# The scache_data test will
	addi	T21,1				# cover the skipped indicies.

scd2_setX_top:
	/* Skip program indicies.
	 */
	blt	T21,a2,1f			# ok if less than 1st index
	nada
	bgt	T21,a3,1f			# ok if greater than last index
	nada
	b	scd2_loop			# skip this index
	
	/* Initialize the tag to a valid clean line in reserved GIO space.
	 */
1:	li	a1,GCACHE_INITSTATE
	sd	a1,0(T31)			# invalidate what is there
	and	a0,T30,T33			# isolate tag (T33==GCACHE_TAG)
	sd	a0,0(T32)			# write tag
	li	a1,GCACHE_INITSTATE|(GCACHE_VALID<<GCACHE_STATE_SHIFT)
	sd	a1,0(T31)			# validate new tag

	/*  Line is 16 doubles (128 bytes).  Fill line with data.  If T22
	 * is 0, write negation of address to the line, otherwise use T22
	 */
	bnez	T22,1f				# use T22 if non zero
	not	a0,T30				# else address based pattern
	b	2f
	nada					# BDSLOT
1:	move	a0,T22
2:	sd	a0,0(T30)
	sd	a0,8(T30)
	sd	a0,16(T30)
	sd	a0,24(T30)
	sd	a0,32(T30)
	sd	a0,40(T30)
	sd	a0,48(T30)
	sd	a0,56(T30)
	sd	a0,64(T30)
	sd	a0,72(T30)
	sd	a0,80(T30)
	sd	a0,88(T30)
	sd	a0,96(T30)
	sd	a0,104(T30)
	sd	a0,112(T30)
	sd	a0,120(T30)

	/*  For set0 we need to check the data first.  We must be careful
	 * about PIOs to tag rams since they unfortunately invalidate the
	 * current index.  Since we have done no PIOs, yet, it should
	 * be ok to access the line.  We will be able to check the tag
	 * after the data, but we cannot check the state...
	 *
	 *  We also can't add any other PIOs (like accessing the duart)
	 * here since we may invalidate the current tag.
	 */
	bnez	T22,1f				# use T22 if non zero
	not	T23,T30				# else address based pattern
	b	2f
	nada					# BDSLOT
1:	move	T23,T22
2:
	checkdata(0,0)				# check each double word
	checkdata(8,0)				# in this line
	checkdata(16,0)
	checkdata(24,0)
	checkdata(32,0)
	checkdata(40,0)
	checkdata(48,0)
	checkdata(56,0)
	checkdata(64,0)
	checkdata(72,0)
	checkdata(80,0)
	checkdata(88,0)
	checkdata(96,0)
	checkdata(104,0)
	checkdata(112,0)
	checkdata(120,0)

	/*  Read the tag and make sure the address matches.  The PIO will
	 * not affect the tag, but will zero the state, so we cannot
	 * check the state on set 3.
	 */
	ld	T34,0(T32)			# check tag data
	and	T23,T30,T33			# isolate tag
	and	T34,T33				# isolate tag
	beq	T34,T23,2f			# unexpected tag?
	move	k1,T32				# BDSLOT: error addr in k1
	jal	scd_tag_error			# goto error handler
	nada
2:	showdata(T21,T23,T34,k0,SCD2_DEBUG)

	/* Invalidate current line now that we are done with it.
	 */
	li	a0,GCACHE_INITSTATE
	sd	a0,0(T31)

scd2_loop:
	addu	T21,1				# incr index
	daddiu	T31,(1<<TAGADDR_INDEX_SHIFT)	# incr index in tag ptr
	daddiu	T32,(1<<TAGADDR_INDEX_SHIFT)	# incr index in set ptr
	bltu	T21,T20,scd2_setX_top		# loop through scache indicies
	daddiu	T30,TCC_LINESIZE		# BDSLOT: increment address

scd2_setX_bottom:

	/* Bump to next pattern and redo entire loop.
	 */
	dli	a0,0x5555555555555555
	bnez	T22,1f				
	nada					# done with first pass
	b	scd2_top			# run with 5 pattern
	move	T22,a0				# BDSLOT
1:	bne	T22,a0,1f
	nada					# done with second pass
	dli	T22,0xaaaaaaaaaaaaaaaa		# run with a pattern
	b	scd2_top
	nada
1:
	/* Test completed.
	 */
	b	scd_done
	nop

	END(scache_data2)

	.data

scache_tag_fail:
	.asciiz "Tag RAM test FAILED @ 0x"
scache_data_tag_fail:
	.asciiz "GCache data tag FAILED @ 0x"
scache_data_fail:
	.asciiz "GCache data comparison FAILED @ 0x"

expect_msg:
	.asciiz "\texpected: 0x"
actual_msg:
	.asciiz " actual: 0x"
index_msg:
	.asciiz "\tindex: 0x"
set_msg:
	.asciiz " set: "
byte_msg:
	.asciiz "\tbyte: "
sram_msg:
	.asciiz " SRAM: "

even_msg:
	.asciiz " [even"
odd_msg:
	.asciiz " [odd"
state_msg:
	.asciiz " state]"
tag_msg:
	.asciiz " tag]"
even2_msg:
	.asciiz " [even]"
odd2_msg:
	.asciiz " [odd]"

even_srams:				# indexed by byte to get SRAM name
	.word	0x53313600		# S16
	.word	0x53313500		# S15
	.word	0x53313100		# S11
	.word	0x53313000		# S10
	.word	0x53313200		# S12
	.word	0x53360000		# S6
	.word	0x53350000		# S5
	.word	0x53340000		# S4
odd_srams:				# indexed by byte to get SRAM name
	.word	0x53313400		# S14
	.word	0x53313300		# S13
	.word	0x53380000		# S8
	.word	0x53370000		# S7
	.word	0x53390000		# S9
	.word	0x53330000		# S3
	.word	0x53320000		# S2
	.word	0x53310000		# S1

	.text

/* invalidate_scache
 *
 *  Initialize all GCache Tag and State RAMs by invalidating them.
 * Loop thru all indices and for each set, initializing both tag data
 * and state.
 *
 * The tag values written into the 4 sets of a given index must be different 
 * from each other.
 *
 * We use addresses in reserved GIO-space.
 */

LEAF(inval_gcache_tags)
        .set    noreorder

	/* get GCache size (in MB)
	 */
	LI	t0,PHYS_TO_K1(IP26_GCACHE_SIZE_ADDR)
	lb	t0,0(t0)

	/* Calculate number of entries at 2048 per MB of cache:
	 *	1MB - 2048
	 *	2MB - 4096
	 *	4MB - 16384
	 *	etc
	 */
	sll	t0,t0,11

	/* free registers : t8 t9 ta3 a1 */
	li	t1,0			# start with index 0
	dli	v1,PHYS_TO_K1(TCC_ETAG_DA)	# tag data base
	dli	v0,PHYS_TO_K1(TCC_ETAG_ST)	# tag state base
	dli	a2,GCACHE_INITSTATE	# tag state init value
	li	a3,0x1000000  		# incr tag addr to next set (16MB)
	li	t3,1<<TAGADDR_SET_SHIFT	# incr addr of tag addr to next set
	lui	ta0, 0x1900		# reserved GIO space

indexloop:
	.align	4
	/*  Start GCache tag in reserved GIO space, as these addresses will
	 * never be accessed.
	 */
	sll	t2,t1,TAGADDR_INDEX_SHIFT
	or	ta1,v1,t2		# calc address of tag data
	or	ta2,v0,t2		# calc address of tag state
	li	a0,4			# set loop count

setloop:
	/* Write into 4 sets of the index, starting from set 0.
	 */
	sd	ta0,0(ta1)		# write tag data
	sd	a2,0(ta2)		# write tag state

	/* Update addresses.
	 */
	addu	ta0,a3			# incr tag value one set
	daddu	ta1,t3			# incr tag data ptr one set
	daddu	ta2,t3			# incr tag state ptr one set

	/* Inner 'set' loop maint.
	 */
	addi	a0,-1
	bgt	a0,zero,setloop		# are we done with all 4 sets?
	nop

	/* Outter 'index' loop maint.
	 */
	addiu	t1,1
	ble	t1,t0,indexloop
	lui	ta0, 0x1900		# BDSLOT: reserved GIO space

	j	ra
	nada
        .set    reorder
END(inval_gcache_tags)
