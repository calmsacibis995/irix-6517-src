#ident	"IP12diags/parallel/plp.h:  $Revision: 1.7 $"

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
#define	PRINTER_ONLINE		0x08
#define	PRINTER_NOINK		0x04
#define	PRINTER_NOPAPER		0x02
#define	PRINTER_FAULT		0x01

#define	PRINTER_PRT		0x02
#define	PRINTER_RESET		0x01
/* memory descriptor structure 
 */
struct md {
    unsigned int bc;
    unsigned int cbp;
    unsigned int nbdp;
};

int plpl_dma_start(struct md *, unsigned, unsigned char *, unsigned);
int plpl_dma_wait(void);

int plp_dma_start(struct md *, unsigned, unsigned char *, unsigned);
int plp_dma_wait(void);

int plp_dbginfo(void);
