/* Common definitions of the PPP finite state machines.
 */

#ifndef __FSM_H__
#define __FSM_H__

#ident "$Revision: 1.12 $"

#include <sys/time.h>


/* states */
enum fsm_state {
	FSM_INITIAL_0,
	FSM_STARTING_1,
	FSM_CLOSED_2,
	FSM_STOPPED_3,
	FSM_CLOSING_4,
	FSM_STOPPING_5,
	FSM_REQ_SENT_6,
	FSM_ACK_RCVD_7,
	FSM_ACK_SENT_8,
	FSM_OPENED_9
};

/* externally visible actions */
enum fsm_action {
	FSM_NULL,			/* nothing of interest */
	FSM_TLU,			/* This-Layer-Up */
	FSM_TLD,			/* This-Layer-Down */
	FSM_TLS,			/* This-Layer-Start */
	FSM_TLF				/* This-Layer-Finished */
};


/* events */
enum fsm_event {
	FSM_UP,
	FSM_DOWN,
	FSM_OPEN,
	FSM_CLOSE,
	FSM_TO_P,			/* timeout, restart counter > 0 */
	FSM_TO_M,			/* timeout, restart counter <= 0 */
	FSM_RCR_P,			/* ok Configure-Request */
	FSM_RCR_M,			/* bad Configure-Request */
	FSM_RCA,			/* Configure-Ack */
	FSM_RCN,			/* Configure-Nak/Rej */
	FSM_RTR,			/* Terminate-Request */
	FSM_RTA,			/* Terminate-Ack */
	FSM_RUC,			/* unknwon code */
	FSM_RXJ_P,			/* Code-Reject or Protocol-Reject */
	FSM_RXJ_M,			/* Code-Reject or Protocol-Reject */
	FSM_RXR
};


struct ppp;				/* (forward reference) */
struct fsm {
	char	name[8];
	int	protocol;
	struct fsm_ops {
	    void    (*scr)(struct ppp*);    /* send configure request */
	    void    (*sca)(struct ppp*,	    /* send configure Ack */
			   struct fsm*);
	    void    (*scn)(struct ppp*,	    /* send configure Nak */
			   struct fsm*);
	    void    (*str)(struct ppp *,    /* send terminate request */
			   struct fsm*);
	    void    (*sta)(struct ppp*,	    /* send terminate Ack */
			   struct fsm*);
	    void    (*scj)(struct ppp *,    /* send code reject */
			   struct fsm*);
	    void    (*ser)(struct ppp *,    /* send echo reply */
			   struct fsm*);
	    void    (*act)(struct ppp *,    /* do something */
			   enum fsm_action);
	} *ops;
	enum fsm_state	state;		/* current state */
	struct timeval	timer;		/* restart timer */
	time_t	ms;			/* next timeout value */
	time_t	restart_ms;
	time_t	restart_ms_lim;
	int	restart;		/* restart counter */
	int	restart0;		/* initial value of restart counter */
	int	nak_sent;		/* failure counters */
	int	nak_recv;
	int	tr_msg_len;
#	define TR_MSG_MAX 128
	char	tr_msg[TR_MSG_MAX];	/* terminate-request message */
	u_char	id;
};

#define RESTART_MS_MAX	(30*1000)
#define RESTART_MS_MIN	100
#define RESTART_MS_DEF	1000
#define RESTART_MS_LIM_DEF 8000
#define STOP_MS_DEF	3000

#define DEF_MAX_FAIL 10
#define DEF_MAX_CONF DEF_MAX_FAIL
#define DEF_MAX_TERM_MS 7000

#define TIME_NEVER	(365*24*60*60)
#define TIME_NOW	0


extern void fsm_init(struct fsm*);
extern char* fsm_action_name(enum fsm_action);
extern char* fsm_state_name(enum fsm_state);
extern char* fsm_code_name(u_char);
extern long cktime(struct timeval*);
extern void fsm_settimer(struct timeval*, time_t);
extern void fsm_run(struct ppp*, struct fsm*, enum fsm_event);
extern int fsm_ck_id(struct fsm*);
extern void fsm_sca(struct ppp*, struct fsm*);
extern void fsm_scn(struct ppp*, struct fsm*);
extern void fsm_str(struct ppp*, struct fsm*);
extern void fsm_sta(struct ppp*, struct fsm*);
extern void fsm_scj(struct ppp*, struct fsm*);
extern void fsm_rcj(struct ppp*, struct fsm*);
extern void fsm_ser(struct ppp*, struct fsm*);
extern void set_tr_msg(struct fsm*, char*, ...);

#endif /* __FSM_H__ */

