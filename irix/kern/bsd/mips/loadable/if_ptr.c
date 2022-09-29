/* 
 *	Copyright 1996 Silicon Graphics (SGI), Inc. All rights reserved.
 *
 *	The driver for PCI token ring adapter from Madge.
 *
 */
#ident	"$Revision: 1.7 $"

#if	defined(IP30) || defined(IP32) || defined(SN0) 	/* IP30, IP32, SN0 */

#define	PCI_PCI2_DEVICE_ID		0x2		/* from hwi/pci2.c */
#define	PCI_PCIT_DEVICE_ID		0x4		/* from hwi/pcit.c */
#define	USE_PCI_IO_ADDR_SPACE		/* use PCI IO space! */
#define	ARB_NEEDED		/* by default, yes. */
#undef	LARGE_DMA_INTR_REQUIRED		/* TX large buffer interrupt.	*/
#define	RUN_TIME_DEBUG_CONTROL		/* debug msg by 'ifconfig debug' */
#define	PIO_VIA_ROUTINES		/* pio via in[bw]() and out[bw](). */
#undef	DWORD_DIO_ADDR		/* not use DWORD_DIO_ADDR in ftk_down.h */

/*	Unfortunely, we need to select the right byte lane for 
 *	the difference of big-endian and little-endian.		
 */
#define SGI_EAGLE_SIFDAT            2	/* DIO data                 */
#define SGI_EAGLE_SIFDAT_INC        0	/* DIO data auto-increment  */
#define SGI_EAGLE_SIFADR            6	/* DIO address (low)        */
#define SGI_EAGLE_SIFINT            4	/* interrupt SIFCMD-SIFSTS  */
#define SGI_EAGLE_SIFACL            10	/* Adapter Control */
#define SGI_EAGLE_SIFADX            14	/* DIO address (high) */
#define	SGI_PCI2_INTERRUPT_STATUS   2
#define	SGI_PCI2_INTERRUPT_CONTROL  1	
#define	SGI_PCI2_RESET		    7
#define	SGI_PCI2_SEEPROM_CONTROL    4
#define	SGI_PCI2_EEPROM_CONTROL	    8

#include <sys/types.h>
#include <sys/param.h>
#include <sys/file.h>
#include <sys/errno.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/mload.h>
#include <sys/param.h>
#include <sys/ddi.h>
#include <sys/kmem.h>
#include <sys/ioctl.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/cpu.h>
#include <sys/sbd.h>
#include <sys/edt.h>
#include <sys/pio.h>
#include <sys/pci_intf.h>
#include <sys/PCI/pciio.h>
#include <sys/PCI/PCI_defs.h>
#include <sys/sema.h>
#include <sys/dmamap.h>
#include <sys/atomic_ops.h>
#include <sys/iograph.h>

typedef struct {
        u_short scb_cmd;                        /* SCB command */
        u_short scb_parm1;                      /* first parameter */
        u_short scb_parm2;                      /* second parameter */
        u_char  scb_pad[2];            		/* pad space */
}SCB;

#include <sys/hashing.h>
#include <sys/tr.h>
#include <sys/llc.h>
#include <sys/tr_user.h>
#include <net/netisr.h>
#include <sys/socket.h>
#include <sys/dlsap_register.h>
#include <sys/tcp-param.h>

#include <sys/mbuf.h>
#include <net/if.h>
#include <net/raw.h>
#include <net/soioctl.h>
#include <sys/misc/ether.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/igmp_var.h>
#include <netinet/if_ether.h>
#include <sys/invent.h>
#include <sys/mips_addrspace.h>		
#include <net/if_types.h>
#if	IP32
#include <sys/IP32.h>
#endif /* IP32 */


#define	FMPLUS			/* MUST! */
#undef  ssb_addr                /* reset the one defined in ... */
#include <sys/if_mtr.h>

#undef	MAC_FRAME_SIZE
#undef	MAX_FRAME_SIZE_4_MBITS
#undef	MAX_FRAME_SIZE_16_MBITS
/* 	MAC frame size: AC(1), FC(1), addrs(12), RIF(<18), FCS(4),
 *	which is different from the one defined in ftk_card.h, 39. */
#define	MAC_CRC_SIZE			4
#define	MIN_MAC_FRAME_SIZE		sizeof(TR_MAC) + MAC_CRC_SIZE
#define	MAX_MAC_FRAME_SIZE		(MIN_MAC_FRAME_SIZE + sizeof(TR_RII))
#define	MAX_FRAME_SIZE_4_MBITS		(TRMTU_4M +  MAX_MAC_FRAME_SIZE)
#define	MAX_FRAME_SIZE_16_MBITS		(TRMTU_16M + MAX_MAC_FRAME_SIZE)

#undef	DEBUG				/* high-level debugging msg */
#undef	DEBUG2				/* intensive low-level debugging msg */
#undef 	DEBUG_PIO			/* intensive PIO debugging msg */
#undef	DEBUG_INITIALIZATION		/* debugging the init process. */

#ifdef	DEBUG
#include <sys/PCI/pciio_private.h>
#endif	/* DEBUG */

#define BEING_TESTED

extern ushort_t	mtr_bc_mask;
extern int	mtr_mtu;
extern int	mtr_batype;
extern uint_t	if_ptr_cnt;
extern int	mtr_s16Mb;
extern int	mtr_max_rx_slots, mtr_max_tx_slots;
extern int      mtr_arb_enabled;	/* get ARB or not	*/
extern int      mtr_participant_claim_token;	/* to compete as a AM or not */
extern int	if_ptr_loaded;

/*	
 *	As the driver recognizes all the adapters it can handle, 
 *	only one instance of the driver can be loaded at one time.
 *
 */
extern struct ifnet 	loif;
extern struct ifqueue 	ipintrq;

#define	SCACHELINE		(scache_linemask + 1)
#define	CACHELINE_128_BYTE	0x20	/* 32*4 bytes */
#define	CACHELINE		(sizeof(uint_t) * CACHELINE_128_BYTE)
#define	LATENCY_64_PCICLK	0x30	/* Moosehead needs at least 0x20 */
#define	CMD_BUSMSTR_ENBL	0x4	/* Bus Master. */
#define	CMD_MEM_ENBL		0x2	/* Memory Space Access */
#define	CMD_IO_ENBL		0x1	/* IO space Access */

/* only 32bit PCI is supported by the adapter.	*/
typedef	__uint32_t	pciaddr_t;	/* 32 bit PCI address */

/*
 *	Verify the protection in INTR_KTHREADS and 
 *	non-INTR_KTHREADS environment:
 *
 *	In non-INTR_KTHREADS, the driver should be protected 
 *	in the splimp() level.
 *
 *	In INTR_KTHREADS, the driver should acquire the ifnet lock 
 *	before it can process the request for the corresponding adapter. 
 *
 */
#ifdef DEBUG
#define       MTR_VERIFY_PROTECTION(ifp)                              \
      MTR_ASSERT( IFNET_ISLOCKED(ifp) )
#else
#define MTR_VERIFY_PROTECTION(ifp)  
#endif

/* 	
 *	The following variables are required by DLM (Dynamic Loadable Module).
 */
char 	*if_ptrmversion = M_VERSION;
int	if_ptrdevflag   = 0;

int    kvp_cnt = 0;
/*
 *	The convention is the routine returns an error code defined in
 *	sys/errno.h for the error. Otherwise, a '0' is returned.
 */
#define	NO_ERROR			0

/*	TO DO: multiple bridges and multiple PCI bus!	*/
#define	NUM_SLOTS			8

#define	MAKE_ALIGNMENT( _size, _align )					\
	( (((_size) & ((_align) - 1)) != 0) ? 			\
	  (((_size) + (_align)) & ~((_align) - 1)) :			\
	  (_size) )

/*	
 *	In cases other than TX and RX, the memory for bus-master operation
 *	comes from KSEG0 or KSEG1 and at dcacheline boundary. As the
 *	memory can be requested in interrupt stack, no sleep is allowed.
 *
 *	The memory for TX and RX are allocated from mbuf because the
 *	upper layer uses mbuf to send and receive the data.
 */
#define	MTR_KMEM_ARGS							\
	(KM_NOSLEEP | VM_DIRECT | KM_CACHEALIGN | KM_PHYSCONTIG)

/*	
 *	Swapping 16-bit or 32-bit.	
 */
#define	SWAP_16(_s)							\
	((((_s) & 0xff) << 8) | (((_s) >> 8) & 0xff))
#define SWAP_32(_w)							\
	((((_w) & 0xff000000) >> 24) | (((_w) & 0x000000ff) << 24) |	\
	 (((_w) & 0x00ff0000) >>  8) | (((_w) & 0x0000ff00) <<  8))

/*	
 *	It is based on PCI's specification 2.0. Mainly for debugging.
 */
#define	PCI_BASE_REGISTERS	6  /* PCI 2.0 */
#define	MTR_PCI_BASE_REGISTERS	2  /* only the first two are used. */
typedef struct PCI_cfg_hdr_s {
	ushort_t	device_id;
	ushort_t	vendor_id;
	ushort_t	pci_status;
	ushort_t	pci_command;
	uint_t		class_code:24,
			revision_id:8;
	uchar_t		reserved1;
	uchar_t		header_type;
	uchar_t		latency_timer;
	uchar_t		cache_line_size;
	uint_t 		base_addr[PCI_BASE_REGISTERS];	
	uint_t		reserved3;
	uint_t		reserved4;
	uint_t		expension_rom_base_addr;
	uint_t		reserved5;
	uint_t		reserved6;
	uchar_t		max_lat;
	uchar_t		min_gnt;
	uchar_t		interrupt_pin;
	uchar_t		interrupt_line;
} PCI_cfg_hdr_t ;

typedef struct arb_general_s {
	SRB_HEADER		header;
	WORD			reserved;
	WORD			status;
} arb_general_t;

#define	EAGLE_SRB_CMD_CODE		0xA0FF
#define	DIO_LOCATION_EAGLE_DATA_PAGE1	0x0001
#define	EAGLE_ARB_FUNCTION_CODE		0x84

/*
 *	Adapter's information.
 */
typedef struct mtr_adapter_s {
	uint_t			memory_size;
	uint_t			mem_io_register;

	/*	adapter's PCI configuration information.	*/
	PCI_cfg_hdr_t		pci_cfg_hdr;
	uint_t 			org_base_addr[PCI_BASE_REGISTERS];	

	int			mtr_s16Mb;	/* speed for the adapter. */

	/*
	 *	The following is for the memory address of SIF registers.
	 *	
	 */
	WORD		*sif_dat;
	WORD		*sif_datinc;
	WORD		*sif_adr;
	WORD		*sif_int;
	WORD		*sif_acl;
	WORD		*sif_adrx;

	FASTMAC_STATUS_BLOCK	stb;		/* Status Block: STB */

	SRB_GENERAL		srb_general;	/* to adapter */
	SRB_GENERAL		srb_general2;	/* from adapter */

	arb_general_t		arb;	      /* Adapter Request Block: ARB */

	SRB_HEADER		*srb_dio_addr;	/* addr of SRB in DIO space */
	arb_general_t		*arb_dio_addr;	/* addr of ARB in DIO space */
	FASTMAC_STATUS_BLOCK	*stb_dio_addr;	/* addr of STB in DIO space */

	/*	
	 *	They are the addresses of the sets of 'slot'
	 *	in the adapter. 
	 *	The driver get them during the initialization.
	 */
	RX_SLOT			*rx_slot_array[FMPLUS_MAX_RX_SLOTS];
	TX_SLOT			*tx_slot_array[FMPLUS_MAX_TX_SLOTS];

	/*	Keep track the next slot for transmitting or receiving.	*/
	ushort_t		active_rx_slot;
	ushort_t		active_tx_slot;

	/*	
	 *	They are the addresses of the kernel data buffers 
	 *	associated with each slot.
	 *	Those buffers are allocated at initialization time
	 *	and their address is given the adapter at 
	 *	initialization.
	 */
	caddr_t			rx_slot_buf[FMPLUS_MAX_RX_SLOTS];	
	pciaddr_t		rx_slot_phys[FMPLUS_MAX_RX_SLOTS];
	caddr_t			tx_slot_buf[FMPLUS_MAX_TX_SLOTS];	
	pciaddr_t		tx_slot_phys[FMPLUS_MAX_TX_SLOTS];
	uint_t			rx_tx_buf_size;		/* each in bytes */
	uint_t			all_rx_tx_bufs_size;	/* total in pages */
	caddr_t			rx_tx_bufs_addr;

	/*
	 *	For SCB.	This is the first: followed by SSB.
	 */
	int			scb_size;
	caddr_t			scb_buff;
	pciaddr_t		scb_phys;
	/*
	 *	For SSB.
	 */
	int			test_size;
	int			ssb_size;
	caddr_t			ssb_buff;
	pciaddr_t		ssb_phys;

	caddr_t			broken_dma_handshake_addr;
	DWORD			broken_dma_handshake_phys;
	WORD			broken_dma;

	INITIALIZATION_BLOCK	*init_block_p;

	int			addrset;
	int			naddrset;

	uint_t			ram_size;
	/* as pci2 has abnormal way to select speed we need this.	*/
	WORD			nselout_bits;	

	BYTE			bEepromByteStore;	
	BYTE			bLastDataBit;

	/* ftk 223 addition */
	ushort_t                at24_access_method;
	uchar_t                 eeprom_byte_store;

	uchar_t			mcl[10];	/* Micro code level */
	uchar_t			pid[18];	/* Product ID */
	uchar_t			sid[4];		/* station ID */

	ushort_t		nullsri;	/* default bcast sri */

	uint_t			functional_address;
	int			ip_multicast_cnt;

        uint_t                  group_address;
        uint_t                  new_group_addr;

	uchar_t			*fmplus_image;
	ushort_t		sizeof_fmplus_array;
	ushort_t		recorded_size_fmplus_array;
	uchar_t			fmplus_checksum;

} mtr_adapter_t;
#define		mtr_dev_id		adapter.pci_cfg_hdr.device_id
#define		mtr_vendor_id		adapter.pci_cfg_hdr.vendor_id
#define		pci_cfg			adapter.pci_cfg_hdr
#define		org_pci_cfg_base_addr	adapter.org_base_addr
#define		mtr_nselout_bits	adapter.nselout_bits

/*
 *	System PCI information for the adapter.
 */
typedef struct sys_pci_s {
	volatile caddr_t	cfgaddr;
	volatile caddr_t	memaddr;

	pciio_info_t		info_p;

	pciio_slot_t		slot;

	pciio_intr_t		intr_handle;
} sys_pci_t;
#define		pci_cfg_base_addr	pci_cfg.base_addr
	
/*
 *	System information.
 */
typedef struct sys_io_s {
	device_desc_t		dev_desc;	

	pciio_piomap_t		io_piomap;
	pciio_piomap_t          cfg_piomap;
	pciio_dmamap_t		dmamap;
	pciio_dmamap_t		ssb_scb_dmamap;

	sys_pci_t		sys_pci;

	int			drain_seq;
} sys_io_t;
#define		mtr_dev_desc		sys_io.dev_desc
#define		mtr_mem_addr		sys_io.sys_pci.memaddr
#define		mtr_cfg_addr		sys_io.sys_pci.cfgaddr
#define		mtr_pciio_info_p	sys_io.sys_pci.info_p
#define		mtr_slot		sys_io.sys_pci.slot
#define		mtr_intr_hdl		sys_io.sys_pci.intr_handle
#define		dmamap_p		sys_io.dmamap
#define		test_dmamap_p		sys_io.ssb_scb_dmamap
#define		io_piomap_p		sys_io.io_piomap
#define         cfg_piomap_p            sys_io.cfg_piomap
#define		mtr_drain_seq		sys_io.drain_seq

/*	
 *	For each adapter recognized by the driver, an instance of the
 *	following data structure is created. As it is also used to link 
 *	with upper layer. 
 *
 */
struct if_mtr_s;
typedef struct mtr_private_s {
	struct if_mtr_s		*ifmtr_p;	/* back to ifnet */

#define	MTR_TAG			0xff00aa55
	uint_t			tag;
	sys_io_t		sys_io;

	/*	The information provided by the adapter.	*/
	mtr_adapter_t		adapter;

	uint_t			tx_queue_full;

	/* 	
	 *	Drvier is wait the init code and the ifnet lock is temporarily 
	 *	released for enabling interrupt so we use this to prevent 
	 *	another request.  
	 */
	mutex_t			init_mutex;

	/*
	 *	Only one outstanding SRB request can be given to the adapter,
	 *	so we have this mutex to guarantee that.
	 */
	int			srb_used;	/* outstanding SRB cmd */
	mutex_t			srb_mutex;	/* protecting srb_used  */

	uchar_t			mtr_baddr[TR_MAC_ADDR_SIZE];

	uint_t			intr_no_sts;
	uint_t			mtr_no_mbuf;
#ifdef	DEBUG
	uint_t			zero_dmastatus;
	uint_t			from_me_cnt;
	uint_t			intr_cnt;
	int			rx_dump_mbuf_size;
	struct mbuf		*rx_dump_mbuf;
#endif	/* DEBUG */
} mtr_private_t;

typedef struct if_mtr_s {
	struct trif		tr_if;
	vertex_hdl_t		connect_vertex;	
	mtr_private_t		*mtr_p;
	uint_t			flags;
} if_mtr_t;

/*	For mtr_private_t.flags */
#define	MTR_FLAG_UNINIT			0x0	/* nothing is done. */
#define	MTR_FLAG_INTR_DONE		0x20	/* setting up intr is done */
#define	MTR_FLAG_INTR_ALLOC		0x40	/* alloc intr is done */
#define MTR_FLAG_PIO_DONE               0x80	/* pio_map is done */

#define	MTR_FLAG_NETWORK_ATTACHED	0x100
#define	MTR_FLAG_NETWORK_REATTACHING	0x200
#define	MTR_FLAG_NETWORK_QUEUE_FULL	0x400

/*
 *	The step of initializing an adapter is controlled by the
 *	following flags: MTR_FLAGS_STATE_...
 *
 *	The state machine is:
 *
 *	halt the adapter -> 	MTR_FLAG_STATE_HALTED
 *	download the code -> 	MTR_FLAG_STATE_CODE_DOWNLOADED
 *	start the adapter ->	MTR_FLAG_STATE_STARTED
 *	initialize the adapter->MTR_FLAG_STATE_INITED
 *				IFF_ALIVE.
 *	
 */
#define	MTR_FLAG_STATE_HALTED		(0x1 << 16)
#define	MTR_FLAG_STATE_CODE_DOWNLOADED	(0x2 << 16)
#define	MTR_FLAG_STATE_STARTED		(0x4 << 16)
#define	MTR_FLAG_STATE_INITED		(0x8 << 16)
#define	MTR_FLAG_STATE_PROMISC		(0x10 << 16)
#define	MTR_IFF_FLAGS	(IFF_BROADCAST | IFF_MULTICAST | \
			IFF_ALLCAST)

#ifdef DEBUG
#define	MTR_FLAG_DEBUG_LEVEL1		(0x1 << 24)
#define	MTR_FLAG_DEBUG_LEVEL2		(0x2 << 24)
#define	MTR_FLAG_DEBUG_LEVEL_PIO	(0x4 << 24)
#define	MTR_FLAG_DEBUG			(MTR_FLAG_DEBUG_LEVEL1|MTR_FLAG_DEBUG_LEVEL2|MTR_FLAG_DEBUG_LEVEL_PIO)
#endif /* DEBUG */

#define	IS_MTR_FLAG_SET( _ptr, _flag )	(((_ptr)->ifmtr_p)->flags &  (_flag))
#define	SET_MTR_FLAG( _ptr, _flag )	(((_ptr)->ifmtr_p)->flags |= (_flag))
#define	RESET_MTR_FLAG( _ptr, _flag )	(((_ptr)->ifmtr_p)->flags &= ~(_flag))

#define	IS_MTR_IFF_SET( _ptr, _flag )					\
		(((_ptr)->mtr_if_flags & (_flag)) == (_flag))
#define	MTR_SET_IFF( _ptr, _flag )	((_ptr)->mtr_if_flags |= (_flag))
#define	MTR_RESET_IFF( _ptr, _flag )	((_ptr)->mtr_if_flags &= ~(_flag))

#define	MAX_WAIT_MTR_INIT_CODE_TIME	(10 * HZ)	/* 10 seconds */
#define	MAX_WAIT_MTR_INIT_CODE_CNT	(MAX_WAIT_MTR_INIT_CODE_TIME / HZ)

#define	MAX_TX_LEN(_mtr_p)						\
	(_mtr_p)->adapter.init_block_p->fastmac_parms.max_frame_size
#define	MAX_RX_LEN(_mtr_p)		MAX_TX_LEN(_mtr_p)

#define	mtr_2_ifnet( _ptr )	tiftoifp(&((_ptr)->ifmtr_p->tr_if))
#define	ifmtr_2_ifnet( _ptr )	( (struct ifnet *)(&((_ptr)->tr_if)) )
#define	ifnet_2_mtr( _ptr )	( ((if_mtr_t *)(_ptr))->mtr_p )
#define	ifnet_2_ifmtr( _ptr )	( (if_mtr_t *)(_ptr) )

#define	mtr_ac			ifmtr_p->tr_if.tif_arpcom
#define	mtr_ifnet		mtr_ac.ac_if
#define	mtr_unit		mtr_ifnet.if_unit
#define	mtr_if_mtu		mtr_ifnet.if_mtu
#define	mtr_if_flags		mtr_ifnet.if_flags
#define	mtr_enaddr		mtr_ac.ac_enaddr
#define	mtr_bia			ifmtr_p->tr_if.tif_bia
#define	mtr_rawif		ifmtr_p->tr_if.tif_rawif
#define	mtr_connect_vertex	ifmtr_p->connect_vertex

typedef struct tokenbufhead {
	struct ifheader		tbh_ifh;	/* 8 bytes */
	struct snoopheader	tbh_snoop;	/* 16 bytes */
} TOKENBUFHEAD;
#define	TRPAD		sizeof(TOKENBUFHEAD)
#define	TR_RAW_MAC_SIZE	sizeof(TR_MAC) + sizeof(TR_RII)

/*	
 *	Forward references:
 */
int			if_ptrattach(vertex_hdl_t);
int			if_ptrdetach(vertex_hdl_t);
static int		mtr_error( void *, int, ioerror_mode_t, ioerror_t *);

static int		mtr_reset(int, int);
static void 		mtr_watchdog(struct ifnet *);
#if IP32
static void		mtr_intr(eframe_t *ef, void *arg);	
#else
static void		mtr_intr(void *arg);
#endif /* IP32 */
static int 		mtr_output(struct ifnet *, struct mbuf *,
				struct sockaddr *, struct rtentry *);
static int 		mtr_ioctl( struct ifnet*, int, void *);

static int		mtr_reopen_adapter(mtr_private_t *, SRB_GENERAL *);
static int		mtr_modify_open_parms(mtr_private_t *);
static int		mtr_group_address( mtr_private_t *, int , caddr_t );

static void 		release_resources( mtr_private_t *);
static void 		report_Configuration( mtr_private_t *);

static int 		driver_prepare_adapter( mtr_private_t *);
static int 		driver_start_adapter(mtr_private_t *);
static void 		hwi_halt_eagle(mtr_private_t *);
static void		hwi_start_eagle(mtr_private_t *);
static void 		hwi_pci_set_dio_address(mtr_private_t *, 
			ushort_t, ushort_t);
static int 		hwi_download_code(mtr_private_t *, 
			DOWNLOAD_RECORD *);
static void		hwi_copy_to_dio_space(mtr_private_t *, 
			ushort_t, ushort_t, uchar_t *, ushort_t);
static int 		hwi_get_init_code(mtr_private_t *);
static int		hwi_pci_install_card(mtr_private_t*, 
			DOWNLOAD_RECORD *);
static int		hwi_bring_up_diag(mtr_private_t *);
static int		hwi_pci_read_node_address(mtr_private_t *);
static void 		mtr_receive(mtr_private_t *mtr_p);
static int		mtr_send(mtr_private_t *, struct mbuf *);
static void 		mtr_snoop_input(mtr_private_t *, 
					struct mbuf *, int, int );
static void		mtr_drain_input(mtr_private_t *, struct mbuf *,
					int, int, u_int);
static int 		mtr_functional_address( mtr_private_t *, int , uint_t );
static int              mtr_read_error_log(mtr_private_t *);
static void 		sys_pci_read_config_byte(mtr_private_t *, 
			uint_t , uchar_t *);
static void 		sys_pci_write_config_byte(mtr_private_t *, 
			uint_t , uchar_t);
static WORD             serial_read_word(mtr_private_t *, uchar_t);
static WORD             at24_serial_read_word(mtr_private_t *, uchar_t);
static BYTE             at24_serial_read_byte(mtr_private_t *, uchar_t);
static BYTE             at24_serial_read_bit_string(mtr_private_t *);
static void             at24_serial_write_bit_string(mtr_private_t *, uchar_t);
static BYTE             at24_in(mtr_private_t *);
static void             at24_out(mtr_private_t *, uchar_t);
static WBOOLEAN         at24_serial_send_cmd(mtr_private_t *, uchar_t);
static WBOOLEAN         at24_wait_ack(mtr_private_t *);
static WBOOLEAN         at24_dummy_wait_ack(mtr_private_t *);
static void             at24_set_clock(mtr_private_t *);
static void             at24_clear_clock(mtr_private_t *);

extern int looutput(struct ifnet *, struct mbuf *, struct sockaddr *,
	struct rtentry *);


/*	
 *	Driver's signature.
 */
static int  	loaded_flag = 0;
static uchar_t	mtr_baddr_c[] = {0xc0, 0x00, 0xff, 0xff, 0xff, 0xff};
static uchar_t	mtr_baddr_f[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

#define	MY_NET_PREFIX		"mtr"
#define	MY_PCI_PREFIX		"if_ptr"
#define	MTR_VENDOR_ID		0x10b6
#define MTR_INFO_LBL            "_ifnet"

static int 	PCI_setup_cfg(mtr_private_t *);
static void	PCI_disable_adapter( mtr_private_t *mtr_p );
static void 	PCI_get_dump_cfg( mtr_private_t *mtr_p, int dump_flag );

#ifdef	PIO_VIA_ROUTINES
	static uchar_t	inb(uchar_t *);
	static ushort_t	ins(ushort_t *);
	static void	outb(uchar_t *, uchar_t);
	static void	outs(ushort_t *, ushort_t);

	#define	INB(_addr)		inb(_addr)
	#define	INS(_addr)		ins(_addr)
	#define	OUTB(_addr, _data)	outb(_addr, _data)
	#define	OUTS(_addr, _data)	outs(_addr, _data)

#else	/* PIO_VIA_ROUTINES */
	#define	INB(_addr)	(uchar_t)(*(volatile uchar_t *)(_addr))
	#define	INS(_addr)	(ushort_t)(*(volatile ushort_t *)(_addr))
	#define	OUTB(_addr, _data)					\
		(*(volatile uchar_t *)(_addr) = (uchar_t)(_data))
	#define	OUTS(_addr, _data)					\
		(*(volatile ushort_t *)(_addr)= (ushort_t)(_data))
#endif	/* PIO_VIA_ROUTINES */

#ifdef	DEBUG

	#ifdef	RUN_TIME_DEBUG_CONTROL
	#ifdef	DEBUG_INITIALIZATION
		#define	ADAPTER_DEBUG_ENABLED( _ptr )			\
			(!(IS_MTR_FLAG_SET(_ptr, MTR_FLAG_NETWORK_ATTACHED))|| \
			 mtr_2_ifnet(_ptr)->if_flags & IFF_DEBUG)
	#else	/* DEBUG_INITIALIZATION */
		#define	ADAPTER_DEBUG_ENABLED( _ptr )			\
			(mtr_2_ifnet(_ptr)->if_flags & IFF_DEBUG)
	#endif	/* DEBUG_INITIALIZATION */
	#else	/* RUN_TIME_DEBUG_CONTROL */
		#define	ADAPTER_DEBUG_ENABLED( _ptr )	1
	#endif	/* RUN_TIME_DEBUG_CONTROL */

	#define	MTR_DUMP_PKT_SIZE					\
		sizeof(TR_MAC) + sizeof(LLC1) + sizeof(SNAP) + 64

	static void 	mtr_dump_pkt(mtr_private_t *, struct mbuf *, char *);

	static void 	mtr_dump_tx_rx_slots(mtr_private_t *, char *);

	#define	MTR_DUMP_TX_RX_SLOTS( _ptr, _msg )			\
		mtr_dump_tx_rx_slots( _ptr, _msg )

	/*	By default, if_ptr_debug is off. 
 	 *	It only controls the debugging stuffs which is
 	 *	done before the mtr_private_t is allocated and after
 	 *	the mtr_private_t is deallocated.
 	 */
	#ifdef	DEBUG_INITIALIZATION
		int	if_ptr_debug = 1;
	#else	/* DEBUG_INITIALIZATION */
		int	if_ptr_debug = 0;
	#endif	/* DEBUG_INITIALIZATION */
	static void 	mtr_assert(char *, int);

	#define	MTR_DUMP_PKT(_ptr, _ptr2, _msg)				\
		mtr_dump_pkt( _ptr, _ptr2, _msg)

	#define	IS_MTR_DEBUG( _ptr )					\
		( IS_MTR_FLAG_SET(_ptr, MTR_FLAG_DEBUG_LEVEL1) &&	\
		 ADAPTER_DEBUG_ENABLED( _ptr ))
	#define	MTR_DBG_PRINT(_ptr, _msg)				\
		if( IS_MTR_DEBUG(_ptr) ) printf _msg
	#define	MTR_ASSERT( _COND )					\
		((_COND) ? ((void) 0) : mtr_assert(__FILE__, __LINE__))
	static void 	dump_vertex( vertex_hdl_t vertex );
	#define	DUMP_VERTEX( _vertex_ptr )	dump_vertex( _vertex_ptr )

	/*	INIT_MTR_DBG_PRINT() is only used 
	 *	when mtr_private_t is not available yet.
	 *	Otherwise, MTR_DBG_PRINT(), IS_MTR_DEBUG() or
	 *	IS_MTR_DEBUG2() should be used.
	 */
	#ifdef	DEBUG_INITIALIZATION
	#define	INIT_MTR_DBG_PRINT(_msg)	if( if_ptr_debug ) printf _msg
	#ifdef  DEBUG2
		#define	INIT_MTR_DBG_PRINT2(_msg) if(if_ptr_debug) printf _msg
	#else   /* DEBUG2 */
		#define	INIT_MTR_DBG_PRINT2(_msg)
	#endif	/* DEBUG2 */
	#else	/* DEBUG_INITIALIZATION */
	#define	INIT_MTR_DBG_PRINT(_msg)
	#endif	/* DEBUG_INITIALIZATION */

	#ifdef	DEBUG2
		#define	MTR_VERIFY_ADX( _ptr )				\
		{							\
			volatile uint_t	adx; 				\
			adx = INS( _ptr->adapter.sif_adrx );		\
			adx <<= 16;					\
			MTR_ASSERT( adx == DIO_LOCATION_EAGLE_DATA_PAGE );\
		}
		#define	IS_MTR_DEBUG2( _ptr )				\
			(IS_MTR_FLAG_SET((_ptr), MTR_FLAG_DEBUG_LEVEL2) && \
			 ADAPTER_DEBUG_ENABLED( _ptr ))
		#define	MTR_DBG_PRINT2(_ptr, _msg)			\
			if( IS_MTR_DEBUG2(_ptr) ) printf _msg
	#else	/* DEBUG2 */
		#define	MTR_VERIFY_ADX( _ptr )
		#define	IS_MTR_DEBUG2( _ptr )	0
		#define	MTR_DBG_PRINT2(_ptr, _msg)
		#define	INIT_MTR_DBG_PRINT2(_msg)
	#endif	/* DEBUG2 */

	#ifdef	DEBUG_PIO
		#define	IS_MTR_DEBUG_PIO( _ptr )			\
			(IS_MTR_FLAG_SET((_ptr), MTR_FLAG_DEBUG_LEVEL_PIO) && \
			 ADAPTER_DEBUG_ENABLED( _ptr ))
		#define	MTR_DBG_PRINT_PIO(_ptr, _msg)			\
			if( IS_MTR_DEBUG_PIO(_ptr) ) printf _msg
		#define	INIT_MTR_DBG_PRINT_PIO( _msg )			\
			if( if_ptr_debug ) printf _msg
	#else	/* DEBUG_PIO */
		#define IS_MTR_DEBUG_PIO( _ptr )	0
		#define MTR_DBG_PRINT_PIO(_ptr, _msg)
		#define	INIT_MTR_DBG_PRINT_PIO( _msg )
	#endif	/* DEBUG_PIO */

	#define MTR_HEX_DUMP(_mtr_p, _msg, _block)			\
	{								\
		int		cnt;					\
		ushort_t	*ptr;					\
									\
		MTR_DBG_PRINT( (_mtr_p), ("mtr%d: %s: 0x",		\
			(_mtr_p)->mtr_unit, (_msg)));			\
		ptr = (ushort_t *)&(_block);				\
		for(cnt = 0; cnt < sizeof(_block);			\
			cnt += sizeof(*ptr), ptr++){			\
			if( cnt != 0 && !(cnt & 0xf) )			\
				MTR_DBG_PRINT( (_mtr_p), ("\n\t0x"));	\
			MTR_DBG_PRINT( (_mtr_p), ("%x.", *ptr));	\
		} /* for */						\
		MTR_DBG_PRINT( (_mtr_p), ("\n"));			\
	} 

#else	/* DEBUG */

	#define	INIT_MTR_DBG_PRINT(_msg)
	#define	INIT_MTR_DBG_PRINT2(_msg)
	#define	INIT_MTR_DBG_PRINT_PIO( _msg )

	#define	MTR_ASSERT( _COND )

	#define	DUMP_VERTEX( _vertex_ptr )	

	#define	ADAPTER_DEBUG_ENABLED( _ptr )		0

	#define	IS_MTR_DEBUG( _ptr)			0
	#define	MTR_DBG_PRINT(_ptr, _msg)

	#define IS_MTR_DEBUG2( _ptr )			0
	#define	MTR_DBG_PRINT2(_ptr, _msg)

	#define IS_MTR_DEBUG_PIO( _ptr )		0
	#define MTR_DBG_PRINT_PIO(_ptr, _msg)

	#define MTR_VERIFY_ADX( _ptr )		       
	#define	MTR_DUMP_PKT(_ptr, _ptr2, _msg)
	#define	MTR_DUMP_TX_RX_SLOTS( _ptr, _msg)	

	#define MTR_HEX_DUMP(_mtr_p, _msg, _block)			
#endif	/* DEBUG */

/*	
 *	The kernel interfaces:						
 */

int if_mtrreg(void);

void
if_ptrinit(void)
{
	INIT_MTR_DBG_PRINT2( ("mtr*: if_ptrinit() called.\n"));

	if_mtrreg();

	/* 	
	 *	Have we already been loaded? 
	 */
	loaded_flag	= atomicAddInt(&if_ptr_loaded, 1);
	MTR_ASSERT( loaded_flag >= 1 );

	if( --loaded_flag ){
		cmn_err(CE_NOTE, "mtr*: init(): _ALREADY_ been loaded(%d)!", 
			loaded_flag);
	} /* if loaded_flag */

	return;
} /* if_ptrinit() */

int
if_mtrreg(void)
{

	int 	error;

	INIT_MTR_DBG_PRINT2( ("mtr*: if_mtrreg() called.\n") );

	if( loaded_flag ){
		cmn_err(CE_NOTE, "mtr*: reg(): _ALREADY_ been loaded(%d)!", 
			loaded_flag);
		goto done;
	} 

	/*
	 *	For old version of PCI bus master adapter, 
	 *	TI chip for PCI interface.
	 */
	INIT_MTR_DBG_PRINT2( 
		("mtr*: if_mtrreg(): pciio_driver_register(0x%x, 0x%x).\n",
		MTR_VENDOR_ID, PCI_PCIT_DEVICE_ID) );
	error = pciio_driver_register(MTR_VENDOR_ID, PCI_PCIT_DEVICE_ID, 
		MY_PCI_PREFIX, 0);
	if( error != NO_ERROR )goto done;

	/*
	 *	For new version of PCI bus master apdater,
	 *	Madge chip for PCI interface.
	 */
	INIT_MTR_DBG_PRINT2( 
		("mtr*: if_mtrreg(): pciio_driver_register(0x%x, 0x%x).\n",
		MTR_VENDOR_ID, PCI_PCI2_DEVICE_ID) );
	error = pciio_driver_register(MTR_VENDOR_ID, PCI_PCI2_DEVICE_ID, 
		MY_PCI_PREFIX, 0);

done:
	INIT_MTR_DBG_PRINT2( ("mtr*: if_mtrreg() done with %d.\n",
		error));

#ifdef	DEBUG_INITIALIZATION
	if_ptr_debug = 1;
#endif	/* DEBUG_INITIALIZATION */

	return(error);
} /* if_mtrreg() */

/*
 *	OS calls this when the driver is being unloaded.
 */
int
if_mtrunreg(void)
{
	int		error = NO_ERROR;

#ifdef	DEBUG_INITIALIZATION
	if_ptr_debug = 1;
#endif	/* DEBUG_INITIALIZATION */
	if( loaded_flag ){
		cmn_err(CE_NOTE, "mtr*: unreg(): _ALREADY_ been loaded(%d)!",
			loaded_flag);
		goto done;
	}

	/* 
	 *	We need to unregister from PCI infrastructure.
	 *	The detach() routine will be called for each adapter.
	 */
	INIT_MTR_DBG_PRINT(("mtr*: calling pciio_driver_unregister(%s).\n", 
		MY_PCI_PREFIX));

	/* XXX should return EBUSY if some one is using the driver */
	pciio_driver_unregister(MY_PCI_PREFIX);

done:
	return(error);
} /* if_mtrunreg() */

/* 	
 *	OS calls this when the driver is being unloaded.  
 *
 */
int
if_mtrunload(void)
{
	int		error = NO_ERROR;

	INIT_MTR_DBG_PRINT2( ("mtr*: if_mtrunload() called.\n") );

	if( loaded_flag ){
		cmn_err(CE_NOTE, "mtr*: unload(): _ALREADY_ been loaded(%d)!", 
			loaded_flag);
		goto done;
	} /* if loaded_flag */

done:
	(void )atomicAddInt(&if_ptr_loaded, -1);
	return( error );
} /* if_mtrunload() */

/*
 *	if_mtrhalt() is called when system is going to be halted.
 */
void
if_mtrhalt(void)
{

	INIT_MTR_DBG_PRINT( ("mtr*: if_mtrhalt() called.\n"));

	return;
} /* if_mtrhalt() */

/* ARGSUSED */
int
if_mtropen(dev_t *devp, int flag, int otyp, struct cred *crp)
{

	INIT_MTR_DBG_PRINT(("mtr*: 0x%x, 0x%x, 0x%x, 0x%x",
		devp, flag, otyp, crp));
	return( if_ptr_cnt == 0 ? ENODEV : NO_ERROR );
} /* if_mtropen() */

int
if_mtrclose()
{
	return 0;
} /* if_mtrclose() */

int
if_mtrread()
{
	return( EOPNOTSUPP );
} /* if_mtrread() */

int
if_mtrwrite()
{
	return( EOPNOTSUPP );
} /* if_mtrwrite() */

int
if_mtrioctl()
{
	return( EOPNOTSUPP );
} /* if_mtrioctl() */

#define DEST_DRAIN		0
#define DEST_IP           	1
#define DEST_LLC          	2
#define	MAX_RX_LOOP_CNT	(8192 * 2)	/* too much time on ICS */

/*
 *	mtr_receive() check the RX_SLOT pointed by active_rx_slot
 *	for any received packets.
 */
static void
mtr_receive(mtr_private_t *mtr_p)
{
	mtr_adapter_t	*adapter_p;
	WORD	        active_rx_slot;
	RX_SLOT		**rx_slot_array;
	WORD	        rx_len, rx_status;
	int		flag;
	caddr_t		rx_p;
	struct mbuf	*mh_p, *mt_p;

	int		blen;
	TR_MAC		*mac_p;
	TR_RII		*rif_p;		/* point to RIF. */
	LLC1		*llc_p;		/* point to Link Level Control (LLC) */
	SNAP		*snap_p;	/* point to SNAP */
	ushort_t	*sri;		/* point to Routing Designator in RIF */
	int		sri_len;	/* Len of RIF */
	ushort_t	port;
	int		hlen;
	int		upper_dest;	/* the destination of upper layer */
	int		buflen;		
	struct ifqueue	*ifq;
	int		header_len;

#ifdef	DEBUG
	int		rx_loop_cnt = 0;
#endif	/* DEBUG */

	MTR_ASSERT( mtr_p != NULL && mtr_p->tag == MTR_TAG );
	MTR_VERIFY_PROTECTION( mtr_2_ifnet(mtr_p) );

	adapter_p 	= &(mtr_p->adapter);
	rx_slot_array 	= adapter_p->rx_slot_array;
	active_rx_slot 	= adapter_p->active_rx_slot;

	MTR_DBG_PRINT2(mtr_p, ("mtr%d: mtr_receive() called.\n",
		mtr_p->mtr_unit));

rx_loop:
	flag = 0;

get_len_again:
	OUTS(adapter_p->sif_adr, (__psunsigned_t)
	     &(rx_slot_array[active_rx_slot]->buffer_len));
	if( (rx_len = INS(adapter_p->sif_dat)) == 0 )
		goto done;

	if( rx_len > MAX_RX_LEN(mtr_p) || rx_len < MIN_MAC_FRAME_SIZE ){
		cmn_err(CE_NOTE, "mtr%d: RX rx_len: %d:%d [%d..%d]!",
			mtr_p->mtr_unit, active_rx_slot,
			rx_len, MIN_MAC_FRAME_SIZE, MAX_RX_LEN(mtr_p));
		if( ++flag == 1 )goto get_len_again;

		MTR_DUMP_TX_RX_SLOTS(mtr_p, "RX");
		mtr_2_ifnet(mtr_p)->if_ierrors++;
		goto next_frame;

	} else if( flag ){
		cmn_err(CE_NOTE, "mtr%d: RX %d.%d, flag: %d.",
			mtr_p->mtr_unit, active_rx_slot, rx_len, flag);
	}
	/*	No packet is available.	*/
	rx_len -= MAC_CRC_SIZE;

	OUTS(adapter_p->sif_adr, (__psunsigned_t)
	     &(rx_slot_array[active_rx_slot]->rx_status));
	rx_status = INS(adapter_p->sif_dat);
		
	if( rx_status & GOOD_RX_FRAME_MASK ){
		cmn_err(CE_NOTE, "mtr%d: read bad frame: 0x%x.",
			mtr_p->mtr_unit, rx_status);
		mtr_2_ifnet(mtr_p)->if_ierrors++;
		goto next_frame;
	}	

	mtr_2_ifnet(mtr_p)->if_ipackets++;
	mtr_2_ifnet(mtr_p)->if_ibytes += rx_len;

#ifdef	R10000_SPECULATION_WAR
	/* In R10K, make sure that data is not speculatively cached. */
        xdki_dcache_validate(adapter_p->rx_slot_buf[active_rx_slot],
                adapter_p->rx_tx_buf_size);
        dki_dcache_inval(adapter_p->rx_slot_buf[active_rx_slot],
                adapter_p->rx_tx_buf_size);
#endif /* R10000_SPECULATION_WAR */

	rx_p = adapter_p->rx_slot_buf[active_rx_slot];

	MTR_DBG_PRINT2(mtr_p, 
		("mtr%d: RX: slot: %d, rx_len: %d, rx_status: 0x%x.\n",
		mtr_p->mtr_unit, active_rx_slot, rx_len, rx_status));

#ifdef	DEBUG2
	if( mtr_p->rx_dump_mbuf && IS_MTR_DEBUG2(mtr_p) ){
		int	dump_len;

		dump_len = (rx_len <= MTR_DUMP_PKT_SIZE) ? 
			rx_len : MTR_DUMP_PKT_SIZE;
		bcopy(rx_p, mtod(mtr_p->rx_dump_mbuf, caddr_t), dump_len);
		mtr_p->rx_dump_mbuf->m_len = dump_len;
		MTR_DUMP_PKT(mtr_p, mtr_p->rx_dump_mbuf, "RX");
	}	
#endif	/* DEBUG2 */

	mac_p = (TR_MAC *)rx_p;
	if( (mac_p->mac_sa[5] == mtr_p->mtr_enaddr[5]) &&
	    (mac_p->mac_sa[4] == mtr_p->mtr_enaddr[4]) &&
	    (mac_p->mac_sa[3] == mtr_p->mtr_enaddr[3]) &&
	    (mac_p->mac_sa[2] == mtr_p->mtr_enaddr[2]) &&
	    (mac_p->mac_sa[1] == mtr_p->mtr_enaddr[1]) &&
	    ((mac_p->mac_sa[0] & ~TR_MACADR_RII) == mtr_p->mtr_enaddr[0]) ){
#ifdef	DEBUG
		mtr_p->from_me_cnt++;
#endif	/* DEBUG */
		goto next_frame;		/* from me, ignore it. */
	}

	/*	RIF?	*/
	sri_len	= 0;
	rif_p 	= (TR_RII *)(((uchar_t *)mac_p) + sizeof(*mac_p));
	if( mac_p->mac_sa[0] & TR_MACADR_RII ){

		/*	RII(Routing Information Indication) is on.	*/
		sri 	= (ushort_t *)rif_p;
		sri_len = (*sri & TR_RIF_LENGTH) >> 8;
		if(sri_len <= 2) {
			sri 	= 0;
		}
		MTR_DBG_PRINT2(mtr_p, ("mtr%d: RX SRI: 0x%x %d.\n",
			mtr_p->mtr_unit, sri, sri_len));
	} else {

		sri 	= 0;
		sri_len	= 0;
	}
	MTR_ASSERT( !(sri_len & 1) && sri_len <= TR_MAC_RIF_SIZE );

	/*	MAC control frame or LLC frame */
	if( (mac_p->mac_fc & TR_FC_TYPE) == TR_FC_TYPE_LLC) {

		/*	It is a LLC frame. 	*/
		/*	WATCH OUT: we only use LLC1, not general LLC! */
		llc_p = (LLC1 *)((uchar_t *)rif_p + sri_len);
		if( llc_p->llc1_dsap == TR_IP_SAP ){

			upper_dest = DEST_IP;
			hlen   	= sizeof(*mac_p) + sri_len + sizeof(*llc_p) +
			  sizeof(*snap_p);
			snap_p 	= (SNAP *)(((uchar_t *)mac_p) + hlen - 
					   sizeof(*snap_p));
			port 	= (snap_p->snap_etype[0] << 8) + 
			  (snap_p->snap_etype[1]);
		} else {
			upper_dest = DEST_LLC;
			port	= TR_PORT_LLC;
			hlen 	= sizeof(*mac_p) + sri_len;
			MTR_DBG_PRINT2(mtr_p, 
				("mtr%d: mac_fc: 0x%x, llc1_dsap: "
				"0x%x not supported -> discarded(%d)!\n",
				mtr_p->mtr_unit, mac_p->mac_fc, 
				llc_p->llc1_dsap, active_rx_slot ));
		}
	} else {
		uchar_t		pcf_attn;

		upper_dest	= DEST_DRAIN;
		port		= TR_PORT_MAC;
		hlen 		= sizeof(*mac_p) + sri_len;

		pcf_attn = mac_p->mac_fc & TR_FC_PCF;
		/*	Is it a MAC frame with PCF attention code */
		switch( pcf_attn ){
		case TR_FC_PCF_BCN:
			cmn_err(CE_NOTE, "mtr%d: received beacon frame: 0x%x!",
				mtr_p->mtr_unit, pcf_attn);
			break;
	
		case TR_FC_PCF_CLM:
			cmn_err(CE_NOTE, "mtr%d: received claim token: 0x%x!",
				mtr_p->mtr_unit, pcf_attn);
			break;

		case TR_FC_PCF_PURGE:
			cmn_err(CE_NOTE, "mtr%d: received ring purge: 0x%x!",
				mtr_p->mtr_unit, pcf_attn);
			break;

		case TR_FC_PCF_AMP:
		case TR_FC_PCF_SMP:
		case TR_FC_PCF_EXP: /* also TR_FC_PCF_REMOV, TR_FC_PCF_DUPA */
		default:
			break;
				
		} /* switch */

		/*	TO DO: stats for MAC control packet.	*/
		MTR_DBG_PRINT2(mtr_p, 
			("mtr%d: mac_fc: 0x%x not supported -> "
			"discarded(%d)!\n",
			mtr_p->mtr_unit, mac_p->mac_fc, active_rx_slot ));

	}

	/*	Make sure IP header starts from right boundary.	*/
	header_len = TRPAD + hlen;
	if( header_len & (sizeof(__psunsigned_t) - 1)){
		header_len += sizeof(__psunsigned_t);
		header_len &= ~(sizeof(__psunsigned_t) -1);
	}

	/*	As length includes header's length and we have
	 *	alredy counted it in header_len, we exclude it.
	 */
	buflen	= (rx_len - hlen) + header_len;
	blen	= buflen > MCLBYTES ? MCLBYTES : buflen;
	mt_p = mh_p = m_vget(M_DONTWAIT, blen, MT_DATA);
	if( mh_p == NULL ){
		if( mtr_p->mtr_no_mbuf & 0x3f ){
			cmn_err(CE_NOTE, "mtr%d: RX: m_vget(%d), %d!", 
				mtr_p->mtr_unit, rx_len, mtr_p->mtr_no_mbuf);
		}
		mtr_p->mtr_no_mbuf++;
		goto next_frame;
	}
	mh_p->m_next = NULL;
	mh_p->m_len  = blen;

	/*	copy MAC header, including those RII.	*/
	bcopy(rx_p, mtod(mh_p, char *) + TRPAD, hlen);
	rx_p 	+= hlen;
	buflen	=  rx_len - hlen;

	/*	copy MAC info part.			*/
	bcopy(rx_p, mtod(mh_p, char *) + header_len, (blen - header_len));
	rx_p	+= (blen - header_len);
	buflen	-= (blen - header_len);

	IF_INITHEADER(mtod(mh_p, caddr_t), mtr_2_ifnet(mtr_p), header_len);

	MTR_ASSERT( buflen >= 0 );
	while( buflen > 0 ){
		struct mbuf	*m_p;

		blen = buflen > MCLBYTES ? MCLBYTES : buflen;
		m_p = m_vget(M_DONTWAIT, blen, MT_DATA);
		if( m_p == NULL ){
			if( mtr_p->mtr_no_mbuf & 0x3f ){
				cmn_err(CE_NOTE, "mtr%d: RX: m_vget(%d) %d.",
					mtr_p->mtr_unit, blen,
					mtr_p->mtr_no_mbuf);
			}
			mtr_p->mtr_no_mbuf++;
			/* free all the mbufs we got so far for the packet. */
			m_freem(mh_p);			
			goto next_frame;
		}

		bcopy(rx_p, mtod(m_p, uchar_t *), blen);
	
		rx_p 		+= blen;
		buflen 		-= blen;

		mt_p->m_next	= m_p;
		mt_p		= m_p;
	} /* while */

	/*	TO DO: snooping.	*/
	if( RAWIF_SNOOPING( &(mtr_p->mtr_rawif) ) ){
		mtr_snoop_input(mtr_p, mh_p, sri_len, rx_len);
	}

	/* 	Check mcast/bcast */
	if (TOKEN_ISGROUP(mac_p->mac_da)) {
		mtr_2_ifnet(mtr_p)->if_imcasts++;		
		if( TOKEN_ISBROAD(mac_p->mac_da) )
			mh_p->m_flags	|= M_BCAST;
		else 
			mh_p->m_flags	|= M_MCAST;
	} else {
		/*	For unicast, is it really for me or
		 *	we got it because we in promisc mode?
		 */
		if( IS_MTR_FLAG_SET(mtr_p, MTR_FLAG_STATE_PROMISC) &&
		    ((mac_p->mac_da[5] != mtr_p->mtr_enaddr[5]) ||
		     (mac_p->mac_da[4] != mtr_p->mtr_enaddr[4]) ||
		     (mac_p->mac_da[3] != mtr_p->mtr_enaddr[3]) ||
		     (mac_p->mac_da[2] != mtr_p->mtr_enaddr[2]) ||
		     (mac_p->mac_da[1] != mtr_p->mtr_enaddr[1]) ||
		     (mac_p->mac_da[0] != mtr_p->mtr_enaddr[0])) ){
			m_freem(mh_p);
			mh_p = NULL;
			goto rx_frame_done;
		}
	}

	MTR_ASSERT( upper_dest == DEST_IP || upper_dest == DEST_LLC ||
		    upper_dest == DEST_DRAIN );
	if( upper_dest == DEST_LLC ){
		dlsap_family_t	*dlp;

		dlp = dlsap_find(llc_p->llc1_dsap, DL_LLC_ENCAP);

                MTR_DBG_PRINT( mtr_p,
                        ("mtr%d: LLC packet encountered. dlp: 0x%x.\n",
                        mtr_p->mtr_unit, dlp));

                if( dlp ){
                        int     roff = header_len - TRPAD - hlen;
                        uchar_t shift_buf[64];

                        if( roff > 0 ){
                                bcopy( mtod(mh_p, caddr_t) + TRPAD,
                                        shift_buf, hlen);
                                bcopy( shift_buf,
                                        mtod(mh_p, caddr_t) + TRPAD + roff,
                                        hlen);
                        }
                        /*
                         *      WATCH OUT: mac_p must point to the mbuf
                         *      given to the upper layer as it might be
                         *      scheduled by netitr, not a direct call.
                         */
                        mac_p = (TR_MAC *)(mtod(mh_p, caddr_t) + TRPAD + roff);

                        /*      Header only has MAC hdr and RI, if any. */
                        IF_INITHEADER(mtod(mh_p, caddr_t),
                                mtr_2_ifnet(mtr_p), TRPAD + roff);

                        if( (*dlp->dl_infunc)(dlp, mtr_2_ifnet(mtr_p),
                                mh_p, mac_p) ){
                                mh_p = NULL;
                                goto rx_frame_done;
                        }

                        cmn_err(CE_NOTE, "dlsap reject the packet.\n");

                        if( roff != 0 ){
                                bcopy( shift_buf,
                                        mtod(mh_p, caddr_t) + TRPAD,
                                        hlen);
                        }
                        mac_p = (TR_MAC *)(mtod(mh_p, char *) + TRPAD);
                        IF_INITHEADER(mtod(mh_p, caddr_t), mtr_2_ifnet(mtr_p),
                                header_len);
                }

                /*      Send to drain(7p), if anyone wants it.  */
                if( RAWIF_DRAINING( &(mtr_p->mtr_rawif)) ){
                        mtr_drain_input(mtr_p, mh_p, rx_len, sri_len, port);

                        /*      WATCH OUT:
                         *      mtr_drain_input() frees mbuf chain up
                         */
                        mh_p = NULL;
                } else {
                        m_freem(mh_p);
                        mh_p = NULL;
                }
                goto rx_frame_done;
        } /* if DEST_LLC */

	switch( port ){
	case ETHERTYPE_IP:
		ifq = &ipintrq;
		if( IF_QFULL(ifq) ){
			IF_DROP(ifq);
			mtr_2_ifnet(mtr_p)->if_iqdrops++;
		} else {

			IF_ENQUEUE(ifq, mh_p);
			mh_p = NULL;		/* can not free so ... */
		}
		schednetisr(NETISR_IP);
		break;

	case ETHERTYPE_ARP:
		if( sri ){
			*sri	&= ~TR_RIF_SRB_NBR;
			*sri	^= TR_RIF_DIRECTION;
		}
		srarpinput(&mtr_p->mtr_ac, mh_p, sri);

		/*	TO DO: mh_p = NULL!? 	*/
		mh_p = NULL;
		break;

	default:

		/*	Send the packet to drain iff it is enabled 
		 *	for the adapter. */
		if( RAWIF_DRAINING( &(mtr_p->mtr_rawif)) ){
#ifdef	DEBUG2
			MTR_DBG_PRINT2(mtr_p,
				("mtr%d: mtr_drain_input"
				 "(0x%x, %d, %d, %d, 0x%x).\n",
				mtr_p->mtr_unit,
				mtr_p, rx_len, sri_len, port, mh_p));
#endif	/* DEBUG2 */
			mtr_drain_input(mtr_p, mh_p, rx_len, sri_len, port);

			/* NOTE: mtr_drain_input() free it up */
			mh_p = NULL;	
		}
		mtr_2_ifnet(mtr_p)->if_noproto++;

		break;
	} /* switch() */

	if(mh_p != NULL){
		m_freem(mh_p);
		mh_p = NULL;
	}
	
rx_frame_done:
	MTR_DBG_PRINT2(mtr_p, ("mtr%d: RX done: %d %d %d.\n",
		mtr_p->mtr_unit, active_rx_slot, rx_len, 
		mtr_2_ifnet(mtr_p)->if_ipackets));

next_frame:
  	dki_dcache_inval(adapter_p->rx_slot_buf[active_rx_slot],
			  adapter_p->rx_tx_buf_size);
	OUTS(adapter_p->sif_adr, (__psunsigned_t)
	     &(rx_slot_array[active_rx_slot]->buffer_len));
	OUTS(adapter_p->sif_dat, 0);

	if( ++active_rx_slot == 
	   adapter_p->init_block_p->fastmac_parms.rx_slots)
		active_rx_slot = 0;
	adapter_p->active_rx_slot = active_rx_slot;

	/*	TO DO: 
	 *	shutdown the adapter or mark it down.
	 */
	MTR_ASSERT( ++rx_loop_cnt < MAX_RX_LOOP_CNT );
	goto rx_loop;

done:
	MTR_DBG_PRINT2(mtr_p, ("mtr%d: RX done!\n", mtr_p->mtr_unit));
	return;
} /* mtr_receive() */

/*
 *	mtr_get_open_status():
 *		Read out STB's adapter_open and open_error to find out
 *		the current open status.
 *
 *		return open status: 0: error, non-0: OK.
 */
static void
mtr_get_open_status(mtr_private_t *mtr_p)
{
	mtr_adapter_t	*adapter_p;

	adapter_p = &(mtr_p->adapter);

	OUTS(adapter_p->sif_adr, (__psunsigned_t)
	     &(adapter_p->stb_dio_addr->adapter_open));

	adapter_p->stb.adapter_open 	= (WBOOLEAN)INS(adapter_p->sif_datinc);
	adapter_p->stb.open_error 	= INS(adapter_p->sif_datinc);

	return;
} /* mtr_get_open_status() */

/*
 *	Processing the following interrupts:
 *		- FASTMAC_SIFINT_ADAPTER_CHECK,
 *		- FASTMAC_SIFINT_SRB_FREE,
 *		- FASTMAC_SIFINT_SSB_RESPONSE,
 *		- FASTMAC_SIFINT_ARB_COMMAND
 *
 *	Two reasons for having them processed separately:
 *		- they are exception, not in normal path so ...
 *		- both PCIT and PCI2 share the same logic so ...
 *
 */
int
mtr_intr2(mtr_private_t *mtr_p, ushort_t sifsts, ushort_t sifsts2)
{
	mtr_adapter_t	*adapter_p;
	int		ack_needed = FALSE;
	int		cnt;
	ushort_t	*ptr;


	if (!IS_MTR_IFF_SET(mtr_p, IFF_ALIVE)) {
		MTR_DBG_PRINT(mtr_p, ("mtr%d: mtr_intr2() fail because "
				      "interface is not up.\n",
				      mtr_p->mtr_unit));
		return ack_needed;
	}

	MTR_DBG_PRINT( mtr_p,
		("mtr%d: mtr_intr2() intr type: 0x%x -> 0x%x.\n", 
		mtr_p->mtr_unit,  sifsts, sifsts2));

	MTR_VERIFY_PROTECTION( mtr_2_ifnet( mtr_p ) );
	adapter_p = &(mtr_p->adapter);

	if( sifsts2 & FASTMAC_SIFINT_ADAPTER_CHECK ){

		/*
		 *	Adapter Check Interrupt.	
		 */

		cmn_err(CE_ALERT, 
			"mtr%d: SIFINT_ADAPTER_CHECK(0x%04x.%04x)!",
			mtr_p->mtr_unit, sifsts, sifsts2);
		/* 
		 *	TO DO: 
		 *	We expect user to restart
		 *	the adapter, not us. Why? we
		 *	are in the intr stack!
		 */
		MTR_RESET_IFF( mtr_p, IFF_ALIVE );

	} else if( sifsts2 & FASTMAC_SIFINT_SRB_FREE ){
		
		/*	
		 *	SRB free interrupts.
		 */
		if( mtr_p->srb_used ){

			hwi_pci_set_dio_address( mtr_p,
				((__psunsigned_t)adapter_p->srb_dio_addr & 0xffff),
				DIO_LOCATION_EAGLE_DATA_PAGE1);
			for( ptr = (ushort_t *)&(adapter_p->srb_general2), 
			     cnt = 0; 
			     cnt < sizeof(SRB_GENERAL);
			     cnt += sizeof(*ptr), ptr++){
				*ptr = INS(adapter_p->sif_datinc);
			}
	
			MTR_DBG_PRINT(mtr_p, ("mtr%d: SIFINT_SRB "
				"(0x%04x.%04x): 0x%02x.%02x -> 0x%02x.%02x.\n",
				mtr_p->mtr_unit, sifsts, sifsts2,
				adapter_p->srb_general.header.function,
				adapter_p->srb_general.header.return_code,
				adapter_p->srb_general2.header.function,
				adapter_p->srb_general2.header.return_code));

			switch( adapter_p->srb_general2.header.function ){
			case SET_FUNCTIONAL_ADDRESS_SRB:
			case SET_GROUP_ADDRESS_SRB:
			case CLOSE_ADAPTER_SRB:
			case OPEN_ADAPTER_SRB:
			case MODIFY_OPEN_PARMS_SRB:
				break;

			case READ_ERROR_LOG_SRB:
				break;

			default:
				MTR_ASSERT(0);
				break;
			} /* swtich */

			wakeup( &mtr_p->srb_used );
		} else {
		
			/* 	Locker could be gone because of signal. */
			cmn_err(CE_DEBUG,
				"mtr%d: 0x%x(SIFINT_SRB) but %d!",
				mtr_p->mtr_unit, sifsts, mtr_p->srb_used);
		}

		ack_needed = TRUE;

	} else if( sifsts2 & FASTMAC_SIFINT_ARB_COMMAND ) { 

		ptr = (ushort_t *) &(adapter_p->arb);

		/*
		 *	For ARB command interrupts and 
		 *	SSB response interrupt, 
		 */

		hwi_pci_set_dio_address( mtr_p, 
			((__psunsigned_t)adapter_p->arb_dio_addr & 0xffff), 
			DIO_LOCATION_EAGLE_DATA_PAGE1);
		for( cnt = 0; cnt < sizeof(arb_general_t); 
			cnt += sizeof(*ptr), ptr++ ){
			*ptr = INS(adapter_p->sif_datinc);
		}	

		mtr_get_open_status(mtr_p);

		if( adapter_p->arb.status ||
		    (adapter_p->stb.adapter_open == 0 && 
		     adapter_p->stb.open_error != EAGLE_OPEN_ERROR_SUCCESS)){
			cmn_err(CE_NOTE,
				"mtr%d: SIFINT_ARB "
				"(0x%04x.%04x): arb: 0x%02x.%02x "
				"open: 0x%04x.%04x",
				mtr_p->mtr_unit, sifsts, sifsts2,
				adapter_p->arb.header.function,
				adapter_p->arb.status,
				adapter_p->stb.adapter_open,
				adapter_p->stb.open_error);
			if( adapter_p->arb.status & 0x40 ){
				cmn_err(CE_WARN,
					"mtr%d: single station in the ring: "
					"0x%x!", mtr_p->mtr_unit,
					adapter_p->arb.status);
			}
		}

		if( adapter_p->arb.header.function == 
			EAGLE_ARB_FUNCTION_CODE){
	
			ack_needed = TRUE;
			OUTS(adapter_p->sif_int, EAGLE_ARB_FREE_CODE);
		} else {
			MTR_ASSERT( 0 );
		}

	} else if( sifsts2 & FASTMAC_SIFINT_SSB_RESPONSE ){

		cmn_err(CE_WARN,
			"mtr%d: SIFINT_SSB (0x%04x.%04x)?",
			mtr_p->mtr_unit, sifsts, sifsts2);

		ack_needed = TRUE;

		MTR_ASSERT(0);
	}
		
	return( ack_needed );
} /* mtr_intr2() */
	
#define	MAX_INTR_POLLING_TIMES		512
#define	POLLING_SIFINT(_mtr_p, _sifsts, _cnt, _limit ) {	\
	ushort_t	sifsts2;					\
	(_cnt) = 0;						\
	while( 1 ){						\
		(_sifsts) = INS( (_mtr_p)->adapter.sif_int );	\
		sifsts2   = INS( (_mtr_p)->adapter.sif_int );	\
		if( sifsts2 == (_sifsts) ){			\
			MTR_DBG_PRINT2( (_mtr_p), 		\
				("mtr%d: POLLING_SIFINT: 0x%x.\n", \
				(_mtr_p)->mtr_unit, sifsts) );	\
			break;					\
		}						\
		++(_cnt);					\
		if( (_cnt) >= (_limit) ){			\
			cmn_err(CE_ALERT, "mtr%d: POLLING_SIFINT: "\
				"0x%x.%x!",			\
				(_mtr_p)->mtr_unit,		\
				sifsts, sifsts2);		\
			break;					\
		};						\
		if( !((_cnt) & 0x1f) ){				\
			MTR_DBG_PRINT( (_mtr_p), 		\
				("mtr%d: POLLING_SIFINT: sifsts: "\
				 "0x%x 0x%x.\n",		\
				 (_mtr_p)->mtr_unit, 		\
				 sifsts, sifsts2));		\
		}						\
	}  /* while() */					\
}
		
/*
 *	The interrupt handler:
 */
#if	IP32
/* ARGSUSED */
static void
mtr_intr(eframe_t *ef, void *arg)
#else
static void
mtr_intr(void *arg)
#endif
{
	if_mtr_t        *ifmtr_p = (if_mtr_t *)arg;
	mtr_private_t	*mtr_p = NULL;
	WORD		sifsts;
	WORD		sifsts2;
	mtr_adapter_t	*adapter_p;
	int		cnt;
	int		proper_sif_int = FALSE;
	int		ack_needed;

	INIT_MTR_DBG_PRINT2 (("mtr*: mtr_intr(0x%x) called.\n", 
			     (int)ifmtr_p));

	IFNET_LOCK( ifmtr_2_ifnet(ifmtr_p) );

	mtr_p = ifmtr_p->mtr_p;
	if( ifmtr_p->flags == MTR_FLAG_UNINIT ){
		MTR_ASSERT(ifmtr_p->mtr_p == NULL);
		goto done;
	}

	if( mtr_p == NULL || mtr_p->tag != MTR_TAG ) {
		/*	no way to ack interrupt so ... */
		cmn_err(CE_PANIC, "mtr*: interrupt with 0x%x?!", ifmtr_p);
	}
	if(!IS_MTR_FLAG_SET(mtr_p, MTR_FLAG_PIO_DONE)) {
		cmn_err(CE_PANIC, 
                        "mtr*: 0x%x.%x.%x interrupt without MTR_FLAG_PIO_DONE!",
                        ifmtr_p, mtr_p, ifmtr_p->flags);
        }
	
	adapter_p = &(mtr_p->adapter);

	POLLING_SIFINT(mtr_p, sifsts, cnt, MAX_INTR_POLLING_TIMES);
	MTR_ASSERT( cnt < MAX_INTR_POLLING_TIMES);

	if( !(sifsts & EAGLE_SIFINT_SYSTEM) ){
		if( !(mtr_p->intr_no_sts & 0x1f) ){
			cmn_err(CE_NOTE, 
				"mtr%d: mtr_intr(): sifsts(0x%x) 0x%x!\n",
			mtr_p->mtr_unit, sifsts, EAGLE_SIFINT_SYSTEM);
		}
		mtr_p->intr_no_sts++;
		goto done;
	};

	proper_sif_int = TRUE;
	if( mtr_p->mtr_dev_id == PCI_PCIT_DEVICE_ID &&
	    adapter_p->broken_dma == PCIT_BROKEN_DMA ){
		WORD	saved_sifadr, dmastatus;

		/*	WATCH OUT: For PCIT only!	*/
		MTR_ASSERT( mtr_p->mtr_dev_id == PCI_PCIT_DEVICE_ID );

		saved_sifadr = INS(adapter_p->sif_adr);

		/* Broken DMA is enabled so check if this was a broken 
		 * DMA interrupt.
		 */
		OUTS(adapter_p->sif_adr, DIO_LOCATION_DMA_CONTROL);
		dmastatus = INS(adapter_p->sif_dat);
		MTR_DBG_PRINT2(mtr_p, 
			("mtr%d: mtr_pcit_intr dmastatus: 0x%x.\n", 
			  mtr_p->mtr_unit, dmastatus));

		/*
		 *	If there is a DMA pending then do some DMA 
		 *	processing because of BROKEN DMA!
		 */
		if( dmastatus ){
			BYTE			broken_dma_status;
			volatile BYTE	*broken_dma_addr = (BYTE *)
				adapter_p->broken_dma_handshake_addr;

			proper_sif_int = FALSE;

			MTR_ASSERT( IS_KSEG1(broken_dma_addr) );

			/* Clear EAGLE_SIFINT_HOST_IRQ to ack intr 
			 * at SIF */
			OUTS(adapter_p->sif_int, 0);

			/* Set the host flag to 0. */
			*broken_dma_addr = 0;
			dki_dcache_wbinval((void *)adapter_p->scb_buff, 
				adapter_p->test_size);

			/*	Clear the flag on the adapter: 
			 *	start DMA. */
			OUTS(adapter_p->sif_dat, 0);

			/*	Wait until the adapter puts 0x11 
			 *	in our flag. 
			 *
			 *	WATCH OUT: 
			 *	a broken adapter -> hang the system!? 
			 */
#define	MAX_BROKEN_DMA_POLL	1024
			MTR_DBG_PRINT2(mtr_p, ("mtr%d: waiting 0x11.\n",
				mtr_p->mtr_unit));	
			cnt = 0;
			broken_dma_status = *broken_dma_addr;
			while(  broken_dma_status != 0x11 ) {
				us_delay(50);		
				broken_dma_status = *broken_dma_addr;
				if( !(cnt & 0x7f) && cnt != 0){
					MTR_DBG_PRINT(mtr_p, 
						("mtr%d: "
						 "broken_dma_status: "
						 "%d: 0x%x.\n",
						 mtr_p->mtr_unit, cnt,
						*broken_dma_addr));
				}
				if( ++cnt < MAX_BROKEN_DMA_POLL ) 
					continue;

				/* I have seen looping here 
				 * so ...
				 * TO DO: what's next!?
				 */
				cmn_err(CE_ALERT,
					"mtr%d: possible lockup: "
					"%d, 0x%x!",
					mtr_p->mtr_unit,
					cnt, broken_dma_status);
				break;
			}  /* while broken_dma_status */
			MTR_DBG_PRINT2(mtr_p, ("mtr%d: got 0x11.\n",
				mtr_p->mtr_unit));	
		} /* dmastatus */

		if( dmastatus == 0x2222 ){
			/* 	A interrupt will follow the DMA.*/
			POLLING_SIFINT(mtr_p, sifsts, cnt,
				MAX_INTR_POLLING_TIMES);
			MTR_ASSERT( cnt < MAX_INTR_POLLING_TIMES );
			if( cnt < MAX_INTR_POLLING_TIMES )
				proper_sif_int = TRUE;
		} /* dmastatus == 0x2222 */

		/*	Restore sifadr.				*/
		OUTS(adapter_p->sif_adr, saved_sifadr);

	}  /* if PCI_PCIT_DEVICE_ID && broken_dma */

	if( proper_sif_int != TRUE ) goto done;

	/*	
	 *	A SIF interrupt has occurred.
	 *	This could be an SRB free, 
	 *	an adapter check or 
	 *	a received frame interrupt.
	 *
	 *	Clear EAGLE_SIFINT_HOST_IRQ to acknowledge 
	 *	interrupt at SIF.	
	 */
	OUTS(adapter_p->sif_int, 0 );

	/*	
	 *	The following codes are from 
	 *	drv_irq.c: driver_interrupt_entry().
	 */
	sifsts2 = ((sifsts & 0x00ff) ^ (sifsts >> 8)) & 0x000f;	

	/*
	 *	Action depends on interrupt type.
	 */	
	if( sifsts2 != 0){
		ack_needed = mtr_intr2(mtr_p, sifsts, sifsts2);
	} else {
		ack_needed = FALSE;
	} /* if sifsts2 != 0 */

	/* 	TX done? 	
	 *	As we use 'blind transfer' method, 
	 *	we do not care interrupt because of TX.
	 */

	/*	RX: 	*/
	if( !iff_dead(mtr_p->mtr_if_flags) ) mtr_receive(mtr_p);
	
	if( ack_needed == TRUE ){
		sifsts2 = (sifsts2 << 8) | DRIVER_SIFINT_IRQ_FASTMAC | 
			DRIVER_SIFINT_FASTMAC_IRQ_MASK;
		OUTS(adapter_p->sif_int, sifsts2);
	}

done:

	IFNET_UNLOCK( ifmtr_2_ifnet(ifmtr_p) );	

#ifdef	DEBUG
	if( mtr_p != NULL )mtr_p->intr_cnt++;
#endif	/* DEBUG */

	MTR_DBG_PRINT2(mtr_p, ("mtr%d: mtr_intr() done %d.\n", mtr_p->mtr_unit,
		(mtr_p != NULL ? mtr_p->intr_cnt : 99999) ));

 	return ;
} /* mtr_intr() */

static int
mtr_attach_net( if_mtr_t *ifmtr_p )
{
	int		if_locked = 0;
	int		error = NO_ERROR;
	mtr_private_t	*mtr_p;
	struct ifnet	*ifnet_p;

	INIT_MTR_DBG_PRINT2( ("mtr*: mtr_attach_net(0x%x) called.\n", 
		ifmtr_p));
	MTR_ASSERT( ifmtr_p->mtr_p != NULL );
	if( (mtr_p = ifmtr_p->mtr_p) == NULL ){
		cmn_err(CE_WARN,
			"mtr%d, mtr_attach_net(): 0x%x mtr_p: 0x%x!",
			ifmtr_2_ifnet(ifmtr_p)->if_unit,
			ifmtr_p, mtr_p);
		error = EIO;
		goto done;
	}

	/*	
	 *	We have not done much to the adapter so 
	 *	it should never be marked as ALIVE at this time.
	 */
	MTR_ASSERT( !IS_MTR_IFF_SET( mtr_p, IFF_ALIVE) );
	if( !IS_MTR_FLAG_SET(mtr_p, MTR_FLAG_INTR_DONE) ){
		cmn_err(CE_WARN,
			"mtr%d, mtr_attach_net(): MTR_FLAG_INTR_DONE: 0x%x!",
			mtr_p->mtr_unit, ifmtr_p->flags);
		error = EIO;
		goto done;
	}

	/*
	 *	Find the corresponding ifnet and lock it up.
	 */
	ifnet_p = mtr_2_ifnet(mtr_p);

	if( mtr_bc_mask ){
		mtr_p->adapter.nullsri = (mtr_bc_mask | 0x200);
	} else {
		mtr_p->adapter.nullsri = (TR_RIF_ALL_ROUTES | 0x200);
	}
	if( mtr_p->mtr_if_mtu >= 17800)
		mtr_p->adapter.nullsri	|= TR_RIF_LF_17800;
	else if ( mtr_p->mtr_if_mtu >= 11407)
		mtr_p->adapter.nullsri 	|= TR_RIF_LF_11407;
	else if ( mtr_p->mtr_if_mtu >= 8144)
		mtr_p->adapter.nullsri 	|= TR_RIF_LF_8144;
	else if ( mtr_p->mtr_if_mtu >= 4472)
		mtr_p->adapter.nullsri 	|= TR_RIF_LF_4472;
	else if ( mtr_p->mtr_if_mtu >= 2052)
		mtr_p->adapter.nullsri 	|= TR_RIF_LF_2052;
	else if ( mtr_p->mtr_if_mtu >= 1470)
		mtr_p->adapter.nullsri 	|= TR_RIF_LF_1470;
	else if ( mtr_p->mtr_if_mtu >= 516)
		mtr_p->adapter.nullsri 	|= TR_RIF_LF_516;
	else 
		mtr_p->adapter.nullsri	|= TR_RIF_LF_INITIAL;

	/*	Configure the broadcast address iff 
	 *	it is not configured yet.
	 */
	if( (mtr_p->mtr_baddr[0] | mtr_p->mtr_baddr[1] |
	     mtr_p->mtr_baddr[2] | mtr_p->mtr_baddr[3] |
	     mtr_p->mtr_baddr[4] | mtr_p->mtr_baddr[5]) == 0){
		MTR_ASSERT( sizeof(mtr_baddr_c) ==
			sizeof(mtr_p->mtr_baddr) );
		if( mtr_batype == 1 ){
			bcopy(mtr_baddr_c, mtr_p->mtr_baddr,
				sizeof(mtr_p->mtr_baddr));
		} else {
			bcopy(mtr_baddr_f, mtr_p->mtr_baddr,
				sizeof(mtr_p->mtr_baddr));
		}
	}

	/*	
	 *	Even the adapter fails to be initialized,
	 *	we still (re)attach to upper layer because
	 *	user can ifconfig 'up' to try again and again.
	 */
	ifnet_p->if_flags 	|= MTR_IFF_FLAGS;
	ifnet_p->if_output 	= mtr_output;
	ifnet_p->if_ioctl  	= mtr_ioctl;
	ifnet_p->if_watchdog	= mtr_watchdog;
	ifnet_p->if_reset	= mtr_reset;
	ifnet_p->if_addrlen	= TR_MAC_ADDR_SIZE;
	ifnet_p->if_rtrequest	= arp_rtrequest;
	ifnet_p->if_baudrate.ifs_value = mtr_p->adapter.mtr_s16Mb ? 
		16*1000*1000 : 4*1000*1000;

	MTR_ASSERT( ifnet_p->if_timer == 0);

	if( IS_MTR_FLAG_SET( mtr_p, MTR_FLAG_NETWORK_REATTACHING) ){
		MTR_DBG_PRINT(mtr_p, ("mtr%d: reattach(0x%x).\n",
			mtr_p->mtr_unit, ifnet_p));
	} else {
		int	raw_hdr_len;

		MTR_DBG_PRINT(mtr_p, ("mtr%d: if_attach(0x%x).\n",
			mtr_p->mtr_unit, ifnet_p));
		if_attach( ifnet_p );

		raw_hdr_len = TR_RAW_MAC_SIZE + sizeof(LLC1) 
			+ sizeof(SNAP);
		rawif_attach( &mtr_p->mtr_rawif,
			ifnet_p,
			(caddr_t)mtr_p->mtr_enaddr,	/* addr */
			(caddr_t)&etherbroadcastaddr[0],/* broadaddr */
			sizeof( mtr_p->mtr_enaddr ),	/* addrlen */
			raw_hdr_len,			/* hdrlen */
			structoff(tr_mac, mac_sa[0]),	/* srcoff */
			structoff(tr_mac, mac_da[0]));	/* dstoff */

		MTR_DBG_PRINT(mtr_p, 
			("mtr%d: rif: %d.%d.%d.%d.%d.%d.\n",
			mtr_p->mtr_unit,
			mtr_p->mtr_rawif.rif_addrlen,
			mtr_p->mtr_rawif.rif_hdrlen,
			RAW_ALIGNGRAIN,
			mtr_p->mtr_rawif.rif_padhdrlen, 
			mtr_p->mtr_rawif.rif_hdrpad, 
			mtr_p->mtr_rawif.rif_hdrgrains
			));

	}
	IFNET_LOCK( ifnet_p);

	if_locked = 1;

	SET_MTR_FLAG( mtr_p, MTR_FLAG_NETWORK_ATTACHED);

done:

	if( IS_MTR_FLAG_SET( mtr_p, MTR_FLAG_NETWORK_REATTACHING) ){
		RESET_MTR_FLAG( mtr_p, MTR_FLAG_NETWORK_REATTACHING);
	}

	if( if_locked ){
		IFNET_UNLOCK( ifnet_p);	
	}

	INIT_MTR_DBG_PRINT2(("mtr*: mtr_attach_net() done with %d.\n", error));
	return( error );
} /* mtr_attach_net() */

int
if_ptrattach(vertex_hdl_t connect_vertex)
{
	int		error = NO_ERROR;
	int		new_mtr;		
	vertex_hdl_t	tr_vertex;
	struct ifnet	*ifnet_p;
	if_mtr_t	*ifmtr_p;
	mtr_private_t	*mtr_p;
	mtr_adapter_t   *adapter_p;
	BYTE            *sif_base;
	char            devnm[MAXDEVNAME];
	dev_t           to;
	int             rc;

	INIT_MTR_DBG_PRINT2( ("mtr*: if_ptrattach(0x%x) called.\n", 
		connect_vertex));
	MTR_ASSERT( if_ptr_cnt < NUM_SLOTS );
	if( if_ptr_cnt >= NUM_SLOTS ){
		cmn_err(CE_WARN,
			"mtr%d: too many adapter -> ignored!", if_ptr_cnt);
		error = EIO;
		goto done;
	}
	DUMP_VERTEX(connect_vertex); 

	INIT_MTR_DBG_PRINT2( ("mtr*: if_ptrattach() kmem_zalloc(0).\n"));
	/*	
	 *  Create and initialize a private context for the adapter.
	 */
	if( (mtr_p = kmem_zalloc( sizeof(*mtr_p), MTR_KMEM_ARGS))
	   == NULL){
		cmn_err(CE_WARN, "mtr%d: attach(): kmem_zalloc(%d)!", 
			if_ptr_cnt, sizeof(*mtr_p));
		error = ENOBUFS;
		goto done;
#ifdef	DEBUG
	} else {
		mtr_p->rx_dump_mbuf_size = MCLBYTES;
		mtr_p->rx_dump_mbuf = m_vget(M_DONTWAIT, 
					     mtr_p->rx_dump_mbuf_size, 
					     MT_DATA);
		if( mtr_p->rx_dump_mbuf == NULL ){
			cmn_err(CE_DEBUG, 
				"mtr%d: rx_dump_mbuf!\n", 
				mtr_p->mtr_unit);
		}
#endif	/* DEBUG */
	} 

	INIT_MTR_DBG_PRINT2( ("mtr*: if_ptrattach() hwgraph_info_get_LBL(%d"
			      ", %s)\n", connect_vertex, MTR_INFO_LBL));
	if (hwgraph_info_get_LBL(connect_vertex, MTR_INFO_LBL, 
				 (arbitrary_info_t *)&ifmtr_p) != 
	    GRAPH_SUCCESS) {
		INIT_MTR_DBG_PRINT2(("mtr%d: if_ptrattach() kmem_zalloc"
				     "(ifmtr_p).\n", if_ptr_cnt));
		if( (ifmtr_p = kmem_zalloc( sizeof(*ifmtr_p),
			MTR_KMEM_ARGS)) == NULL ){
			cmn_err(CE_WARN, "mtr%d: attach(): kmem_zalloc(%d) 1!",
				if_ptr_cnt, sizeof(*ifmtr_p));
			error = ENOBUFS;
			goto done;
		}
		ifmtr_p->connect_vertex		= connect_vertex;

		INIT_MTR_DBG_PRINT2(("mtr%d: if_ptrattach() kmem_zalloc"
				     "(ifnet_p->if_name).\n", if_ptr_cnt));
		ifnet_p = tiftoifp(&(ifmtr_p->tr_if));
		if( (ifnet_p->if_name = kmem_zalloc(strlen(MY_NET_PREFIX)+2,
			KM_NOSLEEP)) == NULL ){
			cmn_err(CE_WARN,
				"mtr%d: attach(): kmem_zalloc(%d) 2!",
				if_ptr_cnt, strlen(MY_NET_PREFIX)+2);
			error = ENOBUFS;
			goto done;
		}
		strcpy(ifnet_p->if_name, MY_NET_PREFIX);
		ifnet_p->if_type = IFT_ISO88025;
		ifnet_p->if_unit = if_ptr_cnt;
		if_ptr_cnt++;


		INIT_MTR_DBG_PRINT2(
			("mtr%d: if_ptrattach() hwgraph_info_add_LBL"
			 "(0x%x, 0x%x).\n", ifnet_p->if_unit,
			 connect_vertex, ifmtr_p));
		hwgraph_info_add_LBL(connect_vertex, 
				     MTR_INFO_LBL,
				     (arbitrary_info_t)ifmtr_p);
		new_mtr = 1;

		INIT_MTR_DBG_PRINT2((
			"mtr%d: if_ptrattach() new ifmtr_t: 0x%x.0x%x.\n", 
			ifnet_p->if_unit, ifmtr_p, mtr_p));

	} else {
		MTR_ASSERT( ifmtr_p->mtr_p == NULL );
		MTR_ASSERT( ifmtr_p->connect_vertex == connect_vertex);
		new_mtr = 0;
		ifnet_p = tiftoifp(&(ifmtr_p->tr_if));
	}

	ifmtr_p->mtr_p		= mtr_p;
	mtr_p->ifmtr_p		= ifmtr_p;
	mtr_p->tag		= MTR_TAG;

	/*	Register my error handler */
	pciio_error_register(connect_vertex,
		mtr_error, (error_handler_arg_t)ifmtr_p);

	mutex_init( &(mtr_p->init_mutex), MUTEX_DEFAULT, MY_NET_PREFIX);
	mutex_init( &(mtr_p->srb_mutex),  MUTEX_DEFAULT, MY_NET_PREFIX);

	MTR_ASSERT( mtr_p   != NULL );
	MTR_ASSERT( ifnet_p != NULL );
	MTR_ASSERT( (ifmtr_p->flags == MTR_FLAG_UNINIT) );
	if( ifmtr_p->flags != MTR_FLAG_UNINIT ){
		cmn_err(CE_WARN, "mtr%d: again for 0x%x: 0x%x.0x%x!?", 
			if_ptr_cnt, connect_vertex, mtr_p, ifmtr_p->flags);
		error = EIO;
		goto done;
	}

#ifdef	DEBUG
	SET_MTR_FLAG( mtr_p, MTR_FLAG_DEBUG_LEVEL1);
	#ifdef	DEBUG2
		SET_MTR_FLAG( mtr_p, MTR_FLAG_DEBUG_LEVEL2);
	#endif	/* DEBUG2 */
	#ifdef	DEBUG_PIO
		SET_MTR_FLAG( mtr_p, MTR_FLAG_DEBUG_LEVEL_PIO);
	#endif	/* DEBUG_PIO */

#endif	/* DEBUG */

	MTR_ASSERT( mtr_p == ifmtr_p->mtr_p && mtr_p->ifmtr_p == ifmtr_p);

	INIT_MTR_DBG_PRINT2( ("mtr%d: if_ptrattach() get PCI.\n",
			    mtr_p->mtr_unit));
	/*	Get PCI information from connect_vertex.	*/
	mtr_p->mtr_pciio_info_p	= pciio_info_get(connect_vertex);
	mtr_p->mtr_slot		= pciio_info_slot_get(mtr_p->mtr_pciio_info_p);
	mtr_p->mtr_vendor_id	= 
		pciio_info_vendor_id_get(mtr_p->mtr_pciio_info_p);
	mtr_p->mtr_dev_id	= 
		pciio_info_device_id_get(mtr_p->mtr_pciio_info_p);

	INIT_MTR_DBG_PRINT2( 
			("mtr%d: if_ptrattach() device_desc_default_set().\n",
			    mtr_p->mtr_unit));
	mtr_p->mtr_dev_desc = device_desc_dup(vhdl_to_dev(connect_vertex));
	device_desc_default_set(vhdl_to_dev(connect_vertex),
				mtr_p->mtr_dev_desc);
	device_desc_intr_name_set(mtr_p->mtr_dev_desc, EDGE_LBL_MADGE_TR);
#if IP32 
	device_desc_intr_swlevel_set(mtr_p->mtr_dev_desc, (ilvl_t)spl5);
#endif /* IP32 */

	mtr_p->adapter.mtr_s16Mb = mtr_s16Mb;

	/* 	Configure PCI stuff */
	if( (error = PCI_setup_cfg( mtr_p ) != NO_ERROR) ) 
	  goto done;

	/* Configure adapter PIO map. */
	MTR_ASSERT( mtr_p->mtr_mem_addr != NULL );
	adapter_p = &(mtr_p->adapter);
	sif_base  = (BYTE *)mtr_p->mtr_mem_addr + 
		((mtr_p->mtr_dev_id == PCI_PCI2_DEVICE_ID) ? 
		 PCI2_SIF_OFFSET : 0);
    	adapter_p->sif_dat  	= (WORD *)(sif_base + SGI_EAGLE_SIFDAT);
    	adapter_p->sif_datinc  	= (WORD *)(sif_base + SGI_EAGLE_SIFDAT_INC);
    	adapter_p->sif_adr  	= (WORD *)(sif_base + SGI_EAGLE_SIFADR);
    	adapter_p->sif_int  	= (WORD *)(sif_base + SGI_EAGLE_SIFINT);
    	adapter_p->sif_acl  	= (WORD *)(sif_base + SGI_EAGLE_SIFACL);
    	adapter_p->sif_adrx  	= (WORD *)(sif_base + SGI_EAGLE_SIFADX);
	MTR_DBG_PRINT2(mtr_p, ("mtr%d: sif_base: 0x%x.\n", 
		mtr_p->mtr_unit, sif_base));
	MTR_DBG_PRINT2(mtr_p, ("mtr%d: sif_dat: 0x%x.\n", 
		mtr_p->mtr_unit, adapter_p->sif_dat));
	MTR_DBG_PRINT2(mtr_p, ("mtr%d: sif_datinc: 0x%x.\n", 
		mtr_p->mtr_unit, adapter_p->sif_datinc));
	MTR_DBG_PRINT2(mtr_p, ("mtr%d: sif_adr: 0x%x.\n", 
		mtr_p->mtr_unit, adapter_p->sif_adr));
	MTR_DBG_PRINT2(mtr_p, ("mtr%d: sif_int: 0x%x.\n", 
		mtr_p->mtr_unit, adapter_p->sif_int));
	MTR_DBG_PRINT2(mtr_p, ("mtr%d: sif_acl: 0x%x.\n", 
		mtr_p->mtr_unit, adapter_p->sif_acl));
	MTR_DBG_PRINT2(mtr_p, ("mtr%d: sif_adrx: 0x%x.\n", 
		mtr_p->mtr_unit, adapter_p->sif_adrx));


	INIT_MTR_DBG_PRINT2( ("mtr%d: if_ptrattach() setup intr.\n",
			    mtr_p->mtr_unit));
	/* 	
	 *	Set up the interrupt in PCI infrastructure. 
	 */
	mtr_p->mtr_intr_hdl = pciio_intr_alloc(
		connect_vertex, 	/* connect vertex */
		mtr_p->mtr_dev_desc, 		/* my device desc */
		PCIIO_INTR_LINE_A, 		/* interrupt line */
		connect_vertex);
	if( mtr_p->mtr_intr_hdl == NULL ){
		cmn_err(CE_WARN, "mtr%d: pciio_intr_alloc(%d) failed!",
			mtr_p->mtr_unit);
		error = EIO;
		MTR_ASSERT( 0 );
		goto done;
	} else {
		MTR_DBG_PRINT2( mtr_p, 
			("mtr%d: pciio_intr_alloc() done.\n", 
			mtr_p->mtr_unit));
		SET_MTR_FLAG(mtr_p, MTR_FLAG_INTR_ALLOC);
	}

	if( pciio_intr_connect( mtr_p->mtr_intr_hdl, 
		(intr_func_t) mtr_intr, (intr_arg_t) ifmtr_p, NULL) < 0){
		cmn_err(CE_WARN, "mtr%d: pciio_intr_connect() failed!",
			mtr_p->mtr_unit);
		error = EIO;
		goto done;
	} else {
		MTR_DBG_PRINT2( mtr_p, 
			("mtr%d: pciio_intr_connect(0x%x) done.\n",
			mtr_p->mtr_unit, mtr_p));
		SET_MTR_FLAG(mtr_p, MTR_FLAG_INTR_DONE);
	}

	if( !new_mtr ){
		SET_MTR_FLAG( mtr_p, MTR_FLAG_NETWORK_REATTACHING);
	}
	error = mtr_attach_net(ifmtr_p); 

done:
	if( error == NO_ERROR ) {
		inventory_t 	*inv_ptr = NULL;

		/*	Update the hinv.	*/
		if (!(inv_ptr = find_inventory(inv_ptr, INV_NETWORK, 
					       INV_NET_TOKEN, 
					       INV_TOKEN_MTRPCI, 
					       ifnet_p->if_unit, 
					       mtr_p->mtr_dev_id))) {
			
			device_inventory_add(connect_vertex, 
					     INV_NETWORK, 
					     INV_NET_TOKEN, 
					     INV_TOKEN_MTRPCI, 
					     ifnet_p->if_unit,
					     mtr_p->mtr_dev_id); 
    		} else {
        		replace_in_inventory(inv_ptr, 
					     INV_NETWORK,
					     INV_NET_TOKEN,
					     INV_TOKEN_MTRPCI,
					     ifnet_p->if_unit,	
					     mtr_p->mtr_dev_id);
		}

		if( hwgraph_traverse(connect_vertex, EDGE_LBL_MADGE_TR, &tr_vertex) 
		   == GRAPH_NOT_FOUND ){

			rc = hwgraph_char_device_add(connect_vertex, 
						     EDGE_LBL_MADGE_TR, 
						     MY_PCI_PREFIX, 
						     &tr_vertex);

			ASSERT(rc == GRAPH_SUCCESS);
			if (rc == GRAPH_SUCCESS) {
				char loc_str[8];
				sprintf(loc_str, "%s%d", EDGE_LBL_MTR, 
					ifnet_p->if_unit);
				(void) if_hwgraph_alias_add(tr_vertex, loc_str);
			}
		}

		/*      Make an alias for /hw/mtr*    */
		sprintf(devnm, "%s%d", EDGE_LBL_MTR, ifnet_p->if_unit);
		if (hwgraph_traverse(hwgraph_root, devnm, &to) == 
		    GRAPH_NOT_FOUND) {
			rc = hwgraph_edge_add(hwgraph_root, connect_vertex, 
					      devnm);
			ASSERT(rc == GRAPH_SUCCESS); rc = rc;
		}

		MTR_DBG_PRINT2(mtr_p, ("mtr%d: attach(0x%x) done!\n", 
			mtr_p->mtr_unit, mtr_p));
	} else {
		release_resources( mtr_p );
		if( new_mtr &&  ifmtr_p ){
			if (hwgraph_info_get_LBL(connect_vertex, MTR_INFO_LBL,
						 (arbitrary_info_t *)&ifmtr_p) 
			    == GRAPH_SUCCESS)
			  hwgraph_info_remove_LBL(connect_vertex,
						  MTR_INFO_LBL, 
						  (arbitrary_info_t *)ifmtr_p);
			kmem_free( ifmtr_p, sizeof(*ifmtr_p) );
		} /* if new_mtr */
	}

	INIT_MTR_DBG_PRINT2( ("mtr*: if_ptrattach() done with %d.\n", error));
	return (error);
} /* if_ptrattach() */

/*
 *	This routine is called indirectly by pciio_driver_unregister().
 *
 *	The resouces for the adapter are released.
 *	The adapter is disabled.
 */
int
if_ptrdetach(vertex_hdl_t connect_vertex)
{
	int		error = NO_ERROR;
	int		both_locked = 0;
	if_mtr_t	*ifmtr_p;
	mtr_private_t	*mtr_p;
	struct ifnet	*ifnet_p;
	vertex_hdl_t	tr_vertex;
	char            name[MAXDEVNAME];
	dev_t           alias;

	INIT_MTR_DBG_PRINT(("mtr*: if_ptrdetach(0x%x) called.\n", 
			    connect_vertex));

	if( hwgraph_info_get_LBL(connect_vertex, MTR_INFO_LBL, 
				 (arbitrary_info_t *)&ifmtr_p)
	   != GRAPH_SUCCESS ){
		cmn_err(CE_WARN, "if_ptrdetach(0x%x) connect_vertex!", 
			connect_vertex);
		error = EIO;
		goto done;
	}
	
	/* Unregister the error handler */
	pciio_error_register(connect_vertex, NULL, NULL);
	
	mtr_p = ifmtr_p->mtr_p;
	if( mtr_p == NULL ){
		cmn_err(CE_WARN, "if_ptrdetach(0x%x): 0x%x not found!",
			connect_vertex, ifmtr_p);
		error = EIO;
		goto done;	
	}
	
	ifnet_p = mtr_2_ifnet(mtr_p);

	/*
	 *	If any initialization or shuting down operation is
	 *	being conducted, we wait until that is done.
	 */
	while( !both_locked ){
		IFNET_LOCK( ifnet_p);
		
		if( mutex_trylock( &mtr_p->init_mutex ) ){
			both_locked = 1;
		} else {
			MTR_ASSERT( !mutex_mine(&mtr_p->init_mutex) );
			IFNET_UNLOCK( ifnet_p);
		}
	} /* if */
	
	/*	Down the interface.	*/
	if( !IS_MTR_FLAG_SET(mtr_p, MTR_FLAG_STATE_HALTED)
	   && IS_MTR_FLAG_SET(mtr_p, MTR_FLAG_STATE_STARTED))
	  hwi_halt_eagle(mtr_p);

	PCI_disable_adapter( mtr_p );

	mutex_unlock( &mtr_p->init_mutex );

	if_down(ifnet);
	ifnet_p->if_flags 	&= ~IFF_ALIVE;
	ifnet_p->if_timer	=  0;
	ifnet_p->if_output 	=  
		(int(*)(struct ifnet *, struct mbuf *, struct sockaddr*, 
		struct rtentry *))nodev;
	ifnet_p->if_ioctl  	=  
		(int (*)(struct ifnet *, int, void *))nodev;
	ifnet_p->if_reset	=  (int (*)(int, int))nodev;
	ifnet_p->if_watchdog	=  (void(*)(struct ifnet *))nodev;

	/* remove the alias /hw/mtr* edge */
	sprintf(name, "%s%d", EDGE_LBL_MTR, mtr_p->mtr_unit);
	if (hwgraph_traverse(hwgraph_root, name, &alias) == GRAPH_SUCCESS) {
		hwgraph_edge_remove(hwgraph_root, name, 0);
	}
	(void) if_hwgraph_alias_remove(name);

	/* remove the connection edge */
	if( hwgraph_traverse(connect_vertex, EDGE_LBL_MADGE_TR, &tr_vertex) 
		== GRAPH_NOT_FOUND ){
		cmn_err(CE_WARN, "if_ptrdetach(0x%x) connect_vertex!", 
			connect_vertex);
		goto done;
	} else {
		hwgraph_edge_remove(connect_vertex, EDGE_LBL_MADGE_TR, 0);
		hwgraph_vertex_destroy(tr_vertex);
	}

	release_resources( mtr_p );
	ifnet_2_ifmtr(ifnet_p)->mtr_p	= NULL;
	IFNET_UNLOCK( ifnet_p);

done:
	return( error );
} /* if_ptrdetach() */

static	int
mtr_error( void *arg, int pci_error, ioerror_mode_t mode, ioerror_t *ioerror)
{
	if_mtr_t	*ifmtr_p = (if_mtr_t *)arg;
	mtr_private_t	*mtr_p;
	int		error = NO_ERROR;
	char		buf[16];

	INIT_MTR_DBG_PRINT( (
		"mtr*: mtr_error(0x%x, %u, %u, %u, 0x%x) called.\n", 
		arg, pci_error, mode, (__psunsigned_t)ioerror));

	MTR_ASSERT( ifmtr_p != NULL );
	mtr_p = ifmtr_p->mtr_p;
	MTR_ASSERT( mtr_p   != NULL );


	INIT_MTR_DBG_PRINT( ("mtr%d: mtr_error() 0x%x -> 0x%x.%x.%x.\n", 
			     mtr_p->mtr_unit, mtr_p->mtr_connect_vertex, 
			     ifmtr_p, mtr_p, (mtr_p ? mtr_p->mtr_unit : 0)) );

	sprintf(buf, "mtr%d ", mtr_p->mtr_unit);
	ioerror_dump(buf, pci_error, mode, ioerror);

	/* TO DO: */

	return( error );
} /* mtr_error() */

/*	
 *	The linkage for network's upper layer.
 *	WATCH OUT: They require IFNET_LOCK protection!!!
 */

/*
 *	mtr_reset() is called when the adapter is to be 
 *	stopped or restarted.
 */
/* ARGSUSED */
static int 
mtr_reset(int arg1, int arg2)
{
	MTR_ASSERT( 0 );		/* no one should call this!	*/
	return(EIO);
} /* mtr_reset() */

/*
 *	Watchdog routine when if_timer is set.
 */
static void
mtr_watchdog(struct ifnet *ifp)
{
	mtr_private_t	*mtr_p;

	mtr_p = ifnet_2_mtr(ifp);
	if( mtr_p == NULL || mtr_p->tag != MTR_TAG ){
		cmn_err(CE_ALERT, "mtr*: mtr_watchdog(%d)!", ifp->if_unit);
		MTR_ASSERT(0);
		goto done;
	}

	MTR_ASSERT( IS_MTR_FLAG_SET( mtr_p, MTR_FLAG_NETWORK_ATTACHED) );

done:
	return;
} /* mtr_watchdog() */

static int
mtr_send(mtr_private_t *mtr_p, struct mbuf *data_p)
{
	int		error = NO_ERROR;
	int		len_s, len_l;
	ushort_t	len, len2;
	mtr_adapter_t	*adapter_p = &(mtr_p->adapter);

	ushort_t		active_tx_slot = adapter_p->active_tx_slot;
	TX_SLOT		**tx_slot_array = adapter_p->tx_slot_array;

	if( data_p == NULL)goto done;
	MTR_VERIFY_PROTECTION( mtr_2_ifnet( mtr_p ) );

#ifdef	DEBUG2
	if( IS_MTR_DEBUG2(mtr_p) )MTR_DUMP_PKT(mtr_p, data_p, "TX");
#endif	/* DEBUG2 */

	if( (len = m_length(data_p)) >	MAX_TX_LEN(mtr_p) ){
		cmn_err(CE_NOTE, "mtr%d: TX pkt: %d > %d!",
			mtr_p->mtr_unit, len, MAX_TX_LEN(mtr_p));
		error = EMSGSIZE;
		MTR_ASSERT(0);
		goto done;
	}

	MTR_VERIFY_ADX( mtr_p );
	OUTS(adapter_p->sif_adr, (__psunsigned_t)
	     &(tx_slot_array[active_tx_slot]->large_buffer_len));
	len_l = INS(adapter_p->sif_dat);
	OUTS(adapter_p->sif_adr, (__psunsigned_t)
	     &(tx_slot_array[active_tx_slot]->small_buffer_len));
	len_s = INS(adapter_p->sif_dat);

	/*	Is there a free TX slot?	*/
	if( (len_s != 0) || (len_l != 0)){

		/*	
		 *	TO DO:
		 *	queue the packet instead of discarding it?!
		 */
		if( !(++mtr_p->tx_queue_full & 0x1f) ){
			cmn_err(CE_DEBUG, 
				"mtr%d: TX full: %d.%d.%d.%d!\n",
				mtr_p->mtr_unit, 
				mtr_p->tx_queue_full,
				active_tx_slot, 
				len_l, len_s);
			if( !(mtr_p->tx_queue_full & (512-1)) )
				MTR_DUMP_TX_RX_SLOTS(mtr_p, "TX");
		}
		MTR_DBG_PRINT2(mtr_p, ("mtr%d: TX full: %d.%d.%d!\n", 
			mtr_p->mtr_unit, active_tx_slot, len_l, len_s));
		error = ENOBUFS;
		goto done;
	}
	mtr_p->tx_queue_full = 0;

	/*	Copy the data into the buffer associated with 
	 *	the free TX slot.
	 */
	len2 = m_datacopy(data_p, 0, MAX_TX_LEN(mtr_p),
		(caddr_t)mtr_p->adapter.tx_slot_buf[active_tx_slot]);
	MTR_ASSERT( len == len2 );
	dki_dcache_wbinval((caddr_t)mtr_p->adapter.tx_slot_buf[active_tx_slot],
		MAX_TX_LEN(mtr_p));

	/*	Update the length.			*/
	OUTS(adapter_p->sif_adr, (__psunsigned_t)
	     &(tx_slot_array[active_tx_slot]->large_buffer_len));	
	OUTS(adapter_p->sif_dat, len2);
	OUTS(adapter_p->sif_adr, (__psunsigned_t)
	     (& tx_slot_array[active_tx_slot]->small_buffer_len));
	OUTS(adapter_p->sif_dat, FMPLUS_SBUFF_ZERO_LENGTH);

	if(++active_tx_slot == adapter_p->init_block_p->fastmac_parms.tx_slots) 
		active_tx_slot = 0;
	adapter_p->active_tx_slot = active_tx_slot;

	mtr_2_ifnet(mtr_p)->if_opackets++;
	mtr_2_ifnet(mtr_p)->if_obytes += len2;

	MTR_DBG_PRINT2(mtr_p, ("mtr%d: TX done: %d, %d, %d.\n",
		mtr_p->mtr_unit, active_tx_slot,
		mtr_2_ifnet(mtr_p)->if_opackets,
		len2));
done:
	MTR_VERIFY_ADX( mtr_p );

	MTR_ASSERT( data_p != NULL && data_p->m_type != MT_FREE );
	return(error);
} /* mtr_send() */

/*
 *	mtr_af_sdl_output(): transmit AF_SDL packet.
 */
static int
mtr_af_sdl_output(
	struct ifnet	*ifnet_p, 
	struct mbuf 	*data_p, 
	struct sockaddr *dst_addr_p)
{
	int		error = NO_ERROR;
	mtr_private_t	*mtr_p;

	struct mbuf	*m_tmp = NULL;
	uchar_t		*map;
	int		sri_len = 0;
	ushort_t	*sri = NULL;
	TR_MAC		*mac_p;
	struct sockaddr_sdl	*sdl;

	MTR_ASSERT( dst_addr_p->sa_family == AF_SDL );
	mtr_p = ifnet_2_mtr(ifnet_p);
	MTR_ASSERT( mtr_p != NULL && mtr_p->tag == MTR_TAG );
	MTR_VERIFY_PROTECTION( ifnet_p );

	sdl 	= (struct sockaddr_sdl *)dst_addr_p;
	map 	= (uchar_t *)sdl->ssdl_addr;
	sri 	= (ushort *)&map[TR_MAC_ADDR_SIZE];
	sri_len = sdl->ssdl_addr_len - TR_MAC_ADDR_SIZE;
	if( sdl->ssdl_addr_len < ETHERADDRLEN ){
		if( ADAPTER_DEBUG_ENABLED( mtr_p ) ){
			cmn_err(CE_DEBUG, "mtr%d: mtr_output(): "
				"sa_family(0x%x: AF_SDL) "
				"but len: %d(!=%d)!\n",
				mtr_p->mtr_unit, dst_addr_p->sa_family,
				sdl->ssdl_addr_len, ETHERADDRLEN);
		}
		error = EAFNOSUPPORT;
		goto done;
	}

	if( (sri_len > 0) &&
	    (!(mtr_p->mtr_if_flags & IFF_ALLCAST) ||
	     sri_len > TR_MAC_RIF_SIZE || 
	     sri_len != (((*sri)&TR_RIF_LENGTH) >> 8)) ) {

		/*	SR is provided but ... */
		if( ADAPTER_DEBUG_ENABLED( mtr_p ) ){
			cmn_err(CE_DEBUG,
				"mtr%d: mtr_output(): AF_SDL && SRI: "
				"sri_len, %d, mtr_if_flags: 0x%x!\n",
				mtr_p->mtr_unit,
				sri_len,
				mtr_p->mtr_if_flags);
		}
		error = EAFNOSUPPORT;
		goto done;
	} else if( TOKEN_ISBROAD(map) ){
		map 	= mtr_p->mtr_baddr;
		sri 	= &mtr_p->adapter.nullsri;
		sri_len	= sizeof(mtr_p->adapter.nullsri);
	}

	if( (m_tmp = m_get(M_DONTWAIT, MT_DATA)) == 0 ){
		error = ENOBUFS;
		if( ADAPTER_DEBUG_ENABLED( mtr_p ) ){
			cmn_err(CE_DEBUG, "mtr%d: mtr_output(): "
				"AF_SDL: mget()",
				mtr_p->mtr_unit);
		}
		goto done;
	}

        m_tmp->m_len    = sizeof(*mac_p) + sri_len + TRPAD;
        MTR_ASSERT( m_tmp->m_len <= MLEN );
        m_tmp->m_next   = data_p;
        data_p          = m_tmp;

        m_adj(m_tmp, TRPAD);

        /*      Build MAC header.       */
        mac_p = mtod(m_tmp, TR_MAC *);
        mac_p->mac_ac = TR_AC_FRAME;
        mac_p->mac_fc = TR_FC_TYPE_LLC;
        bcopy(mtr_p->mtr_enaddr, mac_p->mac_sa, TR_MAC_ADDR_SIZE);
        bcopy(map, mac_p->mac_da, TR_MAC_ADDR_SIZE);

        /*      Build SRI, if any.      */
        if( sri_len > 0 ){
                uchar_t *tsri = ((uchar_t *)mac_p) + sizeof(*mac_p);

                bcopy( (uchar_t *)sri, tsri, sri_len);
                mac_p->mac_sa[0] |=  TR_MACADR_RII;
        }

        if( RAWIF_SNOOPING( &(mtr_p->mtr_rawif) ) ){
                int     pkt_len = m_length(data_p);

                MTR_ASSERT( data_p->m_len == (sizeof(TR_MAC) + sri_len) );
                data_p->m_off   -= TRPAD;
                data_p->m_len   += TRPAD;
                IF_INITHEADER(mtod(data_p, caddr_t), ifnet_p, data_p->m_len);
                mtr_snoop_input(mtr_p, data_p, sri_len, pkt_len);
                m_adj(data_p, TRPAD);
        }

	error = mtr_send(mtr_p, data_p);

done:

	MTR_ASSERT( data_p != NULL && data_p->m_type != MT_FREE );
	if( m_tmp != NULL ){
		(void)m_free(m_tmp);
	}
	return( error );
} /* mtr_af_sdl_output() */

/*
 *	Transmit a packet.
 * 
 */

static int 
mtr_output(
	struct ifnet	*ifnet_p, 
	struct mbuf 	*data_p, 
	struct sockaddr *dst_addr_p,
	struct rtentry *rt)
{
	int		error = NO_ERROR;
	mtr_private_t	*mtr_p = ifnet_2_mtr(ifnet_p);

	int		hdr_len;
	uchar_t		mac_addr[TR_MAC_ADDR_SIZE];
	struct in_addr	*idst;
	struct mbuf	*m_loop = NULL, *m_tmp;
	uchar_t		*map = NULL;
	int		etype;
	int		sri_len = 0;
	ushort_t	*sri = NULL;
	int		pkt_len;
	TR_MAC		*mac_p;
	LLC1		*llc_p;
	SNAP		*snap_p;

	if( mtr_p == NULL || mtr_p->tag != MTR_TAG ){
		cmn_err(CE_ALERT, "mtr*: mtr_output(0x%x,%d)!",
			ifnet_p, ifnet_p->if_unit);
		MTR_ASSERT(0);
		error = EIO;
		goto done;
	}
	
	MTR_VERIFY_PROTECTION( ifnet_p );
	MTR_DBG_PRINT2( mtr_p, ("mtr%d: mtr_output() called: %d.\n", 
		mtr_p->mtr_unit, (data_p ? m_length(data_p) : 0)));

	pkt_len = m_length(data_p);
	if( iff_dead(ifnet_p->if_flags) ){
		MTR_DBG_PRINT(mtr_p, 
			("mtr%d: mtr_output(): 0x%x.%x -> discard!\n",
			mtr_p->mtr_unit,
			(mtr_p->ifmtr_p)->flags, ifnet_p->if_flags));
		error = ENETDOWN;
		goto done;
	}

	switch( dst_addr_p->sa_family ){
	case AF_INET:
		MTR_DBG_PRINT2( mtr_p, ("mtr%d: mtr_output(): AF_INET.\n", 
			mtr_p->mtr_unit));

		idst = &((struct sockaddr_in *)dst_addr_p)->sin_addr;
		if( !arpresolve(&mtr_p->mtr_ac, rt, data_p, 
			idst, mac_addr, &error) ){

			MTR_DBG_PRINT2( mtr_p, 
				("mtr%d: mtr_output(): arpresolve().\n", 
				mtr_p->mtr_unit));
			MTR_ASSERT(m_loop == NULL );
			MTR_ASSERT(data_p != NULL );
			MTR_ASSERT( error == NO_ERROR );
			goto done_without_freeing_mbuf;
		}

		/* 	ARP is done!	*/
		if( TOKEN_ISBROAD(mac_addr) ){
			/*	loop back the packet.	*/
			if( (m_loop = m_copy(data_p, 0, M_COPYALL)) == NULL){
				error = ENOBUFS;
				goto done;	
			}
			map = mtr_p->mtr_baddr;
		} else {
			map = mac_addr;
		}
		etype = ETHERTYPE_IP;
		goto 	prepare_sri;

	case AF_UNSPEC:
		MTR_DBG_PRINT2( mtr_p, ("mtr%d: mtr_output(): AF_UNSPEC.\n",
			mtr_p->mtr_unit));

#define	EP	((struct ether_header *)&dst_addr_p->sa_data[0])
		/*	For ARP packet.		*/
		etype	= EP->ether_type;
		map = (uchar_t * )&EP->ether_dhost[0];
		if (TOKEN_ISBROAD(map)) {
			map = mtr_p->mtr_baddr;
		}
#undef	EP
prepare_sri:
		/* 	prepare RIF iff requested */
		if ((ifnet_p->if_flags & IFF_ALLCAST) == 0) {
			MTR_DBG_PRINT2( mtr_p, 
				("mtr%d: mtr_output(): IFF_ALLCAST.\n",
				mtr_p->mtr_unit));
			sri_len = 0;
			break;
		}

		if (TOKEN_ISGROUP(map)) {

			/* This could be a IP broadcast or ARP packet.	*/
			sri 	= &mtr_p->adapter.nullsri;
			sri_len	= sizeof(mtr_p->adapter.nullsri);
			MTR_DBG_PRINT2( mtr_p, 
				("mtr%d: mtr_output(): group.\n",
				mtr_p->mtr_unit));
		} else if( (sri = srilook((char *)map, 1)) != 0) {

			/* This is an unicat to remote ring */
			sri_len = ((*sri) & TR_RIF_LENGTH) >> 8;
			MTR_DBG_PRINT2( mtr_p, 
				("mtr%d: mtr_output(): unicast to remote.\n",
				mtr_p->mtr_unit));
		} else {

			/* This is an unicat to local ring */
			sri_len = 0;
			MTR_DBG_PRINT2( mtr_p, 
				("mtr%d: mtr_output(): unicast to local.\n",
				mtr_p->mtr_unit));
		}
		break;

	case AF_SDL:
		error = mtr_af_sdl_output( ifnet_p, data_p, dst_addr_p );
		goto done;

	default:
		cmn_err(CE_WARN, 
			"mtr%d: mtr_output(): sa_family(0x%x) not supported.",
			mtr_p->mtr_unit, dst_addr_p->sa_family);
		error = EAFNOSUPPORT;
		goto done;
	} /* switch() */
	MTR_ASSERT( !(sri_len & 1) && sri_len <= TR_MAC_RIF_SIZE );
	MTR_ASSERT( map != NULL );
	MTR_DBG_PRINT2( mtr_p, ("mtr%d: mtr_output(): sri_len: %d.\n",
		mtr_p->mtr_unit, sri_len));

	/* 	Allocate space for MAC, SRI, LLC, and SNAP header */
	if ((m_tmp = m_get(M_DONTWAIT, MT_DATA)) == 0) {
		/*	TO DO: 
		 *	logging.
		 */
		error = ENOBUFS;
		goto done;
	}
	hdr_len =  sizeof(*mac_p) + sizeof(*llc_p) + sizeof(*snap_p) + sri_len;
	pkt_len	+= hdr_len;

	m_tmp->m_next 	= data_p;
	m_tmp->m_len	= hdr_len + TRPAD;
	m_adj(m_tmp, TRPAD);
	data_p		= m_tmp;
	m_tmp 		= NULL;

	/* 	Setup MAC header */
	mac_p 		= mtod(data_p, TR_MAC *);
	mac_p->mac_ac	= TR_AC_FRAME;
	mac_p->mac_fc	= TR_FC_TYPE_LLC;
	bcopy(map, mac_p->mac_da, TR_MAC_ADDR_SIZE);
	bcopy(mtr_p->mtr_enaddr, mac_p->mac_sa, TR_MAC_ADDR_SIZE);

	/*	setup SRCROUTE info */
	if( sri_len > 0 ){
		uchar_t	*tsri = ((uchar_t *)mac_p) + sizeof(*mac_p);
		bcopy( (uchar_t *)sri, tsri, sri_len);
		mac_p->mac_sa[0] |=  TR_MACADR_RII;
	}

	/*	setup LLC header */
	llc_p	= (LLC1 *)(((uchar_t *)mac_p) + sizeof(*mac_p) + sri_len);
	llc_p->llc1_dsap	= TR_IP_SAP;
	llc_p->llc1_ssap	= TR_IP_SAP;
	llc_p->llc1_cont	= RFC1042_CONT;
	
	/*	setup SNAP */
	snap_p 	= (SNAP *)(((uchar_t *)llc_p) + sizeof(*llc_p));
	snap_p->snap_org[0] = RFC1042_K2;	
	snap_p->snap_org[1] = RFC1042_K2;
	snap_p->snap_org[2] = RFC1042_K2;
	snap_p->snap_etype[0] = etype >> 8;
	snap_p->snap_etype[1] = etype &  0xff;

	/*	Send the packet */
	if( m_loop ){
		ifnet_p->if_omcasts++;
		looutput(&loif, m_loop, dst_addr_p, rt);
		if( IS_MTR_DEBUG2(mtr_p) )
			MTR_DUMP_PKT(mtr_p, m_loop, "TX");
		m_loop = NULL;
	} else if( TOKEN_ISGROUP(map) ){
		ifnet_p->if_omcasts++;
	}

	if( RAWIF_SNOOPING( &(mtr_p->mtr_rawif) ) ){
		MTR_ASSERT( data_p->m_len == (sizeof(TR_MAC) + 
			sizeof(LLC1) + sizeof(SNAP) + sri_len) );
		data_p->m_off	-= TRPAD;
		data_p->m_len	+= TRPAD;
		IF_INITHEADER(mtod(data_p, caddr_t), ifnet_p, data_p->m_len);
		mtr_snoop_input(mtr_p, data_p, sri_len, pkt_len);
		m_adj(data_p, TRPAD);
	}

	MTR_ASSERT( data_p->m_len == hdr_len );
        MTR_ASSERT( mtod(data_p, TR_MAC *)->mac_ac == TR_AC_FRAME);
	MTR_ASSERT( mtod(data_p, TR_MAC *)->mac_fc == TR_FC_TYPE_LLC);

	error = mtr_send(mtr_p, data_p);

done:
	MTR_ASSERT( data_p != NULL && data_p->m_type != MT_FREE );
	m_freem(data_p);
	if( error != NO_ERROR ){
		mtr_2_ifnet(mtr_p)->if_odrops++;
		mtr_2_ifnet(mtr_p)->if_oerrors++;
	}
	if( m_loop != NULL )m_freem(m_loop);

	MTR_DBG_PRINT2(mtr_p, 
		("mtr%d: TX done with %d.\n", mtr_p->mtr_unit, error));

done_without_freeing_mbuf:
	return(error);
} /* mtr_output() */

#define	MTR_ILLEGAL_IP_ADDRESS	0x08000000	/* bug found in 6.2 */
/*
 *	Ioctl from socket layer, not from file system directly.
 */
#define SETSPEED   0x01
#define SETMTU     0x02
#define SETIFADDR  0x04

static int
mtr_ioctl(
	struct ifnet *ifnet_p,
	int cmd,
	void *data)
{
	int		error = NO_ERROR;
	mtr_private_t	*mtr_p = ifnet_2_mtr(ifnet_p);
	struct ifreq	*ifr_p;
	mtr_adapter_t	*adapter_p = &mtr_p->adapter;	
	uint_t          if_alive = IS_MTR_IFF_SET(mtr_p, IFF_ALIVE);
	struct sockaddr *sockaddr_p = (struct sockaddr *)data;
	uint_t          functional_address;

	MTR_ASSERT( mtr_p != NULL && mtr_p->tag == MTR_TAG );

	MTR_VERIFY_PROTECTION( mtr_2_ifnet( mtr_p ) );
	MTR_ASSERT(&mtr_p->mtr_ac == (struct arpcom *)ifnet_p);

	if( !( mtr_p->ifmtr_p->flags & MTR_FLAG_NETWORK_ATTACHED) ){
		error = EIO;
		goto done;
	}

	ifr_p = (struct ifreq *)data;
	switch( cmd ){
	case SIOC_TR_GETSPEED:
		ifr_p->ifr_metric = ( mtr_p->adapter.mtr_s16Mb ? 16 : 4);
		break;	

	case SIOCGIFMTU:
		ifr_p->ifr_metric = mtr_p->mtr_if_mtu;
		break;

	case SIOCSIFBRDADDR:
		switch( ifr_p->ifr_addr.sa_family ){
		case AF_RAW:
			if( !TOKEN_ISBROAD(ifr_p->ifr_addr.sa_data) ){
				error = EINVAL;
				break;
			}

			bcopy( ifr_p->ifr_addr.sa_data,
				mtr_p->mtr_baddr, sizeof(mtr_p->mtr_baddr));

			if( showconfig ) report_Configuration(mtr_p);
			break;

		case AF_INET:
			break;

		default:
			error = EOPNOTSUPP;
			break;
		}
		break;

	case SIOCGIFBRDADDR:
		switch( ifr_p->ifr_addr.sa_family ) {
		case AF_RAW:
			bcopy( mtr_p->mtr_baddr, ifr_p->ifr_addr.sa_data,
				TR_MAC_ADDR_SIZE);
			break;

		case AF_INET:
			break;

		default:
			error = EOPNOTSUPP;
			break;
		}
		break;

	case SIOCSIFADDR:

		switch( _IFADDR_SA(data)->sa_family ){
		case AF_INET:

			if( ((struct sockaddr_in *)(_IFADDR_SA(data)))->
				sin_addr.s_addr == INADDR_ANY ){
				error = EINVAL;
				break;
			}
	
			mtr_p->mtr_ac.ac_ipaddr =
				((struct sockaddr_in *)(_IFADDR_SA(data)))->
				sin_addr;
			((struct arpcom *)ifnet_p)->ac_ipaddr =
				mtr_p->mtr_ac.ac_ipaddr;

			MTR_ASSERT( mtr_p->mtr_ac.ac_ipaddr.s_addr !=
				MTR_ILLEGAL_IP_ADDRESS);

			if( if_alive ){
				/*      Update ARP cache.       */
				ARP_WHOHAS((struct arpcom *)ifnet_p,
					&((struct arpcom *)ifnet_p)->ac_ipaddr);
			}
			break;

		case AF_RAW: {
                        SRB_GENERAL     srb_general;

                        adapter_p = &mtr_p->adapter;

                        /*
                         *      TO DO: one-way ticket!?
                         */
                        bzero(&srb_general, sizeof(srb_general));

                        bcopy( _IFADDR_SA(data)->sa_data,
                                mtr_p->mtr_enaddr,
                                sizeof(mtr_p->mtr_enaddr));
                        bcopy( _IFADDR_SA(data)->sa_data,
                                mtr_p->mtr_rawif.rif_addr,
                                mtr_p->mtr_rawif.rif_addrlen);

                        srb_general.header.function = OPEN_ADAPTER_SRB;

                        srb_general.open_adap.open_options      =
                                IS_MTR_FLAG_SET(mtr_p, MTR_FLAG_STATE_PROMISC) ?                                (OPEN_OPT_COPY_ALL_MACS |
                                 OPEN_OPT_COPY_ALL_LLCS) : 0;
                        srb_general.open_adap.open_options      |=
                                mtr_participant_claim_token ?
                                        OPEN_OPT_CONTENDER : 0;;

                        bcopy( mtr_p->mtr_enaddr,
                                &srb_general.open_adap.open_address,
                                sizeof(srb_general.open_adap.open_address));
                        bcopy( &mtr_p->adapter.group_address,
                                &srb_general.open_adap.group_address,
                                sizeof(srb_general.open_adap.group_address));
                        bcopy( &mtr_p->adapter.functional_address,
                                &srb_general.open_adap.functional_address,
                                sizeof(srb_general.open_adap.functional_address));
                        if(mtr_reopen_adapter(mtr_p, &srb_general) != NO_ERROR){                                cmn_err(CE_ALERT,
                                        "mtr%d: SIOCSIFADDR(AF_RAW) failed",
                                        mtr_p->mtr_unit);
                        } else {
                                /*      Update ARP cache.       */
                                if( mtr_p->mtr_ac.ac_ipaddr.s_addr !=
                                    INADDR_ANY ){
                                        ARP_WHOHAS((struct arpcom *)ifnet_p,
                                        &((struct arpcom *)ifnet_p)->ac_ipaddr);                                }
                                if( showconfig ) report_Configuration(mtr_p);
                        }

                	} /* end of SIOCSIFADDR(AF_RAW) block */
			break;

		default:
			error = EOPNOTSUPP;
			break;
		}
		break;  /* _IFADDR_SA(data)->sa_family) */

	case SIOC_TR_RESTART: {
		int			idx;
		int			new_mtu;
		int			new_s16Mb;
		uchar_t			new_enaddr[TR_MAC_ADDR_SIZE];
		mtr_restart_req_t	*restart_req;
		mtr_cfg_req_t		*req_p;

		mtr_p->adapter.fmplus_image = NULL;

		if(!mutex_trylock(&mtr_p->init_mutex)) {
			error = EBUSY;
			goto restart_adapter_done;
		}

		new_mtu   = mtr_p->mtr_if_mtu;
		new_s16Mb = mtr_p->adapter.mtr_s16Mb;
		bcopy( mtr_p->mtr_enaddr, new_enaddr, sizeof(new_enaddr));

		restart_req = (mtr_restart_req_t *)data;

		/*	Get all the parameters ready.	*/
		for( idx = 0;
		     idx < MAX_CFG_PARAMS_PER_REQ && error == NO_ERROR; 
		     idx++){

			req_p = &restart_req->reqs[idx];

			switch( req_p->cmd ){
			case MTR_CFG_CMD_SET_SPEED: 	/* SIOC_TR_SETSPEED: */
				if( req_p->param.ring_speed != 16 && 
				    req_p->param.ring_speed != 4 ){
					error = EINVAL;
					cmn_err(CE_DEBUG, "mtr%d: "
						"MTR_CFG_CMD_SET_SPEED "
						"EINVAL: %d!\n",
						mtr_p->mtr_unit, 
						req_p->param.ring_speed);
				} else {
					new_s16Mb = 
						(req_p->param.ring_speed == 16);
				}
				break;

			case MTR_CFG_CMD_SET_MTU:	/* SIOCSIFMTU */
				if( req_p->param.mtu > TRMTU_16M ){
					error = EINVAL;
					cmn_err(CE_DEBUG, "mtr%d: "
						"MTR_CFG_CMD_SET_MTU EINVAL!\n",
						mtr_p->mtr_unit);
				} else {
					new_mtu = req_p->param.mtu;
				}
				break;

			case MTR_CFG_CMD_NULL:
			default:
				break;;
			}
			
		} /* for idx */
		if( error != NO_ERROR ) {
			goto restart_adapter_unlock_done;
		}

		/*	Verify the new parameters.	*/
		if( (new_s16Mb    && new_mtu > TRMTU_16M) ||
		    ((!new_s16Mb) && new_mtu > TRMTU_4M) ){
			error = EINVAL;
			cmn_err(CE_DEBUG, "mtr%d: %d != f(%d) EINVAL!\n",
				mtr_p->mtr_unit, new_mtu, new_s16Mb);
			goto restart_adapter_unlock_done;
		}

		if( restart_req->dlrec.fmplus_image == 0x0){
			error = EINVAL;
			cmn_err(CE_DEBUG, "mtr%d: fmplus_image(%d) EINVAL!\n",
				mtr_p->mtr_unit, 
				restart_req->dlrec.fmplus_image);
			goto restart_adapter_unlock_done;
		}

		/*	TO DO: to recover the configuration
		 *	from the failure. The problem is that
		 *	we probably won't be able to set the
		 *	original configuration back either!
		 */
		mtr_p->adapter.mtr_s16Mb = new_s16Mb;
		ifnet_p->if_baudrate.ifs_value = new_s16Mb ? 16*1000*1000 :
			4*1000*1000;
		mtr_p->mtr_if_mtu	 = new_mtu;
		bcopy( new_enaddr, mtr_p->mtr_enaddr, 
			sizeof(mtr_p->mtr_enaddr));
		bcopy( new_enaddr, mtr_p->mtr_rawif.rif_addr, 
			mtr_p->mtr_rawif.rif_addrlen);

		if( (error = driver_prepare_adapter(mtr_p)) != NO_ERROR ){
			cmn_err(CE_NOTE,
				"mtr%d: prepare_adapter: %d!",
				mtr_p->mtr_unit, error);
			goto restart_adapter_unlock_done;
		}

		/*	Get the firmware image.	*/
		mtr_p->adapter.sizeof_fmplus_array =
			restart_req->dlrec.sizeof_fmplus_array;
		mtr_p->adapter.recorded_size_fmplus_array =
			restart_req->dlrec.recorded_size_fmplus_array;
		mtr_p->adapter.fmplus_checksum	= 
			restart_req->dlrec.fmplus_checksum;
		if( restart_req->dlrec.sizeof_fmplus_array !=
		    restart_req->dlrec.recorded_size_fmplus_array ){
			cmn_err(CE_WARN, "mtr%d: dlrec: %d != %d!",
				mtr_p->mtr_unit,
				restart_req->dlrec.sizeof_fmplus_array,
				restart_req->dlrec.recorded_size_fmplus_array);
			cmn_err(CE_PANIC, "mtr%d: restart_req: 0x%x!\n",
				mtr_p->mtr_unit, restart_req);
			error = EINVAL;
			goto restart_adapter_unlock_done;
		}
		mtr_p->adapter.fmplus_image = kmem_alloc(
			mtr_p->adapter.sizeof_fmplus_array, KM_SLEEP);
		if( mtr_p->adapter.fmplus_image == NULL ){
			cmn_err(CE_WARN,
				"mtr%d: SIOC_TR_RESTART: kmem_alloc()!",
				mtr_p->mtr_unit);
			error = ENOBUFS;
			goto restart_adapter_unlock_done;
		}
		copyin((caddr_t)restart_req->dlrec.fmplus_image, 
			mtr_p->adapter.fmplus_image,
			mtr_p->adapter.sizeof_fmplus_array);

		if( (error = driver_start_adapter(mtr_p)) == NO_ERROR ){
			if( mtr_p->mtr_ac.ac_ipaddr.s_addr != INADDR_ANY ) {
				ARP_WHOHAS((struct arpcom *)ifnet_p,
					&((struct arpcom *)ifnet_p)->ac_ipaddr);
			}
			if( showconfig ) report_Configuration(mtr_p);
		} else {
			cmn_err(CE_ALERT, 
				"mtr%d: SIOC_TR_RESTART failed: %d!",
				mtr_p->mtr_unit, error);
		}

		
restart_adapter_unlock_done:
		mutex_unlock( &mtr_p->init_mutex );

restart_adapter_done:
		if( mtr_p->adapter.fmplus_image != NULL ){
			kmem_free(mtr_p->adapter.fmplus_image,
				mtr_p->adapter.sizeof_fmplus_array);
		}

		} /* end of SIOC_TR_RESTART block */
		break;	

	case SIOCSIFFLAGS: 
		ifr_p = (struct ifreq *)data;
		
		if( ((ifr_p->ifr_flags & IFF_PROMISC) &&
		     !IS_MTR_FLAG_SET(mtr_p, MTR_FLAG_STATE_PROMISC)) ||
		    (!(ifr_p->ifr_flags & IFF_PROMISC) &&
		     IS_MTR_FLAG_SET(mtr_p, MTR_FLAG_STATE_PROMISC)) ){
			if( ifr_p->ifr_flags & IFF_PROMISC ){
				SET_MTR_FLAG( mtr_p, MTR_FLAG_STATE_PROMISC );
			} else {
				RESET_MTR_FLAG( mtr_p, MTR_FLAG_STATE_PROMISC);
			}
			error = mtr_modify_open_parms(mtr_p);
		}
		break;

        case SIOC_MTR_GETFUNC:
                switch( ifr_p->ifr_dstaddr.sa_family ){
                case AF_RAW: 
                        *((uint_t *)ifr_p->ifr_dstaddr.sa_data)  = 
                                mtr_p->adapter.functional_address;
                        break;
                default:
                        error = EAFNOSUPPORT;
                        break;
                }
                break;

        case SIOC_MTR_ADDFUNC:
        case SIOC_MTR_DELFUNC:
		if( !if_alive ){
			/*	Sorry, we need to talk the adapter.	*/
			error = ENETDOWN;
			break;
		}

		if( ifr_p->ifr_addr.sa_family != AF_RAW ){
			error = EAFNOSUPPORT;
			cmn_err(CE_DEBUG,
				"mtr%d: SIOC_MTR_...FUNC: %d, error: %d!\n",
				mtr_p->mtr_unit, ifr_p->ifr_addr.sa_family,
				error);
			break;
		}

		functional_address = *((uint_t *)ifr_p->ifr_dstaddr.sa_data);
		error = mtr_functional_address( mtr_p, cmd, functional_address);
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
#define         IP_MULTICAST_FUNCTIONAL_ADDRESS         0x00040000
		
                if( !if_alive ){
                        error = ENETDOWN;
                        break;
                }

                switch( sockaddr_p->sa_family ){
                case AF_INET:

                        /*      Enable/disable functional address for IP
                         *      multicast for first/last request only.
                         */
                        if( cmd == SIOCADDMULTI){
                                MTR_ASSERT( adapter_p->ip_multicast_cnt >= 0 );

                                if( adapter_p->ip_multicast_cnt >= 1 ){

                                        /*      Already done.   */
                                        adapter_p->ip_multicast_cnt++;
                                        break;
                                }

                        } else if( cmd == SIOCDELMULTI ){
                                MTR_ASSERT( adapter_p->ip_multicast_cnt >= 0 );

                                /*      Not been enabled yet */
                                if( adapter_p->ip_multicast_cnt == 0 ) {
                                        error = EINVAL;
                                        break;
                                }

                                if( adapter_p->ip_multicast_cnt != 1 ){

                                        /*      Someone else still wants it */
                                        adapter_p->ip_multicast_cnt--;
                                        break;
                                }

                        } else {

                                /*      internal error! */
                                error = EIO;
                                MTR_ASSERT( 0 );
                                break;
                        }

                        functional_address = IP_MULTICAST_FUNCTIONAL_ADDRESS;

                        error = mtr_functional_address( mtr_p, cmd,
                                functional_address);

                        if( error == NO_ERROR && cmd == SIOCADDMULTI ){

                                MTR_ASSERT( adapter_p->ip_multicast_cnt == 0 );
                                MTR_SET_IFF( mtr_p, IFF_ALLMULTI);
                                adapter_p->ip_multicast_cnt++;

                        } else if( error == NO_ERROR && cmd == SIOCDELMULTI ) {

                                MTR_ASSERT( adapter_p->ip_multicast_cnt == 1 );
                                MTR_RESET_IFF( mtr_p, IFF_ALLMULTI);
                                adapter_p->ip_multicast_cnt--;
                        }

		case AF_RAW:
                        /*      The first two bytes of the group address
                         *      must be 0xc000. It is because I am too lazy
                         *      to set group_root_address and other token
                         *      ring drivers have the same requirement.
                         */
                        if( sockaddr_p->sa_data[0] != 0xc0 ||
                            sockaddr_p->sa_data[1] != 0x00  ) {
                                error = EINVAL;
				break;
                        }

                        if( sockaddr_p->sa_data[2] & 0x80) {
                                error = mtr_group_address(mtr_p, cmd,
                                        &sockaddr_p->sa_data[2]);
                                MTR_DBG_PRINT(mtr_p,
                                        ("mtr%d: mtr_group_address() "
                                         "error: 0x%x.\n",
                                         mtr_p->mtr_unit, error));
                                break;
                        } else {
                                /*      It's a functional address. */
                                bcopy(&sockaddr_p->sa_data[2],
                                        &functional_address,
                                        sizeof(functional_address));
                                error = mtr_functional_address( mtr_p,
                                        cmd, functional_address);
                        }
                        break;

		default:
			error = EAFNOSUPPORT;
			cmn_err(CE_DEBUG,
				"mtr%d: SIOC...MULTI: %d: error: %d!\n",
				mtr_p->mtr_unit, sockaddr_p->sa_family, error);
			break;
                } /* switch sockaddr_p->sa_family */
		break;

        case SIOC_TR_GETIFSTATS:

                if( !if_alive ){
                        error = ENETDOWN;
                        goto read_error_log_done;
                }

		error = mtr_read_error_log(mtr_p);

                if( error == NO_ERROR ){
#define srb_error_log   err_log.error_log
                        bcopy( &adapter_p->srb_general2.srb_error_log,
                               &ifr_p->ifr_metric,
                               sizeof(adapter_p->srb_general2.srb_error_log));
#undef  srb_error_log
                }
read_error_log_done:
                break;

	default:
		error = EOPNOTSUPP;
		break;

	} /* switch */	

done:
	return( error );
} /* mtr_ioctl() */	

static void
mtr_snoop_input(mtr_private_t *mtr_p, 
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
	int		hlen;
	int		dlen, dlen2;	/* len of data in packet */
	int		clen;		/* copy size */
	caddr_t		dst;
	TR_MAC		*mac_p;
	LLC1		*llc_p = NULL;

	if( (mh_p = m_get(M_DONTWAIT, MT_DATA)) == 0 )goto snoop_done;
	bzero( mtod(mh_p, void *), MLEN );

	hlen	= TR_RAW_MAC_SIZE + sizeof(LLC1) + sizeof(SNAP);
	hdr_len = TRPAD + hlen + RAW_HDRPAD( (TRPAD + hlen) );
	h_off	= hdr_len - hlen;

	/*	MAC:	*/
	start_off	=  TRPAD;
	dst		=  mtod(mh_p, caddr_t) + h_off;
	m_datacopy(pkt_mh_p, start_off, sizeof(TR_MAC), dst);
	mac_p		=  (TR_MAC *)(mtod(pkt_mh_p, caddr_t) + start_off);
	start_off	+= sizeof(TR_MAC);	 
	dst		+= sizeof(TR_MAC);
	dlen		=  pkt_len - sizeof(TR_MAC);

	/*	RIF:	*/
	if( sri_len != 0 ) m_datacopy(pkt_mh_p, start_off, sri_len, dst);
	start_off	+= sri_len;	
	dst		+= sizeof(TR_RII); 	/* skip TR_RII */
	dlen		-= sri_len;

	/*	LLC1 and SNAP, if they exists	*/
	if( (mac_p->mac_fc & TR_FC_TYPE) == TR_FC_TYPE_LLC ){

		llc_p = (LLC1 *)((uchar_t *)mac_p + sizeof(*mac_p) + sri_len);
		if( llc_p->llc1_dsap == TR_IP_SAP ){

			/*	LLC and SNAP */
			clen = sizeof(LLC1) + sizeof(SNAP);
		} else {
		
			/*	LLC but no SNAP -> LLC1 is data not hdr */
			clen = 0;
		}
	} else {
		/*	MAC but not LLC */
		clen = 0;
	}
	if( clen ){
		m_datacopy(pkt_mh_p, start_off, clen, dst);
	}
	start_off	+= clen;
	dst		+= clen;
	dlen		-= clen;
	dlen2		=  dlen;

	/*	data for first mbuf	*/
	clen		=  min((pkt_len - start_off), (MLEN-h_off-start_off));
	start_off	=  mtod(pkt_mh_p, struct ifheader *)->ifh_hdrlen;
	done_len	=  m_datacopy(pkt_mh_p, start_off, clen, dst);
	mh_p->m_len	=  hdr_len + done_len;
	start_off	+= done_len;
	dlen		-= done_len;
	MTR_ASSERT(mh_p->m_next == NULL );
	
	IF_INITHEADER(mtod(mh_p, caddr_t), mtr_2_ifnet(mtr_p), hdr_len);

	mt_p		= mh_p;
	/*	copy the rest of data */
	while( dlen > 0 ){
		struct mbuf	*m_p;

		clen = (dlen > MCLBYTES) ? MCLBYTES : dlen;
		m_p = m_vget(M_DONTWAIT, clen, MT_DATA);
		if( m_p == NULL ){
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

	snoop_input( &(mtr_p->mtr_rawif), (int)0 /* snoopflags */, 
		(caddr_t)(mtod(mh_p, caddr_t) + h_off), mh_p, dlen2);

snoop_done:
	if (mh_p) m_freem(mh_p);
	return;
} /* mtr_snoop_input() */

static void		
mtr_drain_input(mtr_private_t *mtr_p, 
	struct mbuf	*pkt_mh_p,
	int		pkt_len,
	int 		sri_len, 
	u_int 		port)
{
	struct mbuf		*mh_p, *mt_p;
	int			done_len;
	int			hdr_size;
	int			hdr_len;
	int			h_off;
	int			start_off;
	int			hlen;
	int			dlen, dlen2;
	int			clen;
	caddr_t			dst;
	TR_MAC			*mac_p;
	struct snoopheader	sh;

	MTR_ASSERT( pkt_mh_p != NULL );
	if( (mh_p = m_get(M_DONTWAIT, MT_DATA)) == 0 )goto drain_done;
	bzero( mtod(mh_p, void *), MLEN );	/* overkill! */
	mh_p->m_next = 0;

	hdr_size= TRPAD - sizeof(struct snoopheader);
	hlen    = TR_RAW_MAC_SIZE + sizeof(LLC1) + sizeof(SNAP);
	hdr_len = hdr_size + hlen + RAW_HDRPAD( (hdr_size + hlen) );
	h_off   = hdr_len - hlen;

	/*      MAC:    */
	start_off	=  TRPAD;	/* src pkt always has TRPAD */
	dst		=  mtod(mh_p, caddr_t) + h_off;
	m_datacopy(pkt_mh_p, start_off, sizeof(TR_MAC), dst);
	mac_p		=  (TR_MAC *)dst;
	start_off	+= sizeof(TR_MAC);
	dst		+= sizeof(TR_MAC);
	dlen		=  pkt_len - sizeof(TR_MAC);

	/*	RIF:	*/
	if( sri_len != 0 ) m_datacopy(pkt_mh_p, start_off, sri_len, dst);
	start_off	+= sri_len;
	dst		+= sizeof(TR_RII);
	dlen		-= sri_len;

	/*	LLC1 and SNAP: never have!	*/
	dlen2		=  dlen;	/* real data size only, no hdr */
	MTR_ASSERT( dlen2 > 0 );	/* data size should never 0 */

	/*	fill the snoopheader 		*/
	sh.snoop_seq 		= mtr_p->mtr_drain_seq++;
	sh.snoop_flags		= 0;
	sh.snoop_packetlen	= (dlen2 + sizeof(sh));
#if _MIPS_SIM == _ABI64	
	irix5_microtime(&sh.snoop_timestamp);
#else	/* _ABI64 */
	microtime(&sh.snoop_timestamp);
#endif	/* _ABI64 */

	dst             =  mtod(mh_p, caddr_t) + hdr_len;
	MTR_ASSERT( ((__psunsigned_t)dst & 0x3) == 0);
	MTR_ASSERT( sizeof(sh) == 16 );
	bcopy( &sh, dst, sizeof(sh) );
	dst		+= sizeof(sh);
	
	/*      data for first mbuf     
	 *	src, start_off: skipping hdr in pkt_mh_p, set by IF_INITHEADER()
	 *	dst, dst: skipping hdr and snoop_header in mh_p,
	 *		determined by hdr_len and sizeof(sh).
	 *	len, clen: the minimum size of the data and the left space in
	 *		mbuf.
	 */
	MTR_ASSERT( ((__psunsigned_t)dst & 0x3) == 0);
	clen            =  min( dlen2, (MLEN - hdr_len - sizeof(sh) ));
	start_off	=  mtod(pkt_mh_p, struct ifheader *)->ifh_hdrlen;
	done_len	=  m_datacopy(pkt_mh_p, start_off, clen, dst);
	mh_p->m_len	=  hdr_len + done_len + sizeof(sh);
	start_off	+= done_len;
	dlen		-= done_len;

	IF_INITHEADER(mtod(mh_p, caddr_t), mtr_2_ifnet(mtr_p), hdr_len);

	mt_p		= mh_p;
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

#ifdef	DEBUG2
	MTR_DBG_PRINT2(mtr_p,
		("mtr%d: drain_input(0x%x, 0x%x, 0x%x, 0x%x): %d.%d.%d.%d "
		"0x%x.0x%x.%d.\n",
		mtr_p->mtr_unit, &(mtr_p->mtr_rawif), port,
		mac_p->mac_sa, mh_p, hdr_size, hdr_len, h_off, hlen,
		mh_p, mh_p->m_next, m_length(mh_p)));	
	if( IS_MTR_DEBUG2(mtr_p) )MTR_DUMP_PKT(mtr_p, mh_p, "drain");
#endif	/* DEBUG2 */

	drain_input( &(mtr_p->mtr_rawif), port, (caddr_t)mac_p->mac_sa, mh_p);

drain_done:
	if( pkt_mh_p )m_freem(pkt_mh_p);
	return;
} /* mtr_drain_input() */

/*
 *	Configure the PCI adapters.
 *
 */
static int
PCI_setup_cfg( mtr_private_t *mtr_p)
{
	int			error = NO_ERROR;
	int                     mem_io_register;
#ifndef	IP32
	int                     tmp;
#endif
	volatile caddr_t        cfg_addr;

	MTR_ASSERT( mtr_p != NULL && mtr_p->mtr_dev_desc != NULL );

	/*	get the address of configuration space */
	mtr_p->cfg_piomap_p = 0;
	mtr_p->mtr_cfg_addr = pciio_pio_addr(mtr_p->mtr_connect_vertex,
					     0, PCIIO_SPACE_CFG, 0,
					     PCI_CFG_VEND_SPECIFIC + 0x10,
					     &(mtr_p->cfg_piomap_p), 0);
	if (!mtr_p->mtr_cfg_addr) {
		cmn_err(CE_WARN, "mtr%d: pciio_pio_addr()!",
			mtr_p->mtr_unit);
		error = ENOBUFS;
		goto done;
	}

	/* read PCI CFG space */
	PCI_get_dump_cfg(mtr_p, 1);

	for(mem_io_register = 0; mem_io_register < MTR_PCI_BASE_REGISTERS; 
	    mem_io_register++ ){
		if( mtr_p->org_pci_cfg_base_addr[mem_io_register] == 0 ){
			/*	no more valid base register!	*/
			mem_io_register = MTR_PCI_BASE_REGISTERS;
			break;
		}

#ifdef	USE_PCI_IO_ADDR_SPACE
		if( mtr_p->org_pci_cfg_base_addr[mem_io_register] & 0x1 ){
			/*	found it ! */
			break;
		}
#else	
		if( !(mtr_p->org_pci_cfg_base_addr[mem_io_register] & 0x1) ){
			/*	found it */
			break;
		}
#endif	/* USE_PCI_IO_ADDR_SPACE */
	} /* for */

	if( mem_io_register >= MTR_PCI_BASE_REGISTERS ){
		cmn_err(CE_ALERT, "mtr%d: no memory or io base register!",
			mtr_p->mtr_unit);
		error = EIO;
		goto done;
	}
	mtr_p->adapter.mem_io_register = mem_io_register;

#ifdef	USE_PCI_IO_ADDR_SPACE
	mtr_p->adapter.memory_size = 
		((mtr_p->org_pci_cfg_base_addr[mem_io_register] - 1 ) ^ 
		(pciaddr_t)-1) + 1;
#else	
	mtr_p->adapter.memory_size = 
		(mtr_p->org_pci_cfg_base_addr[mem_io_register] ^ 
		(pciaddr_t)-1) + 1;
#endif	/* USE_PCI_IO_ADDR_SPACE */

	MTR_DBG_PRINT( mtr_p, (
		"mtr%d: PCI_setup_cfg() mem_io_register: %d[0x%x], "
		"memory_size: %d.\n", mtr_p->mtr_unit,
		mtr_p->adapter.mem_io_register, 
		mtr_p->org_pci_cfg_base_addr[mtr_p->adapter.mem_io_register],
		mtr_p->adapter.memory_size) );

	/*	get the first memory address */
	mtr_p->io_piomap_p = 0;
	mtr_p->mtr_mem_addr = pciio_pio_addr(mtr_p->mtr_connect_vertex,
					     0, 
					     PCIIO_SPACE_WIN(mem_io_register),
					     0,
					     mtr_p->adapter.memory_size,
					     &(mtr_p->io_piomap_p),
					     0);
	if(!mtr_p->mtr_mem_addr) {
		cmn_err(CE_WARN, "mtr%d: pciio_piomap_alloc()!",
			mtr_p->mtr_unit);
		error = ENOBUFS;
		goto done;
	}	

	MTR_DBG_PRINT( mtr_p, 
		("mtr%d: io_piomap_p: 0x%x, mtr_mem_addr: 0x%x(%d).\n",
		mtr_p->mtr_unit,
		mtr_p->io_piomap_p,
		mtr_p->mtr_mem_addr,
		mtr_p->adapter.memory_size));

#if  IP32
	pciio_config_set(mtr_p->mtr_connect_vertex,
			 PCI_CFG_CACHE_LINE, 1, CACHELINE_128_BYTE);
	pciio_config_set(mtr_p->mtr_connect_vertex,
			 PCI_CFG_LATENCY_TIMER, 1, LATENCY_64_PCICLK);
	pciio_config_set(mtr_p->mtr_connect_vertex,
			 PCI_CFG_COMMAND, 2, 
			 (CMD_BUSMSTR_ENBL|CMD_IO_ENBL|CMD_MEM_ENBL));

#else
	cfg_addr = (caddr_t)(mtr_p->mtr_cfg_addr+PCI_CFG_CACHE_LINE);
	tmp      = PCI_INW( cfg_addr );
	tmp      = (tmp & ~(0xffff)) | CACHELINE_128_BYTE;
	tmp     |= (PCI_CFG_LATENCY_TIMER << 8);
	PCI_OUTW( cfg_addr, tmp );

	cfg_addr = (caddr_t)(mtr_p->mtr_cfg_addr+PCI_CFG_COMMAND);
	tmp      = PCI_INW( cfg_addr );
	tmp      = 
	  (tmp & ~(0xffff)) | (CMD_BUSMSTR_ENBL|CMD_IO_ENBL|CMD_MEM_ENBL);
	PCI_OUTW( cfg_addr, tmp );      
#endif

	SET_MTR_FLAG(mtr_p, MTR_FLAG_PIO_DONE);

	PCI_get_dump_cfg( mtr_p, 1);
done:
	/* if error, should release piomaps that have been done. */
	if (error) {
		if (mtr_p->cfg_piomap_p) {
			pciio_piomap_free(mtr_p->cfg_piomap_p);
			mtr_p->cfg_piomap_p = NULL;
		}
		if (mtr_p->io_piomap_p) {
			pciio_piomap_free(mtr_p->io_piomap_p);
			mtr_p->io_piomap_p = NULL;
		}
	}
	return(error);
} /* PCI_setup_cfg() */

/*
 *	Disable the adpater by disconnecting it from PCI bus.
 *	After this, the adapter must be recofigured before using it.
 *
 *	
 *	PCI 2.0 specification says:
 *
 *	"When a 0 is written to this register, the device is 
 *	 logcially disconnected from the PCI bus for all 
 *	 accesses except configuration accesses."
 *
 */
static void
PCI_disable_adapter( mtr_private_t *mtr_p )
{
	int			tmp;
	volatile caddr_t	cfg_addr;
	
	MTR_ASSERT( (mtr_p != NULL && mtr_p->mtr_cfg_addr != NULL) );
	
	cfg_addr = (caddr_t)(mtr_p->mtr_cfg_addr+PCI_CFG_COMMAND);
	tmp	 = PCI_INW( cfg_addr );
	tmp	 = (tmp & ~(0xffff));
	PCI_OUTW( cfg_addr, tmp );	
	return;
} /* PCI_disable_adapter() */


static void
release_resources( mtr_private_t *mtr_p )
{
	mtr_adapter_t	*adapter_p;	

	MTR_DBG_PRINT2( mtr_p, ("mtr%d: release_resources(),\n", 
		mtr_p->mtr_unit));
	if( mtr_p == NULL || mtr_p->tag != MTR_TAG ){
		MTR_ASSERT(0);		
		goto done;
	}

	/*	No one should own the mutex lock!	*/
	MTR_ASSERT( !mutex_owned( &mtr_p->init_mutex) );
	MTR_ASSERT( !mutex_owned( &mtr_p->srb_mutex) );
	mutex_destroy( &mtr_p->init_mutex );
	mutex_destroy( &mtr_p->srb_mutex);

        /*      Unregister the interrupt */
        if( (IS_MTR_FLAG_SET(mtr_p, MTR_FLAG_INTR_DONE)) ) {

		pciio_intr_disconnect( mtr_p->mtr_intr_hdl );
		RESET_MTR_FLAG( mtr_p, MTR_FLAG_INTR_DONE);
        } /* if */
        if( (IS_MTR_FLAG_SET(mtr_p, MTR_FLAG_INTR_ALLOC)) ){
		pciio_intr_free( mtr_p->mtr_intr_hdl );
		mtr_p->mtr_intr_hdl = NULL;
		RESET_MTR_FLAG( mtr_p, MTR_FLAG_INTR_ALLOC);
        } /* if */

	if( mtr_p->dmamap_p != NULL ){
  		pciio_dmamap_done(mtr_p->dmamap_p);
		pciio_dmamap_free(mtr_p->dmamap_p);
		mtr_p->dmamap_p = NULL;
	}

	if( mtr_p->test_dmamap_p != NULL ){
  		pciio_dmamap_done(mtr_p->test_dmamap_p);
		pciio_dmamap_free(mtr_p->test_dmamap_p);
		mtr_p->test_dmamap_p = NULL;
	}

	adapter_p = &(mtr_p->adapter);
	MTR_DBG_PRINT2( mtr_p, ("mtr%d: release_resources(0x%x.%x).\n", 
		mtr_p->mtr_unit,
		adapter_p->rx_tx_bufs_addr, 
		adapter_p->all_rx_tx_bufs_size));
	if( adapter_p->rx_tx_bufs_addr != NULL ){
		kvpfree( adapter_p->rx_tx_bufs_addr, 
			(adapter_p->all_rx_tx_bufs_size / NBPP));
		adapter_p->rx_tx_bufs_addr = NULL;
		adapter_p->all_rx_tx_bufs_size = 0;
	}
	MTR_ASSERT( adapter_p->all_rx_tx_bufs_size == 0 );

	MTR_DBG_PRINT2( mtr_p, ("mtr%d: release_resources(0x%x.%x).\n", 
		mtr_p->mtr_unit,
		adapter_p->init_block_p, sizeof(*adapter_p->init_block_p)));
	if( adapter_p->init_block_p ){
		kmem_free(adapter_p->init_block_p, 
			sizeof(*adapter_p->init_block_p));
		adapter_p->init_block_p = NULL;
	}

	if( adapter_p->scb_buff != NULL ){
		kmem_free( adapter_p->scb_buff, adapter_p->test_size);
		adapter_p->scb_buff 	= NULL;
		adapter_p->ssb_buff 	= NULL;
		adapter_p->scb_size 	= 0;
		adapter_p->ssb_size 	= 0;
		adapter_p->test_size 	= 0;
	}

	if (mtr_p->cfg_piomap_p != NULL) {
		pciio_piomap_free(mtr_p->cfg_piomap_p);
		mtr_p->cfg_piomap_p = NULL;
	}

	if( mtr_p->io_piomap_p != NULL ){
		pciio_piomap_free( mtr_p->io_piomap_p );
		mtr_p->io_piomap_p = NULL;
		RESET_MTR_FLAG( mtr_p, MTR_FLAG_PIO_DONE);
	}
	
	if( mtr_p->mtr_dev_desc != NULL ){
		device_desc_free( mtr_p->mtr_dev_desc );
		mtr_p->mtr_dev_desc = NULL;
	}

#ifdef	DEBUG
	if( mtr_p->rx_dump_mbuf ){
		m_freem(mtr_p->rx_dump_mbuf);
		mtr_p->rx_dump_mbuf = NULL;
	}
#endif	/* DEBUG */

	(mtr_p->ifmtr_p)->flags = MTR_FLAG_UNINIT;

	/*	Release itself!		*/
	kmem_free(mtr_p, sizeof(*mtr_p));

done:
	return;
} /* release_resources() */

/*
 *	Dump adapter's PCI configuration space header.
 *
 */
static void
PCI_get_dump_cfg( mtr_private_t *mtr_p, int dump_flag)
{
	int			idx;
	int                     tmp;
	volatile uchar_t	*cfg_addr;

	MTR_ASSERT( (mtr_p != NULL && mtr_p->mtr_cfg_addr != NULL) );
	if( !mtr_p || !mtr_p->mtr_cfg_addr ) goto done;

	cfg_addr = (volatile uchar_t *)(mtr_p->mtr_cfg_addr + PCI_CFG_COMMAND);
	tmp      = PCI_INW( cfg_addr );
	MTR_DBG_PRINT( mtr_p,("mtr%d: PCI_CFG_COMMAND: 0x%x.\n",
			      mtr_p->mtr_unit, tmp));
	mtr_p->pci_cfg.pci_command      = tmp & 0xffff;
	mtr_p->pci_cfg.pci_status       = (tmp >> 16) & 0xffff;
 
	cfg_addr = (volatile uchar_t *) (mtr_p->mtr_cfg_addr + PCI_INTR_LINE);
	tmp      = PCI_INW( cfg_addr ); 
	MTR_DBG_PRINT( mtr_p,("mtr%d: PCI_INTR_LINE: 0x%x.\n",
			      mtr_p->mtr_unit, tmp));
	mtr_p->pci_cfg.interrupt_pin    = (tmp >> 8) & 0xff;
	mtr_p->pci_cfg.interrupt_line   = (tmp) & 0xff;
	mtr_p->pci_cfg.max_lat          = (tmp >> 24) & 0xff;
	mtr_p->pci_cfg.min_gnt          = (tmp >> 16) & 0xff;
	
	cfg_addr = (volatile uchar_t *) (mtr_p->mtr_cfg_addr + PCI_CFG_CACHE_LINE);
	tmp      = PCI_INW( cfg_addr );
	MTR_DBG_PRINT( mtr_p,("mtr%d: PCI_CFG_CACHE_LINE: 0x%x.\n",
			      mtr_p->mtr_unit, tmp));
	mtr_p->pci_cfg.cache_line_size  = (tmp) & 0xff;
	mtr_p->pci_cfg.latency_timer    = (tmp >> 8) & 0xff;


	for( idx = 0; idx < MTR_PCI_BASE_REGISTERS; idx++ ){

		volatile caddr_t        base_addr;
		
		base_addr = (caddr_t) (mtr_p->mtr_cfg_addr + 
				       PCI_CFG_BASE_ADDR_0+(sizeof(uint_t) * idx));
		MTR_DBG_PRINT( mtr_p, ("mtr%d: %d, base_addr: 0x%x,\n", 
				       mtr_p->mtr_unit, idx, base_addr) );
		mtr_p->pci_cfg_base_addr[idx] = PCI_INW( base_addr );
		PCI_OUTW(  base_addr, (uint_t)-1);
		mtr_p->org_pci_cfg_base_addr[idx] = PCI_INW( base_addr );
		PCI_OUTW( base_addr, 
			 (uint_t)mtr_p->pci_cfg_base_addr[idx]);
	} 

	if( !dump_flag )goto done;
	MTR_DBG_PRINT( mtr_p, ("mtr%d: cfg_addr: 0x%x,\n", 
		mtr_p->mtr_unit, mtr_p->mtr_cfg_addr) );
	MTR_DBG_PRINT( mtr_p, ("mtr%d: vendor_id: 0x%x, device_id: 0x%x, "
		"pci_command: 0x%x, pci_status: 0x%x\n",
		mtr_p->mtr_unit,
		mtr_p->mtr_vendor_id,
		mtr_p->mtr_dev_id,
		mtr_p->pci_cfg.pci_command,
		mtr_p->pci_cfg.pci_status));
	for( idx = 0; idx < MTR_PCI_BASE_REGISTERS; idx++ ){
		MTR_DBG_PRINT( mtr_p, 
			("mtr%d: pci_cfg_base_addr[%d]: 0x%x -> 0x%x,\n", 
			mtr_p->mtr_unit, idx,
			mtr_p->org_pci_cfg_base_addr[idx],
			mtr_p->pci_cfg_base_addr[idx]) );
	} 

	MTR_DBG_PRINT( mtr_p, 
		("mtr%d: interrupt_pin: 0x%x, interrupt_line: 0x%x, "
		"max_lat: 0x%x, min_gnt: 0x%x, latency_timer: %d, "
		"cache_line_size: %d.\n",
		mtr_p->mtr_unit,
		mtr_p->pci_cfg.interrupt_pin,
		mtr_p->pci_cfg.interrupt_line,
		mtr_p->pci_cfg.max_lat,
		mtr_p->pci_cfg.min_gnt,
		mtr_p->pci_cfg.latency_timer,
		mtr_p->pci_cfg.cache_line_size));

done:
	return;
} /* PCI_get_dump_cfg() */


static void
report_Configuration( mtr_private_t *mtr_p )
{
	uchar_t			*addr, *addr2;
	mtr_adapter_t		*adapter_p;
	INITIALIZATION_BLOCK	*init_block_p;

	MTR_ASSERT( mtr_p != NULL && mtr_p->tag == MTR_TAG );

        adapter_p       = &(mtr_p->adapter);
        init_block_p    = mtr_p->adapter.init_block_p;
        addr            = (uchar_t *)mtr_p->mtr_bia;
        addr2           = mtr_p->mtr_enaddr;

	cmn_err(CE_CONT, "mtr%d: driver version $Revision: 1.7 $.\n", 
		mtr_p->mtr_unit);
        cmn_err(CE_CONT, "mtr%d: PCI device_id: %d, vertex 0x%x.\n",
                mtr_p->mtr_unit, mtr_p->mtr_dev_id, 
		mtr_p->ifmtr_p->connect_vertex);
        cmn_err(CE_CONT, "mtr%d: ftk_version_for_fmplus: %d, "
                "fmplus_version:\"%s\".\n",
                mtr_p->mtr_unit, ftk_version_for_fmplus, fmplus_version);
        cmn_err(CE_CONT, "mtr%d: tx_slots: %d, rx_slots: %d, "
                "ring_speed: %dMB, ram_size: %dKB.\n",
                mtr_p->mtr_unit,
                init_block_p->fastmac_parms.tx_slots,
                init_block_p->fastmac_parms.rx_slots,
                mtr_p->mtr_nselout_bits ? 4 : 16,
                adapter_p->ram_size);
        cmn_err(CE_CONT, "mtr%d: burnin address: 0x%x:%x:%x:%x:%x:%x, "
                "open address: 0x%x:%x:%x:%x:%x:%x.\n",
                mtr_p->mtr_unit,
                addr[0], addr[1], addr[2],
                addr[3], addr[4], addr[5],
                addr2[0], addr2[1], addr2[2],
                addr2[3], addr2[4], addr2[5]);
        cmn_err(CE_CONT, "mtr%d: broadcast address: 0x%x:%x:%x:%x:%x:%x, "
                "default sri: 0x%x.\n",
                mtr_p->mtr_unit,
                mtr_p->mtr_baddr[0], mtr_p->mtr_baddr[1],
                mtr_p->mtr_baddr[2], mtr_p->mtr_baddr[3],
                mtr_p->mtr_baddr[4], mtr_p->mtr_baddr[5],
                adapter_p->nullsri);

	return;
} /* report_Configuration() */

static int
mtr_issue_srb(mtr_private_t *mtr_p, SRB_GENERAL *srb_ptr)
{
	int		error = NO_ERROR, res;
	int		idx;
	ushort_t	*ptr;
	mtr_adapter_t	*adapter_p;

        /*
         *      Only one outstanding SRB cmd is allowed by adapter.
         */
        if( !mutex_trylock( &mtr_p->srb_mutex) ){
                error = EBUSY;
                goto done;
        }
        if( mtr_p->srb_used != 0 ){
                cmn_err(CE_ALERT, "mtr%d: SIOC...MULTI: srb_used: 0x%x.\n",
                        mtr_p->mtr_unit, mtr_p->srb_used);
                mutex_unlock( &mtr_p->srb_mutex );
                /*      TO DO: is hwi_halt_eagle() enough?      */
                hwi_halt_eagle( mtr_p );
                goto done;
        }
        mtr_p->srb_used++;

	adapter_p       = &(mtr_p->adapter);
	bcopy( srb_ptr, &adapter_p->srb_general, 
		sizeof(adapter_p->srb_general));

	MTR_ASSERT( (sizeof(SRB_GENERAL) & 1) == 0 );
	MTR_ASSERT( ((__psunsigned_t) &adapter_p->srb_general & 1) == 0);
	hwi_pci_set_dio_address( mtr_p, 
		((__psunsigned_t)adapter_p->srb_dio_addr & 0xffff), 
		DIO_LOCATION_EAGLE_DATA_PAGE1);
	for(ptr = (ushort_t *)&adapter_p->srb_general, idx = 0; 
		idx <= sizeof(SRB_GENERAL); idx += sizeof(ushort_t), ptr++){
		OUTS( mtr_p->adapter.sif_datinc, *ptr);
	};
	OUTS(adapter_p->sif_int, EAGLE_SRB_CMD_CODE);

	/*	Interrupt needs to enabled and ifnet needs to be unlocked
	 *	to let SRB_FREE comes in.	
	 */
	IFNET_UNLOCK( mtr_2_ifnet(mtr_p) );	
	res = sleep( &mtr_p->srb_used, PZERO + 1 /* allowing signal */);
	IFNET_LOCK( mtr_2_ifnet(mtr_p) );

	if( res ){
		MTR_DBG_PRINT(mtr_p,
			("mtr%d: mtr_issue_srb() INTR: 0x%x.%x.%x.\n",
			mtr_p->mtr_unit, res,
			adapter_p->srb_general.header.function,
			adapter_p->srb_general.header.return_code));
		error = EINTR;
	} else {
		MTR_DBG_PRINT(mtr_p,
			("mtr%d: SRB(0x%x) result: 0x%x.\n",
			mtr_p->mtr_unit, 
			adapter_p->srb_general.header.function,
			adapter_p->srb_general.header.return_code));
		if( adapter_p->srb_general.header.return_code ){
			cmn_err(CE_NOTE, "mtr%d: SRB(0x%x) result: 0x%x.",
				mtr_p->mtr_unit,
				adapter_p->srb_general.header.function,
				adapter_p->srb_general.header.return_code);
			error = EIO;
		}
	}	

        mtr_p->srb_used--;
        mutex_unlock( &mtr_p->srb_mutex );

done:
        MTR_ASSERT( !mutex_owned( &(mtr_p->srb_mutex) ) );
        MTR_ASSERT( mtr_p->srb_used == 0 );
	return(error);
} /* mtr_issue_srb() */

static int 
mtr_read_error_log(mtr_private_t *mtr_p)
{
        int             error = NO_ERROR;
        SRB_GENERAL     srb_general;

        bzero(&srb_general, sizeof(srb_general));

        srb_general.header.function     = READ_ERROR_LOG_SRB;

        error = mtr_issue_srb( mtr_p, &srb_general );

        return(error);
} /* mtr_read_error_log() */

/*
 *      To add a group address, MAC multicast address.
 */
static int
mtr_group_address( mtr_private_t *mtr_p, int cmd, caddr_t addr_p)
{
        int             error = NO_ERROR;
        SRB_GENERAL     srb_general;
        mtr_adapter_t   *adapter_p;

        if( addr_p == 0 ){
                error = EIO;
                goto done;
        }

        adapter_p       = &(mtr_p->adapter);
        bcopy( addr_p, &(adapter_p->new_group_addr),
                sizeof(adapter_p->new_group_addr));
        switch( cmd ){
        case SIOCADDMULTI:
                if( adapter_p->group_address == adapter_p->new_group_addr ){
                        goto done;
                }
                if( adapter_p->group_address != 0 ){
                        cmn_err(CE_NOTE,
                                "mtr%d group address 0x%x replaces 0x%x!",
                                mtr_p->mtr_unit,
                                adapter_p->new_group_addr,
                                adapter_p->group_address);
                }
                break;
        case SIOCDELMULTI:
                if( adapter_p->group_address == 0 ||
                    adapter_p->group_address != adapter_p->new_group_addr){
                        error = EINVAL;
                        goto done;
                }
                adapter_p->new_group_addr = 0;
                break;
        default:
                error = EIO;
                MTR_ASSERT(0);
                goto done;
        }
        adapter_p->group_address = adapter_p->new_group_addr;

        bzero(&srb_general, sizeof(srb_general));

        srb_general.header.function     = SET_GROUP_ADDRESS_SRB;
        bcopy( &(adapter_p->new_group_addr),
                &srb_general.set_group.multi_address.all,
                sizeof(srb_general.set_func.multi_address.all));

        error = mtr_issue_srb( mtr_p, &srb_general );

        if( error == NO_ERROR ){
                adapter_p->group_address = adapter_p->new_group_addr;
        }

done:
        return(error);
} /* mtr_group_address() */

static int
mtr_modify_open_parms(mtr_private_t *mtr_p)
{
        int             error = NO_ERROR;
        SRB_GENERAL     srb_general;

        bzero(&srb_general, sizeof(srb_general));
        srb_general.header.function = MODIFY_OPEN_PARMS_SRB;
        srb_general.mod_parms.open_options      =
                IS_MTR_FLAG_SET(mtr_p, MTR_FLAG_STATE_PROMISC) ?
                (OPEN_OPT_COPY_ALL_MACS | OPEN_OPT_COPY_ALL_LLCS) : 0;
        srb_general.mod_parms.open_options      |=
                mtr_participant_claim_token ? OPEN_OPT_CONTENDER : 0;
        if( (error = mtr_issue_srb( mtr_p, &srb_general )) != NO_ERROR ){
                cmn_err(CE_DEBUG, "mtr%d: mtr_modify_open_parms(): "
                        "mtr_issue_srb() failed!\n",
                        mtr_p->mtr_unit);
        }

        return( error );
} /* mtr_modify_open_parms() */

static int
mtr_reopen_adapter(mtr_private_t *mtr_p, SRB_GENERAL *new_srb_general)
{
        int             error = NO_ERROR;
        SRB_GENERAL     srb_general;

        /*      close it first. */
        bzero(&srb_general, sizeof(srb_general));
        srb_general.header.function = CLOSE_ADAPTER_SRB;
        error = mtr_issue_srb( mtr_p, &srb_general );

        /*      open it. */
        error = mtr_issue_srb( mtr_p, new_srb_general );

        if( error != NO_ERROR ){
                cmn_err(CE_DEBUG, "mtr%d: mtr_reopen_adapter(): "
                        "mtr_issue_srb() failed!\n",
                        mtr_p->mtr_unit);
        }

        return( error );
}

static int
mtr_functional_address( mtr_private_t *mtr_p, int cmd, uint_t functional_address )
{
	int		error = NO_ERROR;
	SRB_GENERAL	srb_general;
	mtr_adapter_t	*adapter_p;
	uint_t          new_functional_address;

	adapter_p       = &(mtr_p->adapter);
	bzero(&srb_general, sizeof(srb_general));

	switch( cmd ){
	case SIOCADDMULTI:
	case SIOC_MTR_ADDFUNC:
		if( adapter_p->functional_address & functional_address )
			goto done;
		new_functional_address = 
			adapter_p->functional_address | functional_address;
		break;

	case SIOCDELMULTI:
	case SIOC_MTR_DELFUNC:
		if( !(adapter_p->functional_address & functional_address) )
			goto done;
		new_functional_address = 
			adapter_p->functional_address & ~(functional_address);
		break;
	
	default:
		error = EIO;
		goto done;
	}
	srb_general.header.function 		= SET_FUNCTIONAL_ADDRESS_SRB;
	srb_general.set_func.multi_address.all 	= new_functional_address;

	error = mtr_issue_srb( mtr_p, &srb_general );

	if( error == NO_ERROR ){
		adapter_p->functional_address = new_functional_address; 
	}

done:
	return( error );
} /* mtr_functional_address() */

/*static */void
hwi_halt_eagle(mtr_private_t *mtr_p)
{
	ushort_t		acl, acl_out;
	
	MTR_DBG_PRINT2( mtr_p, ("mtr%d: hwi_halt_eagle() called.\n", 
		mtr_p->mtr_unit));

	MTR_ASSERT( mtr_p->adapter.sif_acl != NULL );

	/*
	 * Set output for SIF register EAGLE_ACONTROL.
	 * Maintain parity; set halt, reset, enable SIF interrupts.
	 */
	acl	=  INS(mtr_p->adapter.sif_acl);
	acl_out	=  acl & EAGLE_SIFACL_PARITY;
	acl_out	|= (EAGLE_SIFACL_ARESET | 
		    EAGLE_SIFACL_CPHALT |
		    EAGLE_SIFACL_BOOT   | 
		    EAGLE_SIFACL_SINTEN | 
		    mtr_p->mtr_nselout_bits);

	/*	
	 * Output to SIF register EAGLE_ACONTROL to halt EAGLE. 
	 */
	us_delay(20);		/* at least 14 so ... */
	OUTS(mtr_p->adapter.sif_acl, acl_out);
	MTR_DBG_PRINT2( mtr_p, 
		("mtr%d: hwi_halt_eagle(): 0x%x -> 0x%x(0x%x).\n",
		mtr_p->mtr_unit, acl, acl_out, INS(mtr_p->adapter.sif_acl)));
	acl = acl_out;

	/*	
	 * Bring EAGLE out of reset state, maintain halt status. 
	 */
	us_delay(20);		/* at least 14 so ... */
	acl_out	&= ~(EAGLE_SIFACL_ARESET);
	OUTS(mtr_p->adapter.sif_acl, acl_out);	
	MTR_DBG_PRINT2( mtr_p, 
		("mtr%d: hwi_halt_eagle(): 0x%x -> 0x%x(0x%x).\n",
		mtr_p->mtr_unit, acl, acl_out, INS(mtr_p->adapter.sif_acl)));

	SET_MTR_FLAG( mtr_p, MTR_FLAG_STATE_HALTED);
	RESET_MTR_FLAG( mtr_p, 
		(MTR_FLAG_STATE_CODE_DOWNLOADED |
		 MTR_FLAG_STATE_STARTED | MTR_FLAG_STATE_INITED));
	MTR_RESET_IFF( mtr_p, IFF_ALIVE );

	return;
} /* hwi_halt_eagle() */

static void
hwi_start_eagle(mtr_private_t *mtr_p)
{
	ushort_t	tmp;
#ifdef DEBUG2
	ushort_t        tmp2;
#endif
	MTR_ASSERT( mtr_p );
	MTR_ASSERT( IS_MTR_FLAG_SET(mtr_p, MTR_FLAG_STATE_CODE_DOWNLOADED) );
	MTR_ASSERT( !IS_MTR_FLAG_SET(mtr_p, MTR_FLAG_STATE_STARTED) );
	MTR_DBG_PRINT2( mtr_p, ("mtr%d: hwi_start_eagle() called.\n", 
		mtr_p->mtr_unit));
	tmp  	=  INS(mtr_p->adapter.sif_acl);
#ifdef DEBUG2
	tmp2	=  tmp;
#endif

	tmp  	&= ~(EAGLE_SIFACL_CPHALT);
	tmp	&= 0xff;
	OUTS(mtr_p->adapter.sif_acl, tmp);
	MTR_DBG_PRINT2( mtr_p,
		("mtr%d: hwi_start_eagle(): 0x%x -> 0x%x(0x%x).\n",
		mtr_p->mtr_unit, tmp2, tmp, INS(mtr_p->adapter.sif_acl)));

	SET_MTR_FLAG( mtr_p, MTR_FLAG_STATE_STARTED);
	return;
} /* hwi_start_eagle() */

/*
 *	From hwi_pci2.c:hwi_pci2_set_dio_address() and
 *	hwi_pcit.c: hwi_pcit_set_dio_address().
 */
static void
hwi_pci_set_dio_address(mtr_private_t *mtr_p, 
		ushort_t dio_addr_l, ushort_t dio_addr_h)	
{
	MTR_DBG_PRINT_PIO( mtr_p, (
		"mtr%d: hwi_pci_set_dio_address(0x%x, 0x%x.%x) called.\n", 
		mtr_p->mtr_unit, mtr_p, dio_addr_l, dio_addr_h));

	OUTS(mtr_p->adapter.sif_adrx, dio_addr_h & 0xffff);
	OUTS(mtr_p->adapter.sif_adr,  dio_addr_l & 0xffff);
	return;
} /* hwi_pci_set_dio_address() */

static int
hwi_get_init_code(mtr_private_t *mtr_p)
{
	int	error = EIO;
	ushort_t	init_code;
	int init_cnt;

	MTR_DBG_PRINT2( mtr_p, ("mtr%d: hwi_get_init_code() called.\n", 
		mtr_p->mtr_unit));

	/*	We release the lock because we need intr to finish 
	 *	the initialization. In that case, we uses delay()
	 *	which put us into sleep!!! Because of that, we
	 *	certainly can not hold the lock!
	 *
	 *	Even we release the lock, because of IFF_ALIVE is
	 *	not set, the adapter is in the state no TX and RX
	 *	could be conducted and because of MAX_WAIT_MTR_INIT_CODE_CNT
	 *	no another initialization could be issued during
	 *	our waiting window.
	 *
	 *	TO DO: is it only for BROKEN_DMA? 
	 */
	MTR_ASSERT( !IS_MTR_IFF_SET( mtr_p, IFF_ALIVE) );
	MTR_ASSERT( mutex_owned(&mtr_p->init_mutex) );
	MTR_ASSERT( mutex_mine(&mtr_p->init_mutex) );

	IFNET_UNLOCK( mtr_2_ifnet(mtr_p) );
	for(init_cnt=0; init_cnt < MAX_WAIT_MTR_INIT_CODE_CNT; 
	    init_cnt++){

		/* let interrupt comes in!	*/
		delay(HZ);		

		init_code = INS(mtr_p->adapter.sif_int);
		MTR_DBG_PRINT2( mtr_p, 
			("mtr%d: hwi_get_init_code(): %d 0x%x.\n",
			mtr_p->mtr_unit, init_cnt, init_code));

		if( (init_code & EAGLE_INIT_SUCCESS_MASK) == 0){
			error = NO_ERROR;
			goto done;
		}
		if( (init_code & EAGLE_INIT_FAILURE_MASK) != 0)
			goto done;

	} /* for */

done:
	/*	Get the lock and continue.
	 */
	IFNET_LOCK( mtr_2_ifnet(mtr_p) );

	if( error != NO_ERROR ){
		cmn_err(CE_WARN, 
			"mtr%d: hwi_get_init_code failed: 0x%x(0x%x).",
			mtr_p->mtr_unit, init_code, 
			EAGLE_INIT_FAILURE_MASK);
	}
	return(error);
} /* hwi_get_init_code() */

static void
hwi_copy_to_dio_space(
	mtr_private_t *mtr_p, 
	ushort_t dio_addr_l, 
	ushort_t dio_addr_h, 
	uchar_t  *data, 
	ushort_t len)
{
	int	idx;

	/* 	We made the assumption that size is alway even number.	*/
	MTR_ASSERT( !(len & 1) );		
	MTR_DBG_PRINT2( mtr_p, ("mtr%d: hwi_copy_to_dio_space(, "
		"0x%x.%x, 0x%x, %d) called.\n", mtr_p->mtr_unit, 
		dio_addr_l, dio_addr_h, data, len) );
	
	hwi_pci_set_dio_address(mtr_p, dio_addr_l, dio_addr_h);

	for(idx=0; idx<len-1; idx+=sizeof(ushort_t)){
		OUTS(mtr_p->adapter.sif_datinc, 
			*(ushort_t *)(data + idx));
	} /* for */

#ifdef	DEBUG
	hwi_pci_set_dio_address(mtr_p, dio_addr_l, dio_addr_h);
	for(idx=0; idx<len-1; idx+=sizeof(ushort_t)){
		ushort_t	tmp, tmp2;

		tmp  = INS(mtr_p->adapter.sif_datinc);
		tmp2 = *(ushort_t *)(data + idx);
		if( tmp != tmp2 ){
			cmn_err(CE_WARN, "mtr%d: hwi_copy_to_dio_space(): "
				"%d.0x%x: 0x%x != 0x%x!",
				mtr_p->mtr_unit, 
				idx,  (data+idx),
				tmp, tmp2);
		}
	}
#endif	/* DEBUG */

	return;
} /* hwi_copy_to_dio_space() */

/*
 *	From ftk hwi/hiw_gen.c.
 */
static uchar_t	scb_test_pattern[] = SCB_TEST_PATTERN_DATA;
static uchar_t	ssb_test_pattern[] = SSB_TEST_PATTERN_DATA;
static int
hwi_initialize_adapter(mtr_private_t *mtr_p)
{
	int			idx;
	int			error = NO_ERROR;

	mtr_adapter_t		*adapter_p;	
	INITIALIZATION_BLOCK 	*init_block_p;

	MTR_DBG_PRINT2( mtr_p, (
		"mtr%d: hwi_initialize_adapter() called.\n", mtr_p->mtr_unit));
	adapter_p 	= &(mtr_p->adapter);	
	init_block_p 	= mtr_p->adapter.init_block_p;

	/*	Initialize the memory for DMA testing.	*/
	MTR_ASSERT( adapter_p->scb_size >= SCB_TEST_PATTERN_LENGTH);
	MTR_ASSERT( adapter_p->ssb_size >= SSB_TEST_PATTERN_LENGTH);
	MTR_ASSERT( adapter_p->test_size != 0);
	MTR_ASSERT( adapter_p->scb_buff != NULL );
	MTR_ASSERT( adapter_p->scb_phys != NULL );
	MTR_ASSERT( adapter_p->ssb_buff != NULL );
	MTR_ASSERT( adapter_p->ssb_phys != NULL );
	MTR_ASSERT( !(adapter_p->test_size & (CACHELINE - 1)) );
	bzero(adapter_p->scb_buff, adapter_p->test_size);
	dki_dcache_wbinval((void *)adapter_p->scb_buff, adapter_p->test_size);

	MTR_HEX_DUMP(mtr_p, "init_block.ti_parms",
		init_block_p->ti_parms);
	MTR_HEX_DUMP(mtr_p, "init_block.smart_parms",
		init_block_p->smart_parms);
	MTR_HEX_DUMP(mtr_p, "init_block.fastmac_parms",
		init_block_p->fastmac_parms);

	/* 
	 * Download initialization block to required location in DIO space.
	 * Note routine leaves 0x0001 in EAGLE SIFADRX register.
     	 */
	hwi_copy_to_dio_space(mtr_p, 
		DIO_LOCATION_INIT_BLOCK & 0xffff, 
		((DIO_LOCATION_INIT_BLOCK >> 16) & 0xffff), 
		(uchar_t *)init_block_p, sizeof(INITIALIZATION_BLOCK));
	MTR_VERIFY_ADX( mtr_p );

	/*
	 * Start initialization by output 0x9080 to SIF interrupt register.
	 *	TO DO: 0x9880, 0x9080(EAGLE_INIT_START_CODE) or 0x9000?
	 */
	OUTS(mtr_p->adapter.sif_int, 0x9080);
	
	/*
	 * Wait for a valid initialization code, may wait 11 seconds.
	 */
	if( (error = hwi_get_init_code(mtr_p)) != NO_ERROR ) goto done;	
	MTR_VERIFY_ADX( mtr_p );

	/*
	 *	Check the result of DMA test.
     	 * 	First check SCB for correct test pattern. Remember used Fastmac
     	 * 	transmit buffer address for SCB address.
     	 */
	for (idx = 0; idx < SCB_TEST_PATTERN_LENGTH; idx++){
		if (adapter_p->scb_buff[idx] != scb_test_pattern[idx]) {
			cmn_err(CE_NOTE, 
				"mtr%d: scb_buff[%d]: 0x%x != "
				"scb_test_pattern[%d]: 0x%x.",  
				mtr_p->mtr_unit,
				idx, adapter_p->scb_buff[idx],
				idx, scb_test_pattern[idx]);
		}
	} /* for */
	MTR_DBG_PRINT2( mtr_p,
		("mtr%d: hwi_initialize_adapter() scb test done: "
		 "0x%x.%x.%x.%x.%x.%x.\n",
		 mtr_p->mtr_unit, 
		adapter_p->scb_buff[0], adapter_p->scb_buff[1],
		adapter_p->scb_buff[2], adapter_p->scb_buff[3],
		adapter_p->scb_buff[4], adapter_p->scb_buff[5]
		));

	/*
	 * 	Now check SSB for correct test pattern. Remember used Fastmac
	 * 	receive buffer address for SSB address.
	 */
	for (idx = 0; idx < SSB_TEST_PATTERN_LENGTH; idx++) {
		if( adapter_p->ssb_buff[idx] != ssb_test_pattern[idx]) {
			cmn_err(CE_NOTE, 
				"mtr%d: ssb_buff[%x]: 0x%x != "
				"ssb_test_pattern[%d]: 0x%x.",  
				mtr_p->mtr_unit,	
				idx, adapter_p->ssb_buff[idx], 
				idx, ssb_test_pattern[idx]);
		}
	} /* for */
	MTR_DBG_PRINT2(mtr_p,
		("mtr%d: hwi_initialize_adapter() ssb test done: "
		 "0x%x.%x.%x.%x.%x.%x.%x.%x.\n",
		mtr_p->mtr_unit, 
		adapter_p->ssb_buff[0], adapter_p->ssb_buff[1],
		adapter_p->ssb_buff[2], adapter_p->ssb_buff[3],
		adapter_p->ssb_buff[4], adapter_p->ssb_buff[5],
		adapter_p->ssb_buff[6], (adapter_p->ssb_buff[7] & 0xff)
		));
	
done:

	MTR_DBG_PRINT2(mtr_p,
		("mtr%d: hwi_initialize_adapter() done with %d.\n", 
		mtr_p->mtr_unit, error));
	MTR_VERIFY_ADX( mtr_p );
	return( error );
} /* hwi_initialize_adapter() */

static int
hwi_download_code(mtr_private_t *mtr_p, DOWNLOAD_RECORD *rec) 
{
	int		error = NO_ERROR;
	int		idx;
	int		rec_cnt = 0;
	ushort_t	*sif_datinc;
	uchar_t		checksum;

	MTR_ASSERT( mtr_p != NULL );
	MTR_ASSERT( SWAP_16(rec->type) == DOWNLOAD_RECORD_TYPE_MODULE );
        MTR_ASSERT( SWAP_16(rec->body.module.download_features) & 
			DOWNLOAD_FASTMAC_INTERFACE );
	MTR_ASSERT( IS_MTR_FLAG_SET(mtr_p, MTR_FLAG_STATE_HALTED) );
	MTR_ASSERT( !IS_MTR_FLAG_SET(mtr_p, MTR_FLAG_STATE_CODE_DOWNLOADED) );
	MTR_DBG_PRINT2( mtr_p, (
		"mtr%d: hwi_download_code(, 0x%x) called.\n", 
		mtr_p->mtr_unit, rec));
	RESET_MTR_FLAG( mtr_p, MTR_FLAG_STATE_HALTED);
	sif_datinc = mtr_p->adapter.sif_datinc;

	/*	Verify firmware's length.	*/
        if( mtr_p->adapter.recorded_size_fmplus_array !=
            mtr_p->adapter.sizeof_fmplus_array ){
                cmn_err(CE_WARN, "mtr%d: hwi_download_code(): %d != %d!",
                        mtr_p->mtr_unit,
                        mtr_p->adapter.recorded_size_fmplus_array,
                        mtr_p->adapter.sizeof_fmplus_array);
                error = EIO;
                goto done;
        }

	/*	Verify firmware's checksum.	*/
        for(checksum = 0, idx = 0;
            idx < mtr_p->adapter.sizeof_fmplus_array; idx++ ){
                checksum += mtr_p->adapter.fmplus_image[idx] ;
        } /* for */
        if( (uchar_t)(checksum + mtr_p->adapter.fmplus_checksum) != (uchar_t)0){                cmn_err(CE_WARN,
                        "mtr%d: hwi_download_code(): 0x%x + 0x%x != 0!",
                        mtr_p->mtr_unit, checksum,
                        mtr_p->adapter.fmplus_checksum);
                error = EIO;
                goto done;
        }

	rec = (DOWNLOAD_RECORD *) ((uchar_t * )rec + SWAP_16(rec->length));
	while( SWAP_16(rec->length) > 0 ){
		
		switch( SWAP_16(rec->type) ){
		case DOWNLOAD_RECORD_TYPE_DATA_32:

			MTR_DBG_PRINT_PIO( mtr_p, ("mtr%d: hwi_download_code() "
				"DATA_32: 0x%x.%x, 0x%x.\n", mtr_p->mtr_unit,
				SWAP_16(rec->body.data_32.dio_addr_l), 
				SWAP_16(rec->body.data_32.dio_addr_h),
				SWAP_16(rec->body.data_32.word_count)));

			hwi_pci_set_dio_address(mtr_p, 
				SWAP_16(rec->body.data_32.dio_addr_l),
				SWAP_16(rec->body.data_32.dio_addr_h));
			for(idx=0; idx < SWAP_16(rec->body.data_32.word_count);
				idx++){
				OUTS(sif_datinc, 
					SWAP_16(rec->body.data_32.data[idx]));
			} /* for */

			hwi_pci_set_dio_address(mtr_p, 
				SWAP_16(rec->body.data_32.dio_addr_l),
				SWAP_16(rec->body.data_32.dio_addr_h));

			/*	Read back to verify.	*/
			for(idx=0; idx < SWAP_16(rec->body.data_32.word_count);
				idx++){
				ushort_t	tmp;
				
				tmp = INS(sif_datinc);
				if( tmp == SWAP_16(rec->body.data_32.data[idx]))
					continue;

				cmn_err(CE_WARN, 
					"mtr%d: DATA_32 no match %d.%d: "
					"0x%x != 0x%x!",
					mtr_p->mtr_unit,
					rec_cnt, idx,
					tmp,
					(ushort_t)SWAP_16(rec->body.data_32.data[idx]));
				error = EIO;
				break;
			} /* for */
			break;

		case DOWNLOAD_RECORD_TYPE_FILL_32:

			MTR_DBG_PRINT_PIO( mtr_p, ("mtr%d: hwi_download_code() "
				"FILL_32: 0x%x.%x, 0x%x.\n",
				mtr_p->mtr_unit,
				SWAP_16(rec->body.fill_32.dio_addr_l),
				SWAP_16(rec->body.fill_32.dio_addr_h),
				SWAP_16(rec->body.fill_32.word_count)));

			hwi_pci_set_dio_address(mtr_p,
				SWAP_16(rec->body.fill_32.dio_addr_l),
				SWAP_16(rec->body.fill_32.dio_addr_h));
			for(idx = 0; 
				idx < SWAP_16(rec->body.fill_32.word_count); 
				idx++){                                
				OUTS(sif_datinc, 
					SWAP_16(rec->body.fill_32.pattern));
			} /* for */

			hwi_pci_set_dio_address(mtr_p, 
				SWAP_16(rec->body.fill_32.dio_addr_l),
				SWAP_16(rec->body.fill_32.dio_addr_h));

			/* Read back to verify */
			for(idx=0; idx < SWAP_16(rec->body.fill_32.word_count);
			    idx++) {
				ushort_t tmp;
				
				tmp = INS(sif_datinc);
				if(tmp == SWAP_16(rec->body.fill_32.pattern))
				  continue;

				cmn_err(CE_WARN, 
					"mtr%d: FILL_32 no match %d.%d: "
					"0x%x != 0x%x!",
					mtr_p->mtr_unit,
					rec_cnt, idx,
					tmp,
					(ushort_t)SWAP_16(rec->body.fill_32.pattern));
				error = EIO;
				break;
			} /* for */
					
			break;

		default:
			/* TO DO: log */
			error = EIO;
			goto done;
		} /* switch */
		rec = (DOWNLOAD_RECORD *)((uchar_t *)rec + 
			SWAP_16(rec->length));
		rec_cnt++;
	} /* while() */

done:
	if( error == NO_ERROR ){
		SET_MTR_FLAG(mtr_p, MTR_FLAG_STATE_CODE_DOWNLOADED);
	}
	MTR_DBG_PRINT2( mtr_p, ("mtr%d: hwi_download_code() done with %d.\n",
		mtr_p->mtr_unit, error));
	return( error );
} /* hwi_download_code() */

/*
 *	Check SIF SIFSTS for INITIALIZE bit.
 *	See TI: 4-35 and 4.4.2 for the detail.
 *
 */
#define	MAX_WAIT_BRING_UP_DIAG_CNT	3
static int
hwi_bring_up_diag(mtr_private_t *mtr_p)
{
	int		error = EIO;
	int		cnt;
	ushort_t	sifsts, sifsts2;
	int		mtr_timeout_cnt = 0;

	MTR_DBG_PRINT2( mtr_p, 
		("mtr%d: hwi_bring_up_diag(0x%x) called: %d.\n", 
		mtr_p->mtr_unit, mtr_p, mtr_timeout_cnt));
	MTR_ASSERT( mtr_timeout_cnt == 0);

	for(; mtr_timeout_cnt++ < MAX_WAIT_BRING_UP_DIAG_CNT; ){
		cnt = 0;

again:
		/*	
		 *	We will give up the processor but not the ifnet 
		 *	lock because we are using the adapter now!
		 */
		delay(HZ);

		sifsts =  sifsts2 =  INS(mtr_p->adapter.sif_int);
		sifsts &= EAGLE_BRING_UP_TOP_NIBBLE;
	
		if( sifsts == EAGLE_BRING_UP_SUCCESS ){
			error = NO_ERROR;
			break;
		}
			
		cnt++;
		if( sifsts != EAGLE_BRING_UP_FAILURE && cnt < 15)goto again;

		cmn_err(CE_NOTE, 
			"mtr%d: hwi_bring_up_diag() reset %d.%d: 0x%x!",
			mtr_p->mtr_unit, mtr_timeout_cnt, cnt, sifsts2);
		OUTS(mtr_p->adapter.sif_int, 0xff00 /* TO DO: or 0xff00 */);
	}/* for */

	MTR_DBG_PRINT2(mtr_p,
		("mtr%d: hwi_bring_up_diag() done with %d 0x%x.\n",
		mtr_p->mtr_unit, mtr_timeout_cnt, sifsts2));
	return(error);
} /* hwi_bring_up_diag() */

/*
 *	hwi_pci_install_card() is not from hwi_pci.c. 
 *	Instead it combines the functions from hwi_pci2.c and hwi_pcit.c.
 */
static int
hwi_pci_install_card(mtr_private_t *mtr_p, DOWNLOAD_RECORD *rec)
{
	int		error = NO_ERROR;
	mtr_adapter_t	*adapter_p = &(mtr_p->adapter);

	MTR_DBG_PRINT2( mtr_p, (
		"mtr%d: hwi_pci_install_card(0x%x, 0x%x) called.\n", 
		mtr_p->mtr_unit, rec, mtr_p));
	MTR_ASSERT( IS_MTR_FLAG_SET(mtr_p, MTR_FLAG_STATE_HALTED) );
	

	if( (error = hwi_download_code(mtr_p, rec)) != NO_ERROR) goto done;

	hwi_start_eagle( mtr_p );

	/*
	 *	After being successfully reset, the adapter
	 *	should set INITIALIZE bit in sif_int. 
	 *	Reference: TI: 4.4.2 Software Reset.
	 */
	if( (error = hwi_bring_up_diag(mtr_p)) != NO_ERROR )goto done;

	/* 	For PCI_PCIT_DEVICE_ID's BROKEN_DMA  */
	if( mtr_p->mtr_dev_id == PCI_PCIT_DEVICE_ID &&
	    adapter_p->broken_dma == PCIT_BROKEN_DMA ){
		adapter_p->broken_dma_handshake_addr =
			adapter_p->scb_buff + adapter_p->scb_size +
			adapter_p->ssb_size;
		if( IS_KSEG0( adapter_p->broken_dma_handshake_addr ) ){
			adapter_p->broken_dma_handshake_addr = (caddr_t)
			K0_TO_K1(adapter_p->broken_dma_handshake_addr);
		} else if( !IS_KSEG1(adapter_p->broken_dma_handshake_addr) ) {
			error = EIO;
			MTR_ASSERT(0);
			goto done;
		}

		/* 	Must be in K0 or K1 space */
		adapter_p->broken_dma_handshake_phys =
			adapter_p->scb_phys + adapter_p->scb_size +
			adapter_p->ssb_size;
	
		OUTS(adapter_p->sif_adr, DIO_LOCATION_DMA_POINTER);
		OUTS(adapter_p->sif_datinc, (__psunsigned_t)
			(adapter_p->broken_dma_handshake_phys & 0xffff));
		OUTS(adapter_p->sif_datinc, (__psunsigned_t)
			((adapter_p->broken_dma_handshake_phys >> 16) 
				& 0xffff));
	}
	hwi_pci_set_dio_address( mtr_p, 
		(DIO_LOCATION_EAGLE_DATA_PAGE & 0xffff),
		(DIO_LOCATION_EAGLE_DATA_PAGE >> 16) & 0xffff);

done:
	MTR_DBG_PRINT2(mtr_p, (
		"mtr%d: hwi_pci_install_card() done with %d.\n", 
		mtr_p->mtr_unit, error));


	return( error );
} /* hwi_pci_install_card() */

/*
 *	Read the BIA from adapter's EEPROM and 
 *	store it into mtr_bia(tr_if.tif_bia).
 */
static int
hwi_pci_read_node_address(mtr_private_t *mtr_p)
{
	int		error = NO_ERROR;
	ushort_t	tmp;
	BYTE            location;

	/* We only deal with PCIT and PCI2, and their values are
	 * actually the same here */
	if (mtr_p->mtr_dev_id == PCI_PCIT_DEVICE_ID) {
		location = PCIT_EEPROM_BIA_WORD0;
	} else {
		location = PCI2_EEPROM_BIA_WORD0;
	}

	tmp = serial_read_word(mtr_p, location);
	mtr_p->mtr_bia[0] = (uchar_t)((tmp     ) & 0xff);
	mtr_p->mtr_bia[1] = (uchar_t)((tmp >> 8) & 0xff);

	tmp = serial_read_word(mtr_p, (BYTE)(location + 1));
	mtr_p->mtr_bia[2] = (uchar_t)((tmp     ) & 0xff);
	mtr_p->mtr_bia[3] = (uchar_t)((tmp >> 8) & 0xff);

	tmp = serial_read_word(mtr_p, (BYTE)(location + 2));
	mtr_p->mtr_bia[4] = (uchar_t)((tmp     ) & 0xff);
	mtr_p->mtr_bia[5] = (uchar_t)((tmp >> 8) & 0xff);
	
	/*	Check that the node address is a valid Madge node address. */
	if ((mtr_p->mtr_bia[0] != MADGE_NODE_BYTE_0) ||
            (mtr_p->mtr_bia[1] != MADGE_NODE_BYTE_1) ||
            (mtr_p->mtr_bia[2] != MADGE_NODE_BYTE_2)) {
		cmn_err(CE_WARN, 
			"mtr%d: hwi_pci_read_node_address() "
			"invalid addr: 0x%x.%x.%x!",
			mtr_p->mtr_unit,
			mtr_p->mtr_bia[0], mtr_p->mtr_bia[1],
			mtr_p->mtr_bia[2]);
		error = EIO;
	}

	MTR_DBG_PRINT2( mtr_p, 
		("mtr%d: hwi_pci_read_node_address(): %d "
		 "0x%x:%x:%x:%x:%x:%x.\n", 
		 mtr_p->mtr_unit, error,
		 mtr_p->mtr_bia[0], mtr_p->mtr_bia[1],
		 mtr_p->mtr_bia[2], mtr_p->mtr_bia[3],
		 mtr_p->mtr_bia[4], mtr_p->mtr_bia[5]));

	return(error);
} /* hwi_pci_read_node_address() */

static void
mtr_reset_adapter( mtr_private_t *mtr_p)
{
	/* from hwi/pci2.c: hwi_pci2_install_card() */
	uchar_t	tmp;
	uchar_t	*io_addr;

	MTR_DBG_PRINT2( mtr_p, ( "mtr%d: mtr_reset_adapter() called.\n", 
		mtr_p->mtr_unit));

	if(mtr_p->mtr_dev_id != PCI_PCI2_DEVICE_ID)goto done;

	io_addr = (uchar_t *)(mtr_p->mtr_mem_addr + SGI_PCI2_RESET);
	tmp = INB(io_addr);

	tmp &= ~(PCI2_CHIP_NRES | PCI2_FIFO_NRES | PCI2_SIF_NRES);
	OUTB( io_addr, tmp);
	us_delay(20);		

	tmp |= PCI2_CHIP_NRES;
	OUTB( io_addr, tmp);
	
	tmp |= PCI2_SIF_NRES;
	OUTB( io_addr, tmp);

	tmp |= PCI2_FIFO_NRES;
	OUTB( io_addr, tmp);

done:
	MTR_DBG_PRINT2( mtr_p, ( "mtr%d: mtr_reset_adapter() done.\n", 
		mtr_p->mtr_unit));
	return;
} /* mtr_reset_adapter() */

static int
driver_prepare_adapter( mtr_private_t *mtr_p)
{
	int			error = NO_ERROR;
	mtr_adapter_t		*adapter_p;	
	FASTMAC_INIT_PARMS	*fastmac_parms;
	ushort_t		wHardFeatures;
	INITIALIZATION_BLOCK 	*init_block_p;

	short			fmplus_buffer_size;
	ushort_t		fmplus_max_buffers;
	uint_t			fmplus_max_buffmem;

	MTR_ASSERT( mtr_p != NULL );
	MTR_ASSERT( mtr_p->mtr_cfg_addr != NULL );
	MTR_ASSERT( mtr_p->mtr_mem_addr != NULL );
	MTR_ASSERT( mutex_owned(&mtr_p->init_mutex) );
	MTR_ASSERT( mutex_mine(&mtr_p->init_mutex) );
	MTR_DBG_PRINT2( mtr_p, ("mtr%d: driver_prepare_adapter() called.\n", 
		mtr_p->mtr_unit));

	adapter_p = &(mtr_p->adapter);
	/*	
	 *	0. Allocate the memory for initialization.	
	 */
	if( adapter_p->init_block_p == NULL ){
		adapter_p->init_block_p = (INITIALIZATION_BLOCK *) 
			kmem_alloc(sizeof(INITIALIZATION_BLOCK), KM_NOSLEEP);
		if( adapter_p->init_block_p == NULL ){
			cmn_err(CE_WARN, 
				"mtr%d: driver_prepare_adapter() "
				"kmem_alloc(%d) 2!",
				mtr_p->mtr_unit,
				sizeof(INITIALIZATION_BLOCK));
			error = ENOBUFS;
			goto done;
		}
	}
	bzero(adapter_p->init_block_p, sizeof(*adapter_p->init_block_p));
	init_block_p = adapter_p->init_block_p;

	/*	
	 *	1. Get some of the configuration from EEPROM.
	 */
	if(mtr_p->mtr_dev_id == PCI_PCI2_DEVICE_ID) {
		mtr_reset_adapter( mtr_p );

		init_block_p->ti_parms.madge_mc32_config = 0;

		/*	Get the permanent address from EEPROM! */
		if( (error = hwi_pci_read_node_address(mtr_p)) )goto done;

		adapter_p->ram_size   = 128 *
			serial_read_word(mtr_p, PCI2_EEPROM_RAM_SIZE);
		switch( adapter_p->ram_size ){
		case 256:
			fmplus_max_buffmem = FMPLUS_MAX_BUFFMEM_IN_256K;
			break;
		case 512:
			fmplus_max_buffmem = FMPLUS_MAX_BUFFMEM_IN_512K;
			break;
		default:
		case 128:
			fmplus_max_buffmem = FMPLUS_MAX_BUFFMEM_IN_128K;
			break;
		}
		MTR_DBG_PRINT2( mtr_p, 
			("mtr%d: driver_prepare_adapter(): ramsize: %d.\n", 
			mtr_p->mtr_unit, adapter_p->ram_size));

		wHardFeatures = serial_read_word(mtr_p, PCI2_HWF2);
		MTR_DBG_PRINT2( mtr_p, 
			("mtr%d: driver_prepare_adapter(): "
			 "wHardFeatures: 0x%x.\n", 
			 mtr_p->mtr_unit, wHardFeatures));

		if (!(wHardFeatures & PCI2_HW2_431_READY)) {
			/*
		 	 *	No support 4.31.
			 */
			cmn_err(CE_WARN,
				"mtr%d: not support 4.31: 0x%x.%x!",
				mtr_p->mtr_unit, wHardFeatures, 
				PCI2_HW2_431_READY);
			error = EIO;
			goto done;
		}

		if ( !(wHardFeatures & PCI2_BUS_MASTER_ONLY) ){
			cmn_err(CE_WARN,
				"mtr%d: not support bus master: 0x%x.%x!",
				mtr_p->mtr_unit, wHardFeatures,
				PCI2_BUS_MASTER_ONLY);
			error = EIO;
			goto done;
		}

#ifdef	DEBUG2
		{
			uchar_t	tmp[4];
			tmp[0] = INB((uchar_t*)(mtr_p->mtr_mem_addr + 
					SGI_PCI2_EEPROM_CONTROL));
			tmp[1] = INB((uchar_t*)(mtr_p->mtr_mem_addr + 
					SGI_PCI2_EEPROM_CONTROL + 1));
			tmp[2] = INB((uchar_t*)(mtr_p->mtr_mem_addr + 
					SGI_PCI2_EEPROM_CONTROL + 2));
			tmp[3] = INB((uchar_t*)(mtr_p->mtr_mem_addr + 
					SGI_PCI2_EEPROM_CONTROL + 3));
		
			MTR_DBG_PRINT2( mtr_p, 
				("mtr%d: driver_prepare_adapter(): "	
			 	"PCI2_EEPROM_CONTROL: 0x%x:%x:%x:%x.\n", 
				mtr_p->mtr_unit, tmp[0], tmp[1], 
				tmp[2], tmp[3]));

		}
#endif	/* DEBUG2 */

		/*	
		 *	According to hwi_pci2.c:, 
		 *	0 or 2 must be used.
		 */
		mtr_p->mtr_nselout_bits = (mtr_p->adapter.mtr_s16Mb ? 0 : 1);

		/*
		 *	Set the interrupt control register to enable 
		 *	SIF interrupt and PCI error interrupts.
	 	 */
		MTR_ASSERT( ((__psunsigned_t)(mtr_p->mtr_mem_addr + 
			SGI_PCI2_INTERRUPT_CONTROL) & 0x1) );
		OUTB( (uchar_t *)(mtr_p->mtr_mem_addr + 
			SGI_PCI2_INTERRUPT_CONTROL), 
			(uchar_t)(PCI2_SINTEN | PCI2_PCI_ERR_EN) );

		hwi_halt_eagle( mtr_p );

	} else if(mtr_p->mtr_dev_id == PCI_PCIT_DEVICE_ID) {

		/*	As pci_get_cfg() read four bytes by default,
		 *	we are not able to read a ushort_t at ushort_t boundary.
		 *	In that case, we read four bytes and write them back.
		 */
		volatile caddr_t	cfg_addr;
		uint_t	wControlReg, wControlReg2;

		cfg_addr 	= mtr_p->mtr_cfg_addr+MISC_CONT_REG;
		wControlReg 	= PCI_INW(cfg_addr);
		wControlReg2 	= 0x00E0ffff & wControlReg;
		PCI_OUTW( cfg_addr, wControlReg2);

		MTR_DBG_PRINT2(mtr_p, (
			"mtr%d: driver_prepare_adapter(): "
			"wControlReg: 0x%x -> 0x%x.\n",
			mtr_p->mtr_unit, wControlReg, wControlReg2));

		mtr_p->mtr_nselout_bits = 
			(mtr_p->adapter.mtr_s16Mb ? NSEL_16MBITS : NSEL_4MBITS);

		hwi_halt_eagle( mtr_p );

		/*
		 *	We read PCIT_EEPROM_HWF2 to determine the value
		 *	for madge_mc32_config: 0 or TRN_PCIT_BROKEN_DMA.
		 */
		adapter_p->broken_dma_handshake_phys = 0;
		adapter_p->broken_dma_handshake_addr = 0;

		adapter_p->broken_dma =
			serial_read_word(mtr_p, PCIT_EEPROM_HWF2 /* 0xf */);
		if( adapter_p->broken_dma == PCIT_BROKEN_DMA){
			init_block_p->ti_parms.madge_mc32_config = 
				TRN_PCIT_BROKEN_DMA;
		} else {
			init_block_p->ti_parms.madge_mc32_config = 0;
		}

		/*
		 *	Get the permanent address from EEPROM!
		 */
		hwi_pci_read_node_address(mtr_p);

		adapter_p->ram_size	= 128 *
			serial_read_word(mtr_p, PCIT_EEPROM_RAM_SIZE);
		switch( adapter_p->ram_size ){
		case 256:	
			fmplus_max_buffmem = FMPLUS_MAX_BUFFMEM_IN_256K;
			break;

		case 512:
			fmplus_max_buffmem = FMPLUS_MAX_BUFFMEM_IN_512K;
			break;

		case 128:
		default:
			fmplus_max_buffmem = FMPLUS_MAX_BUFFMEM_IN_128K;
			break;
		} /* switch ram_size */

	} else {
		error = EIO;
		MTR_ASSERT(0);
		goto done;
	}

	/*	
	 *	2. Determine the MTU size.		
	 */
	if( mtr_p->mtr_if_mtu == 0 ){
		/*	mtr_if_mtu is defined here only when it
		 *	has not been defined yet, such as during
		 *	initialization.
		 */
		mtr_p->mtr_if_mtu = 
		  min(max(1, mtr_mtu), 
		      mtr_p->adapter.mtr_s16Mb ? TRMTU_16M : TRMTU_4M);	
	}

	MTR_DBG_PRINT2( mtr_p,
		("mtr%d: driver_prepare_adapter(): 0x%x, mtr_if_mtu: %d.\n",
		mtr_p->mtr_unit, 
		init_block_p->ti_parms.madge_mc32_config,
		mtr_p->mtr_if_mtu));

	/*	
	 *	3. Allocate the resources for SCB and SSB.	
	 */
	if( adapter_p->scb_buff == NULL ){

		MTR_ASSERT(adapter_p->scb_size == 0);
		MTR_ASSERT(adapter_p->ssb_size == 0);
		MTR_ASSERT(adapter_p->ssb_buff == NULL);
		adapter_p->scb_size = 
			MAKE_ALIGNMENT(SCB_TEST_PATTERN_LENGTH, CACHELINE);
		adapter_p->ssb_size = 
			MAKE_ALIGNMENT(SSB_TEST_PATTERN_LENGTH, CACHELINE);
		adapter_p->test_size = 
			adapter_p->ssb_size + adapter_p->scb_size;
		adapter_p->scb_buff = kmem_zalloc(adapter_p->test_size, 
			MTR_KMEM_ARGS);
		if( adapter_p->scb_buff == NULL ){
			cmn_err(CE_WARN, 
				"mtr%d: kmem_zalloc(%d) for SSB & SCB!",
				mtr_p->mtr_unit, adapter_p->test_size);
			error = ENOBUFS;
			goto done;
		}

		mtr_p->test_dmamap_p = pciio_dmamap_alloc(
			mtr_p->mtr_connect_vertex, 	/* connect vertex */
			mtr_p->mtr_dev_desc, 		/* my device desc */
			adapter_p->test_size, 		/* size */
			PCIIO_DMA_CMD |
			PCIIO_BYTE_STREAM);

		if( mtr_p->test_dmamap_p== NULL ){
			cmn_err(CE_WARN, 
				"mtr%d: dmamap_alloc(%d) for SSB & SCB!",
				mtr_p->mtr_unit, adapter_p->test_size);
			error = ENOBUFS;
			goto done;
		}

		adapter_p->scb_phys = pciio_dmamap_addr( mtr_p->test_dmamap_p,
			kvtophys(adapter_p->scb_buff), adapter_p->test_size);
		if( adapter_p->scb_phys == NULL ){
			cmn_err(CE_WARN, 
				"mtr%d: pciio_dmamap_addr(0x%x, 0x%x, %d) "
				"for SCB!", 
				mtr_p->mtr_unit,
				mtr_p->test_dmamap_p, 
				adapter_p->scb_buff, adapter_p->scb_size);
			error = ENOBUFS;
			goto done;
		} /* if adapter_p->scb_phys */

		adapter_p->ssb_buff = 
			adapter_p->scb_buff + adapter_p->scb_size;
		adapter_p->ssb_phys = 
			adapter_p->scb_phys + adapter_p->scb_size;

		MTR_ASSERT( (ulong_t)adapter_p->ssb_buff > 
			(ulong_t)adapter_p->scb_buff );
		MTR_ASSERT( (ulong_t)adapter_p->ssb_phys > 
			(ulong_t)adapter_p->scb_phys );
	} else {
		MTR_ASSERT(mtr_p->test_dmamap_p != NULL );
		MTR_ASSERT(adapter_p->test_size != NULL );
		MTR_ASSERT(adapter_p->scb_size >= SCB_TEST_PATTERN_LENGTH);
		MTR_ASSERT(adapter_p->scb_buff != NULL);
		MTR_ASSERT(adapter_p->scb_phys != NULL );
		MTR_ASSERT(adapter_p->ssb_size >= SSB_TEST_PATTERN_LENGTH);
		MTR_ASSERT(adapter_p->ssb_buff != NULL);
		MTR_ASSERT(adapter_p->ssb_phys != NULL );
		MTR_ASSERT( (ulong_t)adapter_p->ssb_buff > 
			(ulong_t)adapter_p->scb_buff );
		MTR_ASSERT( (ulong_t)adapter_p->ssb_phys > 
			(ulong_t)adapter_p->scb_phys );
	}

	/*	
	 *	4. Set up the initialization block.	
	 */
	init_block_p->ti_parms.init_options 	= TI_INIT_OPTIONS_BURST_DMA;
	init_block_p->ti_parms.scb_addr 	= adapter_p->scb_phys;
	init_block_p->ti_parms.ssb_addr 	= adapter_p->ssb_phys;
	init_block_p->ti_parms.parity_retry 	= TI_INIT_RETRY_DEFAULT;
	init_block_p->ti_parms.dma_retry    	= TI_INIT_RETRY_DEFAULT;
	
	/*
	 *	5. Set up header to identify the smart software 
	 *	initialization parms.
	 */
	init_block_p->smart_parms.header.length		= 
		sizeof(SMART_INIT_PARMS);
	init_block_p->smart_parms.header.signature	= 
		SMART_INIT_HEADER_SIGNATURE;
	init_block_p->smart_parms.header.version	= 
		SMART_INIT_HEADER_VERSION;
	bcopy( mtr_p->mtr_bia, &init_block_p->smart_parms.permanent_address,
		sizeof(init_block_p->smart_parms.permanent_address));
	init_block_p->smart_parms.min_buffer_ram	= 
		SMART_INIT_MIN_RAM_DEFAULT;
	
	/*
	 *	6. Now work out the number of transmit and receive buffers.
	 */
	fmplus_buffer_size = max(FMPLUS_MIN_TXRX_BUFF_SIZE,
		FMPLUS_DEFAULT_BUFF_SIZE_SMALL);
	if( fmplus_buffer_size < FMPLUS_MIN_TXRX_BUFF_SIZE){
		cmn_err(CE_WARN,
			"mtr%d: fmplus_buffer_size (%d) < "
			"FMPLUS_MIN_TXRX_BUFF!",
			mtr_p->mtr_unit, fmplus_buffer_size);
		error = ENOBUFS;
		goto done;	
	}
	init_block_p->smart_parms.rx_tx_buffer_size = fmplus_buffer_size;
	
	fastmac_parms 			=  &init_block_p->fastmac_parms;
	fastmac_parms->max_frame_size 	=  mtr_p->mtr_if_mtu + 
					   MAX_MAC_FRAME_SIZE;
	fastmac_parms->header.length  	=  sizeof(FASTMAC_INIT_PARMS);
	fastmac_parms->header.signature	=  FMPLUS_INIT_HEADER_SIGNATURE;
	fastmac_parms->header.version	=  FMPLUS_INIT_HEADER_VERSION;
	fastmac_parms->int_flags 	|= INT_FLAG_RX ;
#ifdef ARB_NEEDED
	fastmac_parms->int_flags 	|= 
		(mtr_arb_enabled ? INT_FLAG_RING_STATUS_ARB : 0);
#endif	/* ARB_NEEDED */
#ifdef	LARGE_DMA_INTR_REQUIRED
	/*	Since we are using blind transfer mode,
	 *	we don't care TX interrupt.
	 */
	fastmac_parms->int_flags 	|= INT_FLAG_LARGE_DMA;
#endif	/* LARGE_DMA_INTR_REQUIRED */

	fastmac_parms->rx_slots 	=  
		min(mtr_max_rx_slots, FMPLUS_MAX_RX_SLOTS);
	fastmac_parms->tx_slots 	=  
		min(mtr_max_tx_slots, FMPLUS_MAX_TX_SLOTS);

	fastmac_parms->feature_flags 	=  FEATURE_FLAG_AUTO_OPEN;
	fastmac_parms->open_options	=	
		IS_MTR_FLAG_SET(mtr_p, MTR_FLAG_STATE_PROMISC) ?
		(OPEN_OPT_COPY_ALL_MACS | OPEN_OPT_COPY_ALL_LLCS) : 0;
	fastmac_parms->open_options 	|=
	        mtr_participant_claim_token ? OPEN_OPT_CONTENDER : 0;;
	fastmac_parms->group_address	=  0;
	if( (mtr_p->mtr_enaddr[0] | mtr_p->mtr_enaddr[1] |
	     mtr_p->mtr_enaddr[2] | mtr_p->mtr_enaddr[3] |
	     mtr_p->mtr_enaddr[4] | mtr_p->mtr_enaddr[5]) == 0){
		/*	No open address is configured, 
		 *	use BIA.
		 */
		bcopy(mtr_p->mtr_bia, mtr_p->mtr_enaddr, 
			sizeof(mtr_p->mtr_enaddr));
	}
	bcopy( mtr_p->mtr_enaddr, &fastmac_parms->open_address,
		sizeof(fastmac_parms->open_address));
	fastmac_parms->group_root_address= 0x0;
	MTR_ASSERT( sizeof(adapter_p->functional_address) == 
		sizeof(fastmac_parms->functional_address) );
	bcopy(&adapter_p->functional_address, 
		&fastmac_parms->functional_address, 
		sizeof(fastmac_parms->functional_address));
	MTR_DBG_PRINT2(mtr_p, ("mtr%d: max_frame_size: %d [%d, %d].\n", 
		mtr_p->mtr_unit, fastmac_parms->max_frame_size,
		MAX_FRAME_SIZE_16_MBITS, MAX_FRAME_SIZE_4_MBITS));
	MTR_ASSERT( fastmac_parms->max_frame_size == MAX_FRAME_SIZE_16_MBITS ||
		    fastmac_parms->max_frame_size == MAX_FRAME_SIZE_4_MBITS );

	/*
	 * 	7.  Next, work out how much memory on the card
	 *	we can use for buffers.
	 * 	Use the two numbers worked out above to determine 
	 *	the maximum number of buffers we can fit on the card.
	 */
	fmplus_max_buffers =
		(ushort_t)(fmplus_max_buffmem / ((fmplus_buffer_size + 1031) & 
			~1023)) - 5;

	/*
	 * 	8. Finally, allocate the buffers between transmit and receive.
	 */
	fastmac_parms->tx_bufs = 2 *
		((fastmac_parms->max_frame_size + fmplus_buffer_size) /
		 fmplus_buffer_size);

	fastmac_parms->rx_bufs = fmplus_max_buffers - 
		fastmac_parms->tx_slots - fastmac_parms->tx_bufs;

	if (fastmac_parms->rx_bufs < fastmac_parms->tx_bufs) {
		cmn_err(CE_WARN,
			"mtr%d: fmplus_buffer_size: %d < %d!",
			mtr_p->mtr_unit, fastmac_parms->rx_bufs,
			fastmac_parms->tx_bufs);
		error = ENOBUFS;
		goto done;
	}	

done:
	if( error != NO_ERROR ){
		cmn_err(CE_WARN, 
			"mtr%d: driver_prepare_adapter() failed: %d.\n",
			mtr_p->mtr_unit, error);
	}
	return( error );
} /* driver_prepare_adapter() */

static int
driver_start_adapter(mtr_private_t *mtr_p)
{
	int			error = NO_ERROR;
	int			idx;
	int			pages;
	int			alloc;
	int			new_size;
	caddr_t			mem;
	mtr_adapter_t		*adapter_p;	
	RX_SLOT 		**rx_slot_array, *next_rx_slot;
    	TX_SLOT 		**tx_slot_array, *next_tx_slot;
	FASTMAC_INIT_PARMS	*fastmac_parms;
	INITIALIZATION_BLOCK 	*init_block_p;
	__psunsigned_t          tmp;
	pciaddr_t               pci_addr;

	MTR_ASSERT( mtr_p != NULL );
	MTR_ASSERT( mutex_owned(&mtr_p->init_mutex) );
	MTR_ASSERT( mutex_mine(&mtr_p->init_mutex) );
	MTR_DBG_PRINT2( mtr_p, ("mtr%d: driver_start_adapter() called.\n",
		mtr_p->mtr_unit));

	adapter_p 	= &(mtr_p->adapter);
	rx_slot_array 	= &(adapter_p->rx_slot_array[0]);
	tx_slot_array 	= &(adapter_p->tx_slot_array[0]);
	init_block_p 	= mtr_p->adapter.init_block_p;
	fastmac_parms 	= &init_block_p->fastmac_parms;

#if FTK_VERSION_NUMBER_FTK_FM_H == 223
	if( (error = hwi_pci_install_card(mtr_p, (DOWNLOAD_RECORD *)
                (mtr_p->adapter.fmplus_image + 206))) != NO_ERROR) goto done;
#else
	if( (error = hwi_pci_install_card(mtr_p, 
		(DOWNLOAD_RECORD *)fmplus_image)) != NO_ERROR) goto done;
#endif

	if( (error = hwi_initialize_adapter( mtr_p )) != NO_ERROR)goto done;
	MTR_VERIFY_ADX( mtr_p );

	/*
	 *	Set up the slots!
	 */
	OUTS(adapter_p->sif_adr, DIO_LOCATION_SRB_POINTER);
	tmp = (__psunsigned_t)(INS(adapter_p->sif_datinc) & 0xffff);
	adapter_p->srb_dio_addr = (SRB_HEADER *)tmp;

	OUTS(adapter_p->sif_adr, DIO_LOCATION_ARB_POINTER);	/* overkill */
	tmp = (__psunsigned_t)(INS(adapter_p->sif_datinc) & 0xffff);
	adapter_p->arb_dio_addr = (arb_general_t *)tmp;

	OUTS(adapter_p->sif_adr, DIO_LOCATION_STB_POINTER);	/* overkill */
	tmp = (__psunsigned_t)(INS(adapter_p->sif_datinc) & 0xffff);
	adapter_p->stb_dio_addr = (FASTMAC_STATUS_BLOCK *)tmp;

	/* 	Get RX slot address information from adapter:	
	 *  Fastmac Plus spec: 7.3 slot initialization.	*/
	idx = 0;
	OUTS(adapter_p->sif_adr,
		(__psunsigned_t)&(adapter_p->stb_dio_addr->rx_slot_start));
	do {
		tmp = (__psunsigned_t)(INS(adapter_p->sif_dat) & 0xffff);
		rx_slot_array[idx] = (RX_SLOT *)tmp;
	} while( rx_slot_array[idx] == 0 );
	for(;;) {
		OUTS(adapter_p->sif_adr, (__psunsigned_t)
			&(rx_slot_array[idx]->next_slot));
		tmp = (__psunsigned_t)(INS(adapter_p->sif_dat) & 0xffff);
		next_rx_slot = (RX_SLOT *)tmp;
		if( next_rx_slot == rx_slot_array[0] ){
			break;				/* done */
		} 	
		rx_slot_array[++idx] = next_rx_slot;
		if( idx >= fastmac_parms->rx_slots ){
			cmn_err(CE_WARN, 
				"mtr%d: RX slot idx: %d >= %d!",
				mtr_p->mtr_unit, 
				idx, fastmac_parms->rx_slots);
			error = EIO;
			goto done;
		}
	};

	/*	Get TX slot address information from adapter:		*/
	idx = 0;
	OUTS(adapter_p->sif_adr, 
		(__psunsigned_t)&(adapter_p->stb_dio_addr->tx_slot_start));
	do {
		tmp = (__psunsigned_t)(INS(adapter_p->sif_dat) & 0xffff);
		tx_slot_array[idx] = (TX_SLOT *)tmp;
	} while( tx_slot_array[idx] == 0 );
	for(;;){
		OUTS(adapter_p->sif_adr, (__psunsigned_t)
			&(tx_slot_array[idx]->next_slot));
		tmp = (__psunsigned_t)(INS(adapter_p->sif_dat) & 0xffff);
		next_tx_slot = (TX_SLOT *)tmp;
		if( next_tx_slot == tx_slot_array[0] )
			break;			/* end of loop: done! */
		tx_slot_array[++idx] = next_tx_slot;
		if( idx >= fastmac_parms->tx_slots ){
			cmn_err(CE_WARN,
				"mtr%d: TX slot idx: %d >= %d!",
				mtr_p->mtr_unit,
				idx, fastmac_parms->tx_slots);
			error = EIO;
			goto done;
		}
	} 

#ifdef	DEBUG
	if( IS_MTR_DEBUG2(mtr_p) ){
		int	tmp;

		tmp = min(fastmac_parms->tx_slots,
			fastmac_parms->rx_slots);
		for(idx=0; idx < tmp; idx++ ){
			MTR_DBG_PRINT(mtr_p,
				("mtr%d: rx_slot_array[%d]: 0x%x, "
				"tx_slot_array[%d]: 0x%x.\n",
				mtr_p->mtr_unit,
				idx, rx_slot_array[idx],
				idx, tx_slot_array[idx]));
		} 
	} 
#endif	/* DEBUG */
	MTR_VERIFY_ADX( mtr_p );

	adapter_p->rx_tx_buf_size  = MAKE_ALIGNMENT( 
		fastmac_parms->max_frame_size, 1024);
	new_size = adapter_p->rx_tx_buf_size *
		(fastmac_parms->rx_slots + fastmac_parms->tx_slots);
	pages = (new_size + NBPP) / NBPP;
	new_size = pages * NBPP;
	alloc = 1;

	if( adapter_p->all_rx_tx_bufs_size != 0 ||
	    adapter_p->rx_tx_bufs_addr != NULL ){

		/*	As the size of the interface could be changed
		 *	when the adapter is restarted, we released
		 *	the old one and get a new if they are different! */
		MTR_ASSERT( adapter_p->all_rx_tx_bufs_size != 0 &&
			    adapter_p->rx_tx_buf_size != NULL );
		MTR_ASSERT( mtr_p->dmamap_p != NULL );

		if( adapter_p->all_rx_tx_bufs_size == new_size ){

			/*	Same size so we assume we could
			 *	continue to use the same set of
			 *	buffers and the same dmamap!
			 */
			alloc = 0;
		} else {

			/*	Different size so release the old
			 *	one first.
			 */
			kvpfree( adapter_p->rx_tx_bufs_addr, 
				(adapter_p->all_rx_tx_bufs_size / NBPP));
			adapter_p->rx_tx_bufs_addr = NULL;
			adapter_p->all_rx_tx_bufs_size = 0;

			pciio_dmamap_done(mtr_p->dmamap_p);
			pciio_dmamap_free(mtr_p->dmamap_p);
			mtr_p->dmamap_p = NULL;

		} /* if all_rx_tx_bufs_size */
	} /* if */

	if( alloc ){
		/*	Allocate the kernel buffers  for TX and RX.	*/
		mem = (caddr_t)kvpalloc( pages, VM_NOSLEEP|VM_UNCACHED|VM_PHYSCONTIG, 0); 
		if( mem == NULL ){
			cmn_err(CE_ALERT, 
				"mtr%d: failed to allocate memory for TX & "
				"RX: kvpalloc(%d)!", 
				mtr_p->mtr_unit, pages);
			error = ENOBUFS;
			goto done;
		}

		kvp_cnt++;

		/*	Allocate the dmamap for TX and RX. */
		mtr_p->dmamap_p = pciio_dmamap_alloc(
			mtr_p->mtr_connect_vertex, 	/* connect vertex */
			mtr_p->mtr_dev_desc, 		/* my dev desc */
			new_size, 
			PCIIO_DMA_DATA |
			PCIIO_BYTE_STREAM);

		if( mtr_p->dmamap_p == NULL ){
			cmn_err(CE_WARN, 
				"mtr%d: dmamap_alloc(%d) for TX & RX!",
				mtr_p->mtr_unit, 
				adapter_p->all_rx_tx_bufs_size);
			error = ENOBUFS;
			goto done;
		}
		
	} /* if alloc */ else {
		mem = adapter_p->rx_tx_bufs_addr;
	}

	dki_dcache_wbinval(mem, new_size);
	
 	adapter_p->all_rx_tx_bufs_size 	= new_size;
	adapter_p->rx_tx_bufs_addr 	= mem;
	
	pci_addr = pciio_dmamap_addr( mtr_p->dmamap_p,
			(paddr_t)kvtophys( mem ), new_size);
	if( pci_addr == NULL ){
		cmn_err(CE_WARN,
			"mtr%d: dmamap_addr(%d) for RX & TX!",
			mtr_p->mtr_unit,
			new_size);
		error = ENOBUFS;
		goto done;
	}

	MTR_DBG_PRINT2(mtr_p, ("mtr%d: rx_tx_bufs_addr: 0x%x, %d.%d.\n",
		mtr_p->mtr_unit,
		adapter_p->rx_tx_bufs_addr, 
		adapter_p->rx_tx_buf_size,
		adapter_p->all_rx_tx_bufs_size));

	MTR_VERIFY_ADX(mtr_p);
	for (idx = 0; idx < fastmac_parms->rx_slots; idx++) {
		adapter_p->rx_slot_buf[idx] = (caddr_t)mem;
		adapter_p->rx_slot_phys[idx]= pci_addr;
		mem       += adapter_p->rx_tx_buf_size;
		pci_addr  += adapter_p->rx_tx_buf_size;
		
		OUTS(adapter_p->sif_adr, (__psunsigned_t)
		     &((adapter_p->rx_slot_array[idx])->buffer_hiw));
		OUTS(adapter_p->sif_datinc, (__psunsigned_t)
		     (adapter_p->rx_slot_phys[idx] >> 16));
		OUTS(adapter_p->sif_dat, (__psunsigned_t)
		     (adapter_p->rx_slot_phys[idx] & 0xffff));
		
		MTR_DBG_PRINT2(mtr_p,
			       ("mtr%d: rx_slot_buf[%d]: 0x%x: 0x%x -> 0x%x.\n",
				mtr_p->mtr_unit, idx, (__psunsigned_t)
				&((adapter_p->rx_slot_array[idx])->buffer_hiw),
				adapter_p->rx_slot_buf[idx], 
				adapter_p->rx_slot_phys[idx]));
	} /* for */		
	mtr_p->adapter.active_rx_slot = 0;

	MTR_VERIFY_ADX(mtr_p);
	for (idx = 0; idx < fastmac_parms->tx_slots; idx++) {
		adapter_p->tx_slot_buf[idx] = (caddr_t)mem;
		adapter_p->tx_slot_phys[idx]= pci_addr;
		mem += adapter_p->rx_tx_buf_size;
		pci_addr += adapter_p->rx_tx_buf_size;

		OUTS(adapter_p->sif_adr, (__psunsigned_t)
		     &((adapter_p->tx_slot_array[idx])->large_buffer_hiw));
		OUTS(adapter_p->sif_datinc, (__psunsigned_t)
		     (adapter_p->tx_slot_phys[idx] >> 16));
		OUTS(adapter_p->sif_dat, (__psunsigned_t)
		     (adapter_p->tx_slot_phys[idx] &  0xffff));
		
		MTR_DBG_PRINT2(mtr_p,
			       ("mtr%d: tx_slot_buf[%d]: 0x%x: 0x%x -> 0x%x.\n",
				mtr_p->mtr_unit, idx, (__psunsigned_t)
				&((adapter_p->tx_slot_array[idx])->large_buffer_hiw),
				adapter_p->tx_slot_buf[idx], 
				adapter_p->tx_slot_phys[idx]));
	} /* for */

	mtr_p->adapter.active_tx_slot = 0;
	MTR_VERIFY_ADX( mtr_p );

#ifdef	DEBUG
	/*	Read out the permanent node address from STB.	*/
	OUTS(adapter_p->sif_adr, (__psunsigned_t)
		&(adapter_p->stb_dio_addr->permanent_address));
	for(idx = 0; idx < sizeof(NODE_ADDRESS); idx += sizeof(ushort_t)){
		ushort_t	bia;

		bia = INS(adapter_p->sif_datinc);
		if( bia != *((ushort_t *)&(mtr_p->mtr_bia[idx])) ){
			cmn_err(CE_NOTE,
				"mtr%d: mtr_bia[%d](0x%x) != bia(0x%x)!",
				mtr_p->mtr_unit,
				idx,  
				*((ushort_t *)&(mtr_p->mtr_bia[idx])),
				bia);
		}
	} /* for */

	/*	Read out the open address from STB.	*/
	OUTS(adapter_p->sif_adr, (__psunsigned_t)
		&(adapter_p->stb_dio_addr->open_address));
	for(idx = 0; idx < sizeof(NODE_ADDRESS); idx+= sizeof(ushort_t)){
		ushort_t	open_addr;
		ushort_t	*ptr;

		open_addr = INS(adapter_p->sif_datinc);
		ptr = (ushort_t *)&(mtr_p->mtr_enaddr[idx]);
		if( open_addr != *ptr ){
			cmn_err(CE_NOTE,
				"mtr%d: open_addr(0x%x) != "
				"mtr_enaddr[%d]: 0x%x!",
				mtr_p->mtr_unit, open_addr, idx, *ptr);
		}
	} /* for */
#endif	/* DEBUG */
	MTR_VERIFY_ADX( mtr_p );

	/*	Wait for the adpater to be open(AUTOOPEN is set).	*/
	/*	TO DO: it is stupid to wait. Interrupt!			*/
	for(idx = 0; idx < 4; idx++ ){
		mtr_get_open_status(mtr_p);

		if( adapter_p->stb.adapter_open != 0 && 
		    adapter_p->stb.open_error == EAGLE_OPEN_ERROR_SUCCESS )
			break;		/* open successfully */

		if( adapter_p->stb.open_error != EAGLE_OPEN_ERROR_SUCCESS ){
			cmn_err(CE_WARN, "mtr%d: open error(%d, 0x%x.%x)!",
				mtr_p->mtr_unit, idx, 
				adapter_p->stb.adapter_open, 
				adapter_p->stb.open_error);
		}

		/*	
		 *	We will give up the processor but not the lock.
		 */
		delay( HZ );			
	} /* for */
	MTR_VERIFY_ADX( mtr_p );
	
	OUTS(adapter_p->sif_adr, 
		(__psunsigned_t)&(adapter_p->stb_dio_addr->rx_slot_start));
	OUTS(adapter_p->sif_dat, 0);
	MTR_VERIFY_ADX( mtr_p );

done:
	if( error == NO_ERROR )MTR_SET_IFF( mtr_p, IFF_ALIVE );
	return(error);
} /* driver_start_adapter() */

static void
sys_pci_read_config_byte(mtr_private_t *mtr_p, uint_t offset, uchar_t *data)
{
	uint_t			tmp, tmp2;
	volatile caddr_t	cfg_addr;

	cfg_addr = (caddr_t)(mtr_p->mtr_cfg_addr+(offset & ~(0x3)));
	tmp = PCI_INW(cfg_addr);

	tmp2  =  tmp >> (8 * (offset & 0x3));
	*data =  tmp2 & 0xff;
	
	MTR_DBG_PRINT_PIO(mtr_p, 
		("mtr%d: sys_pci_read_config_byte(, %d,) 0x%x.%x.%x.\n",
		mtr_p->mtr_unit, offset, tmp, tmp2, *data));
	return;
} /* sys_pci_read_config_byte() */

static void
sys_pci_write_config_byte(mtr_private_t *mtr_p, uint_t offset, uchar_t data)
{
	volatile caddr_t	cfg_addr;
#ifdef	DEBUG_PIO
	uint_t	tmp, tmp2, offset2;
#else	/* DEBUG_PIO */
	uint_t	tmp, offset2;
#endif	/* DEBUG_PIO */

	MTR_DBG_PRINT_PIO(mtr_p, (
		"mtr%d: sys_pci_write_config_byte(, %d, 0x%x).\n",
		mtr_p->mtr_unit, offset, data));

	offset2 = offset & ~(0x3);
	cfg_addr = (caddr_t)(mtr_p->mtr_cfg_addr+offset2);
	tmp = PCI_INW( cfg_addr );
#ifdef	DEBUG_PIO
	tmp2 = tmp;
#endif	/* DEBUG_PIO */

	switch( offset & 0x3 ){
	case 0:
		tmp  &=  0xffffff00;
		break;
	case 1:
		tmp  &=  0xffff00ff;
		data <<= 8;
		break;
	case 2:
		tmp  &=  0xff00ffff;
		data <<= 16;
		break;
	case 3:
		tmp  &=  0x00ffffff;
		data <<= 24;
		break;
	}
	tmp  |= data;

	offset2  = offset & ~(0x3);
	cfg_addr = (caddr_t)(mtr_p->mtr_cfg_addr+offset2);
	PCI_OUTW( cfg_addr, tmp);

	MTR_DBG_PRINT_PIO( mtr_p, ( 
		"mtr%d: sys_pci_write_config_byte(): 0x%x -> 0x%x.\n",
		mtr_p->mtr_unit, tmp2, tmp));
	return;
} /* sys_pci_write_config_byte() */

static BYTE
at24_in(mtr_private_t *mtr_p)
{
	BYTE contents;            /* contents of port */
    
	MTR_DBG_PRINT_PIO(mtr_p, ("mtr%d: hwi_at24_input(): 0x%x.\n", 
				  mtr_p->mtr_unit, 
				  mtr_p->mtr_mem_addr + SGI_PCI2_SEEPROM_CONTROL));

	if (mtr_p->mtr_dev_id == PCI_PCIT_DEVICE_ID) {
#ifdef          DEBUG_PIO
		BYTE temp, org;
#else
		BYTE temp;
#endif          /* DEBUG_PIO */
		sys_pci_read_config_byte(mtr_p, EEPROM_OFFSET,
					 &contents);
#ifdef          DEBUG_PIO
		org = contents;
#endif
		
		mtr_p->adapter.bEepromByteStore = (BYTE) (contents & 0x8f);  
        
		temp = contents;
		contents &= 0x40;
		contents >>= 6;
		temp &= 0x30;
		temp >>=3;
		contents |= temp; 

		MTR_DBG_PRINT_PIO(mtr_p, 
			("mtr%d: hwi_at24_input() 0x%x -> 0x%x.\n",
			mtr_p->mtr_unit, org, contents));

        } else if (mtr_p->mtr_dev_id == PCI_PCI2_DEVICE_ID) {
    
		contents = INB((uchar_t*)mtr_p->mtr_mem_addr + 
			       SGI_PCI2_SEEPROM_CONTROL);
        	
		mtr_p->adapter.bEepromByteStore = contents & 
                    (BYTE) ~(PNP_EEDEN + PNP_EEDO + PNP_SSK);	
                    
		/* contents &= (BYTE) (PNP_EEDEN + PNP_EEDO + PNP_SSK);	*/
		MTR_DBG_PRINT_PIO(mtr_p, ("mtr%d: hwi_at24_input() 0x%x.\n", 
			mtr_p->mtr_unit, contents));
        } else {
		MTR_ASSERT(0);
		contents = 0xff;
	}

	return contents;    
} /* at24_in */

static void
at24_out(mtr_private_t *mtr_p, uchar_t contents)
{
	BYTE temp;

	MTR_DBG_PRINT_PIO(mtr_p, (
		"mtr%d: at24_out(0x%x, 0x%x).\n", 
		mtr_p->mtr_unit, contents));

        mtr_p->adapter.bLastDataBit = contents & (BYTE) PNP_EEDO;

	if (mtr_p->mtr_dev_id == PCI_PCI2_DEVICE_ID) {

		MTR_DBG_PRINT_PIO(mtr_p, 
			("mtr%d: hwi_at24_write_bits(, 0x%x): 0x%x.\n",
			mtr_p->mtr_unit, contents, 
			mtr_p->mtr_mem_addr + SGI_PCI2_SEEPROM_CONTROL));

		temp = contents & (BYTE) (PNP_EEDEN + PNP_EEDO + PNP_SSK);
		temp |= mtr_p->adapter.bEepromByteStore;
   
		OUTB((uchar_t *)(mtr_p->mtr_mem_addr + 
			SGI_PCI2_SEEPROM_CONTROL), temp);
        	
        } else if (mtr_p->mtr_dev_id == PCI_PCIT_DEVICE_ID) {
        
		MTR_DBG_PRINT_PIO(mtr_p,
			("mtr%d: hwi_at24_write_bits(, 0x%x).\n",
			mtr_p->mtr_unit, contents));

		temp = (BYTE) ((contents & 0x6) << 3);
		temp |= (BYTE) ((contents & 0x1) <<6);
		temp |= mtr_p->adapter.bEepromByteStore;
		sys_pci_write_config_byte(mtr_p, EEPROM_OFFSET, temp);

        } else {
		MTR_ASSERT(0);
	}
        
	/* wait for things to stabilise, at least 10 microseconds */   
 	us_delay(10);

	MTR_DBG_PRINT_PIO(mtr_p, ("mtr%d: hwi_at24_write_bits() -> 0x%x.\n",
		mtr_p->mtr_unit, temp));
	return;
} /* at24_out */


static void
at24_clear_clock(mtr_private_t *mtr_p)
{
	BYTE contents;

	MTR_DBG_PRINT_PIO(mtr_p, ("mtr%d: hwi_at24_clr_clk().\n", 
		mtr_p->mtr_unit));

	contents = at24_in(mtr_p);
	contents &= (BYTE) ~(PNP_SSK);

	contents &= (BYTE) ~(PNP_EEDO);
	contents |= mtr_p->adapter.bLastDataBit;
	at24_out(mtr_p, contents);

} /* at24_clear_clock() */

static uchar_t 
at24_read_data(mtr_private_t *mtr_p)
{
	BYTE data_bit;

	MTR_DBG_PRINT_PIO(mtr_p, ("mtr%d: hwi_at24_read_data().\n", 
		mtr_p->mtr_unit));

	at24_out(mtr_p, (BYTE) (PNP_EEDO+PNP_EEDEN));    
    
	at24_set_clock(mtr_p);
    
	at24_out(mtr_p, (BYTE) (PNP_EEDO+PNP_SSK));   
    
	data_bit = at24_in(mtr_p);
	data_bit &= (BYTE) PNP_EEDO;
	data_bit >>= 1;
    
        at24_out(mtr_p, (BYTE) (PNP_SSK + PNP_EEDO + PNP_EEDEN));  
    
	at24_clear_clock(mtr_p);  

	MTR_DBG_PRINT_PIO(mtr_p, ("mtr%d: hwi_at24_read_data() -> 0x%x.\n", 
				  mtr_p->mtr_unit, data_bit));
    
	return data_bit;       
} /* at24_read_data() */

static WBOOLEAN
at24_wait_ack(mtr_private_t *mtr_p)
{
	WBOOLEAN	acked = FALSE;
	int		cnt;
	uchar_t		data;

	MTR_DBG_PRINT_PIO(mtr_p, ("mtr%d: hwi_at24_wait_ack().\n", 
				  mtr_p->mtr_unit));

	for(cnt = 0; cnt < 10; cnt++ ){
		data =  at24_read_data(mtr_p);
		if( !(data & 1) ){
			acked = TRUE;
			break;
		}
	}
	
	MTR_DBG_PRINT_PIO(mtr_p, ("mtr%d: hwi_at24_wait_ack() -> 0x%x.\n", 
		mtr_p->mtr_unit, acked));
	if( acked != TRUE ){
		cmn_err(CE_WARN, 
			"mtr%d: hwi_at24_wait_ack(): no ack: 0x%x(0x%x)!",
			mtr_p->mtr_unit, data, acked);
	}
	return(acked);
} /* at24_wait_ack() */

static void
at24_set_clock(mtr_private_t *mtr_p)
{
	BYTE contents;

	MTR_DBG_PRINT_PIO(mtr_p, ("mtr%d: at24_set_clockk().\n", 
		mtr_p->mtr_unit));
	contents = at24_in(mtr_p);
	contents |= (BYTE) PNP_SSK;
    
	/* preserve contents of data bit before writing out */    
	contents &= (BYTE) ~(PNP_EEDO);
	contents |= mtr_p->adapter.bLastDataBit;
	at24_out(mtr_p, contents);

} /* at24_set_clock() */

static void
at24_serial_write_bit_string(mtr_private_t *mtr_p, uchar_t data_byte)
{
	BYTE data_bits, temp;
	int i;
    
	MTR_DBG_PRINT_PIO(mtr_p, 
		("mtr%d: hwi_at24_serial_write_bits(, 0x%x).\n",
		mtr_p->mtr_unit, data_byte));

	for(i=0 ; i<8 ; i++) {
		data_bits = (BYTE) (data_byte >> (7-i));
		data_bits &= 1;

		/* at24_write_data(mtr_p, data_bits): */
    
		data_bits <<=1;
		temp = at24_in(mtr_p);
		temp &= (BYTE) ~(PNP_EEDO);
		temp |= (BYTE) PNP_EEDEN;
		data_bits |= temp;   
		at24_out(mtr_p, data_bits);
        
		/* Toggle the clock bit to pass the data to the device */
		at24_set_clock(mtr_p);               
		at24_clear_clock(mtr_p);               
        }

} /* at24_serial_write_bit_string() */

static WBOOLEAN
at24_serial_send_cmd(mtr_private_t *mtr_p, uchar_t cmd)
{
	int i;
	WBOOLEAN sent = FALSE;
    
	for (i=0 ; i<10 ; i++) {
		/* wake up device: at24_start_bit(mtr_p): */
		at24_out(mtr_p, ((BYTE) (PNP_EEDO + PNP_EEDEN)));  
		at24_set_clock(mtr_p);
		at24_out(mtr_p, (BYTE) (PNP_SSK + PNP_EEDEN));
		at24_clear_clock(mtr_p);

		at24_serial_write_bit_string(mtr_p, cmd);
        
		if (at24_wait_ack(mtr_p)) {
			sent = TRUE;
			break;
		}                        
        }

	MTR_DBG_PRINT_PIO(mtr_p, 
		("mtr%d: hwi_at24_serial_send_cmd() -> 0x%x.\n", 
		mtr_p->mtr_unit, sent));
	return sent;    
	
} /* at24_serial_send_cmd() */

static BYTE
at24_serial_read_bit_string(mtr_private_t *mtr_p)
{
	BYTE data_byte = 0;
	BYTE data_bits;
	int i;
    
	MTR_DBG_PRINT_PIO(mtr_p, ("mtr%d: hwi_at24_serial_read_bit_string().\n", 
		mtr_p->mtr_unit));

	for(i=0 ; i<8 ; i++) {
		/* EEPROM clocks data out MSB first */
		data_bits = at24_read_data(mtr_p);
		data_byte <<=1;
		data_byte |= data_bits;        
	}
	return data_byte;    
	
} /* at24_serial_read_bit_string */

static WBOOLEAN at24_dummy_wait_ack(mtr_private_t *mtr_p)
{
	WBOOLEAN ack = FALSE;
	int i;
	BYTE temp;
    
	for (i=0 ; i<10 ; i++) {
		temp = at24_read_data(mtr_p);
		temp &= 1;
		
		if(temp) {
			ack = TRUE;
			break;
		}
        }
	return ack;    
}

static BYTE
at24_serial_read_byte(mtr_private_t *mtr_p, uchar_t addr)
{
	uchar_t data_byte = 0, cmd;
	WBOOLEAN ret_code;

	MTR_DBG_PRINT_PIO(mtr_p, ("hwi_at24_serial_read_byte(, 0x%x).\n",
				  mtr_p->mtr_unit, addr));

	/* at24_enable_eeprom(mtr_p): */
	cmd = at24_in(mtr_p);
	cmd |= (BYTE) PNP_EEDEN;
	at24_out(mtr_p, cmd);

	/* at24_serial_send_cmd_addr(mtr_p, AT24_WRITE_CMD, addr): */
	if (ret_code = at24_serial_send_cmd(mtr_p, AT24_WRITE_CMD)) {
		at24_serial_write_bit_string(mtr_p, addr);
		ret_code = at24_wait_ack(mtr_p);
#ifdef DEBUG3
	} else {
		MTR_ASSERT(0);
#endif
	}

	if (ret_code) {
		if(at24_serial_send_cmd(mtr_p, AT24_READ_CMD)) {
			data_byte = at24_serial_read_bit_string(mtr_p);

			if ( !(at24_dummy_wait_ack(mtr_p))) data_byte = 0xff; 
		}       
        }

	/* at24_stop_bit(mtr_p): deselect EEPROM */
	at24_out(mtr_p, (BYTE) (PNP_EEDEN));  
	at24_set_clock(mtr_p);
	at24_out(mtr_p, (BYTE) (PNP_SSK + PNP_EEDEN + PNP_EEDO));  
	at24_clear_clock(mtr_p);

	at24_out(mtr_p, (BYTE) 0);

	return data_byte;   
} /* at24_serial_read_byte() */

static WORD
serial_read_word(mtr_private_t *mtr_p, uchar_t io_addr) 
{
        return at24_serial_read_word(mtr_p, io_addr);
}

static WORD 
at24_serial_read_word(mtr_private_t *mtr_p, uchar_t word_addr)
{
	WORD data_word;
	BYTE data_low;
	BYTE addr = (BYTE) ((word_addr * 2) & 0xff);
	
	data_low = at24_serial_read_byte(mtr_p, addr);
	data_word = (WORD) at24_serial_read_byte(mtr_p, (BYTE) (addr + 1));
	
	data_word <<= 8;
	data_word |= data_low;

	return data_word;
}

#ifdef	PIO_VIA_ROUTINES
static uchar_t	
inb(uchar_t *addr)
{
	volatile uchar_t 		*addr2, data;
	addr2 = addr;
	data = *(addr2);

	INIT_MTR_DBG_PRINT_PIO( ("mtr*: inb(0x%x) -> 0x%x.\n", addr, data) );
	return(data);
} /* inb() */

static ushort_t
ins(ushort_t *addr)
{
	volatile ushort_t	*addr2, data;
	addr2 = addr;
	data = *(addr2);

	INIT_MTR_DBG_PRINT_PIO( ("mtr*: ins(0x%x) -> 0x%x.\n", addr, data));
	return(data);
} /* ins() */

/*static uint_t
inw(uint_t *addr)
{
	volatile uint_t		data;
	data = *((volatile uint_t *)addr);
	INIT_MTR_DBG_PRINT_PIO( ("mtr*: inw(0x%x) -> 0x%x.\n", addr, data));
	return(data);
} *//* inw() */

static void
outb(uchar_t *addr, uchar_t data)
{
	volatile uchar_t	data2 = data, *addr2;

	INIT_MTR_DBG_PRINT_PIO( ("mtr*: oub(0x%x) -> 0x%x.\n", addr, data2));
	addr2 = addr;
	*(addr2) = data2;
	return;
} /* outb() */

static void
outs(ushort_t *addr, ushort_t data)
{
	volatile ushort_t	data2 = data, *addr2;

	INIT_MTR_DBG_PRINT_PIO( ("mtr*: outs(0x%x) -> 0x%x.\n", addr, data2));
	addr2 = addr;
	*(addr2) = data2;
	return;
} /* outs() */

/*static void
outw(uint_t *addr, uint_t data)
{
	volatile uint_t		data2 = data;
	INIT_MTR_DBG_PRINT_PIO( ("mtr*: ouw(0x%x) -> 0x%x.\n", addr, data2));
	*((volatile uint_t *)addr) = data2;
	return;
} *//* outw() */
#endif	/* PIO_VIA_ROUTINES */

#ifdef	DEBUG
static void
mtr_dump_pkt(mtr_private_t *mtr_p, struct mbuf *m_p, char *msg)
{
	uchar_t	buf[MTR_DUMP_PKT_SIZE];
	int	cnt;
	int	len = m_length(m_p);

	MTR_ASSERT( MTR_DUMP_PKT_SIZE < MCLBYTES );

	printf("mtr%d: %s pkt: 0x%x %d.", mtr_p->mtr_unit, msg, m_p, len);
	len = (len > MTR_DUMP_PKT_SIZE) ? MTR_DUMP_PKT_SIZE : len;
	(void)m_datacopy(m_p, 0, len, (caddr_t)buf);

	for(cnt = 0; cnt < len; cnt++ ){
		if( !(cnt & 0xf) )printf("\n\t%d: ", cnt);
		printf("%x ", buf[cnt]);
	}
	printf("\n");

	return;
}  /* mtr_dump_pkt() */

static void
dump_vertex( vertex_hdl_t vertex )
{
/*	INIT_MTR_DBG_PRINT2( ("dump_vertex(0x%x): 0x%x.%x.%x.%x.%x.%x.\n",
		vertex, vertex->arbitrary, vertex->next, vertex->top,
		vertex->name, vertex->minor, vertex->unit) );*/
	return;
} /* dump_vertex() */

static void
mtr_dump_tx_rx_slots(mtr_private_t *mtr_p, char *from)
{
	int		idx;
	int		tx_slot, max_tx_slot;
	int		rx_slot, max_rx_slot;

	ushort_t	tx_len_s[FMPLUS_MAX_TX_SLOTS];
	ushort_t	tx_len_l[FMPLUS_MAX_TX_SLOTS];
	ushort_t	rx_len[FMPLUS_MAX_RX_SLOTS];

	mtr_adapter_t	*adapter_p = &(mtr_p->adapter);
	TX_SLOT         **tx_slot_array = adapter_p->tx_slot_array;
	RX_SLOT 	**rx_slot_array = adapter_p->rx_slot_array;

	MTR_VERIFY_ADX( mtr_p );

	max_tx_slot = adapter_p->init_block_p->fastmac_parms.tx_slots;
	MTR_ASSERT( max_tx_slot <= FMPLUS_MAX_TX_SLOTS );
	for( tx_slot = 0; tx_slot < max_tx_slot; tx_slot++ ){
		OUTS(adapter_p->sif_adr, (__psunsigned_t)
			(&tx_slot_array[tx_slot]->large_buffer_len));
		tx_len_l[tx_slot] = INS(adapter_p->sif_dat);
		OUTS(adapter_p->sif_adr, (__psunsigned_t)
			(&tx_slot_array[tx_slot]->small_buffer_len));
		tx_len_s[tx_slot] = INS(adapter_p->sif_dat);
	}

	max_rx_slot = adapter_p->init_block_p->fastmac_parms.rx_slots;
	MTR_ASSERT( max_rx_slot <= FMPLUS_MAX_RX_SLOTS );
	for( rx_slot = 0; rx_slot < max_rx_slot; rx_slot++ ){
		OUTS(adapter_p->sif_adr, 
			(__psunsigned_t)&(rx_slot_array[rx_slot]->buffer_len));
		rx_len[rx_slot] = INS(adapter_p->sif_dat);	
	}

	cmn_err(CE_DEBUG, 
		"mtr%d: %s active_tx_slot: %d, active_rx_slot: %d.\n",
		mtr_p->mtr_unit, from,
		adapter_p->active_tx_slot, adapter_p->active_rx_slot);
	for(idx=0; idx < min(max_tx_slot, max_rx_slot); idx++){
		cmn_err(CE_DEBUG,
			"mtr%d: tx_slot[%d]: %d.%d, rx_slot[%d]: %d.\n",
			mtr_p->mtr_unit, 
			idx, tx_len_l[idx], tx_len_s[idx],
			idx, rx_len[idx]);
	}
	return;
} /* mtr_dump_tx_rx_slots() */

/*
 *	My assert routine.
 */
static void 
mtr_assert(char * filedt_p, int line)
{
	cmn_err(CE_PANIC, "iee: ASSERT @ \"%s\".%d!", filedt_p, line);
	return;	
} /* mtr_assert() */


/*
 * print msg, then variable number of hex bytes.
 */
static void
mtr_hexdump(char *msg, char *cp, int len)
{
        int idx;
        int qqq;
        char qstr[512];
        int maxi = 128;
        static char digits[] = "0123456789abcdef";

        if (len < maxi)
                maxi = len;
        for (idx = 0, qqq = 0; qqq<maxi; qqq++) {
                if (((qqq%16) == 0) && (qqq != 0))
                        qstr[idx++] = '\n';
                qstr[idx++] = digits[cp[qqq] >> 4];
                qstr[idx++] = digits[cp[qqq] & 0xf];
                qstr[idx++] = ' ';
        }
        qstr[idx] = 0;
        printf("%s: %s\n", msg, qstr);
}

#endif /* DEBUG */

WORD ftk_version_for_fmplus = 223;
char fmplus_version[]       =  " Madge Fastmac Plus v1.61    ";
char fmplus_copyright_msg[] = 
    "Copyright (c) Madge Networks Ltd 1988-1996. All rights reserved.";

#endif  /* IP30 || IP32 || SN0 */
