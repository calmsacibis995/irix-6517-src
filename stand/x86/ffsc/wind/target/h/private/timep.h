/* timeP.h - private time header file */

/* Copyright 1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01d,17aug93,dvs  changed TIME to TIMEO to fix conficting defines (SPR #2249)
01c,22sep92,rrr  added support for c++
01b,25jul92,smb  modification for the time library
01a,08jul92,smb  created.
*/

#include "time.h"

#ifndef __INCtimePh
#define __INCtimePh

#ifdef __cplusplus
extern "C" {
#endif

#define SECSPERMIN      60	/* Seconds per minute */
#define MINSPERHOUR     60	/* minutes per hour */
#define HOURSPERDAY     24	/* hours per day */
#define DAYSPERWEEK     7	/* days per week */
#define MONSPERYEAR     12	/* months per year */
#define SECSPERHOUR     (SECSPERMIN * MINSPERHOUR)
#define SECSPERDAY      ((long) SECSPERHOUR * HOURSPERDAY)

#define DAYSPERYEAR  	365	/* days per non-leap year */

#define CENTURY	        100	/* years per century */

#define TM_THURSDAY	4	/* Thursday is the fourth day of the week */
#define TM_SUNDAY	0	/* Sunday is the zeroth day of the week */
#define TM_MONDAY	1	/* Monday is the first day of the week */

#define EPOCH_WDAY      TM_THURSDAY
#define EPOCH_YEAR      1970
#define TM_YEAR_BASE    1900	/* Change this before the year 2000! */
#define TM_YEAR_FINAL   2038

#define ABBR		1	/* Abbrivations only */
#define FULL		0	/* Full names */

/* Buffer used for asctime */

#define ASCBUF          "Day Mon dd hh:mm:ss yyyy\n\0"
#define MaxBufferSize   25

/* ZONE and Day light saving definitions */

#define ZONEBUFFER      "UTC:UTC:000:ddmmhh:ddmmhh\0"

#define NAME		0	/* zone name offset */
#define NAME2		1	/* zone name offset */
#define TIMEOFFSET   	2	/* time difference offset */

#define DSTON		0	/* Day light saving on  */
#define DSTOFF		1	/* Day light saving off */

/* LOCALE definitions */

#define DATETIME	0	/* used with time locale, don't change!  */
#define DATE   		1	/* Date offset in locale structure */
#define TIMEO  		2	/* Time offset in locale structure */
#define AMPM   		3	/* AM/PM offset in locale structure */
#define ZONE   		4	/* Zone offset in locale structure */
#define DST   		5	/* DST offset in locale structure */

typedef struct __timeLocale
    {
    char *_Days[14];
    char *_Months[24];
    char *_Format[3];
    char *_Ampm[2];
    char *_Zone[3];
    char *_Isdst[2];
    } TIMELOCALE;

#define isleap(y) ((((y) % 4) == 0 && ((y) % 100) != 0) || ((y) % 400) == 0)
#define EOS '\0'

#if defined(__STDC__) || defined(__cplusplus)

extern int  __daysSinceEpoch (int, int);
extern int  __julday (int, int, int);
extern int  __getTime (const time_t , struct tm *);
extern int  __getDstInfo (struct tm *, TIMELOCALE *);
extern void __getZoneInfo (char *, int, TIMELOCALE *);

#else	/* __STDC__ */

extern int  __daysSinceEpoch ();
extern int  __julday ();
extern int  __getTime ();
extern int  __getDstInfo ();
extern void __getZoneInfo ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCtimePh */
