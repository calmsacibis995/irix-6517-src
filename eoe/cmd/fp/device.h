/*
 *============================================================================
 * 			File:		device.h
 *			Purpose:	This file contains macros and other
 *					misc definitions that pertain to the
 *					file: device.c
 *============================================================================
 */
#ifndef	_DEVICE_H
#define	_DEVICE_H

#include <stdio.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/major.h>
#include <sys/sysmacros.h>
#include <sys/param.h>
#include <sys/dkio.h>
#include <sys/dvh.h>
#include <ctype.h>

#ifdef ARCS_SA
extern int errno;
#include <arcs/errno.h>
#else
#include <errno.h>
#endif

#include <sys/scsi.h>
#include <sys/termio.h>
#include <sys/buf.h>
#include <sys/iobuf.h>
#include <sys/elog.h>
#include <sys/dksc.h>
#include <stddef.h>
#include "misc.h"

#define HMAX_HEADS       255             /* num. heads. capacity > 1Gb */
#define HMAX_SECTORS     63              /* num. sects. capacity > 1Gb */

#define	LMAX_HEADS	64		/* num. heads. capacity < 1Gb */
#define	LMAX_SECTORS	32		/* num. sects. capacity < 1Gb */

#define VP(x)          ((struct volume_header *)x)
#define DP(x)          (&VP(x)->vh_dp)
#define PT(x)          (VP(x)->vh_pt)
#define DT(x)          (VP(x)->vh_vd)
#define NCYLS(x)       ((((uint_t)DP(x)->dp_cylshi)<<16)+(uint_t)DP(x)->dp_cyls)
#define MAINDEFCYL(x)  (NCYLS(x)-1)
#define ALTDEFCYL(x)   (MAINDEFCYL(x)-8)
#define dp_heads       dp_trks0

#endif
