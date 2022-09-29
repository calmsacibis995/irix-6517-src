
#pragma option -l;  // Disable header inclusion in listing
/*
		Silicon Graphics Computer Systems
        Header file for Lego Entry Level System Controller

                        cmd.h

        (c) Copyright 1995   Silicon Graphics Computer Systems

		Brad Morrow
*/


#define	SCI_CMD			1
#define	I2C_CMD			2
#define	CUT_SPACE 		1
#define 	INCLUDE_SPACE 	0

#define	NULL		0x00
#define	BS			0x08
#define	NL			0x0a
#define	CR			0x0d	
#define	DEL		0x7f

struct CMD_INT {
	bits 				cmd_intr;
	unsigned char	cmd_num;
	bits				cmd_stat;
	char				rsp_err;
	char				acp_int_state;	/* state of ACP Interrupt */
	char				cpu_sel;			/* CPU selected for ACP input */
	char				cpu_acp;			/* Last CPU to receive ACP output */
	char				cpu_see;			/* CPU if hub filtering is on */
	char				cpu_mask;		/* status bit map for CPUs receiving an ELSC interrutp*/
};


/* common between cmd_intr */
#define 	CMD_SCI			0			/*cmd counter bit 0 */
#define 	CMD_I2C			1			/*cmd counter bit 1 */
#define 	PASS_THRU		2			/* Pass Thru mode = 1; local = 0; */
#define	ELSC_INT			3			/* ELSC interrupt message waiting */
#define	ELSC_I2C_REQ	4			/* Request from ELSC to use I2C bus */
#define	ELSC_I2C_GRANT	5			/* ELSC is clear to use the I2C Bus */
#define	TOKEN_ENABLE	6			/* Enable the sending of General Calls on I2C */
#define	SCI_CMD_REQ		7			/* request for a SCI command, set in irq.c */

/* cmd_stat bit assignments */
#define SCI_I2C			0			/* I2C = 1; SCI = 0 */
#define	I2C_OUTGOING	1			/* characters ready to be sent on I2C */
#define SUP_MODE			2			/* Supervisor mode = 1; user mode = 0 */
#define CH_ECHO			3			/* Echo character to terminal when set */
#define FAN_HI				4			/* Fan has been set to high by command */
#define CNTLT_CHAR		5			/* Last character received was a control-T */

#define	PARAM_OK			0			/* Parameter fetched from queue and converted to long*/
#define	NO_PARAM			-1			/* No Parameter present on queue */
#define	PARAM_ERR		-2			/* Invalid parameter character */

#define	SEL_NONE			-1			/* Select none value for passing of keyboard input */
#define	SEL_AUTO			-2			/* Auto select value for passing of keyboard input */

/* ACP Interrupt States */
#define	ACP_BUF_EMPTY	0
#define	ACP_BUF_FULL	1

/* Command Numbers */

#define INVALID_CMD	0x00
#define	C_RETURN		0x01		/* Carridge Return */
#define	ACP			0x02		/* Pass through cmd from Hub to ACP */
#define	DBG			0x03		/*	Debug Switch Settings */
#define	DSC			0x04		/* Display Character */
#define	DSP			0x05		/* displays a message on the 1x8 display */
#define	ECH			0x06		/*	Echo Command */
#define	FAN			0x07		/*	Fan Control */
#define	FRE			0x08		/*	Frequence Margin Command */
#define 	HBT			0x09		/*	Heart Beat Command */
#define	HUB			0x0A		/*	Hub Address for I2C Communitcation */
#define 	NMI			0x0B		/*	NMI Command */
#define	OFF			0x0C		/*	Off command from Hub */
#define	PAS			0x0D		/* Password Command */
#define	PNG			0x0E		/*	Ping Command */
#define 	PWR			0x0F		/* Module Power Control */
#define	RCF			0x10		/*	Read Configuration */
#define	RST			0x11		/*	Module Reset Command */
#define	RSW			0x12		/* Read DIP Switch settings */
#define	SEE			0x13		/* See only one HUB's output */
#define 	SEL			0x14		/*	Select HUB */
#define	TAS			0x15		/* Test and Set lock for the Hubs */
#define	TMP			0x16		/* Get Module Temperature Status */
#define	VER			0x17		/* Returns the firmware version number */
#define	VLM			0x18		/* Voltage margin setting */
#define	CLR			0x19		/* Place ELSC into power on state */
#define	GET			0x1a		/*	Hub Requesting Interrupt data or ACP data */
#define	NIC			0x1b		/* Primitive commands for Mid-Plane NIC */
#define	KEY			0x1c		/* Command to request the state of the Key switch */
#define	FSC			0x1d		/* FFSC notification of presence */
#define	MOD			0x1e		/* Module Number */
#define	CFG			0x1f		/* NIC Configuration Information stored in NVRAM */
#define	AUT			0x20		/* Auto power up after PFW or reset of MSC */
#define	SLA			0x21		/* Slave ELSC to Master ELSC command */
#define	TST			0x22		/* Manufacturing Test setting to reduce the fan timeout */
#define	NVM			0x23		/* NVRAM Access command. */

/* HUB Response Error Status */
#define	OK			0
#define	CSUM		1		/* Check sum error */
#define	CMD		2		/* Invalid Command */
#define	ARG		3		/* Invalid Command Argument */
#define	PERM		4		/* Invalid Command Permissions */
#define	CLEN		5		/* Byte Count did not match actual data */
#define	SIZE		6		/* ELSC Command Too Large For Queue */

#ifdef	SPEEDO
/* Slave ELSC to Master ELSC parameters */
#define	SLAVE_NT		'n'		/* Slave Normal Temperature */
#define	SLAVE_HT		'h'		/* Slave High Temperature */
#define	SLAVE_OT		'o'		/* Slave Over Temperature */
#define	SLAVE_FF		4		/* Slave Fan Fail */
#define	SLAVE_MFF	5		/* Slave Multiple Fan Fail */
#define	SLAVE_PSF	6		/* Power Supply Fail */
#define	SLAVE_PR		7		/* Slave Present */
#endif
