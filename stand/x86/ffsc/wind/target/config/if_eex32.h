/* if_eex32.h - header file for interl EtherExpress Flash 32 */

/* 
modification history
--------------------
01b,28jun94,vin  included in the BSP.
01a,12may94,bcs  written
*/

#ifndef	INCif_eex32h
#define	INCif_eex32h


/* I/O offsets (from EISA slot base address) */

#define	CA		0x0000		/* 82596 Channel Attention */
#define PORT		0x0008		/* 82596 PORT (DWORD) */
#define	EISA_ID0	0x0c80		/* EISA Product Identifier byte 0 */
#define	EISA_ID1	0x0c81		/* EISA Product Identifier byte 1 */
#define	EISA_ID2	0x0c82		/* EISA Product Identifier byte 2 */
#define	EISA_ID3	0x0c83		/* EISA Product Identifier byte 3 */
#define	EBC		0x0c84		/* Expansion Board Control */
#define	PLX_CONF0	0x0c88		/* PLX IRQ Control & Preempt Time */
#define	PLX_CONF1	0x0c89		/* PLX USER Pins (media select) */
#define	PLX_CONF2	0x0c8a		/* PLX EEPROM Control (NOT USED) */
#define	PLX_CONF3	0x0c8f		/* PLX SW Reset, Burst Enable */
#define	NET_IA0		0x0c90		/* Ethernet Individual Address */

#define	FLCTL0		0x0400		/* Flash Base Address, FWE0- */
#define	FLCTL1		0x0410		/* Flash Base Control */
#define	FLADDR		0x0420		/* Flash High Address Lines */
#define	IRQCTL		0x0430		/* Interrupt Control */

/* Important values */

#define	EEX32_EISA_ID0	0x25
#define	EEX32_EISA_ID1	0xd4
#define	EEX32_EISA_ID2	0x10
#define	EEX32_EISA_ID3	0x10


/* Interrupt control bits (FLEA ASIC) */

#define	IRQ_EDGE	0x01		/* Interrupt is edge-triggered (RO) */
#define	IRQ_SEL0	0x02		/* IRQ selection bit */
#define	IRQ_SEL1	0x04		/* IRQ selection bit */
#define	IRQ_LATCH	0x08		/* Set to enable latched mode */
#define	IRQ_INTSTAT	0x10		/* Set when interrupt active; write 1 */
					/* to clear interrupt after handling */
#define	IRQ_FORCE_LOW	0x20		/* Set to force IRQ low (disabled) */
#define	IRQ_SW_INT	0x40		/* Set to cause interrupt for testing */
#define	IRQ_EXTEND	0x80		/* Set to enable FLEA IRQs */


#endif	/* INCif_eex32h */
