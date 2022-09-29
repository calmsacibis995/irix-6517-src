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

#ident "$Revision: 1.9 $"

#ifndef	_RTC_H_
#define	_RTC_H_

#if _LANGUAGE_C

#include <sys/SN/agent.h>	/* For rtc_time_t */

/*
 * Microsecond timing routines based on Hub Real Time Clock
 */

#define RTC_TIME_MAX	((rtc_time_t) ~0ULL)

void		rtc_init(void);			/* Init based on hub freq.  */

void		rtc_stop(void);			/* Halt the counting	    */
void		rtc_start(void);		/* Restart the counting	    */

void		rtc_set(rtc_time_t usec);	/* Set time in microseconds */
rtc_time_t	rtc_time(void);			/* Get time in microseconds */
void		rtc_sleep(rtc_time_t usec);	/* Sleep in microseconds    */

void		inst_loop_delay(int num);	/* Sleep for num loops      */

#endif /* _LANGUAGE_C */

#endif /* _RTC_H_ */
