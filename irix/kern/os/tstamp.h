/**************************************************************************
 *                                                                        *
 *       Copyright (C) 1994, Silicon Graphics, Inc.                       *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ifndef __SYS_TSTAMP_H__
#define __SYS_TSTAMP_H__

/*
 * Timestamping suport for realtime events.
 * The idea is that we'll have a buffer per cpu filled up with time
 * stamps and tags identifying interesting realtime events. The buffer
 * will be mappable from user land.
 */

#ifdef _KERNEL

typedef struct {
	/* basic state */
	tstamp_shared_state_t* shared_state;
	__psunsigned_t allocated_base;
	uint allocated_size;
	uint nentries;
	uint water_mark;
	sema_t wmsema;
    	int nsleepers; 
	
	/* dynamic state */
	uint64_t tracemask;	/* merged with utrace_mask */
} tstamp_obj_t;

/* prototypes */
tstamp_obj_t* tstamp_construct(int nentries, int cpu);
void tstamp_destruct(tstamp_obj_t* ts);
void tstamp_install(tstamp_obj_t* ts, int cpu);
tstamp_obj_t* tstamp_uninstall(int cpu);
void tstamp_enable(int cpu);
void tstamp_disable(int cpu);
paddr_t tstamp_get_buffer_paddr(tstamp_obj_t* ts);
paddr_t tstamp_get_buffer_paddr_from_cpu(int cpu);
paddr_t	verify_mappable_tstamp(paddr_t buffer_addr, long len);
void tstamp_reset(tstamp_obj_t* ts, int flag);
void tstamp_set_eob_mode(int cpu, uint mode, uint* omode);
void tstamp_update_mask(int cpu);
void utrace_init();
void idbg_utrace(__psint_t count, __psint_t cpu);

     
#endif /* _KERNEL */


#endif /* __SYS_TSTAMP_H__ */
