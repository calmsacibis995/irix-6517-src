/*
 * xfer.h
 *	Declarations & prototypes for the various file transfer functions
 */

#ifndef _XFER_H_
#define _XFER_H_


/* XModem stuff */
#define PACKET_LEN	128
#define PACKET_LEN_1K	1024		/* XMODEM-1K packet length */

typedef struct xmodem_crc_packet {
	unsigned char Header;		/* Header byte */
	unsigned char PacketNum;	/* Packet number */
	unsigned char PacketNumOC;	/* One's complement of packet number */
	unsigned char Packet[PACKET_LEN];	/* The data itself */
	unsigned char CRCHi;		/* High order byte of CRC */
	unsigned char CRCLo;		/* Low order byte of CRC */
} xmodem_crc_packet_t;

typedef struct xmodem_1k_crc_packet {
	unsigned char Header;		/* Header byte */
	unsigned char PacketNum;	/* Packet number */
	unsigned char PacketNumOC;	/* One's complement of packet number */
	unsigned char Packet[PACKET_LEN_1K];	/* The data itself */
	unsigned char CRCHi;		/* High order byte of CRC */
	unsigned char CRCLo;		/* Low order byte of CRC */
} xmodem_1k_crc_packet_t;

#define SOH	'\001'		/* Start Of Header */
#define SOH_CRC	'C'		/* Start Of Header for XMODEM-CRC */
#define STX	'\002'		/* Start of Text (XMODEM-1K packet) */
#define EOT	'\004'		/* End Of Transmission */
#define EOT_STR "\004"
#define ACK	'\006'		/* Positive Acknowledgement */
#define NAK	'\025'		/* Negative Acknowledgement */
#define CAN	'\030'		/* Cancel */
#define CAN_STR	"\030"
#define PAD	'\032'		/* ^Z for padding */


/*   CRC-16 constants.  From Usenet contribution by Mark G. Mendel,	*/
/*   Network Systems Corp.  (ihnp4!umn-cs!hyper!mark)			*/
#define	P	0x1021		/* the CRC polynomial. */
#define W	16		/* number of bits in CRC */
#define B	8		/* the number of bits per char */

extern unsigned short CRCTab[1<<B];	/* CRC values */


/* Function prototypes for CDI code */
void disable_downloader(void);
void enable_downloader(void);


/* Function prototypes for SGI code */
struct flashStruct;
int getflash(struct flashStruct *, int, int);

#endif  /* _XFER_H_ */
