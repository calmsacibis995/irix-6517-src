#ifndef __SYS_CFEI_H__
#define __SYS_CFEI_H__

/* @(#) cy.h 	Definitions for VME to Low Speed Channel Interface
 *
 ******************************************************************************
 *	The software described herein is public domain and may be used,  
 *	copied, or modified by anyone without restriction.
 ******************************************************************************
 *
 *  h e a d e r  f i l e  f o r   C R A Y  C h a n n e l   D r i v e r
 */
#ident "$Revision: 1.4 $"

#define SHIFT 0
#define	AUB_CALL	0x01000000		/* SUN-style buf.h flag */
#define	AUB_COPY	0x02000000		/* copy instead of DMA */
#define	AUB_BLKCOPY	0x04000000		/* copy individual page */
#define	AUB_DISJ	0x08000000		/* use list in djnt array */
#define	AUB_MAPPED	0x10000000		/* IP5 vme mapping done */

#define	NDJ	(CYMAX/NBPC + 1)		/* max num of disjoint frags */

/*
 * CY constants
 */
#ifndef NCY
#define NCY 4
#endif
#define CY_TIMEOUT	4		/* time out seconds */ 
#define CY_INVDISC	5		/* # of invalid disc intrs allowed */
#define CYMAX		0x40000		/* arbitrary max size transfer (256)*/

extern int cfei_cnt;			/* number intfs this machine */


/*   c y s t a r t    c o m m a n d s   */
#define STC_MCLR 0				/* cystart cmd: master clear */
#define STC_DISC 1				/* cystart cmd: disconnect */
#define STC_READMP 2				/* cystart cmd: read MP */
#define STC_READ 3				/* cystart cmd: read AD */
#define STC_WRITE 4				/* cystart cmd: write */


/*
 *  c y . f l a g s
 */
#define CY_DISCARD	0x01		/* discarding AD */
#define CY_RBLOCK	0x02		/* block reads because AD present */
#define CY_RAW		0x04		/* this Intf is in raw mode */

/*
 *  c y . o p t i o n s
 */
#define	CO_FCOPY	0x08		/* force copying of data */
#define	CO_FDISJ	0x10		/* force no joining in disjointio */

/*
 *  c y . s t a t e
 */
#define	FS_RESET	0		/* reset  */
#define FS_READ 	1		/* read in progress */
#define FS_WRITE	2		/* write in progress */
#define FS_RDWR		3		/* read and write in progress */

/*
 * data structures
 */
struct djntio {
	paddr_t	addr;
	int	count;
};

/* cyio structs are used by cysetup to return (addr,len) pair */
struct cyio {
	char *addr;
	int len;
};

/* cybufs are used to describe a user's IO request */
struct cybuf {
	struct buf buf;			/* MUST BE FIRST */
	/*
	 *	Augmented buffer variables.
	 */
	int	(*aub_iodone)();	/* call on completion if non-zero */
	struct	cy_msg	*aub_mp_ptr;	/* user's virt I/O buf pointer for mp */
	caddr_t	aub_ad_ptr;		/* user's virt pointer for assoc data */
	int	aub_flags;		/* extended flags */
	struct	cympbuf	*aub_mp;	/* points to associated cympbuf */
	struct	djntio	*aub_djp;	/* point to entry in dj */
	struct	djntio	dj[NDJ];
	struct	cyio	aub_bpt;	/* describe each block in transit */
};

#define	EBP	((struct cybuf *) bp)	/* get at extended portion of buf */

/* cympbuf's are used to store out-going and in-coming message propers */

struct cympbuf {
	struct cympbuf	*m_forw;
	time_t		m_late;
	paddr_t	m_vaddr;		/* DVMA addr of m_msg */
	struct cy_msg	m_msg;
};

#define CYMPNUM	30		/* number of cympbuf's */
#define CYBNUM	20		/* number of bufs in pool */
#define b_cympbuf av_back	/* pointer to message proper */

#define	KBUFSZ	65536

#ifdef _KERNEL

/* cy_ch keeps information on a per logical channel basis */

struct cy_ch	{				/* logical channel struct */
	int		open;		/* channel open flag */
	int		header;		/* user wants message propers */
	int		raw;		/* this channel using raw mode */
	struct buf	*rf;		/* first buf on read queue */
	struct buf	*rl;		/* last buf on read queue  */
	struct cympbuf	*mf;		/* first incoming msg */
	struct cympbuf	*ml;		/* last  incoming msg */
	int		nmsg;		/* number of msg's on queue */
	int		mquota;		/* msg quota exceeded */
	int		mnooc;		/* message, no open channel */
	int		mtout;		/* message proper timeout */
	struct	pollhead inpq;		/* select system call - input */
	struct	pollhead excq;		/* select system call - exceptions */
};

/* cy_info keeps information on a per interface basis */

struct cy_info {			/* 1 per cy interface */
	int		flags;		/* status bits */
	int		cy_options;	/* functionality controlling flags */
	int		cy_buflimit;	/* artificially restrict bcopy size */
	int		state;		/* current state  */
	int		open;		/* number of open CY channels */
	/*int		last;		/* last opened CY channel */
	short		intf;		/* Number of this intf, shifted << 2 */
					/* to match usage of minor device no */
	short		board;		/* board set number */
	int		invdisc;	/* number of invalid disc intrs */
	int		discsecs;	/* seconds to timeout on disconnect */
	time_t		rlate;		/* read time out time/date */
	time_t		wlate;		/* write time out time/date */
	/*struct mb_device *md;		/* pointer to mb_device struct */
	struct cv_device *cvp;		/* pointer to memory mapped HW */
	struct buf	*wf;		/* first buf on write queue */
	struct buf	*wl;		/* last buf on write queue */
	struct buf	*bpr;		/* current read buffer  */
	struct buf	*bpw;		/* current write buffer */
	struct cympbuf	*mp;		/* buffer for "msg proper" */

	struct	cystats	cy_stat;	/* statistics */

	struct cv_stat	cv_stat;	/* contents of write only regs */
	struct cy_ch	cy_ch[NCHAN];	/* 1 per logical channel */

	char	rdbuffer[KBUFSZ];	/* place to read all physio data */
	char	wrbuffer[KBUFSZ];	/* place to write all physio data */
	int		cy_wlen;	/* length of last write */
	int		cy_rlen;	/* length of last issued read */

	struct	cybuf	cy_discbuf;	/* buf descrip. for discard */
	struct	cybuf	cy_rb;		/* buf descrip. for read mp */
	struct	cybuf	cy_wb;		/* buf descrip. for write mp */

	struct	pollhead outq;		/* select system call - output */
};

#endif

/*  		C Y   M a c r o s			 */

/*
 *	These macros work on the minor device number or the interrupt
 *	vector number.
 *		ii.. ....	interface number of minor number 
 *		..ll llll	logical channel of minor number
 *		.... xx..	interface number of interrupt vector
 *		.... ..vv	vector of interrupt vector
 *
 * CY(X) gets the interface number of X, shifted right
 * VEC(X) gets the interface number of X, shifted right
 * CYCH(X) gets the logical channel of X
 */
#define CY(X)	( ((int) (X) >> 6) & 03 )	/* minor -> interface */
#define CYCH(X)	( (int) (X) & 0x3F ) 		/* logical channel */
#define VEC(X)	( ((int) (X) >> 2) & 03 )	/* vector -> interface */

/*
 * For low level debugging, there is a wrap around trace buffer
 * with UTSIZE/4 time-stamped entries.
 */

#define	UTSIZE	8192
struct	cytraceb {
	int	cytb_size;
	int	*cytb_ptr;
	struct	cytraceb *cytb_me;
	int	cytb_buf[UTSIZE];
};

#ifdef	DEBUG

extern	struct	cytraceb	cytracebuf;

#define	UTINIT()					\
	{						\
	cytracebuf.cytb_ptr = cytracebuf.cytb_buf;	\
	cytracebuf.cytb_size = UTSIZE;			\
	cytracebuf.cytb_me = &cytracebuf;		\
	}

#ifdef UTPRINT
#define	UTRACE(M,X,Y)					\
	printf ("%d %x %x %x\n", (int)(time), (int)(M), (int)(X), (int)(Y));
#else
#define	UTRACE(M,X,Y)					\
	{ 						\
	*cytracebuf.cytb_ptr++ = (int)(time);		\
	*cytracebuf.cytb_ptr++ = (int)(M);		\
	*cytracebuf.cytb_ptr++ = (int)(X);		\
	*cytracebuf.cytb_ptr++ = (int)(Y);		\
	if (cytracebuf.cytb_ptr >= &cytracebuf.cytb_buf[UTSIZE])	\
		cytracebuf.cytb_ptr = cytracebuf.cytb_buf;		\
	}
#endif

#define UTDUMP()					\
	{						\
	register int *p;				\
	printf ("\n");					\
	for (p=cytracebuf.cytb_buf; p<&cytracebuf.cytb_buf[UTSIZE]; p += 4) \
		printf ("%x %x %x %x\n", *p, *(p+1), *(p+2), *(p+3));	\
	printf ("\n");					\
	}

/* Values for M */
#define UTR_CLOSE	0x01
#define UTR_SREAD	0x02		/* strategy routine read */
#define UTR_SWRITE	0x03		/* strategy routine write */
#define UTR_IOCTL	0x04
#define UTR_START	0x05
#define UTR_IDISC	0x06
#define UTR_IDMA1	0x07
#define UTR_IWRITE	0x08
#define UTR_TACTIVERD	0x09
#define UTR_TMP		0x0a
#define UTR_TSIG	0x0b
#define UTR_DISCARD	0x0c		/* discard-AD routine called */
#define UTR_CLEAR	0x0d
#define UTR_FSMERR	0x0e
#define UTR_READ	0x0f
#define UTR_WRITE	0x10
#define UTR_STRAT	0x11
#define UTR_IDMP	0x12
#define UTR_FSMNULL	0x13
#define UTR_INTR	0x14
#define UTR_APPEND	0x15
#define UTR_READMP	0x16
#define UTR_IMCLR	0x17
#define UTR_SETUP	0x18
#define UTR_OPEN	0x19
#define	UTR_IWRITE1	0x1a
#define	UTR_IWRITE2	0x1b
#define UTR_STRATEXIT	0x1c
#define UTR_STRATDISJ	0x1d
#define UTR_SETUPDISJ	0x1e
#define UTR_INTREND	0x1f
#define UTR_STARTADDR	0x20
#define UTR_IDISC_MP	0x21
#define UTR_STRATFLGS	0x22
#define UTR_CLOSESTAT	0x23
#define UTR_SETUPVIR	0x24
#define	UTR_IWRITEFUDGE	0x25
#define	UTR_SWRITEFUDGE	0x26
#define	UTR_DISCARDFUDGE 0x27
#define	UTR_READMPFUDGE	0x28
#define	UTR_MAPALLOC	0x29
#define	UTR_MAP		0x2a
#define	UTR_SETUPPHYS	0x2b
#define	UTR_OPENPHYS	0x2c
#define	UTR_STARTMAPRG	0x2d
#define	UTR_DISJCOPY	0x2e
#define	UTR_LISTSTRAT	0x2f
#define	UTR_DISCMPERR	0x30
#define	UTR_DISJLIST	0x31
#define	UTR_MPDUMP	0x32
#define	UTR_MPDUMPADR	0x33
#define	UTR_TDISC	0x34
#define	UTR_STARTDONE	0x35
#define UTR_TACTIVEWR	0x36
#define UTR_ICONDSCMCL	0x37
#define UTR_ICONRDWR	0x38
#define UTR_BAD_INTERRUPT 0x39
#define UTR_TACTRESID	0x3a
#define UTR_LSTRATEXIT	0x3b
#define UTR_NOMPBUFS	0x3c
#define UTR_IDISCLOOP	0x3d
#define UTR_TSIGRWRESID	0x3e
#define UTR_TSIGSTAT	0x3f
#define UTR_STARTICON	0x40
#define UTR_EDTINITDN	0x41
#define UTR_READMP_MP	0x42
#define UTR_POLL	0x43
#define UTR_OCANCELWR	0x44
#define	UTR_OCANRESID	0x45
#define UTR_TACTIVE	0x46
#define	UTR_IF_INPEIO	0x47
#define	UTR_IDISC2	0x48
#define	UTR_IDISCEIO	0x49


/* Values for M for if_cray.c */
#define UTR_IF_ATTACH	0x80
#define	UTR_IF_RESET	0x81
#define	UTR_IF_INIT	0x82
#define	UTR_IF_OUTPUT	0x83
#define	UTR_IF_START	0x84
#define	UTR_IF_IOCTL	0x85
#define	UTR_IF_INTRI	0x86
#define	UTR_IF_FLGCNT	0x87
#define	UTR_IF_DESTCNT	0x88
#define	UTR_IF_NONIPPKT	0x89
#define	UTR_IF_NOMBUFS	0x8a
#define	UTR_IF_UNUSED	0x8b
#define	UTR_IF_INTRO	0x8c
#define	UTR_IF_INTRO1	0x8d
#define	UTR_IF_INPCHAIN	0x8e
#define	UTR_IF_EXTRACT	0x8f
#define	UTR_IF_STRTOLST	0x90
#define	UTR_IF_STRTOHDR	0x91
#define	UTR_IF_CHKSUMLN	0x92
#define	UTR_IF_HSHROUTE	0x93
#define	UTR_IF_NONROUTE	0x94
#define	UTR_IF_BADROUTE	0x95
#define	UTR_IF_LOOPBACK	0x96
#define	UTR_IF_OUTLOOP	0x97
#define	UTR_IF_DROP	0x98
#define	UTR_IF_RTEMTU	0x99
#define	UTR_IF_STRTOMLEN 0x9a
#define UTR_IF_EDTINIT	0x9b

#else
#define	UTINIT()
#define	UTRACE(M,X,Y)
#define	UTDUMP()
#endif

/* Values for X and Y depend on M */


/* til gets defined somewhere: */
#define	IFF_ALIVE		(IFF_UP|IFF_RUNNING)
#define	iff_alive(flags)	(((flags) & IFF_ALIVE) == IFF_ALIVE)
#define iff_dead(flags)		(((flags) & IFF_ALIVE) != IFF_ALIVE)

#endif /* __SYS_CFEI_H__ */
