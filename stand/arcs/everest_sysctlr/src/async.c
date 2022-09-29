/*
	ASYNC.C:    Contains circular buffer handling routines
*/

#include <stdio.h>
#include <string.h>
#include <sp.h>
#include <async.h>
#include <proto.h>
#include <sys_hw.h>
#include <io6811.h>

char	rx1buffer[ASYNCBUFSZ * 2];			/* SCI Receive Buffer */
char	tx1buffer[ASYNCBUFSZ];			/* SCI Transmit Buffer */
char	rx2buffer[ASYNCBUFSZ];			/* ACIA Receive Buffer */
char	tx2buffer[ASYNCBUFSZ];			/* ACIA Transmit Buffer */
unsigned char	acia_cntl;				/* Memory copy of control reg */

struct cbufh 	sci_inq,			/* Input SCI data queue */
				sci_outq,			/* Output SCI data queue */
				acia_inq,			/* Input ACIA data queue */
				acia_outq;			/* Output ACIA data queue */

struct cbufh	*rx1qp = &sci_inq,
				*tx1qp = &sci_outq,
				*rx2qp = &acia_inq,
				*tx2qp = &acia_outq;

/************************************************************************
 *																		*
 *	async_init	handles the initialization of the SCI hardware ports	*
 *				and the memory data structures							*
 *																		*
************************************************************************/
non_banked void 
async_init(mode)
char	mode;			/* 0 = all, 1 = hardware only */
{
	struct cbufh	*qptr;			/* pointer to cbuf structure */
	extern char		mf_cmd;
	char			ch;

	/* initialize the data structures */

	/* SCI PORT */
	/* Receive queue */
	if (mode == INIT_ALL) {
		qptr 			= &sci_inq;
		qptr->c_inp 	= rx1buffer;
		qptr->c_outp 	= rx1buffer;
		qptr->c_len		= 0;
		qptr->c_start 	= rx1buffer;
		qptr->c_end 	= qptr->c_start + (ASYNCBUFSZ * 2);
		qptr->c_maxlen 	= ASYNCBUFSZ * 2;

		/* Transmit queue */
		qptr 			= &sci_outq;
		qptr->c_inp 	= tx1buffer;
		qptr->c_outp 	= tx1buffer;
		qptr->c_len		= 0;
		qptr->c_start 	= tx1buffer;
		qptr->c_end 	= qptr->c_start + ASYNCBUFSZ;
		qptr->c_maxlen 	= ASYNCBUFSZ;

		/* ACIA PORT */

		mf_cmd = 0;			/*initialize manf cmd mode */

		/* Receive queue */
		qptr 			= &acia_inq;
		qptr->c_inp 	= rx2buffer;
		qptr->c_outp 	= rx2buffer;
		qptr->c_len		= 0;
		qptr->c_start 	= rx2buffer;
		qptr->c_end 	= qptr->c_start + ASYNCBUFSZ;
		qptr->c_maxlen 	= ASYNCBUFSZ;

		/* Transmit queue */
		qptr 			= &acia_outq;
		qptr->c_inp 	= tx2buffer;
		qptr->c_outp 	= tx2buffer;
		qptr->c_len		= 0;
		qptr->c_start 	= tx2buffer;
		qptr->c_end 	= qptr->c_start + ASYNCBUFSZ;
		qptr->c_maxlen 	= ASYNCBUFSZ;
	}

	/* Hardware Port Setup */
	DDRD = 0x02;			/* bit 0 receiver, bit 1 xmitter */
	BAUD = BAUD_9600;		/* 9600 Baud */
	SCCR1 = 0x0;			/* 8 bit character length */

	/* purge any junk that may be in the receiver register */
	ch = SCDR;


	/*Enable Transmitter and Receiver Ports and Receiver Interrupt*/
	SCCR2 = SCCR2_RIE |  SCCR2_TE | SCCR2_RE; 

	/* ACIA hardware initialization */

	/* Issue a master reset*/
	*L_ACIA_STCT = ACIA_CR1 | ACIA_CR0;

	/* Control Register Settings:	Divide by 16 			(!CR1 and CR0)
	 *								8 bits + 1 stop bit		(CR4,!CR3,CR2)
	 *								TX Interrupt Disabled	(!CR6,!CR5)
	 *								RX Interrupt Enabled	(CR7)
	 */

	acia_cntl = ACIA_CR7 | ACIA_CR4 | ACIA_CR2 | ACIA_CR0;
	*L_ACIA_STCT = acia_cntl;

	/* purge any junk that may be in the receiver register */
	ch = *L_ACIA_TXRX;


}

/*	flush -
		Initialize a circular queue. Input is the pointer to the queue.
*/

non_banked void
flush(qhdr)
struct cbufh	*qhdr;		/* pointer to q header */
{

	qhdr->c_inp = qhdr->c_outp = qhdr->c_start;
	qhdr->c_len = 0;
}

/*	addq -
		Add a character to a circular queue. Inputs are a pointer
		to the queue and the character to add.
*/

non_banked void
addq(qp, c)
struct cbufh	*qp;		/* pointer to the queue	*/
char			c;			/* character to be added */
{
	char	q_error;

	q_error = 0;
	*qp->c_inp++ = c;
	qp->c_len++;
	if (qp->c_inp >= qp->c_end)
		qp->c_inp = qp->c_start;
	if (qp->c_len >= qp->c_maxlen)
		q_error = 1;
}

/************************************************************************
 *																		*
 *	GETCMD	retrieves one CPU command from the circular queue.  All		*
 *			CPU commands are terminated with a null character			*
 *																		*
 *			4/21/93 -	Added a check command length feature to detect	*
 *						dropped characters.  The checking of the length *
 *						is settable by a command from the cpu, and		*
 *						initializes to a non_checking mode				*
 ************************************************************************/
non_banked char
getcmd(qp, buff)
struct cbufh	*qp;			/* pointer to the queue */
char			*buff;			/* pointer to command buffer */
{
 	int		buf_len,			/* length of received command */
			len,
			loop;				/* general loop varible */
	char	ret_val;			/* return value, 	0 = complete command
													-1 = incomplete command*/
	char	*bptr;
	struct RTC				*ptod = (struct RTC *) L_RTC;	/* Pointer to RTC */

	ret_val = 0;
	buf_len = 0;
	loop = 1000;
	bptr = buff;

	while (loop--) {
		*bptr = *qp->c_outp++;
		qp->c_len--;
		if (qp->c_outp >= qp->c_end)
			qp->c_outp = qp->c_start;

		if(*bptr == '\n') {
			*bptr++ = 0;
			break;
		}
		else 
			bptr++;
	}

	if(loop <= 0)
		ret_val = -1;

	else if(ptod->nv_cmd_chk){					/* Verify length of command */
		buf_len = strlen(buff);
		if(buff[0] == 'G') {				/* Get command */
			sscanf(&buff[3],"%2x", &len);
			if(buf_len != len)
				ret_val = -1;
			else
				strcpy(&buff[3], &buff[5]);
		}
		else if(buff[0] == 'S') {			/* Set command */
			sscanf(&buff[2],"%2x", &len);
			if(buf_len != len)
				ret_val = -1;
			else
				strcpy(&buff[2], &buff[4]);
		}
		else								/* Invalid First Character */
			ret_val = -1;
	}
 	return(ret_val);
}

/************************************************************************
 *																		*
 *	PUTCMD	loads one CPU command into the circular queue.  All			*
 *			CPU commands are terminated with a null character			*
 *																		*
 ************************************************************************/
non_banked char
putcmd(qp, buff)
struct cbufh	*qp;			/* pointer to the queue */
char			*buff;			/* pointer to command buffer */
{
 	int		buf_len,			/* length of received command */
			i,
			loop, loop1,		/* general loop varible */
			num_char;
	char	ret_val;			/* return value, 	0 = complete command */
	char	*sptr;
	char	ch_len[3];
	struct RTC				*ptod = (struct RTC *) L_RTC;	/* Pointer to RTC */

	ret_val = 0;
	buf_len = 0;
	loop = 1;

	if(ptod->nv_cmd_chk) {					/* Verify length of command */
		buf_len = (strlen(buff) + 2);
		buf_len &= 0xFF;
		sprintf(ch_len, "%02X", buf_len);
		sptr = strchr(buff, 0);

		if (buff[0] == 'R') {
			/* find the number of characters to shift including the null */
			num_char = sptr - (buff + 3) + 1;   
			for (i = 0; i < num_char; i++, sptr--)
				*(sptr+2) = *sptr;
			buff[3] = ch_len[0];
			buff[4] = ch_len[1];
		}
		else
			strcat(buff, ch_len);
	}

	while (loop) {
		if(qp->c_len > HIWATER) {
			loop1 = 1000;
			SCCR2 |= SCCR2_TIE;				/* make sure the TX is enabled */ 
			while (loop1-- && qp->c_len > LOWWATER)
				delay(SW_1MS);
			if(loop1 <= 0) {
				ret_val = -1;
				break;
			}
		}
		*qp->c_inp++ = *buff;
		qp->c_len++;
		if (qp->c_inp >= qp->c_end)
			qp->c_inp = qp->c_start;
		if(*buff++ == 0)
			break;
	}
	SCCR2 |= SCCR2_TIE;				/* enable TX interrupt */ 
	if(loop1 <= 0)
		ret_val = -1;
 	return(ret_val);
}

/*	remq -
		Remove the next character from a circular queue. Input is
		a pointer to the queue. Returns the next charcter or a 
		zero if the queue is empty.
*/

non_banked unsigned char
remq(qp)
struct cbufh	*qp;	/* circular buffer descriptor	*/
{
	register char	c;		/* temporary character holder	*/

	if (qp->c_len) {
		c = *qp->c_outp++;
		qp->c_len--;
		if (qp->c_outp >= qp->c_end)
			qp->c_outp = qp->c_start;
	}
	else
		c = 0;
	return(c);
}

