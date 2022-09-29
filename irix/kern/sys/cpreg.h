/*	cpreg.h	6.1	83/07/29	*/

/* 
 * Integrated Solutions Communication Processor definitions
 */
struct cpdevice {
	ushort	cp_sel;			/* selection register */
#define	SEL_MC		0x8000		/* master clear */
#define	SEL_MV		0x4000		/* multi vector */
#define	SEL_SHIFT	4
#define	SEL_SILO	16 << SEL_SHIFT
#define	SEL_LP		17 << SEL_SHIFT
#define SEL_CONF_NLINES	0x001F		/* number of lines supported */
#define SEL_CONF_BR	0x0020		/* baud rate table used */
#define SEL_CONF_LP	0x0040		/* line printer supported */
#define SEL_CONF_LPCEN	0x0080		/* line printer is CENTRONICS */
#define SEL_CONF_CLK	0x0100		/* battery clock supported */
#define SEL_CONF_QIC2	0x0200		/* qic2 supported */
#define SEL_CONF_DRV	0x0400		/* drv11 supported */
	ushort	cp_isr; 		/* interrupt status register */
#define	ISR_NI		0x80		/* non-existant memory interrupt */
#define	ISR_SI		0x40		/* silo service interrupt */
#define	ISR_TI		0x20		/* transmitter service interrupt */
#define	ISR_CI		0x10		/* carrier transition interrupt */
#define	ISR_RI		0x08		/* ring transition interrupt */
#define	ISR_PI		0x04		/* printer service interrupt */
#define	ISR_IE		(ISR_NI|ISR_SI|ISR_TI|ISR_CI|ISR_RI|ISR_PI)
	ushort	cp_ler;			/* line enable register */
	ushort	cp_tcr;			/* transmit control register */
	ushort cp_brk;			/* break register */
	ushort	cp_swr;			/* silo window register */
#define	SWR_VDP		0x8000		/* valid data present */
#define	SWR_FE		0x4000		/* framing error/break */
#define	SWR_PE		0x2000		/* parity error */
#define	SWR_DO		0x1000		/* data overrun error */
#define	SWR_LN_MASK	0x0F00		/* mask for line */
#define SWR_LN_SHIFT	8		/* shift for line number */
#define	SWR_CH_MASK	0x00FF		/* mask for character */
	ushort	cp_acr;			/* assert carrier register */
	ushort	cp_dcr;			/* detect carrier register */
	ushort	cp_drr;			/* detect ring register */
	ushort	cp_pr;			/* parameter register */
#define	PR_BITS5	0x0000
#define	PR_BITS6	0x0100
#define	PR_BITS7	0x0200
#define	PR_BITS8	0x0300
#define	PR_TWOSB	0x0400
#define PR_ONESB	0
#define	PR_OPAR		0x0800
#define	PR_PENABLE	0x1000
#define	PR_EPAR		0x0000
#define	PR_XOFF		0x4000		/*   stop transmit on recieve of ^S */
#define	PR_HDUPLX	0x8000
	ushort	cp_sr;			/* status register */
#define	LSR_GO		0x0001		/*   LINE PRINTER STATUS: go bit */
#define	LSR_FLUSH	0x0002		/*   flush action go bit */
#define	LSR_CABLE	0x0200		/*   cable attached */
#define	LSR_PAPE	0x0400		/*   paper empty */
#define	LSR_BUSY	0x0800		/*   busy */
#define	LSR_SEL		0x1000		/*   selected KLUDGE */
#define	LSR_RDY		0x2000		/*   ready, no fault*/
#define	LSR_FF		0x4000		/*   fifo full */
#define	LSR_FE		0x8000		/*   fifo empty */
#define	LSR_BITS	\
	"\20\20FIFOE\17FIFOF\16READY\15SELECT\14BUSY\13PAPER_EMPTY\2FLUSH\1GO"
/*	char	*cp_ba;			/* bus address registers */
	ushort	cp_bah;
	ushort	cp_bal;
	ushort	cp_bc;			/* byte count register */
};

/* minor numbers for line printers */
#define	ISLP(dev)	(minor(dev)&0x80)
#define	CPUNIT(m)	(((m)&0x70)>>4)
#define	CPLINE(m)	((m)&0x0F)
#define	LPCANON(m)	CPLINE(m)
#define	LP_CANON_CAP	1		/* printer only supports lower case */
#define	LP_CANON_RAW	2		/* do not canonize, just dump chars */
#define LP_MAXCOL	132		/* overide with flag in qb_device */
