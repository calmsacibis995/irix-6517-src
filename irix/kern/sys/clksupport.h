/**************************************************************************
 *									  *
 * 		 Copyright (C) 1995, Silicon Graphics, Inc.	          *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"$Revision: 1.23 $"
#ifndef  _KSYS_CLKSUPPORT_H
#define _KSYS_CLKSUPPORT_H

#include <sys/mips_addrspace.h>

#if EVEREST
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/epc.h>	/* for rtodc/wtodc */
typedef evreg_t clkreg_t; 
#define GET_LOCAL_RTC EV_GET_LOCAL_RTC
#define DISABLE_TMO_INTR() EV_SETMYCONFIG_REG(EV_CMPREG0, 0)
#define CLK_FCLOCK_FAST_FREQ NVR_INTR_FAST_RATE
#define CLK_FCLOCK_SLOW_FREQ NVR_INTR_SLOW_RATE
/* The is the address that the user will use to mmap the cycle counter */
#define CLK_CYCLE_ADDRESS_FOR_USER EV_RTC
#endif

#if SN
#include <sys/SN/agent.h>
#include <sys/SN/intr_public.h>
typedef hubreg_t clkreg_t;
extern nasid_t master_nasid;

#define GET_LOCAL_RTC		(clkreg_t)LOCAL_HUB_L(PI_RT_COUNT)
#define DISABLE_TMO_INTR()	if  (cputoslice(cpuid()) == 0) \
					LOCAL_HUB_S(PI_RT_COMPARE_A, 0); \
				else \
					LOCAL_HUB_S(PI_RT_COMPARE_B, 0)
/* This is a hack; we really need to figure these values out dynamically */
/* 
 * Since 800 ns works very well with various HUB frequencies, such as
 * 360, 380, 390 and 400 MHZ, we use 800 ns rtc cycle time.
 */
#define NSEC_PER_CYCLE		800
#define CYCLE_PER_SEC		(NSEC_PER_SEC/NSEC_PER_CYCLE)
/*
 * Number of cycles per profiling intr 
 */
#define CLK_FCLOCK_FAST_FREQ	1250
#define CLK_FCLOCK_SLOW_FREQ	0
/* The is the address that the user will use to mmap the cycle counter */
#define CLK_CYCLE_ADDRESS_FOR_USER LOCAL_HUB_ADDR(PI_RT_COUNT)

#elif IP30
#include <sys/cpu.h>
typedef heartreg_t clkreg_t;
#define NSEC_PER_CYCLE		80
#define CYCLE_PER_SEC		(NSEC_PER_SEC/NSEC_PER_CYCLE)
#define GET_LOCAL_RTC	*((volatile clkreg_t *)PHYS_TO_COMPATK1(HEART_COUNT))
#define DISABLE_TMO_INTR()
#define CLK_CYCLE_ADDRESS_FOR_USER PHYS_TO_K1(HEART_COUNT)
#define CLK_FCLOCK_SLOW_FREQ (CYCLE_PER_SEC / HZ)
#endif

/* Prototypes */
extern void init_timebase(void);
extern void fastick_maint(struct eframe_s *);
extern int audioclock;
extern int prfclk_enabled_cnt;
#endif  /* _KSYS_CLKSUPPORT_H */
