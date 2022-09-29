

/*	async.h	*/

/*
 *	Async port related defines and structures
 */

#ifndef _ASYNC_INCLUDED
#define _ASYNC_INCLUDED

/* circular buffer header */

#define HIWATER		120		/* hi water value */
#define LOWWATER	20		/* low water value */
#define ASYNCBUFSZ	128		/* size of SCI buffers */

struct cbufh {
	char	*c_inp;		/* input pointer	*/
	char	*c_outp;	/* output pointer	*/
	short	c_len;		/* # chars in buffer	*/
	char	*c_start;	/* physical buf start	*/
	char	*c_end;		/* physical buf end	*/
	short	c_maxlen;	/* max # of chars	*/
	};

/* INTERNAL ASYNC PORT */
/* SCI Registers */
#define	BAUD_9600		(char) 0x30		/*9600 baud, prescaler of 1 */

#define	SCCR1_9BITS		(char) 0x10		/*9 bit characters */
#define	SCCR1_WAKE_ADD	(char) 0x04		/*Wake up on address mark */

#define	SCCR2_TIE		(char) 0x80		/*TransmitInterrupt Enable */
#define	SCCR2_TCIE		(char) 0x40		/*Transmit Complete Interrupt Enable */
#define	SCCR2_RIE		(char) 0x20		/*Receive Interrupt Enable */
#define	SCCR2_ILIE		(char) 0x10		/*Idle-Line Interrupt Enable */
#define	SCCR2_TE		(char) 0x08		/*TransmitEnable */
#define	SCCR2_RE		(char) 0x04		/*Receive Enable */
#define	SCCR2_RWU		(char) 0x02		/*Receiver Wake Up */
#define	SCCR2_SBK		(char) 0x01		/*Send Break */

#define	SCSR_TDRE		(char) 0x80		/*Transmit Data Reg Empty */
#define	SCSR_TC			(char) 0x40		/*Transmit Complete */
#define	SCSR_RDRF		(char) 0x20		/*Receive Data Register Full */
#define	SCSR_IDLE		(char) 0x10		/*Idle-Line Detect */
#define	SCSR_OR			(char) 0x08		/*Overrun Error */
#define	SCSR_NF			(char) 0x04		/*Noise Flag */
#define	SCSR_FE			(char) 0x02		/*Framing Error */

/* SCI PORT USAGE */
#define ST_BOOT 		(char) 0x1		/* Currently being used for boot arb */
#define ST_RDY			(char) 0x2		/* Waiting for a CPU command */
#define ST_BUSY			(char) 0x3		/* Currently receiving a CPU command */

/* EXTERNAL ASYNC PORT */
/* Asynchronous Communications Interface Adapter (ACIA) MC6850 */
#define L_ACIA_STCT	(char *) 0x01B8		/* ACIA Status/Control Register */
#define L_ACIA_TXRX	(char *) 0x01B9		/* ACIA Transmit/Receive Register */

/* Control Register */
#define	ACIA_CR0		(char) 0x01		/*Counter divide select bit 0 */
#define	ACIA_CR1		(char) 0x02		/*Counter divide select bit 1 */
#define	ACIA_CR2		(char) 0x04		/*Word select bit 0 */
#define	ACIA_CR3		(char) 0x08		/*Word select bit 1 */
#define	ACIA_CR4		(char) 0x10		/*Word select bit 2 */
#define	ACIA_CR5		(char) 0x20		/*Transmitter control bit 0 */
#define	ACIA_CR6		(char) 0x40		/*Transmitter control bit 1 */
#define	ACIA_CR7		(char) 0x80		/*Receive Interrupt Enable */

/*Status Register */
#define	ACIA_RDRF		(char) 0x01		/* Receive Data Register Full */
#define ACIA_TDRE		(char) 0x02		/* Transmit data Register Empty */
#define ACIA_DCD		(char) 0x04		/* Data Carrier Detect not present*/
#define ACIA_CTS		(char) 0x08		/* Clear To Send from modem */
#define ACIA_FE			(char) 0x10		/* Framing Error */
#define ACIA_OVRN		(char) 0x20		/* Receiver Overrun Error */
#define ACIA_PE			(char) 0x40		/* Parity Error */
#define ACIA_IRQ		(char) 0x80		/* Interrupt Request Pending */


/* General Definitions */
#define CNT_R			(char) 0x12		/* Control R */
#define CNT_$			(char) 0x1C		/* Control $ */
#define CNT_X			(char) 0x18		/* Control X */
#define CNT_Y			(char) 0x19		/* Control Y */
#define CNT_Z			(char) 0x1A		/* Control Z */
#define RSP_CHAR		(char) 'K'		/* Response to boot arbitration */
#define INIT_ALL		(char) 0x0		/* init async data struct and ports */
#define INIT_PORT		(char) 0x1		/* init async ports */
#endif
