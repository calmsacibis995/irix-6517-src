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

#ifndef __FRAME_DEBUG_H__
#define __FRAME_DEBUG_H__

/*
 * Debug codes
 */

#define DC_CREATE      0
#define DC_DESTROY     1
#define DC_ENQUEUE     2
#define DC_JOIN        3
#define DC_START       4
#define DC_YIELD       5
#define DC_UEINTR      6
#define DC_GETQLEN     7
#define DC_READQ       8
#define DC_REMOVEQ     9
#define DC_PINSERTQ    10
#define DC_STOP        11
#define DC_RESUME      12
#define DC_SETATTR     13
#define DC_GETATTR     14
#define DC_STEP        15
#define DC_REF         16
#define DC_CHOOSEP     17
#define DC_REFPROCD    18
#define DC_EVINTR      19
#define DC_DISP        20

/*
 * Debug Masks
 */

#define DC_TO_DM(code) (0x1 << (code))

#define DM_ALL         (~0x0)

#define DM_CREATE      DC_TO_DM(DC_CREATE)
#define DM_DESTROY     DC_TO_DM(DC_DESTROY)
#define DM_ENQUEUE     DC_TO_DM(DC_ENQUEUE)
#define DM_JOIN        DC_TO_DM(DC_JOIN)
#define DM_START       DC_TO_DM(DC_START)
#define DM_YIELD       DC_TO_DM(DC_YIELD)
#define DM_UEINTR      DC_TO_DM(DC_UEINTR)
#define DM_GETQLEN     DC_TO_DM(DC_GETQLEN)
#define DM_READQ       DC_TO_DM(DC_READQ)
#define DM_REMOVEQ     DC_TO_DM(DC_REMOVEQ)
#define DM_PINSERTQ    DC_TO_DM(DC_PINSERTQ)
#define DM_STOP        DC_TO_DM(DC_STOP)
#define DM_RESUME      DC_TO_DM(DC_RESUME)
#define DM_SETATTR     DC_TO_DM(DC_SETATTR)
#define DM_GETATTR     DC_TO_DM(DC_GETATTR)
#define DM_STEP        DC_TO_DM(DC_STEP)
#define DM_REF         DC_TO_DM(DC_REF)
#define DM_CHOOSEP     DC_TO_DM(DC_CHOOSEP)
#define DM_REFPROCD    DC_TO_DM(DC_REFPROCD)
#define DM_EVINTR      DC_TO_DM(DC_EVINTR)
#define DM_DISP        DC_TO_DM(DC_DISP)
#ifdef DEBUG

typedef void (*df_t)(void*);

extern void frs_debug_selector(uint op_mask, int level, df_t f, void* arg);
extern void frs_message(uint op_mask, int level, char* message, long long value);

extern void frs_fsched_info_print(frs_fsched_info_t* info);
extern void frs_fsched_print(frs_fsched_t* frs);
extern void frs_queue_info_print(frs_queue_info_t* info);
extern void frs_attr_info_print(frs_attr_info_t* info);
extern void frs_fsched_idesc_print(intrdesc_t* idesc);
extern void frs_fsched_sdesc_print(syncdesc_t* sdesc);
extern void frs_fsched_sigdesc_print(sigdesc_t* sigdesc);
extern void frs_procd_print(frs_procd_t* procd);

#define FDBG(op, level, f, arg)        frs_debug_selector((op), (level), (df_t)(f), (arg))
#define frs_debug_message(w, x, y, z)  frs_message((w), (x), (y), (z))

#else /* DEBUG */

#define FDBG(op, level, f, arg)
#define frs_debug_message(w, x, y, z)
extern void frs_fsched_print(frs_fsched_t* frs);

#endif /* DEBUG */


#define frs_debug_fsched_info_print(x, y, z)    FDBG((x), (y), frs_fsched_info_print, (z))
#define frs_debug_fsched_print(x, y, z)         FDBG((x), (y), frs_fsched_print, (z))
#define frs_debug_queue_info_print(x, y, z)     FDBG((x), (y), frs_queue_info_print, (z))
#define frs_debug_attr_info_print(x, y, z)      FDBG((x), (y), frs_attr_info_print, (z))
#define frs_debug_fsched_idesc_print(x, y, z)   FDBG((x), (y), frs_fsched_idesc_print, (z))
#define frs_debug_fsched_sdesc_print(x, y, z)   FDBG((x), (y), frs_fsched_sdesc_print, (z))
#define frs_debug_fsched_sigdesc_print(x, y, z) FDBG((x), (y), frs_fsched_sigdesc_print, (z))
#define frs_debug_procd_print(x, y, z)          FDBG((x), (y), frs_procd_print, (z))


#endif /* __FRAME_DEBUG_H__ */

