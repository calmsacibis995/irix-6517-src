/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ifndef __MPCONF_H__
#define __MPCONF_H__

#ident "$Revision: 1.3 $"

#ifdef __cplusplus
extern "C" {
#endif

/* mpconf commands */
#define	_MIPS_MP_NPROCESSORS		1
#define	_MIPS_MP_NAPROCESSORS		2
#define	_MIPS_MP_PROCESSOR_ACCT		3
#define	_MIPS_MP_PROCESSOR_TOTACCT	4
#define	_MIPS_MP_ISPROCESSOR_AVAIL	5
#define	_MIPS_MP_PROCESSOR_BIND		6
#define	_MIPS_MP_PROCESSOR_UNBIND	7
#define	_MIPS_MP_PROCESSOR_EXBIND	8
#define	_MIPS_MP_PROCESSOR_EXUNBIND	9
#define	_MIPS_MP_PROCESSOR_PID		100

#if !defined(_KERNEL)
/* prototype */
extern int mpconf(int, ...);
#endif

#ifdef __cplusplus
}
#endif
#endif
