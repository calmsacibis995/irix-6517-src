/**************************************************************************
 *									  *
 * 		 Copyright (C) 1994 Silicon Graphics, Inc.	  	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.6 $"

#ifndef	_TLB_H_
#define	_TLB_H_

#if _LANGUAGE_C

/* Generic operations */

void	tlb_dropin(ulong asid, ulong vaddr,
		 ulong pte0, ulong pte1, ulong pgmask);
ulong	tlb_get_enhi(int index);
ulong	tlb_get_enlo(int index, int entry);
void	tlb_flush(void);

/* Specialized operations for IP27 */

void	tlb_prom(void);
void	tlb_ram_ualias(void);
void	tlb_ram_cac(void);
void	tlb_ram_cac_node(nasid_t nasid);
void  (*tlb_to_prom_addr(void (*)()))();

#endif /* _LANGUAGE_C */

#endif /* _TLB_H_ */
