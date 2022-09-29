/* 
	Contains all routines that interface with the Executive.
	All processes with the execption of 'tick' are called from
	'execinit'. 'tick' is called from the real time interrupt handler.
*/

#include <sp.h>
#include <exec.h>
#include <proto.h>
#include <io6811.h>

#define	STKSIZ		512		/* process stack size */

int				tm_mode;			/* set by inttmr process */
unsigned char   ctr;


/* rti_init -
		Initialize the Real time Interrupt time to time out on
		4.10 ms periods and generate interrupts when the time
		expires

 */
 non_banked void
 rti_init()
 {

#ifdef FASTTIMER
	/* set timer up for fastest mode (4.1ms)  */
	PACTL &= 0xFC;
#else
	PACTL |= 0x03;
#endif


	/* enable interrupts */
	TMSK2 |= 0x40;
}

/*	tick -
		Interrupt timer expiration handler. Updates TCB entries.
		If a TCB expiration occurs then the message is sent to
		the specified semaphore.

		Each tick represents 4.1 msec

		4/12/93 -	Added code to track the number of available free
					tcb messages.

*/

non_banked void
tick()
{
	extern SCB	scb;
	SCB			*scbp = &scb;		/* Pointer to SCB */
	TCB			*tp;				/* Ponter to current TDB */
	extern struct ENV_DATA	env;	/*data structure for environmental data */
	struct ENV_DATA			*eptr;

	eptr = &env;

	for (tp = scbp->timerq; tp;) {
		if (--(tp->expticks) <= 0) {
			/* UNIX timer interrupt? */
			send((short)tp->expsem, (unsigned long)tp->expmsg);
			tp = tp->nxttcb;
			link((QLINK *)&scbp->freetcbp, (QLINK *)dqueue((QUEUE *)&scbp->timerq));
			eptr->num_free_tcb++;
		}
		else
			tp = tp->nxttcb;
	}
}

/*	procinit -
		Process set up code. Sets up the PCBs for all processes in
		the system.
*/

non_banked void
procinit()
{
	extern PROC	proctable[NUMPROC];					/* Process table */

	PROC	*proc;				/* Pointer to current process struct */

	proc = &proctable[0];		/* system start-up, process 0 */
	proc->textseg = main;
	proc->stacksz = STKSIZ;
	proc->priority = 0;
	proc->state = P_RUN;		/* (this one's currently running) */

	proc = &proctable[SEQUENCE];	/* Sequence Power */
	proc->textseg = sequencer;
	proc->stacksz = STKSIZ;
	proc->priority = 0;
	proc->state = P_STOP;

	proc = &proctable[MONITOR];	/* Voltage, Temp, Clock monitor */
	proc->textseg = sys_mon;
	proc->stacksz = STKSIZ + 0x100;
	proc->priority = 2;
	proc->state = P_STOP;

	proc = &proctable[DISPLAY];	/* Front Panel Display*/
	proc->textseg = display;
	proc->stacksz = STKSIZ + 0x100;
	proc->priority = 1;
	proc->state = P_STOP;

	proc = &proctable[CPU_PROC];	/* CPU serial Interface Read */
	proc->textseg = cpu_proc;
	proc->stacksz = STKSIZ + 0x100;
	proc->priority = 0;
	proc->state = P_STOP;

	proc = &proctable[POK_CHK];	/* POKx, KEY Switch, Overtemp scan process*/
	proc->textseg = pok_chk;
	proc->stacksz = STKSIZ;
	proc->priority = 2;
	proc->state = P_STOP;
}
