/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992-1997, Silicon Graphics, Inc.          *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#include <sys/types.h>
#include <sys/cmn_err.h>
#include <sys/arcs/debug_block.h>
#include <sys/arcs/spb.h>
#include <sys/i8251uart.h>
#include <sys/SN/agent.h>
#include <sys/systm.h>
#include "sn_private.h"

#if SABLE

void
du_earlyinit()
{
	static int inited = 0;

	if (inited)
		return;

	/* Reset the UART */
	LOCAL_HUB_S(MD_UREG0_0, 0);
	LOCAL_HUB_S(MD_UREG0_0, 0);
	LOCAL_HUB_S(MD_UREG0_0, 0);
	LOCAL_HUB_S(MD_UREG0_0, I8251_RESET);

	/* Set the configuration */
	LOCAL_HUB_S(MD_UREG0_0, (I8251_ASYNC16X | I8251_NOPAR | I8251_8BITS | 
			        I8251_STOPB1));
	LOCAL_HUB_S(MD_UREG0_0, (I8251_TXENB | I8251_RXENB | I8251_RESETERR));
	
	inited = 1;
}

#endif /* SABLE */

/*
 * On SN0, unix overlays the prom's bss area.  Therefore, only
 * those prom entry points that do not use prom bss or are 
 * destructive (i.e. reboot) may be called.  There is however,
 * a period of time before the prom bss is overlayed in which
 * the prom may be called.  After unix wipes out the prom bss
 * and before the console is inited, there is a very small window
 * in which calls to dprintf will cause the system to hang.
 *
 * If symmon is being used, it will plug a pointer to its printf
 * routine into the debug block which the kernel will then use
 * for dprintfs.  The advantage of this is that the kp command
 * will not interfere with the kernel's own print buffers.
 *
 */
# define ARGS	 a0,a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15
void
dprintf(fmt,ARGS)
char		*fmt;
__psint_t	ARGS;
{
	/*
	 * Check for presence of symmon
	 */
	if (SPB->Signature == SPBMAGIC && 
	    SPB->DebugBlock &&
	    ((db_t *)SPB->DebugBlock)->db_magic == DB_MAGIC &&
	    ((db_t *)SPB->DebugBlock)->db_printf)
		(*((db_t *)SPB->DebugBlock)->db_printf)(fmt, ARGS);
	else
/* XXX - Fix me when we have a real UART. */
	/*
	 * cn_is_inited() implies that PROM bss has been wiped out
	 * On the other hand, pdaindr[getcpuid()].CpuId != CpuId
	 * implies that we're not ready to user cmn_err(CE_PANIC)...
	 */
	if (cn_is_inited() && (pdaindr[getcpuid()].CpuId == getcpuid())
#if MP && !SABLE 
             && spinlock_initialized(&putbuflck)
#endif
	)
		cmn_err(CE_CONT,fmt,ARGS);
	else
		arcs_printf (fmt,ARGS);
}

