#ifndef SYS_Z8530_H
#define SYS_Z8530_H

#if defined(_KERNEL) || defined(_STANDALONE)

/* Copyright 1986,1993, Silicon Graphics Inc., Mountain View, CA. */

/*
 * Defs for he Zilog Z85130 serial communication controller.
 *
 * $Revision: 1.55 $
 */

/* 
	Z8530 write register offset
*/
#define	WR0	0	/* command */
#define	WR1	1	/* Tx/Rx interrupt and data transfer mode */
#define	WR2	2	/* interrup vector */
#define	WR3	3	/* Rx parameters and control */
#define	WR4	4	/* Tx/Rx miscellaneous parameters and modes */
#define	WR5	5	/* Tx parameters and control */
#define	WR6	6	/* sync character or SDLC addr field */
#define	WR7	7	/* sync character or SDLC flag */
#define	WR8	8	/* Tx buffer */
#define	WR9	9	/* master interrupt control */
#define	WR10	10	/* miscellaneous Tx/Rx control */
#define	WR11	11	/* clock mode control */
#define	WR12	12	/* lower byte of baud rate generator time constant */
#define	WR13	13	/* upper byte of baud rate generator time constant */
#define	WR14	14	/* miscellaneous control bits */
#define	WR15	15	/* external status/interrupt control */

/*
	Z8530 read register offset
*/
#define	RR0	0	/* Tx/Rx buffer/external status */
#define	RR1	1	/* Rx condition status/residue codes */
#define	RR2	2	/* interrup vector (modified in channel B only) */
#define	RR3	3	/* interrupt pending (channel A only) */
#define	RR8	8	/* Rx buffer */
#define	RR10	10	/* loop/clock status */
#define	RR12	12	/* lower byte of baud rate generator time constant */
#define	RR13	13	/* upper byte of baud rate generator time constant */
#define	RR14	14	/* extended function register, WR7' */
#define	RR15	15	/* external status interrupt enable */

/*
	write register bits/commands definition
*/

/* command register */
#define	WR0_RST_TX_URUN		0xc0	/* reset Tx underrun/EOM latch */
#define	WR0_RST_TX_CRC		0x80	/* reset Tx CRC generator */ 
#define	WR0_RST_RX_CRC		0x40	/* reset Rx CRC checker */
#define	WR0_RST_IUS		0x38	/* reset highest IUS */
#define	WR0_RST_ERR		0x30	/* reset error (bits in RR1) */
#define	WR0_RST_TX_INT		0x28	/* reset Tx interrupt pending */
#define	WR0_INT_NXT_RX_CHR	0x20	/* interrupt on next Rx character */
#define	WR0_SDLC_ABRT		0x18	/* send SDLC abort */
#define	WR0_RST_EXT_INT		0x10	/* reset external/status interrupt */

/* Tx/Rx interrupt and data transfer mode register */
#define	WR1_WAIT_REQ_ENBL	0x80	/* Wait/DMA Request enable */
#define	WR1_WAIT_FUNC		0x40	/* use as WAIT */
#define	WR1_WAIT_REQ_RX		0x20	/* Wait/DMA Request Rx mode */
#define	WR1_RX_INT_MSK		0x18	/* Rx interrupt mask */
#define	WR1_RX_INT_ERR		0x18	/* Rx interrupt on error (special) */
#define	WR1_RX_INT_ERR_ALL_CHR	0x10	/* Rx interrupt on error/all chars */
#define	WR1_RX_INT_ERR_1ST_CHR	0x08	/* Rx interrupt on error/1st char */
#define	WR1_PARITY_ERR		0x04	/* consider parity as error (special) */
#define	WR1_TX_INT_ENBL		0x02	/* Tx interrupt enable */
#define	WR1_EXT_INT_ENBL	0x01	/* external master interrupt enable */

/* Rx parameters and control register */
#define	WR3_RX_BITS_MSK		0xc0	/* Rx bits mask */
#define	WR3_RX_8BIT		0xc0	/* 8 bits per Rx character */
#define	WR3_RX_6BIT		0x80	/* 6 bits per Rx character */
#define	WR3_RX_7BIT		0x40	/* 7 bits per Rx character */
#define	WR3_AUTO_ENBL		0x20	/* auto enable Rx/Tx */
#define	WR3_ENT_HUNT		0x10	/* enter hunt mode, synchronization */
#define	WR3_RX_CRC_ENBL		0x08	/* enable CRC checker on next char */
#define	WR3_SDLC_ADDR_SRCH	0x04	/* SDLC address search mode */
#define	WR3_NO_LOAD_SYNC_CHR	0x02	/* sync character load inhibit */
#define	WR3_RX_ENBL		0x01	/* Rx enable */

/* Tx/Rx miscellaneous parameters and modes */
#define	WR4_CLK_MSK		0xc0	/* clock factor mask */
#define	WR4_X64_CLK		0xc0	/* clock rate = 64 x data rate */
#define	WR4_X32_CLK		0x80	/* clock rate = 32 x data rate */
#define	WR4_X16_CLK		0x40	/* clock rate = 16 x data rate */
#define	WR4_SYNC_MSK		0x30	/* sync mode mask */
#define	WR4_EXT_SYNC		0x30	/* external sync mode */
#define	WR4_SDLC		0x20	/* SDLC mode */
#define	WR4_SYNC16		0x10	/* bisync mode */
#define	WR4_STOP_MSK		0x0c	/* stop bit mask */
#define	WR4_2_STOP		0x0c	/* 2 stop bits in async mode */
#define	WR4_3_HALF_STOP		0x08	/* 3/2 stop bits in async mode */
#define	WR4_1_STOP		0x04	/* 1 stop bit in async mode */
#define	WR4_EVEN_PARITY		0x02	/* even parity */
#define	WR4_PARITY_ENBL		0x01	/* enable parity */

/* Tx parameters and control register */
#define	WR5_DTR			0x80	/* data terminal ready */
#define	WR5_TX_BITS_MSK		0x60	/* Tx bits mask */
#define	WR5_TX_8BIT		0x60	/* 8 bits per Tx character */
#define	WR5_TX_6BIT		0x40	/* 6 bits per Tx character */
#define	WR5_TX_7BIT		0x20	/* 7 bits per Tx character */
#define	WR5_SEND_BRK		0x10	/* transmit break code */
#define	WR5_TX_ENBL		0x08	/* Tx enable */
#define	WR5_CRC16_PLY		0x04	/* use CRC-16 polynomial, not SDLC's */
#define	WR5_RTS			0x02	/* request to send */
#define	WR5_TX_CRC_ENBL		0x01	/* enable CRC generator on next char */

/* Z85130 new feature */
#define	WR7_TX_FIFO_INT		0x20	/* Tx interrupt only when FIFO is
					   completely empty */
#define	WR7_EXTENDED_READ	0x40	/* enable extended read */
#define WR7_RX_HALF_FIFO	0x08	/* intr every 4th RX char */

/* master interrupt control register */
#define	WR9_HW_RST		0xc0	/* hardware reset */
#define	WR9_RST_CHNA		0x80	/* reset channel A */
#define	WR9_RST_CHNB		0x40	/* reset channel B */
#define	WR9_STAT_HGH		0x10	/* modify V[4-6] bits in vector */
#define	WR9_MIE			0x08	/* master interrupt enable */
#define	WR9_DLS			0x04	/* disable lower chain */
#define	WR9_NV			0x02	/* no vector */
#define	WR9_VIS			0x01	/* vector includes status */

/* misc. Tx/Rx control register */
#define	WR10_CRC_PRESET_1	0x80	/* preset CRC to 1's */
#define	WR10_ENCODING_MSK	0x60	/* data encoding mask */
#define	WR10_FM0		0x60	/* FM0 encoding */
#define	WR10_FM1		0x40	/* FM1 encoding */
#define	WR10_NRZI		0x20	/* NRZI encoding */
#define	WR10_ACTIVE_ON_POLL	0x10	/* go active on poll, SDLC loop mode */
#define	WR10_MRK_IDLE		0x08	/* idle line condition, SDLC */
#define	WR10_URUN_ABRT		0x04	/* abort on transmit underrun, SDLC */
#define	WR10_SDLC_LOOP		0x02	/* enable SDLC loop mode */
#define	WR10_6BIT_SYNC		0x01	/* 6 bits sync mode */

/* clock mode control register */
#define	WR11_RTXC_XTAL		0x80	/* use quartz crystal */
#define	WR11_RCLK_MSK		0x60	/* Rx clock mask */
#define	WR11_RCLK_DPLL		0x60	/* DPLL output to Rx clock */
#define	WR11_RCLK_BRG		0x40	/* baud rate generator to Rx clock */
#define	WR11_RCLK_TRXC		0x20	/* input from TRxC pin to Rx clock */
#define	WR11_TCLK_MSK		0x18	/* Tx clock mask */
#define	WR11_TCLK_DPLL		0x18	/* DPLL output to Tx clock */
#define	WR11_TCLK_BRG		0x10	/* baud rate generator to Tx clock */
#define	WR11_TCLK_TRXC		0x08	/* input from TRxC pin to Tx clock */
#define	WR11_TRXC_OUTPUT	0x04	/* TRxC function as output pin */
#define	WR11_TRXC_OUTPUT_MSK	0x03	/* TRxC output source mask */
#define	WR11_TRXC_DPLL		0x03	/* DPLL output to TRxC pin */
#define	WR11_TRXC_BRG		0x02	/* baud rate generator to TRxC pin */
#define	WR11_TRXC_TCLK		0x01	/* Tx clock to TRxC pin */

/* miscellaneous control bits register */
#define	WR14_DPLL_NRZI		0xe0	/* DPLL operates in NRZI mode */
#define	WR14_DPLL_FM		0xc0	/* DPLL operates in FM mode */
#define	WR14_DPLL_RTXC		0xa0	/* DPLL clock input from RTxC */
#define	WR14_DPLL_BRG		0x80	/* DPLL clock input from br generator */
#define	WR14_DPLL_DSBL		0x60	/* disable DPLL */
#define	WR14_DPLL_RST_MCLK	0x40	/* disable DPLL */
#define	WR14_DPLL_ENT_SRCH	0x20	/* DPLL enter search mode */
#define	WR14_LCL_LPBK		0x10	/* local loopback, diagnostics */
#define	WR14_ECHO		0x08	/* auto echo enable */
#define	WR14_DTR_REQ		0x04	/* enable DTR as Tx request pin */
#define	WR14_BRG_PCLK		0x02	/* PCLK as baud rate generator source */
#define	WR14_BRG_ENBL		0x01	/* enable baud rate generator */

/* external status/interrupt control register */
#define	WR15_BRK_IE		0x80	/* interrupt on break */
#define	WR15_ABRT_IE		WR15_BRK_IE	/* interrupt on SDLC abort */
#define	WR15_TX_URUN_IE		0x40	/* interrupt on Tx underrun/EOM */
#define	WR15_CTS_IE		0x20	/* interrupt on CTS */
#define	WR15_SYNC_HUNT_IE	0x10	/* interrupt on sync/hunt */
#define	WR15_DCD_IE		0x08	/* interrupt on DCD */
#define	WR15_BRG_0_CNT_IE	0x02	/* interrupt on baud rate zero count */
#define	WR15_Z85130_EXTENDED	0x01	/* enable Z85130 new functions */

/*
	read register bits definition
*/

/* Tx/Rx buffer status/external status register */
#define	RR0_BRK			0x80	/* break detected in asychronous mode */
#define	RR0_ABRT		RR0_BRK	/* abort detected in SDLC mode */
#define	RR0_TX_URUN		0x40	/* Tx underrun/EOM */
#define	RR0_CTS			0x20	/* CTS */
#define	RR0_SYNC		0x10	/* SYNC pin */
#define	RR0_DCD			0x08	/* DCD */
#define	RR0_TX_EMPTY		0x04	/* Tx buffer empty */
#define	RR0_BRG_0_CNT		0x02	/* BRG zero count */
#define	RR0_RX_CHR		0x01	/* Rx character available */

/* Rx condition status/residue codes register */ 
#define	RR1_SDLC_FRAME_END	0x80	/* SDLC end of frame */
#define	RR1_CRC_ERR		0x40	/* CRC error */
#define	RR1_FRAMING_ERR		RR1_CRC_ERR	/* framing error */
#define	RR1_RX_ORUN_ERR		0x20	/* Rx overrun */
#define	RR1_PARITY_ERR		0x10	/* parity error */
#define	RR1_RESIDUE_CODE	0x0E	/* reside code */
#define	RR1_ALL_SENT		0x01	/* all characters are sent in Tx */

/* interrupt pending register */
#define	RR3_CHNA_RX_IP		0x20	/* channel A Rx */
#define	RR3_CHNA_TX_IP		0x10	/* channel A Tx */
#define	RR3_CHNA_EXT_IP		0x08	/* channel A external */
#define	RR3_CHNB_RX_IP		0x04	/* channel B Rx */
#define	RR3_CHNB_TX_IP		0x02	/* channel B Tx */
#define	RR3_CHNB_EXT_IP		0x01	/* channel B external */

/* loop/clock status register */
#define	RR10_1CLK_MISSING	0x80	/* one clock missing */
#define	RR10_2CLK_MISSING	0x40	/* two clock missing */
#define	RR10_SDLC_LP_SENDING	0x10	/* Tx sending in SDLC mode */
#define	RR10_SDLC_ON_LP		0x02	/* SCC on loop */

/* external status/interrupt enable register */
#define	RR13_BRK_IE		0x80	/* break enable */
#define	RR13_ABRT_IE		RR13_BRK_IE	/* abort enable */
#define	RR13_TX_URUN_IE		0x40	/* Tx underrun enable */
#define	RR13_CTS_IE		0x20	/* CTS enable */
#define	RR13_SYNC_IE		0x10	/* sync enable */
#define	RR13_DCD_IE		0x08	/* DCD enable */
#define	RR13_BRG_0_CNT_IE	0x02	/* BRG zero count enable */

#define	CLK_SPEED	3672000
#define	CLK_FACTOR	16

#if IP20 || IP22 || IP26 || IP28 || _SYSTEM_SIMULATION
#define Z8530_DELAY             DELAY(2)
#if IP20 || IP28
#define Z8530_FLUSHBUS		flushbus()
#define Z8530_FBDELAY		Z8530_FLUSHBUS, Z8530_DELAY
#endif	/* IP20 */
#if IP22 || IP26
#define Z8530_FLUSHBUS		*(volatile int *)PHYS_TO_K1(HPC3_INTSTAT_ADDR)
#define Z8530_FBDELAY		Z8530_FLUSHBUS, DELAY(1)
#endif	/* IP22 || IP26 || IP28 */

#if _SYSTEM_SIMULATION
#undef RD_DATA
#undef WR_DATA

#define RD_RR0(p)              	(RR0_CTS|RR0_TX_EMPTY)
#define WR_WR0(p, v)            { }
#else
#ifndef _STANDALONE
#define RD_DATA(p)              rd_uart_reg0_data(p)
#define RD_RR0(p)              	rd_uart_reg0_data(p)
#define WR_DATA(p, v)           {					\
					int s = spl7();			\
					Z8530_DELAY;			\
					*(p) = (v);			\
					Z8530_FLUSHBUS;			\
					splx(s);			\
				}
#define WR_WR0(p, v)            WR_DATA(p, v)
#else
#define	RD_DATA(p)		( Z8530_DELAY, *(p) )
#define	RD_RR0(p)		( Z8530_DELAY, *(p) )
#define	WR_DATA(p, v)		{ Z8530_DELAY; *(p) = (v); Z8530_FLUSHBUS; }
#define	WR_WR0(p, v)		{ Z8530_DELAY; *(p) = (v); Z8530_FLUSHBUS; }
#endif	/* _STANDALONE */
#endif

#ifndef _STANDALONE

u_char	du_cntrl;


#define INITLOCK_PORT(dp)	du_initport(dp)
#define LOCK_PORT(dp,s)		{ s = du_lock(dp); }
#define UNLOCK_PORT(dp,s)	{ du_unlock(dp, s); }

#if _SYSTEM_SIMULATION

#define MHSIM_DUART_WRITE(addr, value) \
{                                        \
   int s;                                \
   s = spl7();                           \
   *(int *) (addr) = (value);            \
   splx(s);                              \
}

#define MHSIM_DUART_READ(addr, value)  \
{                                        \
   int s;                                \
   s = spl7();                           \
   (value) = *(int *)(addr);             \
   splx(s);                              \
}

#define RD_CNTRL(p, r)          ( 0 )
#define WR_CNTRL(p, r, v)       { (((r) != WR1) ? 0 \
			   : ((*((int *) MHSIM_DUART_INTR_ENBL_ADDR)) = \
				   ((((v) & WR1_TX_INT_ENBL) ? MHSIM_DUART_TX_INTR_STATE : 0) | \
				    (((v) & WR1_RX_INT_MSK) ? MHSIM_DUART_RX_INTR_STATE : 0)))); }

#else
#define RD_CNTRL(p, r)          rd_uart_reg(p, r)
#define WR_CNTRL(p, r, v)       {					\
					int s = spl7();			\
					Z8530_DELAY;			\
					*(p) = (r);			\
				  	Z8530_FBDELAY;			\
					*(p) = (v);			\
				  	Z8530_FLUSHBUS;			\
					splx(s);			\
				}
#endif  /* _SYSTEM_SIMULATION */
#else /* _STANDALONE */


#define INITLOCK_PORT(dp)
#define LOCK_PORT(dp)		spl5()
#define UNLOCK_PORT(dp,s)	splx(s)
#define RD_CNTRL(p, r)          ( Z8530_DELAY, *(p) = (r),	\
				  Z8530_DELAY, *(p) )
#define WR_CNTRL(p, r, v)       { Z8530_DELAY; *(p) = (r);	\
				  Z8530_DELAY; *(p) = (v); }
#endif	/* _STANDALONE */

/*
 * On Indigo there's a hardware bug: RTS/DTR/CTS/DCD are all inverted.
 */
#if IP22 || IP26 || IP28
#define DU_RTS_ASSERT(x)	((x) |= WR5_RTS)
#define DU_DTR_ASSERT(x)	((x) |= WR5_DTR)
#define DU_RTS_DEASSERT(x)	((x) &= ~WR5_RTS)
#define DU_DTR_DEASSERT(x)	((x) &= ~WR5_DTR)
#define DU_RTS_ASSERTED(x)	(((x) & WR5_RTS) != 0)
#define DU_DTR_ASSERTED(x)	(((x) & WR5_DTR) != 0)
#define DU_CTS_ASSERTED(x)	(((x) & RR0_CTS) != 0)
#define DU_DCD_ASSERTED(x)	(((x) & RR0_DCD) != 0)
#define DU_CTS_INVERT(x)	(x = (x) ^ RR0_CTS)
#define DU_DTR_INVERT(x)	(x = (x) ^ WR5_DTR)
#else
#define DU_RTS_ASSERT(x)        ((x) &= ~WR5_RTS)
#define DU_DTR_ASSERT(x)        ((x) &= ~WR5_DTR)
#define DU_RTS_DEASSERT(x)      ((x) |= WR5_RTS)
#define DU_DTR_DEASSERT(x)      ((x) |= WR5_DTR)
#define DU_RTS_ASSERTED(x)      (((x) & WR5_RTS) == 0)
#define DU_DTR_ASSERTED(x)      (((x) & WR5_DTR) == 0)
#define DU_CTS_ASSERTED(x)      (((x) & RR0_CTS) == 0)
#define DU_DCD_ASSERTED(x)      (((x) & RR0_DCD) == 0)
#endif

#endif /* IP20 || IP22 || IP26 || IP28 */

#if EVEREST || SN
/*
 * No DELAY is needed on Everest, since the EPC's PBUS timing 
 * characteristics were chosen with this DUART (and other
 * PBUS devices) in mind.  (The 85130 requires at least 350ns
 * between writes.)
 */

#if _LANGUAGE_C

#ifndef _STANDALONE

/* no need to go to spl7 here since the port must be locked and
 * we now lock at spl7 anyway
 */
#define RD_CNTRL(p, r) \
	(/* du_s = spl7(),*/\
	  *(p) = (r),\
	  du_cntrl = *(p),\
	  /*splx(du_s),*/ du_cntrl )

#define WR_CNTRL(p, r, v) \
	{ /*du_s = spl7();*/\
	  *(p) = (r);\
	  *(p) = (v);\
	  /*splx(du_s);*/ }
#else 
#define RD_CNTRL(p, r) \
	( *(p) = (r), *(p) )

#define WR_CNTRL(p, r, v) \
	{ *(p) = (r); *(p) = (v); }

#endif /* ! _STANDALONE */

#endif /* _LANGUAGE_C */

#define RD_DATA(p) 	( *(p) )
#define RD_RR0(p) 	( *(p) )
#define WR_DATA(p, v) 	{ *(p) = (v); }
#define WR_WR0(p, v) 	{ *(p) = (v); }

#define DU_RTS_ASSERT(x)	((x) |= WR5_RTS)
#define DU_DTR_ASSERT(x)	((x) |= WR5_DTR)
#define DU_RTS_DEASSERT(x)	((x) &= ~WR5_RTS)
#define DU_DTR_DEASSERT(x)	((x) &= ~WR5_DTR)
#define DU_RTS_ASSERTED(x)	(((x) & WR5_RTS) != 0)
#define DU_DTR_ASSERTED(x)	(((x) & WR5_DTR) != 0)
#define DU_CTS_ASSERTED(x)	(((x) & RR0_CTS) != 0)
#define DU_DCD_ASSERTED(x)	(((x) & RR0_DCD) != 0)

/*
 * This driver is somewhat unusual in that it provides both streams
 * and raw character device access for multiple processors to a single
 * piece of hardware (duart).  Streams locking handles most normal
 * sycnchronization, but for rare occasions when the kernel prints
 * something, we need a bit of extra locking to prevent hardware state
 * from being corrupted.
 */
#if _STANDALONE

#define INITLOCK_PORT(unit) \
	{ int duartnum = unit/2;\
	  init_spinlock(&du_lock[duartnum], "dup", duartnum);\
	}

#define LOCK_PORT(unit) \
	splockspl(du_lock[(unit/2)], splhi)

#define UNLOCK_PORT(unit,ospl) \
	spunlockspl(du_lock[(unit/2)], ospl)

#else /* !_STANDALONE */

#define SET_SPL()		spl7()
#define SPLX(s)			splx(s)

#define INITLOCK_PORT(dp) \
	{ int duartnum = (dp->dp_index/2)*2;\
	  init_spinlock(&(dports[duartnum].dp_port_lock), \
		    "dup", duartnum);\
          dports[duartnum].dp_port_lockcpu = -1;\
          dports[duartnum].dp_port_locktrips = 0;\
        }

#if 0
#define LOCK_PORT(dp) \
	splockspl(dports[(dp->dp_index/2)*2].dp_port_lock, spl7)

#define UNLOCK_PORT(dp, ospl) \
	spunlockspl(dports[(dp->dp_index/2)*2].dp_port_lock, ospl)
#endif
#define LOCK_PORT(dp) du_lock_port(dp, 0)
#define TRY_LOCK_PORT(dp) du_lock_port(dp, 1)
#define UNLOCK_PORT(dp, ospl) du_unlock_port(dp, ospl)

#endif /* !_STANDALONE */

#endif /* EVEREST || SN */

#endif	/* _KERNEL || _STANDALONE */

/*
 * ioctl(fd, I_STR, arg)
 * use the SIOC_RS422 and SIOC_EXTCLK combination to support MIDI
 */
#define	SIOC		('z' << 8)	/* z for z85130 */
#define	SIOC_EXTCLK	(SIOC | 1)	/* select/de-select external clock */
#define	SIOC_RS422	(SIOC | 2)	/* select/de-select RS422 protocol */
#define	SIOC_ITIMER	(SIOC | 3)	/* upstream timer adjustment */
#ifdef DEBUG
#define	SIOC_LOOPBACK	(SIOC | 4)	/* diagnostic loopback test mode */
#endif

/* arg for SIOC_EXTCLK */
#define	EXTCLK_64X	0xc0	/* external clock is 64 times of data rate */
#define	EXTCLK_32X	0x80	/* external clock is 32 times of data rate */
#define	EXTCLK_16X	0x40	/* external clock is 16 times of data rate */
#define	EXTCLK_1X	0x00	/* external clock is data rate */
#define	EXTCLK_OFF	0xff	/* de-select external clock, use internal
				   baud rate generator */

/* arg for SIOC_RS422 */
#define	RS422_ON	0x1	/* select RS422 protocol */
#define	RS422_OFF	0x0	/* de-select RS422 protocol */

#endif /* SYS_Z8530_H */
