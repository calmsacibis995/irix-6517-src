/* pc.h - PC 386 header */

/*
modification history
--------------------
01l,14jun95,myz  removed #include tyLib.h
01k,21oct94,hdn  deleted ENABLE_A20 macro.
01j,15oct94,hdn  added macros for LPT parallel driver.
01i,25apr94,hdn  moved a macro PC_KBD_TYPE to config.h.
01h,08nov93,vin  added support for pc console drivers.
01g,12oct93,hdn  added interrupt level macros.
01f,16aug93,hdn  added RTC related macros.
01e,03aug93,hdn  changed vectors for serial and timer.
01d,17jun93,hdn  updated to 5.1.
01c,07apr93,hdn  renamed compaq to pc.
01b,26mar93,hdn  deleted a macro CPU because it supports 386 and 486.
01a,15may92,hdn  written based on frc386 version.
*/

/*
This file contains IO address and related constants for the
PC 386.
*/

#ifndef	INCpch
#define	INCpch

#include "drv/intrctl/i8259a.h"
#include "drv/timer/i8253.h"
#include "drv/timer/mc146818.h"
#include "drv/timer/timerdev.h"
#include "drv/hdisk/idedrv.h"
#include "drv/fdisk/nec765fd.h"
#include "drv/serial/pcconsole.h"
#include "drv/parallel/lptdrv.h"

#undef	CAST
#define	CAST

#define TARGET_PC386

#define BUS		VME_BUS		/* XXX */

#define INT_VEC_IRQ0	0x20		/* vector for IRQ0 */


/* programmable interrupt controller (PIC) */

#define	PIC1_BASE_ADR		0x20
#define	PIC2_BASE_ADR		0xa0
#define	PIC_REG_ADDR_INTERVAL	1	/* address diff of adjacent regs. */

/* serial ports (COM1-COM6) */

#define	COM1_BASE_ADR		0x3f8
#define	COM2_BASE_ADR		0x2f8
#define	COM3_BASE_ADR		0x2e0
#define	COM4_BASE_ADR		0x2e8
#define	COM5_BASE_ADR		0x3e0
#define	COM6_BASE_ADR		0x3e8
#define	COM1_INT_LVL		0x04
#define	COM2_INT_LVL		0x03
#define	COM3_INT_LVL		0x0f
#define	COM4_INT_LVL		0x0b
#define	COM5_INT_LVL		0x0c
#define	COM6_INT_LVL		0x0a
#define	COM1_INT_VEC		(INT_VEC_IRQ0 + COM1_INT_LVL)
#define	COM2_INT_VEC		(INT_VEC_IRQ0 + COM2_INT_LVL)
#define	COM3_INT_VEC		(INT_VEC_IRQ0 + COM3_INT_LVL)
#define	COM4_INT_VEC		(INT_VEC_IRQ0 + COM4_INT_LVL)
#define	COM5_INT_VEC		(INT_VEC_IRQ0 + COM5_INT_LVL)
#define	COM6_INT_VEC		(INT_VEC_IRQ0 + COM6_INT_LVL)
#define	UART_REG_ADDR_INTERVAL	1	/* address diff of adjacent regs. */
#define	N_UART_CHANNELS		6


/* timer (PIT) */

#define	PIT_BASE_ADR		0x40
#define	PIT_INT_LVL		0x00
#define	PIT_INT_VEC		(INT_VEC_IRQ0 + PIT_INT_LVL)
#define	PIT_REG_ADDR_INTERVAL	1	/* address diff of adjacent regs. */
#define	PIT_CLOCK		1193180

/* real time clock (RTC) */

#define	RTC_INDEX		0x70
#define	RTC_DATA		0x71
#define	RTC_INT_LVL		0x08
#define	RTC_INT_VEC		(INT_VEC_IRQ0 + RTC_INT_LVL)

/* floppy disk (FD) */

#define FD_INT_LVL              0x06
#define FD_INT_VEC              (INT_VEC_IRQ0 + FD_INT_LVL)

/* hard disk (IDE) */

#define IDE_CONFIG		0x0	/* 1: uses ideTypes table */
#define IDE_INT_LVL             0x0e
#define IDE_INT_VEC             (INT_VEC_IRQ0 + IDE_INT_LVL)

/* parallel port (LPT) */

#define LPT_INT_LVL             0x07
#define LPT_INT_VEC             (INT_VEC_IRQ0 + LPT_INT_LVL)

/* key board (KBD) */

#define PC_XT_83_KBD		0	/* 83 KEY PC/PCXT/PORTABLE 	*/
#define PC_PS2_101_KBD		1	/* 101 KEY PS/2 		*/
#define KBD_INT_LVL		0x01
#define KBD_INT_VEC		(INT_VEC_IRQ0 + KBD_INT_LVL)

#define	COMMAND_8042		0x64
#define	DATA_8042		0x60
#define	STATUS_8042		COMMAND_8042
#define COMMAND_8048		0x61	/* out Port PC 61H in the 8255 PPI */
#define	DATA_8048		0x60	/* input port */
#define	STATUS_8048		COMMAND_8048

#define JAPANES_KBD             0
#define ENGLISH_KBD             1

/* beep generator */

#define DIAG_CTRL	0x61
#define BEEP_PITCH_L	1280 /* 932 Hz */
#define BEEP_PITCH_S	1208 /* 987 Hz */
#define BEEP_TIME_L	(sysClkRateGet () / 3) /* 0.66 sec */
#define BEEP_TIME_S	(sysClkRateGet () / 8) /* 0.15 sec */

/* Monitor definitions */

#define MONOCHROME              0
#define VGA                     1
#define MONO			0
#define COLOR			1
#define	VGA_MEM_BASE		(UCHAR *) 0xb8000
#define	VGA_SEL_REG		(UCHAR *) 0x3d4
#define VGA_VAL_REG             (UCHAR *) 0x3d5
#define MONO_MEM_BASE           (UCHAR *) 0xb0000
#define MONO_SEL_REG            (UCHAR *) 0x3b4
#define MONO_VAL_REG            (UCHAR *) 0x3b5
#define	CHR			2

/* change this to JAPANES_KBD if Japanese enhanced mode wanted */

#define KEYBRD_MODE             ENGLISH_KBD 

/* undefine this if ansi escape sequence not wanted */

#define INCLUDE_ANSI_ESC_SEQUENCE 

#define GRAPH_ADAPTER   VGA

#if (GRAPH_ADAPTER == MONOCHROME)

#define DEFAULT_FG  	ATRB_FG_WHITE
#define DEFAULT_BG 	ATRB_BG_BLACK
#define DEFAULT_ATR     DEFAULT_FG | DEFAULT_BG
#define CTRL_SEL_REG	MONO_SEL_REG		/* controller select reg */
#define CTRL_VAL_REG	MONO_VAL_REG		/* controller value reg */
#define CTRL_MEM_BASE	MONO_MEM_BASE		/* controller memory base */
#define COLOR_MODE	MONO			/* color mode */

#else /* GRAPH_ADAPTER = VGA */

#define DEFAULT_FG	ATRB_FG_BRIGHTWHITE
#define DEFAULT_BG	ATRB_BG_BLUE
#define DEFAULT_ATR	DEFAULT_FG | DEFAULT_BG
#define CTRL_SEL_REG	VGA_SEL_REG		/* controller select reg */
#define CTRL_VAL_REG	VGA_VAL_REG		/* controller value reg */
#define CTRL_MEM_BASE	VGA_MEM_BASE		/* controller memory base */
#define COLOR_MODE	COLOR			/* color mode */

#endif /* (ADAPTER == MONOCHROME) */

#endif	/* INCpch */
