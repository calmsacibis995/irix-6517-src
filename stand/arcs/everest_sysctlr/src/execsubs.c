/*
	Contains Real Time Executive Support Subroutines.
*/

#include <exec.h>
#include <proto.h>
#include <sp.h>
#include <io6811.h>


/*	dispatch -
		Scans the ready queues for the highest priority queue with a 
		head entry and runs the head entry by call to 'runproc'. The 
		flag 'idle' is a global symbol used by the 'tick' routine to 
		keep track of processor utilization. This routine is entered 
		with interrupts masked. As a future enhacement, it may be very 
		desirable to check process stack utilization and display a
		message if close to full.

		4/12/93 - moved the disable_interrupts() call from before the 
					dqueue statement to immediately after the dqueue
					statement.

	 	5/13/93 - Added log messages in the event of a stack fault
*/


non_banked void 
dispatch()
{
	extern int	idle;			/* exec idle flag		*/
	extern SCB	scb;			/* the system control block	*/
	int			i;				/* loop index			*/
	int			stckroom;		/* for calc stack utilization	*/
	PROC		*oldpp;			/* the currently running proc	*/
	PROC 		*pp;			/* pointer to process to be run	*/
	SCB			*scbp = &scb;	/* for optimized scb access	*/
	long		found;			/* search flag			*/


	/* reset COP timer; If we don't get here every second, we are lost */
	COPRST = 0x55;
	COPRST = 0xAA;

	dqueue(&scbp->rdyq[(scbp->runproc)->priority]);
	enable_interrupt();

	idle = 1;
	found = 0;
	while (!found) {
		for (i = 0; i < NUMPRI && !found; i++) {
			if (scbp->rdyq[i].qhead) {
				found = 1;
				idle = 0;
			}
		}
	}
	pp = (PROC *)scbp->rdyq[--i].qhead;
	oldpp = scbp->runproc;
	scbp->runproc = pp;
	stckroom = pp->stacksz - (pp->initstck - pp->stackptr);
	if (stckroom < 0) {
		log_event((STK_PID0 + pp->prid), FP_DISPLAY);
		send(SEQUENCE, DOWN_PARTIAL);
		return;
	}
	else if (stckroom < (pp->stacksz >> 3)) {
		log_event((STK_PID0 + pp->prid), FP_DISPLAY);
		send(SEQUENCE, DOWN_PARTIAL);
		return;
	}
	pp->state = P_RUN;
	runproc((unsigned short) &oldpp->stackptr, (unsigned short) &pp->stackptr);	
}





/*	dlink - 
		Removes an indicated entry from a LIFO queue and returns a 
		pointer to it. Interrupts MUST be disabled at the time this 
		routine is called. Input is a pointer to the previous element
		in the queue.
*/

non_banked char
*dlink(qp)
QLINK	*qp;	/* ptr to previous element */
{
	QLINK	*ep;	/* ptr to dequeued element	*/

	ep = qp->nxtele;
	qp->nxtele = ep->nxtele;
	ep->nxtele = 0;
	return((char *)ep);
}


/*	dqueue -
		Pull an element off of the head of a FIFO queue structure
		and return a pointer to it. Head and tail pointers are updated 
		as appropriate. Input is a pointer to the queue.
*/

non_banked char 
*dqueue(qp)
QUEUE *qp;		/* pointer to the queue struct	*/
{
	QLINK	*ep;		/* ptr to the dqueued element	*/

	if (ep = qp->qhead) {
		qp->qhead = ep->nxtele;
		ep->nxtele = (QLINK *)0;
	}
	return((char *)ep);
}


/*	link - 
		Adds the indicated entry to the indicated LIFO queue. 
		Interrupts MUST be disabled at the time this routine is called.
		Inputs are a pointer to the element in the queue to insert
		after and a pointer to the element to add.
*/

non_banked void
link(qp, ep)
	register QLINK	*qp;	/* ptr to queue element to insert after */
	register QLINK	*ep;	/* ptr to element to add */
	{
	ep->nxtele = qp->nxtele;
	qp->nxtele = ep;
}


/*	queue -
		Add an element to the tail of a FIFO queue structure. Head and
		tail pointers are updated as appropriate. Inputs are a pointer
		to the queue and a pointer to the element to add.
*/

non_banked void 
queue(qp, ep)
QUEUE	*qp;		/* the queue to hang on	*/
QLINK	*ep;		/* the element to queue	*/
{
	if (!qp->qhead)
		qp->qhead = qp->qtail = ep;
	else {
		(qp->qtail)->nxtele = ep;
		qp->qtail = ep;
	}
}


#ifdef STK_CHK
/************************************************************************
 *																		*
 *	stk_chk:	Stk_chk check the stack usage of individual subroutines *
 *				running under processes.  An error is when the stack 	*
 *				for the running process is over ~ 90% used.				*
 *																		*
 ************************************************************************/
non_banked void
stk_chk(void)
{
	PROC 			*pp;			/* pointer to running process */
	extern SCB		scb;			/* the system control block	*/
	SCB				*scbp = &scb;	/* for optimized scb access	*/
	int 			stkroom;		
	extern char		stack_error;	/* location for Breakpoint */

	pp = scbp->runproc;

	stkroom = pp->stacksz - (pp->initstck - getsp());
	if (stkroom < (pp->stacksz >> 3))
		stack_error = 1;
}
#endif
