#ifndef __RACER_HEART_VEC_H__
#define __RACER_HEART_VEC_H__
/*
 * heart_vec.h - heart interrupt vector structure header file.
 *
 * Copyright 1996, Silicon Graphics, Inc.
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
#ident "heart_vec.h $Revision: 1.8 $"

#include "sys/kthread.h"
#include "ksys/xthread.h"

typedef struct heart_ivec_s {
	intr_func_t	hv_func;		/* Function to call */
	intr_arg_t	hv_arg;			/* Arg to function */
	cpuid_t		hv_dest;		/* CPU to interrupt */
	thd_int_t	hv_tinfo;
} heart_ivec_t;

extern heart_ivec_t heart_ivec[];

#define hv_isync	hv_tinfo.thd_isync
#define hv_lat		hv_tinfo.thd_latstats
#define hv_thread	hv_tinfo.thd_ithread
#define hv_flags	hv_tinfo.thd_flags
 
/* Bits in HW dependent area of thread flags */
#define HEART_RETRACE 0x1

#endif /* __RACER_HEART_VEC_H__ */
