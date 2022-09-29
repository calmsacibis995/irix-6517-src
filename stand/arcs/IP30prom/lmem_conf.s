#ident	"IP30prom/lmem_conf.s:  $Revision: 1.29 $"

/* lmem_conf.s -- local memory configuration */

#include <asm.h>
#include <early_regdef.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <pon.h>

#define WBFLUSHM					\
	.set	noreorder ;				\
	ld	zero,PHYS_TO_COMPATK1(HEART_BASE);	\
	.set	at ;					\
	.set	reorder

/*
 * in rev 2 HEART, the lowest 16K bytes of memory can only be accessed through
 * 0x0000-0x4000, and the rest of memory can only be accessed through 
 * address greater 1/2 G, in order to simplifying the probing routine, we
 * temporary map the start of the memory space at 1/2 G + 2 G.
 */
#define	MAX_BANKS	4		/* max # of memory banks */
#define	MAX_UNITS	64		/* max # of memory units per bank */
#define	UNIT_SIZE	0x2000000	/* memory unit, 32 MB */
#define	MAX_BANK_SIZE	(MAX_UNITS * UNIT_SIZE)		/* 2 GB */
#define	UNIT_SHIFT	HEART_MEMCFG_UNIT_SHFT		/* 1 << 25 = 32 MB */
#define	PROBE_MEMBASE	(K1BASE + ABS_SYSTEM_MEMORY_BASE + MAX_BANK_SIZE)
#define	PROBE_MEMCFG	0x81bf0040	/* VLD=1, MSIZE=110111111,
					 * BASE=001000000 */
#define	ASCII_0		0x30
#define	ASCII_1		(ASCII_0+1)
#define	ASCII_S		0x53
#define	ASCII_SPACE	0x20


	.text
#ifndef ENETBOOT

/* configure HEART_MEMCFG0/1 */
LEAF(init_memconfig)
	move	RA4,ra		# save return address

	/* use the jumper to determine verbosity during memory probe */
	LI	v0,PHYS_TO_COMPATK1(BRIDGE_BASE+BRIDGE_INT_STATUS)
	lw	v0,0(v0)
	and	v0,PROM_PASSWD_DISABLE
	mtlo	v0		# !0 -> verbose

probe_memory:
	/* invalidate all 4 banks of memory */
	LI	v0,PHYS_TO_COMPATK1(HEART_MEMCFG0)
	sd	zero,0(v0)
	sd	zero,HEART_MEMCFG(1)-HEART_MEMCFG0(v0)

	move	T40,zero	# bank counter
	mthi	zero		# clear out bad DIMMs code

bank_loop: 	
	move	a0,T40		# bank number
	li	a1,PROBE_MEMCFG
	jal	set_bank_conf

	/* find the size, in 32 MB, of the current bank */ 
	move	a0,T40		# bank number
	jal	size_bank
	beq	v0,zero,bank_done

	/* run walking bit test */
	dli	a0,PROBE_MEMBASE	# starting address
	dsll	a1,v0,UNIT_SHIFT	# bank size in bytes

	jal	pon_walkingbit
	beq	v0,zero,test_done	# v0==0 ->  walking bit test passed

	/* walking bit test failed. print message and invalidate this bank */
	mfhi	a3
	dsll	v1,T40,1	# bank number * 2
	dsllv	v1,v0,v1	# shift bad DIMMs code
	or	a3,v1		# or in bad DIMMs code
	mthi	a3

	move	a0,T40		# bank number
	move	a1,v0		# bad DIMMs code
	dla	a2,datafail_msg
	jal	memfail		# print nice error message

	move	a0,T40
	move    a1,zero		# invalidate this bank
	jal	set_bank_conf
	b	bank_done

test_done:
	/*
	 * invalidate the bank for now, DIMM size information should be
	 * correct.  set the HEART_MEMCFG_ADDR_MSK to all 1's to
	 * differentiate between an empty and a 32 MB bank
	 */
	move	a0,T40		# bank number
	jal	get_bank_conf
	and	a1,v0,~HEART_MEMCFG_VLD
	or	a1,HEART_MEMCFG_ADDR_MSK
	move	a0,T40
	jal	set_bank_conf

bank_done:
	add	T40,1		# increment bank number
	bne	T40,MAX_BANKS,bank_loop

	/* set the base address fields in HEART_MEMCFG0/1 */
	jal	set_bank_addrs
	move	T41,v0		# memory size in bytes

#ifdef	VERBOSE
	dla	a0,mem_size_done
	jal	pon_puts
	LI	T40,PHYS_TO_COMPATK1(HEART_MEMCFG0)
	ld	a0,0(T40)
	jal	pon_puthex64
	li	a0,ASCII_SPACE
	jal	pon_putc
	ld	a0,HEART_MEMCFG(1)-HEART_MEMCFG0(T40)
	jal	pon_puthex64
	dla	a0,crlf
	jal	pon_puts
#endif

	bne	T41,zero,addr_test

	/*
	 * if no memory found, turn on verbose level and re-probe memory
	 * to get some debugging help
	 */
	mflo	v0
	bne	v0,zero,1f
	li	v0,1
	mtlo	v0
	b	probe_memory
1:
	/* no memory, print message and flash LED forever */
	dla	a0,nomem_msg
	jal	pon_puts
	jal	pon_flash_led		# never returns

addr_test:
	dli	a0,PROBE_MEMBASE	# first address
	move	a1,T41			# size in bytes

	jal	pon_addr
	beq	v0,zero,mem_ok

	/* test failed, print message and flash LED forever */
	dla	a0,addrfail_msg
	jal	pon_puts
	jal	pon_flash_led		# never returns

mem_ok:
	/*
	 * now that we are done with probing, re-map the memory the normal
	 * way
	 */
	move	T40,zero
1:
	move	a0,T40
	jal	get_bank_conf
	and	v1,v0,HEART_MEMCFG_VLD
	beq	v1,zero,2f	# branch if not a valid bank

	/* take out the 2 gigabyte offset */
	dsubu	a1,v0,MAX_UNITS
	move	a0,T40
	jal	set_bank_conf
2:
	daddu	T40,1
	blt	T40,MAX_BANKS,1b

#ifndef SABLE			/* sable clears mem for us */
	/*
	 * just clear memory between start of the PROM BSS and the top of
	 * the PROM stack.  no point to use uncached accelerate access
	 * here since the PROM is uncached, which implies flushing of the
	 * uncached buffer on every instruction fetch
	 */
	dli	a0,PROM_BSS
	dli	a1,PROM_STACK
	jal	hmclear
#endif	/* EMULATION */

	move	v0,T41
	j	RA4
	END(init_memconfig)


/*
 * print error message
 *	a0 - bank number of the failing memory word
 *	a1 - bad DIMMs mask
 *	a2 - address of error message string
 */
LEAF(memfail)
	move	RA3,ra			# save return address
	move	T30,a0			# save bank number
	move	T31,a1			# save bad DIMMs mask
	move	T32,a2			# error string

	dla	a0,membank_msg
	jal	pon_puts

	add	a0,T30,ASCII_0		# convert bank number to ASCII
	jal	pon_putc

	move	a0,T32
	jal	pon_puts

	and	a0,T31,BAD_DIMM0
	beq	a0,zero,1f

	li	a0,ASCII_SPACE
	jal	pon_putc
	li	a0,ASCII_S
	jal	pon_putc

	dsll	a0,T30,1		# bank number * 2
	add	a0,1+ASCII_0
	jal	pon_putc

1:
	and	a0,T31,BAD_DIMM1
	beq	a0,zero,1f

	li	a0,ASCII_SPACE
	jal	pon_putc
	li	a0,ASCII_S
	jal	pon_putc

	dsll	a0,T30,1		# bank number * 2
	add	a0,2+ASCII_0
	jal	pon_putc

1:
	dla	a0,crlf
	jal	pon_puts

	j	RA3
	END(memfail)


/*
 * print message with bank and DIMM(s) number appended
 *	a0 - bank number
 *	a1 - DIMMs mask
 *	a2 - address of message string
 */
LEAF(print_message_bank_dimm)
	move	RA2,ra			# save return address
	move	T20,a0			# save bank number
	move	T21,a1			# save DIMMs mask

	move	a0,a2
	jal	pon_puts

	add	a0,T20,ASCII_0		# convert bank number to ASCII
	jal	pon_putc

	dla	a0,memdimm_msg
	jal	pon_puts

	and	a0,T21,BAD_DIMM0
	beq	a0,zero,1f

	li	a0,ASCII_S
	jal	pon_putc

	dsll	a0,T20,1		# bank number * 2
	add	a0,1+ASCII_0
	jal	pon_putc

	li	a0,ASCII_SPACE
	jal	pon_putc

1:
	and	a0,T21,BAD_DIMM1
	beq	a0,zero,1f

	li	a0,ASCII_S
	jal	pon_putc

	dsll	a0,T20,1		# bank number * 2
	add	a0,2+ASCII_0
	jal	pon_putc

1:
	dla	a0,crlf
	jal	pon_puts

	j	RA2
	END(print_message_bank_dimm)



/* print result of the probe */
LEAF(print_probe_message)
	mflo	v0
	bne	v0,zero,1f		# verbose on ?
	j	ra

1:
	move	RA2,ra

	move	T20,a0			# address
	move	T21,a1			# expected data
	move	T22,a2			# actual data
	move	T23,a3			# bank number

	LA	a0,membank_msg
	jal	pon_puts
	daddu	a0,T23,ASCII_0
	jal	pon_putc

	LA	a0,memaddr_msg
	jal	pon_puts
	move	a0,T20
	jal	pon_puthex64

	LA	a0,memexpected_msg
	jal	pon_puts
	move	a0,T21
	jal	pon_puthex64

	LA	a0,memactual_msg
	jal	pon_puts
	move	a0,T22
	jal	pon_puthex64

	LA	a0,crlf
	jal	pon_puts

	/* use the jumper to determine whether to print out more info */
	LI	v0,PHYS_TO_COMPATK1(BRIDGE_BASE+BRIDGE_INT_STATUS)
	lw	v0,0(v0)
	and	v0,PROM_PASSWD_DISABLE
	beq	v0,zero,print_probe_message_done

	LA	a0,mfg_memxor_msg
	jal	pon_puts
	xor	T21,T22			# xor result
	move	a0,T21
	jal	pon_puthex64

	LA	a0,mfg_memdimm_msg
	jal	pon_puts
	dsll	a0,T23,1		# bank number * 2
	and	a1,T20,0x8		# DIMM within bank
	dsrl	a1,3
	dadd	a0,a1
	dadd	a0,ASCII_1		# DIMM number start at 1
	jal	pon_putc

	LA	a0,mfg_membx_msg
	jal	pon_puts

	and	a0,T21,0x3ffff<<0	# bit 0..17
	beq	a0,zero,1f
	LA	a0,mfg_membx_u32_msg
	jal	pon_puts

1:
	and	a0,T21,0x3fff<<18	# bit 18..31
	beq	a0,zero,1f
	LA	a0,mfg_membx_u33_msg
	jal	pon_puts

1:
	and	a0,T21,0x3fff<<32	# bit 32..45
	beq	a0,zero,1f
	LA	a0,mfg_membx_u35_msg
	jal	pon_puts

1:
	and	a0,T21,0x3ffff<<46	# bit 46..63
	beq	a0,zero,1f
	LA	a0,mfg_membx_u36_msg
	jal	pon_puts

1:
	LA	a0,mfg_msg_end
	jal	pon_puts

print_probe_message_done:
	j	RA2
	END(print_probe_message)



/*
 * size the given memory bank
 *	a0 - memory bank number
 *
 * return:
 *	v0 - memory bank size in unit of 32 MB
 */
LEAF(size_bank)

#define	DATA	0x5555555555555555

	move	RA3,ra			# save return addr
	move	T30,a0			# save the bank number

	dli	a0,PROBE_MEMBASE	# starting addr to probe
	dli	a1,DATA

	/*
	 * since all 64 bits of a double word come out of the same DIMM,
	 * we need to probe 2 different double word addresses to make
	 * sure both DIMMs in a bank are there
	 */
	.set	noreorder
	sd	a1,0(a0)
	dsll	a1,1
	sd	a1,8(a0)
	dsrl	a1,1			# get back the original DATA
	.set	reorder

	/*
	 * make sure we have a complete set of DIMMs before
	 * determining the size of the DIMMs. a store must be done before
	 * a load to clobber all 128 bits in the data path
	 */
	move	T34,zero			# clear bad DIMMs code

	sd	zero,16(a0)
	sd	zero,24(a0)

	ld	a2,0(a0)		# read back the first double word
	beq	a1,a2,1f
	or	T34,BAD_DIMM0		# data mismatch, no DIMM 0

	move	T32,a0
	move	T33,a1
	move	a3,T30
	jal	print_probe_message
	move	a0,T32
	move	a1,T33

1:
	dsll	a1,1
	ld	a2,8(a0)		# read back the second double word
	beq	a1,a2,2f
	or	T34,BAD_DIMM1		# data mismatch, no DIMM 1

	move	T32,a0
	move	T33,a1
	move	a3,T30
	daddu	a0,8
	jal	print_probe_message
	move	a0,T32
	move	a1,T33

2:
	beq	T34,zero,DIMMs_found	# continue if we have a whole set

	/* assume no memory if date mismatch occur in both DIMMs */
	li	a2,BAD_ALLDIMMS
	beq	T34,a2,4f

	/* bad DIMM(s) and incomplete bank */
	move	a0,T30
	move	a1,T34
	dla	a2,badmem_msg
	jal	print_message_bank_dimm

4:
	dli	a0,PROBE_MEMBASE
	sd	zero,0(a0)
	sd	zero,8(a0)
	sd	zero,16(a0)
	sd	zero,24(a0)

	/*
	 * set up the configuration register to indicate this bank has no
	 * usable memory
	 */
	move	a0,T30
	move	a1,zero
	jal	set_bank_conf

	move	v0,zero			# indicate no memory found
	j	RA3

/* a complete set of DIMMs is found.  determine size of the DIMMs */
DIMMs_found:
	dli	a0,PROBE_MEMBASE
	jal	get_DIMM_config
	move	T31,v0

	dli	a0,PROBE_MEMBASE+0x8
	jal	get_DIMM_config

	beq	v0,T31,5f

	/* mismatched DIMM */
	move	a0,T30
	li	a1,BAD_ALLDIMMS
	dla	a2,mismatchmem_msg
	jal	print_message_bank_dimm

	b	4b

5:
	dli	a0,PROBE_MEMBASE
	sd	zero,0(a0)
	sd	zero,8(a0)
	sd	zero,16(a0)
	sd	zero,24(a0)

	or	a1,T31,HEART_MEMCFG_VLD
	move	a0,T30
	jal	set_bank_conf

	/* return bank size in 32 MB chunks */
	and	T32,T31,HEART_MEMCFG_RAM_MSK
	dsrl	T32,HEART_MEMCFG_RAM_SHFT
	add	T32,1

	.set	noat
	LI	AT,PHYS_TO_COMPATK1(HEART_BASE+WIDGET_ID)
	lwu	AT,0(AT)
	dsrl	AT,WIDGET_REV_NUM_SHFT
	bge	AT,6,1f
	.set	at

	/* rev < 6 HEART does not support 4-bank 64MBits DIMMs */
#define	HEART_MEMCFG_DENSITY_SHFT	22
	and	T31,HEART_MEMCFG_DENSITY
	dsrl	T31,HEART_MEMCFG_DENSITY_SHFT
	and	T31,1
	beq	T31,zero,2f

	/*
	 * we have 64Mb DIMMs in this bank
	 * with the rev < 6 heart and 64Mb DIMMs, the locations pointed to
	 * by a0 and a1 are aliases of each other.  the signal CA12 is not
	 * used and is driven as CA13
	 */
	dli	a0,PROBE_MEMBASE+(0x1<<7)		# CA13==1
	dli	a1,PROBE_MEMBASE+(0x1<<7)+(0x1<<25)	# CA13==CA12==1
	move	v0,zero
	not	v1,zero
	sd	v0,0(a0)
	sd	v1,0(a1)
	ld	v0,0(a0)
	ld	v1,0(a1)
	sd	zero,0(a0)
	sd	zero,0(a1)
	bne	v0,v1,2f		# no aliasing

	/* don't want this message show up twice in the tty console */
#ifndef IP30_RPROM
	LA	a0,membank_msg
	jal	pon_puts
	daddu	a0,T30,ASCII_0
	jal	pon_putc
	LA	a0,unsupported_dimm
	jal	pon_puts
#endif	/* IP30_RPROM */

	/* these are 4-bank 64MBits DIMMs, invalidate this bank */
	move	a0,T30
	move	a1,zero
	jal	set_bank_conf

	/* return as no memory found */
	move	v0,zero
	j	RA3

1:
	/*
	 * this is put in for rev >=6 HEART.  when set up as the max bank
	 * configuration during memory probe, the CA9/RA13 signals are tied
	 * together, this causes the code to think we have twice the amount of
	 * memory in the bank with 64Mb SDRAMs
	 */
	dsll	v1,T32,UNIT_SHIFT-1	# * 32MB / 2, half the size of DIMM bank
	dli	a0,PROBE_MEMBASE
	daddu	v1,a0,v1		# address in the middle of the bank
	sd	zero,0(a0)
	not	a2,zero
	sd	a2,0(v1)
	ld	a2,0(a0)
	sd	zero,0(a0)
	sd	zero,0(v1)
	beq	a2,zero,2f		# no aliasing

	dsrl	T32,1			# only have half as we thought

	/* update the HEART memory config register */
	move	a0,T30
	jal	get_bank_conf
	and	v1,v0,HEART_MEMCFG_RAM_MSK
	and	v0,~HEART_MEMCFG_RAM_MSK
	dsrl	v1,HEART_MEMCFG_RAM_SHFT+1
	dsll	v1,HEART_MEMCFG_RAM_SHFT
	or	a1,v0,v1
	move	a0,T30
	jal	set_bank_conf


2:
	move	v0,T32
	j	RA3

	END(size_bank)



LEAF(get_DIMM_config)
	move	v0,zero
	dli	v1,0x1<<30

	/* one physical bank or two ? */
	dli	a2,0x1111111111111111
	daddu	a1,a0,0x1<<8		# assert CS1*
	sd	a2,0(a1)
	not	a2
	sd	a2,0(a0)		# clobber data lines
	not	a2
	ld	a3,0(a1)
	bne	a3,a2,1f		# second bank doesn't exist
	or	v0,0x8<<2

1:
	/* 256 MB DRAM ? */
	dsll	a2,1
	sd	a2,0(a0)
	daddu	a1,a0,0x1<<27		# RA14
	not	a2
	sd	a2,0(a1)
	not	a2
	ld	a3,0(a0)
	bne	a3,a2,2f		# address aliasing
	or	v0,0x4<<2
	b	3f

2:
	/* this is added to support >= rev6 HEART */
	.set	noat
	LI	AT,PHYS_TO_COMPATK1(HEART_BASE+WIDGET_ID)
	lwu	AT,0(AT)
	dsrl	AT,WIDGET_REV_NUM_SHFT
	blt	AT,6,2f
	dli	v1,0x1<<26
	.set	at


2:
	/* 64 MB DRAM ? */
	dsll	a2,1
	sd	a2,0(a0)
	daddu	a1,a0,0x1<<25		# RA12
	not	a2
	sd	a2,0(a1)
	not	a2
	ld	a3,0(a0)
	bne	a3,a2,3f		# address aliasing
	or	v0,0x2<<2

3:
	dsll	a2,1
	sd	a2,0(a0)
	daddu	a1,a0,v1		# CA9 or CA11
	not	a2
	sd	a2,0(a1)
	not	a2
	ld	a3,0(a0)
	bne	a3,a2,4f		# address aliasing
	or	v0,0x1<<2

4:
	dla	v1,msize
	daddu	v1,v0
	lwu	v0,0(v1)
	j	ra
	
	.data
msize:	.word	0x00000040,0x00010040,0x00430040,0x00470040
	.word	0x008f0040,0x009f0040,0x00000040,0x00000040
	.word	0x01010040,0x01030040,0x01470040,0x014f0040
	.word	0x019f0040,0x01bf0040,0x00000040,0x00000040
	.text
	END(get_DIMM_config)




/* 
 * set the BASE and the VALID fields in HEART_MEMCFG0/1
 *
 * return:
 *	v0 - memory size in bytes
 */
LEAF(set_bank_addrs)
	move	RA2,ra

	li	T10,MAX_BANKS		# number of banks to configure
	/* BASE value to use */
	li	T11,(PROBE_MEMBASE-K1BASE-ABS_SYSTEM_MEMORY_BASE)>>UNIT_SHIFT
	li	T20,MAX_UNITS		# current bank size

next_size:
	subu	T21,T20,1		# current MSIZE value
	move	T22,zero		# current bank

next_bank:
	move	a0,T22			# bank number
	jal	get_bank_conf		# get bank configuration info
	beq	v0,zero,2f		# bank invalid

	and	T23,v0,HEART_MEMCFG_RAM_MSK
	dsrl	T23,HEART_MEMCFG_RAM_SHFT
	bne	T23,T21,2f		# branch if not the right size

	or	v0,HEART_MEMCFG_VLD	# turn valid bit on
	and	a1,v0,~HEART_MEMCFG_ADDR_MSK	# mask off address bits
	or	a1,T11			# add address offset to config word
	move	a0,T22			# bank number
	jal	set_bank_conf		# set the config word for the bank
	addu	T11,T20			# increment address offset

	subu	T10,1			# decrement configured bank count
	beq	T10,zero,addr_done

2:
	add	T22,1			# increment bank number
	bne	T22,MAX_BANKS,next_bank

	dsrl	T20,1			# compute next bank size
	bne	T20,zero,next_size

addr_done:
	dsubu	T11,(PROBE_MEMBASE-K1BASE-ABS_SYSTEM_MEMORY_BASE)>>UNIT_SHIFT
	dsll	v0,T11,UNIT_SHIFT
	j	RA2
	END(set_bank_addrs)



/*
 * set the configuration of the specified bank
 *	a0 - bank number
 *	a1 - bank configuration
 */
LEAF(set_bank_conf)
	/* Figure out which memory configuration register and which half of it
	 * to use.
	 */
	LI	T00,PHYS_TO_COMPATK1(HEART_MEMCFG0)
	dsll	a0,2
	daddu	T00,a0

	sw	a1,0(T00)		# write the new value
	WBFLUSHM
	j	ra
	END(set_bank_conf)



/*
 * get the configuration of the specified bank
 *	a0 - bank number
 *
 * return:
 *	v0 - configuration info in the lower 32 bits
 */
LEAF(get_bank_conf)
	/* Figure out which memory configuration register and which half of it
	 * to use.
	 */
	LI	T00,PHYS_TO_COMPATK1(HEART_MEMCFG0)
	dsll	a0,2			# register and which half of it to
	daddu	T00,a0			# use

	lwu	v0,0(T00)		# read the value
	j	ra
	END(get_bank_conf)
#endif 	/* !ENETBOOT */


/*
 * clear a range of memory. addresses must have 2nd cache block, 128 bytes,
 * alignment since we want to make use of the uncached accelerate write
 *	a0 - address of the first word to be cleared
 *	a1 - address of the last word to be cleared
 */
LEAF(hmclear)
	.set	noreorder
	LI	a3,PHYS_TO_K1(0x0)

1:
	and	a2,a0,0x3fff	# 128 cachelines
	bne	a2,zero,2f
	daddu	a0,128		# BDSLOT

	/*  give a chance to refresh memory */
	ld	zero,0(a3)
2:
	sd	zero,-128(a0)
	sd	zero,-120(a0)
	sd	zero,-112(a0)
	sd	zero,-104(a0)
	sd	zero,-96(a0)
	sd	zero,-88(a0)
	sd	zero,-80(a0)
	sd	zero,-72(a0)
	sd	zero,-64(a0)
	sd	zero,-56(a0)
	sd	zero,-48(a0)
	sd	zero,-40(a0)
	sd	zero,-32(a0)
	sd	zero,-24(a0)
	sd	zero,-16(a0)
	bltu	a0,a1,1b
	sd	zero,-8(a0)

	j	ra
	nop

	.set	reorder
	END(hmclear)

/*
 * use HEART_MEMCFG0/1 to determine amount of memory in bytes
 *
 * return:
 *	v0 - amount of memory in bytes
 */
LEAF(szmem)
#ifdef	ENETBOOT
XLEAF(init_memconfig)
	mthi	zero			# clear out bad DIMMs code
#endif

	LI	v1,PHYS_TO_COMPATK1(HEART_MEMCFG0)
	daddu 	T00,v1,HEART_MEMCFG(1)-HEART_MEMCFG0
	move	v0,zero			# amount of memory found so far

1:
	ld	T02,0(v1)		# content of HEART_MEMCFG0/1

	/* bank 1/3 */
	and	T01,T02,HEART_MEMCFG_VLD
	beq	T01,zero,2f		# ignore if VALID bit is not set

	and	T01,T02,HEART_MEMCFG_RAM_MSK
	dsrl	T01,HEART_MEMCFG_RAM_SHFT
	addu	T01,0x1			# get bank size
	addu	v0,T01			# add to cumulative total
	
2:
	dsrl32	T02,0			# want the upper word

	/* bank 0/2 */
	and	T01,T02,HEART_MEMCFG_VLD
	beq	T01,zero,3f		# ignore if VALID bit is not set

	and	T01,T02,HEART_MEMCFG_RAM_MSK
	dsrl	T01,HEART_MEMCFG_RAM_SHFT
	addu	T01,0x1			# get bank size
	addu	v0,T01			# add to cumulative total
	
3:
	beq	v1,T00,4f
	move	v1,T00
	b	1b

4:
	dsll	v0,UNIT_SHIFT		# convert into bytes
	j	ra

	END(szmem)



	.data
badmem_msg:
	.asciiz	"\r\nBad or missing DIMM in bank "
mismatchmem_msg:
	.asciiz	"\r\nMismatched DIMM(s) found in bank "
nomem_msg:
	.asciiz "\r\nNo usable memory found.  Make sure you have at least one full bank (2 DIMMs).\r\n"

membank_msg:
	.asciiz	"\r\nBank "
memaddr_msg:
	.asciiz	": Address: "
memexpected_msg:
	.asciiz	", Expected: "
memactual_msg:
	.asciiz	", Actual: "

datafail_msg:
	.asciiz " memory diagnostics (data test) *FAILED*\r\n\tCheck or replace:  DIMM"

addrfail_msg:
	.asciiz "\r\nMemory diagnostic (address test) *FAILED*\r\n\tMemory is not usable.  Check or replace all DIMMs.\r\n"

unsupported_dimm:
	.asciiz	": This version of HEART (< 6) does not support 4-bank 64Mb DIMMs.\r\n"

#ifdef	VERBOSE
mem_size_done:	.asciiz "Memory sizing done: "
#endif

mfg_memxor_msg:
	.asciiz	"   (XOR: "
memdimm_msg:
mfg_memdimm_msg:
	.asciiz	", DIMM "
mfg_membx_msg:
	.asciiz ", BX"
mfg_membx_u32_msg:
	.asciiz	" U32"
mfg_membx_u33_msg:
	.asciiz	" U33"
mfg_membx_u35_msg:
	.asciiz	" U35"
mfg_membx_u36_msg:
	.asciiz	" U36"
mfg_msg_end:
	.asciiz	")\r\n"
