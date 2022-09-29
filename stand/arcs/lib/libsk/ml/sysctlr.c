/***********************************************************************\
*	File:		sysctlr.c					*
*									*
*	Contains sundry routines and utilities for talking to 		*
*	the Everest system controller.					*
*									*
\***********************************************************************/

#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/sbd.h>
#include <arcs/types.h>
#include <arcs/time.h>
#include <sys/EVEREST/sysctlr.h>
#include <sys/EVEREST/nvram.h>
#include <sys/EVEREST/evconfig.h>

#define NULL 0
#define SNUMSIZE 10

/*
 * clear_incoming()
 *	Clears out any incoming characters by continuously reading
 *	from the ccuart until no more characters have arrived for
 *	25 milliseconds.
 */

void
clear_incoming()
{
	do {
		/* Clear any incoming characters */
		while (ccuart_poll()) 
			ccuart_flush();

		us_delay(25000);	/* Wait for 25 milliseconds */

	} while (ccuart_poll());
}


/*
 * spin_read()
 *	Spin a finite number of times waiting for a character to come
 *	in.  If no character shows up in 100 milliseconds, give up and
 *	return an error.  Return -1 if an error, otherwise returns the
 *	ascii character read.
 */

#define MAX_WAIT_TIME 1000000
int 
spin_read()
{
	int time_waited = 0; 

	while (time_waited < MAX_WAIT_TIME) {
		if (ccuart_poll()) {
			return ccuart_getc();
		} 
		else {
			us_delay(200);
			time_waited += 200;
		}
	}

	return -1;
}


/*
 * sysctlr_getserial()
 *	Reads the serial number from the system controller and
 *	places it in the given string.
 */

int
sysctlr_getserial(char *serial)
{
	uint i;			/* Index variable into serial string */
	int ch;			/* Character returned by spin_read() */

	/* Flush out any outstanding garbage */
	clear_incoming();

	/* Send the get serial number command to the system controller */
	ccuart_putc(SC_ESCAPE); 
	ccuart_putc(SC_GET);
	ccuart_putc(SC_SERIAL);
	ccuart_putc(SC_SERIAL);
	ccuart_putc(SC_TERM);

	/* Check for a response */ 
	if ((ch = spin_read()) != SC_RESP) {
		clear_incoming();
		printf("sysctlr_getserial: got bad response char: '%d'\n", ch);
		return -1;
	}

	/* Check for sequence number */
	if ((ch = spin_read()) != SC_SERIAL) {
		clear_incoming();
		printf("sysctlr_getserial: got bad sequence: '%d'\n", ch);
		return -1;
	}

	/* Check response type and ensure it is type serial */
	if ((ch = spin_read()) != SC_SERIAL) {
		clear_incoming();
		printf("sysctlr_getserial: got bad response type: '%d'\n", ch);
		return -1;
	}

	/* Now read in the actual serial number */
	for (i = 0; i < SNUMSIZE; i++) {
		if ((ch = spin_read()) == -1) {
			clear_incoming();
			printf("sysctlr_getserial: spin_read return -1\n");
			return -1;
		} 
		else {
			serial[i] = ch;

			if (ch == NULL)
				return 0; 
		}
	}

	/* If we get to this point, we're getting way too many 
	 * characters back in the response.  Clear them out and
	 * complain.
	 */
        serial[0] = '\0';
	clear_incoming();
	return -1;
}


/*
 * sysctlr_setserial()
 *	Set the System serial number.
 */

int
sysctlr_setserial(char *serial)
{
	int i;			/* Index variable for serial string */
	char serialbuf[SNUMSIZE+1];

	/*
	 * First we copy the data into a null-padded buffer.
	 */
	for (i = 0; i < SNUMSIZE+1; i++) 
		serialbuf[i] = NULL; 
	strncpy(serialbuf, serial, SNUMSIZE);
	strncpy(EVCFGINFO->ecfg_snum, serial, SNUMSIZE+1);

	ccuart_putc(SC_ESCAPE);
	ccuart_putc(SC_SET);
	ccuart_putc(SC_SERIAL);
	
	for (i = 0; i < SNUMSIZE; i++) 
		ccuart_putc(serialbuf[i]);

	ccuart_putc(SC_TERM);

	return 0;
}

