#ident	"$Revision: 1.5 $"

#define PAR_CTRL_DIAG	0x7f702100		/* strobe params */
#define PAR_CTRL_STB	0x17120300		/* strobe params */

#define	FIRST_CHAR	0x20
#define	LAST_CHAR	0x5f
#define	NCHARS		(LAST_CHAR - FIRST_CHAR + 1 + 2) /* 2 for lf/cr */
#define	RWCHARS		(LAST_CHAR - FIRST_CHAR + 1)

#define	LFIRST_CHAR		0x0
#define	LLAST_CHAR		0xff
#define	LNCHARS			(LLAST_CHAR - LFIRST_CHAR + 1 + 1) /*1 for lf*/
#define	LRWCHARS			(LLAST_CHAR - LFIRST_CHAR+1)

#define MS1			1000		/* 1 ms */
#define	PRINTER_DELAY		(5 * 1000)	/* 5 sec */
#define	PRINTER_RESET_DELAY	8		/* 8 ms */

#define PRINTER_STATUS_MASK	0X0f
#define	PRINTER_ONLINE		0x10
#define	PRINTER_NOINK		0x04
#define	PRINTER_NOPAPER		0x20
#define	PRINTER_FAULT		0x08

#define	PRINTER_CTRL_RESET	0x02
#define	PRINTER_PRT		0x02
#define	PRINTER_RESET		0x02

#define PBUS_DMASTART           0x08 /* set pbus-ctrl to write dma & start */
#define PBUS_INT_PEND           0x01 /* interrupt pending on reads */
#define PI1_DMA_MEMTODEV        0x40 /* set PI1 to write dma */
#define PI1_DMA_START           0x80 /* set PI1 to start dma */
#define PI1_BLK_MODE            0x10 /* set PI1 to block mode */
#define PI1_FIFOEM_INT          0x08 /* is fifo empty interrupt */
#define PI1_DMA_ABORT           0x02 /* reset all dma */

/* memory descriptor structure 
 */
struct md {
    __uint32_t cbp;
    __uint32_t bc;
    __uint32_t nbdp;
    __uint32_t pad;
};

void plp_dbginfo(void);
