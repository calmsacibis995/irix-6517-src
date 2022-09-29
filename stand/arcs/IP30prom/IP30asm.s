#ident "$Revision: 1.19 $"

#include <asm.h>
#include <regdef.h>
#include <sys/cpu.h>
#include <sys/RACER/IP30nvram.h>
#include <sys/RACER/IP30addrs.h>
#include <sys/RACER/gda.h>
#include <sys/ds1687clk.h>
#include <sys/sbd.h>
#include <fault.h>

INITICACHE_LOCALSZ = 1			# save ra only
INITICACHE_FRAMESZ =(((NARGSAVE + INITICACHE_LOCALSZ) * SZREG) + ALSZ) & ALMASK
INITICACHE_RAOFF = INITICACHE_FRAMESZ - (1 * SZREG)
NESTED(init_icache, INITICACHE_FRAMESZ, zero)
	PTR_SUBU	sp,INITICACHE_FRAMESZ
	sd	ra,INITICACHE_RAOFF(sp)

	jal	iCacheSize

	dsrl	v1,v0,1			# 2 ways
	dli	v0,K0_RAMBASE
	PTR_ADDU	v1,v0

	.set	noreorder
	mtc0	zero,C0_TAGLO
	mtc0	zero,C0_TAGHI
	mtc0	zero,C0_ECC
1:	
	PTR_ADDU	v0,CACHE_ILINE_SIZE
	cache	CACH_PI|C_ISD,-64(v0)	# initialize data array
	cache	CACH_PI|C_ISD,-63(v0)
	cache	CACH_PI|C_ISD,-60(v0)
	cache	CACH_PI|C_ISD,-59(v0)
	cache	CACH_PI|C_ISD,-56(v0)
	cache	CACH_PI|C_ISD,-55(v0)
	cache	CACH_PI|C_ISD,-52(v0)
	cache	CACH_PI|C_ISD,-51(v0)
	cache	CACH_PI|C_ISD,-48(v0)
	cache	CACH_PI|C_ISD,-47(v0)
	cache	CACH_PI|C_ISD,-44(v0)
	cache	CACH_PI|C_ISD,-43(v0)
	cache	CACH_PI|C_ISD,-40(v0)
	cache	CACH_PI|C_ISD,-39(v0)
	cache	CACH_PI|C_ISD,-36(v0)
	cache	CACH_PI|C_ISD,-35(v0)
	cache	CACH_PI|C_ISD,-32(v0)
	cache	CACH_PI|C_ISD,-31(v0)
	cache	CACH_PI|C_ISD,-28(v0)
	cache	CACH_PI|C_ISD,-27(v0)
	cache	CACH_PI|C_ISD,-24(v0)
	cache	CACH_PI|C_ISD,-23(v0)
	cache	CACH_PI|C_ISD,-20(v0)
	cache	CACH_PI|C_ISD,-19(v0)
	cache	CACH_PI|C_ISD,-16(v0)
	cache	CACH_PI|C_ISD,-15(v0)
	cache	CACH_PI|C_ISD,-12(v0)
	cache	CACH_PI|C_ISD,-11(v0)
	cache	CACH_PI|C_ISD,-8(v0)
	cache	CACH_PI|C_ISD,-7(v0)
	cache	CACH_PI|C_ISD,-4(v0)
	bltu	v0,v1,1b
	cache	CACH_PI|C_ISD,-3(v0)
	.set	reorder

	ld	ra,INITICACHE_RAOFF(sp)
	PTR_ADDU	sp,INITICACHE_FRAMESZ

	j	ra
	END(init_icache)

INITDCACHE_LOCALSZ = 1			# save ra only
INITDCACHE_FRAMESZ =(((NARGSAVE + INITDCACHE_LOCALSZ) * SZREG) + ALSZ) & ALMASK
INITDCACHE_RAOFF = INITDCACHE_FRAMESZ - (1 * SZREG)
NESTED(init_dcache, INITDCACHE_FRAMESZ, zero)
	PTR_SUBU	sp,INITDCACHE_FRAMESZ
	sd	ra,INITDCACHE_RAOFF(sp)

	jal	dCacheSize

	dsrl	v1,v0,1			# 2 ways
	dli	v0,K0_RAMBASE
	PTR_ADDU	v1,v0

	.set	noreorder
	mtc0	zero,C0_TAGLO
	mtc0	zero,C0_TAGHI
	mtc0	zero,C0_ECC
1:	
	PTR_ADDU	v0,CACHE_DLINE_SIZE
	cache	CACH_PD|C_ISD,-32(v0)	# initialize data array
	cache	CACH_PD|C_ISD,-31(v0)
	cache	CACH_PD|C_ISD,-28(v0)
	cache	CACH_PD|C_ISD,-27(v0)
	cache	CACH_PD|C_ISD,-24(v0)
	cache	CACH_PD|C_ISD,-23(v0)
	cache	CACH_PD|C_ISD,-20(v0)
	cache	CACH_PD|C_ISD,-19(v0)
	cache	CACH_PD|C_ISD,-16(v0)
	cache	CACH_PD|C_ISD,-15(v0)
	cache	CACH_PD|C_ISD,-12(v0)
	cache	CACH_PD|C_ISD,-11(v0)
	cache	CACH_PD|C_ISD,-8(v0)
	cache	CACH_PD|C_ISD,-7(v0)
	cache	CACH_PD|C_ISD,-4(v0)
	bltu	v0,v1,1b
	cache	CACH_PD|C_ISD,-3(v0)
	.set	reorder

	ld	ra,INITDCACHE_RAOFF(sp)
	PTR_ADDU	sp,INITDCACHE_FRAMESZ

	j	ra
	END(init_dcache)

INITSCACHE_LOCALSZ = 1			# save ra only
INITSCACHE_FRAMESZ =(((NARGSAVE + INITSCACHE_LOCALSZ) * SZREG) + ALSZ) & ALMASK
INITSCACHE_RAOFF = INITSCACHE_FRAMESZ - (1 * SZREG)
NESTED(init_scache, INITSCACHE_FRAMESZ, zero)
	PTR_SUBU	sp,INITSCACHE_FRAMESZ
	sd	ra,INITSCACHE_RAOFF(sp)

	jal	sCacheSize

	dsrl	v1,v0,1			# 2 ways
	dli	v0,K0_RAMBASE
	PTR_ADDU	v1,v0

	.set	noreorder
	mtc0	zero,C0_TAGLO
	mtc0	zero,C0_TAGHI
	mtc0	zero,C0_ECC
1:	
	PTR_ADDU	v0,CACHE_SLINE_SIZE
#if (CACHE_SLINE_SIZE == 128)
	cache	CACH_SD|C_ISD,-128(v0)
	cache	CACH_SD|C_ISD,-127(v0)
	cache	CACH_SD|C_ISD,-112(v0)
	cache	CACH_SD|C_ISD,-111(v0)
	cache	CACH_SD|C_ISD,-96(v0)
	cache	CACH_SD|C_ISD,-95(v0)
	cache	CACH_SD|C_ISD,-80(v0)
	cache	CACH_SD|C_ISD,-79(v0)
#endif	/* CACHE_SLINE_SIZE == 128 */
	cache	CACH_SD|C_ISD,-64(v0)	# initialize date array
	cache	CACH_SD|C_ISD,-63(v0)
	cache	CACH_SD|C_ISD,-48(v0)
	cache	CACH_SD|C_ISD,-47(v0)
	cache	CACH_SD|C_ISD,-32(v0)
	cache	CACH_SD|C_ISD,-31(v0)
	cache	CACH_SD|C_ISD,-16(v0)
	bltu	v0,v1,1b
	cache	CACH_SD|C_ISD,-15(v0)
	.set	reorder

	ld	ra,INITSCACHE_RAOFF(sp)
	PTR_ADDU	sp,INITSCACHE_FRAMESZ

	j	ra
	END(init_scache)

/* Invalidate all tlb entries */
LEAF(init_tlb)
	.set	noreorder
	dmtc0	zero,C0_TLBLO_0
	dmtc0	zero,C0_TLBLO_1
	move	v1,zero			# C0_TLBHI

	li	v0,TLBPGMASK_MASK
	mtc0	v0,C0_PGMASK
	mtc0	zero,C0_FMMASK

	li	a0,_PAGESZ*2		# VPN increment
	li	v0,NTLBENTRIES-1
1:
	dmtc0	v1,C0_TLBHI
	mtc0	v0,C0_INX
	nop
	c0	C0_WRITEI
	daddu	v1,a0			# next VPN

	bne	v0,zero,1b
	sub	v0,1

	j	ra
	nop                             # BDSLOT
	.set	reorder
	END(init_tlb)

LEAF(pon_nmi_handler)
	.set	noat
	move	k1,zero
	b	1f
XLEAF(pon_nmi_slave_handler)
	li	k1,0x2222
1:
	LI	k0,PHYS_TO_K1(IP30_EARLY_NMI)
	REG_S	AT,R_AT*SZREG(k0)
	.set	at
	REG_S	v0,R_V0*SZREG(k0)
	dmfc1	v0,$f1				# restore k1
	REG_S	v0,R_K1*SZREG(k0)
	REG_S	v1,R_V1*SZREG(k0)
	REG_S	a0,R_A0*SZREG(k0)
	REG_S	a1,R_A1*SZREG(k0)
	REG_S	a2,R_A2*SZREG(k0)
	REG_S	a3,R_A3*SZREG(k0)
	REG_S	a4,R_A4*SZREG(k0)
	REG_S	a5,R_A5*SZREG(k0)
	REG_S	a6,R_A6*SZREG(k0)
	REG_S	a7,R_A7*SZREG(k0)
	REG_S	t0,R_T0*SZREG(k0)
	REG_S	t1,R_T1*SZREG(k0)
	REG_S	t2,R_T2*SZREG(k0)
	REG_S	t3,R_T3*SZREG(k0)
	REG_S	s0,R_S0*SZREG(k0)
	REG_S	s1,R_S1*SZREG(k0)
	REG_S	s2,R_S2*SZREG(k0)
	REG_S	s3,R_S3*SZREG(k0)
	REG_S	s4,R_S4*SZREG(k0)
	REG_S	s5,R_S5*SZREG(k0)
	REG_S	s6,R_S6*SZREG(k0)
	REG_S	s7,R_S7*SZREG(k0)
	REG_S	t8,R_T8*SZREG(k0)
	REG_S	t9,R_T9*SZREG(k0)
	REG_S	gp,R_GP*SZREG(k0)
	REG_S	sp,R_SP*SZREG(k0)
	REG_S	s8,R_S8*SZREG(k0)
	REG_S	ra,R_RA*SZREG(k0)

#define PUTS(label)			\
	LA	a0,label;		\
	jal	pon_puts
#define PUTREG(label,value)		\
	PUTS(label);			\
	REG_L	a0,value*SZREG(k0);	\
	jal	_pon_puthex64
#define PUTSTACK(offset)		\
	ld	a0,offset(sp);		\
	jal	pon_puthex64;		\
	PUTS(blank)

/*
Exception: <vector=NMI> count=xxxxxxxx 
Registers CPU 0
a0=................ a1=................ a2=................ a3=................
a4=................ a5=................ a6=................ a7=................
t0=................ t1=................ t2=................ t3=................
s0=................ s1=................ s2=................ s3=................
s4=................ s5=................ s6=................ s7=................
s8=................ t8=................ t9=................ gp=................
ra=................ AT=................ v0=................ v1=................
k0=....trashed....  k1=................

Data @ sp=................
0x................ 0x................ 0x................ 0x................
0x................ 0x................ 0x................ 0x................
0x................ 0x................ 0x................ 0x................
0x................ 0x................ 0x................ 0x................

<pon_handler output>

Registers CPU 1	[on MP]
....
*/

/*
 * RPROM also call this fct to dump the registers, extra info
 * has been placed in fa0 (EXCEPT type), fa1 (C0_CAUSE), fa2 (C0_EPC)
 * for the pon_handler
 */
#ifdef IP30_RPROM
	mfc1	a0,fa0
	bnez	a0,2f
#endif /* IP30_RPROM */

	/* pon io does not touch k0 */
	bnez	k1,1f
	PUTS(nmi_exception_msg)
	LI	a0,GDA_ADDR
	ld	a0,G_COUNT(a0)
	jal	pon_puthex

1:	PUTS(nmi_msg)
	ld	a0,PHYS_TO_COMPATK1(HEART_PRID)
	addi	a0,0x30			# '0'
	jal	pon_putc
2:	PUTS(crlf)

	PUTREG(a0_msg,R_A0)
	PUTREG(a1_msg,R_A1)
	PUTREG(a2_msg,R_A2)
	PUTREG(a3_msg,R_A3)
	PUTREG(a4_msg,R_A0)
	PUTREG(a5_msg,R_A1)
	PUTREG(a6_msg,R_A2)
	PUTREG(a7_msg,R_A3)
	PUTREG(t0_msg,R_T0)
	PUTREG(t1_msg,R_T1)
	PUTREG(t2_msg,R_T2)
	PUTREG(t3_msg,R_T3)
	PUTREG(s0_msg,R_S0)
	PUTREG(s1_msg,R_S1)
	PUTREG(s2_msg,R_S2)
	PUTREG(s3_msg,R_S3)
	PUTREG(s4_msg,R_S4)
	PUTREG(s5_msg,R_S5)
	PUTREG(s6_msg,R_S6)
	PUTREG(s7_msg,R_S7)
	PUTREG(s8_msg,R_S8)
	PUTREG(t8_msg,R_T8)
	PUTREG(t9_msg,R_T9)
	PUTREG(gp_msg,R_GP)
	PUTREG(ra_msg,R_RA)
	PUTREG(AT_msg,R_AT)
	PUTREG(v0_msg,R_V0)
	PUTREG(v1_msg,R_V1)
	PUTS(k0_msg)
	PUTREG(k1_msg,R_K1)

	PUTREG(sp_msg,R_SP)
	PUTS(crlf)
	LI	a0,(1<<63)		# does it look like a kernel address?
	REG_L	sp,R_SP*SZREG(k0)
	and	a0,a0,sp
	beqz	a0,1f

	PUTSTACK(0x00); PUTSTACK(0x08); PUTSTACK(0x10); PUTSTACK(0x18)
	PUTS(crlf)
	PUTSTACK(0x20); PUTSTACK(0x28); PUTSTACK(0x30); PUTSTACK(0x38)
	PUTS(crlf)
	PUTSTACK(0x30); PUTSTACK(0x38); PUTSTACK(0x40); PUTSTACK(0x48)
	PUTS(crlf)
	PUTSTACK(0x50); PUTSTACK(0x58); PUTSTACK(0x60); PUTSTACK(0x68);
	PUTS(crlf)

	/* Hook to check for specific diag mode to clear the CPU mask */
	jal	check_clear_cpumask
1:
	/* dump rest of the information and hook up soft power */
	REG_L	ra,R_RA*SZREG(k0)	# restore ra so it is correct in dump

#ifdef IP30_RPROM
/*
 * Called by RPROM system exception handlers,
 * restore the cause, EPC, and EXCEPT_X type from fp regs
 * and let pon_handler print them
 */
	.set	noreorder
	mfc1	a0,fa0
	beqz	a0,1f
	nop
	mfc1	a1,fa1
	dmfc1	a2,fa2
	.set	reorder
	j	pon_handler
1:
#endif /* IP30_RPROM */

	li	a0,99
	j	pon_handler
	END(pon_nmi_handler)

#if (NVOFF_CPUDISABLE+1) > 114
#error NVOFF_CPUMASK dependancy broken!
#endif

/* If NMI 5 times in a row w/o a reset then clear cpumask/bootmaster */
LEAF(check_clear_cpumask)
	LI	a0,GDA_ADDR				# read count in GDA
	ld	a0,G_COUNT(a0)
	addi	a0,-5					# 5x?
	bnez	a0,9f					# skip it if not.

	LI	t3,IOC3_PCI_DEVIO_K1PTR|IOC3_SIO_CR;	# slow dallas mode
	lw	a2,0(t3)				# read value
	and	a1,a2,~SIO_SR_CMD_PULSE			# tweak it
	or	a1,0x8 << SIO_CR_CMD_PULSE_SHIFT;
	sw	a1,0(t3)				# write back
	lw	zero,0(t3)				# flushbus
	lw	zero,0(t3)				# flushbus
	LI	t0,RTC_ADDR_PORT			# clock address
	LI	t1,RTC_DATA_PORT			# clock data
	/* select bank 0 as diagmode and cpumask are there */
	li	a0,RTC_REG_NUM(RTC_CTRL_A)
	sb	a0,0(t0)
	li	a1,RTC_OSCILLATOR_ON
	sb	a1,0(t1)
	/* read byte 1 of cpu disable */
	li	a0,RTC_REG_NUM(NVRAM_REG_OFFSET+NVOFF_CPUDISABLE+1)
	sb	a0,0(t0)
	lbu	a1,0(t1)
	ori	a1,CPU_DISABLE_INVALID			# disable checking.
	sb	a0,0(t0)
	sb	a1,0(t1)

	/* latch scratch register to redirect any power down noise */
	li	a0,RTC_REG_NUM(NVRAM_REG_OFFSET)
	sb	a0,0(t0)
	lb	zero,0(t1)
	sw	a2,0(t3)				# return IOC3_SIO_CR
	lw	zero,0(t3)				# flushbus
	lw	zero,0(t3)				# flushbus
9:	j	ra
	END(check_clear_cpumask)

	.data
crlf:
	.asciiz	"\r\n"
nmi_exception_msg:
#ifdef IP30_RPROM
	.asciiz "\r\nRPROM Exception: <vector=NMI> count="
#else
	.asciiz "\r\nException: <vector=NMI> count="
#endif /* IP30_RPROM */
nmi_msg:
	.asciiz "\r\nRegisters CPU "
sp_msg:
	.asciiz "\r\nData @ sp="
a0_msg:
	.asciiz "a0="
a1_msg:
	.asciiz " a1="
a2_msg:
	.asciiz " a2="
a3_msg:
	.asciiz " a3="
a4_msg:
	.asciiz "\r\na4="
a5_msg:
	.asciiz " a5="
a6_msg:
	.asciiz " a6="
a7_msg:
	.asciiz " a7="
t0_msg:
	.asciiz "\r\nt0="
t1_msg:
	.asciiz " t1="
t2_msg:
	.asciiz " t2="
t3_msg:
	.asciiz " t3="
s0_msg:
	.asciiz "\r\ns0="
s1_msg:
	.asciiz " s1="
s2_msg:
	.asciiz " s2="
s3_msg:
	.asciiz " s3="
s4_msg:
	.asciiz "\r\ns4="
s5_msg:
	.asciiz " s5="
s6_msg:
	.asciiz " s6="
s7_msg:
	.asciiz " s7="
s8_msg:
	.asciiz "\r\ns8="
t8_msg:
	.asciiz " t8="
t9_msg:
	.asciiz " t9="
gp_msg:
	.asciiz " gp="
ra_msg:
	.asciiz "\r\nra="
AT_msg:
	.asciiz " AT="
v0_msg:
	.asciiz " v0="
v1_msg:
	.asciiz " v1="
k0_msg:
	.asciiz "\r\nk0=....trashed.... "
k1_msg:
	.asciiz " k1="
blank:
	.asciiz " "
