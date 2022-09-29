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


#ifndef __SYS_TLBDUMP_H__
#define __SYS_TLBDUMP_H__

#include <sys/immu.h>
#include <sys/sbd.h>


#if	!LANGUAGE_ASSEMBLY && _KERNEL
/*
 * Data structures used to Dump TLB contents to an array in case of
 * PANIC hoping that this would be a useful data for core analysis.
 */
typedef struct {
        k_machreg_t	tlbhi;
        pde_t		tlblo;
#if     R4000 || R10000
        pde_t		tlblo1;
#endif
} tlbent_t;

typedef struct {
        uint            tlbentry_valid;
#if TFP || BEAST
        tlbent_t        tlbents[NTLBENTRIES][NTLBSETS];
#else
        tlbent_t        tlbents[MAX_NTLBENTRIES];
#endif
} tlbinfo_t;

extern  tlbinfo_t       *tlbdumptbl;


#if     TFP
extern  void tlbgetent(int, int, void *, void *);
#endif
#if     R4000 || R10000
extern  void tlbgetent(int, void *, void *, void *);
#endif

#endif	/* !LANGUAGE_ASSEMBLY && _KERNEL  */

#endif /* __SYS_TLBDUMP_H__ */
