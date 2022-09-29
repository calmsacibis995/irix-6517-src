/*
 * FILE: eoe/cmd/miser/lib/libmiser/src/util.c
 *
 * DESCRIPTION:
 *      Miscellaneous library functions that provides support to rest of miser.
 */

/**************************************************************************
 *									  *
 * 		 Copyright (C) 1997 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/


#include "libmiser.h"
#include <syslog.h>
#include <time.h>

int G_syslog = 0;


void
msyslog(int priority, const char *fmt, ...)
/*
 * Print miser syslog to SYSLOG or stderr.
 */
{
	va_list args;

	va_start(args, fmt);

        if (G_syslog) { 
                vsyslog(priority, fmt, args);
	} else {	
		vfprintf(stderr, fmt, args);
	}

	va_end(ap);

	if (!G_syslog) {
		fprintf(stderr, "\n");
	}

} /* msyslog */


void
merror(const char *fmt, ...)
/*
 * Print output to SYSLOG and stderr.
 */
{
	va_list	args;

	if (G_syslog) {
		va_start(args, fmt);
		vsyslog(LOG_ERR, fmt, args);
		va_end(args);
	} 

	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);

	fprintf(stderr, "\n");

} /* merror */


void
merror_hdr(const char *fmt, ...)
/*
 * Print message header to SYSLOG and stderr.
 */
{
	va_list	args;

	if (G_syslog) {
		va_start(args, fmt);
		vsyslog(LOG_ERR, fmt, args);
		va_end(args);
	} 

	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);

} /* merror_hdr */


void
merror_v(const char *fmt, va_list args)
/*
 * Print output to SYSLOG and stderr.
 */
{
	if (G_syslog) {
		vsyslog(LOG_ERR, fmt, args);
	}

	vfprintf(stderr, fmt, args);
	fprintf(stderr, "\n");

} /* merror_v */


char*
fmt_str2num(const char *str, uint64_t *num)
/*
 * Convert string to number.
 */
{
	uint64_t cnt = 0;
	char*    cp;

	for (cp = (char *) str; isdigit(*cp); cp++) {
		cnt = (10 * cnt) + (uint64_t) (*cp - '0');
	}

	*num = cnt;

	return cp;

} /* fmt_str2num */


char* 
fmt_time(char *buf, int size, time_t time)
/*
 * Convert local time to string.
 */
{	
	struct tm* tmptr;

	memset(buf, 0, size);	
	tmptr = localtime(&time);
	strftime(buf, size, "%D %T", tmptr);

	return buf;

} /* fmt_time */


int
fmt_str2bcnt(const char *str, memory_t *bcnt)
/*
 * Convert memory string to memory_t format.
 */
{
	char*     ks = "bkmgt";
	char*     kp;
	char*     cp;
	uint64_t  bc;

	if ((cp = fmt_str2num(str, &bc)) == str) {
		return 0;	/* error */
	}

	if (*cp && (kp = strchr(ks, *cp))) {
		bc <<= 10 * (kp-ks);
		cp++;
	}

	return !(*cp) && (*bcnt = (memory_t) bc, 1);

} /* fmt_str2bcnt */


int
fmt_str2time(const char *str, quanta_t *time)
/*
 * Convert time string to quantum time.
 */
{
	uint64_t sum = 0;
	uint64_t num;
	char*    cp;

	if ((cp = fmt_str2num(str, &num)) == str) {
		return 0;	/* error */
	}

	switch (*cp) {

	case 's':
		cp++;

	case '\0':
		sum = num;
		break;

	case 'm':
		cp++;
		sum = num * 60;
		break;

	case 'h':
		cp++;
		sum = num * 3600;
		break;

	case ':':
		str = ++cp;
		sum = num * 3600;

		if ((cp = fmt_str2num(str, &num)) == str || num > 59) {
			return 0;	/* error */
		}

	case '.':
		sum += num * 60;

		if (*cp != '.') {
			break;
		}

		str = ++cp;

		if ((cp = fmt_str2num(str, &num)) == str || num > 59) {
			return 0;	/* error */
		}

		sum += num;

	default:
		break;

	} /* switch */

	return !(*cp) && (*time = (quanta_t) sum, 1);

} /* fmt_str2time */


void
curr_time_str(char* timebuf)
/*
 * Returns a pointer to the current time string in format. 
 */
{
	struct tm*	tmptr;
	struct timeval	curtime;

	gettimeofday(&curtime, 0);

	tmptr = localtime(&curtime.tv_sec);

	strftime(timebuf, 30, "%D %T", tmptr);

} /* curr_time_str */
