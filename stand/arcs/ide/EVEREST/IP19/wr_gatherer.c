/**************************************************************************
 *									  *
 * 		 Copyright (C) 1993, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/************************************************************************
 *									*
 *	wr_gatherer.c - IP19 write gatherer test                        *
 *									*
 ************************************************************************/

#ident "arcs/ide/EVEREST/IP19/wr_gatherer.c $Revision: 1.4 $"

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/IP19.h>
#include "everr_hints.h"
#include "ide_msg.h"
#include <ip19.h>
#include "prototypes.h"

static __psunsigned_t cpu_loc[2];

/* WG command address format:	 * EV_WGINPUT_BASE + command + optype   * 
				 * bits 0-2 - zeros                     * 
				 * bits 3-4 - operand type              * 
				 *	00 store cmd + 64bit data       * 
				 *	01 store cmd + lower 32bit data * 
				 *	10 store cmd + upper 32bit data * 
				 *	11 store cmd only               * 
				 * bits 5-19 - command                  */

#define	OP1W_MASK	0x18 	/* 1 word sent */

#define FLUSH_CMD	EV_WGINPUT_BASE + 0x1fe0
#define WGSTS_WG_PEND	0x20000
#define WGSTS_WG_A	0x10000
#define WG_ACOUNT	0x3f
#define WG_BCOUNT	0x3f00
#define WG_BCOUNT_SHFT	0x8
#define PHYS_DEST	0x800000	
#define PHYS_DESTMAX	0x8000ff	
#define CACHE0_LO	PHYS_DEST	
#define CACHE0_HI	0x800040	
#define CACHE1_LO	0x800080	
#define CACHE1_HI	0x8000c0	
#define MIXED_COUNT	32
#define DATA_COUNT	30
#define CMD_COUNT	17
#define ADR_COUNT	254

uint test_pat[] = {
	0x0,
	0x55555555,
	0x5a5a5a5a,
	0xaaaaaaaa,
	0xa5a5a5a5,
	0xffffffff,
	0xa5a5a5a5,
	0xaaaaaaaa,
	0x5a5a5a5a,
	0x55555555,
	0x0,
	0xffffffff,
	0xfec80137,
	0x0137fec8,
	0x8cef7310,
	0x73108cef,
};
uint exp_pat[] = {
	0x55555555,
	0x0,
	0x55555555,
	0x1,
	0x1,
	0x5a5a5a5a,
	0xaaaaaaaa,
	0x5a5a5a5a,
	0xaaaaaaaa,
	0x1,
	0x1,
	0xa5a5a5a5,
	0xffffffff,
	0xa5a5a5a5,
	0xffffffff,
	0x1,
	0x1,
	0xa5a5a5a5,
	0xaaaaaaaa,
	0xa5a5a5a5,
	0xaaaaaaaa,
	0x1,
	0x1,
	0x5a5a5a5a,
	0x55555555,
	0x5a5a5a5a,
	0x55555555,
	0x1,
	0x1,
	0x0,
	0xffffffff,
	0x0,
};

int
wr_gatherer ()
{
    uint i, j, tmpword, command, mem, retval, count;
    long long tmp, memaddr, data;
    uint * word;

    retval=0;
    getcpu_loc(cpu_loc);
    msg_printf(INFO, " (wr_gatherer) \n");
    msg_printf(VRB, "Running CPU %d on IP19 in slot %d\n", cpu_loc[1], cpu_loc[0]);

    if (console_is_gfx())
    {
	msg_printf(INFO, "Test skipped. Can run only on an ASCII console\n");
	return(0);
    }
    ta_spl();
    pte_setup();		/* set up page tables, k0a, and k1a */
    flushall_tlb();
    /* initialize memory with background data */
    for (i = PHYS_TO_K1(PHYS_DEST); i <= PHYS_TO_K1(PHYS_DESTMAX); i+=4)
    {
	*(uint *)i = 0xdeadbeef;
    }

    /* disable write gatherer */
    EV_SET_LOCAL(EV_WGCNTRL, (long long)2); 

    /* clear the write gatherer */
    EV_SET_LOCAL(EV_WGCLEAR, 0x0);

    /* set up the destination to be uncached memory
     *    bits 0-2   zeros
     *    bits 3-7   # of words sent % 32
     *    bits 8-39  destination address
     */
    memaddr = (long long)PHYS_DEST;
    EV_SET_LOCAL(EV_WGDST, memaddr);
    msg_printf(DBG, "Write gatherer destination register written with 0x%llx\n", memaddr);

    /* enable write gatherer */
    EV_SET_LOCAL(EV_WGCNTRL, 0x3);
    /*
     * read control register after enable to sync up the 2 different clocks
     * in the write gatherer
     */
    tmp = EV_GET_LOCAL(EV_WGCNTRL);
    msg_printf(DBG, "Write gatherer control register set to 0x%llx\n", tmp);

    /* do a commamd only stream, manual flush */
    count = 0;
    while (count < ADR_COUNT)
    {
	for (i = 0; i < CMD_COUNT; i++)
	{
	    if ((count + i + 1) > ADR_COUNT) break;
	    tmpword = (count + i + 1) << 5;
	    command = (EV_WGINPUT_BASE + tmpword) | OP1W_MASK;
	    data = ((long long) command << 32) | command;
	    EV_SET_LOCAL(command, data);
	    msg_printf(DBG, "WG command : 0x%x\n", command);
	}

	/* read status of write gatherer */
	tmp = EV_GET_LOCAL(EV_WGCOUNT);
	msg_printf(DBG, "Write gatherer status register after command write 0x%llx\n", tmp);

	/* flush the write gatherer now */
	EV_SET_LOCAL(FLUSH_CMD, 0);

	/* read status of write gatherer */
	tmp = EV_GET_LOCAL(EV_WGCOUNT);
	msg_printf(DBG, "Write gatherer status register after flush 0x%llx\n", tmp);

	/* check memory for data written */
	for (i = 0, word = (uint *)(PHYS_TO_K1(CACHE1_LO)); i <= CMD_COUNT; i++, word++)
	{
	    mem = *word;
	    if (i % 2)
		command = count + i;
	    else
		command = count + i + 2;
	    if ((i == (CMD_COUNT - 1)) || (command > ADR_COUNT)) continue;
	    if (mem != command)
	    {
		err_msg(WG_CMDERR, cpu_loc, word, command, mem);
		for (j = PHYS_TO_K1(CACHE1_LO); j <= PHYS_TO_K1(CACHE1_LO+0x7f); j+=4)
		{
		    msg_printf(ERR, "0x%x", *(uint *)j);
		}
		msg_printf(ERR, "\n", mem);
		retval = 1;
	    }
	}
	count += CMD_COUNT;
    }

    /* do a mixed command/data stream, automatic flush */
    for (i = 0; i <= 10; i++)
    {
	if (i == 0) 
	{
	    command = EV_WGINPUT_BASE + 0x2000;
	}
	else 
	{
	    command = EV_WGINPUT_BASE + 0x20;
	}
	data = ((long long)test_pat[i] << 32) | test_pat[i+1];
	EV_SET_LOCAL(command, data);
	msg_printf(DBG, "WG command : 0x%x  WG data : 0x%llx\n", command, data);
    }

    /* read status of write gatherer */
    tmp = EV_GET_LOCAL(EV_WGCOUNT);
    msg_printf(DBG, "Write gatherer status register after mixed write 0x%llx\n", tmp);

    /* check memory for data written */
    for (j = 0, word = (uint *)(PHYS_TO_K1(CACHE0_LO)); j < MIXED_COUNT; j++, word++)
    {
	mem = *word;
	command = exp_pat[j];
	if (mem != command)
	{
	    err_msg(WG_MIXERR, cpu_loc, word, command, mem);
	    retval = 1;
	}
    }

    /* do a data only stream, automatic flush */
    for (i = 0; i < DATA_COUNT/2; i++)
    {
	data = ((long long)test_pat[i] << 32) | test_pat[i+1];
	EV_SET_LOCAL(EV_WGINPUT_BASE + 0x2000, data);
	msg_printf(DBG, "WG data : 0x%llx\n", data);
    }

    /* read status of write gatherer */
    tmp = EV_GET_LOCAL(EV_WGCOUNT);
    msg_printf(DBG, "Write gatherer status register after data write 0x%llx\n", tmp);

    /* check memory for data written */
    for (j = 0, word = (uint *)(PHYS_TO_K1(CACHE1_HI)); j < 8; j++, word++)
    {
	mem = *word;
	tmpword = test_pat[j+1];
	if (mem != tmpword)
	{
	    err_msg(WG_DATAERR, cpu_loc, word, tmpword, mem);
	    retval = 1;
	}
	word++;
	mem = *word;
	tmpword = test_pat[j];
	if (mem != tmpword)
	{
	    err_msg(WG_DATAERR, cpu_loc, word, tmpword, mem);
	    retval = 1;
	}
    }
    for (j = 8, word = (uint *)(PHYS_TO_K1(CACHE1_LO)); j < DATA_COUNT/2; j++, word++)
    {
	mem = *word;
	tmpword = test_pat[j+1];
	if (mem != tmpword)
	{
	    err_msg(WG_DATAERR, cpu_loc, word, tmpword, mem);
	    retval = 1;
	}
	word++;
	mem = *word;
	tmpword = test_pat[j];
	if (mem != tmpword)
	{
	    err_msg(WG_DATAERR, cpu_loc, word, tmpword, mem);
	    retval = 1;
	}
    }

    /* disable write gatherer */
    EV_SET_LOCAL(EV_WGCNTRL, (long long)2); 

    msg_printf(INFO, "Completed IP19 write gatherer test\n");
    return (retval);
}

