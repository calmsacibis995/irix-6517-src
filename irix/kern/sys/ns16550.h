#ifndef __SYS_NS16550_H__
#define __SYS_NS16550_H__

#define MIPS_SER0_BASE	PHYS_TO_COMPATK1(IOC3_UART_A)
#define MIPS_SER1_BASE	PHYS_TO_COMPATK1(IOC3_UART_B)

#if LANGUAGE_C
typedef	uchar_t		uart_reg_t;

/*
 * SER_BASE should be an uchar_t/__uint32_t/__uint64_t pointer if uart
 * device address is 8/32/64 bits aligned
 */
#define	RD_REG(SER_BASE,REG)		((uchar_t)(*(SER_BASE + REG)))
#define	WR_REG(SER_BASE,REG,VAL)	(*(SER_BASE + REG) = VAL)
#endif	/* LANGUAGE_C */

#define	RCV_BUF_REG		0x0		/* RBR, DLAB=0 in the LCR */
#define	XMIT_BUF_REG		0x0		/* THR, DLAB=0 in the LCR */
#define	INTR_ENABLE_REG		0x1		/* IER, DLAB=0 in the LCR */
#define	INTR_ID_REG		0x2		/* IIR */
#define	FIFO_CNTRL_REG		0x2		/* FCR */
#define	LINE_CNTRL_REG		0x3		/* LCR */
#define	MODEM_CNTRL_REG		0x4		/* MCR */
#define	LINE_STATUS_REG		0x5		/* LSR */
#define	MODEM_STATUS_REG	0x6		/* MSR */
#define	SCRATCH_PAD_REG		0x7		/* SCR */
#define	DIVISOR_LATCH_LSB_REG	0x0		/* DLL, DLAB=1 in the LCR */
#define	DIVISOR_LATCH_MSB_REG	0x1		/* DLM, DLAB=1 in the LCR */

#define TI_SER_XIN_CLK		22000000
#define TI_SER_PREDIVISOR       3

#define SER_XIN_CLK		TI_SER_XIN_CLK
#define SER_PREDIVISOR		TI_SER_PREDIVISOR

/* generate a UART clock from a pre-divisor */
#define SER_CLK_SPEED(prediv)           ((SER_XIN_CLK << 1) / prediv)
#define	SER_DIVISOR(x, clk)		(((clk) + (x) * 8) / ((x) * 16))
#define DIVISOR_TO_BAUD(div, clk)	((clk) / 16 / (div))

#define	IER_ENABLE_RCV_INTR		0x01
#define	IER_ENABLE_XMIT_INTR		0x02
#define	IER_ENABLE_LINE_STATUS_INTR	0x04
#define	IER_ENABLE_MODEM_STATUS_INTR	0x08

#define	IIR_NO_INTR			0x01
#define	IIR_LINE_STATUS_INTR		0x06
#define	IIR_RCV_INTR			0x04
#define	IIR_RCV_TIMEOUT_INTR		0x0c
#define	IIR_XMIT_INTR			0x02
#define	IIR_MODEM_STATUS_INTR		0x00
#define	IIR_INTR_MASK			0x0f

#define	FCR_ENABLE_FIFO			0x01
#define	FCR_RCV_FIFO_RESET		0x02
#define	FCR_XMIT_FIFO_RESET		0x04
#define	FCR_RCV_TRIGGER_1		0x00
#define	FCR_RCV_TRIGGER_4		0x40
#define	FCR_RCV_TRIGGER_8		0x80
#define	FCR_RCV_TRIGGER_14		0xc0
#define	FCR_RCV_TRIGGER_MASK		0xc0

#define	LCR_5_BITS_CHAR			0x00
#define	LCR_6_BITS_CHAR			0x01
#define	LCR_7_BITS_CHAR			0x02
#define	LCR_8_BITS_CHAR			0x03
#define LCR_MASK_BITS_CHAR		0x03 /* bits per char mask */
#define	LCR_1_STOP_BITS			0x00
#define	LCR_2_STOP_BITS			0x04
#define LCR_MASK_STOP_BITS		0x04
#define	LCR_ENABLE_PARITY		0x08
#define	LCR_EVEN_PARITY			0x10
#define	LCR_STICK_PARITY		0x20
#define	LCR_SET_BREAK			0x40
#define	LCR_DLAB			0x80

#define	MCR_DTR				0x01
#define	MCR_RTS				0x02
#define	MCR_OUT1			0x04
#define	MCR_ENABLE_IRQ			0x08
#define	MCR_LOOP			0x10

#define	LSR_DATA_READY			0x01
#define	LSR_OVERRUN_ERR			0x02
#define	LSR_PARITY_ERR			0x04
#define	LSR_FRAMING_ERR			0x08
#define	LSR_BREAK			0x10
#define	LSR_XMIT_BUF_EMPTY		0x20
#define	LSR_XMIT_EMPTY			0x40
#define	LSR_RCV_FIFO_ERR		0x80

#define	MSR_DELTA_CTS			0x01
#define	MSR_DELTA_DSR			0x02
#define	MSR_DELTA_RI			0x04
#define	MSR_DELTA_DCD			0x08
#define	MSR_CTS				0x10
#define	MSR_DSR				0x20
#define	MSR_RI				0x40
#define	MSR_DCD				0x80

#define	XMIT_FIFO_SIZE			16
#define	RCV_FIFO_SIZE			16

#endif /* __SYS_NS16550_H__ */
