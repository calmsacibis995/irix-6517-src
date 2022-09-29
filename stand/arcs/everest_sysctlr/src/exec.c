/* 
	Contains the Executive user interface routines.
*/

#include <exec.h>
#include <proto.h>
#include <sp.h>

/* Global Data Declarations	*/

int		idle;
char	*stckbtm;
char	*lowmem;
MSG		msgnodes[NUMMSGS];
PROC	proctable[NUMPROC];
SCB		scb;
SEM		sem[NUMSEM];
TCB		tcbnodes[NUMTCBS];


/*	execinit -
		Initialize all exec data structures and run initial process.

		4/12/93 -	Added lines to initialize the varibles used for
					tracking the number of remaining free tcb and msg
					nodes.
*/

non_banked void
execinit()
{
	SCB		*scbp;		/* fast ptr to scb	*/
	PROC	*pp;		/* ptr into proc table	*/
	MSG		*msgp;		/* ptr into msg list	*/
	TCB		*tcbp;		/* ptr into tcb list	*/
	int		i;			/* loop index		*/
	extern struct ENV_DATA	env;	/*data structure for environmental data */
	struct ENV_DATA		*eptr;

	eptr = &env;

#ifdef NOTYET
	lowmem = &end; /*BLM for now we will leave this as is. However
					 when I get the assembler and compiler the are for 
					 memory used for the following entries will be EXT_RAM_1,
					 with the stack located in EXT_RAM_2.
				 	*/
#endif
	for (i = 0, msgp = msgnodes; i < NUMMSGS - 1; i++, msgp++)
		msgp->nxtmsg = msgp + 1;
	for (i = 0, tcbp = tcbnodes; i < NUMTCBS - 1; i++, tcbp++)
		tcbp->nxttcb = tcbp + 1;
	scbp = &scb;
	scbp->rdyq[0].qhead = (QLINK *)proctable;
	scbp->rdyq[0].qtail = (QLINK *)proctable;
	scbp->runproc = &proctable[0];
	scbp->ptp = proctable;
	scbp->stp = sem;
	scbp->freetcbp = tcbnodes;
	scbp->freemsgp = msgnodes;

	eptr->num_free_msg = NUMMSGS;
	eptr->min_free_msg = NUMMSGS;
	eptr->num_free_tcb = NUMTCBS;
	eptr->min_free_tcb = NUMTCBS;

	procinit();

	stckbtm = STACK_BOTTOM;
	for (i = 0,pp = proctable; i < NUMPROC; i++, pp++) {
		if (pp->stacksz) {
			pp->prid = i;
			pp->initstck = stckbtm;
			stckbtm -= pp->stacksz;
		}
	}
	for (i = 1; i < NUMPROC; i++)
		start(i);
}

/*	check -
		Check a semaphore for a message to be delivered. If 
		present, dequeue and return it, otherwise, return 0.
		Input is the semaphore number to check.

		4/12/93 -	Added code to track the number of remaining free
					msg nodes.
*/

non_banked monitor short
check(semnum)
short		semnum;			/* semaphore number to be checked */
{
						/*BLM the length of the rvp will have to be adjusted */
	short 			rvp;					/* return value			*/
	MSG				*mp;					/* msg node pointer		*/
	SCB				*scbp = &scb;			/* quick scb access pointer	*/
	SEM				*semp = &sem[semnum];	/* sem to check		*/
	extern struct ENV_DATA	env;	/*data structure for environmental data */
	struct ENV_DATA			*eptr;

	eptr = &env;

	if (semp->scount > 0) {
		mp = (MSG *)dqueue(&semp->semq);
		semp->scount--;
		rvp = mp->msgtext;
		link((QLINK *)&scbp->freemsgp, (QLINK *)mp);
		eptr->num_free_msg++;
	}
	else
		rvp = 0;
	return(rvp);
}

/*	create - 
		Create a new process with the given parameters. Inputs are
		the address of the text segment, the stack size, and the
		priority. Returns the new process id.
*/

non_banked char
create(text, stksiz, pri)
void	(*text)();	/* Address of text segment */
unsigned short	stksiz;		/* Stack size */
char			pri;		/* Priority  */
{
	extern PROC		proctable[];	/* Process table */
	extern char		*stckbtm;		/* Bottom of stack area */
	PROC	*pp;			/* Pointer to new PCB */
	int	i;				/* Loop index variable */

	if (pri > NUMPRI)
		return(0);
	for (i = NUMPROC; i < MAXPROCS; i++)
		/* Search for the next available process number */
		if (proctable[i].prid == 0)
			break;
	if (i >= MAXPROCS)				/*No room left for additional processes*/
		return(0);

	stckbtm -=  stksiz;
	pp = &proctable[i];
	pp->textseg = (void *) text;
	pp->initstck = stckbtm;
	pp->stacksz = stksiz;
	pp->state = P_STOP;
	pp->priority = pri;
	pp->prid = i;
	start(i);
	return(i);
}

/*	duplicate -
		Duplicate a process and return the new pid. Input is the
		process id to be duplicated.
*/

non_banked char
dupproc(ipid)
char		ipid;		/* Input PID */
{
	extern PROC	proctable[];	/* Process table */
	PROC		*pp;			/* Pointer to input PCB */

	pp = &proctable[ipid];
	if ((ipid < NUMPROC) && (pp->prid != 0))
		return(create(pp->textseg, pp->stacksz, (char)pp->priority));
	else
		return(0);
}

/*	forfit -
		Take this process and put it at the end of its readyq, then do a
		dispatch.
 */

non_banked monitor void
forfit()
{
	PROC	*pp;			/* this procs process blk ptr	*/
	SCB		*scbp = &scb;	/* scb access pointer	*/

	pp = scbp->runproc;
	pp->state = P_RDY;
	queue(&scbp->rdyq[pp->priority], (QLINK *)pp);
	dispatch();
}

/*	getmypid -
		Return this processes id.
*/

non_banked char
getmypid()
{
	return(scb.runproc->prid);
}

/*	remove -
		Remove a process from the process list. Input is the process
		id of the process to remove.
*/

non_banked monitor void
remove(pid)
char		pid;		/* process id to be removed */
{
	extern PROC	proctable[];	/* Process table */
	PROC		*pp;			/* pointer to PCB */

	if ((pid < NUMPROC) && (pid > 16)) {
		pp = &proctable[pid];
		if (pp->state == P_RUN) {
			pp->state = P_STOP;
			pp->prid = 0;
			dispatch();
		}
		else {
			stop(pid);
			pp->prid = 0;
		}
	}
}

/*	receive -
		Check a semaphore for a message to be delivered. If 
		present, dequeue and return it, otherwise, wait. Performs
		a dispatch if it waits. Input is the semaphore number.

		4/12/93 -	Changed the interrupt handling from a monitor routine
					to manually disabling the interrupts.  If the running
					process has a new message, then the interrupts are 
					enabled before returning to the same process. In the 
					other case dispatch will enable the interrupt call after
					dqueuing the running process.

				-	Added code to track the number of remaining free
					msg nodes.
*/

non_banked unsigned short
receive(semnum)
char	semnum;		/* semaphore number to be checked */
{
	unsigned short	rvp;					/* return value	*/
	MSG				*mp;					/* msg node pointer */
	PROC			*pp;					/* this procs process blk ptr */
	SCB				*scbp = &scb;			/* quick scb access pointer */
	SEM				*semp = &sem[semnum];	/* sem to check	*/
	extern struct ENV_DATA	env;	/*data structure for environmental data */
	struct ENV_DATA			*eptr;

	eptr = &env;

	disable_interrupt();

	if (semp->scount > 0) {
		mp = (MSG *)dqueue(&semp->semq);
		semp->scount--;
		rvp = mp->msgtext;
		link((QLINK *)&scbp->freemsgp, (QLINK *)mp);
		eptr->num_free_msg++;
		enable_interrupt();
	}
	else {
		pp = scbp->runproc;
		pp->retvalp = &rvp;
		pp->state = P_WAIT;
		queue(&semp->semq, (QLINK *)pp);
		semp->scount--;
		dispatch();
	}
	return(rvp);
}

/*	send -
		Check a semaphore for a process waiting for a message. 
		If found, jam the message into the receipients stack (D0) and
		make him ready to run. Otherwise, just queue the message.
		Inputs are the semaphore number and the message.

		4/12/93 -	Added code to track the number of remaining free
					msg nodes, and to log an error in the event that
					there are no free msg nodes.
*/

non_banked monitor void
send(semnum, msg)
short			semnum;		/* semaphore number to be checked */
unsigned long	msg;		/* the message to be delivered	*/
{
	MSG		*mp;					/* msg node pointer		*/
	PROC	*pp;					/* this procs process blk ptr	*/
	SCB		*scbp = &scb;			/* quick scb access pointer	*/
	SEM		*semp = &sem[semnum];	/* sem to check		*/
	extern struct ENV_DATA	env;	/*data structure for environmental data */
	struct ENV_DATA			*eptr;

	eptr = &env;

	if (semp->scount >= 0) {
		mp = (MSG *)dlink((QLINK *)&scbp->freemsgp);
		eptr->num_free_msg--;
		if (eptr->num_free_msg < eptr->min_free_msg)
			eptr->min_free_msg = eptr->num_free_msg;
		if (eptr->num_free_msg == 0) {
			log_event(FMSG_ERR, FP_DISPLAY);
			_opc(0x3F); 					/* SWI  */
		}
		semp->scount++;
		mp->msgtext = msg;
		queue(&semp->semq, (QLINK *)mp);
	}
	else {
		pp = scbp->runproc;
		pp = (PROC *)dqueue(&semp->semq);
		semp->scount++;
		*(pp->retvalp) = msg;
		pp->state = P_RDY;
		queue(&scbp->rdyq[pp->priority], (QLINK *)pp);
	}
}

/*	start -
		Move a process onto the ready queue; force the PC to the 
		base of the text segment (process init code). Input is the
		process id of the process to start.

		The stackpointer is initialized to initstack - 3 to conform to 
		the HC11 processor of the SP pointing to the next available 
		location.  The inital value loaded (entry point to the process) is
		then placed at the initstack - 2.
*/

non_banked void
start(pid)
char		pid;			/* the process id */
{
	PROC	*pp = &proctable[pid];		/* proc pointer	*/
	unsigned short 	*stack;				/* temp stack pointer */

	if (pid < NUMPROC) {
		if (pp->textseg) {
			if (pp->state == P_STOP) {
				pp->state = P_RDY;
				pp->stackptr = pp->initstck - 3;
				stack = (unsigned short *) pp->initstck - 1;
				*stack = (unsigned short) pp->textseg; 
				queue(&scb.rdyq[pp->priority], (QLINK *)pp);
			}
		}
	}
}

/*	stop -
		Finds a process within the exec and puts it in a stopped state.
		If the process is running then it does a dispatch. Input is
		the process id of the process to stop.
*/

non_banked monitor void
stop(pid)
char		pid;			/* id of process being stopped*/
{
	int		found = 0;		/* search variable	*/
	short	i;				/* loop index		*/
	PROC	*pp;			/* proc being stopped	*/
	PROC	*p;				/* temp proc ptr	*/
	SCB		*scbp = &scb;	/* working variable	*/
	SEM		*semp;			/* for scanning sems	*/

	pp = &proctable[pid];
	if (pid < NUMPROC) {
		switch(pp->state) {
		case P_RDY:
			p = (PROC *)(scbp->rdyq[pp->priority]).qhead;
			if (p == pp)
				dqueue(&scbp->rdyq[pp->priority]);
			else {
				while (p->nxtproc != pp)
					p = p->nxtproc;
				p->nxtproc = pp->nxtproc;
				pp->nxtproc = 0;
				if (pp == (PROC *)(scbp->rdyq[pp->priority]).qtail)
					(scbp->rdyq[pp->priority]).qtail = (QLINK *)p;
			}
			pp->state = P_STOP;
			break;

		case P_RUN:
			pp->state = P_STOP;	/* dqueue in dispatch	*/
			dispatch();
			break;

		case P_WAIT:
			for (i = 0; i < NUMSEM && !found; i++) {
				semp = &scbp->stp[i];
				if (semp->scount < 0) {
					p = (PROC *)(semp->semq).qhead;
					if (p == pp) {
						dqueue(&semp->semq);
						semp->scount++;
						found = 1;
					}
					else {
						while (p && p->nxtproc != pp)
							p = p->nxtproc;
	
						if (p) {
							p->nxtproc = (p->nxtproc)->nxtproc;
							found = 1;
							semp->scount++;
						}
					}
				}
			}
			pp->state = P_STOP;
			break;

		default:
			break;
		}
	}
}
		


/*	timeout -
		Takes a request for message in 'tval' time units and builds an
		appropriate timer contro block (tcb). Timeout values within the
		tcb's are decremented at each clock tick by the 'tick' routine.
		When the specified number of ticks has occurred then the message
		is sent to the specified semaphore and the tcb is removed.
		Inputs are the semaphore number and message to send, and the
		number of clock ticks to wait.

		4/12/93 -	Added code to track the number of remaining free
					tcb nodes, and logs an error if no remaining tcb
					exist.
*/

non_banked monitor void
timeout(semnum, msg, tval)
short 			semnum;		/* semaphore for send at exp	*/
unsigned short	msg;		/* msg to be sent at expiration	*/
unsigned short	tval;		/* timeout value in 'units'	    */
{
	SCB	*scbp = &scb;		/* quick ptr to sys ctl block   */
	TCB	*tcbp;				/* newly allocated tcb		    */
	TCB	*tp;				/* temp tcb pointer		        */
	TCB	*prevtp = 0;		/* tcb before new one		    */
	extern struct ENV_DATA	env;	/*data structure for environmental data */
	struct ENV_DATA			*eptr;

	eptr = &env;

	tcbp = (TCB *)dlink((QLINK *)&scbp->freetcbp);
	eptr->num_free_tcb--;
	if (eptr->num_free_tcb < eptr->min_free_tcb)
		eptr->min_free_tcb = eptr->num_free_tcb;
	if (eptr->num_free_tcb == 0) {
			log_event(FTCB_ERR, FP_DISPLAY);
			_opc(0x3F); 					/* SWI  */
	}
	tcbp->expmsg = msg;
	tcbp->expticks = tval;
	tcbp->expsem = semnum;
	for (tp = scbp->timerq; tp && tval > tp->expticks; tp = tp->nxttcb)
		prevtp = tp;
	if (prevtp) {
		tcbp->nxttcb = prevtp->nxttcb;
		prevtp->nxttcb = tcbp;
	}
	else {
		tcbp->nxttcb = scbp->timerq;
		scbp->timerq = tcbp;
	}
}

/*	cancel_timeout -
		Interrupt timer request cancel.  Cancels all requests
		for the same semaphore and message.  Has no effect if
		there were no such requests.

		4/12/93 -	Added code to track the number of available
					free tcb nodes.
*/

non_banked monitor short
cancel_timeout(semnum,msg)
short 			semnum;		/* semaphore for send at exp	*/
unsigned long	msg;		/* msg to be sent at expiration	*/
{
	TCB			*tp;		/* Pointer to current TCB */
	extern SCB	scb;
	SCB			*scbp;		/* Pointer to SCB */
	TCB			**prevtp;	/* Pointer to "tp" pointer */
	short		ticksleft;	/* number of ticks left */
	extern struct ENV_DATA	env;	/*data structure for environmental data */
	struct ENV_DATA			*eptr;

	eptr = &env;

	/* put SCB address in register for efficient access */
	scbp = &scb;

	/* unlink all matches in TCB list */
	ticksleft = 0;							/* default to none left */
	prevtp = &scbp->timerq;					/* point to list header */
	for (;;) {								/* go through whole list */
		tp = *prevtp;	 					/* point to new TCB */
		if (!tp)  							/* is there another? */
			break;							/* all done if not */
	   	if (tp->expsem == semnum			/* does semaphore match? */
			&& tp->expmsg == msg) {			/* and message too? */
			ticksleft = tp->expticks;		/* # of unused ticks */
			*prevtp = tp->nxttcb;			/* unlink from active list */
			tp->nxttcb = scbp->freetcbp;	/* link to */
			scbp->freetcbp = tp;			/* free list */
			eptr->num_free_tcb++;
		} else {
			prevtp = &tp->nxttcb;			/* next in chain */
		}
	}

	/* return count of unused ticks */
	return (ticksleft);
}
