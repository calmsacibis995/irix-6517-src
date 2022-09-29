/*
 * os/numa/pmo_error.h
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

#ifndef __NUMA_PMO_ERROR_H__
#define __NUMA_PMO_ERROR_H__

#include <sys/errno.h>
#include <sys/pmo.h>

/*
 * Policy Group Error Handles
 */


#define PMOERR_NOMEM                      -1
#define PMOERR_EFAULT                     -2
#define PMOERR_NS_NOSPACE                 -3
#define PMOERR_INV_OPCODE                 -4
#define PMOERR_ENOENT                     -5
#define PMOERR_ESRCH                      -6
#define PMOERR_EINVAL                     -7
#define PMOERR_EBUSY                      -8
#define PMOERR_EDEADLK                    -9

#define PMOERR_INV_PLACPOL                -10
#define PMOERR_INV_FBCKPOL                -11
#define PMOERR_INV_REPLPOL                -12
#define PMOERR_INV_MIGRPOL                -13
#define PMOERR_INV_PAGNPOL                -14

#define PMOERR_INV_MLD_HANDLE             -20
#define PMOERR_INV_MLDSET_HANDLE          -21
#define PMOERR_INV_PM_HANDLE              -22
#define PMOERR_INV_PMGROUP_HANDLE         -23
#define PMOERR_NOTPLACED                  -24

#define PMOERR_INV_TOPOLOGY               -30
#define PMOERR_INV_MLDLIST                -31
#define PMOERR_INV_RAFFLIST               -32
#define PMOERR_INV_RQMODE                 -33

#define PMOERR_E2BIG                      -40
#define PMOERR_NOTSUP			  -41

#define pmo_iserrorhandle(handle)      ((handle) < 0)
                                        
extern int pmo_checkerror(pmo_handle_t handle);


#endif /* __NUMA_PMO_ERROR_H__ */
