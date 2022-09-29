#ifndef	__MS_CMS_TYPES_H
#define	__MS_CMS_TYPES_H

/*
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

#include "sys/types.h"
#include "sys/sema.h"

#ifdef _KERNEL
#include "ksys/cell.h"
#include "sys/systm.h"
#include "sys/cmn_err.h"
#include "ksys/cell/cell_set.h"
#include "ksys/cell/cell_hb.h"
#include "ksys/cell/mesg.h"
#include "ksys/cell/subsysid.h"
#else
#include "cell_set.h"
#include "cms_kernel.h"
#endif


typedef int ageno_t;
typedef int seq_no_t;

typedef enum cms_state {
	CMS_NASCENT,		/* Initial state of cmsd on boot 	     */
	CMS_FOLLOWER,		/* Cells waits for a proposal and membership */
	CMS_LEADER,		/* Initiates leader protocol 		     */
	CMS_STABLE,		/* Stable state. Agrees on membership 	     */
	CMS_DEAD		/* Cell is dead 			     */
} cms_state_t;

#endif	/* _MS_CMS_TYPES_H */
