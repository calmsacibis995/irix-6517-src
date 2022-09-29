/*
 * os/numa/debug_levels.h
 *
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

#ifndef __NUMA_DEBUG_LEVELS_H__
#define __NUMA_DEBUG_LEVELS_H__

/*
 * Debug Codes
 */

#define DC_MIGR        0
#define DC_MLD         1
#define DC_RAFF        2
#define DC_MLDSET      3
#define DC_PM          4
#define DC_ASPM        5
#define DC_MEMD        6
#define DC_BOUNCE      7
#define DC_QUEUE       8
#define DC_UNPEGGING   9
#define DC_TRAFFIC     10




/*
 * Debug Masks
 */

#define DC_TO_DM(code) (0x1 << (code))

#define DM_ALL         (~0x0LL)
#define DM_MIGR        DC_TO_DM(DC_MIGR)
#define DM_MLD         DC_TO_DM(DC_MLD)
#define DM_RAFF        DC_TO_DM(DC_RAFF)
#define DM_MLDSET      DC_TO_DM(DC_MLDSET)
#define DM_PM          DC_TO_DM(DC_PM)
#define DM_ASPM        DC_TO_DM(DC_ASPM)


extern void mc_msg(int code, int level, char *fmt, ...);


#ifdef NUMADEBUG

/*
 * Debug Functions
 */
typedef void (*df_t)(void*, void*);

extern void numa_message_func(__uint64_t mask,
                              int level,
                              char* m,
                              long long v1,
                              long long v2);

#define FDBG(mask, level, f, arg1, arg2)   \
        frs_debug_selector((mask), (level), (df_t)(f), (arg1), (arg2))

#define numa_message(mask, level, message, value1, value2)  \
        numa_message_func((mask), (level), (message), (value1), (value2))

#else /* ! DEBUG */

#define FDBG(mask, level, f, arg1, arg2)
#define numa_message(mask, level, message, value1, value2)

#endif /* ! DEBUG */



#endif /* __NUMA_DEBUG_LEVELS_H__ */
