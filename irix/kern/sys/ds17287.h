#ifndef __SYS_DS17287_H__
#define __SYS_DS17287_H__

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
#ident "$Revision: 1.3 $"


#define DS_REG_STRIDE 1
#define DS_OFFSET(b,o) (b+(o)*DS_REG_STRIDE)

#ifdef IP32
#undef  DS_REG_STRIDE
#define DS_REG_STRIDE 256
#endif

#if defined(_LANGUAGE_C)

#define DS_ALIGN(n) volatile unsigned char n;

#ifdef IP32
#undef DS_ALIGN
#define DS_ALIGN(n)\
   volatile unsigned char n;\
   unsigned char n##_pad[DS_REG_STRIDE-1]
#endif



struct ds17287_clk {
	DS_ALIGN( sec );	/* seconds register       */
	DS_ALIGN( sec_alarm );	/* seconds alarm register */
	DS_ALIGN( min );	/* minutes register       */
	DS_ALIGN( min_alarm );	/* minutes alarm register */
	DS_ALIGN( hour );   	/* hour register          */
	DS_ALIGN( hour_alarm );	/* hour alarm register    */
	DS_ALIGN( day );   	/* day register           */
	DS_ALIGN( date );   	/* date register          */
	DS_ALIGN( month );   	/* month register         */
	DS_ALIGN( year );   	/* year register          */
	DS_ALIGN( registera );	/* command register a     */
	DS_ALIGN( registerb );	/* command register b     */
	DS_ALIGN( registerc );	/* command register c     */
	DS_ALIGN( registerd );	/* command register d     */
        volatile unsigned char ram[1];	/* ram/bank1 regs */
};

typedef struct ds17287_clk      ds17287_clk_t;
#endif

/*
 * Regiser offsets
 */
#define DS_SEC_REG           0
#define DS_SEC_ALARM_REG     1
#define DS_MIN_REG           2
#define DS_MIN_ALARM_REG     3
#define DS_HOUR_REG          4
#define DS_HOUR_ALARM_REG    5
#define DS_DAY_REG           6
#define DS_DATE_REG          7
#define DS_MONTH_REG         8
#define DS_YEAR_REG          9
#define DS_A_REG             10
#define DS_B_REG             11
#define DS_C_REG             12
#define DS_D_REG             13
#define DS_RAM               14
#define DS_RAM_SIZE          114

/*
 * Bank 1 offsets in ds17287->ram
 */
#define DS_BANK1_BASE			(DS_OFFSET(0,0x40 - 0xE))
#define DS_BANK1_MODEL			(DS_OFFSET(DS_BANK1_BASE,0))
#define DS_BANK1_SERIAL(n)		(DS_OFFSET(DS_BANK1_BASE,1+n))
#define DS_BANK1_CRC			(DS_OFFSET(DS_BANK1_BASE,7))
#define DS_BANK1_CENTURY		(DS_OFFSET(DS_BANK1_BASE,8))
#define DS_BANK1_DATE_ALARM		(DS_OFFSET(DS_BANK1_BASE,9))

/*
 * Bank 1 extended control register "A" and bit definitions.
 */
#define DS_BANK1_XCTRLA			(DS_OFFSET(DS_BANK1_BASE,0xa))
#define DS_XCTRLA_VRT2			0x80
#define DS_XCTRLA_INCR			0x40
#define DS_XCTRLA_BME			0x20
#define DS_XCTRLA_PAB			0x8
#define DS_XCTRLA_RF			0x4
#define DS_XCTRLA_WF			0x2
#define DS_XCTRLA_KF			0x1

/*
 * Bank 1 extended control register "B" and bit definitions.
 */
#define DS_BANK1_XCTRLB			(DS_OFFSET(DS_BANK1_BASE,0xb))
#define DS_XCTRLB_ABE			0x80
#define DS_XCTRLB_E32K			0x40
#define DS_XCTRLB_CS			0x20
#define DS_XCTRLB_RCE			0x10
#define DS_XCTRLB_PRS			0x8
#define DS_XCTRLB_RIE			0x4
#define DS_XCTRLB_WIE			0x2
#define DS_XCTRLB_KSE			0x1

#define DS_BANK1_XRAM_ADDR_LSB		(DS_OFFSET(DS_BANK1_BASE,0x10))
#define DS_BANK1_XRAM_ADDR_MSB		(DS_OFFSET(DS_BANK1_BASE,0x11))
#define DS_BANK1_XRAM_DATA		(DS_OFFSET(DS_BANK1_BASE,0x13))

#define RTDS_CLOCK_ADDR   RT_CLOCK_ADDR

#define WTR_READ	0		/* _clock_func read command  */
#define WTR_WRITE	1		/* _clock_func write command */

/* 
 * Clock dependent defs for Watchdog Timekeeper Registers 
 *
 * Control register A definitions.
 */
#define DS_REGA_UIP		0x80	/* update in progress bit    */
#define DS_REGA_DV2		0x40	/* countdown chan reset = 1  */
#define DS_REGA_DV1		0x20	/* oscillator enable = 1     */
#define DS_REGA_DV0		0x10	/* bank select: 0 = bank 0   */
#define DS_REGA_RS3		0x8	/* RS 0,1,2,3: rate select   */
#define DS_REGA_RS2		0x4
#define DS_REGA_RS1		0x2
#define DS_REGA_RS0		0x1

/*
 * Control register B definitions.
 */
#define DS_REGB_SET		0x80	/* inhibit update of RTC = 1    */
#define DS_REGB_PIE		0x40	/* periodic interrupt ena = 1   */
#define DS_REGB_AIE		0x20	/* alarm interrupt enable = 1   */
#define DS_REGB_UIE		0x10	/* update ended intr enable = 1 */
#define DS_REGB_SQWE		0x8	/* square wave gen enable = 1   */
#define DS_REGB_DM		0x4	/* data mode, 1 = binary        */
#define DS_REGB_2412		0x2	/* 12/24 hour mode 1 = 24 hour  */
#define DS_REGB_DSE		0x1	/* daylight saving enable = 1   */

/*
 * Control register C definitions.
 * only the high order nibble has defined bits.
 */
#define DS_REGC_IRQF		0x80	/* interrupt flag, intr occured = 1 */
#define DS_REGC_PF		0x40	/* periodic interrupt occured = 1   */
#define DS_REGC_AF		0x20	/* alarm interrupt occured = 1      */
#define DS_REGC_UF		0x10	/* update ended intr occured = 1    */

/*
 * Control register D definitions.
 * Only the high order bit is defined.
 */
#define DS_REGD_VRT		0x80	/* battery is alive = 1 */

/*
 * don't care code for dallas alarm.
 */
#define DS_ALARM_DONT_CARE	0xc0	/* don't care alarm time */

#endif /* __SYS_DS1286CLK_H__ */
