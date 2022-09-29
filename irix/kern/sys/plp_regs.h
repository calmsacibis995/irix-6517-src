/*
 * IP6 Audio and Centronics port registers
 *
 * $Revision: 1.5 $
 *
 */

#define PRDMALO		0x1f9d0006	/* RW DMA Low Address (16 bit)        */
#define PRDMAHI		0x1f920402	/* RW DMA High Address page map start */
#define PRDMADR 	0x1f9f000c	/* RW DMA "Direct mode" data (32 bit) */
#define PRDMACT 	0x1f9c0002	/* RW DMA byte count (16 bit)         */
#define PRDMACN		0x1f9f000a	/* RW DMA control (16 bit)            */
#define PRDMAST		0x1f9c0205	/* R  DMA status  (8 bit)             */
#define PRST		0x1f9f0004	/* R  Begin channel reset             */
#define PRDY		0x1f9f0000	/* R  End channel reset               */
#define PRDMASTART	0x1f9e000c	/* W  start dma                       */
#define PRDMASTOP	0x1f9e0004	/* W  stop dma                        */
#define PRSWACK		0x1f9e0008	/* W  generate "soft" acknowledge     */
#define PRPCHRLD	0x1f9d0000	/* W  reload channel register         */
#define PRMAPINDEX	0x1f9e0003	/* W  printer map index register      */
#define PRBSTAT		0x1f970001	/* R  printer byte status (4 bit)     */
#define ADREG		0x1f9c0305	/* RW A/D I/O register (8 bit)        */
#define AIGNDAC		0x1f9c0105	/* W  Audio input gain control        */
#define AOGNDAC		0x1f9c0005	/* W  Audio output gain control       */

#define ADDR_TO_DMALO(x) (x & 0xfff)	/* low twelve bits of address         */
#define ADDR_TO_DMAHI(x) btoct(x)	/* page number of address             */

/* PRDMACN bits */
#define CNTL_SEL1	(1 << (7+8))	/* audio sample rate select bit 1 */
#define CNTL_SEL0	(1 << (6+8))	/* audio sample rate select bit 0 */
#define CNTL_DMABUSY	(1 << (5+8))	/* dma in progress */
#define CNTL_LOOPEN	(1 << (4+8))	/* loop mode (for audio) */
#define CNTL_AUDEN	(1 << (3+8))	/* audio enable */
#define CNTL_PINTEN	(1 << (2+8))	/* interupt enable */
#define CNTL_PINT	(1 << (1+8))	/* pending interupt */
#define CNTL_LPBUSY	(1 << (0+8))	/* lp busy (for polling) */


/* PRDMAST bits are defined in plp.h */

#define PRMAXDMA	0xffff		/* limited by 16-bit dma counter */
#define PRMAXPAGES	btoct(PRMAXDMA)	/* less than 256 page map maximum*/
#define PLPWATCHTIME	(HZ/2)		/* how often to check for status */

/* how long to assert reset to the printer */
#define RESET_DELAY	400 /* micro seconds */

#define PLPSPRI		PUSER
#define AUDSPRI		PUSER

#define USEPRI		register int s
#define RAISE		s = spl5()
#define LOWER		splx(s)
