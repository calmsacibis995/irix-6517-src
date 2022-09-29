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
/*
 * sn0.h -- hardware specific defines for sn0 boards
 * The defines used here are used to limit the size of 
 * various datastructures in the PROM. eg. KLCFGINFO, MPCONF etc.
 */

#ifndef __SYS_SN_SN0_H__
#define __SYS_SN_SN0_H__

#ident "$Revision: 1.8 $"

extern xwidgetnum_t hub_widget_id(nasid_t);
extern nasid_t get_nasid(void);
extern int	get_slice(void);
extern int     is_fine_dirmode(void);
extern hubreg_t get_hub_chiprev(nasid_t nasid);
extern hubreg_t get_region(cnodeid_t);
extern hubreg_t nasid_to_region(nasid_t);
extern int      verify_snchip_rev(void);
extern void 	ni_reset_port(void);

#ifdef SN0_USE_POISON_BITS
extern int hub_bte_poison_ok(void);
#endif /* SN0_USE_POISON_BITS */

#endif /* __SYS_SN_SN0_H__ */

