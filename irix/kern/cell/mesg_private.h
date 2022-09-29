/**************************************************************************
 *									  *
 * 		 Copyright (C) 1994-1997 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Id: mesg_private.h,v 1.1 1997/10/11 01:07:57 sp Exp $"

#ifndef	_MESG_PRIVATE_H_
#define	_MESG_PRIVATE_H_ 1

extern void mesg_demux(cell_t, int, mesg_channel_t, idl_msg_t *);

extern int mesg_xpc_connect(cell_t, int, xpchan_t *);
extern void mesg_xpc_free(mesg_channel_t, idl_msg_t *);

#define MESG_MAX_SIZE   512	/* XXX really xpc specific */

typedef struct reply_info {
	uint	ri_subsys;
	uint	ri_opcode;
	time_t	ri_start;
	uint	ri_id;
	uint	ri_callback;
	uint	ri_cb_subsys;
	uint	ri_cb_opcode;
	time_t	ri_cb_start;
} reply_info_t;

typedef struct reply {
	uint	re_free : 1,
		re_chain : 1,
		re_callback : 1,
		re_id : 29;
	union {
		idl_msg_t	*reu_mesg;
		struct creply	*reu_list;
	} re_un;
	mesg_channel_t	re_channel;
	kthread_t	*re_kthread;
	reply_info_t	*re_info;
} reply_t;

#define	re_mesg		re_un.reu_mesg

typedef struct reply_ent {
	reply_t		rt_reply;
	sv_t		rt_sv;
} reply_ent_t;

#define rt_list		rt_reply.re_un.reu_list
#define rt_chain	rt_reply.re_chain
#define rt_free		rt_reply.re_free

typedef struct creply {
	reply_t		cr_reply;
	struct creply	*cr_next;
} creply_t;

#define MESG_REPLY_ID_MAX	0x1fffffff		/* 29 bits */

#define MESG_REPLY_ID_TABSZ	16

typedef struct {
	kqueue_t		ci_list;
	short			ci_channo; /* main or membership */
	mesg_channel_state_t	ci_state;
	short			ci_refcnt;
	cell_t			ci_cell;
	xpchan_t		ci_chanid;
	mutex_t			ci_lock;
	uint			ci_nextid;
	reply_ent_t		ci_reply_tab[MESG_REPLY_ID_TABSZ];
	sv_t			ci_wait;
} chaninfo_t;

#endif	/* _MESG_PRIVATE_H_ */
