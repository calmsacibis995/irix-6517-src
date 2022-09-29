/**************************************************************************
 *									  *
 * 		 Copyright (C) 1994 Silicon Graphics, Inc.	  	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.35 $"

#ifndef	_HUB_H_
#define	_HUB_H_

#include <sys/SN/agent.h>
#if defined(_LANGUAGE_C)
#include <sys/SN/slotnum.h>
#endif
#include <sys/SN/SN0/ip27config.h>
#include <rtc.h>

#define HUB_FLAG_GMASTER	IIO_SCRATCH_BIT0_0
#define HUB_FLAG_IO_DISABLE	IIO_SCRATCH_BIT0_1
#define HUB_FLAG_HAVE_IOC3	IIO_SCRATCH_BIT0_2
#define HUB_FLAG_HAVE_ELSC	IIO_SCRATCH_BIT0_3
#define HUB_FLAG_ALIVE_A	IIO_SCRATCH_BIT0_4
#define HUB_FLAG_ALIVE_B	IIO_SCRATCH_BIT0_5
#define HUB_FLAG_DISPLAY_TAKEN	IIO_SCRATCH_BIT0_6
#define HUB_FLAG_PMASTER	IIO_SCRATCH_BIT0_7

#ifdef _LANGUAGE_C

#define HUB_FLAG_SET(flag, state)				\
	SD(LOCAL_HUB(IIO_SCRATCH_REG0), (state) ?		\
	   LD(LOCAL_HUB(IIO_SCRATCH_REG0)) | (flag) :		\
	   LD(LOCAL_HUB(IIO_SCRATCH_REG0)) & ~(flag))

#define HUB_FLAG_GET(flag)					\
	((LD(LOCAL_HUB(IIO_SCRATCH_REG0)) & (flag)) != 0)

#endif /* _LANGUAGE_C */

#define HUB_LOCK_PRINT		10
#define HUB_LOCK_FPROM		12
#define HUB_LOCK_I2C		15
#define HUB_LOCK_JUNKUART	16
#define HUB_LOCK_IOC3UART	17
#define HUB_LOCK_NIC		18

#if _LANGUAGE_ASSEMBLY

#define HUB_CPU_GET()						\
	dli	v0, LOCAL_HUB(0);				\
	ld	v0, PI_CPU_NUM(v0);

/*
 * The following is used in entry.s and only uses k0 and k1.
 */

#define HUB_LED_SET(leds)					\
	dli	k1, IP27CONFIG_SN00_ADDR;			\
	lw	k1, (k1);					\
	dli	k0, LOCAL_HUB(PI_CPU_NUM);			\
	bnez	k1, 98f;					\
	 nop;							\
	ld	k1, (k0);					\
	dli	k0, LOCAL_HUB(MD_LED0);				\
	b	99f;						\
	 dsll	k1, 3;						\
98:	/* Sn00 LED code */					\
	ld	k1, (k0);					\
	dli	k0, LOCAL_HUB(MD_UREG1_0);			\
	dsll	k1, 5;						\
99:	daddu	k0, k1;						\
	or	k1, zero, leds;					\
	sd	k1, (k0)

#define HUB_LED_SET_REMOTE(nasid, cpu, leds)			\
	dli	k0, IP27CONFIG_SN00_ADDR_NODE(0);		\
	or	k1, zero, nasid;				\
	dsll	k1, NASID_SHFT;					\
	daddu	k0, k1;						\
	lw	k0, (k0);					\
	bnez	k0, 98f;					\
	 nop;							\
	dli	k0, REMOTE_HUB(0, MD_LED0);			\
	daddu	k0, k1;						\
	or	k1, zero, cpu;					\
	b	99f;						\
	 dsll	k1, 3;						\
98:	/* Sn00 LED code */					\
	dli	k0, REMOTE_HUB(0, MD_UREG1_4);			\
	daddu	k0, k1;						\
	or	k1, zero, cpu;					\
	dsll	k1, 5;						\
99:	daddu	k0, k1;						\
	or	k1, zero, leds;					\
	sd	k1, (k0)

#define HUB_LED_FLASH(leds)					\
	or	s0, zero, leds;					\
99:	HUB_LED_SET(s0);					\
	li	a0, 500000;					\
	jal	rtc_sleep;					\
	 nop;							\
	HUB_LED_SET(0);						\
	li	a0, 500000;					\
	jal	rtc_sleep;					\
	 nop;							\
	b	99b;						\
	 nop

#define HUB_LED_PAUSE(leds)					\
	HUB_LED_SET(leds);					\
	dli	a0, 1000000;					\
	jal	rtc_sleep;					\
	 nop

#define HUB_TRIGGER()						\
	dli	v0, LOCAL_HUB(PI_CPU_NUM);			\
	dli	v1, 0x11111111;					\
	sd	v1, (v0)			/* For logic analyzer */

#endif /* _LANGUAGE_ASSEMBLY */

#if _LANGUAGE_C

int	hub_cpu_get(void);
int	hub_kego(void);			/* Returns TRUE if KEGO */
void	hub_halt(void);
void	hub_halt_remote(nasid_t nasid, int cpu);

int	hub_num_local_cpus(void);
int	hub_local_master(void);
int	hub_local_slave(void);

#define hub_global_master_set(gmaster)				\
	HUB_FLAG_SET(HUB_FLAG_GMASTER, (gmaster))
#define hub_global_master()					\
	(hub_local_master() && HUB_FLAG_GET(HUB_FLAG_GMASTER))

#define hub_partition_master_set(pmaster)			\
	HUB_FLAG_SET(HUB_FLAG_PMASTER, (pmaster))
#define hub_partition_master()					\
	((get_BSR() & BSR_PART) ? (hub_local_master() && 	\
	HUB_FLAG_GET(HUB_FLAG_PMASTER)) : (hub_global_master()))

#define hub_alive_set(cpu, alive)				\
	HUB_FLAG_SET((cpu) ? HUB_FLAG_ALIVE_B :			\
		     HUB_FLAG_ALIVE_A, (alive))
#define hub_alive(cpu)						\
	HUB_FLAG_GET((cpu) ? HUB_FLAG_ALIVE_B :			\
		     HUB_FLAG_ALIVE_A)

#define hub_io_disable_set(io_disable)				\
	HUB_FLAG_SET(HUB_FLAG_IO_DISABLE, (io_disable))
#define hub_io_disable()					\
	HUB_FLAG_GET(HUB_FLAG_IO_DISABLE)

#define hub_have_ioc3_set(have_ioc3)				\
	HUB_FLAG_SET(HUB_FLAG_HAVE_IOC3, (have_ioc3))
#define hub_have_ioc3()						\
	HUB_FLAG_GET(HUB_FLAG_HAVE_IOC3)

#define hub_have_elsc_set(have_elsc)				\
	HUB_FLAG_SET(HUB_FLAG_HAVE_ELSC, (have_elsc))
#define hub_have_elsc()						\
	HUB_FLAG_GET(HUB_FLAG_HAVE_ELSC)

#define hub_set_display_taken()				    	\
	HUB_FLAG_SET(HUB_FLAG_DISPLAY_TAKEN,1)
#define hub_display_taken()						\
	HUB_FLAG_GET(HUB_FLAG_DISPLAY_TAKEN)

#define	hub_reset_fence_set(nasid)				\
	REMOTE_HUB_S(nasid, NI_PROTECTION, 0)
#define	hub_reset_fence_clear(nasid)				\
	REMOTE_HUB_S(nasid, NI_PROTECTION, 1)

void 	hub_display_digits(int slot, __uint64_t val);
void	hub_led_elsc_set(__uint64_t val);

 void	hub_led_set(__uint64_t leds);
void	hub_led_set_remote(nasid_t nasid, int cpu, __uint64_t leds);
void	hub_led_show(__uint64_t leds);
void	hub_led_flash(__uint64_t leds);
void	hub_led_pause(__uint64_t leds);

#define hub_trigger()						\
	SD(LOCAL_HUB(PI_CPU_NUM), 0x11111111)	/* For logic analyzer */

#endif /* _LANGUAGE_C */

#if _LANGUAGE_ASSEMBLY

/*
 * Defines: HUB_INT_SET         : Sets INT_PEND0/1 bit 'intnum'
 *          HUB_INT_SET_REMOTE  : Sets INT_PEND0/1 bit 'intnum' in remote hub
 *          HUB_INT_CLEAR       : Clears INT_PEND0/1 bit 'intnum'
 *          HUB_INT_CLEAR_REMOTE: Clears INT_PEND0/1 bit 'intnum' in remote hub
 *          HUB_INT_TEST        : Sets v0 to value of INT_PEND0/1 bit 'intnum'
 *          HUB_INT_TEST_REMOTE : Like HUB_INT_TEST but for remote node
 *
 * Parameters: intnum must be a register or constant in the range 0 to 122.
 *             intnum0 must be a register or constant in the range 0 to 63.
 * Destroys: v0-v1
 */

#define HUB_INT_SET(intnum)					\
	or	v0, zero, intnum;				\
	or	v0, 0x100;					\
	dli	v1, LOCAL_HUB(0);				\
	sd	v0, PI_INT_PEND_MOD(v1)

#define HUB_INT_SET_REMOTE(nasid, intnum)			\
	or	v0, zero, intnum;				\
	or	v0, 0x100;					\
	or	v1, zero, nasid;				\
	dsll	v1, NASID_SHFT;					\
	daddu	v1, IO_BASE + (1 << SWIN_SIZE_BITS) + 0x800000; \
	sd	v0, PI_INT_PEND_MOD(v1)

#define HUB_INT_CLEAR(intnum)					\
	or	v0, zero, intnum;				\
	dli	v1, LOCAL_HUB(0);				\
	sd	v0, PI_INT_PEND_MOD(v1)

#define HUB_INT_CLEAR_REMOTE(nasid, intnum)			\
	or	v0, zero, intnum;				\
	or	v1, zero, nasid;				\
	dsll	v1, NASID_SHFT;					\
	daddu	v1, IO_BASE + (1 << SWIN_SIZE_BITS) + 0x800000; \
	sd	v0, PI_INT_PEND_MOD(v1)

#define HUB_INT_TEST(intnum)					\
	dli	v0, LOCAL_HUB(0);				\
	or	v1, zero, intnum;				\
	bge	v1, 64, 91f;					\
	 nop;							\
	b	92f;						\
	 ld	v0, PI_INT_PEND0(v0);				\
91:	ld	v0, PI_INT_PEND1(v0);				\
	sub	v1, 64;						\
92:	dsrl	v0, v1;						\
	and	v0, 1

#define HUB_INT_TEST_REMOTE(nasid, intnum)			\
	or	v1, zero, intnum;				\
	or	v0, zero, nasid;				\
	dsll	v0, NASID_SHFT;					\
	daddu	v0, IO_BASE + (1 << SWIN_SIZE_BITS) + 0x800000; \
	bge	v1, 64, 91f;					\
	 nop;							\
	b	92f;						\
	 ld	v0, PI_INT_PEND0(v0);				\
91:	ld	v0, PI_INT_PEND1(v0);				\
	sub	v1, 64;						\
92:	dsrl	v0, v1;						\
	and	v0, 1

#endif /* _LANGUAGE_ASSEMBLY */

#if _LANGUAGE_C

#define hub_int_set(intnum)					\
	SD(LOCAL_HUB(PI_INT_PEND_MOD), 0x100 | (intnum))

#define hub_int_set_remote(nasid, intnum)			\
	SD(REMOTE_HUB(nasid, PI_INT_PEND_MOD), 0x100 | (intnum))

#define hub_int_clear(intnum)					\
	SD(LOCAL_HUB(PI_INT_PEND_MOD), (intnum))

#define hub_int_clear_remote(nasid, intnum)			\
	SD(REMOTE_HUB(nasid, PI_INT_PEND_MOD), (intnum))

#define hub_int_test(intnum)					\
	((intnum) < 64 ?					\
	 LD(LOCAL_HUB(PI_INT_PEND0)) >> (intnum)      & 1 :	\
	 LD(LOCAL_HUB(PI_INT_PEND1)) >> (intnum) - 64 & 1)

#define hub_int_test_remote(nasid, intnum)			\
	((intnum) < 64 ?					\
	 LD(REMOTE_HUB(nasid, PI_INT_PEND0)) >> (intnum)      & 1 : \
	 LD(REMOTE_HUB(nasid, PI_INT_PEND1)) >> (intnum) - 64 & 1)

void	hub_int_set_all(__uint64_t mask1, __uint64_t mask0);
void	hub_int_get_all(__uint64_t *mask1, __uint64_t *mask0);

void	hub_int_mask_out(int intnum);
void	hub_int_mask_in(int intnum);

void	hub_int_set_mask(__uint64_t mask1, __uint64_t mask0);
void	hub_int_get_mask(__uint64_t *mask1, __uint64_t *mask0);

int	hub_int_diag(void);
int	intrpt_diag(int diag_mode);

#endif /* _LANGUAGE_C */

#if _LANGUAGE_C

#ifdef RTL
#define HUB_REGS_PI	0x01
#define HUB_REGS_MD	0x02
#define HUB_REGS_NI	0x04
#define HUB_REGS_II	0x08
#define HUB_REGS_CORE	0x10
#define HUB_REGS_ALL	0xff

void	hub_init_regs(int mask);
void	hub_access_regs(int mask);
#endif

void	hub_barrier(void);

int	hub_lock_timeout(int level, rtc_time_t timeout);
void	hub_lock(int level);
void	hub_unlock(int level);

int	hub_nic_get(nasid_t nasid, int verbose, __uint64_t *nicp);

#endif /* _LANGUAGE_C */

#endif /*_HUB_H_ */
