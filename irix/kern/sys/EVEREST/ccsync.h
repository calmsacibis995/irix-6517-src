/**************************************************************************
*                                                                        *
*               Copyright (C) 1994, Silicon Graphics, Inc.               *
*                                                                        *
*  These coded instructions, statements, and computer programs  contain  *
*  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
*  are protected by Federal copyright law.  They  may  not be disclosed  *
*  to  third  parties  or copied or duplicated in any form, in whole or  *
*  in part, without the prior written consent of Silicon Graphics, Inc.  *
*                                                                        *
**************************************************************************/

/* $Id: ccsync.h,v 1.2 1996/04/23 23:59:48 beck Exp $ */

#ifndef __CCSYNC_H__
#define __CCSYNC_H__

#define CCSYNCCODE(x)	('c'<<8|(x))

#define CCSYNCIO_PSET	CCSYNCCODE(1)	/* define processor set */
#define CCSYNCIO_INFO	CCSYNCCODE(2)	/* get device info */

#define	BV_VEC	1

typedef struct ccsyncinfo_s {
	pid_t	ccsync_master;		/* master pid */
	cpuid_t	ccsync_cpu;		/* cpu */
	uint_t	ccsync_flags;		/* flags */
	int8_t	ccsync_value;		/* register value */
} ccsyncinfo_t;
/*
 * Flags
 */
#define	CCSYNC_MAPPED		0x1	/* device is mapped */
#define	CCSYNC_MRLOCKONLY	0x2	/* mustrun lock only */

#endif /* __CCSYNC_H__ */
