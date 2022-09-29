/* m68901.h - Motorola MC68901 MFP (Multi-Function Peripheral) */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01m,30jan93,dzb  added sys and aux TIMER structures to TY_CO_DEV.
                 consolidated each timer's MFP_TCR macros into generic macros.
		 added MFP_TCR_{A,B,C,D}_MASK macros.  added comments.
01l,04jan93,caf  added timer driver support to TY_CO_DEV structure.
		 also added baudFreq, baudMin and baudMax to TY_CO_DEV.
01k,18dec92,caf  added TY_CO_DEV and function declarations for 5.1 upgrade.
		 for 5.0.x compatibility, define INCLUDE_TY_CO_DRV_50 when
		 including this header.
01j,22sep92,rrr  added support for c++
01i,26may92,rrr  the tree shuffle
01h,04oct91,rrr  passed through the ansification filter
		  -changed ASMLANGUAGE to _ASMLANGUAGE
		  -changed copyright notice
01g,05oct90,shl  added copyright notice.
                 made #endif ANSI style.
01f,28jul90,gae  fixed typo in TADR definition introduced in 01e.
01e,13jul90,gae  simplified MFP macros.
01d,15feb89,jcc  incorporated MFP_REG_ADDR_INTERVAL into
		 register address definitions.
01c,16jan89,jcf  added defines for rest of chip.
01b,04may88,gae  fixed name on banner.
01a,30apr88,gae	 extracted from mv133.h.
*/

#ifndef __INCm68901h
#define __INCm68901h

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The macro MFP_REG_ADDR_INTERVAL must be defined
 * when including this header.
 */


#ifdef	_ASMLANGUAGE

#define	CAST

#else	/* _ASMLANGUAGE */

#define	CAST	(char *)

#ifndef	INCLUDE_TY_CO_DRV_50

#include "tylib.h"

typedef struct		/* TIMER */
    {
    char *cr;		/* timer control register			*/
    char *dr;		/* timer data register				*/
    char *ier;		/* interrupt enable register			*/
    char *imr;		/* interrupt mask register			*/
    int cMask;		/* timer control register mask			*/
    int iMask;		/* interrupt register mask			*/
    int prescale;	/* clock timer resolution constant		*/
    } TIMER;

typedef struct		/* TY_CO_DEV */
    {
    TY_DEV tyDev;
    BOOL created;	/* true if this device has really been created  */
    uint32_t baudFreq;	/* reference clock, in Hz                       */
    uint16_t baudMin;	/* minimum baud rate                            */
    uint16_t baudMax;	/* maximum baud rate                            */
    char *ucr;		/* USART control register                       */
    char *rsr;		/* receiver status register                     */
    char *tsr;		/* transmit status register                     */
    char *udr;		/* USART data register                          */
    char *imra;
    char *imrb;
    char *iera;
    char *ierb;
    char *baud1;	/* baud rate 1 generator data register		*/
    char *baud2;	/* baud rate 2 generator data register		*/
    TIMER sys;		/* system clock timer				*/
    TIMER aux;		/* auxilliary clock timer			*/
    } TY_CO_DEV;

#endif	/* INCLUDE_TY_CO_DRV_50 */

#endif	/* _ASMLANGUAGE */

#define MFP_ADRS(base,reg)   (CAST (base+(reg*MFP_REG_ADDR_INTERVAL)))

/* register definitions */

#define MFP_GPIP(base)	MFP_ADRS(base,0x00)	/* Data I/O port           */
#define MFP_AER(base)	MFP_ADRS(base,0x01)	/* active edge register    */
#define MFP_DDR(base)	MFP_ADRS(base,0x02)	/* data direction reg.     */
#define MFP_IERA(base)	MFP_ADRS(base,0x03)	/* interrupt enable reg A  */
#define MFP_IERB(base)	MFP_ADRS(base,0x04)	/* interrupt enable reg B  */
#define MFP_IPRA(base)	MFP_ADRS(base,0x05)	/* int. pending reg A      */
#define MFP_IPRB(base)	MFP_ADRS(base,0x06)	/* int. pending reg B      */
#define MFP_ISRA(base)	MFP_ADRS(base,0x07)	/* int. in-service reg A   */
#define MFP_ISRB(base)	MFP_ADRS(base,0x08)	/* int. in-service reg B   */
#define MFP_IMRA(base)	MFP_ADRS(base,0x09)	/* interrupt mask reg A    */
#define MFP_IMRB(base)	MFP_ADRS(base,0x0a)	/* interrupt mask reg B    */
#define MFP_VR(base)	MFP_ADRS(base,0x0b)	/* vector reg              */
#define MFP_TACR(base)  MFP_ADRS(base,0x0c)	/* timer A control reg     */
#define MFP_TBCR(base)  MFP_ADRS(base,0x0d)	/* timer B control reg     */
#define MFP_TCDCR(base) MFP_ADRS(base,0x0e)	/* timers C&D control reg  */
#define MFP_TADR(base)  MFP_ADRS(base,0x0f)	/* timer A data reg        */
#define MFP_TBDR(base)  MFP_ADRS(base,0x10)	/* timer B data reg        */
#define MFP_TCDR(base)  MFP_ADRS(base,0x11)	/* timer C data reg        */
#define MFP_TDDR(base)  MFP_ADRS(base,0x12)	/* timer D data reg        */
#define MFP_SCR(base)	MFP_ADRS(base,0x13)	/* sync char reg           */
#define MFP_UCR(base)	MFP_ADRS(base,0x14)	/* USART control reg       */
#define MFP_RSR(base)	MFP_ADRS(base,0x15)	/* receiver status reg     */
#define MFP_TSR(base)	MFP_ADRS(base,0x16)	/* transmitter status reg  */
#define MFP_UDR(base)   MFP_ADRS(base,0x17)	/* USART data reg          */

/* vector number offsets */

#define MFP_INT_GP0		0x00		/* general purpose I0 */
#define MFP_INT_GP1		0x01		/* general purpose I1 */
#define MFP_INT_GP2		0x02		/* general purpose I2 */
#define MFP_INT_GP3		0x03		/* general purpose I3 */
#define MFP_INT_TIMER_D		0x04		/* timer D */
#define MFP_INT_TIMER_C		0x05		/* timer C */
#define MFP_INT_GP4		0x06		/* general purpose I4 */
#define MFP_INT_GP5		0x07		/* general purpose I5 */
#define MFP_INT_TIMER_B		0x08		/* timer B */
#define MFP_INT_TX_ERR		0x09		/* transmit error */
#define MFP_INT_TRANS		0x0a		/* transmit buffer empty */
#define MFP_INT_RX_ERR		0x0b		/* receive error */
#define MFP_INT_RECV		0x0c		/* receive buffer full */
#define MFP_INT_TIMER_A		0x0d		/* timer A */
#define MFP_INT_GP6		0x0e		/* general purpose I6 */
#define MFP_INT_GP7		0x0f		/* general purpose I7 */

/* fields of vector register (VR) */

#define MFP_VR_SOFT		0x08	/* software acknowledge interrupts */

/* fields of interrupt enable A (IERA), interrupt pending A (IPRA), */
/* interrupt in-service A (ISRA), interrupt mask A (IMRA) */

#define MFP_A_TIMER_B		0x01	/* timer B */
#define MFP_A_TX_ERR		0x02	/* transmitter error */
#define MFP_A_TX_EMPTY		0x04	/* transmitter buffer empty */
#define MFP_A_RX_ERR		0x08	/* receiver error */
#define MFP_A_RX_FULL		0x10	/* receiver buffer full */
#define MFP_A_TIMER_A		0x20	/* timer A */
#define MFP_A_GPIP6		0x40	/* general purpose i/o 6 */
#define MFP_A_GPIP7		0x80	/* general purpose i/o 7 */

/* fields of interrupt enable B (IERB), interrupt pending B (IPRB), */
/* interrupt in-service B (ISRB), interrupt mask B (IMRB) */

#define MFP_B_GPIP0		0x01	/* general purpose i/o 0 */
#define MFP_B_GPIP1		0x02	/* general purpose i/o 1 */
#define MFP_B_GPIP2		0x04	/* general purpose i/o 2 */
#define MFP_B_GPIP3		0x08	/* general purpose i/o 3 */
#define MFP_B_TIMER_D		0x10	/* timer D */
#define MFP_B_TIMER_C		0x20	/* timer C */
#define MFP_B_GPIP4		0x40	/* general purpose i/o 4 */
#define MFP_B_GPIP5		0x80	/* general purpose i/o 5 */

/* fields of usart control register (UCR) */

#define MFP_UCR_16X		0x80	/* divide clock souce by 16 */
#define MFP_UCR_8BIT		0x00	/* 8 bit word length */
#define MFP_UCR_7BIT		0x20	/* 7 bit word length */
#define MFP_UCR_6BIT		0x40	/* 6 bit word length */
#define MFP_UCR_5BIT		0x60	/* 5 bit word length */
#define MFP_UCR_SYNC		0x00	/* synchronous */
#define MFP_UCR_1STOP		0x08	/* one stop bit */
#define MFP_UCR_1_AND_HALF_STOP	0x10	/* one and a half stop bits */
#define MFP_UCR_2STOP		0x18	/* two stop bits */
#define MFP_UCR_NO_PARITY	0x00	/* disable parity */
#define MFP_UCR_EVEN_PARITY	0x06	/* enable even parity */
#define MFP_UCR_ODD_PARITY	0x04	/* enable odd parity */

/* fields of receiver status register (RSR) */

#define MFP_RSR_RX_ENABLE		0x01	/* enable receiver */
#define MFP_RSR_SYNC_STRIP_ENABLE	0x02	/* strip sync characters */
#define MFP_RSR_CHAR_IN_PROGRESS	0x04	/* receiving a character */
#define MFP_RSR_BREAK_DETECT		0x08	/* break detect */
#define MFP_RSR_FRAME_ERROR		0x10	/* framing error */
#define MFP_RSR_PARITY_ERROR		0x20	/* parity error */
#define MFP_RSR_OVERRUN_ERROR		0x40	/* overrun error */
#define MFP_RSR_BUFFER_FULL		0x80	/* receive buffer full */

/* fields of transmitter status register (TSR) */

#define MFP_TSR_TX_ENABLE		0x01	/* enable receiver */
#define MFP_TSR_HI_Z			0x00	/* hi - Z */
#define MFP_TSR_LOW			0x02	/* LOW */
#define MFP_TSR_HIGH			0x04	/* HIGH */
#define MFP_TSR_LOOP			0x06	/* LOOP */
#define MFP_TSR_BREAK			0x08	/* transmit break */
#define MFP_TSR_EOT			0x10	/* end of transmission */
#define MFP_TSR_AUTO_TURNAROUND		0x20	/* parity error */
#define MFP_TSR_UNDERRUN_ERROR		0x40	/* overrun error */
#define MFP_TSR_BUFFER_EMPTY		0x80	/* transmit buffer empty */

/* fields of timer control registers */

#define MFP_TCR_A_MASK		0xf	/* timer A mask                       */
#define MFP_TCR_B_MASK		0xf	/* timer B mask                       */
#define MFP_TCR_C_MASK		0x70	/* timer C mask                       */
#define MFP_TCR_D_MASK		0x7	/* timer D mask                       */
#define MFP_TCR_RESET		0x10	/* reset timer A&B		      */
#define MFP_TCR_STOP		0x00	/* stop timer 			      */
#define MFP_TCR_DELAY_4		0x11	/* delay mode, divide by 4 prescale   */
#define MFP_TCR_DELAY_10	0x22	/* delay mode, divide by 10 prescale  */
#define MFP_TCR_DELAY_16	0x33	/* delay mode, divide by 16 prescale  */
#define MFP_TCR_DELAY_50	0x44	/* delay mode, divide by 50 prescale  */
#define MFP_TCR_DELAY_64	0x55	/* delay mode, divide by 64 prescale  */
#define MFP_TCR_DELAY_100	0x66	/* delay mode, divide by 100 prescale */
#define MFP_TCR_DELAY_200	0x77	/* delay mode, divide by 200 prescale */
#define MFP_TCR_EVENT		0x8	/* event count mode		      */
#define MFP_TCR_PULSE_4		0x9	/* pulse mode, divide by 4 prescale   */
#define MFP_TCR_PULSE_10	0xa	/* pulse mode, divide by 10 prescale  */
#define MFP_TCR_PULSE_16	0xb	/* pulse mode, divide by 16 prescale  */
#define MFP_TCR_PULSE_50	0xc	/* pulse mode, divide by 50 prescale  */
#define MFP_TCR_PULSE_64	0xd	/* pulse mode, divide by 64 prescale  */
#define MFP_TCR_PULSE_100	0xe	/* pulse mode, divide by 100 prescale */
#define MFP_TCR_PULSE_200	0xf	/* pulse mode, divide by 200 prescale */

/* function declarations */

#ifndef	INCLUDE_TY_CO_DRV_50
#ifndef	_ASMLANGUAGE
#if defined(__STDC__) || defined(__cplusplus)

IMPORT	void	tyCoRxInt (void);
IMPORT	void	tyCoTxInt (void);

#else	/* __STDC__ */

IMPORT	void	tyCoRxInt ();
IMPORT	void	tyCoTxInt ();

#endif	/* __STDC__ */
#endif	/* _ASMLANGUAGE */
#endif	/* INCLUDE_TY_CO_DRV_50 */

#ifdef __cplusplus
}
#endif

#endif /* __INCm68901h */
