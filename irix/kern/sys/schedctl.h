/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ifndef __SYS_SCHEDCTL_H__
#define __SYS_SCHEDCTL_H__

#ifdef __cplusplus
extern "C" {
#endif

#ident	"$Revision: 3.49 $"

#include "sys/sysmp.h"
#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)
#include "sys/time.h"
#endif

/* defines for schedctl() commands */
#define	NDPRI		MPTS_RTPRI
#define	SLICE		MPTS_SLICE
#define	RENICE		MPTS_RENICE

/*
 * Additional schedctl commands.  These give precise emulation of the BSD4.3
 * getpriority and setpriority system calls.  They may be invoked through the
 * schedctl interface or by the direct system call hooks.
 */
#define RENICE_PROC	MPTS_RENICE_PROC
#define RENICE_PGRP	MPTS_RENICE_PGRP
#define RENICE_USER	MPTS_RENICE_USER
#define	GETNICE_PROC	MPTS_GTNICE_PROC
#define GETNICE_PGRP	MPTS_GTNICE_PGRP
#define GETNICE_USER	MPTS_GTNICE_USER

/* More schedctl commands */
#define SETHINTS	MPTS_SETHINTS
#define SCHEDMODE	MPTS_SCHEDMODE
#define SETMASTER	MPTS_SETMASTER
#define AFFINITY_ON	MPTS_AFFINITY_ON
#define AFFINITY_OFF 	MPTS_AFFINITY_OFF
#define AFFINITY_STATUS MPTS_AFFINITY_GET
/* schedctl inquiry codes */
#define GETNDPRI	MPTS_GETRTPRI

/* possible non-degrading priorities */

/* these priorities are higher than ALL normal user process priorities */
#define NDPHIMAX	30
#define NDPHIMIN	39
#define ishiband(x)	((x) >= NDPHIMAX && (x) <= NDPHIMIN)

/* these priorities overlap normal user process priorities */
#define NDPNORMMAX	40
#define NDPNORMMIN	127

/* these priorities are below ALL normal user process priorities */
#define NDPLOMAX	128
#define NDPLOMIN	254

/* values for share group scheduling mode (schedctl(SCHEDMODE)) */
#define SGS_FREE	1	/* all procs schedule independently */
#define SGS_SINGLE	2	/* only one process runs */
#define SGS_EQUAL	3	/* all processes get equal share of cpu */
#define SGS_GANG	4	/* all processes gang schedule */

#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)
#ifndef _KERNEL
ptrdiff_t schedctl(int, ...);
#endif
#endif

#ifdef _KERNEL
/* system configurable parameter */
union rval;
extern int schedctl(int, void *, void *, union rval *);

#endif

#ifdef __cplusplus
}
#endif

#endif /* __SYS_SCHEDCTL_H__ */
