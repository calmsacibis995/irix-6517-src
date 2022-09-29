/***************************************************************************
*									   *
* Copyright 1997 Adaptec, Inc.,  All Rights Reserved.			   *
*									   *
* This software contains the valuable trade secrets of Adaptec.  The	   *
* software is protected under copyright laws as an unpublished work of	   *
* Adaptec.  Notice is for informational purposes only and does not imply   *
* publication.	The user of this software may make copies of the software  *
* for use with parts manufactured by Adaptec or under license from Adaptec *
* and for no other use.							   *
*									   *
***************************************************************************/

#ifndef _HWPROTO_H
#define _HWPROTO_H

/***************************************************************************
*
*  Module Name:   hwproto.h
*
*  Description:   Prototype definitions of SHIM
*    
*  Owners:	  ECX IC Firmware Team
*    
*  Notes:	  
*
***************************************************************************/

/***************************************************************************
* Function prototypes for Slim HIM interface references
***************************************************************************/

/* hwconfig.c								*/
INT sh_get_configuration (struct shim_config *s);
INT sh_initialize(struct shim_config *s, void *ulm_info);

/* hwutil.c */
void sh_access_scratch (struct access_scratch_ram *c);
INT sh_save_state(struct shim_config *s, void *p);
INT sh_restore_state(struct shim_config *s, void *p);
INT sh_enable_TCB_ChkSum(struct shim_config *c);
INT sh_disable_TCB_ChkSum(struct shim_config *c);
void sh_disable_irq (struct control_blk *c);
void sh_enable_irq(struct control_blk *c);
INT sh_interrupt_pending(struct control_blk *c);
void sh_shutdown(struct control_blk *c, UBYTE option);
void sh_reset_chip(register REGISTER base);
void sh_read_seeprom(register REGISTER base, UBYTE *,INT,INT);
INT sh_write_non_protected_seeprom(register REGISTER base,INT,INT,UBYTE *);
INT sh_write_seeprom(register REGISTER base,INT,INT,UBYTE *);
UBYTE sh_read_mia_type(struct control_blk *c);
UBYTE4 sh_get_residual_length(struct control_blk *,void *,void *,BUS_ADDRESS);

/* hwintr.c */
INT sh_frontend_isr(struct control_blk *c);
void sh_backend_isr(struct control_blk *c);

/* hwtrans.c */
void sh_deliver_tcb(struct reference_blk *r);
INT sh_special_tcb(struct reference_blk *r);

/***************************************************************************
* Function prototypes to be provided by ULM
***************************************************************************/

/* ulmutil.c */
void ulm_event(struct reference_blk *r, INT event, void *ulm_info);
void ulm_tcb_complete(struct reference_blk *r, void *ulm_info);
void ulm_primitive_complete(struct reference_blk *r, void *ulm_info);
void ulm_swap8(register UBYTE *ptr, UBYTE4 count, void *ulm_info);
void ulm_delay(void *ulm_info, ULONG us);

/***************************************************************************
* Function prototypes for Slim HIM internal references
***************************************************************************/
/* hwconfig.c								*/
void sh_invalidate_tcbs(struct control_blk *c);

/* hwutil.c */
void sh_pausetop (struct control_blk *c);
void sh_unpause_sequencer(register REGISTER);
INT sh_load_sequencer (struct control_blk *c, INT type);
INT sh_setup_loop_bypass(struct control_blk *c);
void sh_output_paddr(struct control_blk *c, INT reg, BUS_ADDRESS baddr, UBYTE i);
void sh_setup_emerald_software(struct control_blk *c);
void sh_enable_seq_breakpoint(struct control_blk *c);
INT sh_enable_access (register REGISTER base, void *ulm_info);
INT sh_get_size_mainline_code(void);
VIRTUAL_ADDRESS sh_get_vaddr_mainline_code(void);
void sh_reload_mainline_firmware(struct control_blk *c);
void sh_loadnrun_lip(struct reference_blk *r, struct control_blk *c);
void sh_start_sequencer(struct control_blk *c);
void sh_start_mainline_sequencer(struct control_blk *c);
void sh_pause_sequencer(struct control_blk *c);
INT sh_setup_internal_loopback(struct control_blk *c);
void sh_save_sequencer_state(struct control_blk *c);
void sh_clear_pre_lip_queues(struct control_blk *c);
void sh_restore_sequencer_queues(struct control_blk *c);
void sh_setup_doneq(struct control_blk *c);
void sh_startup_sequencer(struct control_blk *c);
#if SHIM_DEBUG
void sh_dump_prim_tcb(struct primitive_tcb *t, UBYTE flag);
#endif
void sh_resume_post_lip_io(struct control_blk *c);
void sh_restore_pre_lip_queues(struct control_blk *c);
void sh_special_abort(struct reference_blk *r);

/* hwlip.c */
void sh_send_lip_primitive_in_lip(struct control_blk *c);
void sh_start_lip_sequencer(struct control_blk *c);
INT sh_get_size_lip_code(void);
VIRTUAL_ADDRESS sh_get_vaddr_lip_code(void);
UBYTE sh_timer_checking(struct control_blk *c);
void sh_check_loopfailure(struct control_blk *c);

#endif /* _HWPROTO_H */
