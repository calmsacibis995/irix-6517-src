/*	PiStruct.h - Edit 6

	LoadICE Version 3
	Copyright (C) 1990-95 Grammar Engine, Inc.
	All rights reserved
	
	NOTICE:  This software source is a licensed copy of Grammar Engine's
	property.  It is supplied to you as part of support and maintenance
	of some Grammar Engine products that you may have purchased.  Use of
	this software is strictly limited to use with such products.  Any
	other use constitutes a violation of this license to you.
*/

/*	 - Data structures for PROMICE support software
	
	This file contains the ROM; FILE; CONFIG and LINK
	data structures as well as other control structures. 
*/

/* PILINK - structure to describe PROMICE chain */

typedef struct
	{
	char	name[PIC_SN];	/* serial device name */
	int		saddr;			/* serial device address (USER) */
	long	brate;			/* baud rate */
	char	pname[PIC_PN];	/* parallel device name */
	int		paddr;			/* parallel port address (USER) */
#ifdef MSDOS
	unsigned ppin;			/* parallel port input port */
	unsigned ppout;			/* parallel port output port */
	unsigned ppdat;			/* parallel port data port */
#endif
#ifdef MAC
	unsigned char *ppin;    /* parallel port input port */
	unsigned char *ppout;   /* parallel port output port */
	unsigned char *ppdat;   /* parallel port data port */
#endif
#ifdef  UNIX
	int	ppin;
	int	ppout;
	int	ppdat;
#endif
	short	flags;				/* flags */
#define	PLSU	0x0001		/* - use user supplied address */
#define	PLHI	0x0002		/* - high speed response */
#define	PLOW	0x0004		/* - slow speed response */
#define	PLST	0x0008		/* - link in start state */
#define	PLPP	0x0100		/* - parallel link in use */
#define	PLPQ	0x0200		/* - parallel link only */
#define	PLPB	0x0400		/* - parallel link is bussed */
#define	PLPU	0x0800		/* - use user supplied address */
#define	PLIN	0x8000		/* - reinit link */
	} PILINK;

/* PIROM - structure to describe PROMICE emulation modules */

typedef struct
	{
	char	ver[4];			/* uCode version */
	long	size;			/* real memory size */
	long	esize;			/* emualtion size */
	long	ssize;			/* socket size */
	long	amask;			/* PROMICE load address mask */
	long	smask;			/* socket mask for different units */
	long	bmask;			/* bank select mask */
	long	cramp;			/* current RAM pointer */
	long	kstart;			/* checksum start address */
	long	kend;			/* checksum end address */
	long	kstore;			/* checksum store address */
	long	ksize;			/* size of checksum in bytes */
	long	fstart;			/* start address for fill */
	long	fend;			/* end address for fill */
	long	fdata;			/* data value */
	long	fdata2;			/* more data if > 32 bits */
	long	fsize;			/* fill data size in bytes */
	short	banks;			/* number of banks */
	short	flags;			/* misc flags */
#define	PRCK	0x0001	/* do checksum */
#define	PRKA	0x0002	/* do addition checksum */
#define	PRK1	0x0004	/* store 1's complement */
#define	PRKO	0x0008	/* store checksum backwards */
#define	PRFL	0x0010	/* fill ROM */
#define	PRLP	0x0020	/* need to load the pointer */
#define	PRPP	0x0040	/* parallel port is ON */
#define	PRMI	0x0080	/* master ID is set */
#define	PRRB	0x0100	/* its a ROMboy */
#define	PRBA	0x0200	/* we are banking */
#define	PRLO	0x4000	/* low performance micro-code pre 6.0A */
#define	PRHI	0x8000	/* high performance micro-code 8.0A+ */
	short	mid;			/* master unit's ID */
	short	sid;			/* slave unit's ID */
	short	res;			/* resource byte */
#define	SLAVE	0x0001	/* slave is present */
#define	GOTAI	0x0002	/* AI is present */
#define	AIS31	0x0004	/* AI is rev 3.1 */
#define	AISUP	0x0008	/* AI link is up */
#define	PISUP	0x0010	/* PI link is up */
	} PIROM;

/* PICONFIG - structure to describe PROMICE (ROM) configuration */

struct PICONFIG
	{
	struct PICONFIG *next;	/* pointer to next bank */
	long	start;			/* start address */
	long	end;			/* last location */
	long	max;			/* max for the whole chain */
	long	lptr;			/* current load pointer for the whole chain */
	long	lct;			/* count before pointer must be reloaded */
	short	words;			/* ROM config word size */
	short	uid[PIC_WS];	/* byte order */
	short	banks;			/* number of banks */
	short	flags;			/* misc flags */
#define	PCOK	0x0001	/* config is defined */
#define	PCFL	0x0002	/* fill space */
#define	PCBA	0x0004	/* banking operation */
#define	PCLO	0x0010	/* units are in load mode */
#define	PCXX	0x8000	/* config is a default */
	};

typedef struct PICONFIG PICONFIG;

/* PIFILE - structure to describe user data files */

typedef	struct
	{
	char	name[PIC_FN+1];	/* name of the file */
	char	htype;			/* hex record type */
	short	type;			/* BIN or HEX file */
#define	PFHEX	0		/* Hex file */
#define	PFBIN	1		/* BINary file */
#define	PFBN2	2		/* formatted binary file */
	long	skip;			/* any data to skip in BIN */
	long	offset;			/* any address offset */
	long	saddr;			/* start address for transfer */
	long	eaddr;			/* end address for transfer */
	PICONFIG *pfcfg;		/* file data configuration */
	short	flags;			/* misc flags */
#define	PFCS	0x0001	/* check hex record checksum */
#define	PFVF	0x0002	/* verify down-load */
#define	PFPL	0x0004	/* partial load */
#define	PFNO	0x0008	/* ignore address out of range error */
#define	PFCF	0x0100	/* user supplied config */
	} PIFILE;

/*	GLOBALS: - structures & variables */

extern	PILINK		pxlink;			/* PROMICE chain */
extern	PICONFIG	pxconfig[PIC_NC];/* ROM configuration */
extern	PICONFIG	pxaltcfg;		/* alternate configuration */
extern	PICONFIG	*pxcfg;			/* current ROM configuration */
extern	PICONFIG	*pxfcfg;		/* free configurations */
extern	PICONFIG	*pxpcfg;		/* physical configuration */
extern	PICONFIG	*pxacfg;		/* active ROM configuration */
extern	PIFILE		pxfile[PIC_NF];	/* files */
extern	PIROM		pxrom[PIC_NR];	/* ROMs */

extern	short		pxnfile;		/* number for files */
extern	short		pxcfile;		/* current file */
extern short		pxnpiu;			/* number of promice units */
extern	short		pxnrom;			/* number of roms */
extern	short		pxcrom;			/* current rom */
extern	short		pxsrom;			/* start loading this one */
extern	short		pxprom;			/* number of PROMICE modules */
extern	char		pxmodei;		/* mode at connect */
extern	char		pxmodeo;		/* mode at exit */
extern	short		pxrtime;		/* target reset time */
extern	short		pxaiid;			/* unit id for the AITTY mode */
extern	short		pxaibchr;		/* AITTY break character */
extern	short		pxhints;		/* number of restars to skip */
extern	short		pxhso;			/* HSO polarity */
extern	short		pxreq;			/* REQ polarity */
extern	short		pxack;			/* ACK polarity */
extern	unsigned char pxcmode;		/* protocol mode */
extern	short		pxtout;			/* timeout has occurred */
extern	long		pxnotot;		/* don't do timeouts */
extern	long		pxdelay;		/* variable timeout delay */
extern long		pxtcpu;			/* target CPU */
extern	long		pxailoc;		/* ailoc for AITTY mode */
extern	long		pxaibr;			/* AITTY baud rate */
extern	long		pxaibrc;		/* code for above */
extern long		pxaiws;			/* target word size for AiCOM use */
extern	long		pxbanks;		/* number of banks */
extern	long		pxbank;			/* current bank */
extern	int			pxexitv;		/* exit value */
extern int			pclog;			/* loggging stuff */
extern	char		pxfirst;		/* first char of input */
extern	unsigned char pxmode1;		/* mode byte 1 */
extern	unsigned char pxmode2;		/* mode byte 2 */
extern	unsigned char pxtintl;		/* external int length */
extern unsigned char pxaieco;		/* AI mode control option */
extern unsigned char pxburst;		/* AI burst control */
extern unsigned char pxwrite;		/* AI write enable signals */
extern char		pxhost[PIC_HN];	/* fastport host name */

extern	char		pxcmd[260];		/* PROMICE command buffer */
#define	PIID		0				/* ID offset */
#define	PICM		1				/* command offset */
#define	PICT		2				/* count offset */
#define	PIDT		3				/* start of data area */
#define	PIMI		4				/* minimum cmd/rsp packet size */
extern	char		pxrsp[260];		/* response from PROMICE */
extern	long		pxcmdl;			/* length of data in `cmd` */
extern	long		pxrspl;			/* lenght of data in `rsp` */

extern	char		pxbbf[PIC_BS];	/* big buffer - file data */
extern	long		pxbbc;			/* data count in `bbf` */
extern	char		pxxbf[PIC_BS];	/* transfer buffer out - ROM data */
extern	long		pxxbc;			/* data count in `xbf` */
extern	char		pxybf[PIC_BS];	/* transfer buffer in - ROM data */
extern	long		pxybc;			/* data count in `ybf` */
extern	char		*pxdbf;			/* buffer pointer for the driver */
extern	long		pxdbc;			/* count for driver transfer */
extern	long		pxxloc;			/* xbuf load pointer */
extern	long		pxyloc;			/* ybuf load pointer */
extern	long		pxcloc;			/* current load pointer */
extern	long		pxmax;			/* last ROM loc in current config */
extern	long		pxdat;			/* global data holder */
extern	long		pxdlc;			/* down-load data counter */
extern	char		*pxkeys[PIC_KK];	/* function key storage */
extern	char		pxcline[PIC_CL];	/* reconstructed command line */
extern	char		pxuline[PIC_CL];	/* user input */

extern	long		pxdstart;		/* dump start address */
extern	long		pxdend;			/* dump end address */
extern	long		pxestart;		/* edit start address */
extern	long		pxelist[PIC_LST]; /* edit data list */
extern	long		pxelcnt;		/* edit list element count */
extern	long		pxfstart;		/* fill start address */
extern	long		pxfend;			/* fill end address */
extern	long		pxfdata;		/* fill data */
extern	long		pxfdata2;		/* more fill data when words>32 */
extern	long		pxfsize;		/* size of data in bytes */
extern	long		pxkstart;		/* checksum start address */
extern	long		pxkend;			/* checksum end address */
extern	long		pxkstore;		/* checksum store address */
extern	long		pxksize;		/* size of checksum in bytes */
extern	long		pxmstart;		/* move start address */
extern	long		pxmend;			/* move end address */
extern	long		pxmdest;		/* move destination address */
extern	long		pxsstart;		/* save start address */
extern	long		pxsend;			/* save end address */
extern	char		*pxlstr;		/* search string */
extern	long		pxlstart;		/* search start address */
extern	long		pxlend;			/* search end address */
extern	char		pxldata[PIC_SS]; /* search value */
extern	long		pxlsize;		/* search size for binary */
extern long		pxcode;			/* exit code on failure to connect */
extern long		pxctry;			/* connect try count */
extern short		ppxdl0;			/* parallel port strobe stretch */
extern short		ppxdl1;			/* parallel port delay */
extern short		ppxdl2;			/* parallel port b_back delay */
extern short		pibusy;			/* PromICE is busy */

extern	long		pxflags;		/* user option flags */
#define	POCP		0x00000001		/* do compare */
#define 	POLO		0x00000002		/* do load */
#define	POIX		0x00000004		/* enter dialog */
#define	POVF		0x00000008		/* verify data */
#define	PONO		0x00000010		/* ignore address out of range */
#define	PODO		0x00000020		/* but display bogus data */
#define	PONK		0x00000040		/* ignore checksum */
#define	PODK		0x00000080		/* but display bogus data */
#define	POKA		0x00000100		/* use addition to compute checksum */
#define	POK1		0x00000200		/* store one's complement checksum */
#define	POKC		0x00000400		/* checksum config */
#define	POKR		0x00000800		/* checksum ROMs */
#define	POKO		0x00001000		/* store checksum backwards */
#define	POFR		0x00002000		/* fill ROMs */
#define	POFC		0x00004000		/* fill config */
#define	POMP		0x00010000		/* display map information */
#define	POAR		0x00020000		/* do auto recovery */
#define	POSP		0x00040000		/* use parallel port in slow mode */
#define	PORQ		0x00080000		/* use REQ/ACK for read/write */
#define	POXX		0x80000000		/* exit */

extern	long		piflags;		/* LoadICE - internal flags */
#define	PiMO		0x00000001		/* need to update mode */
#define	PiEL		0x00000002		/* edit list is given */
#define	PiCM		0x00000004		/* doing command line */
#define	PiiX		0x00000008		/* interactive command */
#define	PiCK		0x00000010		/* do ROM checksum */
#define	PiFM		0x00000020		/* fill memory */
#define	PiSW		0x00000040		/* LoadICE talking through AISwitch */
#define	PiPH		0x00000080		/* check for locked unit/per Pat Hunt */
#define	PiAI		0x00000100		/* set up the AITTY mode */
#define	PiZZ		0x00000200		/* fast host */
#define	PiLP		0x00000400		/* load new pointer */
#define	PiII		0x00000800		/* immediate file processing */
#define	PiNE		0x00001000		/* we have a network link */
#define	PiMU		0x00002000		/* unit(s) emulating */
#define	PiNT		0x00004000		/* no timeout */
#define	PiLO		0x00008000		/* low grade unit seen */
#define	PiRF		0x00010000		/* reset FastPort */
#define	PiCR		0x00020000		/* user type CR */
#define	PiHC		0x00040000		/* hide (spinning) cursor */
#define	PiSS		0x00080000		/* search strings */
#define	PiUP		0x00100000		/* link is up */
#define	PiBC		0x00200000		/* break character specified */
#define	PiFP		0x00400000		/* fast parallel port */
#define	PiXP		0x00800000		/* put parallel port in xprnt mode */
#define	PiDF		0x01000000		/* dump command failed */
#define	PiNL		0x02000000		/* no id list given */
#define	PiSK		0x04000000		/* socket size is given */
#define	PiRQ		0x08000000		/* REQ/ACK polarity change */
#define	PiHO		0x10000000		/* HSO signaling required */
#define	PiSO		0x20000000		/* send command serial only */
#define	PiAB		0x40000000		/* change AI bits */
#define	PiER		0x80000000		/* an error did occur */

extern	short		pxdisp;			/* display control flag */
#define	PXVL		0x0001			/* very low level */
#define	PXLL		0x0002			/* low-low level */
#define	PXLO		0x0004			/* low level */
#define	PXML		0x0008			/* medium-low level */
#define	PXMI		0x0010			/* medium level */
#define	PXMH		0x0020			/* medium-high level */
#define	PXHI		0x0040			/* high level */
#define	PXVH		0x0080			/* very high level */
#define	PLOG		0x8000			/* log traffic to file */

#define	PcALL		0x001f			/* display the whole thing */
#define	PcLNK		0x0001			/* only link information */
#define	PcROM		0x0002			/* only ROM information */
#define	PcFLE		0x0004			/* only FILE information */
#define	PcPCF		0x0008			/* only Config information */
#define	PcCFG		0x0010			/* only current config */
extern char	pxlog[PIC_FN+1];		/* log file name if any */
