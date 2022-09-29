/**************************************************************************
 *									  *
 * 		 Copyright (C) 1994-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ifndef	_KSYS_MESG_H_
#define	_KSYS_MESG_H_	1
#ident "$Id: mesg.h,v 1.14 1997/09/15 23:14:14 sp Exp $"

#ifndef	CELL
#error included by non-CELL configuration
#endif

#include <sys/types.h>
#include <ksys/xpc.h>
#include <ksys/cell/service.h>
#include <ksys/cell/remote.h>

typedef struct {
	xpm_t	msg_xpm;
	xpmb_t	msg_xpmb;
} idl_msg_t;

typedef struct {
	uint	hdr_type : 3,
		hdr_id : 29;
	uint	hdr_callbackid;
	uint	hdr_subsys;
	uint	hdr_opcode;
	__uint64_t hdr_rtime;
} idl_hdr_t;

#define IDL_MESG_HDR(msg)	((idl_hdr_t *)(((paddr_t)(msg)) + (msg)->msg_xpmb.xpmb_ptr))
#define IDL_MESG_BODY(msg)	(IDL_MESG_HDR(msg) + 1)

typedef void	*mesg_channel_t;

typedef void	(*idl_message_handler_t)(mesg_channel_t, idl_msg_t *);

void mesg_handler_register(idl_message_handler_t, int);

extern int mesg_max_size;

#define MESG_NO_BUFFER		((size_t)-1)

#define	MESG_CHANNEL_MAIN	0
#define	MESG_CHANNEL_MEMBERSHIP	1

/*
 * Channel states.
 */
typedef enum mesg_channel_state {
	MESG_CHAN_INVALID,	/* Invalid state                       */
	MESG_CHAN_READY,	/* Ready to send messages              */
	MESG_CHAN_FROZEN,	/* Frozen. Messages will wait for      */
				/* to either go to ready or closed     */
	MESG_CHAN_CLOSED	/* Channel is closed. Messages return  */
				/* with ECELLDOWN                      */

} mesg_channel_state_t;

extern int		mesg_connect(cell_t, int);
extern void		mesg_disconnect(cell_t, int);
extern mesg_channel_t	cell_to_channel(cell_t, int);
extern void		mesg_channel_rele(mesg_channel_t);
extern idl_msg_t	*mesg_allocate(mesg_channel_t);
extern void		mesg_free(mesg_channel_t, idl_msg_t *);
extern int		mesg_send(mesg_channel_t, idl_msg_t *, int);
extern idl_msg_t	*mesg_rpc(mesg_channel_t, idl_msg_t *);
extern int		mesg_reply(mesg_channel_t, idl_msg_t *, idl_msg_t *,
					int);
extern idl_msg_t	*mesg_callback(mesg_channel_t, idl_msg_t *);
extern int		mesg_is_rpc(idl_msg_t *);
extern service_t	mesg_get_callback_svc(void);
extern void		mesg_set_callback_hdr(idl_hdr_t *);
extern void		mesg_marshal_indirect(idl_msg_t *, void *, size_t);
extern int		mesg_indirect_count(idl_msg_t *);
extern void		mesg_unmarshal_indirect(idl_msg_t *, int, void **,
					size_t *);
extern void		*mesg_get_immediate_ptr(idl_msg_t *, size_t);

extern trans_t		*mesg_thread_info(void);
extern void 		mesg_channel_set_state(cell_t, int, 
					mesg_channel_state_t);
extern void		mesg_cleanup(cell_t, int);
extern void		mesg_info_push(kthread_t *);
extern void		mesg_info_pop(kthread_t *);


/*
 * Defines for all message systems
 */
extern struct ktrace *mesg_trace;	/* tracing buffer header */
extern __uint64_t mesg_trace_mask;	/* mask of subsystems */
extern __uint64_t mesg_id;		/* unique ID for each message */

#define MESGNAMSZ	32
struct mesgstat {
	unsigned int	count;		/* Count of messages sent */
	uint64_t	ctime;		/* Total cpu time of messages */
	uint64_t 	etime;		/* Total elapsed time of messages */
	void		*addr;		/* Address of service message handler */
	void		*return_addr;	/* Address of caller */
	char		name[MESGNAMSZ];/* Name of service message handler */
	struct mesgstat	*next;		/* Pointer to next stat */
};
	
extern struct mesgstat *mesgstat[];
extern void mesgstat_enter(int, void *, char *, void *, struct mesgstat **);
extern void mesgstat_exit(struct mesgstat *, unsigned int, unsigned int);

#endif /* _SYS_MESG_H */
