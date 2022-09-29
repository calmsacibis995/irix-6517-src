/*
 * IP19/mp/mpchk_cache.c
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
#include <setjmp.h>
#include <fault.h>
#include <libsc.h>
#include "ide_msg.h"
#include "everr_hints.h"
#include "ip19.h"
#include "pattern.h"
#include "prototypes.h"
 
#define STARTADDR BASE1
#define MAX1MBADDR 	(STARTADDR+0x100000)
#define MAX2MBADDR 	(STARTADDR+0x200000)
#define MAX3MBADDR 	(STARTADDR+0x300000)
#define MAX4MBADDR 	(STARTADDR+0x400000)
#define TAGMASK_1M 0xfffffc
#define TAGMASK_4M 0xfffff0

void warn_msg(void);
static __psunsigned_t cpu_loc[2];
static jmp_buf fault_buf;

char jumper_cpu0[] = "F3H5 and F7H8";
char jumper_cpu1[] = "F8M5 and F9M6";
char jumper_cpu2[] = "G9I5 and G9I6";
char jumper_cpu3[] = "H2D7 and H2G8";

char *jumpers[] = { jumper_cpu0, jumper_cpu1, jumper_cpu2, jumper_cpu3, };
uint size_mb; 

/* mpchk_cache()
 * CHECK SCACHE SIZE:
 * 	1. write address compliment to first 2MB of uncache memory  
 * 	2. read first MB of cached memory to fill scache 
 * 	3. write address to first MB of cached memory to dirty scache 
 * 	4. read a location from the 2nd MB of cached memory to cause 
 *	   a cache line replacement for 1MB scache 
 * 	5. check whether first MB uncache memory is written back with address 
 * 	6. if so, 1MB scache is present 
 * 	7. if not, can we assume that 4MB scache is present? 
 *	8. check if it agrees with scache size in EAROM
 * CHECK JUMPERS FOR BUS TAG RAM:
 * 	1. write address data to bus tag RAM assuming 4MB scache size 
 * 	2. verify contents of bus tag RAM assuming 1MB scache size 
 * 	3. if data doesn't match, jumpers are installed 
 */
uint
mpchk_cache(int argc, char **argv)
{
    register __psunsigned_t firstp, lastp, osr, ptr;
    register uint tag_size;
    uint fail, act_mb;
    __psunsigned_t savedata, max_tagaddr, data;

    getcpu_loc(cpu_loc);
    msg_printf(INFO, " (mpchk_cache) \n");

    msg_printf(VRB,"argc is %d\n", argc);
    msg_printf(VRB,"argv is %s\n", *argv);

    if (argc != 2)
    {
	msg_printf(ERR,"Usage: mpchk_cache sizeinMB\n");
	return(1);
    }
    else
    {
	if (atob(argv[1], (int *)&size_mb) == 0) {
	    msg_printf(ERR, "Unable to understand the sizeinMB argument\n");
	    return (1);
	}
    }
    msg_printf(VRB, "size_mb is %d\n", size_mb);

    osr = get_sr();
    set_sr(osr & SR_DE);
    ide_invalidate_caches();

    if (setjmp(fault_buf))
    {
	err_msg(MP_EXCP, cpu_loc);
	data = EV_GET_REG(EV_CACHE_SZ_LOC);
	if (data == 0x14)
	    msg_printf(ERR, "Scache size in EAROM currently set to: 0x%x (1M)\n\n", data);
	else if (data == 0x16)
	    msg_printf(ERR, "Scache size in EAROM currently set to: 0x%x (4M)\n\n", data);
	else
	    msg_printf(ERR, "Scache size in EAROM currently set to: 0x%x (??)\n\n", data);
	warn_msg();
	return(2);
    }
    else
    {
	set_nofault(fault_buf);
    firstp = PHYS_TO_K1(STARTADDR);
    lastp = PHYS_TO_K1(MAX4MBADDR);

    msg_printf(VRB, "Write address data to 4MB of uncached memory: 0x%x to 0x%x-1\n", firstp, lastp);

    for (ptr = firstp; ptr < lastp; ptr++) {
	*(uint *)ptr = (uint)ptr;
    }

    firstp = PHYS_TO_K0(STARTADDR);
    lastp = PHYS_TO_K0(MAX1MBADDR);

    msg_printf(VRB, "Fill 1MB of cached memory and dirty it: 0x%x to 0x%x-1\n", firstp, lastp);

    for (ptr = firstp; ptr < lastp; ptr++) {
	if (*(uint *)ptr != (uint)K0_TO_K1(ptr))
	{
	    err_msg(MP_CDATA, cpu_loc, ptr, (uint)K0_TO_K1(ptr), *(uint *)ptr);
	    fail = 1;
	    goto EXIT1;
	}
	else
	{
	    *(uint *)ptr = ~(uint)ptr;
	}
    }

    msg_printf(VRB, "Read from 2nd MB of cached memory to cause possible cacheline replacement\n");

    ptr = PHYS_TO_K0(MAX2MBADDR);
    if (*(uint *)ptr != (uint)K0_TO_K1(ptr))
    {
	err_msg(MP_CDATA, cpu_loc, ptr, (uint)K0_TO_K1(ptr), *(uint *)ptr);
	fail = 1;
	goto EXIT1;
    }

    msg_printf(VRB, "Check for write back to first MB of uncached memory\n");

    firstp = PHYS_TO_K1(STARTADDR);
    lastp = PHYS_TO_K1(MAX1MBADDR);

    for (ptr = firstp; ptr < lastp; ptr++) {
	if (*(uint *)ptr == ~(uint)(K1_TO_K0(ptr)))
	{
	    msg_printf(VRB, "Write back occured at 0x%x Got 0x%x\n", ptr, *(uint *)ptr);
	    msg_printf(VRB, "Scache size on CPU %d in slot %d is verified to be 1 MB\n", cpu_loc[1], cpu_loc[0]);
	    fail = 0;
	    if (size_mb != 1)
	    {
		data = EV_GET_REG(EV_CACHE_SZ_LOC);
		if (data != 0x16) {
		    msg_printf(ERR, "Writing 0x16 to 0xb9000128\n");
		    EV_SET_REG(EV_CACHE_SZ_LOC, 0x16);
		}
		act_mb = 1;
		err_msg(MP_CSIZE, cpu_loc, act_mb, size_mb);
		warn_msg();
		return(2);
	    }
	    goto EXIT1;
	}
	else
	{
	    if (*(uint *)ptr != (uint)ptr)
	    {
		err_msg(MP_CDATA, cpu_loc, ptr, ptr, *(uint *)ptr);
	    }
	}
    }

    msg_printf(VRB, "Fill 3 more MB of cached memory and dirty it\n");

    firstp = PHYS_TO_K0(MAX1MBADDR);
    lastp = PHYS_TO_K0(MAX4MBADDR);

    for (ptr = firstp; ptr < lastp; ptr++) {
	if (*(uint *)ptr != (uint)K0_TO_K1(ptr))
	{
	    err_msg(MP_CDATA, cpu_loc, ptr, (uint)K0_TO_K1(ptr), *(uint *)ptr);
	    fail = 1;
	    goto EXIT1;
	}
	else
	{
	    *(uint *)ptr = ~(uint)ptr;
	}
    }

    msg_printf(VRB, "Read from 5th MB of cached memory to cause possible cacheline replacement\n");

    ptr = PHYS_TO_K0(MAX4MBADDR);
    data = *(uint *)ptr;
    msg_printf(VRB, "cachesz: pattern at 0x%x is 0x%x\n", ptr, data);

    msg_printf(VRB, "Check for write back to first 4MB of uncached memory\n");

    firstp = K0_TO_K1(STARTADDR);
    lastp = K0_TO_K1(MAX4MBADDR);

    for (ptr = firstp; ptr < lastp; ptr++) {
	if (*(uint *)ptr == ~(uint)(K1_TO_K0(ptr)))
	{
	    msg_printf(VRB, "Write back occured at 0x%x Got 0x%x\n", ptr, *(uint *)ptr);
	    msg_printf(VRB, "Scache size on CPU %d in slot %d is verified to be 4 MB\n", cpu_loc[1], cpu_loc[0]);
	    fail = 0;
	    if (size_mb != 4)
	    {
		data = EV_GET_REG(EV_CACHE_SZ_LOC);
		if (data != 0x14) {
		    msg_printf(ERR, "Writing 0x14 to 0xb9000128\n");
		    EV_SET_REG(EV_CACHE_SZ_LOC, 0x14);
		}
		act_mb = 4;
		err_msg(MP_CSIZE, cpu_loc, act_mb, size_mb);
		warn_msg();
		return(2);
	    }
	    else
		break;
	}
	else
	{
	    if (*(uint *)ptr != (uint)ptr)
	    {
		err_msg(MP_CDATA, cpu_loc, ptr, ptr, *(uint *)ptr);
		fail = 1;
		break;
	    }
	}
    }

    msg_printf(VRB, "Write to bus tag RAM using jumpers\n");

    tag_size = calc_bustag_size();
    msg_printf(VRB, "bus tag size: 0x%x\n", tag_size);
    max_tagaddr = EV_BUSTAG_BASE + tag_size - 8;

	savedata = EV_GET_REG(max_tagaddr);
	EV_SET_REG(max_tagaddr, 0x5a5a5a5a);
	data = EV_GET_REG(max_tagaddr);
	msg_printf(VRB, "Data read from max_tagaddr 0x%x is 0x%x\n", max_tagaddr, data);
	EV_SET_REG(max_tagaddr, savedata);

	if (data != (0x5a5a5a5a & TAGMASK_1M))
	{
	    if (data == (0x5a5a5a5a & TAGMASK_4M))
	    {
		msg_printf(INFO, "Bus tag address jumpers are installed for 4MB cache\n");
		fail = 0;
	    }
	    else
	    {
		err_msg(MP_JUMPER, cpu_loc);
		msg_printf(ERR, "Bus tag RAM type may not be compatible with 4MB cache\n");
		fail = 1;
	    }
	}
    }
EXIT1:
    clear_nofault();
    invalidate_caches();
    if (fail == 0) set_sr(osr);
    return(fail);
} /* fn mpchk_cache */

void warn_msg(void)
{
    __psunsigned_t thiscpu;

    thiscpu = cpu_loc[1];
    msg_printf(ERR, "\n****The system must be RESET after this failure ****\n");
    if (size_mb == 4)
    {
	msg_printf(ERR, "****Make sure jumpers %s are installed for bus tags****\n\n", jumpers[thiscpu]);
    }
}
