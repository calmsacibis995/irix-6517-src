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
 *	dang_gr2ram.c - dang chip/gio gr2 shared ram test		*
 *									*
 *	the primary goal of this test is to exercise the gio bus	*
 *	address and data lines;  the gr2 graphics shared ram area	*
 *	is a means to this end						*
 *									*
 ************************************************************************/

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/fault.h>
#include <fault.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/evintr.h>
#include <sys/EVEREST/everror.h>
#include <sys/EVEREST/addrs.h>
#include <sys/EVEREST/evmp.h>
#include <sys/EVEREST/io4.h>
#include <sys/EVEREST/dang.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/gr2.h>
#include <sys/gr2hw.h>
#include <setjmp.h>
#include <uif.h>
#include <io4_tdefs.h>
#include <everr_hints.h>
#include <setjmp.h>
#include "danglib.h"

/* size of the shared ram buffer in words */
#define SHRAM_SIZE	32768
#define NUMPATS		6

extern jmp_buf dangbuf;

static unsigned int testpat[NUMPATS] =
{
    0, 0xFFFFFFFF, 0x55555555, 0xAAAAAAAA, 0xA5A5A5A5, 0x5A5A5A5A
};

static int ide_dang_gr2ram(int, int);
static int loc[3];

int
dang_gr2ram(int argc, char** argv)
{
    int retval;

    msg_printf(INFO, "\ndang_gr2ram - dang chip/gio gr2 shared ram test\n");

    if (console_is_gfx())
    {
	msg_printf(INFO, "Test skipped. Can run only on an ASCII console\n");
	return(TEST_SKIPPED);
    }
    retval = test_adapter(argc, argv, IO4_ADAP_DANG, ide_dang_gr2ram);

    return (retval);
}


static int ide_dang_gr2ram(int slot, int adap)
{
    int window, retval, pattern, index;
    volatile long long * adptr;
    struct gr2_hw * gio_va;
    register unsigned int curpat;
    register unsigned int *uptr;

    retval = 0;
    gio_va = NULL;
    loc[0] = slot; loc[1] = adap; loc[2] = -1;
    

    /*
     * get the window # of the current io board
     */
    window = io4_window(slot);
    adptr = (long long *) SWIN_BASE(window, adap);

    msg_printf(INFO, "dang_gr2ram: looking at slot %x, adap %x\n", slot, adap);

    /*
     * Get the current interrupt mask contents and disable all interrupt
     * sources - if cannot disable interrupts, exit
     */
    if (setjmp(dangbuf)) {
	msg_printf(ERR, "Exception Accessing DANG chip!\n");
	clear_nofault();
	return (1);
    }
    else {
	set_nofault(dangbuf);
	setup_err_intr(slot, adap);
	clear_nofault();
    }

    /*
     * initialize the dang chip and the large window virtual map to access it
     */
    if (!(gio_va = (struct gr2_hw *)ide_dang_init(slot, adap))) {
	msg_printf(VRB,"\tunable to initialize dang & buffers\n");
	return(TEST_SKIPPED);
    }
    else if (!ide_Gr2Probe((struct gr2_hw *)gio_va)) {
	msg_printf(VRB,"\tunable to locate Gr2 at address 0x%lx\n",
		(long) gio_va);
	return(TEST_SKIPPED);
    }

    /*
     * go through simple test patterns - look for stuck bits, etc
     */
    msg_printf(VRB,"\tsimple pattern test\n");
    for (pattern = 0; pattern < NUMPATS; pattern++)
    {
	curpat = testpat[pattern];

	uptr = (unsigned int *)&gio_va->shram[0];
	for (index = 0; index < SHRAM_SIZE; index++)
	    *uptr++ = curpat;

	uptr = (unsigned int *)&gio_va->shram[0];
	for (index = 0; index < SHRAM_SIZE; index++, uptr++)
	{
	    if (*uptr != curpat)
	    {
		err_msg(DANG_BADSHRAM, loc, index, curpat, *uptr);
		retval++;
	    }
	}
    }

    /*
     * do marching ones test - look for tied bits
     */
    msg_printf(VRB, "\tmarching ones test\n");
    for (pattern = 0; pattern < 32 ; pattern++)
    {
	curpat = 1 << pattern;

	uptr = (unsigned int *)&gio_va->shram[0];
	for (index = 0; index < SHRAM_SIZE; index++)
	    *uptr++ = curpat;

	uptr = (unsigned int *)&gio_va->shram[0];
	for (index = 0; index < SHRAM_SIZE; index++, uptr++)
	{
	    if (*uptr != curpat)
	    {
		err_msg(DANG_BADSHRAM, loc, index, curpat, *uptr);
		retval++;
	    }
	}
    }

    /*
     * do marching zeros test - look for tied bits
     */
    msg_printf(VRB, "\tmarching zeros test\n");
    for (pattern = 0; pattern < 32 ; pattern++)
    {
	curpat = ~(1 << pattern);

	uptr = (unsigned int *)&gio_va->shram[0];
	for (index = 0; index < SHRAM_SIZE; index++)
	    *uptr++ = curpat;

	uptr = (unsigned int *)&gio_va->shram[0];
	for (index = 0; index < SHRAM_SIZE; index++, uptr++)
	{
	    if (*uptr != curpat)
	    {
		err_msg(DANG_BADSHRAM, loc, index, curpat, *uptr);
		retval++;
	    }
	}
    }

    /*
     * do address-in-address test - check address lines
     */
    msg_printf(VRB, "\taddress-in-address test\n");

    uptr = (unsigned int *)&gio_va->shram[0];
    for (index = 0; index < SHRAM_SIZE; index++)
	*uptr++ = index;

    uptr = (unsigned int *)&gio_va->shram[0];
    for (index = 0; index < SHRAM_SIZE; index++, uptr++)
    {
	if (*uptr != index)
	{
	    err_msg(DANG_BADSHRAM, loc, index, index, *uptr);
	    retval++;
	}
    }

    /*
     * do inverse address-in-address test - check address lines
     */
    msg_printf(VRB, "\tinverse address-in-address test\n");

    uptr = (unsigned int *)&gio_va->shram[0];
    for (index = 0; index < SHRAM_SIZE; index++)
	*uptr++ = ~index;

    uptr = (unsigned int *)&gio_va->shram[0];
    for (index = 0; index < SHRAM_SIZE; index++, uptr++)
    {
	if (*uptr != ~index)
	{
	    err_msg(DANG_BADSHRAM, loc, index, ~index, *uptr);
	    retval++;
	}
    }

    return (retval);
}
