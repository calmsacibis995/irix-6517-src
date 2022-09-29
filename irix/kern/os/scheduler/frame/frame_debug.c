/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#if (defined(EVEREST) || defined(SN0) || defined(IP30)) && defined(DEBUG)

#include <sys/types.h>
#include <sys/proc.h>
#include <sys/kmem.h>
#include <sys/runq.h>
#include <sys/sysmp.h>
#include <sys/schedctl.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/pda.h>
#include <sys/errno.h>
#include <sys/systm.h>
#include <sys/atomic_ops.h>
#include <sys/rtmon.h>
#include <sys/frs.h>

#include "frame_barrier.h"
#include "frame_semabarrier.h"
#include "frame_base.h"
#include "frame_cpu.h"
#include "frame_process.h"
#include "frame_procd.h"
#include "frame_minor.h"
#include "frame_major.h"
#include "frame_sched.h"
#include "frame_debug.h"

static uint op_debug_mask = DM_ALL;
static int  op_debug_level[] = {
        2,  /* DC_CREATE  */
        2,  /* DC_DESTROY */
        0,  /* DC_ENQUEUE */
        2,  /* DC_JOIN    */
        2,  /* DC_START   */
        0,  /* DC_YIELD   */
        2,  /* DC_UEINTR  */
        2,  /* DC_GETQLEN */
        2,  /* DC_READQ   */
        2,  /* DC_REMOVEQ */
        2,  /* DC_PINSERTQ */
        2,  /* DC_STOP     */
        2,  /* DC_RESUME   */
        2,  /* DC_SETATTR  */
        2,  /* DC_GETATTR  */
        0,  /* DC_STEP     */
        0,  /* DC_REF      */
        0,  /* DC_CHOOSEP  */
        0,  /* DC_REFPROCD */
        0,  /* DC_EVINTR   */
        2,  /* DC_DISP     */
        0
};

static int
mask_to_code(uint mask)
{
        int i;

        ASSERT (mask != 0);
        
        for (i = 0; mask != 0x1; i++) {
                mask >>= 1;
        }

        return (i);
}

void
frs_message(uint op_mask, int level, char* message, long long value)
{
        if (op_mask & op_debug_mask) {
                if (op_debug_level[mask_to_code(op_mask)] >= level) {
                        printf("<FRS> %s [0x%x]\n", message, value);
                }
        }
}

void
frs_debug_selector(uint op_mask, int level, df_t f, void* arg)
{
        if (op_mask & op_debug_mask) {
                if (op_debug_level[mask_to_code(op_mask)] >= level) {
                        (*f)(arg);
                }
        }
}
         

#endif /* EVEREST || SN0 */

