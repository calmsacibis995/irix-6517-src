/*
 *
 *
 *
 */
#if IP22 || IP26 || IP28 		/* compile for IP2[268] */

#define	BEING_TESTED

#define	DBG_LVL		1	/* from tr_user.h for DP, DBG_LVL_DRV */
#define	DEBUG2			/* turn on/off more intensive debugging */
#define FMPLUS			/* use FastMac Plus, never FastMac! */
#undef	DWORD_DIO_ADDR		/* not using DWORD_DIO_ADDR in ftk_down.h. */
#undef	LOCAL_EISA_CLEANUP	/* no eisa_clean(). */

#ifdef	BEING_TESTED
#endif	/* BEING_TESTED */

#define	MAX_INTR_LOOP_CNT	512	/* max loop times for reading
					 * intr status. */
#define	MAX_QFULL_IN_ROW	1024	/* max consective tx queue full
					 * before shutting the adapter down. */
#define	MAX_WRONG_RX_LEN	64	/* max consective wrong rx len
					 * before shutting the adapter down. */
#define	MAX_MTR_INIT_LOOP_CNT	1024	/* max loop times for getting
					 * information for initialization. */

#ident "$Revision: 1.14 $; it is from $ Revision: 1.9 $"

#ifdef	IP28
	#ifdef	R10000_SPECULATION_WAR	
	char if_mtr_rcsinfo[] = 
	"@(#) if_mtr: $Revision: 1.14 $ (IP28 w R10000_SPECULATION_WAR & BEING_TESTED).";
	#else	/* R10000_SPECULATION_WAR */
	char if_mtr_rcsinfo[] = 
		"@(#) if_mtr: $Revision: 1.14 $ (IP28 _NO_ ...).";
	#endif	/* R10000_SPECULATION_WAR */
#else	
	char if_mtr_rcsinfo[] = 
		"@(#) if_mtr: $Revision: 1.14 $ (IP2[26] w BEING_TESTED).";
#endif

#include "sys/types.h"
#include "sys/pio.h"
#include "sys/eisa.h"
#include "ksys/xthread.h"
#include "sys/mc.h"
#include "sys/tcp-param.h"
#include "sys/sysmacros.h"
#include "sys/cmn_err.h"
#include "sys/debug.h"
#include "sys/cpu.h"
#include "sys/vmereg.h"
#include "sys/immu.h"
#include "sys/pda.h"
#include "sys/param.h"
#include "sys/systm.h"
#include "sys/mbuf.h"
#include "sys/socket.h"
#include "sys/errno.h"
#include "sys/edt.h"
#include "sys/kopt.h"
#include "sys/invent.h"
#include "sys/major.h"
#include "bstring.h"

#include "sys/time.h"
#include "sys/sbd.h"
#include "sys/stream.h"
#include "sys/strmp.h"
#include "sys/uio.h"

#include "net/soioctl.h"
#include "net/if.h"
#include "net/if_types.h"
#include "net/netisr.h"
#include "sys/par.h"
#include "net/raw.h"
#include "net/multi.h"
#include "net/route.h"

#include "netinet/in_systm.h"
#include "netinet/in.h"
#include "netinet/ip.h"
#undef _KERNEL
#include "netinet/in_var.h"
#define _KERNEL
#include "netinet/if_ether.h"
#include "netinet/ip_var.h"
#include "netinet/in_pcb.h"
#include "netinet/udp.h"
#include "netinet/tcp.h"
#include "netinet/udp_var.h"
#include "netinet/tcpip.h"

#include "sys/misc/ether.h"
#include "sys/tr.h"
#include "sys/llc.h"
#include "sys/fddi.h"
#include "sys/tms380.h"
#include "sys/tr_user.h"
#include "sys/if_gtr.h"
#include "sys/dlsap_register.h"
#ifdef	BEING_TESTED
#include "sys/atomic_ops.h"
#endif	/* BEING_TESTED */

#undef	ssb_addr		/* reset the one defined in ... */
#define	FAR			/* this for DOS so ... */

#define	MADGE_EISA_REQUIRED
#include "sys/if_mtr.h"

#define	FTK_223_FIRMWARE_OFFSET_IN_BYTE	(12*16+14)

/*	EISA address is 32 bit */
typedef	__uint32_t              eisa_addr_t;

#include "sys/mload.h"
char	*if_mtrmversion = M_VERSION;
int	if_mtrdevflag = 0;

/*
 *	DP() is used when ifnet is not ready or not accessible.
 *	DP is controlled by DBG_LVL which is enabled during compile time.
 *
 *	Otherwise, MTR_DBG_PRINT() is used because it is controlled
 *	by 'ifconfig mtr? debug'. That can be done during run time.
 */
#define MTR_DBG_PRINT(_mtr, _str)   				\
	if ((_mtr)->mif_flags & IFF_DEBUG) printf _str
#ifdef	DEBUG2
#define	DUMP_RX_TX_SLOTS(_ptr)	dump_rx_tx_slots( _ptr )

static void 	mtr_assert(char * filedt_p, int line);
#define	MTR_DBG_PRINT2(_str) printf _str
#define MTR_ASSERT( _COND )					\
	((_COND) ? ((void) 0) : mtr_assert(__FILE__, __LINE__))

#else	/* DEBUG2 */
#define	DUMP_RX_TX_SLOTS(_ptr)
#define MTR_ASSERT( _COND )
#define	MTR_DBG_PRINT2(_str)

#endif	/* DEBUG2 */

#define MTR_MAXBD		4
#define MTR_ID			0x02008734
#define MTR_IRQ_MASK		0x9e08 	/* irq 3, 9, 10, 11, 12, 15 */
static u_short mtr_irq_mask = MTR_IRQ_MASK;

#define MADG_IOPORT1		0x10
#define MADG_IOPORT2		0xc90	/* bmic reg */
#define MADG_IOPORT3		0xc91	/* bmic reg */

#define M1_INIT			0x80	/* rom disable, int level sensitive */

static u_char m1_speed[2] = {
	0x00,		/* 4Mbit */
	0x10		/* 16Mbit */
};

static u_char m1_media[2] = {
	0x00,		/* DB9 STP Connect */
	0x20		/* RJ45 UTP Connect */
};

static u_char m1_irq[16] = {	/* irq 3, 9, 10, 11, 12, 15 else nop */
	0, 0, 0, 1, 0, 0, 0, 0,
	0, 0, 2, 3, 6, 0, 0, 7
};

#define M2_INIT			0x0	/* int level sensitive */

static u_char m2_speed[2] = {
	0x80,		/* 4Mbit */
	0xa0		/* 16Mbit */
};

static u_char m2_irq[16] = {
	0x0, 0x0, 0x0, 0x3, 0x0, 0x0, 0x0, 0x0,
	0x0, 0x2, 0xa, 0xb, 0xc, 0x0, 0x0, 0xf
};

#define M3_INIT			0x80	/* rom disable, ibm compat disable */

static u_char m3_media[2] = {
	0x00,		/* DB9 STP Connect */
	0x20		/* RJ45 UTP Connect */
};

typedef struct arb_general_s {
	SRB_HEADER	header;
	ushort_t	reserved;
	ushort_t	status;
} arb_general_t;

#define	EAGLE_SRB_CMD_CODE		0xA0FF
#define	DIO_LOCATION_EAGLE_DATA_PAGE1	0x0001

#ifdef	SIOC_TR_GETIFSTATS
#else	/* SIOC_TR_GETIFSTATS */
#define	SIOC_TR_GETIFSTATS	_IOWR('S', 16, struct ifreq)
#endif	/* SIOC_TR_GETIFSTATS */

/*	For functional address.	*/
#define SIOC_MTR_GETFUNC        _IOWR('S', 17, struct ifreq)
#define SIOC_MTR_ADDFUNC        _IOWR('S', 18, struct ifreq)
#define SIOC_MTR_DELFUNC        _IOWR('S', 19, struct ifreq)

/*	Get the enabled group, multicast, address */
#define	SIOC_MTR_GETGROUP	_IOWR('S', 94, struct ifreq)

#define	EAGLE_ARB_FUNCTION_CODE	0x84	/* for arb_general_t.header.function */

#define	MTR_NAME	"mtr"
#define	MTR_KMEM_ARGS						\
	(KM_NOSLEEP | VM_DIRECT | KM_CACHEALIGN | KM_PHYSCONTIG)

/*
 * Token Ring internal data structure
 */
struct mtr_info;
typedef struct if_mtr_s {
	struct trif	trif;
	struct mtr_info	*mtr;
	uint_t		flags;
} if_mtr_t;

typedef struct mtr_info {
	if_mtr_t		*if_mtr_p;

	struct mtr_info		*next;		/* next one on the list */

    	ushort_t		*sif_dat;	/* SIF register	IO locations*/
    	ushort_t		*sif_datinc;
    	ushort_t		*sif_adr;
    	ushort_t		*sif_int;
    	ushort_t		*sif_acl;
    	ushort_t		*sif_adr2;
    	ushort_t		*sif_adx;
    	ushort_t		*sif_dmalen;
    	ushort_t		*sif_sdmadat;
    	ushort_t		*sif_sdmaadr;
    	ushort_t		*sif_sdmaadx;

	uchar_t			*fmplus_image;
	ushort_t		sizeof_fmplus_array;
	ushort_t		recorded_size_fmplus_array;
	uchar_t			fmplus_checksum;

	int			test_size;
	int			scb_size;
	caddr_t			scb_buff;
	eisa_addr_t		scb_phys;

	int			ssb_size;
	caddr_t			ssb_buff;
	eisa_addr_t		ssb_phys;

	int			mtr_s16Mb;	/* 16/4 Mb */

    	FASTMAC_STATUS_BLOCK	stb;		/* Status Block: STB.	*/
    	SRB_GENERAL		srb_general;	/* System Request Block: SRB */
    	SRB_GENERAL		srb_general2;	/* System Request Block: SRB */
    	ushort_t		size_of_srb;	/* size	of current SRB	    */
    	arb_general_t		arb;		/* Adapter Request Block */

    	INITIALIZATION_BLOCK 	*init_block;	/* ptr Fastmac init block   */
    	SRB_HEADER	 	*srb_dio_addr;	/* addr	of SRB in DIO space */
    	arb_general_t		*arb_dio_addr;
    	FASTMAC_STATUS_BLOCK 	*stb_dio_addr;	/* addr	of STB in DIO space */

    	uint_t			adapter_status;	/* prepared or running	 */
    	uint_t			srb_status;	/* free	or in use	 */

    	STATUS_INFORMATION * status_info;	/* ptr adapter status info  */

	/* slot DIO addrs */
    	RX_SLOT	 		*rx_slot_array[FMPLUS_MAX_RX_SLOTS];
    	TX_SLOT 		*tx_slot_array[FMPLUS_MAX_TX_SLOTS];

	int			all_rx_tx_bufs_size;	/* all in pages */
	int			rx_tx_buf_size;		/* each in bytes */
	caddr_t			rx_tx_bufs_addr;
    	caddr_t			slot_buf_start, slot_buf_end;
    	caddr_t			rx_slot_buf[FMPLUS_MAX_RX_SLOTS];
    	caddr_t			tx_slot_buf[FMPLUS_MAX_TX_SLOTS];

    	ushort_t		active_tx_slot;
    	ushort_t		active_rx_slot;

    	caddr_t	mio;	/* look for the board here */

    	GTR_CNTS	 	*stat;		/* status counters - 120 */

    	u_int			flags;		/* miscellaneous flags */
    	u_int			q_full_cnt;
	int			mtr_drain_seq;
	u_int			wrong_rx_len_cnt;

#define	MTR_STATE_MASK		0xff	/* those flags will be reset when
					 * adapter is restarted or halted. */
#define	MTR_STATE_UNINIT	0x00
#define	MTR_STATE_RDY_START	0x01	/* device is dormant state */
#define MTR_STATE_OK		0x02	/* device is xmitting & receiving */
#define	MTR_STATE_PROMISC	0x100	/* PROMISC mode */
    	u_int			bd_state;	/* useful state bits */

    	int			rcvpad;

    	u_char			mcl[10];	/* Micro Code Level */
    	u_char			pid[18];	/* Product ID */
    	u_char  		sid[4];		/* Station ID */

						/* broadcast addr */
	u_char			mtr_baddr[TR_MAC_ADDR_SIZE];	
    	u_short			nullsri;	/* default bcast sri */

	int			srb_used;	/* outstanding SRB cmds */
	mutex_t			srb_mutex;	/* protecting srb_used */

	uint_t			group_address;
	uint_t			new_group_addr;	/* working area */

	uint_t			functional_address;
	uint_t			new_functional_address;	/* working area */

	uint_t			ip_multicast_cnt;

	int			irq;

} mtr_info_t;
mtr_info_t		*mtr_info_link = NULL;
#define	mtr_tif		if_mtr_p->trif
#define mtr_ac		mtr_tif.tif_arpcom
#define mtr_bia		mtr_tif.tif_bia	/* permanent_address */
#define mtr_rawif	mtr_tif.tif_rawif
#define mif		mtr_ac.ac_if
#define mif_enaddr	mtr_ac.ac_enaddr
#define mif_unit	mif.if_unit
#define mif_mtu		mif.if_mtu
#define mif_flags	mif.if_flags
#define	mtr_info_2_ifnet(_mtr_p)	&((_mtr_p)->mif)
#define	ifnet_2_mtr_info(_ifp)		((if_mtr_t *)(_ifp))->mtr
#define	ifmtr_2_ifnet(_ifmtr_p)		((struct ifnet *)(_ifmtr_p))
#define	ifnet_2_ifmtr(_ifp)		((if_mtr_t *)(_ifp))

typedef struct tokenbufhead {
    struct ifheader tbh_ifh;		/* 8 bytes */
    struct snoopheader tbh_snoop;	/* 16 bytes */
} TOKENBUFHEAD;

/* overheads */
#define TRPAD		sizeof(TOKENBUFHEAD)
#define TR_RAW_MAC_SIZE sizeof(TR_MAC) + sizeof(TR_RII)

#define INB(x)          (*(volatile u_char *)(x))
#define INS_NSW(x)      (*(volatile u_short *)(x))
#define INW(x)          (*(volatile uint_t *)(x))
#define OUTB(x,y)       (*(volatile u_char *)(x) = (y))
#define OUTS(x,y)       (*(volatile u_short *)(x) = \
				((ushort_t)(y) >> 8) | (ushort_t)((y) << 8))
#define OUTS_NSW(x,y)   (*(volatile u_short *)(x) = (y))
#define OUTW_NSW(x,y)   (*(volatile u_int *)(x) = (y))

static ushort_t irq_mask = MTR_IRQ_MASK;

#define	MAX_RX_LENGTH(_mtr)				\
	((_mtr)->init_block->fastmac_parms.max_frame_size + 4)	/* CRC */
#define	MAX_TX_LENGTH(_mtr)				\
	(_mtr)->init_block->fastmac_parms.max_frame_size
#define	MTR_NO_ERROR			0


extern time_t lbolt;
extern struct ifnet loif;
extern struct ifqueue ipintrq;

extern u_int eisa_preempt, eisa_quiet;
extern ushort 	mtr_bc_mask;
extern int    	mtr_mtu;
extern int    	mtr_s16Mb;
extern int    	mtr_UTPmedia;
extern int    	mtr_batype;
extern uint_t	if_mtr_cnt;
extern int	mtr_participant_claim_token;
extern int	mtr_arb_enabled;
extern int	if_mtr_loaded;
extern int	mtr_max_rx_slots, mtr_max_tx_slots;
extern void	**if_mtr_sub_addrs;

static int 	mtr_prepare_adapter( mtr_info_t *mtr);
static int 	mtr_install_card(mtr_info_t *mtr, DOWNLOAD_RECORD *rec);
static void 	dump_rx_tx_slots(mtr_info_t *mtr);

static ushort_t read_rx_slot_len(mtr_info_t *);
static ushort_t read_rx_slot_status(mtr_info_t *);
static void	write_rx_slot_len(mtr_info_t *, ushort_t);
static void	mtr_get_open_status(mtr_info_t *);
static int	hwi_get_init_code(mtr_info_t *);
static void	hwi_copy_to_dio_space(mtr_info_t *, ushort_t, ushort_t, 
		uchar_t*, ushort_t);
static int	hwi_get_node_address_check(mtr_info_t *);
static int	hwi_get_bring_up_code(mtr_info_t *);
static void	hwi_start_eagle(mtr_info_t *);
static int	hwi_download_code(mtr_info_t *, DOWNLOAD_RECORD *);
static void	hwi_halt_eagle(mtr_info_t *);
static void	hwi_eisa_set_dio_address(mtr_info_t *, ushort_t, ushort_t);
static int	hwi_initialize_adapter(mtr_info_t *);
void		if_mtrintr(mtr_info_t *);
static int	mtrsend(mtr_info_t *, struct mbuf *);
static void	mtr_snoop_input(mtr_info_t *, struct mbuf *, int, int);
static void 	mtr_drain_input(mtr_info_t *mtr, struct mbuf *pkt_mh_p,
        	int pkt_len, int sri_len, u_int port);
static int 	mtr_issue_srb( mtr_info_t *mtr, SRB_GENERAL *srb_ptr);
static int 	mtr_group_address( mtr_info_t *, int , caddr_t );
static int 	mtr_read_error_log( mtr_info_t *mtr );
static int 	mtr_functional_address( mtr_info_t *, int , uint_t );
static int	mtr_reopen_adapter(mtr_info_t *, SRB_GENERAL *);
static int	mtr_modify_open_parms(mtr_info_t *);
static void 	report_Configuration( mtr_info_t *mtr);

static uchar_t	mtr_baddr_c[6] = {0xc0, 0x00, 0xff, 0xff, 0xff, 0xff};
static uchar_t	mtr_baddr_f[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

static char	scb_test_pattern[] = SCB_TEST_PATTERN_DATA;
static char	ssb_test_pattern[] = SSB_TEST_PATTERN_DATA;

static int 	mtr_af_sdl_output( struct ifnet *, struct mbuf *, 
			struct sockaddr *);
static int	mtr_output(struct ifnet *, struct mbuf *, 
		struct sockaddr *dst, struct rtentry *ptr);
static int	mtr_ioctl(struct ifnet *, int , caddr_t );
static void 	release_resources(mtr_info_t *, int );
int 		if_mtropen(dev_t *devp, int mode);
int 		if_mtrclose(dev_t dev);
int 		if_mtrwrite(dev_t dev, uio_t *uiop);
int 		if_mtrioctl(dev_t , int , int , int , struct cred *, int *);
void 		if_mtrinit(void);
void 		if_mtredtinit(struct edt *edtp);
void		eisa_refresh_off(void);
int 		if_mtrunload(void );
#ifdef	LOCAL_EISA_CLEANUP
static void	eisa_cleanup( mtr_info_t *);
#endif	/* LOCAL_EISA_CLEANUP */

static void 	*if_mtr_subs[] = {
	(void *) mtr_prepare_adapter,		/* 0 */
	(void *) read_rx_slot_len,
	(void *) read_rx_slot_status,
	(void *) write_rx_slot_len,
	(void *) NULL,				/* 4 */
	(void *) mtr_get_open_status,
	(void *) hwi_get_init_code,
	(void *) hwi_copy_to_dio_space,
	(void *) hwi_get_node_address_check,	/* 8 */
	(void *) hwi_get_bring_up_code,
	(void *) hwi_start_eagle,
	(void *) hwi_download_code,
	(void *) hwi_halt_eagle,		/* 12 */
	(void *) hwi_eisa_set_dio_address,
	(void *) hwi_initialize_adapter,
	(void *) if_mtrintr,
	(void *) mtrsend,
	(void *) mtr_snoop_input,		/* 16 */
	(void *) mtr_drain_input,
	(void *) mtr_issue_srb,
	(void *) mtr_group_address,
	(void *) mtr_read_error_log,		/* 20 */
	(void *) mtr_functional_address,
	(void *) NULL,
	(void *) if_mtrintr,
	(void *) mtr_af_sdl_output,		/* 24 */
	(void *) mtr_output,
	(void *) mtr_ioctl,
#ifdef	LOCAL_EISA_CLEANUP
	(void *) eisa_cleanup,
#else	/* LOCAL_EISA_CLEANUP */
	(void *) NULL,
#endif	/* LOCAL_EISA_CLEANUP */
	(void *) release_resources,		/* 28 */
	(void *) if_mtropen,
	(void *) if_mtrclose,
	(void *) if_mtrwrite,
	(void *) if_mtredtinit,			/* 32 */
	(void *) if_mtrunload,
	(void *) (__psunsigned_t)0xff00aa55,	/* delimiter */
	(void *) 0x0
};
static int	load_multiple_times = 0;

#ifdef	DEBUG2
static void
mtr_assert(char * filedt_p, int line)
{
        cmn_err_tag(242,CE_PANIC, "iee: ASSERT @ \"%s\".%d!", filedt_p, line);
        return;
} /* mtr_assert() */
#endif	/* DEBUG2 */

#undef	MAC_FRAME_SIZE
#undef	MAX_FRAME_SIZE_4_MBITS
#undef	MAX_FRAME_SIZE_16_MBITS
#define MAC_CRC_SIZE                    4
#define MIN_MAC_FRAME_SIZE              sizeof(TR_MAC) + MAC_CRC_SIZE
#define MAX_MAC_FRAME_SIZE              (MIN_MAC_FRAME_SIZE + sizeof(TR_RII))
#define MAX_FRAME_SIZE_4_MBITS          (TRMTU_4M +  MAX_MAC_FRAME_SIZE)
#define MAX_FRAME_SIZE_16_MBITS         (TRMTU_16M + MAX_MAC_FRAME_SIZE)

#define	CACHELINE_128_BYTE	20
#define	CACHELINE               (sizeof(__uint32_t) * CACHELINE_128_BYTE)
#define MAKE_ALIGNMENT( _size, _align )                                 \
        ( (((_size) & ((_align) - 1)) != 0) ?                   	\
          (((_size) + (_align)) & ~((_align) - 1)) :                    \
          (_size) )

/*	TO DO: better way to find out the permanent addresss in adapter.*/
NODE_ADDRESS my_permanent_address = {
	MADGE_NODE_BYTE_0, MADGE_NODE_BYTE_1, MADGE_NODE_BYTE_2, 
	0xff, 0xff, 0xff 
};

/*
 *	Prepare the information to initialize the adatper:
 *
 *	- get space for init parameters
 *	- prepare init paramters
 *	- determine mtu size
 *	- get space for dma testing
 *
 *	Similiar to driver_prepare_adapter() in PCI side.
 */
static int
mtr_prepare_adapter( mtr_info_t *mtr)
{
	INITIALIZATION_BLOCK	*init_block_p;
    	FASTMAC_INIT_PARMS 	*fastmac_parms;
    	RX_SLOT 		*next_rx_slot;
    	TX_SLOT 		*next_tx_slot;
    	RX_SLOT 		**rx_slot_array;
    	TX_SLOT 		**tx_slot_array;
    	int			slot_index;
    	char			*a;
	struct ifnet		*ifp = mtr_info_2_ifnet(mtr);

	short			fmplus_buffer_size;
	ushort_t		fmplus_max_buffers;
	uint_t			fmplus_max_buffmem;

	int			error = MTR_NO_ERROR;
	int			loop_cnt;

	MTR_DBG_PRINT2(("mtr%d: mtr_prepare_adapter().\n", mtr->mif_unit));

	hwi_halt_eagle(mtr);

	if( (init_block_p = mtr->init_block) == NULL ){
		init_block_p = mtr->init_block = (INITIALIZATION_BLOCK * )
			kmem_alloc(sizeof(INITIALIZATION_BLOCK), KM_NOSLEEP);
		if( init_block_p == NULL) {
			cmn_err_tag(243,CE_WARN,
				"mtr%d: ENOBUFS for init block memory.",
				mtr->mif_unit);
			error = ENOBUFS;
			goto done;
		}
	}
	bzero( init_block_p, sizeof(*init_block_p) );

	/*
	 *	Determine MTU.
	 */
	if( mtr->mif_mtu == 0 ){
		/* 	mif_mtu is defined here only when it
		 * 	has not been defined yet, such as during
		 *	initialization.
		 */
		mtr->mif_mtu = min(max(1, mtr_mtu),
			mtr->mtr_s16Mb ? TRMTU_16M : TRMTU_4M);
	}

	/*
	 *	Allocate the resource for SCB and SSB.
	 */
	if( mtr->scb_buff == NULL ){

		MTR_ASSERT( mtr->ssb_buff == NULL && mtr->scb_buff == NULL);
		MTR_ASSERT( mtr->ssb_size == 0 && mtr->scb_size == 0 );
		MTR_ASSERT( mtr->test_size == 0 );
		mtr->scb_size = 
			MAKE_ALIGNMENT(SCB_TEST_PATTERN_LENGTH, CACHELINE);
		mtr->ssb_size = 
			MAKE_ALIGNMENT(SSB_TEST_PATTERN_LENGTH, CACHELINE);
		mtr->test_size= mtr->scb_size + mtr->ssb_size;

		mtr->scb_buff = kmem_zalloc(mtr->test_size, 
			(KM_SLEEP | KM_PHYSCONTIG | KM_CACHEALIGN));
		if( mtr->scb_buff == NULL ){
			cmn_err(CE_WARN,
				"mtr%d: kmem_zalloc(%d) for SSB & SCB!",
				mtr->mif_unit, mtr->test_size);
			error = ENOBUFS;
			goto done;
		}
		mtr->scb_phys = kvtophys(mtr->scb_buff);

		mtr->ssb_buff = mtr->scb_buff + mtr->scb_size;
		mtr->ssb_phys = kvtophys(mtr->ssb_buff);	

		MTR_ASSERT( (__psunsigned_t) mtr->ssb_buff >
			(__psunsigned_t) mtr->scb_buff );
		MTR_ASSERT( (__psunsigned_t) mtr->ssb_phys > 
			(__psunsigned_t) mtr->scb_phys );
	
		/*	TO DO: do we really need this?	*/
    		dki_dcache_wbinval((void *)mtr->scb_buff, mtr->test_size);
	} else {
		
		MTR_ASSERT( mtr->ssb_buff != NULL && mtr->scb_buff != NULL );
		MTR_ASSERT( mtr->ssb_size != 0 && mtr->scb_size != 0 );
		MTR_ASSERT( mtr->test_size != 0 );
		MTR_ASSERT( mtr->scb_phys != NULL && mtr->ssb_phys != NULL );
	}

    	/* 	Set up the TI initialization parameters.	
	 *	Those fields not set up	here must be zero.
     	 */
    	init_block_p->ti_parms.init_options = TI_INIT_OPTIONS_BURST_DMA;
	init_block_p->ti_parms.scb_addr	    = mtr->scb_phys;
	init_block_p->ti_parms.ssb_addr	    = mtr->ssb_phys;
    	init_block_p->ti_parms.madge_mc32_config = 0;
    	init_block_p->ti_parms.parity_retry = TI_INIT_RETRY_DEFAULT;
    	init_block_p->ti_parms.dma_retry    = 	TI_INIT_RETRY_DEFAULT;

    	/* 	Set up header to identify the smart software 
	 *	initialization parms.
     	 */
    	init_block_p->smart_parms.header.length   = sizeof(SMART_INIT_PARMS);
    	init_block_p->smart_parms.header.signature= SMART_INIT_HEADER_SIGNATURE;
    	init_block_p->smart_parms.header.version  = SMART_INIT_HEADER_VERSION;
    	bcopy(&my_permanent_address, 
		&init_block_p->smart_parms.permanent_address,
		sizeof(init_block_p->smart_parms.permanent_address));

    	init_block_p->smart_parms.min_buffer_ram = SMART_INIT_MIN_RAM_DEFAULT;

	fastmac_parms = &init_block_p->fastmac_parms;
	fastmac_parms->header.length    = sizeof(FASTMAC_INIT_PARMS);
	fastmac_parms->header.signature = FMPLUS_INIT_HEADER_SIGNATURE;
	fastmac_parms->header.version   = FMPLUS_INIT_HEADER_VERSION;
	fastmac_parms->int_flags 	|= INT_FLAG_RX;
	fastmac_parms->int_flags 	|= INT_FLAG_LARGE_DMA;
	if( mtr_arb_enabled )
		fastmac_parms->int_flags |= INT_FLAG_RING_STATUS_ARB;
	fastmac_parms->rx_slots = mtr_max_rx_slots;
	fastmac_parms->tx_slots = mtr_max_tx_slots;
	fastmac_parms->max_frame_size = ifp->if_mtu + MAX_MAC_FRAME_SIZE;
	init_block_p->smart_parms.sif_burst_size = (ushort_t)0;
	fastmac_parms->feature_flags    = FEATURE_FLAG_AUTO_OPEN;
        fastmac_parms->open_options     = 
		(mtr_participant_claim_token ? OPEN_OPT_CONTENDER : 0);
	if( mtr->bd_state & MTR_STATE_PROMISC ) {
		fastmac_parms->open_options	|=
			(OPEN_OPT_COPY_ALL_MACS | OPEN_OPT_COPY_ALL_LLCS);
	}
	fastmac_parms->group_address    = 0;
	fastmac_parms->functional_address = 0;
	fastmac_parms->group_root_address = 0x00;
	fastmac_parms->group_root_address = 0xc000;

    	/*
   	 * Now work	out the	number of transmit and receive buffers.
	 * The number of buffers allocated depends on the maximum frame size
	 * anticipated, and on the amount of memory on the card. Unfortunately,
	 * one cannot know the maximum possible frame size until after the mac
	 * code has started and worked out what the ring speed is. Thus it is
	 * necessary to make an assumption about what the largest frame size
	 * supported will be.
	 */
    	fmplus_buffer_size = max(FMPLUS_MIN_TXRX_BUFF_SIZE,
		FMPLUS_DEFAULT_BUFF_SIZE_SMALL);
	MTR_ASSERT( fmplus_buffer_size >= FMPLUS_MIN_TXRX_BUFF_SIZE );
    	init_block_p->smart_parms.rx_tx_buffer_size = fmplus_buffer_size;

	/*
	 *	TO DO: Find out adapter's RAM size at the run-time.
	 */
    	fmplus_max_buffmem = FMPLUS_MAX_BUFFMEM_IN_256K;
    	fmplus_max_buffers = 
        	(ushort_t)(fmplus_max_buffmem / ((fmplus_buffer_size + 1031) & 
			~1023)) - 5;

    	/*
    	 * Finally, allocate the buffers between transmit and receive. Notice
    	 * that we allow here for frames as big as they are ever going to get
    	 * on tokenring. For smaller frames and 4 Mbps rings, this will just
    	 * have the effect of improving back-to-back transmit performance.
    	 */

    	fastmac_parms->tx_bufs = 2 * ((fastmac_parms->max_frame_size + 
        	fmplus_buffer_size) / fmplus_buffer_size);
    	fastmac_parms->rx_bufs = fmplus_max_buffers - 
        	fastmac_parms->tx_slots - fastmac_parms->tx_bufs;
    	if (fastmac_parms->rx_bufs < fastmac_parms->tx_bufs) {
		cmn_err(CE_WARN, 
			"mtr%d: fastmac_parms->rx_bufs < "
			"fastmac_parms->tx_bufs.",
		 	mtr->mif_unit);
        	goto done;
    	}

done:
	if( error == MTR_NO_ERROR ){
    		ifp->if_flags |= (IFF_UP | IFF_RUNNING);
		mtr->bd_state &= ~(MTR_STATE_MASK);
    		mtr->bd_state |= MTR_STATE_OK;
	}
	MTR_DBG_PRINT2(("mtr%d: mtr_prepare_adapter(): %s[%d].\n", 
		mtr->mif_unit, 
		(error == MTR_NO_ERROR ? "successfully done" : "failed"), 
		error));
    	return(error);
} /* mtr_prepare_adapter() */

/* 
 *	Start the adapter.
 *
 *	call mtr_install_card(),
 *	call hwi_initialize_adapter(),
 *	setup rx & tx slots,
 *	open the adapter,
 *	start the RX operation.
 *
 *	Similiar to driver_start_adapter() on PCI side.
 */
static int
mtr_start_adapter(mtr_info_t *mtr)
{
	int			error = MTR_NO_ERROR;
	RX_SLOT			*next_rx_slot, **rx_slot_array;
	TX_SLOT			*next_tx_slot, **tx_slot_array;
	INITIALIZATION_BLOCK	*init_block_p;
	FASTMAC_INIT_PARMS	*fastmac_parms;
	int			slot_index, loop_cnt;
	int			alloc_required;
	int			idx, new_size, pages;
	caddr_t			mem;
	eisa_addr_t		buf_physaddr;
	__psunsigned_t		tmp;
	struct ifnet		*ifp = mtr_info_2_ifnet(mtr);
	
	MTR_DBG_PRINT2(("mtr%d: mtr_start_adapter().\n", mtr->mif_unit));

	init_block_p  = mtr->init_block;

	/*	Download the firmware.	*/
	if( (error = mtr_install_card(mtr, (DOWNLOAD_RECORD *)
		(mtr->fmplus_image + FTK_223_FIRMWARE_OFFSET_IN_BYTE))) 
		!= MTR_NO_ERROR){
		cmn_err(CE_WARN, "mtr%d: mtr_install_card() failed: %d.",
			mtr->mif_unit, error);
		goto done;
	}

	if( (error = hwi_initialize_adapter( mtr )) != MTR_NO_ERROR){
		cmn_err(CE_WARN, "mtr%d: hwi_initialize_adapter() failed.",
			mtr->mif_unit);
		goto done;
	}

	/*
	 *	Set up the slots!
	 */
    
	/*
    	 * Get the DIO addresses of the Fastmac SSB and STB (ststus block).
    	 * Only use non-extended SIF regs (EAGLE_SIFADR and EAGLE_SIFDAT_INC).
    	 */

        OUTS_NSW(mtr->sif_adr, DIO_LOCATION_SRB_POINTER);
	tmp = (__psunsigned_t)(INS_NSW(mtr->sif_datinc) & 0xffff);
        mtr->srb_dio_addr = (SRB_HEADER * ) tmp;

        OUTS_NSW(mtr->sif_adr, DIO_LOCATION_ARB_POINTER);
        tmp = (__psunsigned_t)(INS_NSW(mtr->sif_datinc) & 0xffff);
        mtr->arb_dio_addr = (arb_general_t *) tmp;

        OUTS_NSW(mtr->sif_adr, DIO_LOCATION_STB_POINTER);
        tmp = (__psunsigned_t)(INS_NSW(mtr->sif_datinc) & 0xffff);
        mtr->stb_dio_addr = (FASTMAC_STATUS_BLOCK * ) tmp;
	
	/*
    	 * 	Now recover ADAPTER's receive slot and transmit slot chains.
    	 */
        rx_slot_array = mtr->rx_slot_array;
        tx_slot_array = mtr->tx_slot_array;
	fastmac_parms = &mtr->init_block->fastmac_parms; 

    	/*
   	 * Start with the receive slot chain. We must poll the location in the
    	 * status block that holds the start address until it is non-zero.  It
    	 * is then safe to run down the chain finding the other slots.
    	 */

        OUTS_NSW(mtr->sif_adr, (ushort_t) (__psunsigned_t)
                &mtr->stb_dio_addr->rx_slot_start);
        loop_cnt = 0;
        do {	
		tmp = (__psunsigned_t) (INS_NSW(mtr->sif_dat) & 0xffff);
                rx_slot_array[0] = (RX_SLOT * ) tmp;
        } while (rx_slot_array[0] == 0 && ++loop_cnt < MAX_MTR_INIT_LOOP_CNT);
        if( rx_slot_array[0] == 0 ){
                cmn_err(CE_WARN, "mtr%d: rx_slot_array[0]:%d %d.",
                        mtr->mif_unit, rx_slot_array[0], loop_cnt);
                error = EIO;
                goto done;
        }
        slot_index = 0;
        do {
                OUTS_NSW(mtr->sif_adr, (ushort_t) (__psunsigned_t)
                        &rx_slot_array[slot_index]->next_slot);
                tmp = (__psunsigned_t) (INS_NSW(mtr->sif_dat) & 0xffff);
                next_rx_slot = (RX_SLOT  * ) tmp;
                if (next_rx_slot != rx_slot_array[0]) {
                        rx_slot_array[++slot_index]      = next_rx_slot;
                }
        } while (next_rx_slot != rx_slot_array[0] &&
                 slot_index < fastmac_parms->rx_slots);
        if( slot_index != (fastmac_parms->rx_slots -1) ){
                cmn_err(CE_WARN,
                        "mtr%d: RX slot_index: %d != %d "
                        "failed to init adapter",
                        mtr->mif_unit, slot_index,
                        (fastmac_parms->rx_slots-1));
                error = EIO;
                goto done;
        } /* if slot_index */

	/*
    	 * Now do the same for the transmit slots.
    	 */
        OUTS_NSW(mtr->sif_adr, (ushort_t) (__psunsigned_t)
                &mtr->stb_dio_addr->tx_slot_start);
        loop_cnt = 0;
        do {
                tmp = (__psunsigned_t) (INS_NSW(mtr->sif_dat) & 0xffff);
                tx_slot_array[0] = (TX_SLOT * ) tmp;
        } while (tx_slot_array[0] == 0 && ++loop_cnt < MAX_MTR_INIT_LOOP_CNT);
        if( tx_slot_array[0] == 0 ){
                cmn_err(CE_WARN, "mtr%d: tx_slot_array[0]:%d %d.",
                        mtr->mif_unit, tx_slot_array[0], loop_cnt);
                error = EIO;
                goto done;
        }
        slot_index = 0;
        do {
                OUTS_NSW(mtr->sif_adr, (ushort_t) (__psunsigned_t)
                        &tx_slot_array[slot_index]->next_slot);
                tmp = (__psunsigned_t) (INS_NSW(mtr->sif_dat) & 0xffff);
                next_tx_slot = (TX_SLOT  * ) tmp;
                if (next_tx_slot != tx_slot_array[0]) {
                        tx_slot_array[++slot_index]      = next_tx_slot;
                }
        } while (next_tx_slot != tx_slot_array[0] &&
                 slot_index < fastmac_parms->tx_slots);
        if( slot_index != (fastmac_parms->tx_slots -1) ){
                cmn_err(CE_WARN,
                        "mtr%d: TX slot_index: %d != %d "
                        "failed to init adapter",
                        mtr->mif_unit, slot_index,
                        (fastmac_parms->tx_slots-1));
        } /* if slot_index */

	/*
	 *	As the mtu can be changed, when the adapter is restarted,
	 *	we better verify the size to make sure the buffer is 
	 *	good enough.
	 */
	mtr->rx_tx_buf_size=MAKE_ALIGNMENT(fastmac_parms->max_frame_size, 1024);
	new_size = mtr->rx_tx_buf_size *
		(fastmac_parms->rx_slots + fastmac_parms->tx_slots);
	pages = (new_size + NBPP) / NBPP;
	new_size = pages * NBPP;
	alloc_required = 1;
	if( mtr->all_rx_tx_bufs_size != 0 || mtr->rx_tx_bufs_addr != NULL ){
		if( mtr->all_rx_tx_bufs_size == new_size ){	
			alloc_required = 0;	/* same so no need. */
		} else {
			/*	different size!	*/
			kvpfree( mtr->rx_tx_bufs_addr,
				(mtr->all_rx_tx_bufs_size / NBPP));
			mtr->rx_tx_bufs_addr = NULL;
			mtr->all_rx_tx_bufs_size = 0;	

			/* 	TO DO: release dma map!	*/

		} /* if all_rx_tx_bufs_size */
	} /* if ... */

	if( alloc_required ){
		mem = (caddr_t)kvpalloc( pages, VM_NOSLEEP|VM_PHYSCONTIG, 0);
		if( mem == NULL ){
			cmn_err(CE_ALERT,
				"mtr%d: failed to allocate memory for TX & "
				"RX: kvpalloc(%d)!",
				mtr->mif_unit, pages);
			error = ENOBUFS;
			goto done;
		}

		/*	TO DO: allocate dma map */

	} else {
		mem = mtr->rx_tx_bufs_addr;
	}
	dki_dcache_inval(mem, mtr->all_rx_tx_bufs_size * NBPP);

	mtr->all_rx_tx_bufs_size= new_size;
	mtr->rx_tx_bufs_addr	= mem;

	/*	Configure the adapter with the address of 
	 *	host's memory buffer. */
	for( idx = 0; idx < fastmac_parms->rx_slots; idx++){
		mtr->rx_slot_buf[idx] = (caddr_t)mem;
		mem += mtr->rx_tx_buf_size;
		buf_physaddr = (eisa_addr_t)kvtophys(mtr->rx_slot_buf[idx]);
		OUTS_NSW(mtr->sif_adr, (ushort_t)(__psunsigned_t)
			& mtr->rx_slot_array[idx]->buffer_hiw);
		OUTS_NSW(mtr->sif_datinc, (ushort_t)(buf_physaddr >> 16));
		OUTS_NSW(mtr->sif_dat, (ushort_t)(buf_physaddr & 0xFFFF));
	}
        mtr->active_rx_slot = 0;

	for( idx = 0; idx < fastmac_parms->tx_slots; idx++){
		mtr->tx_slot_buf[idx] = (caddr_t)mem;
		mem += mtr->rx_tx_buf_size;
		buf_physaddr = (eisa_addr_t)kvtophys(mtr->tx_slot_buf[idx]);
		OUTS_NSW(mtr->sif_adr, (ushort_t)(__psunsigned_t)
			& mtr->tx_slot_array[idx]->large_buffer_hiw);
		OUTS_NSW(mtr->sif_datinc, (ushort_t)(buf_physaddr >> 16));
		OUTS_NSW(mtr->sif_dat, (ushort_t)(buf_physaddr & 0xFFFF));
	}
        mtr->active_tx_slot = 0;

	DUMP_RX_TX_SLOTS(mtr);

        /*
         * Check that Fastmac has correctly installed. Do this by reading
         * node address from Fastmac status block. If routine fails return
         * failure (error record already filled in). Note for EISA cards,
         * this is actually     first time get node address.
         */

        if ( (error = hwi_get_node_address_check(mtr)) != MTR_NO_ERROR) {
                cmn_err(CE_WARN,
                        "mtr%d: hwi_get_node_address_check failed.",
                        mtr->mif_unit);
                goto done;
        }

        if( showconfig ) {
		report_Configuration( mtr );
        }

        mtr_get_open_status(mtr);
        if( !mtr->stb.adapter_open  &&
                mtr->stb.open_error != EAGLE_OPEN_ERROR_SUCCESS ) {
                cmn_err(CE_WARN, "mtr%d: open error (0x%x.%x).",
                        mtr->mif_unit,
                        mtr->stb.adapter_open,
                        mtr->stb.open_error);
        }

	/*	Start the RX operation!	*/
        OUTS_NSW(mtr->sif_adr, (ushort_t) (__psunsigned_t)
                &mtr->stb_dio_addr->rx_slot_start);
        OUTS_NSW(mtr->sif_dat, 0);

done:
	if( error == MTR_NO_ERROR ){
		ifp->if_flags |= (IFF_UP | IFF_RUNNING);
		mtr->bd_state &= ~(MTR_STATE_MASK);
		mtr->bd_state |= MTR_STATE_OK;
	}
	MTR_DBG_PRINT2(("mtr%d: mtr_start_adapter(): %s[%d].\n", 
		mtr->mif_unit, 
		(error == MTR_NO_ERROR ? "successfully done" : "failed"), 
		error));
	return (error );
} /* mtr_start_adapter() */

/*
 *	Install the adapter:
 *
 *	download the firmare,
 *	start the eagle(firmware),
 *	verify BUD is successfully done,
 *	set up the DIO address to the right place.
 *	setup nullsri for the adapter.
 */
static int
mtr_install_card(mtr_info_t *mtr, DOWNLOAD_RECORD *rec)
{
        int             error = 0;

        MTR_DBG_PRINT2(("mtr%d: mtr_install_card().\n", mtr->mif_unit));
        MTR_ASSERT( mtr->sif_dat != 0 );

        hwi_halt_eagle(mtr);

        mtr->wrong_rx_len_cnt   = 0;
        mtr->q_full_cnt         = 0;

        if( (hwi_download_code(mtr, rec)) != MTR_NO_ERROR )goto done;

        hwi_start_eagle(mtr);

        error = hwi_get_bring_up_code(mtr);
        if ( error != 0 )goto done;

        /* Set DIO address to point to EAGLE DATA page 0x10000L.
         */
        hwi_eisa_set_dio_address(mtr,
                (DIO_LOCATION_EAGLE_DATA_PAGE & 0xffff),
                ((DIO_LOCATION_EAGLE_DATA_PAGE >> 16) & 0xffff) );

        if (mtr_bc_mask == 0)
                mtr->nullsri = (TR_RIF_ALL_ROUTES | 0x200);
        else
                mtr->nullsri = (mtr_bc_mask | 0x200);

        if (mtr->mif_mtu >= 17800)
                mtr->nullsri |= TR_RIF_LF_17800;
        else if (mtr->mif_mtu >= 11407)
                mtr->nullsri |= TR_RIF_LF_11407;
        else if (mtr->mif_mtu >= 8144)
                mtr->nullsri |= TR_RIF_LF_8144;
        else if (mtr->mif_mtu >= 4472)
                mtr->nullsri |= TR_RIF_LF_4472;
        else if (mtr->mif_mtu >= 2052)
                mtr->nullsri |= TR_RIF_LF_2052;
        else if (mtr->mif_mtu >= 1470)
                mtr->nullsri |= TR_RIF_LF_1470;
        else if (mtr->mif_mtu >= 516)
                mtr->nullsri |= TR_RIF_LF_516;
        else
                mtr->nullsri |= TR_RIF_LF_INITIAL;

	if( (mtr->mtr_baddr[0] | mtr->mtr_baddr[1] |
	     mtr->mtr_baddr[2] | mtr->mtr_baddr[3] |
	     mtr->mtr_baddr[4] | mtr->mtr_baddr[5]) == 0){
		if( mtr_batype == 1 ){
			bcopy(mtr_baddr_c, mtr->mtr_baddr,
				sizeof(mtr->mtr_baddr));
		} else {
			bcopy(mtr_baddr_f, mtr->mtr_baddr,
				sizeof(mtr->mtr_baddr));
		}
	}

done:
	mtr->bd_state &= ~(MTR_STATE_MASK);
	if( error == MTR_NO_ERROR ) {
		mtr->bd_state |= MTR_STATE_RDY_START;
	} else {
		mtr->bd_state |= MTR_STATE_UNINIT;
	}

	MTR_DBG_PRINT2(("mtr%d: mtr_install_card(): %s[%d].\n",
		mtr->mif_unit, 
		(error == MTR_NO_ERROR ? "successfully done" : "failed"), 
		error));

	return( error );
} /* mtr_install_card */

/*
 *      Initialize the adapter.
 *      mtr_install_card() needs to called first. 
 *      In that case, init_block is reset to zero.
 */
static int	
hwi_initialize_adapter(mtr_info_t *mtr)
{
	int			error;
    	uint_t		   	init_success;
    	UINT		   	idx;
    	ushort_t	   	fmplus_buffer_size;
    	DWORD			fmplus_max_buffmem;
    	ushort_t		fmplus_max_buffers;
    	INITIALIZATION_BLOCK 	*init_block = mtr->init_block;
    	FASTMAC_INIT_PARMS	*fastmac_parms = &init_block->fastmac_parms;
    	struct ifnet 		*ifp = &mtr->mif;

	MTR_DBG_PRINT2(("mtr%d: hwi_initialize_adapter().\n",
		mtr->mif_unit));

	MTR_ASSERT( mtr->scb_size >= SCB_TEST_PATTERN_LENGTH );
	MTR_ASSERT( mtr->ssb_size >= SSB_TEST_PATTERN_LENGTH );
	MTR_ASSERT( mtr->test_size!= 0);
	MTR_ASSERT( mtr->scb_buff != NULL && mtr->ssb_buff != NULL );
	MTR_ASSERT( mtr->scb_phys != NULL && mtr->ssb_phys != NULL );

	/*	Initialize the memory for DMA testing.	*/
	bzero(mtr->scb_buff, mtr->test_size);
    	dki_dcache_wbinval((void *)mtr->scb_buff, mtr->test_size);

#if defined(IP28) || defined(IP22)
	{
		int		cnt;
		ushort_t	*ptr;

		MTR_DBG_PRINT2(("init_block.ti_parms(%d): 0x", 
			sizeof(init_block->ti_parms)));
		ptr = (ushort_t *)&(init_block->ti_parms);
		for(cnt = 0; cnt < sizeof(init_block->ti_parms); 
		    cnt += sizeof(*ptr), ptr++){
			if( cnt != 0 && !(cnt & 0xf) ) 
				MTR_DBG_PRINT2(("\n\t0x"));
			MTR_DBG_PRINT2(("%x.", *ptr));
		} /* for */

		MTR_DBG_PRINT2(("\ninit_block.smart_parms(%d): 0x",
			sizeof(init_block->smart_parms)));
		ptr = (ushort_t *)&(init_block->smart_parms);
		for(cnt = 0; cnt < sizeof(init_block->smart_parms);
		    cnt += sizeof(*ptr), ptr++){
			if( cnt != 0 && !(cnt & 0xf) )
				MTR_DBG_PRINT2(("\n\t0x"));
			MTR_DBG_PRINT2(("%x.", *ptr));
		}
				
		MTR_DBG_PRINT2(("\ninit_block.fastmac_parms(%d): 0x",
			sizeof(init_block->fastmac_parms)));
		ptr = (ushort_t *)&(init_block->fastmac_parms);
		for(cnt = 0; cnt < sizeof(init_block->fastmac_parms);
		    cnt += sizeof(*ptr), ptr++){
			if( cnt != 0 && !(cnt & 0xf) )
				MTR_DBG_PRINT2(("\n\t0x"));
			MTR_DBG_PRINT2(("%x.", *ptr));
		}

		MTR_DBG_PRINT2(("\n"));
	}
#endif	/* IP22 || IP28 */
    	/* 	Download initialization block to required location in DIO space.
    	 * 	Note routine leaves 0x0001 in EAGLE SIFADRX register.
     	 */
    	hwi_copy_to_dio_space(mtr, 
		(DIO_LOCATION_INIT_BLOCK & 0xffff), 
		(DIO_LOCATION_INIT_BLOCK >> 16) & 0xffff, 
		(uchar_t *) init_block,
		(ushort_t) sizeof(*init_block) );


    	/* 
	 *	Start initialization by output 0x9080 to SIF interrupt register.
     	 */
    	OUTS_NSW(mtr->sif_int, EAGLE_INIT_START_CODE);

    	/*
    	 * Wait for	a valid	initialization code, may wait 11 seconds.
    	 * During this process test	DMAs need to occur, hence in PIO mode needs
    	 * calls to	hwi_interrupt_entry, hence need	interrupts or polling active.
    	 */
    	if( (error = hwi_get_init_code(mtr)) != MTR_NO_ERROR ){
	    	cmn_err(CE_WARN, "mtr%d: hwi_get_init_code() failed[%d].\n",
			mtr->mif_unit, error);
		goto done;
    	}

   	/*
    	 * Check that test DMAs were successful.
    	 */

    	/*
   	 * First check SCB for correct test	pattern. Remember used Fastmac
    	 * transmit	buffer address for SCB address.
    	 */

#ifdef	R10000_SPECULATION_WAR
    	/* ensure data not cached */
	dki_dcache_inval((void *)mtr->scb_buff, mtr->test_size);
#endif	/* R10000_SPECULATION_WAR */

    	for (idx = 0; idx < SCB_TEST_PATTERN_LENGTH; idx++) {
		if (mtr->scb_buff[idx] != scb_test_pattern[idx]) {
	    		cmn_err(CE_WARN, 
				"mtr%d: mtr->scb_buff[%d]: 0x%x != "
				"scb_test_pattern[%d]: 0x%x.",  mtr->mif_unit,
				idx, mtr->scb_buff[idx],
				idx, scb_test_pattern[idx]);
			error = EIO;
		}
    	}

    	/*
    	 * Now check SSB for correct test pattern. Remember	used Fastmac
    	 * receive buffer address for SSB address.
    	 */
    	for (idx = 0; idx < SSB_TEST_PATTERN_LENGTH; idx++) {
		if (mtr->ssb_buff[idx] != ssb_test_pattern[idx]) {
	    		cmn_err(CE_WARN, 
				"mtr%d: mtr->ssb_buff[%x]: 0x%x != "
				"ssb_test_pattern[%d]: 0x%x.",  mtr->mif_unit,
				idx, mtr->ssb_buff[idx], 
				idx, ssb_test_pattern[idx]);
			error = EIO;
		}
    	}

done:
	MTR_DBG_PRINT2(("mtr%d: hwi_initialize_adapter(): %s[%d].\n",
		mtr->mif_unit, 
		(error == MTR_NO_ERROR ? "successfully done" : "failed"), 
		error));
    	return error;
} /* hwi_initialize_adapter() */

#ifdef	DEBUG2
static void
dump_rx_tx_slots(mtr_info_t *mtr)
{
	int 		i, max;
	ushort_t	rx_len, tx_len[2];

	max = (mtr->init_block->fastmac_parms.tx_slots > 
	       mtr->init_block->fastmac_parms.rx_slots) ? 
	      mtr->init_block->fastmac_parms.tx_slots :
	      mtr->init_block->fastmac_parms.rx_slots;
	printf("mtr%d: dump_rx_tx_slots: active_[rt]x_slot: %d, %d.\n",
		mtr->mif_unit,
		mtr->active_rx_slot, mtr->active_tx_slot);
	for( i = 0; i < max; i++) {
    		ushort_t 		slot	 	= i;
    		RX_SLOT 	**rx_slot_array = mtr->rx_slot_array;
    		TX_SLOT 	**tx_slot_array = mtr->tx_slot_array;

		if( i < mtr->init_block->fastmac_parms.rx_slots ){
    			OUTS_NSW(mtr->sif_adr, (ushort_t)(__psunsigned_t) 
				& rx_slot_array[slot]->buffer_len);
    			rx_len = (INS_NSW(mtr->sif_dat));
		} else {
			rx_len = (ushort_t) -1;
		}
		
		if( i < mtr->init_block->fastmac_parms.tx_slots ){
    			OUTS_NSW(mtr->sif_adr, (ushort_t)(__psunsigned_t) 
				& tx_slot_array[slot]->small_buffer_len);
    			tx_len[0] = INS_NSW(mtr->sif_dat);

    			OUTS_NSW(mtr->sif_adr, (ushort_t)(__psunsigned_t) 
				& tx_slot_array[slot]->large_buffer_len);
    			tx_len[1] = INS_NSW(mtr->sif_dat);
		} else {
			tx_len[0] = tx_len[1] = (ushort_t)-1;
		}

		printf("mtr%d: dump_rx_tx_slots: "
			"rx[%d.%d]: 0x%x, tx[%d.%d]: 0x%x.%x.\n",
			mtr->mif_unit, 
			i, mtr->init_block->fastmac_parms.rx_slots, rx_len, 
			i, mtr->init_block->fastmac_parms.tx_slots, 
			tx_len[0], tx_len[1]);
	} /* for */

	return;
} /* dump_rx_tx_slots() */
#endif	/* DEBUG2 */

/*
 * read frame off the board.
 */
#define DRAINFRM	0
#define IPFRM		1
#define LLCFRM		2

void	
if_mtrintr(mtr_info_t *mtr)
{
    int	i;
    TR_MAC * mh;
    LLC1 * llcp;
    TR_RII * rii;
    u_short * sri;
    int	rilen = 0;
    register char	*cp, *dp;
    register int	hlen, frm, ft;
    u_short	port = 0;
    struct ifqueue *ifq = 0;
    struct mbuf *m0, *mt, *m;
    int	length, blen, buflen;
    int	len = 0;
    int snoopflag = 0;
    caddr_t rxp;
    SNAP *snap;

    ushort_t	   sifint_value;
    ushort_t	   sifint_tmp;

    ushort_t poll_count = 0;
    caddr_t rx_frame_addr;
    	caddr_t *rx_slot_cache_virt;
    int	header_len;

	int 	loop_cnt;


	IFNET_LOCK( mtr_info_2_ifnet(mtr) );
	rx_slot_cache_virt = mtr->rx_slot_buf;

    if (((sifint_value = INS_NSW(mtr->sif_int)) & FASTMAC_SIFINT_IRQ_DRIVER) !=
        0) {

	/* This could be an SRB free, an adapter check or a recvd frame	int
	 * WARNING: Do NOT reorder the clearing	of the SIFINT register with
	 * the reading of it. If SIFINT is cleared after reading it, any
	 * interrupts raised after reading it will be lost.
	 */

	OUTS_NSW(mtr->sif_int, 0);
	sifint_tmp = INS_NSW(mtr->sif_int);

	/* Record the EAGLE SIF	interrupt register value.
	 * WARNING: Continue to	read the SIFINT	register until it is stable
	 *   because of	a potential problem involving the host reading	the
	 *   register after the	adapter	has written the	low byte of it,	but
	 *   before it has written the high byte. Failure to wait  for	the
	 *   SIFINT register to	settle can cause spurious interrupts.
	 */

	sifint_value = INS_NSW(mtr->sif_int);
	loop_cnt = 0;
	do {
	    sifint_tmp = sifint_value;
	    sifint_value = INS_NSW(mtr->sif_int);
	} while (sifint_tmp != sifint_value && loop_cnt++ < MAX_INTR_LOOP_CNT);
	if( sifint_tmp != sifint_value ){
		cmn_err(CE_WARN,
			"mtr%d: if_mtrintr() loop_cnt: %d, 0x%x != 0x%x!",
			mtr->mif_unit, loop_cnt, sifint_tmp, sifint_value);
		printf("mtr%d: if_mtrintr() loop_cnt: %d.\n", mtr->mif_unit);
		hwi_halt_eagle(mtr);
		goto done;
	}

	sifint_value = (sifint_value & 0x00FF) ^ (sifint_value >> 8);
	sifint_value = sifint_value & 0x000F;

	if ( sifint_value & FASTMAC_SIFINT_ADAPTER_CHECK ) {

	    /* XXX do something? */
	    	cmn_err(CE_WARN, "mtr%d: Adapter Check!",  mtr->mif_unit);
		hwi_halt_eagle(mtr);
	    	goto done;

	} else if ( sifint_value & FASTMAC_SIFINT_SRB_FREE ) {
		int		cnt;
		ushort_t	*ptr = (ushort_t *)&(mtr->srb_general2);

		/*	
		 *	SRB free interrupt: adapter done 
		 *	the request.
		 */
		if( mtr->srb_used ){

			MTR_ASSERT( mtr->srb_used == 1 );
			hwi_eisa_set_dio_address(mtr,
				((__psunsigned_t)mtr->srb_dio_addr & 0xffff),
				(DIO_LOCATION_EAGLE_DATA_PAGE >> 16) & 0xffff);
			for( cnt = 0; cnt < sizeof(mtr->srb_general2); 
				cnt += sizeof(*ptr), ptr++ ){
				*ptr = INS_NSW(mtr->sif_datinc);
			} /* for cnt */

			MTR_DBG_PRINT(mtr, ("mtr%d: SIFINT_SRB "
				"(0x%04x): 0x%02x.%02x -> 0x%02x.%02x.\n",
				mtr->mif_unit, sifint_value,
				mtr->srb_general.header.function,
				mtr->srb_general.header.return_code,
				mtr->srb_general2.header.function,
				mtr->srb_general2.header.return_code));

			switch( mtr->srb_general2.header.function ){
			case SET_FUNCTIONAL_ADDRESS_SRB:
			case SET_GROUP_ADDRESS_SRB:
			case READ_ERROR_LOG_SRB:
#ifdef	DEBUG2
				break;
			case CLOSE_ADAPTER_SRB:
				MTR_DBG_PRINT2(( "mtr%d: "
					"SIFINT_SRB[CLOSE_ADAPTER_SRB].\n",
					mtr->mif_unit));
				break;
			case OPEN_ADAPTER_SRB:
				MTR_DBG_PRINT2(( "mtr%d: "
					"SIFINT_SRB[OPEN_ADAPTER_SRB].\n",
					mtr->mif_unit));
				break;
			case MODIFY_OPEN_PARMS_SRB:
				MTR_DBG_PRINT2(( "mtr%d: "
					"SIFINT_SRB[MODIFY_OPEN_PARMS_SRB].\n",
					mtr->mif_unit));
				break;
#else	/* DEBUG2 */
			case CLOSE_ADAPTER_SRB:
			case OPEN_ADAPTER_SRB:
			case MODIFY_OPEN_PARMS_SRB:
				break;
#endif	/* DEBUG2 */
			default:
				MTR_ASSERT(0);
				break;
			}

			wakeup( &mtr->srb_used );
		} else {
		
			/*
			 *	Locker could be gone because of signal.
			 */
			cmn_err(CE_DEBUG, "mtr%d: 0x%x(SIFINT_SRB) but %d!",
				mtr->mif_unit, 
				mtr->srb_general2.header.function,
				mtr->srb_used);
		}

	} else if( sifint_value & FASTMAC_SIFINT_ARB_COMMAND ){
		int		cnt;
		ushort_t	*ptr = (ushort_t *)&mtr->arb;

    		hwi_eisa_set_dio_address(mtr, 
			((__psunsigned_t)mtr->arb_dio_addr & 0xffff),
			((DIO_LOCATION_EAGLE_DATA_PAGE >> 16) & 0xffff) );
		for( cnt = 0; cnt < sizeof(arb_general_t);
			cnt += sizeof(*ptr), ptr++ ){
			*ptr = INS_NSW(mtr->sif_datinc);
		} /* for */
		if( mtr->arb.header.function != EAGLE_ARB_FUNCTION_CODE ){
			cmn_err(CE_NOTE, "mtr%d: SIFINT_ARB: 0x%x!?",
				mtr->mif_unit, mtr->arb.header.function);
		}

		mtr_get_open_status(mtr);

		if( mtr->arb.status != 0 ||
		    (mtr->stb.adapter_open == 0 && mtr->stb.open_error) ){
			cmn_err(CE_NOTE,
				"mtr%d: SIFINT_ARB "
				"(0x%04x): arb: 0x%04x open: 0x%04x.%04x",
				mtr->mif_unit, sifint_value,
				mtr->arb.status,
				mtr->stb.adapter_open, mtr->stb.open_error);
			if( mtr->arb.status & 0x40 ){
				cmn_err(CE_WARN, 
					"mtr%d: single station in the ring: "
					"0x%x!", mtr->mif_unit, 
					mtr->arb.status);
			}
		} /* if mtr->arb.status */
	} /* if sifint_value */

	/*
	 * Convert from	FASTMAC_SIFINT interrupt into DRIVER_SIFINT_ACK
	 * to acknowledge interrupt.
	 */

	sifint_value = (sifint_value << 	8);
	sifint_value = (sifint_value | DRIVER_SIFINT_IRQ_FASTMAC);
	sifint_value = (sifint_value | DRIVER_SIFINT_FASTMAC_IRQ_MASK);
	OUTS_NSW(mtr->sif_int, sifint_value);
    }

    if (iff_dead(mtr->mif.if_flags)) {
	if( mtr->mif_flags & IFF_DEBUG)
		cmn_err(CE_DEBUG, "mtr%d: interrupt during down.", 
			mtr->mif_unit);
	goto done;
    }

    while (length = read_rx_slot_len(mtr)) {
	ushort_t	rx_status;

#ifdef	R10000_SPECULATION_WAR	
	__dcache_inval(rx_slot_cache_virt[mtr->active_rx_slot], MAX_RX_LENGTH(mtr)+128);	/* ensure data not cached */
#endif	/* R10000_SPECULATION_WAR */

	if( length > MAX_RX_LENGTH(mtr) ){
		int	level = CE_NOTE;

		mtr->mif.if_ierrors++;
		if( ++mtr->wrong_rx_len_cnt > MAX_WRONG_RX_LEN)level = CE_WARN;
		cmn_err(level, "mtr%d: RX pkt: %d:%d > %d!",
			mtr->mif_unit, mtr->active_rx_slot, length,  
			MAX_RX_LENGTH(mtr));
		if( level == CE_WARN )hwi_halt_eagle(mtr);
		DUMP_RX_TX_SLOTS(mtr);
		goto nextframe;
	}
	mtr->wrong_rx_len_cnt = 0;

	if (((rx_status = read_rx_slot_status(mtr)) & GOOD_RX_FRAME_MASK) 
		!= 0) {
	    	cmn_err(CE_NOTE, "mtr%d: read bad frame: 0x%x.",  
			mtr->mif_unit, rx_status);
		mtr->mif.if_ierrors++;
	    	goto nextframe;
	}

	rxp = rx_frame_addr = rx_slot_cache_virt[mtr->active_rx_slot];

	/* Count packet/bytes regardless of error */
	mtr->mif.if_ipackets++;
	mtr->mif.if_ibytes += length;
	length -= 4;				/* excluding MAC's CRC */

	if( mtr->mif_flags & IFF_DEBUG){
		cmn_err(CE_DEBUG, "mtr%d: RX: %d, %d.\n",
			mtr->mif_unit, mtr->mif.if_ipackets, length);
	} /* IFF_DEBUG */

	/* get hdr ptrs. SRI PAD must be applied uncoditionally */
	mh = (TR_MAC * )rxp;

	/* purge my own frame */
	if ((mh->mac_sa[5] == mtr->mif_enaddr[5]) && 
	    (mh->mac_sa[4] == mtr->mif_enaddr[4]) && 
	    (mh->mac_sa[3] == mtr->mif_enaddr[3]) && 
	    (mh->mac_sa[2] == mtr->mif_enaddr[2]) && 
	    (mh->mac_sa[1] == mtr->mif_enaddr[1]) && 
	    ((mh->mac_sa[0] & ~TR_MACADR_RII) == mtr->mif_enaddr[0])) {
	    goto read_ret;
	}

	rii = (TR_RII * )(((char * )mh) + sizeof(*mh));
	if (mh->mac_sa[0] & TR_MACADR_RII) {
	    	sri = (u_short * )rii;
	    	rilen = (*sri & TR_RIF_LENGTH) >> 8;
	    	if (rilen <= 2) {
			sri = 0;
	    	}
	    	llcp = (LLC1 * )(((char * )rii) + rilen);
	} else {
	    	sri = 0;
	    	rilen = 0;
	    	llcp = (LLC1 * )rii;
	}

	/* Quick parse for frame type */
	if ((mh->mac_fc & TR_FC_TYPE) == TR_FC_TYPE_LLC) {
	    	if (llcp->llc1_dsap == TR_IP_SAP) {

			/*	LLC frame with SNAP */
			hlen 	= (sizeof(*mh) + rilen + sizeof(LLC1)
		    	   	+ sizeof(SNAP));
			snap 	= (SNAP * )
				(((char * )mh) + hlen - sizeof(SNAP));
			port 	= (snap->snap_etype[0] << 8) + 
				(snap->snap_etype[1]);
			frm 	= IPFRM;
	    	} else {

			/*	LLC frame but no SNAP */
			hlen 	= sizeof(*mh) + rilen;
			port 	= TR_PORT_LLC;
			frm 	= LLCFRM;
	    	}
	} else {

		/*	MAC frame, not LLC frame.	*/
		uchar_t	pcf_attn;

	    	hlen 	= sizeof(*mh) + rilen;
	    	port 	= TR_PORT_MAC;
	    	frm 	= DRAINFRM;
	
		pcf_attn = mh->mac_fc & TR_FC_PCF;
		/*      Is it a MAC frame with PCF attention code */
		switch( pcf_attn ){
		case TR_FC_PCF_BCN:
			cmn_err(CE_NOTE, "mtr%d: received beacon frame: 0x%x!",
				mtr->mif_unit, pcf_attn);
			break;

		case TR_FC_PCF_CLM:
			cmn_err(CE_NOTE, "mtr%d: received claim token: 0x%x!",
				mtr->mif_unit, pcf_attn);
			break;

		case TR_FC_PCF_PURGE:
			cmn_err(CE_NOTE, "mtr%d: received ring purge: 0x%x!",
				mtr->mif_unit, pcf_attn);
			break;
	
		case TR_FC_PCF_AMP:
		case TR_FC_PCF_SMP:
		case TR_FC_PCF_EXP: /* also TR_FC_PCF_REMOV, TR_FC_PCF_DUPA */
		default:
			break;
	
		} /* switch */

	}

	header_len = mtr->rcvpad + hlen;
	if( header_len & (sizeof(__psunsigned_t) - 1)){
		header_len += sizeof(__psunsigned_t);
		header_len &= ~(sizeof(__psunsigned_t) -1);
	}
	/*	As length includes header's length and we have
	 *	alredy counted it in header_len, we exclude it from buflen.
	 */
	buflen	= (length - hlen) + header_len;
	blen	= buflen > MCLBYTES ? MCLBYTES : buflen;
	mt = m0 = m_vget(M_DONTWAIT, blen, MT_DATA);
	if( m0 == NULL ){
		cmn_err(CE_NOTE, "mtr%d: RX: hdr out of mbufs.", mtr->mif_unit);
		dki_dcache_inval(rx_frame_addr, MAX_RX_LENGTH(mtr)+128);
		mtr->mif.if_ierrors++;
		goto cleanup_done;
	}
	m0->m_next = NULL;
	m0->m_len  = blen;
	bzero(mtod(m0, caddr_t), m0->m_len);

	/*	Copy MAC header, including those RI, LLC, SNAP, if any.	*/
	bcopy(rxp, mtod(m0, char *) + mtr->rcvpad, hlen);
	rxp	+= hlen;
	buflen	=  length - hlen;	/* buflen: length of data in MAC info part */

	/*	Copy MAC info part.			*/
	if( buflen < 0 ){
		printf("rxp: 0x%x, m0: 0x%x, header_len: %d, length: %d, hlen: %d.\n",
			rxp, m0, header_len, length, hlen);
		cmn_err(CE_PANIC, "if_mtrintr() buflen < 0");
	}
	if( buflen != 0 ){
		bcopy(rxp, mtod(m0, char *) + header_len, (blen - header_len));
		rxp	+= (blen - header_len);
		buflen	-= (blen - header_len);
	}

	IF_INITHEADER(mtod(m0, char * ), &mtr->mif, header_len);

	while (buflen > 0) {
	    
	    	blen = buflen > MCLBYTES ? MCLBYTES : buflen;

	    	m = m_vget(M_DONTWAIT, blen, MT_DATA);
	    	if (m == 0) {
			cmn_err(CE_NOTE, 
				"mtr%d: RX: out of mbufs.", mtr->mif_unit);
			dki_dcache_inval(rx_frame_addr, MAX_RX_LENGTH(mtr)+128);
			m_freem(m0);
	
			mtr->mif.if_ierrors++;
			goto cleanup_done;
	    	}

	    	bcopy(rxp, mtod(m, char * ), blen);
	
	    	rxp += blen;
	    	buflen -= blen;

	    	mt->m_next = m;
	    	mt = m;
	}

	if( RAWIF_SNOOPING( &(mtr->mtr_rawif) ) ){
		mtr_snoop_input(mtr, m0, rilen, length);
	}

	/* check mcast/bcast */
	if (TOKEN_ISGROUP(mh->mac_da)) {
    		mtr->mif.if_imcasts++;
    		if (TOKEN_ISBROAD(mh->mac_da))
			m0->m_flags |= M_BCAST;
    		else {
			m0->m_flags |= M_MCAST;
    		}
	} else {
		 /*	When promis mode is enabled we can 
		  *	receive the packets which are not detinated
		  *	to us.
		  */
		if( (mtr->bd_state & MTR_STATE_PROMISC) &&
                    ((mh->mac_da[5] != mtr->mif_enaddr[5]) ||
                     (mh->mac_da[4] != mtr->mif_enaddr[4]) ||
                     (mh->mac_da[3] != mtr->mif_enaddr[3]) ||
                     (mh->mac_da[2] != mtr->mif_enaddr[2]) ||
                     (mh->mac_da[1] != mtr->mif_enaddr[1]) ||
                     (mh->mac_da[0] != mtr->mif_enaddr[0])) ){
                        m_freem(m0);
                        mh = NULL;
                        goto read_ret;
                }
	} 

	/* Final frame type adjustment */
	if (frm == LLCFRM) {
		dlsap_family_t 	*dlp;

#define	DUMP_MTR_PKT_SIZE	128

		dlp = dlsap_find(llcp->llc1_dsap, DL_LLC_ENCAP);

		MTR_DBG_PRINT( mtr, 
			("mtr%d: LLC packet encountered. dlp: 0x%x.\n",
			mtr->mif_unit, dlp));

		if( dlp ){

			/*	The convention setup by other tokenring
			 *	driver requires the frame in the following
			 *	format.
			 *
			 *	TRPAD, padding, MAC, RI, LLC data.
			 *
			 */
			int	roff = header_len - (mtr->rcvpad) - hlen;
			uchar_t	shift_buf[64];

			if( roff != 0 ){
				bcopy( mtod(m0, char *) + mtr->rcvpad, 
					shift_buf, hlen);
				bcopy( shift_buf, mtod(m0, char *) + 
					mtr->rcvpad + roff, hlen);
			}
			mh = (TR_MAC *)(mtod(m0, char *) + mtr->rcvpad + roff);

	    		IF_INITHEADER(mtod(m0, char * ), 
				&mtr->mif, mtr->rcvpad + roff);
		
			if ((*dlp->dl_infunc)(dlp, &mtr->mif, m0, mh)) {
				m0 = NULL;/* someone free the mbuf, not me. */
				goto llc_done;
			}

			if( roff != 0 ){
				bcopy( shift_buf, mtod(m0, char *) + 
					mtr->rcvpad, hlen);
			}

			mh = (TR_MAC *)(mtod(m0, char *) + mtr->rcvpad);
			IF_INITHEADER(mtod(m0, char * ),
				 &mtr->mif, header_len);
		}

		/*	TO DO: drain(7p)	*/
	    	if (RAWIF_DRAINING(&mtr->mtr_rawif)) {
			mtr_drain_input(mtr, m0, length, rilen, port);
			m0 = NULL;
	    	}
	    	mtr->mif.if_noproto++;
		
llc_done:
		if (m0) m_freem(m0);
		goto read_ret;
	
	} 

	switch (port) {
	case ETHERTYPE_IP:

	    	ifq = &ipintrq;
	    	if (IF_QFULL(ifq)) {
			IF_DROP(ifq);
			mtr->mif.if_iqdrops++;
	    	} else {
	    		IF_ENQUEUE(ifq, m0);
	    		m0 = NULL;	
	    		schednetisr(NETISR_IP);
	    	}
		break;

	case ETHERTYPE_ARP:
	    	if (sri) {
			*sri &= ~TR_RIF_SRB_NBR;
			*sri ^= TR_RIF_DIRECTION;
	    	}
	    	srarpinput(&mtr->mtr_ac, m0, sri);
		m0 = NULL;
		break;

	default:
	    	if (RAWIF_DRAINING(&mtr->mtr_rawif)) {
			mtr_drain_input(mtr, m0, length, rilen, port);
			m0 = NULL;
	    	}
	    	mtr->mif.if_noproto++;
	}

	if (m0) m_freem(m0);

read_ret:
	dki_dcache_inval(rx_frame_addr, MAX_RX_LENGTH(mtr)+128);

nextframe:
	write_rx_slot_len(mtr, 0);

	/* Update the slot counter ready for the next one.		    */
	if (++mtr->active_rx_slot == mtr->init_block->fastmac_parms.rx_slots)
	    mtr->active_rx_slot = 0;

    } /* while read_rx_slot_len() */

done:
	IFNET_UNLOCK( mtr_info_2_ifnet(mtr) );
    return;

cleanup_done:
    	write_rx_slot_len(mtr, 0);
	/* Update the slot counter ready for the next one.	    */
	if (++mtr->active_rx_slot == mtr->init_block->fastmac_parms.rx_slots)
	   	mtr->active_rx_slot = 0;
	goto done;
}


static ushort_t
read_rx_slot_len(mtr_info_t *mtr)
{
    	ushort_t 	active_rx_slot	= mtr->active_rx_slot;
    	RX_SLOT **rx_slot_array = mtr->rx_slot_array;

    	ushort_t	data;
    	ushort_t	flag = 0;

again:
    	OUTS_NSW(mtr->sif_adr, (ushort_t)(__psunsigned_t) 
		& rx_slot_array[active_rx_slot]->buffer_len);
    	data = (INS_NSW(mtr->sif_dat));
    	if( data > MAX_RX_LENGTH(mtr) ){
		cmn_err(CE_NOTE, "mtr%d: read_rx_slot_len: %d.0x%x: %d > %d!",
			mtr->mif_unit, flag, 
			(ushort_t)(__psunsigned_t) 
				&rx_slot_array[active_rx_slot]->buffer_len,
			data, MAX_RX_LENGTH(mtr));
		if( flag++ == 0 )goto again;
    	} else if( flag ){
		cmn_err(CE_NOTE, "mtr%d: read_rx_slot_len: %d: %d.",
			mtr->mif_unit, flag, data);
    	}
    return(data);
}


static ushort_t
read_rx_slot_status(mtr_info_t *mtr)
{
    ushort_t active_rx_slot	 = mtr->active_rx_slot;
    RX_SLOT * *	rx_slot_array = 	mtr->rx_slot_array;

    ushort_t	data;
    OUTS_NSW(mtr->sif_adr, (ushort_t)(__psunsigned_t) 
	& rx_slot_array[active_rx_slot]->rx_status);
    data = INS_NSW(mtr->sif_dat);
    return( data );
}


static void	
write_rx_slot_len(mtr_info_t *mtr, ushort_t len)
{
    ushort_t active_rx_slot	 = mtr->active_rx_slot;
    RX_SLOT * *	rx_slot_array = 	mtr->rx_slot_array;

    OUTS_NSW(mtr->sif_adr, (ushort_t)(__psunsigned_t) 
	& rx_slot_array[active_rx_slot]->buffer_len);
    OUTS_NSW(mtr->sif_dat, len);
    return;
}

static u_int oldpreempt, oldquiet;

static int
mtr_af_sdl_output( struct ifnet *ifp, struct mbuf *m0, struct sockaddr *dst)
{
	int			error = MTR_NO_ERROR;
	mtr_info_t		*mtr;
	struct sockaddr_sdl 	*sdl;
	uchar_t			*map;
	TR_MAC			*mac_p;
	struct mbuf		*m_tmp = NULL;
	uchar_t			mac_addr[TR_MAC_ADDR_SIZE];
	ushort_t		*sri;
	int			sri_len;

	mtr	= ifnet_2_mtr_info(ifp);
	sdl     = (struct sockaddr_sdl *)dst;
	map	= (uchar_t *)sdl->ssdl_addr;
	sri	= (ushort *)&(map[TR_MAC_ADDR_SIZE]);
	sri_len = sdl->ssdl_addr_len - TR_MAC_ADDR_SIZE;

	if( sdl->ssdl_addr_len < TR_MAC_ADDR_SIZE){
		cmn_err(CE_DEBUG, "mtr%d: mtr_af_sdl_output(): "
			"sa_family(0x%x: AF_SDL) "
			"but len: %d(!=%d)!\n",
			mtr->mif_unit, dst->sa_family,
			sdl->ssdl_addr_len, TR_MAC_ADDR_SIZE);
		error = EAFNOSUPPORT;
		goto done;
	}

	if( (sri_len > 0) &&
	    (!(mtr->mif_flags & IFF_ALLCAST) ||
	     sri_len > TR_MAC_RIF_SIZE ||
	     sri_len != (((*sri)&TR_RIF_LENGTH) >> 8)) ) {

		/*	RI is provided but ... */
		cmn_err(CE_DEBUG,
			"mtr%d: mtr_af_sdl_output(): AF_SDL && SRI: "
			"sri_len, %d, mtr_if_flags: 0x%x!\n",
			mtr->mif_unit,
			sri_len,
			mtr->mif_flags);
		error = EAFNOSUPPORT;
		goto done;
	} else if( TOKEN_ISBROAD(map) ){
		map 	= mtr->mtr_baddr;
		sri	= &mtr->nullsri;
		sri_len	= sizeof(mtr->nullsri);
	}

	if( (m_tmp = m_get(M_DONTWAIT, MT_DATA)) == 0 ){
		error = ENOBUFS;		
		cmn_err(CE_DEBUG, "mtr%d: mtr_af_sdl_output(): "
			"AF_SDL: mget()", mtr->mif_unit);
		goto done;
	}

        m_tmp->m_len    = sizeof(*mac_p) + sri_len + TRPAD;
        MTR_ASSERT( m_tmp->m_len <= MLEN );
        m_tmp->m_next   = m0;
        m0		= m_tmp;

        m_adj(m_tmp, TRPAD);

        /*      Build MAC header.       */
        mac_p = mtod(m_tmp, TR_MAC *);
        mac_p->mac_ac = TR_AC_FRAME;
        mac_p->mac_fc = TR_FC_TYPE_LLC;
        bcopy(mtr->mif_enaddr, mac_p->mac_sa, TR_MAC_ADDR_SIZE);
        bcopy(map, mac_p->mac_da, TR_MAC_ADDR_SIZE);

        /*      Build SRI, if any.      */
        if( sri_len > 0 ){
                uchar_t *tsri = ((uchar_t *)mac_p) + sizeof(*mac_p);

                bcopy( (uchar_t *)sri, tsri, sri_len);
                mac_p->mac_sa[0] |=  TR_MACADR_RII;
        }

        if( RAWIF_SNOOPING( &(mtr->mtr_rawif) ) ){
                int     pkt_len = m_length(m0);

                MTR_ASSERT( m0->m_len == (sizeof(TR_MAC) + sri_len) );
                m0->m_off   -= TRPAD;
                m0->m_len   += TRPAD;
                IF_INITHEADER(mtod(m0, caddr_t), ifp, m0->m_len);
                mtr_snoop_input(mtr, m0, sri_len, pkt_len);
                m_adj(m0, TRPAD);
        }

	error = mtrsend(mtr, m0);

done:
	/*	
	 *	mtr_output() frees the packets, here only the
	 *	allocated mbuf is freed.
	 */
	if( m_tmp != NULL ){
		m0 = m_free(m_tmp);
	}
	MTR_ASSERT( m0 != NULL && m0->m_type != MT_FREE );
	return(error);
} /* mtr_af_sdl_output() */

extern int looutput(struct ifnet *, struct mbuf *, struct sockaddr *);
/*
 * Token-Ring output.
 */
static int	
mtr_output(struct ifnet *ifp, struct mbuf *m0, struct sockaddr *dst, 
	struct rtentry *rt_p)
{
    	TR_MAC 	*mh;
    	LLC1 	*llcp;
    	SNAP 	*snap;
    	int	s, etyp;
	int 	pkt_len;
    	uchar_t	*map;
    	uchar_t	maddr[TR_MAC_ADDR_SIZE];

    	int	hdr_len;
    	int	error = 0;
    	int	sri_len = 0;
    	u_short * sri = 0;

    	struct mbuf *mloop = 0;
	struct in_addr *idst;
    	mtr_info_t 	*mtr;
    	struct mbuf 	*m1;

    	/* 1: check and get info struct */
    	MTR_ASSERT((ifp->if_unit >= 0) && (ifp->if_unit < MTR_MAXBD));
	mtr = ifnet_2_mtr_info(ifp);

    	/* 2: make sure board has been initialized properly */
    	if ( (mtr->bd_state & MTR_STATE_MASK) != MTR_STATE_OK || 
	 	iff_dead(ifp->if_flags)) {
		MTR_DBG_PRINT(mtr, 
			("mtr%d: xmit: board not operational\n", 
			mtr->mif_unit));
		error = ENETDOWN; 
		goto output_ret;
    	}

	pkt_len = m_length(m0);
    	switch (dst->sa_family) {
    	case AF_INET: 
	    	idst = &((struct sockaddr_in *)dst)->sin_addr;

	    	if (!ip_arpresolve(&mtr->mtr_ac, m0, idst, &maddr[0])) {
			MTR_ASSERT( mloop == NULL );
			MTR_ASSERT( error == MTR_NO_ERROR );
			goto output_ret_without_freeing_mbuf;
		}

	    	/* At this point, ARP resolved,
		 * which means, SRI is also learned
		 * whether it is over bridge or not.
		 */
	    	if (TOKEN_ISBROAD(maddr)) {
			if ((mloop = m_copy(m0, 0, M_COPYALL)) == 0) {
		    		error = ENOBUFS; 
		    		goto output_ret;
			}
			map = mtr->mtr_baddr;
	    	} else {
			map = maddr;
	    	}
	    	etyp = ETHERTYPE_IP;

	    	/* prepare SRCROUTE for resolved IP packets */
	    	if ((mtr->mif.if_flags & IFF_ALLCAST) == 0) {
			sri_len = 0;
			break;
	    	}
	    	if (TOKEN_ISGROUP(map)) {
			/* This is IP broadcat */
			sri = &mtr->nullsri;
			sri_len = 2;
	    	} else if ((sri = srilook((char *)map, 1)) != 0) {
			/* This IP unicast for remote ring */
			sri_len = ((*sri) & TR_RIF_LENGTH) >> 8;
	    	} else {
			/* This is IP unicast for local ring */
			sri_len = 0;
	    	}
		break;

    	case AF_UNSPEC:

# define EP ((struct ether_header *)&dst->sa_data[0])
		etyp = EP->ether_type; 
		map  = &EP->ether_dhost[0];
		if (TOKEN_ISBROAD(map)) {
	    		map = mtr->mtr_baddr;
		}
# undef EP
		/* prepare SRCROUTE for ARP packets */
		if ((mtr->mif.if_flags & IFF_ALLCAST) == 0) {
	    		sri_len = 0;
	    		break;
		}

		if (TOKEN_ISGROUP(map)) {

	    		/* This is ARP request */
	    		sri = &mtr->nullsri;
	    		sri_len = 2;
		} else if ((sri = srilook((char *)map, 1)) != 0) {

	    		/* This ARP response to remote ring */
	    		sri_len = ((*sri) & TR_RIF_LENGTH) >> 8;
		} else {
	
	    		/* This is ARP response to local ring */
	    		sri_len = 0;
		}
		break;

    	case AF_RAW:
		error = mtrsend(mtr, m0);

		goto output_ret;

	case AF_SDL:
		error = mtr_af_sdl_output( ifp, m0, dst);
		goto output_ret;

    	default:
		MTR_DBG_PRINT(mtr, 
			("mtr%d: can not handle address family 0x%x\n",
	    		mtr->mif_unit, dst->sa_family));
		error = EAFNOSUPPORT; 
		goto output_ret;
    	}

    	/* 3: prepare for SRCROUTE is done already */

    	/* 4: Allocate MAC/SRI/LLC/SNAP header space */
    	/*    For simplicity just allocate new mbuf for header */
    	if ((m1 = m_get(M_DONTWAIT, MT_DATA)) == 0) {
		error = ENOBUFS; 
		goto output_ret;
    	}
	hdr_len	   = (sizeof(*mh)+sizeof(*llcp)+sizeof(*snap)) + sri_len;
	pkt_len	   += hdr_len;

    	m1->m_next = m0;
	/* have TRPAD in mbuf for snooping */
    	m1->m_len  = hdr_len + mtr->rcvpad;
	/* but skip the TRPAD part to set up ral header */
	m_adj(m1, mtr->rcvpad);		
    	m0 = m1;

    	/* 5: setup MAC header */
    	mh 		= mtod(m0, TR_MAC * );
    	mh->mac_ac 	= TR_AC_FRAME;
    	mh->mac_fc 	= TR_FC_TYPE_LLC;
    	bcopy(map, mh->mac_da, TR_MAC_ADDR_SIZE);
    	bcopy(mtr->mif_enaddr, mh->mac_sa, TR_MAC_ADDR_SIZE);

    	/* 6: setup SRCROUTE info */
    	if (sri_len > 0) {
		char	*tsri = ((char*)mh) + sizeof(*mh);
		bcopy((char * )sri, tsri, sri_len);
		mh->mac_sa[0] |= TR_MACADR_RII;
    	}

    	/* 7: setup LLC header */
	llcp = (LLC1 * )(((char * )mh) + sizeof(*mh) + sri_len);
	llcp->llc1_dsap = TR_IP_SAP;
	llcp->llc1_ssap = TR_IP_SAP;
	llcp->llc1_cont = RFC1042_CONT;

    	/* 8: setup SNAP */
	snap = (SNAP * )(((char * )llcp) + sizeof(*llcp));
    	snap->snap_org[0] = RFC1042_K2;
    	snap->snap_org[1] = RFC1042_K2;
    	snap->snap_org[2] = RFC1042_K2;
    	snap->snap_etype[0] = etyp >> 8;
    	snap->snap_etype[1] = etyp & 0xff;

    	/* 9: send out packet */
    	if (mloop) {
		ifp->if_omcasts++;
		looutput(&loif, mloop, dst);
		mloop = 0;
    	} else if( TOKEN_ISGROUP(map) ){
		ifp->if_omcasts++;
	}

	if( RAWIF_SNOOPING( &(mtr->mtr_rawif) ) ){
		if( mtr->rcvpad != TRPAD ){
			cmn_err(CE_NOTE, "mtr%d: snoop: %d != %d!\n",
				mtr->mif_unit, mtr->rcvpad, TRPAD );
		} else {
			/*	Make our snoop rtn happy.	*/
			m0->m_off	-= mtr->rcvpad;
			m0->m_len	+= mtr->rcvpad;
			IF_INITHEADER(mtod(m0, caddr_t), ifp, m0->m_len);
			mtr_snoop_input(mtr, m0, sri_len, pkt_len);

			m_adj(m0, mtr->rcvpad);
		}
	}

    	/* mtr_hexdump(mtod(m0, char*), 40); */
    	error = mtrsend(mtr, m0);

output_ret:
	m_freem( m0 );
	if (error) ifp->if_oerrors++;

output_ret2:
    	if (mloop) m_freem(mloop);
output_ret_without_freeing_mbuf:
    	return(error);
} /* mtr_output() */


/*
 * mtrsend:
 *
 *	- accepts only exact amount of RII
 *	- should CONSUME mbuf
 *	- should be called in splimp.
 *	- should count oerror.
 */
static int	
mtrsend(mtr_info_t *mtr, struct mbuf *m0)
{
    	int		i, slen, slen2;
    	ushort_t 	len;
    	int		error = 0;
    	uint_t 		buf_physaddr;
    	uchar_t 	*p;
    	ushort_t 		active_tx_slot = mtr->active_tx_slot;
    	TX_SLOT 	**tx_slot_array = mtr->tx_slot_array;

    	MTR_ASSERT(m0 != 0);

    	if( (len = m_length(m0)) > MAX_TX_LENGTH(mtr) ){
		/* too big */
		cmn_err(CE_NOTE, "mtr%d: TX pkt: %d > %d!\n",
			mtr->mif_unit, len, MAX_TX_LENGTH(mtr));
		error = EMSGSIZE;
		goto done;
    	}

    	OUTS_NSW(mtr->sif_adr, (ushort_t)(__psunsigned_t) 
		&tx_slot_array[active_tx_slot]->small_buffer_len);
    	slen = INS_NSW(mtr->sif_dat);

    	OUTS_NSW(mtr->sif_adr, (ushort_t)(__psunsigned_t) 
		&tx_slot_array[active_tx_slot]->large_buffer_len);
    	slen2 = INS_NSW(mtr->sif_dat);

    	if ((slen != 0) || (slen2 != 0)) {
		mtr->mif.if_odrops++;
		if( ++mtr->q_full_cnt > MAX_QFULL_IN_ROW ){
			cmn_err(CE_WARN, 
				"mtr%d: tx queue full: %d.%d.%d.%d -> 0x%x.", 
				mtr->mif_unit, slen, slen2,
				mtr->mif.if_odrops, mtr->q_full_cnt,
				mtr->mif_flags);
			hwi_halt_eagle(mtr);
			DUMP_RX_TX_SLOTS(mtr);
		} else {
			if( mtr->mif_flags & IFF_DEBUG)
				cmn_err(CE_NOTE, 
					"mtr%d: tx queue full: %d.%d.%d.%d.", 
					mtr->mif_unit, slen, slen2,
					mtr->mif.if_odrops, mtr->q_full_cnt);
		}
		error = ENOBUFS;
		goto done;
    	}
    	mtr->q_full_cnt = 0;

    	len = m_datacopy(m0, 0, mtr->mif_mtu+128,
			(caddr_t)mtr->tx_slot_buf[active_tx_slot]);
    	dki_dcache_wbinval((caddr_t)mtr->tx_slot_buf[active_tx_slot], 
		MAX_TX_LENGTH(mtr)+128);

    	/* Now write out the length of the buffer. This	will start the DMA.  */
    	OUTS_NSW(mtr->sif_adr, (ushort_t)(__psunsigned_t) 
		&tx_slot_array[active_tx_slot]->large_buffer_len);
    	OUTS_NSW(mtr->sif_dat, len);

    	/* No small buffer is present, but we still need to write the small 
    	 * buffer length, so use the special value (see	FastmacPlus manual) 
	 */
    	OUTS_NSW(mtr->sif_adr, (ushort_t)(__psunsigned_t) 
		&tx_slot_array[active_tx_slot]->small_buffer_len);
    	OUTS_NSW(mtr->sif_dat, FMPLUS_SBUFF_ZERO_LENGTH);

    	/* Update the next-slot indicator.   */
    	if (++active_tx_slot == mtr->init_block->fastmac_parms.tx_slots)
		active_tx_slot = 0;

    	mtr->active_tx_slot = active_tx_slot;

    	mtr->mif.if_opackets++;
    	mtr->mif.if_obytes += len;

	MTR_DBG_PRINT(mtr, ("mtr%d: TX: %d, %d.\n",
		mtr->mif_unit, mtr->mif.if_opackets, len));

done:
   	return(error);
}

/*
 *  Process an ioctl request.
 */
static int	
mtr_ioctl(struct ifnet *ifp, int cmd, caddr_t data)
{
    	int			i;
    	int			error = MTR_NO_ERROR;
    	mtr_info_t 		*mtr;
	struct ifreq    	*ifr = (struct ifreq *)data;
	uint_t			functional_address;

	mtr = ifnet_2_mtr_info(ifp);
    	MTR_ASSERT(&mtr->mtr_ac == (struct arpcom *)ifp);

    	switch (cmd) {

	case SIOC_TR_GETSPEED:
		ifr->ifr_metric = ( mtr->mtr_s16Mb ? 16 : 4);
		break;
	
	case SIOCGIFMTU:
		ifr->ifr_metric = mtr->mif_mtu;
		break;

	case SIOCAIFADDR: {		/* Add interface alias address */
		struct sockaddr *addr = _IFADDR_SA(data);
#ifdef INET6
		/*
		 * arp is only for v4.  So we only call arpwhohas if there is
		 * an ipv4 addr.
		 */
		if (ifp->in_ifaddr != 0)
			arprequest(&mtr->mtr_ac, 
				&((struct sockaddr_in*)addr)->sin_addr,
				&((struct sockaddr_in*)addr)->sin_addr,
				(&mtr->mtr_ac)->ac_enaddr);
#else
		arprequest(&mtr->mtr_ac, 
			&((struct sockaddr_in*)addr)->sin_addr,
			&((struct sockaddr_in*)addr)->sin_addr,
			(&mtr->mtr_ac)->ac_enaddr);
#endif
		
		break;
	}

	case SIOCDIFADDR:		/* Delete interface alias address */
		/* currently nothing needs to be done */
		break;

    	case SIOCSIFADDR: {
		struct sockaddr		*s_addr;

		s_addr = _IFADDR_SA(data);
	    	switch ( s_addr->sa_family ) {
	    	case AF_INET:
			if( mtr->mtr_ac.ac_ipaddr.s_addr == 0x08000000 ){
				MTR_ASSERT(0);
				error = EINVAL;
				break;
			}

			mtr->mtr_ac.ac_ipaddr = 
				((struct sockaddr_in*)s_addr)->sin_addr;
			((struct arpcom *)ifp)->ac_ipaddr = 
				mtr->mtr_ac.ac_ipaddr;

			if( (mtr->mif_flags & IFF_ALIVE) != IFF_ALIVE ) {
	    			arpwhohas((struct arpcom *)ifp,
	        			&((struct arpcom *)ifp)->ac_ipaddr);
			}
			break;

		case AF_RAW: {
			SRB_GENERAL	srb_general;
			
			bzero(&srb_general, sizeof(srb_general));
			bcopy( _IFADDR_SA(data)->sa_data,
			 	mtr->mif_enaddr,
				sizeof(mtr->mif_enaddr));
			bcopy( mtr->mif_enaddr, mtr->mtr_rawif.rif_addr,
				mtr->mtr_rawif.rif_addrlen);

			srb_general.header.function = OPEN_ADAPTER_SRB;

			srb_general.open_adap.open_options =
				(mtr->bd_state & MTR_STATE_PROMISC) ?
				 (OPEN_OPT_COPY_ALL_MACS |
				 OPEN_OPT_COPY_ALL_LLCS) : 0;
			srb_general.open_adap.open_options |= 
				mtr_participant_claim_token ?
					OPEN_OPT_CONTENDER : 0;

			bcopy( mtr->mif_enaddr,
				&srb_general.open_adap.open_address,
				sizeof(srb_general.open_adap.open_address));
			bcopy( &mtr->group_address,
				&srb_general.open_adap.group_address,
				sizeof(srb_general.open_adap.group_address));
			bcopy( &mtr->functional_address,
				&srb_general.open_adap.functional_address,
				sizeof(srb_general.open_adap.functional_address));
			if( mtr_reopen_adapter(mtr, &srb_general) 
				== MTR_NO_ERROR){
				/*	Update ARP cache. 	*/
				if( mtr->mtr_ac.ac_ipaddr.s_addr !=
					INADDR_ANY ){
					arpwhohas((struct arpcom *)ifp,
					&((struct arpcom *)ifp)->ac_ipaddr);
				}
				if( showconfig ) report_Configuration(mtr);
			} else {
				cmn_err(CE_ALERT,
					"mtr%d: SIOCSIFADDR(AF_RAW) failed",
					mtr->mif_unit);
			}
	
			} /* end of SIOCSIFADDR(AF_RAW) block */
			break;
	    	default:
			MTR_DBG_PRINT(mtr, 
				("mtr%d: SIOCSIFADDR: 0x%x\n", 
				mtr->mif_unit, ifr->ifr_addr.sa_family));
			error = EOPNOTSUPP;
			break;
	    	}

		}
siocsifaddr_done:
		break;

	case SIOC_TR_RESTART:
		{
		int			idx;
		int			new_mtu;
		int			new_s16Mb;
		uchar_t			new_enaddr[TR_MAC_ADDR_SIZE];
		mtr_restart_req_t	*restart_req;
		mtr_cfg_req_t		*req_p;

		mtr->fmplus_image = NULL;

		new_mtu		= mtr->mif_mtu;
		new_s16Mb	= mtr->mtr_s16Mb;
		bcopy( mtr->mif_enaddr, new_enaddr, sizeof(new_enaddr));

		restart_req = (mtr_restart_req_t *)data;

		/*	Get all the parameters ready.	*/
		for(idx = 0; 
		    idx < MAX_CFG_PARAMS_PER_REQ && error == MTR_NO_ERROR;
		    idx++){

			req_p = &restart_req->reqs[idx];

			switch( req_p->cmd ){
			case MTR_CFG_CMD_SET_SPEED:
				if( req_p->param.ring_speed != 16 &&
				    req_p->param.ring_speed != 4 ){
					error = EINVAL;
					cmn_err(CE_DEBUG, "mtr%d: "
						"MTR_CFG_CMD_SET_SPEED "
						"EINVAL: %d!\n",
						mtr->mif_unit,
						req_p->param.ring_speed);
				} else {
					new_s16Mb =
						(req_p->param.ring_speed == 16);
				}
				break;
	
			case MTR_CFG_CMD_SET_MTU:
				if( req_p->param.mtu > TRMTU_16M ){
					error = EINVAL;
					cmn_err(CE_DEBUG, "mtr%d: "
						"MTR_CFG_CMD_SET_MTU EINVAL!\n",
						mtr->mif_unit);
				} else {
					new_mtu = req_p->param.mtu;
				}
				break;

			case MTR_CFG_CMD_NULL:
			default:
				break;
			} /* switch req_p->cmd */	
		} /* for idx */
		if( error != MTR_NO_ERROR ){
			goto restart_adapter_unlock_done;
		}

		/*      Verify the new parameters.      */
		if( (new_s16Mb    && new_mtu > TRMTU_16M) ||
		    ((!new_s16Mb) && new_mtu > TRMTU_4M) ){
			error = EINVAL;
			cmn_err(CE_DEBUG, "mtr%d: %d != f(%d) EINVAL!\n",
				mtr->mif_unit, new_mtu, new_s16Mb);
			goto restart_adapter_unlock_done;
		}

		if( restart_req->dlrec.fmplus_image == 0x0){
			error = EINVAL;
			cmn_err(CE_DEBUG, "mtr%d: fmplus_image(%d) EINVAL!\n",
				mtr->mif_unit,
				restart_req->dlrec.fmplus_image);
			goto restart_adapter_unlock_done;
		}

		mtr->mtr_s16Mb = new_s16Mb;
		ifp->if_baudrate.ifs_value = new_s16Mb ? 16*1000*1000 :
			4*1000*1000;
		mtr->mif_mtu           = new_mtu;
		bcopy( new_enaddr, mtr->mif_enaddr,
			sizeof(mtr->mif_enaddr));
		bcopy( new_enaddr, mtr->mtr_rawif.rif_addr,
			mtr->mtr_rawif.rif_addrlen);

		if( (error = mtr_prepare_adapter(mtr)) != MTR_NO_ERROR ){
			cmn_err(CE_NOTE,
				"mtr%d: mtr_prepare_adapter: %d!",
				mtr->mif_unit, error);
			goto restart_adapter_unlock_done;
		}

		/*      Get the firmware image. */
		mtr->sizeof_fmplus_array =
			restart_req->dlrec.sizeof_fmplus_array;
		mtr->recorded_size_fmplus_array =
			restart_req->dlrec.recorded_size_fmplus_array;
		mtr->fmplus_checksum  =
			restart_req->dlrec.fmplus_checksum;
		if( restart_req->dlrec.sizeof_fmplus_array !=
		    restart_req->dlrec.recorded_size_fmplus_array ){
			cmn_err(CE_WARN, "mtr%d: dlrec: %d != %d!",
				mtr->mif_unit,
				restart_req->dlrec.sizeof_fmplus_array,
				restart_req->dlrec.recorded_size_fmplus_array);
			cmn_err(CE_PANIC, "mtr%d: restart_req: 0x%x!\n",
				mtr->mif_unit, restart_req);
			error = EINVAL;
			goto restart_adapter_unlock_done;
		}

		mtr->fmplus_image = kmem_alloc(
			mtr->sizeof_fmplus_array, KM_SLEEP);
		if( mtr->fmplus_image == NULL ){
			cmn_err(CE_WARN,
				"mtr%d: SIOC_TR_RESTART: kmem_alloc()!",
				mtr->mif_unit);
			error = ENOBUFS;
			goto restart_adapter_unlock_done;
		}
		copyin((caddr_t)restart_req->dlrec.fmplus_image,
			mtr->fmplus_image,
			mtr->sizeof_fmplus_array);

		if( (error = mtr_start_adapter(mtr)) == MTR_NO_ERROR ){
			if( mtr->mtr_ac.ac_ipaddr.s_addr != INADDR_ANY ) {
				arpwhohas((struct arpcom *)ifp,
					&((struct arpcom *)ifp)->ac_ipaddr);
			}
			if( showconfig ) report_Configuration(mtr);
		} else {
			cmn_err(CE_ALERT,
				"mtr%d: SIOC_TR_RESTART failed: %d!",
				mtr->mif_unit, error);
                }

restart_adapter_unlock_done:
#ifdef	RELEASE_IFNET_NEEDED_FOR_INIT
		mutex_unlock( &mtr->init_mutex );
#endif	/* RELEASE_IFNET_NEEDED_FOR_INIT */

restart_adapter_done:
		if( mtr->fmplus_image != NULL ){
			kmem_free(mtr->fmplus_image,
				mtr->sizeof_fmplus_array);
		}

		};
		break;

    	case SIOCSIFFLAGS:  

	    	MTR_DBG_PRINT(mtr, ("mtr%d: SIOCSIFFLAGS: n=0x%x.\n",
	    		mtr->mif_unit, ifr->ifr_flags));

		if( ((ifr->ifr_flags & IFF_PROMISC) && 
		     !(mtr->bd_state & MTR_STATE_PROMISC)) ||
		    (!(ifr->ifr_flags & IFF_PROMISC) &&
		     (mtr->bd_state & MTR_STATE_PROMISC)) ){
	
			if( ifr->ifr_flags & IFF_PROMISC ){
				mtr->bd_state |= MTR_STATE_PROMISC;
			} else {
				mtr->bd_state &= ~(MTR_STATE_PROMISC);
			}

			error = mtr_modify_open_parms(mtr);
		}
		break;

promisc_done:
	    	MTR_DBG_PRINT(mtr, ("mtr%d: SIOCSIFFLAGS done with %d\n.", 
			mtr->mif_unit, error));
		break;

    	case SIOCADDMULTI:
    	case SIOCDELMULTI: 
#define	IP_MULTICAST_FUNCTIONAL_ADDRESS		0x00040000
		if( (mtr->mif_flags & IFF_ALIVE) != IFF_ALIVE ){
			error = ENETDOWN;
			break;
		}

		switch( ((struct sockaddr *)data)->sa_family ){
		case AF_INET:

			/*	Enable/disable functional address for IP
			 *	multicast for first/last request only.
			 */
			if( cmd == SIOCADDMULTI){
				if( mtr->ip_multicast_cnt >= 1){
					mtr->ip_multicast_cnt++;
					goto siomulti_done;
				}
			} else if( cmd == SIOCDELMULTI ){
				if( mtr->ip_multicast_cnt == 0 ){
					/*	not eabled!	*/
					error = EINVAL;
					goto siomulti_done;
				} else if( mtr->ip_multicast_cnt != 1 ){
					/* 	someone else still uses it */
					mtr->ip_multicast_cnt--;
					goto siomulti_done;
				}
			} else {
				error = EIO;
				MTR_ASSERT(0);
				goto siomulti_done;	
			}

			functional_address = IP_MULTICAST_FUNCTIONAL_ADDRESS;

			error = mtr_functional_address( mtr, cmd,
				functional_address);

			if( error == MTR_NO_ERROR ){
				if( cmd == SIOCADDMULTI ){
					ifp->if_flags |= IFF_ALLMULTI;
					mtr->ip_multicast_cnt++;
					MTR_ASSERT( mtr->ip_multicast_cnt == 1);
				} else {
					MTR_ASSERT( cmd == SIOCDELMULTI );
					ifp->if_flags &= ~IFF_ALLMULTI;
					mtr->ip_multicast_cnt--;
					MTR_ASSERT( mtr->ip_multicast_cnt == 0);
				}
			}
			break;

		case AF_RAW:
			if( ((struct sockaddr *)data)->sa_data[0] != 0xc0 ||
			    ((struct sockaddr *)data)->sa_data[1] != 0x00 ){
				error = EINVAL;
				break;
			}

			if( ((struct sockaddr *)data)->sa_data[2] & 0x80) {

				/* 	It is a multicast address */
				error = mtr_group_address( mtr, cmd,
					&((struct sockaddr *)data)->sa_data[2]);
				MTR_DBG_PRINT(mtr,
					("mtr%d: mtr_group_address() error: 0x%x.\n",
					 mtr->mif_unit, error));
			} else {
				error = EOPNOTSUPP;
			}
			break;

		default:
			error = EOPNOTSUPP;
			break;
		} /* switch */
siomulti_done:
		break;

    	case SIOC_TR_GETCONF: 
	 	{
	    	TR_CONFIG trc;
	    	TR_SIOC * sioc = (TR_SIOC * )data;
	    	int	len = MIN(sizeof(trc), sioc->len);

	    	bcopy(mtr->mif_enaddr, trc.caddr, TR_MAC_ADDR_SIZE);
	    	bcopy(mtr->mtr_bia, trc.bia, TR_MAC_ADDR_SIZE);
	    	bcopy(mtr->mcl, trc.mcl, sizeof(trc.mcl));
	    	bcopy(mtr->pid, trc.pid, sizeof(trc.pid));
	    	bcopy(mtr->sid, trc.sid, sizeof(trc.sid));

	    	if (copyout((caddr_t) & trc, (caddr_t)sioc->tr_ptr_u.conf,
	         	len)) error = EFAULT;
		} 
		break;

	case SIOC_TR_GETIFSTATS:
		/*	Get soft error counter */
		if( (mtr->mif_flags & IFF_ALIVE) != IFF_ALIVE ){
			error = ENETDOWN;
			break;
		}

		error = mtr_read_error_log(mtr);

		if( error == MTR_NO_ERROR ){
#define	srb_error_log	err_log.error_log
			bcopy( &mtr->srb_general2.srb_error_log,
				&ifr->ifr_metric,
				sizeof(mtr->srb_general2.srb_error_log));
#undef	srb_error_log
		}
		break;

	case SIOC_MTR_ADDFUNC:
	case SIOC_MTR_DELFUNC:
		if( ifr->ifr_addr.sa_family != AF_RAW ){
			error = EAFNOSUPPORT;
			break;
		}
		if( (mtr->mif_flags & IFF_ALIVE) != IFF_ALIVE ){
			error = ENETDOWN;
			break;
		}

		functional_address = *((uint_t *)ifr->ifr_dstaddr.sa_data);

		MTR_ASSERT( error == MTR_NO_ERROR );
		error = mtr_functional_address( mtr, cmd, functional_address);
		break;
	
	case SIOC_MTR_GETFUNC:
		if( ifr->ifr_addr.sa_family != AF_RAW ){
			error = EAFNOSUPPORT;
			break;
		}
	
		*((uint_t *)ifr->ifr_dstaddr.sa_data)  =
			mtr->functional_address;
		break;

	case SIOC_MTR_GETGROUP:
		if( ifr->ifr_addr.sa_family != AF_RAW ){
			error = EAFNOSUPPORT;
			break;
		}
	
		*((uint_t *)ifr->ifr_dstaddr.sa_data) = mtr->group_address;
		break;

	case SIOCSIFBRDADDR:
		switch( ifr->ifr_addr.sa_family ){
		case AF_RAW:
			if( !TOKEN_ISBROAD(ifr->ifr_addr.sa_data) ){
				error = EINVAL;
				break;
			}

			bcopy( ifr->ifr_addr.sa_data,
				mtr->mtr_baddr, sizeof(mtr->mtr_baddr));

			if( showconfig )
				report_Configuration(mtr);
			break;

		case AF_INET:
			break;
	
		default:
			error = EAFNOSUPPORT;
		}
		break;

	case SIOCGIFBRDADDR:
		switch( ifr->ifr_addr.sa_family ) {
		case AF_RAW:
			bcopy( mtr->mtr_baddr, ifr->ifr_addr.sa_data,
				TR_MAC_ADDR_SIZE);
			break;

		case AF_INET:
			break;

		default:
			error = EOPNOTSUPP;
		}
		break;

    	case SIOC_TR_GETMULTI:
    	case SIOC_TR_ADDMULTI:
    	case SIOC_TR_DELMULTI:
    	case SIOC_TR_GETFUNC:
    	case SIOC_TR_ADDFUNC:
    	case SIOC_TR_DELFUNC:
    	case SIOCADDSNOOP:
    	case SIOCDELSNOOP:
    	case SIOC_TR_GETST: 
    	case SIOC_TR_GETLLCSTAT: 
	default:
		error = EOPNOTSUPP;
		break;

    }
    return(error);
}

#ifdef	LOCAL_EISA_CLEANUP
/* 	WATCH OUT: copy from ../io/eisa.c    */
#define	MAX_EISA_SLOTS	4
struct eisa_intvec {
        void    (*eisa_irq_svc[MAX_EISA_SLOTS])(long);
        long    eisa_arg[MAX_EISA_SLOTS];
        u_char  level;
        u_char  reserved;
        int     count;
};
extern  struct eisa_intvec eisa_int_info[16];

/*
 *      To clean up the allocated EISA interrupt resource
 *      that includes interrupt level and the registered
 *      call back routine.
 *
 *      WATCH OUT: this routine is tightly coupled with
 *      the implementation in io/eisa.c so ...
 *
 */
static void
eisa_cleanup( mtr_info_t *mtr )
{
        struct eisa_intvec *exec;

        exec = &eisa_int_info[mtr->irq];

        if( exec->count != 1 ){

                /* Oops! I do not how to handle this.
                 * The code in eisa.c assumes there is no NULL hole so ...!
                 */
                cmn_err(CE_WARN, "mtr%d: eisa_cleanup(): irq: %d, count: %d!",
                        mtr->mif_unit, mtr->irq, exec->count);
                goto done;
        }

        /* unregister the routine */
        exec->eisa_irq_svc[0]   = NULL;
        exec->eisa_arg[0]       = 0;

        exec->level             = 0;
        exec->count             = 0;
done:
        return;
} /* eisa_cleanup */
#endif	/* LOCAL_EISA_CLEANUP */

static void
release_resources(mtr_info_t *mtr, int flag /* releasing ifnet also? */)
{
	struct ifnet	*ifp;

	if( mtr == NULL )goto done;

	hwi_halt_eagle( mtr );

	if( flag ){
		if( (ifp = mtr_info_2_ifnet(mtr)) != NULL ){
			if( ifp->if_name != NULL )
				kmem_free(ifp->if_name, strlen(MTR_NAME)+1 );
			kmem_free( mtr->if_mtr_p, sizeof(*mtr->if_mtr_p) );
		}
	}

	/*	TO DO: PIO map */

	/* 	TO DO: DMA map */

	if( mtr->slot_buf_start != NULL && mtr->all_rx_tx_bufs_size != 0 ){
		kvpfree( mtr->rx_tx_bufs_addr, 
			(mtr->all_rx_tx_bufs_size / NBPP));
		mtr->slot_buf_start = NULL;
	}

	kmem_free( mtr, sizeof(*mtr) );

done:
	return;
} /* release_resources() */

int
if_mtropen(dev_t *devp, int mode)
{
	return( if_mtr_cnt == 0 ? ENODEV : MTR_NO_ERROR );
} /* if_mtropen() */

int
if_mtrclose(dev_t dev)
{
	return(EINVAL);
} /* if_mtrclose() */

int
if_mtrwrite(dev_t dev, uio_t *uiop)
{
	return(EINVAL);
} /* if_mtrwrite() */

int
if_mtrioctl(dev_t dev, int cmd, int arg, int mode, struct cred *crp, int *rvp)
{
	return(EINVAL);
} /* if_mtrioctl() */

void
if_mtrinit(void)
{

	MTR_DBG_PRINT2(("if_mtrinit().\n"));

#ifdef	BEING_TESTED	
	load_multiple_times = atomicAddInt( &if_mtr_loaded, 1 );
#else
	load_multiple_times = ++if_mtr_loaded;
#endif	/* BEING_TESTED */
	if( load_multiple_times > 1 ){
		cmn_err(CE_WARN, "if_mtr loaded!");
		goto done;
	}
	load_multiple_times--;

	/* 	Let's turn off EISA DMA refresh. It could hang the system!
	 */
	eisa_refresh_off();

    	eisa_preempt &= 0xffff;
    	eisa_quiet   &= 0xffff;
    	OUTW_NSW(PHYS_TO_K1(EIU_PREMPT_REG), eisa_preempt);
    	OUTW_NSW(PHYS_TO_K1(EIU_QUIET_REG), eisa_quiet);
    	oldpreempt	= eisa_preempt;
    	oldquiet 	= eisa_quiet;
    	eisa_preempt 	= INW(PHYS_TO_K1(EIU_PREMPT_REG));
    	eisa_quiet   	= INW(PHYS_TO_K1(EIU_QUIET_REG));
    	if( oldpreempt != eisa_preempt || oldquiet   != eisa_quiet){
		cmn_err(CE_ALERT, 
			"if_mtr: init: preempt: 0x%x.0%x "
			"quite: 0x%x.0x%x.",
			oldpreempt, eisa_preempt, 
			oldquiet, eisa_quiet );
    	} else if( showconfig ){
		cmn_err(CE_CONT,
			"if_mtr: eisa: preempt: 0x%x.0%x; quite: 0x%x.0x%x.\n",
			oldpreempt, eisa_preempt,
			oldquiet, eisa_quiet );
	}

	if_mtr_sub_addrs = if_mtr_subs;

done:
	return;
} /* if_mtrinit() */

void
if_mtredtinit(struct edt *edtp)
{
    	int	     	mtr_buff_size;	/* size of each buffer */
    	int		i, s;
    	piomap_t 	*pmap;
    	mtr_info_t 	*mtr;
    	struct ifnet 	*ifp;
	if_mtr_t	*if_mtr_p;
    	uint_t 		unit;
    	caddr_t 	io_addr, sif_base, control_x;
    	caddr_t 	mem;
    	uchar_t 	cstat, initval;
    	FASTMAC_INIT_PARMS *fastmac_parms;
	int		res;
	int		alloc_ifnet = 0;
	int		error = MTR_NO_ERROR;

	MTR_DBG_PRINT2(("if_mtredtinit(0x%x: %d).\n", 
		edtp, (edtp ? edtp->e_ctlr : 0)));

	if( load_multiple_times )goto done2;
	
	/*	Find out the unit number.	*/
    	unit = edtp->e_ctlr;
    	if (unit > MTR_MAXBD + 1 || unit < 1) {
		cmn_err(CE_ALERT, "mtr%d: bad EDT ctlr entry.", unit);
		error = EIO;
		goto done;
    	}

	/*	Do we really have the adapter?	*/
    	pmap = pio_mapalloc(ADAP_EISA, 0, &edtp->e_space[0], 
		PIOMAP_FIXED, MTR_NAME);
    	if (pmap == 0) {
		cmn_err(CE_ALERT, "mtr%d: could not allocate pio map.", unit);
		error = EIO;
		goto done;
    	}

	if( edtp->e_space[0].ios_iopaddr == NULL ){
		cmn_err(CE_ALERT, "mtr%d: edtp->e_space[0].ios_iopaddr: 0x%x",
			unit, edtp->e_space[0].ios_iopaddr);
		error = EIO;
		goto done;
	}
    	io_addr = pio_mapaddr(pmap, edtp->e_space[0].ios_iopaddr);
	MTR_DBG_PRINT2(("if_mtredtinit(): mtr%d: io_addr: 0x%x.\n", 
		unit, io_addr));
    	if (badaddr(io_addr + EisaId, 4) || 
	   (INW(io_addr + EisaId) != MTR_ID)) {
		pio_mapfree(pmap);
		cmn_err(CE_NOTE, "mtr%d: no adapter on slot %d.", 
			unit, (unit - 1));
		error = EIO;
		goto done2;
    	}

	/*	Allocate mtr_info.	*/
	mtr = kmem_zalloc( sizeof(struct mtr_info), MTR_KMEM_ARGS);
	if( mtr == NULL ){
		cmn_err(CE_ALERT,"mtr%d kmem_zalloc falied", unit);
		error = EIO;
		goto done2;
	}

	/*	Allocate if_mtr_t, if it does not exist for the adapter */
	for( ifp = ifnet; ifp != NULL; ifp = ifp->if_next){
		if( strncmp(ifp->if_name, MTR_NAME, strlen(MTR_NAME)) != 0)
			continue;
		if( ifp->if_unit == unit ){
    			MTR_DBG_PRINT2(("mtr%d: ifp: 0x%x found.\n", 
				unit, ifp));
			break;
		}
	} /* for */
	if( ifp != NULL ){
		if_mtr_p = ifnet_2_ifmtr(ifp);
	} else {
		if( (if_mtr_p = kmem_zalloc(sizeof(if_mtr_t), MTR_KMEM_ARGS)) 
		     == NULL ){
			cmn_err(CE_ALERT,"mtr%d kmem_zalloc falied 1",
				unit);
			error = EIO;
			kmem_free( mtr, sizeof(*mtr) );
			goto done2;
		}
		ifp = ifmtr_2_ifnet(if_mtr_p);
		alloc_ifnet = 1;
		if( (ifp->if_name = kmem_zalloc(strlen(MTR_NAME)+1, 
			MTR_KMEM_ARGS)) == NULL ){
			cmn_err(CE_ALERT,"mtr%d kmem_zalloc falied 2", unit);
			error = EIO;
			kmem_free( mtr, sizeof(*mtr) );
			goto done2;
		}
		strncpy( ifp->if_name, MTR_NAME, strlen(MTR_NAME));
	}
	if_mtr_p->mtr 	= mtr;
	mtr->if_mtr_p 	= if_mtr_p;
	mtr->next	= NULL;
	if( mtr_info_link == NULL ){
		mtr_info_link = mtr;
	} else {
		mtr_info_t	*tmp;

		for( tmp = mtr_info_link; tmp->next != NULL; tmp = tmp->next ){
			MTR_ASSERT( tmp->mif_unit != unit );
		};
		tmp->next = mtr;
	}
    	MTR_DBG_PRINT2(("mtr%d: ifp: 0x%x, mtr: 0x%x, mtr_info_link: 0x%x.\n", 
		unit, ifp, mtr, mtr_info_link));

    	mtr->mio 	= io_addr;
	mtr->mtr_s16Mb 	= mtr_s16Mb;
	mtr->irq	= -1;

	mutex_init( &(mtr->srb_mutex), MUTEX_DEFAULT, MTR_NAME);
    	/*
    	 * Save IO locations of SIF registers.
    	 */
    	sif_base 	= mtr->mio + EISA_FIRST_SIF_REGISTER;
    	mtr->sif_dat  	= (ushort_t * )(sif_base + EAGLE_SIFDAT);
    	mtr->sif_datinc = (ushort_t * )(sif_base + EAGLE_SIFDAT_INC);
    	mtr->sif_adr  	= (ushort_t * )(sif_base + EAGLE_SIFADR);
    	mtr->sif_int  	= (ushort_t * )(sif_base + EAGLE_SIFINT);
    	mtr->sif_acl  	= (ushort_t * )(sif_base + EISA_EAGLE_SIFACL);
    	mtr->sif_adr2  	= (ushort_t * )(sif_base + EISA_EAGLE_SIFADR_2);
    	mtr->sif_adx  	= (ushort_t * )(sif_base + EISA_EAGLE_SIFADX);
    	mtr->sif_dmalen = (ushort_t * )(sif_base + EISA_EAGLE_DMALEN);
    	control_x 	= mtr->mio + EISA_CONTROLX_REGISTER;
    	OUTB(control_x, 0x1);
    	MTR_DBG_PRINT2(("mtr%d: mtr->mio: 0x%x.\n", unit, mtr->mio));
    	MTR_DBG_PRINT2(("mtr%d: sif_base: 0x%x.\n", unit, sif_base));
    	MTR_DBG_PRINT2(("mtr%d: sif_dat: 0x%x.\n", unit, mtr->sif_dat));
    	MTR_DBG_PRINT2(("mtr%d: sif_datinc: 0x%x.\n", unit, mtr->sif_datinc));
    	MTR_DBG_PRINT2(("mtr%d: sif_adr: 0x%x.\n", unit, mtr->sif_adr));
    	MTR_DBG_PRINT2(("mtr%d: sif_int: 0x%x.\n", unit, mtr->sif_int)); 
    	MTR_DBG_PRINT2(("mtr%d: sif_acl: 0x%x.\n", unit, mtr->sif_acl));
    	MTR_DBG_PRINT2(("mtr%d: sif_adr2: 0x%x.\n", unit, mtr->sif_adr2));
    	MTR_DBG_PRINT2(("mtr%d: sif_adx: 0x%x.\n", unit, mtr->sif_adx));
    	MTR_DBG_PRINT2(("mtr%d: sif_dmalen: 0x%x.\n", unit, mtr->sif_dmalen));
    	MTR_DBG_PRINT2(("mtr%d: control_x: 0x%x.\n", unit, control_x));

    	/*
    	 * Allocate the slot buffers.
     	 */
    	mtr_max_rx_slots= min(mtr_max_rx_slots, FMPLUS_MAX_RX_SLOTS);
    	mtr_max_tx_slots= min(mtr_max_tx_slots, FMPLUS_MAX_TX_SLOTS);
    	mtr_mtu 	= min(max(1, mtr_mtu), 
			      mtr->mtr_s16Mb ? TRMTU_16M : TRMTU_4M);
    	mtr_buff_size	= MAKE_ALIGNMENT( (mtr_mtu + 36 + 128),  256);
    	mtr->all_rx_tx_bufs_size = (mtr_buff_size * 
		(mtr_max_tx_slots + mtr_max_rx_slots) + NBPP) / NBPP;

	MTR_DBG_PRINT2(("mtr%d: mtr_max_rx_slots: %d, mtr_max_tx_slots: %d,\n"
		"\tmtr_mtu: %d, mtr_buff_size: %d, all_rx_tx_bufs_size: %d.\n",
		unit,
		mtr_max_rx_slots, mtr_max_tx_slots,
		mtr_mtu,
		mtr_buff_size,
		mtr->all_rx_tx_bufs_size));

    	mem = (caddr_t)kvpalloc( mtr->all_rx_tx_bufs_size, 
		VM_NOSLEEP | VM_PHYSCONTIG, 0);
    	if (mem == NULL) {
	    	cmn_err(CE_ALERT, 
			"mtr%d: unable to allocate buff memory: %d.", 
			unit, mtr->all_rx_tx_bufs_size);
		error = EIO;
		goto done;
    	}
	
    	mtr->slot_buf_start 	= mem;
    	mtr->slot_buf_end	= mem + (mtr->all_rx_tx_bufs_size * NBPP);
    	for (i = 0; i < mtr_max_rx_slots; i++) {
		mtr->rx_slot_buf[i] = (caddr_t)mem;
		mem += mtr_buff_size;
    	}
    	for (i = 0; i < mtr_max_tx_slots; i++) {
		mtr->tx_slot_buf[i] = (caddr_t)mem;
		mem += mtr_buff_size;
    	}
	dki_dcache_inval(mem, mtr->all_rx_tx_bufs_size * NBPP);

    	mtr->irq = eisa_ivec_alloc(0, mtr_irq_mask, EISA_LEVEL_IRQ);
	MTR_DBG_PRINT2(("mtr%d: eisa_ivec_alloc(0, 0x%x, 0x%x) -> %d.\n",
		unit, mtr_irq_mask, EISA_LEVEL_IRQ, mtr->irq));
    	mtr_irq_mask &= ~(1 << mtr->irq);

    	initval = M1_INIT | m1_speed[mtr->mtr_s16Mb] |
              m1_media[mtr_UTPmedia] | m1_irq[mtr->irq];
    	OUTB(io_addr + MADG_IOPORT1, initval);

    	initval = M2_INIT | m2_speed[mtr->mtr_s16Mb] | m2_irq[mtr->irq];
    	OUTB(io_addr + MADG_IOPORT2, initval);
	
    	initval = M3_INIT | m3_media[mtr_UTPmedia];
    	OUTB(io_addr + MADG_IOPORT3, initval);

    	mtr->rcvpad = TRPAD;

	MTR_DBG_PRINT2(("mtr%d: eisa_ivec_set(0, 0x%x, 0x%x, 0x%x).\n",
		unit, mtr->irq, (void (*)(long))if_mtrintr, (long)mtr));
    	eisa_ivec_set(0, mtr->irq, (void (*)(long))if_mtrintr, (long)mtr);

    	/* 	ATTACH interface */
    	ifp->if_unit 	= unit;
    	ifp->if_type 	= IFT_ISO88025;
    	ifp->if_mtu 	= mtr_mtu;
#ifdef	DEBUG2
    	ifp->if_flags 	= IFF_BROADCAST | IFF_MULTICAST | 
        		  IFF_ALLCAST | IFF_IPALIAS | IFF_DEBUG;
#else	/* DEBUG2 */
    	ifp->if_flags 	= IFF_BROADCAST | IFF_MULTICAST |
        		  IFF_ALLCAST | IFF_IPALIAS;
#endif	/* DEBUG2 */
    	ifp->if_output 	= mtr_output;
    	ifp->if_ioctl 	= (int (*)(struct ifnet *, int, void * ))mtr_ioctl;
	ifp->if_reset	= (int (*)(int, int))nodev;
	ifp->if_watchdog= (void(*)(struct ifnet *))nodev;
	ifp->if_timer	= 0;
	ifp->if_baudrate.ifs_value = mtr->mtr_s16Mb ? 16*1000*1000 :4*1000*1000;
    	if_attach(ifp);

	{
		int		raw_hdr_len;

		raw_hdr_len = TR_RAW_MAC_SIZE + sizeof(LLC1)
				+ sizeof(SNAP);

		rawif_attach( &mtr->mtr_rawif,
			ifp,
			(caddr_t)mtr->mif_enaddr,	/* addr */
			(caddr_t)&etherbroadcastaddr[0],/* broadcast addr */
			sizeof( mtr->mif_enaddr),	/* addrlen */
			raw_hdr_len,			/* hdrlen */
			structoff(tr_mac, mac_sa[0]), 	/* srcoff */
			structoff(tr_mac, mac_da[0]));	/* dstoff */
	} /* */

    	add_to_inventory(INV_NETWORK, INV_NET_TOKEN, INV_TOKEN_MTR, 
		unit, (mtr->mtr_s16Mb ? 1 : 0) /* 16M or 4M */);

done:
	if( error != MTR_NO_ERROR ){
		release_resources( mtr, alloc_ifnet );
	} else {
		if_mtr_cnt++;
	}

done2:
    	return;
} /* if_mtredtinit() */

int
if_mtrunload(void )
{
	int		error = MTR_NO_ERROR;
	struct ifnet	*ifp;
	mtr_info_t	*mtr, *mtr2;
	if_mtr_t	*if_mtr_p;

	if( load_multiple_times )goto done;

	for( mtr = mtr_info_link; mtr != NULL; ){
#ifdef	LOCAL_EISA_CLEANUP
		mtr2 	= mtr->next;
		if_mtr_p= mtr->if_mtr_p;

		IFNET_LOCK( ifmtr_2_ifnet(if_mtr_p));

		hwi_halt_eagle( mtr );

		if( mtr->irq != -1 )eisa_cleanup( mtr );
	
		if_down(ifp);
		ifp->if_timer 	= 0;
    		ifp->if_output 	= (int (*)(struct ifnet *, 
			struct mbuf *, struct sockaddr *))nodev;
    		ifp->if_ioctl 	= (int (*)(struct ifnet *, int, void * ))nodev;
		ifp->if_reset	= (int (*)(int, int))nodev;
		ifp->if_watchdog= (void (*)(struct ifnet *))NULL;
		release_resources( mtr, 0 );

		IFNET_UNLOCK( ifmtr_2_ifnet(if_mtr_p));

		mtr = mtr2;
#else	/* EISA_CLEANUP */
		error = EIO;		/* not support */
		break;
#endif	/* EISA_CLEANUP */
	} /* for */

done:
	if_mtr_loaded--;
	return( error );
} /* mtr_unload() */

static int
mtr_issue_srb( mtr_info_t *mtr, SRB_GENERAL *srb_ptr)
{
	int		error = MTR_NO_ERROR, res;
	int		idx;
	ushort_t	*ptr;

	MTR_ASSERT( (sizeof(SRB_GENERAL) & 1) == 0);
	
	if( !mutex_trylock( &mtr->srb_mutex) ){
		error = EBUSY;
		goto done;
	}
	MTR_ASSERT( mtr->srb_used == 0 );	
	if( mtr->srb_used != 0 ){
		cmn_err(CE_WARN, "mtr%d: mtr_issue_srb: srb_used: 0x%x.",
			mtr->mif_unit, mtr->srb_used );
		mtr->srb_used = 0;
		mutex_unlock( &mtr->srb_mutex );
		hwi_halt_eagle( mtr );
		goto done;
	}
	mtr->srb_used++;

	bcopy( srb_ptr, &(mtr->srb_general), sizeof(mtr->srb_general));
	bzero( &(mtr->srb_general2), sizeof(mtr->srb_general2) );
	mtr->srb_general2.header.function    = 0xff;
	mtr->srb_general2.header.return_code = 0xff;

   	hwi_eisa_set_dio_address(mtr, 
		((__psunsigned_t)mtr->srb_dio_addr & 0xffff),
		((DIO_LOCATION_EAGLE_DATA_PAGE >> 16) & 0xffff) );
	for( ptr = (ushort_t *)&(mtr->srb_general), idx = 0;
		idx <= sizeof(SRB_GENERAL); idx += sizeof(*ptr), ptr++){
    		OUTS_NSW( mtr->sif_datinc, *ptr);
	} /* for */
	OUTS_NSW( mtr->sif_int, EAGLE_SRB_CMD_CODE);

	/*
	 *	Interrupt needs to enabled and ifnet needs to be
	 *	locked by the interrupt handler to let SRB_FREE
	 *	comes in. 
	 */
	IFNET_UNLOCK( &mtr->mif );
	res = sleep( &mtr->srb_used, PZERO+1 /* allow signal */);
	IFNET_LOCK( &mtr->mif );

	if( res ){
        	MTR_DBG_PRINT(mtr, 
			("mtr%d: mtr_issue_srb() INTR: 0x%x.%x.%x.\n",
			 mtr->mif_unit, res, 
			 mtr->srb_general.header.function,
			 mtr->srb_general.header.return_code));
		error = EINTR;
	} else {

		MTR_DBG_PRINT(mtr,
			("mtr%d: SRB(0x%x) result: 0x%x.\n",
			 mtr->mif_unit,
			 mtr->srb_general2.header.function,
			 mtr->srb_general2.header.return_code));

		if( mtr->srb_general2.header.function !=
		    mtr->srb_general.header.function ){
			cmn_err(CE_WARN, "mtr%d: SRB for 0x%x not 0x%x!",
				mtr->mif_unit,
				mtr->srb_general2.header.function,
				mtr->srb_general.header.function);
			error = EIO;
			hwi_halt_eagle( mtr );
			goto done;
		}

		if( mtr->srb_general2.header.return_code ){
			cmn_err(CE_WARN, "mtr%d: SRB(0x%x) result: 0x%x.",
				mtr->mif_unit,
				mtr->srb_general2.header.return_code);
			error = EIO;
		}
	} /* if res */
	
	mtr->srb_used--;
	MTR_ASSERT( mtr->srb_used == 0 );
	mutex_unlock( &mtr->srb_mutex );	

done:
	return( error );
}  /* mtr_issue_srb() */

static int
mtr_read_error_log( mtr_info_t *mtr )
{
	int		error = MTR_NO_ERROR;
	SRB_GENERAL	srb_general;

	bzero( &srb_general, sizeof(srb_general) );
	srb_general.header.function	= READ_ERROR_LOG_SRB;

	error = mtr_issue_srb( mtr, &srb_general);

done:
	return( error );
} /* mtr_read_error_log() */

/*
 *	To add a groupd address, MAC multicast address.
 */
static int
mtr_group_address( mtr_info_t *mtr, int cmd, caddr_t addr_p)
{
	int		error = MTR_NO_ERROR;
	SRB_GENERAL	srb_general;

	if( addr_p == 0 ){
		error = EIO;
		goto done;
	}

	bcopy( addr_p, &(mtr->new_group_addr), sizeof(mtr->new_group_addr));
	switch( cmd ){
	case SIOCADDMULTI:
		if( mtr->group_address == mtr->new_group_addr ){
			goto done;
		}
		if( mtr->group_address != 0 ){
			cmn_err(CE_NOTE, 
				"mtr%d group address 0x%x replacing 0x%x!",
				mtr->mif_unit, mtr->new_group_addr, 
				mtr->group_address );
		}
		break;
	case SIOCDELMULTI:
		if( mtr->group_address == 0 ||
		    mtr->group_address != mtr->new_group_addr ){
			error = EINVAL;
			goto done;
		}
		mtr->new_group_addr = 0;
		break;
	default:
		error = EIO;
		MTR_ASSERT(0);
		goto done;
	}

	bzero( &srb_general, sizeof(srb_general));

	srb_general.header.function	= SET_GROUP_ADDRESS_SRB;
	bcopy( &(mtr->new_group_addr), &srb_general.set_group.multi_address.all,
		sizeof(srb_general.set_func.multi_address.all));

	error = mtr_issue_srb( mtr, &srb_general );

	if( error == MTR_NO_ERROR ){
		/*	commit the change	*/
		mtr->group_address = mtr->new_group_addr;
	}

done:
	return( error );
} /* mtr_group_address() */

static int
mtr_modify_open_parms(mtr_info_t *mtr)
{
        int             error = MTR_NO_ERROR;
        SRB_GENERAL     srb_general;

        bzero(&srb_general, sizeof(srb_general));
        srb_general.header.function = MODIFY_OPEN_PARMS_SRB;
        srb_general.mod_parms.open_options     = 
		(mtr_participant_claim_token ? OPEN_OPT_CONTENDER : 0);
	if( mtr->bd_state & MTR_STATE_PROMISC ) 
		srb_general.mod_parms.open_options	|=
                	(OPEN_OPT_COPY_ALL_MACS | OPEN_OPT_COPY_ALL_LLCS);

        if( (error = mtr_issue_srb( mtr,  &srb_general )) != MTR_NO_ERROR ){
                cmn_err(CE_DEBUG, "mtr%d: mtr_modify_open_parms(): "
                        "mtr_issue_srb() failed!\n",
                        mtr->mif_unit);
        }

        return( error );
} /* mtr_modify_open_parms() */

static int
mtr_reopen_adapter(mtr_info_t *mtr, SRB_GENERAL *new_srb_general)
{
        int             error = MTR_NO_ERROR;
        SRB_GENERAL     srb_general;

        /*      close it first. */
        bzero(&srb_general, sizeof(srb_general));
        srb_general.header.function = CLOSE_ADAPTER_SRB;
        error = mtr_issue_srb( mtr, &srb_general );
	if( error != MTR_NO_ERROR ){
		cmn_err(CE_DEBUG, "mtr%d: mtr_reopen_adapter(): "
			"mtr_issue_srb() 1 failed!\n", mtr->mif_unit);
	}

        /*      open it. */
        error = mtr_issue_srb( mtr, new_srb_general );
        if( error != MTR_NO_ERROR ){
                cmn_err(CE_DEBUG, "mtr%d: mtr_reopen_adapter(): "
                        "mtr_issue_srb() 2 failed!\n", mtr->mif_unit);
        }

        return( error );
} /* mtr_reopen_adapter() */

static int
mtr_functional_address( mtr_info_t *mtr, int cmd, 
	uint_t functional_address)
{
	int		error = MTR_NO_ERROR;
	int		idx;
	SRB_GENERAL	srb_general;
	
	bzero(&srb_general, sizeof(srb_general));

	switch( cmd ){
	case SIOC_MTR_ADDFUNC:
	case SIOCADDMULTI:
		if( mtr->functional_address & functional_address )
			goto done;
		mtr->new_functional_address =
			mtr->functional_address | functional_address;
		break;

	case SIOC_MTR_DELFUNC:
	case SIOCDELMULTI:
		if( !(mtr->functional_address & functional_address) )
			goto done;
		mtr->new_functional_address =
			mtr->functional_address & ~(functional_address);
		break;

	default:
		error = EIO;
		MTR_ASSERT(0);
		goto done;
	}
	srb_general.header.function		= SET_FUNCTIONAL_ADDRESS_SRB;
	srb_general.set_func.multi_address.all	= 
					mtr->new_functional_address;	

	error = mtr_issue_srb( mtr, &srb_general );

	if( error == MTR_NO_ERROR ){	
		mtr->functional_address = mtr->new_functional_address;
	}
	
done:
	return( error );
} /* mtr_functional_address() */

static void	
hwi_halt_eagle(mtr_info_t *mtr)
{
    	ushort_t acontrol_output, t;
    	ushort_t * sif_adapter_control_register = mtr->sif_acl;

    	acontrol_output = INS_NSW(sif_adapter_control_register);
    	acontrol_output &= EAGLE_SIFACL_PARITY;
    	acontrol_output |= (EAGLE_SIFACL_ARESET  | EAGLE_SIFACL_CPHALT  | 
        		    EAGLE_SIFACL_BOOT    | EAGLE_SIFACL_SINTEN);

    	us_delay(50);		
    	OUTS_NSW(sif_adapter_control_register, acontrol_output);

    	/* 14us is min time to hold ARESET low between resets.  */
    	us_delay(50);		

    	/* Bring EAGLE out of reset state, maintain halt status.
	 */
    	OUTS_NSW(sif_adapter_control_register,
        	acontrol_output & ~EAGLE_SIFACL_ARESET);

	mtr->mif_flags &= ~(IFF_RUNNING | IFF_UP);

	return;
} /* hwi_halt_eagle */

#define SWAP_16(_s)	((((_s) & 0xff) << 8) | (((_s) >> 8) & 0xff))
static int	
hwi_download_code( mtr_info_t *mtr, DOWNLOAD_RECORD *dlrec)
{
	int		error = MTR_NO_ERROR;
    	ushort_t 	*sif_data_inc_r = mtr->sif_datinc;
    	uint_t 		i;
    	ushort_t 	t;
    	int		idx;
    	uchar_t		checksum;

	MTR_DBG_PRINT2(("mtr%d: hwi_download_code().\n", mtr->mif_unit));
    	MTR_ASSERT( dlrec != NULL );

    	MTR_ASSERT( SWAP_16(dlrec->type) == DOWNLOAD_RECORD_TYPE_MODULE );
    	MTR_ASSERT( SWAP_16(dlrec->body.module.download_features) & 
        	DOWNLOAD_FASTMAC_INTERFACE);

        if( SWAP_16(dlrec->type) != DOWNLOAD_RECORD_TYPE_MODULE ||
            !(SWAP_16(dlrec->body.module.download_features) &
              DOWNLOAD_FASTMAC_INTERFACE) ){
                cmn_err(CE_WARN, "mtr%d: failed to download: "
                        "rec_type: 0x%x, 0x%x!\n",
                        SWAP_16(dlrec->type),
                        SWAP_16(dlrec->body.module.download_features));
                error = EIO;
                goto done;
        }

	/*	Verify firmware's length.	*/
    	if( mtr->recorded_size_fmplus_array != mtr->sizeof_fmplus_array ){
		cmn_err(CE_WARN, "mtr%d: hwi_download_code(): %d != %d!",
			mtr->mif_unit, mtr->recorded_size_fmplus_array,
			mtr->sizeof_fmplus_array);
		error = EIO;
		goto done;
    	}

	/*	Verify firmware's checksum.	*/
    	for(checksum = 0, idx = 0; idx < mtr->recorded_size_fmplus_array; 
	    idx++ ){
		checksum += mtr->fmplus_image[idx] ;
    	}
    	if( (uchar_t)(checksum + mtr->fmplus_checksum) != (uchar_t)0 ){
		cmn_err(CE_WARN,
			"mtr%d: hwi_download_code(): 0x%xx + 0x%x != 0!",
			mtr->mif_unit, checksum, mtr->fmplus_checksum);
		error = EIO;
		goto done;
    	}

    	/* 	Skip the first record!	*/
    	dlrec = (DOWNLOAD_RECORD *)((uchar_t * )dlrec + SWAP_16(dlrec->length));
	idx = 0;
    	while ( SWAP_16(dlrec->length) > 0) {

		MTR_DBG_PRINT2((
			"mtr%d: hwi_download_code() idx: %d; type: %d.\n", 
			mtr->mif_unit, idx, SWAP_16(dlrec->type)));
		if ( SWAP_16(dlrec->type) == DOWNLOAD_RECORD_TYPE_DATA_32) {

	    		/* Set DIO address for downloading data 
			 * to SIF registers */
	    		hwi_eisa_set_dio_address(mtr, 
				SWAP_16(dlrec->body.data_32.dio_addr_l),
				SWAP_16(dlrec->body.data_32.dio_addr_h));
	    		for (i = 0; i < 
				SWAP_16(dlrec->body.data_32.word_count); i++) {
				OUTS_NSW(sif_data_inc_r, 
					SWAP_16(dlrec->body.data_32.data[i]));
	    		} /* for i */

			/* Read the data back to verify them.
			 */
	    		hwi_eisa_set_dio_address(mtr, 
				SWAP_16(dlrec->body.data_32.dio_addr_l),
				SWAP_16(dlrec->body.data_32.dio_addr_h));

	    		for (i = 0; i < 
				SWAP_16(dlrec->body.data_32.word_count); i++){
				if ((t = INS_NSW(sif_data_inc_r)) == 
		    		    SWAP_16(dlrec->body.data_32.data[i])) 
					continue;

		    		cmn_err(CE_WARN, 
					"mtr%d: download 0x%x != 0x%x!",
			 		mtr->mif_unit, (uint_t)t, 
					SWAP_16(dlrec->body.data_32.data[i]));
		    		error = EIO;
				goto done;
			} /* for i */ 

		} else if (SWAP_16(dlrec->type) == 
			DOWNLOAD_RECORD_TYPE_FILL_32) {

	    		/* Fill EAGLE memory with pattern */
	    		hwi_eisa_set_dio_address(mtr, 
				SWAP_16(dlrec->body.fill_32.dio_addr_l),
				SWAP_16(dlrec->body.fill_32.dio_addr_h));

	    		for (i = 0; i < 
				SWAP_16(dlrec->body.fill_32.word_count); i++) {
				OUTS_NSW(sif_data_inc_r, 
					SWAP_16(dlrec->body.fill_32.pattern));
	    		}

		} else {
	    		/* */
			cmn_err(CE_WARN,
				"mtr%d: download type: %d(0x%x)!",
				mtr->mif_unit, SWAP_16(dlrec->type),
				SWAP_16(dlrec->type));
	    		error = EIO;
			goto done;
		}

		/* 	next record */
		dlrec = (DOWNLOAD_RECORD *) 
			((uchar_t * )dlrec + SWAP_16(dlrec->length));
		idx++;
    	} /* while */

done:
	MTR_DBG_PRINT2(("mtr%d: hwi_download_code(): %s[%d].\n", 
		mtr->mif_unit, 
		(error == MTR_NO_ERROR ? "successfully done" : "failed"), 
		error));
    	return (error);
} /* hwi_download_code() */

static void	
hwi_start_eagle( mtr_info_t *mtr)
{
    	ushort_t 	*sif_adapter_control_register = mtr->sif_acl;
    	ushort_t 	tmp;

	MTR_DBG_PRINT2(("mtr%d: hwi_start_eagle().\n", mtr->mif_unit));
    	/* change halt status in the SIF register EAGLE_ACONTROL */
    	tmp = INS_NSW(sif_adapter_control_register);
    	tmp &= ~EAGLE_SIFACL_CPHALT;
    	OUTS_NSW(sif_adapter_control_register, tmp);
	return;
}

/* 	Poll for valid bring up code.  */
#define	MAX_WAIT_BRING_UP_DIAG_CNT	3
static int
hwi_get_bring_up_code( mtr_info_t *mtr)
{
	int		error = EIO;
	int		mtr_timeout_cnt, cnt;
    	uchar_t 	bring_up_code;
    	ushort_t 	*sif_interrupt_register = mtr->sif_int;

	MTR_DBG_PRINT2(("mtr%d: hwi_get_bring_up_code().\n", mtr->mif_unit));
	for(mtr_timeout_cnt = 0; 
		mtr_timeout_cnt++ < MAX_WAIT_BRING_UP_DIAG_CNT; ){

		cnt = 0;
again:
		/*	WATCH OUT:
		 *	We give up the processor but not ifnet.
		 */
		delay(HZ);
		MTR_DBG_PRINT2(("mtr%d: hwi_get_bring_up_code() delay done.\n",
			mtr->mif_unit));

    		bring_up_code = INB(sif_interrupt_register);
    		if ((bring_up_code & EAGLE_BRING_UP_TOP_NIBBLE) == 
			EAGLE_BRING_UP_SUCCESS) {

			error = MTR_NO_ERROR;
			break;
    		} else if ((bring_up_code & EAGLE_BRING_UP_TOP_NIBBLE) == 
        		EAGLE_BRING_UP_FAILURE) {

			bring_up_code &= EAGLE_BRING_UP_BOTTOM_NIBBLE;
			cmn_err(CE_WARN, 
				"mtr%d: BUD failure (error%d).",
		 		mtr->mif_unit, bring_up_code);
			break;
    		}

		cnt++;
		MTR_DBG_PRINT2(("mtr%d: hwi_get_bring_up_code(): %d.%d.\n", 
			mtr->mif_unit, mtr_timeout_cnt, cnt));
		if( ++cnt < 15 )goto again;

		cmn_err(CE_NOTE,
			"mtr%d: BUD: reset: %d.%d: 0x%x!",
			mtr->mif_unit, mtr_timeout_cnt, cnt, bring_up_code);
		OUTS(sif_interrupt_register, 0xff00 ); 
	} /* for */

	MTR_DBG_PRINT2(("mtr%d: hwi_get_bring_up_code(): %s[%d].\n", 
		mtr->mif_unit, 
		(error == MTR_NO_ERROR ? "successfully done" : "failed"), 
		error));
	return( error );
} /* hwi_get_bring_up_code() */

/* put 32 bit DIO addr into SIF DIO addr and extended DIO addr regs
 * extended addr reg should be loadded first
 */

static void	
hwi_eisa_set_dio_address(
	mtr_info_t *mtr,
	ushort_t dio_addr_l,
	ushort_t dio_addr_h)
{
    	ushort_t   * sif_dio_adr	  = mtr->sif_adr;
    	ushort_t   * sif_dio_adrx  = mtr->sif_adx;

    	/*
    	 * Note EISA cards have single page of all SIF registers, hence do not
	 * need to page in certain SIF registers.
	 */
    	OUTS_NSW(sif_dio_adrx, (ushort_t)dio_addr_h);
    	OUTS_NSW(sif_dio_adr,  (ushort_t)dio_addr_l);
	return;
}

static void	
hwi_copy_to_dio_space(
	mtr_info_t *mtr,
	ushort_t	dio_location_l,
	ushort_t	dio_location_h,
	uchar_t		*download_data,
	ushort_t	data_length_bytes)
{
    	int	i;

    	hwi_eisa_set_dio_address(mtr, dio_location_l, dio_location_h);

    	/*
   	  * Copy data into DIO space.
    	 */
    	for (i = 0; i < data_length_bytes - 1; i += 2) {
		OUTS_NSW(mtr->sif_datinc, *(ushort_t * )(download_data + i));
    	}

    	return;
} /* hwi_copy_to_dio_space() */


static int
hwi_get_node_address_check(mtr_info_t *mtr)
{
    	int	i, error = MTR_NO_ERROR;

    /*
     * Get the permanent node address from the STB in DIO space.
     * Record address in adapter structure.
     */

    	OUTS_NSW(mtr->sif_adr, (ushort_t)(__psunsigned_t) 
		&mtr->stb_dio_addr->permanent_address);

    	for (i = 0; i < sizeof(NODE_ADDRESS); i += 2) {
		*(ushort_t * )(&mtr->mtr_bia[i]) = INS_NSW(mtr->sif_datinc);
    	}

    /*
     * Check that the node address is a	valid Madge node address.
     */

    	if ((mtr->mtr_bia[0] != MADGE_NODE_BYTE_0) || 
            (mtr->mtr_bia[1] != MADGE_NODE_BYTE_1) || 
            (mtr->mtr_bia[2] != MADGE_NODE_BYTE_2)) {

		cmn_err(CE_WARN, 
			"mtr%d: not valid madge node address: 0x%x:%x:%x.",  
			mtr->mif_unit, mtr->mtr_bia[0], 
			mtr->mtr_bia[1], mtr->mtr_bia[2]);
		error = EIO;
    	} else {
    		bcopy(mtr->mtr_bia, mtr->mif_enaddr, sizeof(mtr->mif_enaddr));
	}

    	return error;
} /* hwi_get_node_address_check() */


static int
hwi_get_init_code(mtr_info_t *mtr)
{
	int		error = EIO;
    	UINT		index;
    	uchar_t		init_code;
    	ushort_t	*sif_interrupt_register = mtr->sif_int;

	IFNET_UNLOCK( mtr_info_2_ifnet(mtr) );
    	for (index = 0; index < 100; index++) {
		/*
		 * Get initialization code from	SIFSTS register.
		 */

		init_code = INB(sif_interrupt_register);

		/*
		 * Check for successful	initialization in top nibble of	SIFSTS.
		 * Success is shown by INITIALIZE, TEST	and ERROR bits 
		 * being zero.
		 */

		if ((init_code & EAGLE_INIT_SUCCESS_MASK) == 0) {
			error = MTR_NO_ERROR;
			goto done;
		}

		/*
		 * Check for failed initialization in top nibble of SIFSTS.
		 * Failure is shown by the ERROR bit being set.
		 */

		if ((init_code & EAGLE_INIT_FAILURE_MASK) != 0) {
	    		/*
	    		 * Get actual init error code from bottom nibble of 
			 * SIFSTS.
	    	 	 */

	    		init_code = 	init_code & EAGLE_INIT_BOTTOM_NIBBLE;
	    		cmn_err(CE_WARN, "mtr%d: init failed code = 0x%x.", 
				mtr->mif_unit, init_code);
	
			goto done;
		}

		us_delay(250000);
    	}

    	/*
    	 * At least	11 seconds have	gone so	return time out	failure.
    	 */
	IFNET_LOCK( mtr_info_2_ifnet(mtr) );
done:
    	return error;
} /* hwi_get_init_code() */

static void
mtr_get_open_status(mtr_info_t *mtr)
{
	int	cnt = 0;

	for(cnt = 0; cnt < 16; cnt++){
		OUTS_NSW(mtr->sif_adr, (ushort_t) (__psunsigned_t) 
			& mtr->stb_dio_addr->adapter_open);

		mtr->stb.adapter_open	= (WBOOLEAN) INS_NSW(mtr->sif_datinc);
		mtr->stb.open_error	= INS_NSW(mtr->sif_datinc);

		if ( mtr->stb.open_error != EAGLE_OPEN_ERROR_SUCCESS ||
		     mtr->stb.adapter_open ) {
			break;
		}

		us_delay(250000);
	}
	return;
}

static void
mtr_snoop_input(mtr_info_t *mtr, 
	struct mbuf *pkt_mh_p, 	/* mbuf chain for the packet */
	int sri_len, 		/* length of RIF */
	int pkt_len)		/* length of pakcet, including all hdrs. */
{
	/*
	 *	|<-------------------  hdr_len ---------------->|
	 *	|		|				|
	 *	|<--- h_off --->|<------- hlen ---------------->|
	 *	|		|				|
	 *	v		v				v
	 *
	 *	+---------------+-------------------------------+---->
	 *	| TRPAD	|padding| MAC, RFI, LLC, SNAP		| data
	 *	+---------------+-------------------------------+---->
	 *	
	 *	^	^					^
	 *	|	|					|
	 *	+-------+---------------------------------------+
	 *		|
	 *		MUST at 32-bit boundary.
	 *
	 *	RFI only in some packets and its size must be
	 *	in byte and multiple of 2 and less than 18. 
	 *	Because of it, the size header is _NOT_ fixed.
	 *	That gives some trouble for using rawif because
	 *	it assumes a fixed size header!
	 *
	 *	The solution here is to also provide RFI with
	 *	the maximum length, sizeof(TR_RII).
	 *
	 */

	struct mbuf	*mh_p;	/* hdr of mbuf chain for snoop*/
	struct mbuf	*mt_p;	/* tail of mbuf chain for snoop*/
	int		done_len;
	int		hdr_len;	/* TRPAD, pad and hlen */
	int		h_off;		/* offset skipping TRPAD & pad*/
	int		start_off;
	int		hlen;		/* len of hdr in packet */
	int		dlen, dlen2;	/* len of data in packet */
	int		clen;		/* copy size */
	caddr_t		dst;
	TR_MAC		*mac_p;
	LLC1		*llc_p = NULL;

	if( (mh_p = m_get(M_DONTWAIT, MT_DATA)) == NULL)goto snoop_done;
	bzero( mtod(mh_p, void *), MLEN );

	hlen	= TR_RAW_MAC_SIZE + sizeof(LLC1) + sizeof(SNAP);
	hdr_len = mtr->rcvpad + hlen + RAW_HDRPAD( (mtr->rcvpad + hlen) );
	h_off	= hdr_len - hlen;

	/*	MAC:	*/
	start_off	=  mtr->rcvpad;
	dst		=  mtod(mh_p, caddr_t) + h_off;
	mac_p		=  (TR_MAC *)(mtod(pkt_mh_p, caddr_t) + start_off);
	m_datacopy(pkt_mh_p, start_off, sizeof(TR_MAC), dst);
	start_off	+= sizeof(TR_MAC);
	dst		+= sizeof(TR_MAC);
	dlen		=  pkt_len - sizeof(TR_MAC);

	/*	RIF:	*/
	if( sri_len != 0 ) m_datacopy(pkt_mh_p, start_off, sri_len, dst);
	start_off	+= sri_len;	
	dst		+= sizeof(TR_RII); 	/* skip TR_RII */
	dlen		-= sri_len;

	/*      LLC1 and SNAP, if they exists   */
	if( (mac_p->mac_fc & TR_FC_TYPE) == TR_FC_TYPE_LLC ){
		llc_p = (LLC1 *)((uchar_t *)mac_p + sizeof(TR_MAC) + sri_len);
		if( llc_p->llc1_dsap == TR_IP_SAP ){
			/*      LLC and SNAP */
			clen = sizeof(LLC1) + sizeof(SNAP);
		} else {
			/*      LLC but not a SNAP, LLC1 is data, not hdr */
			clen = 0;
		}
	} else {
		/* 	MAC but no LLC.	*/
		clen = 0;
	}
	if( clen ) m_datacopy(pkt_mh_p, start_off, clen, dst);
	start_off	+= clen;
	dst		+= clen;
	dlen		-= clen;
	dlen2		=  dlen;

	/*	data for first mbuf	*/
	clen		=  min((pkt_len - start_off), (MLEN-h_off-start_off));
	dst		=  mtod(mh_p, caddr_t) + hdr_len;
	start_off       =  mtod(pkt_mh_p, struct ifheader *)->ifh_hdrlen;
	done_len	=  m_datacopy(pkt_mh_p, start_off, clen, dst);
	mh_p->m_len	=  hdr_len + done_len;
	start_off	+= done_len;
	dlen		-= done_len;

	IF_INITHEADER(mtod(mh_p, char * ), &mtr->mif, hdr_len);

	mt_p		= mh_p;
	/*	copy the rest of data */
	while( dlen > 0 ){
		struct mbuf	*m_p;

		clen = (dlen > MCLBYTES) ? MCLBYTES : dlen;
		m_p = m_vget(M_DONTWAIT, clen, MT_DATA);
		if( m_p == NULL ){
			cmn_err(CE_NOTE, 
				"mtr%d: mtr_snoop_input: m_vget(%d).",
				mtr->mif_unit, clen);

			/* free all the mbufs we got so far for the packet. */
			goto snoop_done;
		}

		done_len = m_datacopy(pkt_mh_p, 
			start_off, clen, mtod(m_p, caddr_t));
		m_p->m_len = done_len;

		m_p->m_next = NULL;
		if( mh_p->m_next == NULL ) mh_p->m_next = m_p;
		mt_p->m_next	= m_p;
		mt_p 		= m_p;

		start_off	+= done_len;
		dlen		-= done_len;
	} /* while */

	snoop_input( &(mtr->mtr_rawif), (int)0 /* snoopflags */, 
		(caddr_t)(mtod(mh_p, caddr_t) + h_off), mh_p, dlen2);

snoop_done:
	if( mh_p != NULL )m_freem(mh_p);			

done:
	return;
} /* mtr_snoop_input() */

static void
mtr_drain_input(mtr_info_t *mtr,
        struct mbuf     *pkt_mh_p,
        int             pkt_len,
        int             sri_len,
        u_int           port)
{
        struct mbuf             *mh_p, *mt_p;
        int                     done_len;
        int                     hdr_size;
        int                     hdr_len;
        int                     h_off;
        int                     h_len;
        int                     start_off;
        int                     hlen;
        int                     dlen, dlen2;
        int                     clen;
        caddr_t                 dst;
        TR_MAC                  *mac_p;
        struct snoopheader      sh;

        MTR_ASSERT( pkt_mh_p != NULL );
        if( (mh_p = m_get(M_DONTWAIT, MT_DATA)) == 0 )goto drain_done;
        bzero( mtod(mh_p, void *), MLEN );      /* overkill! */
        mh_p->m_next = 0;

        hdr_size= TRPAD - sizeof(struct snoopheader);
        hlen    = TR_RAW_MAC_SIZE + sizeof(LLC1) + sizeof(SNAP);
        hdr_len = hdr_size + hlen + RAW_HDRPAD( (hdr_size + hlen) );
        h_off   = hdr_len - hlen;

        /*      MAC:    */
        start_off       =  TRPAD;       /* src pkt always has TRPAD */
        dst             =  mtod(mh_p, caddr_t) + h_off;
        m_datacopy(pkt_mh_p, start_off, sizeof(TR_MAC), dst);
        mac_p           =  (TR_MAC *)dst;
        start_off       += sizeof(TR_MAC);
        dst             += sizeof(TR_MAC);
        dlen            =  pkt_len - sizeof(TR_MAC);

        /*      RIF:    */
        if( sri_len != 0 ) m_datacopy(pkt_mh_p, start_off, sri_len, dst);
        start_off       += sri_len;
        dst             += sizeof(TR_RII);
        dlen            -= sri_len;

        /*      LLC1 and SNAP: never have!      */
        dlen2           =  dlen;        /* real data size only, no hdr */
        MTR_ASSERT( dlen2 > 0 );        /* data size should never 0 */

        /*      fill the snoopheader            */
        sh.snoop_seq            = mtr->mtr_drain_seq++;
        sh.snoop_flags          = 0;
        sh.snoop_packetlen      = (dlen2 + sizeof(sh));
	/*	Shameless steal from snoop_input() in bsd/net/raw.c */
#if _MIPS_SZLONG == 64
        irix5_microtime(&sh.snoop_timestamp);
#else   /* _MIPS_SZLONG == 64 */
        microtime(&sh.snoop_timestamp);
#endif  /* _MIPS_SZLONG == 64 */

        dst             =  mtod(mh_p, caddr_t) + hdr_len;
        MTR_ASSERT( ((__psunsigned_t)dst & 0x3) == 0);
        MTR_ASSERT( sizeof(sh) == 16 );
        bcopy( &sh, dst, sizeof(sh) );
        dst             += sizeof(sh);

        /*      data for first mbuf
         *      src, start_off: skipping hdr in pkt_mh_p, set by IF_INITHEADER()         *      dst, dst: skipping hdr and snoop_header in mh_p,
         *              determined by hdr_len and sizeof(sh).
         *      len, clen: the minimum size of the data and the left space in
         *              mbuf.
         */
        MTR_ASSERT( ((__psunsigned_t)dst & 0x3) == 0);
        clen            =  min( dlen2, (MLEN - hdr_len - sizeof(sh) ));
        start_off       =  mtod(pkt_mh_p, struct ifheader *)->ifh_hdrlen;
        done_len        =  m_datacopy(pkt_mh_p, start_off, clen, dst);
        mh_p->m_len     =  hdr_len + done_len + sizeof(sh);
        start_off       += done_len;
        dlen            -= done_len;

	IF_INITHEADER(mtod(mh_p, char * ), &mtr->mif, hdr_len);

        mt_p            = mh_p;
        while( dlen > 0 ){
                struct mbuf     *m_p;

                clen = (dlen > MCLBYTES) ? MCLBYTES : dlen;
                m_p = m_vget(M_DONTWAIT, clen, MT_DATA);
                if( m_p == NULL ){
                        /* free all the mbufs we got so far for the packet. */
                        if( mh_p )m_freem(mh_p);
                        goto drain_done;
                }

                done_len = m_datacopy(pkt_mh_p,
                        start_off, clen, mtod(m_p, caddr_t));
                m_p->m_len = done_len;

                m_p->m_next = NULL;
                if( mh_p->m_next == NULL ) mh_p->m_next = m_p;
                mt_p->m_next    = m_p;
                mt_p            = m_p;

                start_off       += done_len;
                dlen            -= done_len;
        } /* while */

#ifdef  DEBUG2
        MTR_DBG_PRINT2( (
		"mtr%d: drain_input(0x%x, 0x%x, 0x%x, 0x%x): %d.%d.%d.%d "
                "0x%x.0x%x.%d.\n",
                mtr->mif_unit, &(mtr->mtr_rawif), port,
                mac_p->mac_sa, mh_p, hdr_size, hdr_len, h_off, hlen,
                mh_p, mh_p->m_next, m_length(mh_p)));
#endif  /* DEBUG2 */

        drain_input( &(mtr->mtr_rawif), port, (caddr_t)mac_p->mac_sa, mh_p);

drain_done:
        if( pkt_mh_p )m_freem(pkt_mh_p);
        return;
} /* mtr_drain_input() */

static void
report_Configuration( mtr_info_t *mtr )
{
	uchar_t		*addr, *addr2;
	uchar_t		*mtr_baddr;

	addr	= (uchar_t *)mtr->mtr_bia;
	addr2	= (uchar_t *)mtr->mif_enaddr;

	cmn_err(CE_CONT, "mtr%d: ftk_version_for_fmplus: %d, "
		"fmplus_version:\"%s\".\n",
		mtr->mif_unit, ftk_version_for_fmplus, fmplus_version);
	cmn_err(CE_CONT, "mtr%d: tx_slots: %d, rx_slots: %d, "
		"ring_speed: %dMB, ram_size: %dKB.\n",
		mtr->mif_unit, 
		mtr->init_block->fastmac_parms.tx_slots,
		mtr->init_block->fastmac_parms.rx_slots,
		mtr->mtr_s16Mb ? 16 : 4, 
		256 /* TO DO: find out ram size. */);
	cmn_err(CE_CONT, "mtr%d: burnin address(BIA): 0x%x:%x:%x:%x:%x:%x, "
		"open address: 0x%x:%x:%x:%x:%x:%x.\n",
		mtr->mif_unit,	
		addr[0], addr[1], addr[2],
		addr[3], addr[4], addr[5] & 0x0ff,
		addr2[0], addr2[1], addr2[2],
		addr2[3], addr2[4], addr2[5] & 0x0ff);
	cmn_err(CE_CONT, "mtr%d: broadcast address: 0x%x:%x:%x:%x:%x:%x, "
		"default sri: 0x%x.\n",
		mtr->mif_unit,
		mtr->mtr_baddr[0], mtr->mtr_baddr[1],
		mtr->mtr_baddr[2], mtr->mtr_baddr[3],
		mtr->mtr_baddr[4], mtr->mtr_baddr[5] & 0x0ff,
		mtr->nullsri);

	return;
} /* report_Configuration() */

ushort_t ftk_version_for_fmplus = 223;
char fmplus_version[]       = " Madge Fastmac Plus v1.61 ";
char fmplus_copyright_msg[] = "Copyright (c) Madge Networks Ltd 1988-1996. All rights reserved.";

#endif /* IP22 || IP26 || IP28 */
