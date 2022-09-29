#ident	"$Id: pon.h,v 1.2 1995/10/15 18:22:28 benf Exp $"
/**************************************************************************
 *									  *
 * 		 Copyright (C) 1993, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/


/*
 * pon.h - power-on diagnostics
 */

/* Bitmasks indicating which diagnostics have failed
 */
#define FAIL_NOGRAPHICS		0x1
#define FAIL_BADGRAPHICS	0x2
#define FAIL_INT2		0x4
#define FAIL_MEMORY		0x8
#define FAIL_CACHES		0x10
#define FAIL_SCSIFUSE		0x20
#define FAIL_SCSICTRLR		0x100
#define FAIL_SCSIDEV		0x200
#define FAIL_DUART		0x400
#define FAIL_PCCTRL		0x800
#define FAIL_PCKBD		0x1000
#define FAIL_PCMS		0x2000
#define FAIL_ISDN		0x4000

/* Masks to indicate which bytes/words are bad within a bank, as determined
 * by memory configuration and diags
 */
#define	BAD_SIMM0	0x00008000
#define	BAD_SIMM1	0x00004000
#define	BAD_SIMM2	0x00002000
#define	BAD_SIMM3	0x00001000
#define	BAD_ALLSIMMS	0x0000f000

#if defined(_LANGUAGE_C)
extern unsigned int simmerrs;
#endif /* _LANGUAGE_C */
