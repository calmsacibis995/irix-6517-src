/*
 * IP19/mp/mpmem_rd.c
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
#include "libsc.h"
#include "ide_msg.h"
#include "everr_hints.h"
#include "ip19.h"
#include "pattern.h"
#include "prototypes.h"

static __psunsigned_t cpu_loc[2];

/* mpmem_rd() reads contents of memory via unmapped access
 * to the value specified by the 'pattern' parm starting at 
 * the location specified by the 'start_addr' parm for the 
 * size specified by the 'size' parm.
 */
uint
mpmem_rd(int argc, char **argv)
{
    __psunsigned_t start_addr, ptr;
    uint size;
    uint pattern; /* value to verify */
    uint count = 0;
    register __psunsigned_t firstp, lastp;

    msg_printf(DBG,"argc is %d\n", argc);
    msg_printf(DBG,"argv is %s\n", *argv);

    if (argc != 4)
    {
	msg_printf(SUM,"Usage: mpmem_rd 0xPhysAddress 0xSize 0xDataPattern\n");
	return(1);
    }
    else
    {
	if (atob(argv[1], (int *)&start_addr) == 0) {
	    msg_printf(SUM, "Unable to understand the address argument\n");
	    return (1);
	}
	if (atob(argv[2], (int *)&size) == 0) {
	    msg_printf(SUM, "Unable to understand the size argument\n");
	    return (1);
	}
	if (atob(argv[3], (int *)&pattern) == 0) {
	    msg_printf(SUM, "Unable to understand the pattern argument\n");
	    return (1);
	}
    }

    msg_printf(DBG, "start_addr is 0x%x\n", start_addr);
    msg_printf(DBG, "size is 0x%x\n", size);
    msg_printf(DBG, "pattern is 0x%x\n", pattern);

    getcpu_loc(cpu_loc);
    msg_printf(VRB, "cpu_loc[0] %d  cpu_loc[1] %d\n", cpu_loc[0], cpu_loc[1]);

    /* access via cached and unmapped segment */
    firstp = start_addr;
    lastp = firstp + size;

    if (KDM_TO_PHYS(lastp) > MAX_PHYS_MEM) {
	msg_printf(ERR, "mpmem_rd: size argument is too large\n");
	return(1);
    }

    msg_printf(VRB, "Verify pattern 0x%x at 0x%x\n",
	    pattern, firstp);
    for (ptr = firstp; ptr <= lastp; ptr++) {
	if (*(uint *)ptr != pattern)
	{
	    msg_printf(ERR, "mpmem_rd: pattern mismatch at 0x%x Expect 0x%x Got 0x%x\n", ptr, pattern, *(uint *)ptr);
	    return(1);
	}
	count += sizeof(uint);
    }

    msg_printf(VRB, "mpmem_rd exits, 0x%x bytes initialized\n",count);
    return(0);

} /* fn mpmem_rd */
