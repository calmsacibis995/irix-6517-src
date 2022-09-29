/*
 * switches.c
 *	Functions for handling the switches on the FFSC display
 */
#include <vxworks.h>

#include <intlib.h>
#include <iolib.h>
#include <loglib.h>
#include <semlib.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <syslib.h>
#include <time.h>
#include <timers.h>
#include <usrlib.h>

#include <arch\i86\ivi86.h>
#include <drv\intrctl\i8259a.h>

#include "ffsc.h"
#include "switches.h"
#include "swtst.h"


/* Globals */
SEM_ID debouncesema;
int swbits, swlockout;
int dispfd;			/* to redirect display stdout to the display */
int *swread;			/* Array of switch samples */
int swread_len;			/* Size of swread */

/*
 * sw_isr
 *	Interrupt Service Routine for switch presses
 */
void 
sw_isr( void )
{
   /*
    ** Read the switch port
    */

	int i;

	swbits = 0x3f;
	for (i=0; i<swread_len; i++)
	    swread[i] = (sysInByte( SWITCH_PORT ) & SWITCH_MASK);
	for (i=0; i<swread_len; i++)
	    swbits &= swread[i];
	if (!swlockout){
		if (ffscDebug.Flags & FDBF_SWITCH) {
			logMsg("In sw_isr, transmitting swbits %d\r\n",
			       swbits,0,0,0,0,0);
		}
		swlockout=1;
		semGive(debouncesema);		/* start debounce delay */
		i=write(DispReqFD,"i",2);
		if (i != 2  &&  (ffscDebug.Flags & FDBF_SWITCH)) {
			logMsg("In sw_isr, got %d from write\r\n",
			       i,0,0,0,0,0);
		}
	}
}


/*
 * initswitches
 *	Set up an interrupt service routine to field switch presses
 */
STATUS 
initswitches(void)
{

	int lockkey;

	/*
	** Set up an array to hold the switch samples.
	*/
	swread_len = ffscTune[TUNE_NUM_SWITCH_SAMPLES];
	swread = (int *) malloc(swread_len * sizeof(int));
	if (swread == NULL) {
		ffscMsg("Unable to allocate switch sample array - "
			    "switches are disabled");
		return ERROR;
	}

	/*
	** Disable interrupts before setting up anything.
	*/

	lockkey = intLock();

	/*
	** Enable the RX data ready interrupt.
	*/

	sysOutByte( SWITCH_PORT + IER, 0x01);
	sysOutByte( SWITCH_PORT + MCR,
		    sysInByte(SWITCH_PORT + MCR) | INT_ENABLE);

	/* Initialize interrupt controller */

	if (sysIntEnablePIC(SWITCH_INT) == ERROR) 
	{
		perror("Error initializing interrupt controller");
		return ERROR;
	}

	/*
	** Create a switch press isr.
	*/
    
	if (intConnect( (VOIDFUNCPTR *) INUM_TO_IVEC(37), sw_isr, 0 ) == ERROR)
	{
		perror("Error connecting ISR");
		return ERROR;
	}

	swlockout=1;		/* prevent switch ISRs until ready */

	/*
	** Enable interrupts now that setup is done.
	*/
    
	intUnlock(lockkey);
	return OK;
}


/*
 * swdelay
 *	A simple task that arranges for key presses to be ignored for
 *	a short time, generally for debounce purposes
 */
void 
swdelay(void)
{

	struct timespec delay;

	while (1) {
		swlockout = 0;				/* enable switch ISR */
		semTake(debouncesema, WAIT_FOREVER);	/* wait for isr */

		if (ffscDebug.Flags & FDBF_SWITCH) {
			ffscMsg("In swdelay, got debouncesema");
		}

		/* Recalculate the delay every time, since it may have	*/
		/* been retuned since the last time we ran.		*/
		delay.tv_sec  = ffscTune[TUNE_DEBOUNCE_DELAY] / 1000000;
		delay.tv_nsec = ((ffscTune[TUNE_DEBOUNCE_DELAY] % 1000000)
				 * 1000);
		nanosleep(&delay, NULL);	/* delay to lock out bounces */

		if (ffscDebug.Flags & FDBF_SWITCH) {
			ffscMsg("In swdelay, after nanosleep");
		}
	}
}


/*
 * initdisplay
 *	Initialize the FFSC display handlers
 */
void 
initdisplay(void) 
{

	logFdSet(2);		/* logging to stderr */

	debouncesema = semBCreate(SEM_Q_FIFO, SEM_EMPTY);
	dispfd = open("/pcConsole/0",O_WRONLY,0);
	ioTaskStdSet(0,1,dispfd);

	initswitches();
}


/*
 * sampleswitches
 *	Poll the display switches for the specified number of iterations
 *	and returns the flags corresponding to the *pressed* switches.
 *	Useful at boot-time for reading special "messages" from the user.
 *	It probably isn't a good idea to call this function after the
 *	formal switch processing functions (sw_isr, etc.) have been set up.
 */
int
sampleswitches(int Samples)
{
	int i;
	int swbits;

	swbits = SWITCH_ALL;
	for (i = 0;  i < Samples;  ++i) {
		swbits &= sysInByte(SWITCH_PORT) & SWITCH_MASK;
	}

	return (~swbits) & SWITCH_ALL;
}
