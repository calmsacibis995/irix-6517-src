/*
 * IP6/IP12/IP20/IP22/IP32/EVEREST Parallel Port
 *
 * $Revision: 1.21 $
 *
 */
#define PLPIOC	('p' << 8)

#define PLPIOCSTATUS	(PLPIOC|1)	/* returns status byte defined below */
#define PLPIOCRESET	(PLPIOC|2)	/* performs printer reset */
#define PLPIOCSETMASK	(PLPIOC|3)	/* set fatal status mask */
#define	PLPIOCTYPE 	(PLPIOC|4)	/* set versatec/centronics */
#define	PLPIOCSTROBE 	(PLPIOC|5)	/* set strobe length/duty cycle */
#define	PLPIOCIGNACK 	(PLPIOC|6)	/* ignore ack input */
#define	PLPIOCWTO 	(PLPIOC|7)	/* set write timeout in secs */
#define	PLPIOCRTO 	(PLPIOC|8)	/* set read timeout in secs */
#define	PLPIOCREAD 	(PLPIOC|9)	/* enable reading from port */
#define PLPIOMODE       (PLPIOC|10)     /* enable ricoh mode in pi1 chip, IP22
					   only */
#define PLPIOCESTROBE	(PLPIOC|11)	/* set IO4 plp strobe times.  Changed
					   from (PLPIOC|10) due to IP22
					   conflict. */
#define PLPMKHWG	(PLPIOC|12)	/* make hwgraph alias */

#define PLPIOCDCR	(PLPIOC|14)	/* write control reg, IP32 only */
#define PLPIOCDSR	(PLPIOC|15)	/* read status reg, IP32 only */
#define PLPIOCDATA	(PLPIOC|16)	/* write data reg, IP32 only */
#define PLPIOCADDRWRITE	(PLPIOC|17)	/* write the argument to
					   the EPP address port.  IP32 only. */
#define PLPIOCADDRREAD	(PLPIOC|18)	/* read and return an int
					   from the EPP address
					   port.  IP32 only. */

/* directory where parallel port devices reside in hwgraph */
#define PLP_HWG_DIR             "parallel"

/* status bits */
#define PLPFAULT	1	/* printer fault */
#define PLPEOP		2	/* end-of-paper condition */ 
#define PLPEOI		4	/* end-of-ink (or ribbon out) */
#define PLPONLINE	8	/* printer is on-line */

/* printer types */
#define PLP_TYPE_VERS	1	/* inverts STB and ACK signals */
#define PLP_TYPE_CENT	2

/* strobe length
 *
 *	dc = total time for strobe in 50 ns ticks (dc < 127)
 *	fall = time until strobe falls in 50 ns ticks (time < 31)
 *	rise = time until strobe rises again in 50 ns ticks (rise < 31)
 *
 *	---------------|             |---------------
 *		       |             |
 *		       |-------------|
 *	<---  fall --->
 *			<--  rise --->
 *	<------------------  dc -------------------->
 */
#define PLP_STROBE(dc,fall,rise)	\
	(((dc&0x7f)<<24)|(((fall)&0x1f)<<16)|(((rise)&0x1f)<<8))

/* NOTE ABOUT PLP_STROBE ON INDIGO:
 *
 *	dc = total time for strobe in 30ns ticks (dc < 127)
 *	fall = time until strobe falls in 30 ns ticks (time < dc)
 *	rise = time until strobe rises again in 30 ns ticks (rise < dc)
 *
 *	---------------|             |---------------
 *		       |             |
 *		       |-------------|
 *	<---  fall --->
 *			<---  rise --->
 *	<------------------  dc -------------------->
 *
 * The following definition should be used for Indigo:
 *
 * #define PLP_STROBE(dc,fall,rise)	\
 *	(((dc&0x7f)<<24)|(((dc-fall)&0x7f)<<16)|(((dc-fall-rise)&0x7f)<<8))
 */

/* IO4 strobe length (diagram is for Centronics polarity)
 *
 * 	set-up = time until strobe falls in 1us ticks (time < 256)
 * 	pulse = length of strobe pulse in 1us ticks (time < 512)
 * 	hold = time until strobe can fall again in 1us ticks (time < 256)
 *
 *	---------------|             |-------------
 *		       |             |
 *                     |-------------|
 *      <--  setup --->              <--- hold --->
 *                     <--- pulse --->
 */

#define PLP_ESTROBE(_setup,_pulse,_hold)	\
	(((_setup & 0xff)<<24)|((_hold & 0xff)<<16)|((_pulse & 0x1ff)<<7))

/* parallel interface modes, IP32 only */
#define	PLP_PIO		(0x0)		/* Centronics PIO mode */
#define	PLP_CMPTBL	(0x4<<4)	/* Centronics FIFO mode */
#define	PLP_BI		(0x6<<4)	/* bidirectional mode */
#define	PLP_ECP		(0x6<<4)	/* ECP (Extended Capabilities mode) */
#define	PLP_EPP		(0x8<<4)	/* EPP (Enhanced Parallel Port mode) */
