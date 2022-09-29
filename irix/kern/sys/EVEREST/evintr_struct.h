/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/


#ifndef __SYS_EVEREST_INTR_STRUCT_H__
#define __SYS_EVEREST_INTR_STRUCT_H__

typedef struct evintr {
	unchar		evi_level;		/* Ebus interrupt level */
	unchar		evi_dest;		/* logical cpuid destination */
	ulong		evi_spl;		/* sw spl descriptor */
	evreg_t		*evi_regaddr;		/* Ebus intr reg addr */
	EvIntrFunc	evi_func;		/* interrupt handler */
	void *		evi_arg;		/* arg passed to evi_func */
	thd_int_t	evi_tinfo;		/* Thread info */
} evintr_t;

extern evintr_t evintr[EVINTR_MAX_LEVELS];

#define evi_flags evi_tinfo.thd_flags
#define evi_isync evi_tinfo.thd_isync
#define evi_lat evi_tinfo.thd_latstats
#define evi_thread evi_tinfo.thd_ithread

#endif /* __SYS_EVEREST_INTR_STRUCT_H__ */
