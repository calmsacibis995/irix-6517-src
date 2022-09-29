/*
 * IP19/mp/mpinval.c
 *
 * 	MP test modules for IP19
 *
 *
 * Copyright 1993, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#ident "$Revision: 1.2 $"

#include <sys/cpu.h>
#include <sys/types.h>
#include <sys/sbd.h>
#include "ide_msg.h"
#include "everr_hints.h"
#include "ip19.h"
#include "prototypes.h"

static __psunsigned_t cpu_loc[2];

uint
mpinval()
{
    getcpu_loc(cpu_loc);

    ide_invalidate_caches();
    msg_printf(VRB, "mpinval exits cpu %d of slot %d\n", cpu_loc[1], cpu_loc[0]);

    return(0);

} /* fn mpinval */
