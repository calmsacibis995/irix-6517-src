/*
 * include file for the DS1687 real time clock
 */

#ifndef __SYS_DS1687CLK_H__
#define __SYS_DS1687CLK_H__

/**************************************************************************
 *									  *
 * 		 Copyright (C) 1990, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 1.6 $"

#include <sys/clock.h>		/* general clock definitions */

#define	RTC_BANK_0(x)		(x)
#define	RTC_BANK_1(x)		((x) | 0x80)
#define	RTC_BANK(x)		(((x) & 0x80) >> 3)	/* RTC_SELECT_BANK_1 */
#define	RTC_REG_NUM(x)		((x) & 0x7f)
#define	RTC_IS_SHARED_REG(x)	(RTC_REG_NUM(x) < 0x40)

#define	RTC_SEC			RTC_BANK_0(0x0)
#define	RTC_SEC_ALARM		RTC_BANK_0(0x1)
#define	RTC_MIN			RTC_BANK_0(0x2)
#define	RTC_MIN_ALARM		RTC_BANK_0(0x3)
#define	RTC_HOUR		RTC_BANK_0(0x4)
#define	RTC_HOUR_ALARM		RTC_BANK_0(0x5)
#define	RTC_DAY			RTC_BANK_0(0x6)
#define	RTC_DATE		RTC_BANK_0(0x7)
#define	RTC_MONTH		RTC_BANK_0(0x8)
#define	RTC_YEAR		RTC_BANK_0(0x9)
#define	RTC_CENTURY		RTC_BANK_1(0x48)
#define	RTC_DATE_ALARM		RTC_BANK_1(0x49)
#define	RTC_CTRL_A		RTC_BANK_0(0xa)
#define	RTC_CTRL_B		RTC_BANK_0(0xb)
#define	RTC_CTRL_C		RTC_BANK_0(0xc)
#define	RTC_CTRL_D		RTC_BANK_0(0xd)	
#define	RTC_X_CTRL_A		RTC_BANK_1(0x4a)
#define	RTC_X_CTRL_B		RTC_BANK_1(0x4b)
#define	RTC_X_RAM_ADDR		RTC_BANK_1(0x50)
#define	RTC_X_RAM_DATA		RTC_BANK_1(0x53)

/* bits in control A register */
#define	RTC_UPDATING			0x80
#define	RTC_RST_COUNTDOWN_CHAIN		0x40
#define	RTC_OSCILLATOR_ON		0x20
#define	RTC_SELECT_BANK_1		0x10
#define	RTC_OUTPUT_RATE_MSK		0x0f

/* bits in control B register */
#define	RTC_INHIBIT_UPDATE		0x80
#define	RTC_PERIODIC_INTR_ENABLE	0x40
#define	RTC_ALARM_INTR_ENABLE		0x20
#define	RTC_UPDATE_ENDED_INTR_ENABLE	0x10
#define	RTC_SQW_OUTPUT_ENABLE		0x08
#define	RTC_BINARY_DATA_MODE		0x04
#define	RTC_24HR_MODE			0x02
#define	RTC_DAYLIGHT_SAVING_MODE	0x01

/* bits in control C register */
#define	RTC_INTR			0x80
#define	RTC_PERIODIC_INTR		0x40
#define	RTC_ALARM_INTR			0x20
#define	RTC_UPDATE_ENDED_INTR		0x10

/* bits in control D register */
#define	RTC_BAT_OK			0x80

/* bits in extended control A register */
#define	RTC_AUX_BAT_OK			0x80
#define	RTC_INCR_IN_PROGRESS		0x40
#define	RTC_POWER_INACTIVE		0x08
#define	RTC_RAM_CLR_INTR		0x04
#define	RTC_WAKEUP_ALARM_INTR		0x02
#define	RTC_KICKSTART_INTR		0x01

/* bits in the extended control B register */
#define	RTC_AUX_BAT_ENABLE		0x80
#define	RTC_32K_SQW_ENABLE		0x40
#define	RTC_CRYSTAL_125_PF		0x20	/* use 12.5 pF crystal */
#define	RTC_RAM_CLR_ENABLE		0x10
#define	RTC_PWR_ACTIVE_ON_FAIL		0x08
#define	RTC_RAM_CLR_INTR_ENABLE		0x04
#define	RTC_WAKEUP_ALARM_INTR_ENABLE	0x02
#define	RTC_KICKSTART_INTR_ENABLE	0x01

#define	RTC_READ	0
#define	RTC_WRITE	1

/*
 * use these macros if the bank select bit in the control A register is
 * already set up to access the desired bank
 */
#if IP30 && LANGUAGE_C
#define	RD_DS1687_RTC(x)	(*RTC_ADDR_PORT = RTC_REG_NUM(x),	\
				 *RTC_DATA_PORT)
#define	WR_DS1687_RTC(x,v)	{*RTC_ADDR_PORT = RTC_REG_NUM(x);	\
				 *RTC_DATA_PORT = (v);}

/* dallas part access routines */
ioc3reg_t slow_ioc3_sio(void);
void restore_ioc3_sio(ioc3reg_t);
#endif	/* IP30 */

#endif /* __SYS_DS1687CLK_H__ */
