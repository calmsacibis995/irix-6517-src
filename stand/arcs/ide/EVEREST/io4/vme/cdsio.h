#define	CDSIO_BASE_ADDRESS		0xf00000
#define	BLK_SIZE			0x2000
#define	NUMBER_OF_PORTS			6

#define SGI_FIRMWARE 20000              /* versions > this are SGI */

/*
   CDSIO board commands provided by the standard firmware
*/
#define	SET_SCC_REGISTER		0x00
#define	SET_BAUD_RATE_REGISTERS		0x01
#define	SET_XON_XOFF_MASK_ENABLE	0x02
#define	SET_XON_XOFF_CHAR		0x03
#define	SEND_BLOCK0			0x04
#define	SEND_BLOCK1			0x05
#define	SEND_IMMEDIATE_CHAR		0x06
#define	SEND_BREAK			0x07
#define	SET_INPUT_DELAY_TIME		0x08
#define	SET_INPUT_HIGH_WATER_MARK	0x09
#define	SET_IMMEDIATE_INPUT_INTR	0x0a
#define	SET_INTR_ON_STATUS_BITS_0_MASK	0x0b
#define	SET_INTR_ON_STATUS_BITS_1_MASK	0x0c
#define	SET_INTR_VECTOR_INPUTS		0x0d
#define	SET_INTR_VECTOR_OUTPUTS		0x0e
#define	SET_INTR_VECTOR_STATUS		0x0f
#define	SET_INTR_VECTOR_PARITY_ERR	0x10

/*
   CDSIO board address shared by all ports
*/
#define	STATUS_PORT			 0xfffd
#define	FIRMWARE_REVISION_NUMBER	 0x1ffe
#define	COMMAND_PORT			 0xffff

/*
   errors returned by the CDSIO board during input
*/
#define	FRAMING_ERR			0x40
#define	OVERRUN_ERR			0x20
#define	PARITY_ERR			0x10

/*
   interrupts returned by the CDSIO board
*/
#define	OUTPUT_DONE_INTR		0x01
#define	INPUT_INTR			0x02
#define	STATUS_CHANGE_INTR		0x04
#define	PARITY_ERR_INTR			0x08

/*
   control/status bits in the CDSIO board host status ports
*/
#define	INTR_PENDING			0x08
#define	VME_SUPERVISORY_ACCESS		0x02
#define	CDSIO_READY			0x01

/*
   CDSIO board command status
*/
#define	INVALID_COMMAND			0x01

/*
   CDSIO board port i/o block
*/

#define OUTBUF_LEN 3056
#define INBUF_LEN 1024        

/*
    Old structure - the new one matches layout, if not names, of the
    structure in irix/kern/io/cdsio.c

struct CDSIO_port {
   volatile unsigned char		output_buf0[OUTBUF_LEN];
   volatile unsigned char		output_buf1[OUTBUF_LEN];
   volatile unsigned char		input_buf[INBUF_LEN];
   volatile unsigned char		err_buf[INBUF_LEN];
   volatile unsigned short		command_code;
   volatile unsigned short		command_data;
   volatile unsigned short		command_status;
   volatile unsigned short		SCC_status;
   volatile unsigned short		filling_ptr;
   volatile unsigned short		emptying_ptr;
   volatile unsigned short		ovfl_count;
   volatile unsigned short		intr_source;
   volatile unsigned short		output_busy;
   volatile unsigned short		output_stopped;
   volatile unsigned short		ram_parity_err;
   volatile unsigned short		port_intr_status;
   volatile unchar pad8[6]
};

*/

/*
 * Layout of the dual port ram
 */
struct	CDSIO_port{
	union {
	    struct {			/* old firmware */
		unchar  output_buf0[OUTBUF_LEN]; /* 0000-0BEF: out buffer 0 */
		unchar  output_buf1[OUTBUF_LEN]; /* 0BF0-17DF: out buffer 1 */
		unchar  input_buf[INBUF_LEN];    /* 17E0-1BDF: in buffer */
		unchar  err_buf[INBUF_LEN];   /* 1BE0-1FDF: error buffer */
	    } cd;
	    struct {			/* new firmware */
		ushort  input_buf[INBUF_LEN];    /* 0000-07ff: in buffer */
		unchar  output_buf0[OUTBUF_LEN]; /* 0800-13df: out buffer 0 */
		unchar  output_buf1[OUTBUF_LEN]; /* 14f0-1fdf: out buffer 1 */
	    } sg;
	} dat;
	unchar	pad0;			/* 1FE0: not used */
	unchar	command_code;		/* 1FE1: command code */
	ushort	command_data;		/* 1FE2: command data word */
	unchar	command_intr;		/* 1FE4: !=0 for interrupt at end */
	unchar	command_status;		/* 1FE5: command status */
	unchar	pad2;			/* 1FE6: */
	unchar	SCC_status;		/* 1FE7: status from uart */
	ushort	filling_ptr;		/* 1FE8-1FE9: input filling pointer */
	ushort	emptying_ptr;		/* 1FEA-1FEB: input emptying ptr */
	ushort	ovfl_count;		/* 1FEC-1FED: input buf overflows */
	unchar	pad3;			/* 1FEE: */
	unchar	intr_source;		/* 1FEF: interrupt source flag */
	unchar	pad4;			/* 1FF0: */
	unchar	output_busy;		/* 1FF1: output busy flag */
	unchar	pad5;			/* 1FF2: */
	unchar	output_stopped;		/* 1FF3: output stopped */
	unchar	pad6;			/* 1FF4: */
	unchar	ram_parity_err;		/* 1FF5: parity error */
	unchar	pad7;			/* 1FF6: */
	unchar	port_intr_status;	/* 1FF7: port interrupt status flag */
	unchar	pad8[6];		/* 1FF8-1FFD: */
	ushort	firmwareRev;		/* 1FFE-1FFF: firmware revision */
};

