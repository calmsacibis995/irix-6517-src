#ident	"IP20prom/ledprom.s:  $Revision: 1.6 $"

#include <asm.h>
#include <regdef.h>
#include <sys/cpu.h>
#include <sys/sbd.h>

#define	CONFIG_COHRNT_EXLWR	0x5

#define WBFLUSHM					\
	.set	noreorder; 				\
	.set	noat;					\
	li	AT,PHYS_TO_K1(CPUCTRL0);		\
	lw	AT,0(AT);				\
	.set	at; 					\
	.set	reorder

/*
 * BEGIN: prom entry point table
 */
LEAF(start)
				/* 0x000 offset */
	j	realstart	/* prom entry point */
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
				/* 0x040 offset */
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
				/* 0x080 offset */
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
				/* 0x0c0 offset */
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
				/* 0x100 offset */
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
				/* 0x140 offset */
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
				/* 0x180 offset */
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
				/* 0x1c0 offset */
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
				/* 0x200 offset */
	j	bev_utlbmiss	/* utlbmiss boot exception vector, 32 bits */
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
				/* 0x240 offset */
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
				/* 0x280 offset */
	j	bev_xutlbmiss	/* utlbmiss boot exception vector, 64 bits */
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
				/* 0x2c0 offset */
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
				/* 0x300 offset */
	j	bev_cacheerror
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
				/* 0x340 offset */
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
				/* 0x380 offset */
	j	bev_general
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented

/*
 * END: prom entry point table
 */

realstart:
	li	v0,SR_BEV|SR_DE
	.set	noreorder
	mtc0	v0,C0_SR		# state unknown on reset
	mtc0	zero,C0_CAUSE		# clear software interrupts
	mtc0	zero,C0_PGMASK		# init tlb pgmask
	mtc0	zero,C0_WATCHLO         # clear watchpoint interrupt
	mtc0	zero,C0_WATCHHI

#ifdef BLOCKSIZE_8WORDS
	li	v0,CONFIG_IB|CONFIG_DB
#else
	move	v0,zero
#endif

#ifdef IP20_MC_VIRTUAL_DMA
	or	v0,CONFIG_COHRNT_EXLWR
#else
	or	v0,CONFIG_NONCOHRNT
#endif
	mtc0	v0,C0_CONFIG
	.set	reorder

#ifndef	ENETBOOT
	li	v0,PHYS_TO_K1(HPC_ENDIAN&~3)
	lw	v1,0(v0)		# note: this's a word access
	and	v1,v1,HPC_CPU_LITTLE

	/* If we're little endian, the bit should be 1, otherwise
	 * set to 1.  Opposite for big endian.
	 */
#ifdef	_MIPSEL
	bnez	v1,endian_ok
	li	v1,HPC_CPU_LITTLE
#else
	beqz	v1,endian_ok
	li	v1,0
#endif
	sw	v1,0(v0)

	/*
	 * according to james tornes, we don't need to reset the system
	 * after changing the endianness of the machine since MC and the R4000
	 * already know it
	 */

endian_ok:
#endif	/* ndef ENETBOOT */

	sw	zero,PHYS_TO_K1(CPU_ERR_STAT)	# clear CPU and GIO bus error
	sw	zero,PHYS_TO_K1(GIO_ERR_STAT)
	WBFLUSHM

#ifndef	PiE_EMULATOR
#ifdef	_MIPSEL
#define	CPUCTRL0_DEFAULT	0x300430f0
#else
#define	CPUCTRL0_DEFAULT	0x300030f0
#endif

#else	/* ndef PiE_EMULATOR */

#ifdef	_MIPSEL
#define	CPUCTRL0_DEFAULT	0x31d420f0
#else
#define	CPUCTRL0_DEFAULT	0x31d020f0
#endif
#endif	/* ndef PiE_EMULATOR */

#define	CPUCTRL0_REFS_4_LINES	0x00000002
#define	CPUCTRL0_REFS_8_LINES	0x00000004

	/* set up CPU control register 0, CPUCTRL0 */
	li	t0,PHYS_TO_K1(CPUCTRL0)
#ifdef	PiE_EMULATOR
	li	t1,CPUCTRL0_DEFAULT|CPUCTRL0_REFS_4_LINES
	li	t4,PHYS_TO_K1(CTRLD)	# refresh counter preload
	li	t5,128			# refresh 4 lines every 128 cycles
	sw	t5,0(t4)
#else
	/* set up the MUX_HWM according to the cache line size */
	.set	noreorder
	mfc0	a0,C0_CONFIG
	.set	reorder

	and	a1,a0,CONFIG_SC
	bne	a1,zero,1f		# no secondary cache

	and	a1,a0,CONFIG_SB
	srl	a1,CONFIG_SB_SHFT	# 0/1/2/3=4/8/16/32 words
	add	a1,4
	li	a2,1
	sll	a1,a2,a1		# bytes in cache line
	b	2f

	/* use primary data cache line if there's no secondary cache */
#ifdef	BLOCKSIZE_8WORDS
1:	li	a1,32
#else
1:	li	a1,16
#endif

2:	srl	a1,3			# divided by 8
	add	a1,4			# add 4
	sll	a1,20			# shift into correct position
	or	t1,a1,CPUCTRL0_DEFAULT|CPUCTRL0_REFS_8_LINES
#endif	/* PiE_EMULATOR */
	sw	t1,0(t0)
	WBFLUSHM

	/* set MC_HWM to 0x6, enable GIO bus time out */
	li	t0,PHYS_TO_K1(CPUCTRL1)
#ifdef	_MIPSEL
	li	t1,CPUCTRL1_ABORT_EN|CPUCTRL1_HPC_FX|CPUCTRL1_HPC_LITTLE|0x6
#else
	li	t1,CPUCTRL1_ABORT_EN|CPUCTRL1_HPC_FX|0x6
#endif
	sw	t1,0(t0)
	WBFLUSHM

#define	CPU_MEMACC_DEFAULT	0x11453433
#define	GIO_MEMACC_DEFAULT	0x34322

	/* set up CPU_MEMACC and GIO_MEMACC */
	li	t0,PHYS_TO_K1(CPU_MEMACC)
	li	t1,CPU_MEMACC_DEFAULT
	sw	t1,0(t0)

	li	t0,PHYS_TO_K1(GIO_MEMACC)
	li	t1,GIO_MEMACC_DEFAULT
	sw	t1,0(t0)

	j	flash_led
	END(start)

/*
 * notimplemented -- deal with calls to unimplemented prom services
 */
LEAF(notimplemented)
1:	b	1b
	END(notimplemented)

/*
 * boot exception vector handler
 *
 * this routine check fp for a current exception handler
 * if non-zero, jump to the appropriate handler else spin
 */
LEAF(bev_general)
XLEAF(bev_cacheerror)
XLEAF(bev_utlbmiss)
XLEAF(bev_xutlbmiss)
1:	b	1b
	END(bev_general)




#ifdef SIMULATE
#define	WAITCOUNT	1
#else	/* !SIMULATE */
#ifdef PiE_EMULATOR
#define	WAITCOUNT	10000
#else	/* !PiE_EMULATOR */
#define WAITCOUNT	200000
#endif	/* !PiE_EMULATOR */
#endif	/* !SIMULATE */

LEAF(flash_led)
        .set    noreorder

        /* just loop, flipping LED */
        /* Like this
         *
         * while (1) {
         *      *(unsigned char *)PHYS_TO_K1(CPU_AUX_CONTROL) |= CONSOLE_LED;
         *      delay();
         *      *(unsigned char *)PHYS_TO_K1(CPU_AUX_CONTROL) &= ~CONSOLE_LED;
         *      delay();
         * }
         */

         li     t0,PHYS_TO_K1(CPU_AUX_CONTROL)
         lbu    t1,0(t0)
         move   t3,a0

repeat:
         ori    t1,CONSOLE_LED  # led off
         sb     t1,0(t0)

         li     t2,WAITCOUNT    # delay
2:       bnez   t2,2b
         subu   t2,1

         and    t1,t1,~CONSOLE_LED      # led on
         sb     t1,0(t0)

         li     t2,WAITCOUNT    # delay
3:       bnez   t2,3b
         subu   t2,1

         b	repeat
         nop
	 /* NOTREACHED */

         j      ra
         nop

        .set    reorder
        END(flash_led)
