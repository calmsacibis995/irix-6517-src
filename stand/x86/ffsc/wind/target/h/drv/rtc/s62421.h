/* s62421.h - Seiko 62421 real time clock */

/* Copyright 1984-1992 Wind River Systems, Inc. */
/*
modification history
--------------------
01e,22sep92,rrr  added support for c++
01d,26may92,rrr  the tree shuffle
01c,04oct91,rrr  passed through the ansification filter
		  -changed copyright notice
01b,05oct90,shl  added copyright notice.
                 made #endif ANSI style.
01a,28nov88,jcf	 written.
*/

/*
This file contains constants for the Seiko 62421.
*/

#ifndef __INCs62421h
#define __INCs62421h

#ifdef __cplusplus
extern "C" {
#endif

/* RTC 62421 register definition */

#define RTC_1_SEC(base)	 ((char *) ((base) + 0x0))	/* 1 sec. digit */
#define RTC_10_SEC(base) ((char *) ((base) + 0x1))	/* 10 sec. digit */
#define RTC_1_MIN(base)	 ((char *) ((base) + 0x2))	/* 1 min. digit */
#define RTC_10_MIN(base) ((char *) ((base) + 0x3))	/* 10 min. digit */
#define RTC_1_HR(base)	 ((char *) ((base) + 0x4))	/* 1 hour digit */
#define RTC_10_HR(base)	 ((char *) ((base) + 0x5)) 	/* pm/am+10 hr digit */
#define RTC_1_DAY(base)	 ((char *) ((base) + 0x6)) 	/* 1 day digit */
#define RTC_10_DAY(base) ((char *) ((base) + 0x7))	/* 10 day digit */
#define RTC_1_MON(base)	 ((char *) ((base) + 0x8))	/* 1 month digit */
#define RTC_10_MON(base) ((char *) ((base) + 0x9))	/* 10 month digit */
#define RTC_1_YR(base)	 ((char *) ((base) + 0xa))	/* 1 year digit */
#define RTC_10_YR(base)	 ((char *) ((base) + 0xb))	/* 10 year digit */
#define RTC_WEEK(base)	 ((char *) ((base) + 0xc))	/* week */
#define RTC_CTR_D(base)	 ((char *) ((base) + 0xd))	/* control d */
#define RTC_CTR_E(base)	 ((char *) ((base) + 0xe))	/* control e */
#define RTC_CTR_F(base)	 ((char *) ((base) + 0xf))	/* control f */

#ifdef __cplusplus
}
#endif

#endif /* __INCs62421h */
