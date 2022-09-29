/* if_enp.h - CMC ENP 10/L network interface header */

/* Copyright 1984-1993 Wind River Systems, Inc. */
/*
modification history
--------------------
01j,03mar93,ccc  changed BYTE_ORDER, LITTLE_ENDIAN to _BYTE_ORDER,
		 _LITTLE_ENDIAN, SPR 2057.
01i,22sep92,rrr  added support for c++
01h,26may92,rrr  the tree shuffle
01g,04oct91,rrr  passed through the ansification filter
		  -changed READ, WRITE and UPDATE to O_RDONLY O_WRONLY and O_RDWR
		  -changed copyright notice
01f,18jun91,km   added O_WRONLY/READLONG macro and pragma for 960.
01e,22oct90,gae  added BCB transmit and receive bits; reworked ENPSTAT.
01d,05oct90,shl  added copyright notice.
01c,13apr89,del  added ENPDEVICE structure to be compatible with CMC
		 Link-10 v4.0 roms.
01c,04aug88,dnw  added enp_status and associated values.
01b,19nov87,dnw  added e_intvector
01a,27jul87,rdc  adapted from cmc's 4.2 driver.
*/


#ifndef __INCif_enph
#define __INCif_enph

#ifdef __cplusplus
extern "C" {
#endif


#if	(CPU_FAMILY==I960 && defined(__GNUC__))
#pragma	align	1		/* tell gcc960 not to optimize alignments */
#endif	/* CPU_FAMILY==I960 */


/*
 * Since the ENP-10/L supports only 16 bit data accesses,
 * use the following macros to access 32 bit values.
 */

#if	_BYTE_ORDER == _LITTLE_ENDIAN
#define WRITELONG(addr, value) *(short *) addr = (unsigned) value & 0xffff; \
			      *((short *) addr + 1) = (unsigned) value >> 16;
#define READLONG(source, dest) *(short *) dest = *(short *) source; \
				*((short *) dest + 1) = *((short *) source + 1);
#else
#define WRITELONG(addr, value) *(short *) addr = (unsigned) value >> 16; \
			      *((short *) addr + 1) = (unsigned) value & 0xffff;
#define READLONG(source, dest) *(short *) dest = *(short *)source; \
				*((short *) dest + 1) = *((short *) source + 1);
#endif	/* _BYTE_ORDER == _LITTLE_ENDIAN */


#define ENPSTART	0xf02000	/* standard enp start addr */
#define K		*1024

#define ENP_BSS_SIZE	0x102

#define ENPSIZE		(124 K)		/* VME bus space allocated to enp */
#define MINPKTSIZE	60		/* minimum ethernet packet size */

/* Note: paged window (4 K) is identity mapped by ENP kernel to provide
 * 124 K contiguous RAM (as reflected in RAM_SIZE
 */

#define RAM_WINDOW	(128 K)
#define IOACCESS_WINDOW (4 K)
#define FIXED_WINDOW	(RAM_WINDOW - IOACCESS_WINDOW)
#define RAMROM_SWAP	(4 K)
#define RAM_SIZE	(FIXED_WINDOW - RAMROM_SWAP)

#define HOST_RAMSIZE	(48 K)
#define ENP_RAMSIZE	(20 K)

/* ...top of 4K local i/o space for ENP */

typedef struct iow10 {
	char	pad1[0x81];
/* written to: causes an interrupt on the host at the vector written
   read from : returns the most significant 8 bits of the slave address */
	char	vector;
	char	pad2[0x1F];
	char	csrarr[0x1E];
	char	pad3[2];
	char	ier;		/* intr. enable reg., 0x80 == enable,0 == off*/
	char	pad4[1];
	char	tir;		/* transmit intr. (Level 4 INP autovector) */
	char	pad5[1];
	char	rir;		/* receive intr. (Level 5 INP autovector) */
	char	pad6[1];
	char	uir;		/* utility intr. (Level 1 INP autovector) */
	char	pad7[7];
	char	mapfirst4k;	/* bit 7 set means ram, clear means rom */
	char	pad8[0x11];
	char	exr;		/* exception register, see bit defines above */
	char	pad9[0xD1F];
	char	hst2enp_interrupt;	/* R or W interrupts ENP */
	char	pad10[0xFF];
	char	hst2enp_reset;	/* R or W resets ENP */
} iow10;

typedef struct
    {
    u_char enaddr[6];
    } ETHADDR;

typedef struct
    {
    int	e_listsize;		/* active address entries */
    ETHADDR e_baseaddr;		/* address LANCE is working with */
    ETHADDR e_addrs [16];	/* possible addresses */
    } ETHLIST;

typedef struct enpstat
    {
#define	e_xmit_successful e_stat[0]	/* successful transmissions */
#define	e_mult_retry	e_stat[1]	/* multiple retries on xmit */
#define	e_one_retry	e_stat[2]	/* single retries */
#define	e_fail_retry	e_stat[3]	/* too many retries */
#define	e_deferrals	e_stat[4]	/* trsm. delayed due to active medium */
#define	e_xmit_buff_err	e_stat[5] /* xmit data chaining failed - can't happen */
#define	e_silo_underrun	e_stat[6]	/* transmit data fetch failed */
#define	e_late_coll	e_stat[7]	/* collision after xmit */
#define	e_lost_carrier	e_stat[8]
#define	e_babble	e_stat[9]	/* xmit length > 1518 */
#define	e_no_heartbeat	e_stat[10]  /* transceiver mismatch -- not an error */
#define	e_xmit_mem_err	e_stat[11]
#define	e_rcv_successful e_stat[12]	/* good receptions */
#define	e_rcv_missed	e_stat[13]	/* no recv buff available */
#define	e_crc_err	e_stat[14]	/* checksum failed */
#define	e_frame_err	e_stat[15]  /* crc error AND data length != 0 mod 8 */
#define	e_rcv_buff_err	e_stat[16]  /* rcv data chain failure - can't happen */
#define	e_silo_overrun	e_stat[17]	/* receive data store failed */
#define	e_rcv_mem_err	e_stat[18]

    ULONG e_stat [19];
    } ENPSTAT;

typedef struct
    {
    short r_rdidx;
    short r_wrtidx;
    short r_size;
    short r_pad;
    int	r_slot [1];
    } RING;

typedef struct
    {
    short r_rdidx;
    short r_wrtidx;
    short r_size;
    short r_pad;			/* to make VAXen happy */
    int	  r_slot [32];
    } RING32;

/*
 * ENP 10 RAM data layout
 *
 * The two structures that follow are almost identical, the only difference
 * is that the second, ENPDEVICE, contians two extra RING32 fields that
 * CMC decided to add to the middle of the old ENPDEVICE structure
 * (ENPDEVICE_OLD) instead of at the end of the structure, thus
 * making the new structure incompatible with the old structure.
 */

typedef struct {
    char enp_ram_rom[4 K];
    union {
	char	all_ram[RAM_SIZE];
	struct {
	    unsigned short	t_go;
	    unsigned short	t_status;
	    unsigned int	t_pstart;
	} t;
	struct {
	    char	nram[RAM_SIZE - (HOST_RAMSIZE + ENP_RAMSIZE)];
	    char	hram[HOST_RAMSIZE];
	    char	kram[ENP_RAMSIZE];
	} u_ram;
	struct {
	    char	pad7[ 0x100 ];	/* starts 0x1100 - 0x2000 */
	    short	e_enpstate;
	    short	e_enpmode;
	    UINT	e_enpbase;
	    short	e_enprun;
	    short	e_intvector;

	    RING32	h_toenp;
	    RING32	h_hostfree;
	    RING32	e_tohost;
	    RING32 	e_enpfree;

	    ENPSTAT	e_stat;
	    ETHLIST	e_netaddr;
	} iface;
    } enp_u;

    iow10	enp_iow;

    } ENPDEVICE_OLD;		/* cmc link-10 v3.x */

typedef struct
    {
    char enp_ram_rom[4 K];
    union {
	char	all_ram[RAM_SIZE];
	struct
	    {
	    USHORT	t_go;
	    USHORT	t_status;
	    UINT	t_pstart;
	    } t;
	struct
	    {
	    char	nram[RAM_SIZE - (HOST_RAMSIZE + ENP_RAMSIZE)];
	    char	hram[HOST_RAMSIZE];
	    char	kram[ENP_RAMSIZE];
	    } u_ram;
	struct
	    {
	    char	pad7[ 0x100 ];	/* starts 0x1100 - 0x2000 */
	    short	e_enpstate;
	    short	e_enpmode;
	    UINT	e_enpbase;
	    short	e_enprun;
	    short	e_intvector;

	    RING32	h_toenp;
	    RING32	h_hostfree;
	    RING32	e_tohost;
	    RING32 	e_enpfree;

	    RING32	e_rcvdma;
	    RING32	h_rcv_d;

	    ENPSTAT	e_stat;
	    ETHLIST	e_netaddr;
	    } iface;
	} enp_u;

    struct iow10	enp_iow;
    } ENPDEVICE;			/* cmc link-10 v4.0 */

#define	enp_ram		enp_u.all_ram
#define	enp_nram	enp_u.u_ram.nram
#define	enp_hram	enp_u.u_ram.hram
#define	enp_kram	enp_u.u_ram.kram
#define	enp_go		enp_u.t.t_go
#define	enp_status	enp_u.t.t_status
#define	enp_prog_start	enp_u.t.t_pstart
#define enp_state	enp_u.iface.e_enpstate
#define enp_mode	enp_u.iface.e_enpmode
#define enp_base	enp_u.iface.e_enpbase
#define enp_enprun	enp_u.iface.e_enprun
#define enp_intvector	enp_u.iface.e_intvector
#define enp_toenp	enp_u.iface.h_toenp
#define enp_hostfree	enp_u.iface.h_hostfree
#define enp_tohost	enp_u.iface.e_tohost
#define enp_enpfree	enp_u.iface.e_enpfree
#define enp_stat	enp_u.iface.e_stat
#define enp_addr	enp_u.iface.e_netaddr

#define ENPVAL		0xff	/* value to poke in enp_iow.hst2enp_interrupt */
#define RESETVAL	0x00	/* value to poke in enp_iow.enp2hst_clear_intr*/
#define GOVAL		0x8080	/* value to poke in enp_go */

#define INTR_ENP(addr)		addr->enp_iow.hst2enp_interrupt = ENPVAL
#define ACK_ENP_INTR(addr)
#define IS_ENP_INTR(addr)	1
#define RESET_ENP(addr)		addr->enp_iow.hst2enp_reset = 01

#define ENP_GO(addr,start)	{  \
				  int goval = 0x80800000; \
				  WRITELONG(&addr->enp_prog_start, start) \
				  WRITELONG(&addr->enp_go, goval) \
				}


/*
 * status bits - in t_status for 3.1 t_csr for 4.0
 */

#define STS_ONLINE	0x02	/* set when GO command received */
#define STS_READY	0x04	/* set when ready for GO command */
#define STS_NO_PROM	0x08	/* don't go to PROM address */
#define STS_INT_ENABLE	0x40	/* enable enp interrupts */

/*
 * state bits
 */

#define S_ENPRESET	01		/* enp is in reset state */
#define S_ENPRUN	02		/* enp is in run state */

/*
 * mode bits
 */

#define E_SWAP16		0x1		/* swap two octets within 16 */
#define E_SWAP32		0x2		/* swap 16s within 32 */
#define E_SWAPRD		0x4		/* swap on read */
#define E_SWAPWRT		0x8		/* swap on write */
#define E_DMA			0x10		/* enp does data moving */

#define E_EXAM_LIST		0x80000000	/* enp should examine addr list */


typedef struct BCB /* Data Buffer Structure */
    {
    struct BCB    *b_link;
    unsigned short b_stat;
    short          b_len;
    char          *b_addr;
    short          b_msglen;
    short          b_reserved;
    } BCB;

/*
 * transmit & receive status bits
 */

#define	BCB_DONE	0x8000	/* buffer complete */
#define	BCB_ERR		0x4000	/* error occurred */
#define	BCB_FILL	0x2000	/* TX: store chip address outgo */
#define	BCB_FRAME	0x2000	/* RX: framing error */
#define	BCB_MORE	0x1000	/* TX: more than one retry */
#define	BCB_OFLO	0x1000	/* RX: silo overflow */

#define	BCB_ONE		0x0800	/* TX: one retry required */
#define	BCB_CRC		0x0800	/* RX: CRC error */
#define	BCB_DEFER	0x0400	/* TX: cable busy */
#define	BCB_RXBUF	0x0400	/* RX: receive buffer error */
#define	BCB_STP		0x0200	/* start of packet */
#define	BCB_ENP		0x0100	/* end of packet */

#define	BCB_TXBUF	0x0080	/* TX: transmit buffer error */
#define	BCB_UFLO	0x0040	/* TX: transmit silo underflow */
#define	BCB_BABBLE	0x0020	/* TX: transmit babbling */
#define	BCB_LATCOL	0x0010	/* TX: late collision */

#define	BCB_CARRLOSS	0x0008	/* TX: carrier lost */
#define	BCB_RETRY	0x0004	/* TX: missed packet */
#define	BCB_MEMERR	0x0002	/* memory error */
#define	BCB_COLERR	0x0001	/* TX: collision error */
#define	BCB_MISSED	0x0001	/* RX: missed packet */

#if ((CPU_FAMILY==I960) && (defined __GNUC__))
#pragma	align	0		/* restore alignment setting */
#endif	/* CPU_FAMILY==I960 */

#ifdef __cplusplus
}
#endif

#endif /* __INCif_enph */
