/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* #ident	"@(#)cron:funcs.c	1.2" */
#ident	"$Revision: 1.6 $"

#include <sys/types.h>
#include <ctype.h>
#include <stdio.h>
#include <syslog.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "funcs.h"

int
days_btwn(int m1, int d1, int y1, int m2, int d2, int y2)
{
	/*
	 * Calculate the number of "full" days in between
	 * m1/d1/y1 and m2/d2/y2.
	 *
	 * There should not be more than a year separating the dates.
	 * m should be in the range 0-11, and d in the range 1-31.
	 */
	int days;

	if (m1 == m2) {
		if (d1 == d2 && y1 == y2)
			return(0);
		if (d1 < d2)
			return(d2 - d1 - 1);
	}

	/* the remaining dates are on different months */
	days = (days_in_mon(m1, y1) - d1) + (d2 - 1);
	for (m1 = (m1 + 1) % 12; m1 != m2; m1 = (m1 + 1) % 12) {
		if (m1 == 0)
			y1++;
		days += days_in_mon(m1, y1);
	}
	return(days);
}

int
leap(int y)
{
	/*
	 * A year is a leap year if it is divisible by 4, unless it is
	 * divisible by 100. In that case, it is only a leap year
	 * if also divisible by 400.
	 */
	return((y % 4) == 0 && ((y % 100) != 0 || (y % 400) == 0));
}

int
days_in_mon(int m, int y)
{
	/*
	 * Returns the number of days in month m of year y.
	 * m should be in the range 0-11.
	 */
	static int dom[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

	return (m != 1 ? dom[m] : dom[m] + leap(y));
}

void *
xmalloc(size_t size)
{
	void *p;
	int n = 0;

	while ((p = malloc(size)) == NULL) {
		if (n++ > 20) {
			syslog(LOG_ALERT, "can't allocate %lu bytes of space",
			       (unsigned long) size);
			n = 0;
		}
		sleep(20);
	}
	return(p);
}

int
set_cloexec(int fd)
{
	return(fcntl(fd, F_SETFD, fcntl(fd, F_GETFD) | FD_CLOEXEC));
}
