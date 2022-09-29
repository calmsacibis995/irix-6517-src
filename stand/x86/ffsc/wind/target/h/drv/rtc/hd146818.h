/* hd146818.h - Hitachi 146818 RTC Real Time Clock header */

/* Copyright 1991-1992 Wind River Systems, Inc. */
/*
modification history
--------------------
01e,22sep92,rrr  added support for c++
01d,26may92,rrr  the tree shuffle
01c,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed ASMLANGUAGE to _ASMLANGUAGE
		  -changed copyright notice
01b,07jun91,hdn  defined HD146818_REG_OFFSET.
01a,26feb91,hdn  written.
*/

#ifndef __INChd146818h
#define __INChd146818h

#ifdef __cplusplus
extern "C" {
#endif

#define	HD146818_REG_OFFSET		3

#ifndef _ASMLANGUAGE
typedef struct
    {
    UCHAR pad0[HD146818_REG_OFFSET];
    UCHAR seconds;
    UCHAR pad1[HD146818_REG_OFFSET];
    UCHAR secAlarm;
    UCHAR pad2[HD146818_REG_OFFSET];
    UCHAR minutes;
    UCHAR pad3[HD146818_REG_OFFSET];
    UCHAR minAlarm;
    UCHAR pad4[HD146818_REG_OFFSET];
    UCHAR hours;
    UCHAR pad5[HD146818_REG_OFFSET];
    UCHAR hrAlarm;
    UCHAR pad6[HD146818_REG_OFFSET];
    UCHAR dayOfWeek;
    UCHAR pad7[HD146818_REG_OFFSET];
    UCHAR date;
    UCHAR pad8[HD146818_REG_OFFSET];
    UCHAR month;
    UCHAR pad9[HD146818_REG_OFFSET];
    UCHAR year;
    UCHAR pad10[HD146818_REG_OFFSET];
    UCHAR controlA;
    UCHAR pad11[HD146818_REG_OFFSET];
    UCHAR controlB;
    UCHAR pad12[HD146818_REG_OFFSET];
    UCHAR controlC;
    UCHAR pad13[HD146818_REG_OFFSET];
    UCHAR controlD;
    } RTC;
#endif	/* _ASMLANGUAGE */

/* bit values for control register A */
#define RTC_UIP			0x80
#define RTC_1MHZ		0x10
#define RTC_32KHZ		0x20
#define RTC_DV_RESET		0x60

/* bit values for control register B */
#define RTC_SET			0x80
#define RTC_PIE			0x40
#define RTC_AIE			0x20
#define RTC_UIE			0x10
#define RTC_SQWE		0x08
#define RTC_DM			0x04
#define RTC_24			0x02
#define RTC_DSE			0x01

/* bit values for control register C */
#define RTC_IRQF		0x80
#define RTC_PF			0x40
#define RTC_AF			0x20
#define RTC_UF			0x10

#ifdef __cplusplus
}
#endif

#endif /* __INChd146818h */
