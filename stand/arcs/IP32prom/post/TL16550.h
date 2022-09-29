/*****************************************
 * TL16550 serial controller related definitions
 *
 *
 */

/*
 * 16550 registers definitions
 */

/*
 * register offsets
 */

#define     REG_DATA_IN_OUT     0x00
#define     REG_IER             0x01
#define     REG_IIR             0x02
#define     REG_FCR             0x02
#define     REG_LCR             0x03
#define     REG_MCR             0x04
#define     REG_LSR             0x05
#define     REG_MSR             0x06
#define     REG_SCR             0x07


/*
 * Interrupt enable register bit definitions
 */

#define UART_IER_ERDAI		0x01
#define UART_IER_ETHREI		0x02
#define UART_IER_ERLSI		0x04
#define UART_IER_EMSI		0x08

/*
 * Fifo Control register bit definitions
 */

#define UART_FCR_FIFO_ENABLE	0x01
#define UART_FCR_RCVFIFO_RST	0x02
#define UART_FCR_XMTFIFO_RST	0x04
#define UART_FCR_DMA_MODE	0x08
#define UART_FCR_RCV_TRIG_LSB	0x40
#define UART_FCR_RCV_TRIG_MSB	0x80

/*
 * Line control register bit definitions
 */

#define UART_LCR_WLS0		0x01
#define UART_LCR_WLS1		0x02
#define UART_LCR_STB		0x04
#define UART_LCR_PEN		0x08
#define UART_LCR_EPS		0x10
#define UART_LCR_STP		0x20
#define UART_LCR_BRK		0x40
#define UART_LCR_DLAB		0x80


/*
 * Modem control register bit definitions
 */

#define UART_MCR_DTR		0x01
#define UART_MCR_RTS		0x02
#define UART_MCR_OUT1		0x04
#define UART_MCR_OUT2		0x08
#define UART_MCR_LOOP		0x10
#define UART_MCR_AFCE		0x20

/*
 * Line status register bit definitions
 */

#define UART_LSR_DR		0x01
#define UART_LSR_ORERR		0x02
#define UART_LSR_PERR		0x04
#define UART_LSR_FERR		0x08
#define UART_LSR_BI		0x10
#define UART_LSR_THR		0x20
#define UART_LSR_TER		0x40
#define UART_LSR_EIRF	        0x80

/*
 * Modem staus register bit definitions
 */

#define UART_MSR_DCTS		0x01
#define UART_MSR_DDSR		0x02
#define UART_MSR_TERI		0x04
#define UART_MSR_DDCD		0x08
#define UART_MSR_CTS		0x10
#define UART_MSR_DSR		0x20
#define UART_MSR_RI		0x40
#define UART_MSR_DCD	        0x80


/*
 * ISA bus definitions
 */

#define K1_BASE			0xA0000000   
#define PERPH_CONT_OFFSET	0x300000
#define MACE_BASE_ADDRS		0x1F000000
#define ISA_BUS_OFFSET		0x380000
#define SERIAL_A_OFFSET		0x10000
#define SERIAL_B_OFFSET		0x18000

#define ISA_DMA_REG_OFFSET	0x10000

#define ISA_BUS_ADDRS     (K1_BASE+MACE_BASE_ADDRS+ISA_BUS_OFFSET)

#define SERIAL_A_ADDRS       ISA_BUS_ADDRS + SERIAL_A_OFFSET 
#define SERIAL_B_ADDRS       ISA_BUS_ADDRS + SERIAL_B_OFFSET
#define SERIAL_1		0x01
#define SERIAL_2		0x02

#define ISA_RING_BASE_REG    (K1_BASE+MACE_BASE_ADDRS+PERPH_CONT_OFFSET+ISA_DMA_REG_OFFSET)

#define MACE_IO_ADDR		(K1_BASE+MACE_BASE_ADDRS)

#define INTR_STATUS_OFFSET   0x10

#define INTR_MASK_OFFSET     0x18

#define INTR_MASK_REG        (ISA_RING_BASE_REG+INTR_MASK_OFFSET)

#define INTR_STATUS_REG      (ISA_RING_BASE_REG+INTR_STATUS_OFFSET)
/*
 * 16550's registers are mapped into 256 chunks
 */

#define REG_INT_VIEW_OFFSET    0x100

/*
 * DMA related definitions
 */

#define MACE_PAGE_SIZE         0x4000
#define DMA_RING_BUFF_SIZE     0x8000

#define SERIAL_A_DMA_REG_ADDRS (K1_BASE+MACE_BASE_ADDRS+PERPH_CONT_OFFSET+ISA_DMA_REG_OFFSET + (2 * MACE_PAGE_SIZE))

#define SERIAL_B_DMA_REG_ADDRS (K1_BASE+MACE_BASE_ADDRS+PERPH_CONT_OFFSET+ISA_DMA_REG_OFFSET + (3 * MACE_PAGE_SIZE))


#define DMA_ENABLE             0x200
#define DMA_RESET              0x400
#define DMA_CHAN_ACTIVE        0x0

#define DMA_INTR_INACTIVE      0x0
#define DMA_INTR_25            0x20
#define DMA_INTR_50            0x40
#define DMA_INTR_75            0x60
#define DMA_INTR_RING_EMPTY    0x80
#define DMA_INTR_RING_NEMPTY   0xA0
#define DMA_INTR_RING_FULL     0xC0
#define DMA_INTR_RING_NFULL    0xE0


#define SERIAL_A_TX_RING       0x4
#define SERIAL_A_RX_RING       0x5
#define SERIAL_B_TX_RING       0x6
#define SERIAL_B_RX_RING       0x7

#define DMA_RING_SIZE          0x1000

#define TX_CNTRL_INVALID	0x00
#define TX_CNTRL_VALID		0x40
#define TX_CNTRL_MCR		0x80
#define TX_CNTRL_DELAY		0xc0

#define POST_TX_INT		0x20

#define RX_STATUS_VALID		0x80
#define RX_STATUS_BREAK		0x08
#define RX_ERROR_FRAME		0x04
#define RX_ERROR_PARITY		0x02
#define RX_ERROR_OVERRUN	0x01

#define CH1_DEVICE		0x0010000000000000
#define TX1_THRESHOLD		0x0020000000000000
#define TX1_PAIR		0x0040000000000000
#define TX1_MEM_ERROR		0x0080000000000000
#define RX1_THRESHOLD		0x0100000000000000
#define RX1_ERROR		0x0200000000000000
#define CH2_DEVICE		0x0400000000000000
#define TX2_THRESHOLD		0x0800000000000000
#define TX2_PAIR		0x1000000000000000
#define TX2_MEM_ERROR		0x2000000000000000
#define RX2_THRESHOLD		0x4000000000000000
#define RX2_ERROR		0x8000000000000000
#define ALL_SERIAL_INTS		0xfff0000000000000

typedef struct
{
    unsigned long long txCont;
    unsigned long long txRead;
    unsigned long long txWrite;
    unsigned long long txDepth;
    unsigned long long rxCont;
    unsigned long long rxRead;
    unsigned long long rxWrite;
    unsigned long long rxDepth;
} SERIAL_DMA_RING_REG_DESCRIPTOR;
