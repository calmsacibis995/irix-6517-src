/*
 * IP19/mp/mpchk_cstate.c
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

#ident "$Revision: 1.3 $"

#include <sys/cpu.h>
#include <sys/types.h>
#include <sys/sbd.h>
#include "libsc.h"
#include "ide_msg.h"
#include "everr_hints.h"
#include "ip19.h"
#include "cache.h"
#include "prototypes.h"

/* Other secondary cache states are defined in sbd.h */

#define SSHARED		0x00001800
#define SDIRTYSHARED	0x00001c00

extern int read_tag(int, __psunsigned_t, __psunsigned_t);
static __psunsigned_t cpu_loc[2];
static uint check_state(__psunsigned_t, uint);

char cstate0[] = "INVALID";
char cstate1[] = "CLEAN EXCLUSIVE";
char cstate2[] = "DIRTY EXCLUSIVE";
char cstate3[] = "SHARED";
char cstate4[] = "DIRTY SHARED";

char *cstate[] = { cstate0, cstate1, cstate2, cstate3, cstate4,};
uint
mpchk_cstate(int argc, char **argv)
{
    uint physaddr, fail=0;
    uint expectedstate, cachestate;

    msg_printf(DBG,"argc is %d\n", argc);
    msg_printf(DBG,"argv is %s\n", *argv);

    if (argc != 3)
    {
        msg_printf(SUM,"Usage: mpchk_cstate CacheState 0xPhysicalAddress\n");
        msg_printf(SUM,"       CacheState=0 is Invalid\n");
	msg_printf(SUM,"       CacheState=1 is Clean Exclusive\n");
	msg_printf(SUM,"       CacheState=2 is Dirty Exclusive\n");
	msg_printf(SUM,"       CacheState=3 is Shared\n");
	msg_printf(SUM,"       CacheState=4 is Dirty Shared\n");
	return(1);
    }
    else
    {
        if (atob(argv[1], (int *)&cachestate) == 0) {
            msg_printf(SUM, "Unable to understand the cache state argument\n");
            return (1);
        }
        if (atob(argv[2], (int *)&physaddr) == 0) {
            msg_printf(SUM, "Unable to understand the address argument\n");
            return (1);
        }
    }
    msg_printf(DBG, "cache state is %d\n", cachestate);

    switch (cachestate)
    {
	case 0:
		expectedstate=SINVALID;
		break;
	case 1:
		expectedstate=SCLEANEXCL;
		break;
        case 2:
                expectedstate=SDIRTYEXCL;
                break;
        case 3:
                expectedstate=SSHARED;
                break;
        case 4:
                expectedstate=SDIRTYSHARED;
                break;
	default:
		msg_printf(SUM, "Unable to understand the cache state argument\n");
            	return (1);
    }

    getcpu_loc(cpu_loc);

    fail = check_state(physaddr, expectedstate);
    if (fail)
	msg_printf(ERR, "%s is the expected state\n", cstate[cachestate]);
    msg_printf(VRB, "mpchk_cstate exits cpu %d of slot %d\n", cpu_loc[1], cpu_loc[0]);

    return(fail);

}

static uint
check_state(__psunsigned_t addr, uint expect)
{
    struct tag_regs tags;   /* taghi and taglo regs */
    uint state, physaddr;
    uint expectedstate, actualstate;

    read_tag(SECONDARY, addr, (__psunsigned_t)&tags);
    state = ((tags.tag_lo & SSTATEMASK) >> SSTATE_RROLL);
    physaddr = (tags.tag_lo&SADDRMASK) << TAGADDRLROLL;
    msg_printf(VRB, "SEC: taglo 0x%x; state 0x%x, paddr 0x%x\n",
			tags.tag_lo,state,physaddr);
    actualstate = state;
    expectedstate = ((expect & SSTATEMASK) >> SSTATE_RROLL);
    if (expectedstate != actualstate) 
    {
	msg_printf(ERR, "mpchk_cstate: Secondary cache state at addr %x Expected %d Got %d\n", 
		physaddr, expectedstate, actualstate); 
	return(1);
    }
    return(0);
}
