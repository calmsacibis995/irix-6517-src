#pragma option -l;  // Disable header inclusion in listing
//#ifndef	_I2C_H_
//#define	_I2C_H_
/*
		Silicon Graphics Computer Systems
        Header file for Lego Entry Level System Controller

                        i2c.h

        (c) Copyright 1995   Silicon Graphics Computer Systems

		Brad Morrow
*/

/* I2C receive buffer must be large enough to hold the largest
 * command from the hub.  This would be 90:
 * ByteCount "acp" Space n Space <80-chars> "\r\n" CheckSum
 * We leave a couple extra.
 */
#define I2CRBUFSZ		92
#define I2CRHIWATER	89

/* I2C transmit buffer must be large enough to hold the largest
 * response from the ELSC.  This would probably be an ACP response
 * with several characters in it. */
 
#define I2CTBUFSZ		20
#define I2CTHIWATER	17

/*
 * Response buffer must be large enough to hold the largest
 * response other than ACP.
 */
#define RSPBUFSZ		20		/* size of I2C Command Response Buffer */
#define RSP_HI_WATER 16		/* Hi water mark of response queue */

#define ACPBUFSZ		20		/* size of ACP Interrupt buffer */

/* Port Address */
#define	I2C_S0		(long) 0x2000	/* Address of PCF8584 data shift register */
#define	I2C_S1		(long) 0x2001	/* Address of PCF8584 control register */

/* I2C Bus Address */
#define	I2C_BUS_TOKEN		0xe0
#define	ELSC_I2C_ADD		0x10
#define 	HUB0_I2C_ADD		0x12
#define 	I2C_WRITE			0
#define 	I2C_READ				1
#define 	I2C_RETRY_CNT		4		/* Actual count is one less then value */
#define 	I2C_WAIT_COUNT  	0x7F	/* Number of times to wait for a free bus*/
#ifdef	SPEEDO
#define	ELSC_SLAVE_I2C_ADD 0x08
#define	ELSC_SLAVE_TOKEN	I2C_BUS_TOKEN | 4	 
#endif
	
/* Control Characters */
#define CNTL_T				0x14	/* Control T ascii code */

/* PCF8584 Control register values */
#define	CTL_PIN		0x80		/* Pending Interrupt Not */
#define	CTL_ESO		0x40		/* Enable Serial Output */
#define	CTL_ES1		0x20		/* Register Control Selection */
#define	CTL_ES2		0x10		/* Register Control Selection */
#define	CTL_ENI		0x08		/* Enable External Interrupt */
#define	CTL_STA		0x04		/* Send Start Bit */
#define	CTL_STO		0x02		/* Send Stop Bit */
#define	CTL_ACK		0x01		/* ACK enable */

/* PCF8584 Status register values */
#define	STA_PIN		0x80		/* Pending Interrupt Bit */
#define	STA_STS		0x20		/* External Stop bit recieved (slave mode only) */
#define	STA_BER		0x10		/* Bus Error Mis-placed stop bit */
#define	STA_AD0_LRB	0x08		/* Last Received bit*/
#define	STA_AAS		0x04		/* Addressed as Slave*/
#define	STA_LAB		0x02		/* Lost Arbitration Bit*/
#define	STA_BBN		0x01		/* Bus Busy NOT*/

struct	i2c_scb {
	char	xfer_type;			/* type of transfer, HUB, NVRAM, ONE, MULTI BYTE */
	char	token_add;			/* cpu number for general call message */
	char 	token_cnt;
	char	i2c_add;				/* i2c slave address */
	char	i2c_sub_add;		/* NVRAM byte address */
	char	*xfer_ptr;			/* pointer to ELSC RAM for storage */
	char	xfer_count;			/* number of bytes to transfer */
	char	bytes_xfered;
};

#define	I2C_NVRAM		2

#define	I2C_TOKEN		3
#define	M_FLAG			0x08

#define 	I2C_PIN_TO		1

/* I2C Status Flags */

/* Bit Definitions */


/* I2C Error Codes */
#define	I2C_ERROR_NONE			0
#define 	I2C_ERROR_INIT			-1		/* Initialization Error */
#define	I2C_ERROR_TO_BUSY		-2		/* Timed out waiting for busy bus */
#define 	I2C_ERROR_TO_SENDA	-3		/* Timed out sending address byte */
#define	I2C_ERROR_TO_SENDD	-4		/* Timed out sending data byte */
#define 	I2C_ERROR_TO_RECVA	-5		/* Timed out receiving address byte */
#define	I2C_ERROR_TO_RECVD	-6		/* Timed out receiving data byte */
#define	I2C_ERROR_TO_SLAVE	-7		/* Timed out on being addressed as slave */
#define 	I2C_ERROR_BUS			-8		/* Illegal Bus State */
#define 	I2C_ERROR_NAK			-9		/* Addressed slave not responding */
#define	I2C_ERROR_ARB			-10	/* Lost Arbitration */
#define	I2C_ERROR_SLAVE		-11	/* Can't master: addressed as slave */
#define	I2C_ERROR_OVFLOW		-12	/* Slave receive buffer overflow */
#define	I2C_SLAVE_TOKEN_TO	-13	/* Token never recieved from Master ELSC */


/* I2C Slave definitions */

struct slave_pcb {
	char	slave_stat;			/* I2C Slave port command status */
	char	i2c_cmd_stat;		/* Status of current i2c command */
	char	byte_count;			/* Byte count expected */
	char	bytes_rec;			/* Running count of bytes received */
	char	bytes_used;			/* Number of bytes loaded into SCI_OUT queue */
	char	chk_sum;				/* Running check sum, added to check sum byte */
	char	sci_xfer_ct;		/* Bytes to be transfered from I2C to SCI output queue */
	char	slave_bus;			/* logging of any bus errors */
};

/* SLAVE_STAT Values */
#define	SLAVE_IDLE			0			/* Slave Interface Idle */
#define	SLAVE_RECV			1			/* ELSC receiving data from hub */
#define	SLAVE_XMIT			2			/* ELSC tranmitting data to hub */
#define	HUB_CMD_REC			3			/* Receive of command mnemonic in process */
#define	ACP_CMD				4			/* Command is pass thru data to SCI output port */
#define	ELSC_CMD				5			/* Command will be executed on ELSC */
#define 	ACP_REJECT_CHAR	6			/* SCI Output queue is full, reject remaining ch */
#define	ELSC_REJECT_CHAR	7			/* Command was larger than incoming queue */
#define	CMD_EXEC				8			/* Waiting for command to execute */
#define	HUB_RSP_RDY			9			/* Command Response is Ready to transmit */
#define	HUB_RSP_XMIT		10			/* Commnad Response Transmit in Process */
#define	HUB_RSP_HOLD		11			/* Response is on hold until command completion */

#define	I2C_CMD_IDLE		0			/* no Command present */
#define	I2C_CMD_RECV		1			/* Command is being received */
#define	I2C_CMD_WAIT		2			/* Command is being executed */
#define 	I2C_CMD_DONE		3			/* Command is done read with response */
#define	I2C_CMD_BLOCKED	4			/* Command (acp) block because of full queue */

//#endif /* _I2C_H_ */
#pragma option +l;  // Enable header inclusion in listing


