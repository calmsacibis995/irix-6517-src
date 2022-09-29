/* rtc64613.h - Seiko-Epson rtc64613 RTC header file */

/* Copyright 1991-1992 Wind River Systems, Inc. */
/*
modification history
--------------------
01d,22sep92,rrr  added support for c++
01c,26may92,rrr  the tree shuffle
01b,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed ASMLANGUAGE to _ASMLANGUAGE
		  -changed copyright notice
01a,23may91,hdn  written.
*/

#ifndef __INCrtc64613h
#define __INCrtc64613h

#ifdef __cplusplus
extern "C" {
#endif

#define	RTC64613_REG_OFFSET		3

#ifndef _ASMLANGUAGE
typedef struct
    {
    UCHAR pad0[RTC64613_REG_OFFSET];
    UCHAR hz64;
    UCHAR pad1[RTC64613_REG_OFFSET];
    UCHAR second;
    UCHAR pad2[RTC64613_REG_OFFSET];
    UCHAR minute;
    UCHAR pad3[RTC64613_REG_OFFSET];
    UCHAR hour;
    UCHAR pad4[RTC64613_REG_OFFSET];
    UCHAR dayWeek;
    UCHAR pad5[RTC64613_REG_OFFSET];
    UCHAR date;
    UCHAR pad6[RTC64613_REG_OFFSET];
    UCHAR month;
    UCHAR pad7[RTC64613_REG_OFFSET];
    UCHAR year;
    UCHAR pad8[RTC64613_REG_OFFSET];
    UCHAR alarmHz64;
    UCHAR pad9[RTC64613_REG_OFFSET];
    UCHAR alarmSecond;
    UCHAR pad10[RTC64613_REG_OFFSET];
    UCHAR alarmMinute;
    UCHAR pad11[RTC64613_REG_OFFSET];
    UCHAR alarmHour;
    UCHAR pad12[RTC64613_REG_OFFSET];
    UCHAR alarmDayWeek;
    UCHAR pad13[RTC64613_REG_OFFSET];
    UCHAR alarmDate;
    UCHAR pad14[RTC64613_REG_OFFSET];
    UCHAR controlA;
    UCHAR pad15[RTC64613_REG_OFFSET];
    UCHAR controlB;
    } RTC;
#endif	/* _ASMLANGUAGE */

/* bit value for control A register */
#define RTC_CARRY_FLAG		0x80
#define RTC_CARRY_INT		0x10
#define RTC_ALARM_INT		0x08
#define RTC_ALARM_FLAG		0x01

/* bit value for control B register */
#define RTC_TEST_MODE		0x08
#define RTC_ADJUST		0x04
#define RTC_RESET		0x02
#define RTC_START_STOP		0x01

#ifdef __cplusplus
}
#endif

#endif /* __INCrtc64613h */
