/*
 * IP19/mp/mpclear_ip.c
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
#include <sys/EVEREST/evintr.h>
#include "ide_msg.h"
#include "everr_hints.h"
#include "ip19.h"
#include "prototypes.h"

static __psunsigned_t cpu_loc[2];

uint
mpclear_ip()
{

    __psunsigned_t tmp;

    getcpu_loc(cpu_loc);

    for (tmp = 0; tmp < EVINTR_MAX_LEVELS; tmp++)
	    EV_SET_LOCAL(EV_CIPL0, tmp);
    EV_SET_LOCAL(EV_IP0, 0);
    EV_SET_LOCAL(EV_IP1, 0);

    /*
     * as long as interrupts still pending, clear them
     */

    while (tmp = EV_GET_LOCAL(EV_HPIL)) {
	msg_printf(DBG, "interrupt level %x still pending\n", (int) tmp);
	EV_SET_LOCAL(EV_CIPL0, tmp);
    }

    msg_printf(VRB, "mpclear_ip exits cpu %d of slot %d\n", cpu_loc[1], cpu_loc[0]);

    return(0);

}
