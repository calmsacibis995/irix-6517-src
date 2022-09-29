/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1996 Silicon Graphics, Inc.           *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#include <sys/PCI/ioc3.h>

#define RTC_BASE_ADDR		(uchar_t *)(nvram_base)

/* Defines for the SGS-Thomson M48T35 clock */
#define RTC_SGS_WRITE_ENABLE    0x80
#define RTC_SGS_READ_PROTECT    0x40
#define RTC_SGS_YEAR_ADDR       (RTC_BASE_ADDR + 0x7fffL)
#define RTC_SGS_MONTH_ADDR      (RTC_BASE_ADDR + 0x7ffeL)
#define RTC_SGS_DATE_ADDR       (RTC_BASE_ADDR + 0x7ffdL)
#define RTC_SGS_DAY_ADDR        (RTC_BASE_ADDR + 0x7ffcL)
#define RTC_SGS_HOUR_ADDR       (RTC_BASE_ADDR + 0x7ffbL)
#define RTC_SGS_MIN_ADDR        (RTC_BASE_ADDR + 0x7ffaL)
#define RTC_SGS_SEC_ADDR        (RTC_BASE_ADDR + 0x7ff9L)
#define RTC_SGS_CONTROL_ADDR    (RTC_BASE_ADDR + 0x7ff8L)

/* Defines for the Dallas DS1386 */
#define RTC_DAL_UPDATE_ENABLE   0x80
#define RTC_DAL_UPDATE_DISABLE  0x00
#define RTC_DAL_YEAR_ADDR       (RTC_BASE_ADDR + 0xaL)
#define RTC_DAL_MONTH_ADDR      (RTC_BASE_ADDR + 0x9L)
#define RTC_DAL_DATE_ADDR       (RTC_BASE_ADDR + 0x8L)
#define RTC_DAL_DAY_ADDR        (RTC_BASE_ADDR + 0x6L)
#define RTC_DAL_HOUR_ADDR       (RTC_BASE_ADDR + 0x4L)
#define RTC_DAL_MIN_ADDR        (RTC_BASE_ADDR + 0x2L)
#define RTC_DAL_SEC_ADDR        (RTC_BASE_ADDR + 0x1L)
#define RTC_DAL_CONTROL_ADDR    (RTC_BASE_ADDR + 0xbL)
#define RTC_DAL_USER_ADDR       (RTC_BASE_ADDR + 0xeL)

#if 0
/* Defines for the M48T35 clock */
#define RTC_WRITE_ENABLE		0x80
#define RTC_READ_PROTECT		0x40
#define RTC_BASE_ADDR		(uchar_t *)(nvram_base)
#define RTC_YEAR_ADDR		(RTC_BASE_ADDR + 0x7fffL)
#define RTC_MONTH_ADDR		(RTC_BASE_ADDR + 0x7ffeL)
#define RTC_DATE_ADDR		(RTC_BASE_ADDR + 0x7ffdL)
#define RTC_DAY_ADDR		(RTC_BASE_ADDR + 0x7ffcL)
#define RTC_HOUR_ADDR		(RTC_BASE_ADDR + 0x7ffbL)
#define RTC_MIN_ADDR		(RTC_BASE_ADDR + 0x7ffaL)
#define RTC_SEC_ADDR		(RTC_BASE_ADDR + 0x7ff9L)
#define RTC_CONTROL_ADDR		(RTC_BASE_ADDR + 0x7ff8L)
#endif

#define BCD_TO_INT(x) (((x>>4) * 10) + (x & 0xf))

#define YRREF	1970 



