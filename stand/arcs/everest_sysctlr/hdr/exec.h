/*
	Contains System Controller executive structures and definitions.
*/

#define _EXEC_INCLUDED

/* Configuration Parameters	*/

#define NUMPRI		5
#define NUMPROC		6
#define	MAXPROCS	10		/* Maximum number of processes */
#define NUMSEM		(NUMPROC)
#define NUMTCBS		(8 * NUMPROC)
#define	NUMMSGS		(32 * NUMPROC)

/* Process Control Block Structure */

#define PROC struct process_ctl_blk
PROC	{
	PROC			*nxtproc;		/* link to next proc in queue	*/
	void 			(*textseg)(void);	/* process text segment		*/
	char			*initstck;		/* initial stack pointer	*/
	char			*stackptr;		/* current stack and frame ptrs:*/
	char			*frameptr;		/*   do NOT separate these two	*/
	unsigned short	*retvalp;		/* ptr ret val (D0) on stack	*/
	unsigned short	stacksz;		/* num bytes in stack		*/
	char			state;			/* i.e. stopped,ready,running	*/
	unsigned short	priority;		/* process priority		*/
	char			prid;			/* process id number		*/
};

/* Timer Control Block Structure */

#define TCB struct timer_ctl_blk
TCB	{
	TCB				*nxttcb;	/* for linking timer on queue	*/
	short			expmsg;		/* expiration message		*/
	short			expticks;	/* ticks till expiration	*/
	unsigned short	expsem;		/* sem to which expmsg is sent	*/
};

/* Message Node Structure */

#define MSG struct msg_node
MSG	{
	MSG				*nxtmsg;	/* link for queuing message	*/
	unsigned short	msgtext;	/* 4 bytes of message data	*/
};

/* Generic Queue Link Structure	*/

#define QLINK struct q_link
QLINK	{
	QLINK		*nxtele;	/* next element in Q		*/
	char		body;		/* rest of the Q element	*/
};

/* Generic Queue Structure	*/

#define QUEUE struct queue
QUEUE	{
	QLINK		*qhead;		/* beginning of queue		*/
	QLINK		*qtail;		/* last element in queue	*/
};

/* Semaphore Control Block Structure */

#define SEM struct sem_ctl_blk
SEM	{
	signed short	scount;		/* count of elements queued	*/
	QUEUE			semq;		/* items enqueued		*/
};

/* System Control Block Structure */

#define SCB struct sysctl 
SCB	{
	QUEUE	rdyq[NUMPRI];	/* 1 ready q per priority	*/
	PROC	*runproc;		/* ptr to running process	*/
	PROC	*ptp;			/* process table ptr		*/
	SEM		*stp;			/* semaphore table ptr		*/
	TCB		*timerq;		/* q of outstanding timeouts	*/
	TCB		*freetcbp;		/* ptr to next avail tcb	*/
	MSG		*freemsgp;		/* ptr to next avail msg node	*/
};


/* Process States */

#define P_INV		0		/* process invalid	*/
#define	P_RDY		1		/* ready to run		*/
#define	P_RUN		2		/* process running	*/
#define	P_STOP		3		/* process stopped	*/
#define	P_WAIT		4		/* process q'd on sem	*/


/* STACK BEGIN POINT */
#define STACK_BOTTOM	(char *) 0x1FFE		/*word boundary */
