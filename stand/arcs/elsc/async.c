/*
	ASYNC.C:    Contains circular buffer handling routines
*/
//#include <16c74.h>
//#include <elsc.h>
//#include <async.h>
//#include <proto.h>


/************************************************************************
 *																								*
 *	async_init	handles the initialization of the SCI hardware ports		*
 *				and the memory data structures										*
 *																								*
************************************************************************/
void 
async_init()
{


	/* initialize the data structures */

	/* SCI PORT */
	/* Receive queue */
	sci_inq.c_inp 		= rxbuffer;
	sci_inq.c_outp 	= rxbuffer;
	sci_inq.c_len		= 0;
	sci_inq.c_start 	= rxbuffer;
	sci_inq.c_end 		= sci_inq.c_start + ASYNCBUFSZ;
	sci_inq.c_maxlen 	= ASYNCBUFSZ;

	/* Transmit queue */
	sci_outq.c_inp 	= txbuffer;
	sci_outq.c_outp 	= txbuffer;
	sci_outq.c_len		= 0;
	sci_outq.c_start 	= txbuffer;
	sci_outq.c_end 	= sci_outq.c_start + ASYNCBUFSZ;
	sci_outq.c_maxlen = ASYNCBUFSZ;
	

	/* Hardware Port Setup */
	TXSTA = 0;					/* Initialize the port */
	RCSTA = 0;					/* Initialize the port */

	SPBRG = 12;

	/* purge any junk that may be in the receiver register */
	ch = RCREG;
	ch = RCREG;

	RCSTA.SPEN = 1;				/* Configure ports for serial mode */
	RCSTA.CREN = 1;				/* Enable Reception, should be last */
	TXSTA.TXEN = 1;				/* Enable Transmitter now, and enable interrupts later */
	
#ifdef MODEM
	/* Enable the Flow Control bits */
#ifdef	SPEEDO
	reg_state.reg_out1.DTR = 0;
#ifdef RTS_CORRECTION
	reg_state.reg_out1.RTS = 0;
#else
	reg_state.reg_out1.RTS = 1;
#endif
	ext_ptr = OUTP1;
	*ext_ptr = reg_state.reg_out1;
	
#else	//SPEEDO
	
	reg_state.reg_out3.DTR = 0;
#ifdef RTS_CORRECTION
	reg_state.reg_out3.RTS = 0;
#else
	reg_state.reg_out3.RTS = 1;
#endif
	ext_ptr = OUTP3;
	*ext_ptr = reg_state.reg_out3;
#endif
#endif

}

#ifdef INDIRECT_Q

/************************************************************************
 *																								*
 *	flush	Flush modifies pointer to the desired character buffer			*
 *			to an initial state to flush the queue.								*
 *																								*
 *			Due to Byte Crafts in-ability to use pointers to structures		*
 *			a generic routine with pointer could not be used.					*
 *																								*
************************************************************************/
void
flush(qp)
int near * qp;							/* Queue to flush */
{

	qp->c_inp = qp->c_start;
	qp->c_outp = qp->c_start;
	qp->c_len = 0;

}


/************************************************************************
 *																		*
 *	addq	Add a character to a circular queue.  Inputs are constant	*
 *			that indicates the queue to work on, and the character to	*
 *			add.														*
 *																		*
 *			Due to Byte Crafts in-ability to use pointers to structures	*
 *			a generic routine with pointer could not be used.			*
 *																		*
************************************************************************/
void
addq(qp, c)
int near 	*qp;							/* Queue to add a character to */
char	c;								/* character to be added */
{

	*(qp->c_inp) = c;
	qp->c_inp++;
	qp->c_len++;
	/* Separate statements are necessary because of compiler limitations
	 * handling the 'if' statements with data structure elements
	 */
	q_var1 = qp->c_inp;
	q_var2 = qp->c_end;
	if (q_var1 >= q_var2)
		qp->c_inp = qp->c_start;

}

/************************************************************************
 *																								 *
 *	chkq	Returns the next character on the queue but does not update the *
 *			output pointers.																 *
 *																								 *
 *			Allows evaluating the next character on the queue before 		 *
 *			removing it with the remq													 *
 *																								 *
 *																								 *
*************************************************************************/
unsigned char
chkq(qp)
int near	*qp;				/* queue to add the character to */
{
	char	c;		/* temporary character holder	*/

	if (qp->c_len > 0)
		c = *(qp->c_outp);
	else
		c = 0;
		
	return(c);
}


/************************************************************************
 *																								*
 *	remq	Remove a character to a circular queue.  The input is a 			*
 *			constant that indicates the queue to work on. 						*
 *																								*
 *			Due to Byte Crafts in-ability to use pointers to structures		*
 *			a generic routine with pointer could not be used.					*
 *																								*
************************************************************************/
unsigned char
remq(qp)
int near	*qp;				/* queue to add the character to */
{
	char	c;		/* temporary character holder	*/

	if (qp->c_len > 0) {
		c = *(qp->c_outp);
		qp->c_outp++;
		qp->c_len--;
		/* Separate statements are necessary because of compiler limitations
	 	* handling the 'if' statements with data structure elements
	 	*/
		q_var1 = qp->c_outp;
		q_var2 = qp->c_end;
		if (q_var1 >= q_var2)
			qp->c_outp = qp->c_start;
	}
	else
		c = 0;
		
	return(c);
}

#else
/************************************************************************
 *																								*
 *	flush	Flush modifies pointer to the desired character buffer			*
 *			to an initial state to flush the queue.								*
 *																								*
 *			Due to Byte Crafts in-ability to use pointers to structures		*
 *			a generic routine with pointer could not be used.					*
 *																								*
************************************************************************/
void
flush(queue)
int queue;							/* Queue to flush */
{
	switch (queue) {
		case	SCI_IN:
			sci_inq.c_inp = sci_inq.c_start;
			sci_inq.c_outp = sci_inq.c_start;
			sci_inq.c_len = 0;
			break;
		case	SCI_OUT:
			sci_outq.c_inp = sci_outq.c_start;
			sci_outq.c_outp = sci_outq.c_start;
			sci_outq.c_len = 0;
			break;
		case	I2C_IN:
			i2c_inq.c_inp = i2c_inq.c_start;
			i2c_inq.c_outp = i2c_inq.c_start;
			i2c_inq.c_len = 0;
			break;
		case	I2C_OUT:
			i2c_outq.c_inp = i2c_outq.c_start;
			i2c_outq.c_outp = i2c_outq.c_start;
			i2c_outq.c_len = 0;
			break;
		default:
			break;
	}

}


/************************************************************************
 *																		*
 *	addq	Add a character to a circular queue.  Inputs are constant	*
 *			that indicates the queue to work on, and the character to	*
 *			add.														*
 *																		*
 *			Due to Byte Crafts in-ability to use pointers to structures	*
 *			a generic routine with pointer could not be used.			*
 *																		*
************************************************************************/
void
addq(queue, c)
int queue;							/* Queue to add a character to */
char	c;								/* character to be added */
{
	switch (queue) {
		case	SCI_IN:
			*(sci_inq.c_inp) = c;
			sci_inq.c_inp++;
			sci_inq.c_len++;
			if (sci_inq.c_inp >= sci_inq.c_end)
				sci_inq.c_inp = sci_inq.c_start;
			break;
		case	SCI_OUT:
			*(sci_outq.c_inp) = c;
			sci_outq.c_inp++;
			sci_outq.c_len++;
			if (sci_outq.c_inp >= sci_outq.c_end)
				sci_outq.c_inp = sci_outq.c_start;
			break;
		case	I2C_IN:
			*(i2c_inq.c_inp) = c;
			i2c_inq.c_inp++;
			i2c_inq.c_len++;
			if (i2c_inq.c_inp >= i2c_inq.c_end)
				i2c_inq.c_inp = i2c_inq.c_start;
			break;
		case	I2C_OUT:
			*(i2c_outq.c_inp) = c;
			i2c_outq.c_inp++;
			i2c_outq.c_len++;
			if (i2c_outq.c_inp >= i2c_outq.c_end)
				i2c_outq.c_inp = i2c_outq.c_start;
			break;
		default:
			break;
	}

}
/************************************************************************
 *																								 *
 *	chkq	Returns the next character on the queue but does not update the *
 *			output pointers.																 *
 *																								 *
 *			Allows evaluating the next character on the queue before 		 *
 *			removing it with the remq													 *
 *																								 *
 *																								 *
*************************************************************************/
unsigned char
chkq(queue)
int	queue;				/* queue to add the character to */
{
	char	c;		/* temporary character holder	*/

	switch (queue) {
		case	SCI_IN:
			if (sci_inq.c_len > 0)
				c = *(sci_inq.c_outp);
			else
				c = 0;
			break;
			
		case	SCI_OUT:
			if (sci_outq.c_len > 0) 
				c = *(sci_outq.c_outp);
			else
				c = 0;
			break;
		case	I2C_IN:
			if (i2c_inq.c_len > 0) 
				c = *(i2c_inq.c_outp);
			else
				c = 0;
			break;
		case	I2C_OUT:
			if (i2c_outq.c_len > 0) 
				c = *(i2c_outq.c_outp);
			else
				c = 0;
			break;
		default:
			break;
	}
	
	return(c);
}


/************************************************************************
 *																								*
 *	remq	Remove a character to a circular queue.  The input is a 			*
 *			constant that indicates the queue to work on. 						*
 *																								*
 *			Due to Byte Crafts in-ability to use pointers to structures		*
 *			a generic routine with pointer could not be used.					*
 *																								*
************************************************************************/
unsigned char
remq(queue)
int	queue;				/* queue to add the character to */
{
	char	c;		/* temporary character holder	*/

	switch (queue) {
		case	SCI_IN:
			if (sci_inq.c_len > 0) {
				c = *(sci_inq.c_outp);
				sci_inq.c_outp++;
				sci_inq.c_len--;
				if (sci_inq.c_outp >= sci_inq.c_end)
					sci_inq.c_outp = sci_inq.c_start;
			}
			else
				c = 0;
			break;
			
		case	SCI_OUT:
			if (sci_outq.c_len > 0) {
				c = *(sci_outq.c_outp);
				sci_outq.c_outp++;
				sci_outq.c_len--;
				if (sci_outq.c_outp >= sci_outq.c_end)
					sci_outq.c_outp = sci_outq.c_start;
			}
			else
				c = 0;
			break;
		case	I2C_IN:
			if (i2c_inq.c_len > 0) {
				c = *(i2c_inq.c_outp);
				i2c_inq.c_outp++;
				i2c_inq.c_len--;
				if (i2c_inq.c_outp >= i2c_inq.c_end)
					i2c_inq.c_outp = i2c_inq.c_start;
			}
			else
				c = 0;
			break;
		case	I2C_OUT:
			if (i2c_outq.c_len > 0) {
				c = *(i2c_outq.c_outp);
				i2c_outq.c_outp++;
				i2c_outq.c_len--;
				if (i2c_outq.c_outp >= i2c_outq.c_end)
					i2c_outq.c_outp = i2c_outq.c_start;
			}
			else
				c = 0;
			break;
		default:
			break;
	}
	
	return(c);
}

#endif
