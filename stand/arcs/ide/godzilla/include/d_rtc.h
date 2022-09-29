#ifndef __IDE_rtc_H__
#define	__IDE_rtc_H__

/*
 * d_rtc.h
 *
 * IDE rtc tests header 
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#ident	"ide/godzilla/include/d_rtc.h:  $Revision: 1.1 $"

typedef unsigned char              rtcregisters_t;
                                         
/*
 * constants used in rtc_regs.c
 */
#define	RTC_REGS_PATTERN_MAX	6

typedef struct	_rtc_Registers {
	char		*name;  	/* name of the register */
	__uint32_t	address;	/* address */
	__uint32_t	mode;	    	/* read / write only or read & write */
	unsigned char	mask;	    	/* read-write mask */
	unsigned char	def;    	/* default value */
} rtc_Registers;

#endif	/* __IDE_rtc_H__ */

