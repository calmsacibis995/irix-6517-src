/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992-1996, Silicon Graphics, Inc.          *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * File: kdebug.c
 *	Functions to facilitate kernel debugging.
 */

#include <sys/types.h>
#include <sys/SN/SN0/IP27.h>
#include <sys/SN/arch.h>
#include <sys/SN/gda.h>
#include <sys/idbg.h>
#include "pod.h"
#include "ip27prom.h"
#include "libasm.h"
#include "symbol.h"
#include "kdebug.h"
#include <libkl.h>

static char *ksymoff = "\n*** Turning off kernel symbol table lookup\n";
static char *ksymon = "\n*** Turning on kernel symbol table lookup\n";
static char *ksymno = "\n*** Kernel symbol table information not available\n";

void
kdebug_syms_on(void)
{
    if (((get_BSR() & BSR_GDASYMS) == 0) &&
	(GDA != NULL) &&
	(GDA->g_magic == GDA_MAGIC) &&
	(GDA->g_dbstab != NULL)) {
	printf(ksymon);
	set_BSR(get_BSR() | BSR_GDASYMS);
    }
    else
	printf(ksymno);
}


void
kdebug_syms_off(void)
{
    if (get_BSR() & BSR_GDASYMS) {
	printf(ksymoff);
	set_BSR(get_BSR() & ~BSR_GDASYMS);
    }
}


void
kdebug_syms_toggle(void)
{
    if (get_BSR() & BSR_GDASYMS) {
	printf(ksymoff);
	set_BSR(get_BSR() & ~BSR_GDASYMS);
    }
    else {
	if ((GDA != NULL) && (GDA->g_dbstab != NULL)) {
	    printf(ksymon);
	    set_BSR(get_BSR() | BSR_GDASYMS);
	}
	else
	    printf(ksymno);
    }
}

void
kdebug_alt_regs(__uint64_t regs_num, struct flag_struct *flags)
{
    if (regs_num < MAXCPUS) 
	regs_num = KERN_NMI_ADDR(regs_num >> 1, regs_num & 1);
    else 
	regs_num = KERN_NMI_ADDR(get_nasid(), LD(LOCAL_HUB(PI_CPU_NUM)));
	
    printf("Using Alternate register set at 0x%x\n", regs_num);

    flags->alt_regs = regs_num;
    set_BSR(get_BSR() | BSR_KREGS);
}	


void
kdebug_idbg_printf(int (*prom_qprintf)(const char *, va_list))
{
    syminfo_t syminfo;
    void **idbgfunc_addr;
    syminfo.sym_name = "idbgfuncs";
    if (symbol_lookup_name(&syminfo) == -1) {
	printf("Could not find symbol idbgfuncs\n");
	return;
    }
    idbgfunc_addr = (void **)(*(__psunsigned_t *)syminfo.sym_addr);
    /*
     * The idbgfuncs struct is not available here. Hard code where qprintf
     * pointer lives and assign that to the proms printf
     */
    *(idbgfunc_addr + 9) = (void *)prom_qprintf;
    return;
}


void 
kdebug_init(__uint64_t stack)
{
    if (!stack) {
	stack = TO_NODE_CAC(get_nasid(), KERN_STACK_OFF);
	if (LD(LOCAL_HUB(PI_CPU_NUM)))
	    stack -= KERN_STACK_SIZE;
    }	
    set_BSR(get_BSR() | BSR_KDEBUG);
    pod_move_sp(stack);
}

void
kdebug_setup(struct flag_struct *flags)
{
    kdebug_syms_on();
    kdebug_alt_regs(-1, flags);
    kdebug_idbg_printf(vprintf);
}


