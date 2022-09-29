/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SYS_TTYDEV_H
#define _SYS_TTYDEV_H

/*	@(#)ttydev.h 2.6 88/02/08 SMI; from UCB 4.3 83/05/18	*/

#include <standards.h>

/*
 * Terminal definitions related to underlying hardware. */

/*
 * Speeds
 * Not all rates are supported by all hardware.
 */
/*
 * When an old TCGETA is used on a line that has bit rate
 * other than standard 0-38400, __OLD_INVALID_BAUD is returned in c_cflags.
 * Also, when this value is used in an old-style TCSETA, 
 * the bit rate is unchanged -- it implies that the application
 * does not understand the new bit rates.
 */

#define __NEW_INVALID_BAUD	1800

#define	__OLD_B0	0
#define	__OLD_B50	0000001		/* not supported */
#define	__OLD_B75	0000002
#define	__OLD_B110	0000003
#define	__OLD_B134	0000004
#define	__OLD_B150	0000005
#define	__OLD_B200	0000006		/* not supported */
#define	__OLD_B300	0000007
#define	__OLD_B600	0000010
#define	__OLD_B1200	0000011
#define	__OLD_B1800	0000012		/* not supported */
#define	__OLD_B2400	0000013
#define	__OLD_B4800	0000014
#define	__OLD_B9600	0000015
#define	__OLD_B19200	0000016
#define	__OLD_EXTA	0000016
#define	__OLD_B38400	0000017
#define	__OLD_EXTB	0000017
#define __OLD_INVALID_BAUD	__OLD_B1800

#if !defined(_OLD_TERMIOS) && !_ABIAPI

#define	B0	0
#define	B50	50
#define	B75	75
#define	B110	110
#define	B134	134
#define	B150	150
#define	B200	200
#define	B300	300
#define	B600	600
#define	B1200	1200
#define	B1800	1800
#define	B2400	2400
#define	B4800	4800
#define	B9600	9600
#define	B19200	19200
#define	EXTA	19200
#define	B38400	38400
#define	EXTB	38400
#define B57600	57600
#define B76800	76800
#define B115200	115200
#define __INVALID_BAUD 1800

#else /* _OLD_TERMIOS || _ABIAPI */

#define	B0	__OLD_B0
#define	B50	__OLD_B50
#define	B75	__OLD_B75
#define	B110	__OLD_B110
#define	B134	__OLD_B134
#define	B150	__OLD_B150
#define	B200	__OLD_B200
#define	B300	__OLD_B300
#define	B600	__OLD_B600
#define	B1200	__OLD_B1200
#define	B1800	__OLD_B1800
#define	B2400	__OLD_B2400
#define	B4800	__OLD_B4800
#define	B9600	__OLD_B9600
#define	B19200	__OLD_B19200
#define	EXTA	__OLD_B19200
#define	B38400	__OLD_B38400
#define	EXTB	__OLD_B38400
#define __INVALID_BAUD __OLD_INVALID_BAUD

#endif /* _OLD_TERMIOS || _ABIAPI */

#endif /*_SYS_TTYDEV_H */
