/*
 * msg.h
 *	Header file for message functions.
 */

#ifndef _MSG_H_
#define _MSG_H_

#include "ffsc.h"


/* Other constants */
#define MAX_REQS			(MAX_RACKS + MAX_BAYS + 2)
#define EXTRANEOUS_LINE_LEN		80
	

/* Message Info */
#define MI_NAME_LEN	8
typedef struct msg {
	char	Name[MI_NAME_LEN];	/* Handy name */
	char	*Text;			/* Buffer for message */
	char	*CurrPtr;		/* Current end-of-text */
	int	MaxLen;			/* Size of message buffer */
	int	FD;			/* FD of acknowledge pipe/socket */  
	int	Type;			/* Description of the Other Side */
	int	State;			/* Current state token */
} msg_t;

/* msg_t Types */
#define RT_UNKNOWN	0	/* Don't know who other side is! */
#define RT_ELSC		1	/* Other side is a local ELSC */
#define RT_REMOTE	2	/* Other side is a remote FFSC */
#define RT_CONSOLE	3	/* Other side is a Console task */
#define RT_DISPLAY	4	/* Other side is the DISPLAY task */
#define RT_SYSTEM	5	/* Other side is The System (IRIX) */
#define RT_LOCAL	6	/* Other side is us (pseudo-type) */

/* msg_t States */

/* start states */
#define RS_UNKNOWN	 0	/* Unknown state! */
#define RS_NONE		 0	/* a.k.a. no state */

#define RS_SEND		 1	/* Ready to send */
#define RS_NEED_FFSCMSG  2	/* Waiting for an FFSC message */
#define RS_NEED_FFSCACK	 3	/* Waiting for an FFSC response */
#define RS_NEED_ELSCMSG  4	/* Waiting for an ELSC message */
#define RS_NEED_ELSCACK	 5	/* Waiting for an ELSC response */
#define RS_NEED_DISPACK	 6	/* Waiting for a display task response */

/* terminal states */
#define RS_ERROR	 10	/* Error */
#define RS_DONE		 11	/* Response complete */

/* intermediate states */
#define RS_SENDING	 20	/* Some data sent, more to go */

#define RS_NEED_FFSCEND	 21	/* Waiting for end of FFSC response */

#define RS_NEED_ELSCEND  22	/* Waiting for end of ELSC message (part 1) */
#define RS_NEED_ELSCEND2 23	/* Waiting for end of ELSC message (part 2) */
#define RS_NEED_ELSCEND1 24	/* ditto, but CHAR2 came before CHAR1 */

#define RS_NEED_ELSCACK_END  25	/* Waiting for end of ELSC ACK (part 1) */
#define RS_NEED_ELSCACK_END2 26	/* Waiting for end of ELSC ACK (part 2) */


/* Function prototypes */
void	msgClear(msg_t *);
void	msgFree(msg_t *);
msg_t	*msgNew(int, int, int, char *);
STATUS	msgRead(msg_t *, int);
STATUS	msgWrite(msg_t *, int);

#ifndef PRODUCTION
STATUS  msgShow(msg_t *, int, char *);
#endif  /* !PRODUCTION */

#endif /* _MSG_H_ */
