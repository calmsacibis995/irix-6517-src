/**************************************************************************
 *                                                                        *
 *  Copyright (C) 1996 Silicon Graphics, Inc.                             *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ifndef _EV_PRIVATE_H_
#define _EV_PRIVATE_H_

#include <sys/pda.h>
/* cpu-specific information stored under INFO_LBL_CPU_INFO */
typedef struct cpuinfo_s {
	pda_t		*ci_cpupda;	/* pointer to CPU's private data area */
	cpuid_t		ci_cpuid;	/* CPU ID */
} *cpuinfo_t;

/* Get the cpu-specific info hanging off the cpu vertex */
#define cpuinfo_get(vhdl, infoptr) ((void)hwgraph_info_get_LBL \
	(vhdl, INFO_LBL_CPU_INFO, (arbitrary_info_t *)infoptr))

/* Store the cpu-specific info off the cpu vertex */
#define cpuinfo_set(vhdl, infoptr) (void)hwgraph_info_add_LBL \
	(vhdl, INFO_LBL_CPU_INFO, (arbitrary_info_t)infoptr)

extern void evhwg_setup(vertex_hdl_t,cpumask_t);
#endif /* _EV_PRIVATE_H_ */
