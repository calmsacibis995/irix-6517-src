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
 *	dang_gr2write.c - dang chip gio bus write utility		*
 *									*
 ************************************************************************/

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/fault.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/addrs.h>
#include <sys/EVEREST/evmp.h>
#include <sys/EVEREST/io4.h>
#include <sys/EVEREST/dang.h>
#include <sys/EVEREST/evconfig.h>
#include <setjmp.h>
#include <uif.h>
#include <io4_tdefs.h>
#include <everr_hints.h>
#include "danglib.h"

extern jmp_buf dangbuf;

static int loc[3];

int
dang_gr2write(int argc, char** argv)
{
    unsigned int gio_address, pattern;
    int window;
    volatile long long * adptr;
    struct gr2_hw * gio_va;
    gio_va = NULL;

    msg_printf(INFO, "\ndang_gr2write - DANG/GIO Gr2 data write utility\n");

    if (console_is_gfx())
    {
	msg_printf(INFO, "Test skipped. Can run only on an ASCII console\n");
	return(TEST_SKIPPED);
    }

    if (argc != 5) 
    {
	msg_printf(ERR,"dang_gr2write: bad command line format\n");
	msg_printf(VRB,"usage:\n");
	msg_printf(VRB,"\tdang_gr2write slot# adap# gio_address pattern\n");
	return (1);
    }

    /*
     * if bad command line, exit
     */
    if (io4_select (TRUE, argc, argv))
	return(1);

    /*
     * get the gio address and the pattern to write
     */
    atob(argv[3], &gio_address);
    atob(argv[4], &pattern);

    /*
     * get the window # of the current io board
     */
    window = io4_window(io4_tslot);
    adptr = (long long *) SWIN_BASE(window, io4_tadap);

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
	setup_err_intr(io4_tslot, io4_tadap);
	clear_nofault();
    }

    /*
     * initialize the dang chip and the large window virtual map to access it
     */
    if (!(gio_va = (struct gr2_hw *)ide_dang_init(io4_tslot, io4_tadap))) {
	msg_printf(VRB,"unable to initialize dang & buffers\n");
	return(TEST_SKIPPED);
    }
    else if (!ide_Gr2Probe((struct gr2_hw *)gio_va)) {
	msg_printf(VRB,"unable to locate Gr2 at address 0x%lx\n",
		(long) gio_va);
	return(TEST_SKIPPED);
    }

    ((unsigned int *)gio_va)[gio_address] = pattern;

    return (0);
}
