#ident	"$Id: lmem_config.s,v 1.1 1994/07/21 00:18:20 davidl Exp $"
/**************************************************************************
 *									  *
 * 		 Copyright (C) 1993, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/


/*
 * lmem_conf.s -- local memory configuration
 */

#include <asm.h>
#include <early_regdef.h>
#include "sys/sbd.h"
#include "sys/mc.h"
#include "pon.h"
#include "monitor.h"

#define LED_GREEN_OFF 0x10
#define LED_RED_OFF 0x20
#define LED_AMBER 0x30
#define K0_RAMBASE    0x88000000
#define K1_RAMBASE    0xA8000000


#define WBFLUSHM				\
	.set	noreorder ;			\
	.set	noat	;			\
	li	AT, X_TO_K1(CPUCTRL0) ;	\
	lw	AT,0(AT) ;			\
	.set	at ;				\
	.set	reorder



#define	MAXBANKS	2	/* max number of memory banks */
#define	MAXUNITS	32	/* max number of memory units in a bank */
#define	UNITSIZE	0x400000	/* memory unit, 4 MB */
#define	MAXSIZE		(MAXUNITS * UNITSIZE)
#define	UNITSHIFT	22	/* 1 << 22 = 4MB */
#define	LOW_MEM_BOUND	0x60	/* low memory upper bound  / UNITSIZE */
#define	HIGH_MEM_BOUND	0x80	/* high memory lower bound / UNITSIZE */

/*
 * assume maximum memory size for each bank.  with MC, we can use kseg1 to
 * access local memory from 0x08000000 to 0x18000000, the rest of the local
 * memory from 0x20000000 to 0x30000000 must be mapped.  therefore, we are
 * going to size each bank individually with the other 3 banks set to
 * invalid and set the base adress to 0x08000000
 */
#define	PROBE_MEMCFG	0x3f20		/* valid, 1 subbank, 128M, address
					   start at 128M */


	.text

#define	LOCALMEMCFG0 0xbfa000c4
#define LOCALMEMCFG1 0xbfa000cc

LEAF(meminit_indy)

        move    s4,ra          # save return address in T33 (s4) 

        move    s6,zero        # bank counter
	li	s5,K1_RAMBASE

        /* invalidate all 4 banks of memory */
        li      v1,LOCALMEMCFG0
        sw      zero,0(v1)
        sw      zero,8(v1)

bank_loop_dmon:

        move    a0, s6
        li      a1, PROBE_MEMCFG
        jal     set_bank_conf

	move	a0, s6
	jal	size_bank 

	move	s7,v0		# size in 4MB

	move	a0, s6
	move	a1, s7
	jal	set_bank_size

	addi	s6,1
	bne	s6,MAXBANKS,bank_loop_dmon


	jal	set_bank_addrs
	move	a1,v0 
	move	a0, s5 
	jal	dmon_bank_init  
	j	s4	
END(meminit_indy)


/*

Actually writes addresses given the start address and the size
	a0 - starting address
	a1 - size, in bytes 
*/

LEAF(dmon_bank_init)

#define INITPATTERN	0x007cafe

	li	t1,INITPATTERN	
write_loop:
	sd	t1,0(a0)
	sd	t1,8(a0)
	sd	t1,16(a0)
	sd	t1,24(a0)
	addiu	a0, 32
	addiu	a1, -32
	bne	a1, zero, write_loop

	j	ra	
END(dmon_bank_init)



/*
 * size the given memory bank
 *	a0 - memory bank number
 *
 * return:
 *	v0 - memory bank size in unit of 4MB
 *	v1 - bad SIMMs code
 *
 * this is a Level 1 routine and is C-callable
 */
LEAF(size_bank)

#define	DATA	0x11111111

	move	s3,ra			# save return addr
	move	t3,a0			# save the bank number

	li	a0,K1_RAMBASE		# starting addr to probe
	li	a1,DATA

	/*
	 * since all 32 bits of a word come out of the same SIMM, we need
	 * to probe 4 different word addresses to make sure all 4 SIMMs
	 * in a bank are there
	 */
	.set	noreorder
	sw	a1,0(a0)		# use different data for each SIMM
	sll	a1,1			# such that we won't read back what
	sw	a1,4(a0)		# we write when there's no SIMM
	sll	a1,1
	sw	a1,8(a0)
	sll	a1,1
	sw	a1,12(a0)
	.set	reorder

	/*
	 * we must first make sure we have a complete set of SIMMs before
	 * determining the size of the SIMMs. we must do a store before
	 * doing a read to clear all 128 bits in the 2 DMUXs, otherwise,
	 * probing will give the wrong result.  same thing when we probe for
	 * the second subbanks
	 */
	move	v1,zero			# clear bad SIMMs code
	srl	a1,3			# get back the original DATA

	sw	zero,16(a0)
	lw	a2,0(a0)		# read back the first word
	beq	a1,a2,1f
	or	v1,BAD_SIMM0		# data mismatch, no SIMM 0

1:
	sw	zero,20(a0)
	lw	a2,4(a0)		# read back the second word
	sll	a1,1
	beq	a1,a2,2f
	or	v1,BAD_SIMM1		# data mismatch, no SIMM 1

2:
	sw	zero,24(a0)
	lw	a2,8(a0)		# read back the third word
	sll	a1,1
	beq	a1,a2,3f
	or	v1,BAD_SIMM2		# data mismatch, no SIMM 2

3:
	sw	zero,28(a0)
	lw	a2,12(a0)		# read back the fourth word
	sll	a1,1
	beq	a1,a2,4f
	or	v1,BAD_SIMM3		# data mismatch, no SIMM 3

4:
	beq	v1,zero,memory_available	# continue to probe if we have
						# a whole set of SIMMs
	/*
	 * a incomplete set of SIMMs shows up as no memory, the calling
	 * routine should interpret the bad SIMMs code and warn the user
	 * appropriately
	 */
	move	v0,zero			# indicate no memory found, bad SIMMs
	j	s3			# code in v1

/* a complete set of SIMMs is found.  determine size of the SIMMs */
memory_available:
	li	t4,MAXSIZE>>1		# max amount of memory in a subbank
	li	v0,UNITSIZE

	sw	zero,0(a0)		# clear locations where addr aliasing
	sw	zero,4(a0)		# are expected to occur
	sw	zero,8(a0)		# a0 still equal to K1_RAMBASE
	sw	zero,12(a0)

/*
 * on SIMMs with 2 subbanks, adddresses on one subbank is greater than all the
 * addresses on the other subbank.  initially, we assume only one subbank on
 * all SIMMs and increment by a factor of 4 each time
 */
size_loop:
	addu	a3,v0,a0		# starting probe addr
	srl	a1,3			# get back the original DATA

	.set	noreorder
	sw	a1,0(a3)
	sll	a1,1
	sw	a1,4(a3)
	sll	a1,1
	sw	a1,8(a3)
	sll	a1,1
	sw	a1,12(a3)
	.set	reorder

	lw	a2,0(a0)		# read back the first word
	beq	a2,zero,1f
	or	v1,BAD_SIMM0		# addr aliasing has occured

1:
	lw	a2,4(a0)		# read back the second word
	beq	a2,zero,2f
	or	v1,BAD_SIMM1		# addr aliasing has occured

2:
	lw	a2,8(a0)		# read back the third word
	beq	a2,zero,3f
	or	v1,BAD_SIMM2		# addr aliasing has occured

3:
	lw	a2,12(a0)		# read back the fourth word
	beq	a2,zero,4f
	or	v1,BAD_SIMM3		# addr aliasing has occured

4:
	bne	v1,zero,5f		# stop probing if addr aliasing has
					# occurred in one of the SIMMs
	sll	v0,2			# multiply by 4
	bne	v0,t4,size_loop	# continue if max has not been reached
	b	probe_subbank
				
5:
	beq	v1,BAD_ALLSIMMS,probe_subbank	# if not all SIMMs are of the
						# same size, no need to do more
	srl	v0,v0,UNITSHIFT
	j	s3	

/* determine whether the SIMMs have one or two subbanks */
probe_subbank:
	move	t4,v0			# save SIMMs size

	move	a0,t3			# bank number
	li	a1,PROBE_MEMCFG|MEMCFG_BNK	# set 2-subbanks bit
	jal	set_bank_conf

	li	a0,K1_RAMBASE+(MAXSIZE>>1)	# first addr in the 2nd subbank
	li	a1,DATA

	/*
	 * since all 32 bits of a word come out of the same SIMM, we need
	 * to probe 4 different word addresses to make sure all 4 SIMMs
	 * in a bank are there
	 */
	.set	noreorder
	sw	a1,0(a0)		# use different data for each SIMM
	sll	a1,1			# such that we won't read back what
	sw	a1,4(a0)		# we write when there's no SIMM
	sll	a1,1
	sw	a1,8(a0)
	sll	a1,1
	sw	a1,12(a0)
	.set	reorder

	move	v1,zero			# clear bad SIMMs code
	srl	a1,3			# get back the original DATA

	sw	zero,16(a0)
	lw	a2,0(a0)		# read back the first word
	beq	a1,a2,1f
	or	v1,BAD_SIMM0		# data mismatch, SIMM 0 has 1 subbank

1:
	sw	zero,20(a0)
	lw	a2,4(a0)		# read back the second word
	sll	a1,1
	beq	a1,a2,2f
	or	v1,BAD_SIMM1		# data mismatch, SIMM 1 has 1 subbank

2:
	sw	zero,24(a0)
	lw	a2,8(a0)		# read back the third word
	sll	a1,1
	beq	a1,a2,3f
	or	v1,BAD_SIMM2		# data mismatch, SIMM 2 has 1 subbank

3:
	sw	zero,28(a0)
	lw	a2,12(a0)		# read back the fourth word
	sll	a1,1
	beq	a1,a2,4f
	or	v1,BAD_SIMM3		# data mismatch, SIMM 3 has 1 subbank

4:
	bne	v1,zero,5f		# branch if at least 1 SIMM doesn't
					# has 2 subbanks
	srl	v0,t4,UNITSHIFT-1	# memory size in 4MB units
	j	s3	

5:
	move	a0,t3			# bank number
	li	a1,PROBE_MEMCFG		# clear 2-subbanks bit
	jal	set_bank_conf

	srl	v0,t4,UNITSHIFT		# memory size in 4MB units
	j	s3	

	END(size_bank)



/* 
 * set the size field within MEMCFG0/1, this also has the effect of
 * invalidate the valid bit.  those bits will be set correctly in 
 * set_bank_addr
 *	a0 - memory bank number
 *	a1 - memory bank size in unit of 4MB
 *
 * return
 *	v1 - half word with the correct size information
 *
 * this is a Level 1 routine and is C-callable
 */
LEAF(set_bank_size)
	move	s3,ra			# save return address

	bgt	a1,zero,1f

	move	a1,zero			# this bank has no memory
	jal	set_bank_conf
	j	s3	

1:      
	and	a2,a1,0x2a		# 8/32/128 MB ?
	beq	a2,zero,2f
	li	a2,MEMCFG_BNK		# set 2-subbanks bits for 8/32/128 MB bank
2:     

	sub	a1,1
	sll	a1,8
	
	or	a1,a2
	move	t3,a1

	or	a1,MEMCFG_ADDR_MASK	# use this to distinguish between an

	jal	set_bank_conf		# invalid bank and a bank with 256Kx32


	move	v0,t3
	j	s3	
	END(set_bank_size)



/* 
 * set the base address field within MEMCFG0/1.  memory bank with larger size
 * memory must have a lower base address.  in case multiple banks have the
 * the same size memory, lower base address should go to the bank with a lower
 * bank number
 *
 * return:
 *	v0 - memory size in bytes
 *
 * this should be a Level 1 routine, but since we need more than 2 local
 * registers for manipulation, it is promoted to a level 2 routine and is not
 * C-callable
 */
LEAF(set_bank_addrs)
	move	s3,ra

	li	t3,MAXBANKS			# count up to 4 banks
	li	t4,PHYS_RAMBASE>>UNITSHIFT 	# base address
	li	t5,MAXUNITS			# start with maximum size

next_size:
	subu	t6,t5,1		# bank size
	move	t7,zero		# count up from bank 0

next_bank:
	move	a0,t7		# current bank number
	jal	get_bank_conf		# get bank configuration info

	beq	v0,zero,2f		# bank invalid

/*  Swap T23 with T00  */

	and	t0,v0,MEMCFG_DRAM_MASK

	srl     t0,8
	bne	t0,t6,2f		# branch if not the right size
	or	v0,v0,MEMCFG_VLD	# turn valid bit on
	and	a1,v0,~MEMCFG_ADDR_MASK	# mask off address bits
	or	a1,t4			# add address offset to config word
	move	a0,t7			# bank number
	jal	set_bank_conf		# set the config word for the bank
	addu	t4,t5			# increment address offset
	bne	t4,LOW_MEM_BOUND,1f	

	/* end of the lower section of local memory */
	li	t4,HIGH_MEM_BOUND

1:
	subu	t3,1			# decrement configured bank count
	beq	t3,zero,addr_done

2:
	add	t7,1			# increment bank number
	bne	t7,MAXBANKS,next_bank

	srl	t5,1			# compute next bank size
	bne	t5,zero,next_size

addr_done:
	jal	szmem			# can't use T11<<UNITSHIFT - PHYS_RAMBASE
					# because there is a gap in local memory
	j	s3	
	END(set_bank_addrs)


/*
 * set the configuration of the specified bank
 *	a0 - bank number
 *	a1 - bank configuration
 *
 * this is a Level 0 routine and is C-callable
 */
LEAF(set_bank_conf)
	move	s2,ra	
	li	t0,X_TO_K1(MEMCFG0)	# addr of MEMCFG0
	blt	a0,2,1f
	addu	t0,LOCALMEMCFG1-LOCALMEMCFG0	# addr of MEMCFG1

1:
	/* determine which half of the memory configuration register to use */
	lw	t1,0(t0)	# content of the memory configuration register
	and	t2,a0,0x1	# bank 0/2 or 1/3 ?
	bne	t2,zero,2f	# branch if bank 1/3

	sll	a1,16		# shift left 16 bits to set bank 0/2 info
	li	t2,0xffff	# load mask for upper half word

	b	3f

2:
	lui	t2,0xffff	# load mask for lower half word

3:
	and	t1,t2		# mask out the old content
	or	t1,a1		# or in the new content

	sw	t1,0(t0)	# save content of config reg

	WBFLUSHM
	
	j	s2	
	END(set_bank_conf)


/*
 * get the configuration of the specified bank
 *	a0 - bank number
 *
 * return:
 *	v0 - configuration info in the lower 16 bits
 *
 * this is a Level 0 routine and is C-callable
 */
LEAF(get_bank_conf)
	li	T00,X_TO_K1(MEMCFG0)	# addr of MEMCFG0
	blt	a0,2,1f
	addu	T00,MEMCFG1-MEMCFG0	# addr of MEMCFG1

1:
	/* determine which half of the memory configuration register to use */
	lw	v0,0(T00)	# content of the memory configuration register

	and	T01,a0,0x1	# bank 0/2 or 1/3 ?
	bne	T01,zero,2f	# branch if bank 1/3
	srl	v0,16		# shift right 16 bits to get bank 0/2 info

2:
	and	v0,0xffff	# mask off unwanted bits

	j	ra
	END(get_bank_conf)
#endif 	/* !ENETBOOT */



/*
 * use MEMCFG0/1 to determine amount of memory in bytes
 *
 * return:
 *	v0 - amount of memory in bytes
 *
 * this is a Level 0 routine and is C-callable
 */
LEAF(szmem)
#ifdef	ENETBOOT
XLEAF(init_memconfig)
	mthi	zero			# clear out bad SIMMs code
#endif

	li	v1,X_TO_K1(MEMCFG0)	# addr of MEMCFG0
	addu	T00,v1,MEMCFG1-MEMCFG0	# addr of MEMCFG1
	move	v0,zero			# amount of memory found so far

1:
	lw	T02,0(v1)		# content of MEMCFG0/1

	/* bank 1/3 */
	and	T01,T02,MEMCFG_VLD	# get valid bit
	beq	T01,zero,2f		# branch if this bank is not valid

	and	T01,T02,MEMCFG_DRAM_MASK	# get bank size
	addu	T01,0x0100
	sll	T01,14
	addu	v0,T01			# add to accumulated total
	
2:
	srl	T02,16			# want the upper half word

	/* bank 0/2 */
	and	T01,T02,MEMCFG_VLD	# get valid bit
	beq	T01,zero,3f		# branch if this bank is not valid

	and	T01,T02,MEMCFG_DRAM_MASK	# get bank size
	addu	T01,0x0100
	sll	T01,14
	addu	v0,T01			# add to accumulated total
	
3:
	beq	v1,T00,4f
	move	v1,T00
	b	1b

4:
	j	ra

	END(szmem)

LEAF(pon_putc)
	nop
	j ra
END(pon_putc)



/*
 * clear a range of memory. addresses must have 16 bytes alignment
 *      a0 - address of the first word to be cleared
 *      a1 - address of the last word to be cleared + 4
 *
 * this is a Level 0 routine and is C-callable
 */
LEAF(hmclear)
1:
        sw      zero,0(a0)
        sw      zero,4(a0)
        sw      zero,8(a0)
        sw      zero,12(a0)
        addu    a0,16
        blt     a0,a1,1b
        j       ra
END(hmclear)

nomem_msg:
	.asciiz "\r\nNo usable memory found.  Make sure you have a full bank (4 SIMMs).\r\n"

membank_msg:
	.asciiz	"\r\nBank "
probefail_msg:
	.asciiz " memory probe *FAILED*\r\n\tCheck or replace:  SIMM"
datafail_msg:
	.asciiz " memory diagnostics (data test) *FAILED*\r\n\tCheck or replace:  SIMM"

addrfail_msg:
	.asciiz "\r\nMemory diagnostic (address test) *FAILED*\r\n\tMemory is not usable.  Check or replace all SIMMs.\r\n"


#ifdef	VERBOSE
mem_size_done:	.asciiz "\r\nMemory sizing done: "
#endif
