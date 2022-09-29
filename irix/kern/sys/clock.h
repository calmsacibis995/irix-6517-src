/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*#ident	"@(#)kern-port:sys/clock.h	10.1"*/
#ident	"$Revision: 3.6 $"
 
#define	SECHR	(60*60)	/* seconds/hr */
#define	SECDAY	(24*SECHR)	/* seconds/day */
#define	SECYR	(365*SECDAY)	/* seconds/common year */
#define SECMIN	60		/* seconds/min */
#define SECHOUR	SECHR

#define YRREF		1970
#define	LEAPYEAR(x)	(((x) % 4 == 0 && (x) % 100 != 0) || (x) % 400 == 0)
#define	TODRZERO	(1<<26)
