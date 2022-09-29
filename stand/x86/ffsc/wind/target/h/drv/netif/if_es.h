/* if_es.h - Dynatem etherstar interface header */

/* Copyright (c) 1988 Wind River Systems, Inc. */

/*
modification history
--------------------
01a,15oct90,phf  written.
*/

#ifndef	__INCif_esh
#define	__INCif_esh

#ifdef	__cplusplus
extern "C" {
#endif

#define INET

#define ES_MIN_FIRST_DATA_CHAIN	96	/* min size of 1st buf in data-chain */
#define ES_MAX_MDS		128	/* max number of [r|t]md's */

typedef struct {
   	u_char dlcr0;		/* $00 */
#define	es_tmt_ok	0x80
#define	es_net_bsy	0x40
#define	es_16_col	0x02
   	u_char dlcr1;		/* $01  */
#define ES_TMT_MASK	0x82
   	u_char dlcr2;		/* $02 */
#define	es_pkt_rdy	0x80
#define es_bus_rd_err	0x40
   	u_char dlcr3;		/* $03 */
#define ES_RCV_MASK	0x80
   	u_char dlcr4;		/* $04 */
#define	es_lbc		0x02
#define	es_tm		0x04
   	u_char dlcr5;		/* $05 */
#define	es_buf_emp	0x40
   	u_char dlcr6;		/* $06 */
#define	es_ena_dlc	0x80
   	u_char dlcr7;		/* $07 */
   	u_char dlcr8;		/* $08 */
   	u_char dlcr9;		/* $09 */
   	u_char dlcr10;		/* $0a */
   	u_char dlcr11;		/* $0b */
   	u_char dlcr12;		/* $0c */
   	u_char dlcr13;		/* $0d */
   	u_char dlcr14;		/* $0e */
   	u_char dlcr15;		/* $0f */
	u_short bmpr0;		/* $10 */
/*ccc	u_short bmpr1; */
	u_short bmpr2;		/* $12 */
/*XXX  	u_short bmpr3; */
   	u_char bmpr4;		/* $14 */
	u_char extra15;
	u_char extra16;
	u_char extra17;
	u_char extra18;
	u_char extra19;
	u_char extra1a;
	u_char extra1b;
	u_char extra1c;
	u_char extra1d;
	u_char extra1e;
	u_char extra1f;
	u_char ROM_IDa;
	u_char eth_vector;	/* $21 */
	u_char ROM_IDb;
	u_char eth_vector1;
	u_char ROM_IDc;
	u_char eth_vector2;
	u_char ROM_IDd;
	u_char eth_vector3;
	u_char ROM_IDe;
	u_char eth_vector4;
	u_char ROM_IDf;
	u_char eth_vector5;
    	} ES_DEVICE;


#define	ls_if		ls_ac.ac_if		/* network-visible interface */
#define	ls_enaddr 	ls_ac.ac_enaddr		/* hardware ethernet address */

/*
 * Initialization Block.
 * Specifies addresses of receive and transmit descriptor rings.
 */
typedef struct esIB
    {
    u_short	esIBMode;		/* mode register */
    char	esIBPadr [6]; /* PADR: byte swapped ethernet physical address */
					/* LADRF: logical address filter */
    u_short	esIBLadrfLow;		/* least significant word */
    u_short	esIBLadrfMidLow;	/* low middle word */
    u_short	esIBLadrfMidHigh;  	/* high middle word */
    u_short	esIBLadrfHigh;		/* most significant word */
					/* RDRA: read ring address */
    u_short	esIBRdraLow;		/* low word */
    u_short	esIBRdraHigh;		/* high word */
					/* TDRA: transmit ring address */
    u_short	esIBTdraLow;		/* low word */
    u_short	esIBTdraHigh;		/* high word */
    } es_ib;

/*
 * Receive Message Descriptor Entry.
 * Four words per entry.  Number of entries must be a power of two.
 */
typedef struct esRMD
    {
    u_short	esRMD0;		/* bits 15:00 of receive buffer address */
    union
	{
        u_short	    esRMD1;	/* bits 23:16 of receive buffer address */
	u_short     es_rmd1b;
        } es_rmd1;

    u_short	esRMD2;			/* buffer byte count (negative) */
    u_short	esRMD3;			/* message byte count */
    } es_rmd;

#define	esrmd1_OWN	0x8000		/* Own */
#define esrmd1_ERR      0x4000          /* Error */
#define esrmd1_FRAM     0x2000          /* Framming Error */
#define esrmd1_OFLO     0x1000          /* Overflow */
#define esrmd1_CRC      0x0800          /* CRC */
#define esrmd1_BUFF     0x0400          /* Buffer Error */
#define esrmd1_STP      0x0200          /* Start of Packet */
#define esrmd1_ENP      0x0100          /* End of Packet */
#define esrmd1_HADR     0x00FF          /* High Address */

#define RMD_ERR		0x6000

#define	rbuf_ladr	esRMD0
#define rbuf_rmd1	es_rmd1.esRMD1
#define	rbuf_bcnt	esRMD2
#define	rbuf_mcnt	esRMD3

/*
 * Transmit Message Descriptor Entry.
 * Four words per entry.  Number of entries must be a power of two.
 */
typedef struct esTMD
    {
    u_short	esTMD0;		/* bits 15:00 of transmit buffer address */
    union
	{
        u_short	    esTMD1;	/* bits 23:16 of transmit buffer address */
	u_short     es_tmd1b;
	} es_tmd1;
    u_short	esTMD2;			/* message byte count */
    union
	{
	u_short	    esTMD3;		/* errors */
	u_short     es_tmd3b;
        } es_tmd3;
    } es_tmd;

#define estmd1_OWN      0x8000          /* Own */
#define estmd1_ERR      0x4000          /* Error */
#define estmd1_MORE     0x1000          /* More than One Retry */
#define estmd1_ONE      0x0800          /* One Retry */
#define estmd1_DEF      0x0400          /* Deferred */
#define estmd1_STP      0x0200          /* Start of Packet */
#define estmd1_ENP      0x0100          /* End of Packet */
#define estmd1_HADR     0x00FF          /* High Address */

#define estmd3_BUFF     0x8000          /* Buffer Error */
#define estmd3_UFLO     0x4000          /* Underflow Error */

#define estmd3_LCOL     0x1000          /* Late Collision */
#define estmd3_LCAR     0x0800          /* Lost Carrier */
#define estmd3_RTRY     0x0400          /* Retry Error */
#define estmd3_TDR      0x03FF          /* Time Domain Reflectometry */

#define TMD_OWN		0x8000
#define TMD_ERR		0x6000

#define TMD_BUFF	0x8000
#define TMD_UFLO	0x4000
#define TMD_LCAR	0x0800

#define	tbuf_ladr	esTMD0
#define tbuf_tmd1	es_tmd1.esTMD1
#define	tbuf_bcnt	esTMD2
#define tbuf_tmd3	es_tmd3.esTMD3

#define	ES_BUFSIZE	(ETHERMTU + sizeof (struct ether_header) + 6)

#define ES_RMD_RLEN	5	/* ring size as a power of 2 -- 32 RMD's */
#define ES_TMD_TLEN	5	/* same for transmit ring    -- 32 TMD's */
#define ES_NUM_RESERVES	8	/* have up to 8 reserved RMD's for loans */

/* Ethernet software status per interface.
 *
 * Each interface is referenced by a network interface structure,
 * ls_if, which the routing code uses to locate the interface.
 * This structure contains the output queue for the interface,
 * its address, etc.
 */

struct ls_softc
    {
    struct arpcom  ls_ac;		/* ethernet common part */
    es_ib	  *ib;			/* Initialization Block */
    struct
	{
        int	r_po2;			/* RMD ring size as a power of 2! */
	int	r_size;			/* RMD ring size (power of 2!) */
	int	r_index;		/* index into RMD ring */
	es_rmd	*r_ring;		/* RMD ring */
	char	*r_bufs;		/* receive buffers base */
        } rmd_ring;
    struct 
	{
        int	t_po2;			/* TMD ring size as a power of 2! */
	int	t_size;			/* TMD ring size (power of 2!) */
	int	t_index;		/* index into TMD ring */
	int	d_index;		/* index into TMD ring */
	es_tmd	*t_ring;		/* TMD ring */
	char	*t_bufs;		/* transmit buffers base */
        } tmd_ring;

    u_char	ls_flags;		/* misc control flags */
    int		ivec;			/* interrupt vector */
    int		ilevel;			/* interrupt level */
    ES_DEVICE	*devAdrs;		/* device structure address */
    char	*memBase;		/* LANCE memory pool base */
    int	  	memWidth;		/* width of data port */
    int		csr0Errs;		/* count of csr0 errors */

    int		bufSize;		/* size of buffer in the LANCE ring */
    BOOL	canLoanRmds;		/* can we loan out rmd's as clusters? */
    char	*freeBufs[ES_NUM_RESERVES];	/* available reserve buffers */
    int		nFree;			/* number of free reserve buffers */
    u_char	loanRefCnt[ES_MAX_MDS];	/* reference counts for loaned RMD's */
    };

#define LS_PROMISCUOUS_FLAG	0x1
#define LS_MEM_ALLOC_FLAG	0x2
#define LS_PAD_USED_FLAG	0x4
#define LS_RCV_HANDLING_FLAG	0x8
#define LS_START_OUTPUT_FLAG	0x10

#define	ls_if		ls_ac.ac_if		/* network-visible interface */
#define	ls_enaddr 	ls_ac.ac_enaddr		/* hardware ethernet address */

#define	ls_rpo2		rmd_ring.r_po2
#define	ls_rsize	rmd_ring.r_size
#define	ls_rindex	rmd_ring.r_index
#define	ls_rring	rmd_ring.r_ring

#define	ls_tpo2		tmd_ring.t_po2
#define	ls_tsize	tmd_ring.t_size
#define	ls_tindex	tmd_ring.t_index
#define	ls_dindex	tmd_ring.d_index
#define	ls_tring	tmd_ring.t_ring

#ifdef	__cplusplus
}
#endif

#endif	/* __INCif_esh */
