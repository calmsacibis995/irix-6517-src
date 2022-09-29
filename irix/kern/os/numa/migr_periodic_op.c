/*****************************************************************************
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
 ****************************************************************************/

/*
 * Periodic operations for migration control:
 */

#include <sys/types.h>
#include <sys/bsd_types.h>
#include <sys/cpumask.h>
#include <sys/sysmacros.h>
#include <sys/debug.h>
#include <sys/systm.h>
#include <sys/cmn_err.h>

#include <sys/numa.h>
#include <sys/nodepda.h>
#include "pfms.h"
#include "migr_traffic.h"
#include "migr_bounce.h"
#include "migr_queue.h"
#include "migr_unpegging.h"
#include "migr_manager.h"
#include "migr_control.h"
#include "debug_levels.h"
#include "numa_stats.h"















