/*
 * os/numa/pmo_error.c
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

#include <sys/pmo.h>
#include <sys/errno.h>
#include <sys/cmn_err.h>
#include "pmo_error.h"


/*
 * The pmo errorcode to user errorcode map table
 */

static struct pmoerr_mapent {
        int   pmoerr_code;
        int   irix_code;
} pmoerr_maptable[] = {
        PMOERR_NOMEM,               ENOMEM,
        PMOERR_EFAULT,              EFAULT,
        PMOERR_NS_NOSPACE,          ENOSPC,
        PMOERR_INV_OPCODE,          ENOSYS,
        PMOERR_ENOENT,              ENOENT,
        PMOERR_ESRCH,               ESRCH,
        PMOERR_EINVAL,              EINVAL,
        PMOERR_INV_PLACPOL,         ENOENT,
        PMOERR_INV_FBCKPOL,         ENOENT,
        PMOERR_INV_REPLPOL,         ENOENT,
        PMOERR_INV_MIGRPOL,         ENOENT,
        PMOERR_INV_PAGNPOL,         ENOENT,
        PMOERR_INV_MLD_HANDLE,      EINVAL,
        PMOERR_INV_MLDSET_HANDLE,   EINVAL,
        PMOERR_INV_PM_HANDLE,       EINVAL,
        PMOERR_INV_PMGROUP_HANDLE,  EINVAL,
        PMOERR_INV_TOPOLOGY,        EINVAL,
        PMOERR_INV_MLDLIST,         EINVAL,
        PMOERR_INV_RAFFLIST,        EINVAL,
        PMOERR_INV_RQMODE,          EINVAL,
        PMOERR_NOTPLACED,           EXDEV,
        PMOERR_EBUSY,               EBUSY,
        PMOERR_EDEADLK,             EDEADLK,
        PMOERR_E2BIG,               E2BIG,
	PMOERR_NOTSUP,		    ENOTSUP
};


/*
 * Map pmoerr codes to irix error codes
 */
int
pmo_checkerror(pmo_handle_t handle)
{
        int i;
        
        if (pmo_iserrorhandle(handle)) {
                for (i = 0;
                     i < sizeof(pmoerr_maptable)/sizeof(struct pmoerr_mapent);
                     i++) {
                        if (pmoerr_maptable[i].pmoerr_code == handle) {
                                return (pmoerr_maptable[i].irix_code);
                        }
                }
#ifdef DEBUG                
                cmn_err(CE_NOTE, "[pmo_checkerror]: Invalid pmoerr_code %d", handle);
#endif                
                return (EINVAL);
        } else {
                return (0);
        }
}
                
                        
