/*
 * tty.h
 *	Prototypes and declarations for MMSC serial port functions
 */

#ifndef _TTY_H_
#define _TTY_H_


/* TTY descriptors */
#define FFSC_NUM_TTYS	6	/* 6 serial ports on the FFSC */

typedef struct ttyinfo {
	int32_t	BaudRate;	/* Baud rate of TTY */
	int16_t CtrlFlags;	/* FFSC-specific flags */
	int16_t Port;		/* Assigned port */
	int32_t RxBufSize;	/* Receive buffer size */
	int32_t TxBufSize;	/* Transmit buffer size */
} ttyinfo_t;

#define TTYCF_CFVALID	0x8000	/* Control flags are valid */
#define TTYCF_PORTVALID 0x4000	/* Port field is valid */
#define TTYCF_NOFFSC		0x2000	/* FFSC command processing disabled */
#define TTYCF_OOBOK		0x1000	/* OOB message processing enabled */
#define TTYCF_HWFLOW		0x0800	/* Hardware Flow control enabled */

#define TTY_DFLT_RX_BUF_SIZE 4096	/* Default receive buffer size */
#define TTY_DFLT_TX_BUF_SIZE 4096	/* Default transmit buffer size */


/* Global TTY file descriptors */
extern int DISPLAYfd;			/* VGA display screen */
extern int TTYfds[FFSC_NUM_TTYS];	/* Serial ports */


/* Return codes from various com update functions */
#define TTYR_ERROR	-1	/* Something went wrong */
#define TTYR_OK		0	/* Success, change effective immediately */
#define TTYR_RESET	1	/* Success, change effective after reset */


/* Function prototypes */
const ttyinfo_t *comGetInfo(int);
int comGetRxBuf(int);
int comGetTxBuf(int);
int comResetFlag(int, int16_t);
int comSetFlag(int, int16_t);
int comSetFunction(int, int16_t);
int comSetRxBuf(int, int);
int comSetSpeed(int, int);
int comSetTxBuf(int, int);
STATUS comSetFlowCntl(int, int);

const ttyinfo_t *ttyGetInfo(int, int *);
int    ttyGetRxBuf(int);
int    ttyGetTxBuf(int);
STATUS ttyInit(void);
STATUS ttyReInit(void);
STATUS ttySetSpeed(int, int);
STATUS ttyToggleHwflow(int); 

#endif  /* _TTY_H_ */
