/* @(#) cy.h 	Ioctl definitions for VME to Low Speed Channel Interface
 *
 ******************************************************************************
 *	The software described herein is public domain and may be used,  
 *	copied, or modified by anyone without restriction.
 ******************************************************************************
 *
 *	cyioctl contains definitions needed for CY driver user applications
 *	to build their own protocol headers, called message propers, and
 *	to do any necessary ioctl calls.
 */
#define	NCHAN	8		/* number of logical channels (<= 64) */

/*	cy_msg is the format of the message proper needed for header mode */

#define	CY_HDR								\
	unsigned	u1:15;		/* unused */			\
	unsigned	ad:1;		/* associated data */		\
	unsigned	seccnt:16;	/* count of Cray sectors */	\
	unsigned char	to[2];		/* destination */		\
	unsigned char	from[2];	/* source */			\
	unsigned short	count;		/* MP number for debugging */	\
	unsigned char	CYtype;		/* TCP/IP type */		\
	unsigned char	CYoffset;	/* TCP/IP offset  == 4 */	\
	unsigned int	len;		/* AD length for debugging */

struct	cy_hdr	{
	CY_HDR
};

#define CY_IPLINK	0x34
#define	CY_XTPLINK	0x44
#define CY_IPOFF	4

struct cy_msg	{
	CY_HDR
	unsigned char	data[48];	/* plain old data */
};

#ifdef	NDJ
#define	NDJS	NDJ
#else
#define	NDJS	65			/* should be set = NDJ */
#endif
	/* s t a t i s t i c s */
struct cystats {

	long		no_read;	/* no of read requests  */
	long		no_b_read;	/* no of bytes read  */
	long		no_write;	/* no of write requests */
	long		no_b_write;	/* no of bytes written */
	long		no_error;	/* no of errors */
	long		no_tout;	/* no of time outs */
	long		no_disc;	/* no of discards */
	long		no_cancel;	/* no of cancels */
	long		no_bcopy;	/* no i/o forced to copy */
	long		no_disj;	/* no i/o forced to disjointio */
	long		no_disjlist;	/* no i/o disjointlist calls */
	long		no_disjs[NDJS];	/* no i/o in each disj count */
	long		no_if_ombufs;	/* no extra output mbufs added */
	long		no_intr[4];	/* no interrupts, each type */
	long		no_blkcopyr;	/* no of read blks copied to k buf */
	long		no_blkcopyw;	/* no of write blks copied from k buf */
	long		no_if_startlist;/* no if_outputs wo/copy */
	long		no_if_stnolist;	/* no if_outputs w/copy */
	long		no_if_oerrors;	/* no if_output errors */
	long		no_if_opackets;	/* no if_output packets */
};

/*
 * cyioctl commands
 */
#define CYINFO		0xF1		/* return cy_info structure  */
#define CYERR		0xF2		/* sleep until error */
#define CYTRACE		0xF3		/* dump internal trace buffer */
#define CYMCLR		0xf4		/* master clear interface (suser) */
#define CYHEADER	0xf5		/* User builds and reads MPs */
#define CYPEEK		0xf6		/* Dump HW regs */
#define CYBBUF		0xf7		/* dump bufs */
#define CYMPBUF		0xf8		/* dump MP bufs */
#define CYDISC		0xf9		/* Disable disc intr for uncabling */
#define CYDSTART	0xfa		/* Send IO Master Clear, raw mode */
#define CYDDUMP		0xfb		/* Send Deaddump, enter raw mode */
#define CYTRACECLEAR	0xfc		/* Empty trace buffer */
#define	CYSTAT		0xfd		/* Obtain driver maintained statistics */
#define	CYOPTIONS	0xfe		/* Control performance, methods */
#define	CYBUFLIMIT	0xff		/* Shorten bcopy buffer for tests */
#define	CYSTATCLEAR	0xe0		/* zero out statistics area */
#define CYTRACEST	0xe1		/* dump internal trace structure */

/*
 *	this cv_peek structure contains the layout of the readable registers
 *	as returned to a user through an ioctl CYPEEK call.
 */
struct	cv_peek {
	unsigned short	sr;			/* status */
	unsigned char	icr0;			/* disc intr cntl */
	unsigned char	icr1;			/* mclr intr cntl */
	unsigned char	icr2;			/* dma0 intr cntl */
	unsigned char	icr3;			/* dma1 intr cntl */
	unsigned char	ivr0;			/* disc intr vector */
	unsigned char	ivr1;			/* mclr intr vector */
	unsigned char	ivr2;			/* dma0 intr vector */
	unsigned char	ivr3;			/* dma1 intr vector */
	unsigned short	ircr;			/* input read cntl */
	unsigned short	irwc;			/* input read word count */
	unsigned short	irac;			/* input read address counter */
	unsigned short	orcr;			/* output read cntl */
	unsigned short	orwc;			/* output read word count */
	unsigned short	orac;			/* output read addr counter */
};
