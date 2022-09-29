/*
 * Copyright 1998, Silicon Graphics, Inc. 
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or 
 * duplicated in any form, in whole or in part, without the prior written 
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions 
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or 
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished - 
 * rights reserved under the Copyright Laws of the United States.
 */
/*
 * IRIX Alteon Tigon PCI Gigabit Ethernet Driver
 * 
 * Refer to "Gigabit Ethernet/PCI Network Interface Card Software
 * Interface Definition", Version 1.0, April 30, 1997, Alteon Networks.
 * (as updated by rev 11.3 of 2/98) (PN 020001)
 */

#if defined(IP31) || defined(IP30) || defined(IP27) || defined(IP32)
#ident "$Revision: 1.13 $"

#if 0
#define	DEBUG	1	/* XXX takeout later */
#endif

#define NEW_INIT 1

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/hwgraph.h>
#include <sys/invent.h>
#include <sys/iograph.h>
#include <sys/mbuf.h>
#include <sys/kmem.h>
#include <sys/PCI/pciio.h>
#include <sys/PCI/bridge.h>
#include <sys/idbgentry.h>
#include <sys/clksupport.h>
#include <net/if.h>
#include <net/raw.h>
#include <net/soioctl.h>
#include <ether.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <sys/tigon.h>
#include <sys/if_eg.h>
#ifdef IP32
#include <ksys/cacheops.h>
#endif
#ifdef GE_FICUS
#include <sys/kthread.h>
#endif	/* GE_FICUS */
#include <sys/pda.h>
#include <stddef.h>
#ifndef GE_FICUS
#include <net/rsvp.h>
#endif  /* GE_FICUS */

#ifndef IP32
extern int pcibr_rrb_alloc(vertex_hdl_t, int *, int *);	/* XXX */
#endif

#define LNK   0x1
#define RX    0x2
#define TX    0x4
#define INTR  0x8
#define CMD   0x10

int	eg_std_hist[512];
int	eg_jum_hist[256];

/* 
 *	MTU size for each interface. Default is 1500, use 9000 for jumbo frames	
 */
extern	int eg_mtu[EG_MAX_UNIT];

/*
 *	eg_send_coal_bd 	specifies how many buffers to send before 
 *				the NIC generates an interrupt 
 *
 *	eg_send_coal_ticks	specifies the number of uSECS to wait after at least
 *				one buffer has been sent.
 */
extern	int eg_send_coal_ticks;
extern	int eg_send_coal_bd;

/*
 *	eg_recv_coal_bd		specifies how many buffers to receive before
 *				the NIC generates an interrupt
 *
 *	eg_recv_coal_ticks	specifies the number of uSECS to wait after at least
 *				one buffer has been received. Since this depends on 
 *				mtu size, it is an array indexed by device number.
 *				Use 72 for 1500 mtu and 17 for 9000 mtu as starting 
 *				values. 
 *
 */
extern	int eg_recv_coal_bd;
extern	int eg_recv_coal_ticks[EG_MAX_UNIT];

uint eg_autoneg_array[EG_MAX_UNIT] = {1,1,1,1,1,1,1,1,1,1};

U32 eg_trace = TRACE_TYPE_SEND | TRACE_TYPE_RECV| TRACE_TYPE_COMMAND | TRACE_TYPE_MAC;
int eg_ifqlen = 2048;
int eg_stat_ticks = TG_TICKS_PER_SEC * 5;  /* 10000 * 5 */
int eg_fill_thresh = 10;

/* NFS ficus client bug */
#ifdef GE_FICUS
int eg_recl_thresh = 20;
#else
int eg_recl_thresh = 0;
#endif

int eg_rx2_thresh = 0;
int eg_skip_rxcksum = 0;
int eg_hardreset=0;
int eg_watchdog_on=1;
int eg_watch_time=1;

#ifdef GE_FICUS
#define INV_ETHER_GE    28      /* Gigabit Ethernet: sys/invent.h */
int mbuf_hasroom(struct mbuf *, int);  /* same as m_hasroom in kudzu */
graph_error_t if_hwgraph_alias_add(vertex_hdl_t, char *);
#endif

/*
 * Declarations for variables in master.d/if_eg
 */


/*
 * Prototypes
 */
void		if_eginit(void); 
int		if_egopen(dev_t *devp, int flag, int otyp, struct cred *crp);
static void	eg_ifattach(struct eg_info *ti, int unit);
int		if_egattach(vertex_hdl_t conn_vhdl);
static int	eg_detach(vertex_hdl_t enet_vhdl);
static int	eg_allocti(struct eg_info *ti);
static void	eg_deallocti(struct eg_info *ti);
static int	eg_initialize(struct eg_info *ti);
static struct mbuf *eg_reclaim(struct eg_info *ti, struct ifqueue *);
static int	eg_xinit(struct etherif *eif, int flags);
static int	eg_init(struct etherif *eif, int flags);
static void	eg_xreset(struct etherif *eif);
static void	eg_fill(struct eg_info *ti);
static void	eg_fill_std(struct eg_info *ti);
static void	eg_fill_jmb(struct eg_info *ti);
static void	eg_reset(struct etherif *eif);
static int	eg_ioctl(struct etherif *eif, int cmd, void *data);
#ifdef IP32
static void     eg_intr(eframe_t *, struct eg_info *ti);
#else
static void     eg_intr(struct eg_info *ti);
#endif
static struct ifqueue *eg_recv(struct eg_info *ti, int nic_rxcons, struct ifqueue *);
static int	eg_transmit(struct etherif *eif, struct etheraddr *edst,
			struct etheraddr *esrc, u_short type, struct mbuf *m0);
static void	eg_selfsnoop(struct eg_info *ti,
			 struct ether_header *eh,
			 struct mbuf *m0, int len);
static void	eg_watchdog(struct ifnet *ifp);
static void     eg_linkstate(struct eg_info *ti, U32 code);
static void	eg_dump(int unit);
static void     eg_dumpif(struct ifnet *ifp);
static void     eg_dumpti(struct eg_info *ti);
static int      eg_resetcard(struct eg_info *ti);
static int      eg_startcard(struct eg_info *ti);
static int      eg_loadFW(struct eg_info *ti);
static void     eg_freeFW(struct eg_info *ti);
static void     eg_stopFW(struct eg_info *ti);
static void     eg_startFW(struct eg_info *ti);
static void     eg_free(struct eg_info *ti);
static void     eg_memcpy(caddr_t toAddr, caddr_t fromAddr, int len);
static int      eg_writeshmem(struct eg_info *ti, U32 tgAddr, caddr_t useraddr, int len);
static int      eg_readshmem(struct eg_info *ti, U32 tgAddr, caddr_t useraddr, int len);
static void     eg_tuneparams(struct eg_info *ti);
static void     eg_errupdate(struct eg_info *ti);
#ifndef IP32
static void	eg_checkbus(struct eg_info *ti);
#endif
static int      eg_docmd(struct eg_info *ti, U32 cmd, U32 index, U32 code);
static int      eg_waitforevent(struct eg_info *ti, U32 event);
static int      eg_getmacaddr(struct eg_info *ti, struct etheraddr *macaddr);
static int      readTGSerEeprom(struct eg_info *ti, U32 tgAddr, caddr_t userAddr, int len);
static U32      read_serial_eeprom(struct eg_info *ti, U32 addr, int *worked);
static void     ser_eeprom_prep(struct eg_info *ti, U8 pattern);
static int      ser_eeprom_chk_ack(struct eg_info *ti);
static int      get_serial_byte(struct eg_info *ti, U32 addr);
static mval_t   CalcIndex(unsigned char *addr, int len);

#ifndef GE_FICUS
/*
 * For packet scheduling/RSVP
 */
static void	eg_ps_register(struct eg_info *ti, int reinit);
static void	eg_setstate(struct ifnet *ifp, int setting);
static int	eg_txfree_len(struct ifnet *ifp);
#endif /* GE_FICUS */

static void    eg_checkFW(struct eg_info *ti, U32 tgAddr);
static void    eg_printmagicnum(struct eg_info *ti);

#ifdef IP32
#define W_REG(reg, val)		pciio_pio_write32((val), &(reg))
#define R_REG(reg)		pciio_pio_read32(&(reg))
#define W_REGp(reg, val)	pciio_pio_write32((val), (volatile uint32_t *)(reg))
#define R_REGp(reg)		pciio_pio_read32((volatile uint32_t *)(reg))
#define W_REG64(reg, val)	pciio_pio_write64((val), &(reg))
#define R_REG64(reg)		pciio_pio_read64(&(reg))
#define AND_REG(reg, val)	pciio_pio_write32(pciio_read32(&(reg)) & val, &(reg))
#define OR_REG(reg, val)	pciio_pio_write32(pciio_read32(&(reg)) | val, &(reg))
#else
#define	W_REG(reg,val)		(reg) = (val)
#define R_REG(reg)		(reg)
#define	W_REGp(regp,val)	(*((U32 *)(regp))) = (val)
#define R_REGp(regp)		(*((U32 *)(regp)))
#define AND_REG(reg,val) 	(reg) &= (val)
#define OR_REG(reg,val) 	(reg) |= (val)

/* ted: for now.   Might have to change this to two 32-bit ops */
#define	W_REG64(reg,val)	(reg) = (val)
#define R_REG64(reg)		(reg)

#endif

#define HI32(x)  ((x >> 32) & 0xffffffff) 
#define LO32(x)  (x & 0xffffffff)
/*#define HI32(x)  LO32(x) */

#ifdef IP30
/* XXX heart_war needs to be added for octane */
#define CACHE_INVAL(addr, len)
#define	CACHE_WB(addr, len)
#elif IP32
#define CACHE_INVAL(addr, len)	__vm_dcache_inval(addr, len)
#define	CACHE_WB(addr, len)	dki_dcache_wb(addr, len)
#else
#define CACHE_INVAL(addr, len)
#define	CACHE_WB(addr, len)
#endif

#define	DBG_PRINT(a)	if (ti->ti_if.if_flags & IFF_DEBUG) printf a
#define EG_PRINT(t, bit, level, str)  if (((t)->ti_dbg_mask & bit) && ((t)->ti_dbg_level > level)) { printf("eg%d: ",ti->ti_unit); printf str ; }

#define	ALIGNED(a, n)	(((u_long)(a) & ((n)-1)) == 0)

#define ETHER_HDRLEN    (sizeof (struct ether_header))

#define	KVTOIOADDR_CMD(ti, addr)   pciio_dmatrans_addr((ti)->ti_conn_vhdl, 0, \
		kvtophys((caddr_t)(addr)), sizeof(int), PCIIO_DMA_A64 | PCIIO_DMA_CMD )

#ifdef GE_FICUS
#define	KVTOIOADDR_DATA(ti, addr)   pciio_dmatrans_addr((ti)->ti_conn_vhdl, 0,\
		kvtophys((caddr_t)(addr)), sizeof(int), PCIIO_DMA_A64 | PCIIO_DMA_DATA )
#else
#define	KVTOIOADDR_DATA(ti, addr)	pciio_dmamap_addr((ti)->ti_fastmap, \
		kvtophys((caddr_t)(addr)), sizeof (int))
#endif

#define	DELTATXD(h, t)		((h - t) & (SEND_RING_ENTRIES - 1))
#define	DELTARXD(p, c)		((p - c) & (RECV_STANDARD_RING_ENTRIES - 1))
#define	DELTARXJUMBO(p, c)	((p - c) & (RECV_JUMBO_RING_ENTRIES - 1))
#define	DELTARXD2(p, c)		((p - c) & (RECV_RETURN_RING_ENTRIES - 1))
#define INCRING(i, size)	 (((i) + 1) & ((size) -1))

#define	eg_info_set(v,i)	hwgraph_fastinfo_set((v),(arbitrary_info_t)(i))
#define	eg_info_get(v)		((struct eg_info *)hwgraph_fastinfo_get((v)))

int if_egdevflag = D_MP;

/*
 * Opsvec for ether.[ch] interface.
 */
static struct etherifops egops =
	{ eg_xinit, eg_xreset, eg_watchdog, eg_transmit, eg_ioctl };


#ifdef EG4GB_TEST
/*
 *      Make the NIC try to send the last byte on a 4GB
 *      boundary. If the firmware is correct, the nic will
 *      not halt.
 */

static void
eg_fault_nic (struct eg_info *ti)
{

	int	prod;
	int	nic_fault_len;
	int	nic_fault_flags;
	U64	nic_fault_addr;

	prod = ti->ti_txprod;
	nic_fault_addr = 0x98000000fffffc50ll;
	nic_fault_len = 0x03b0;
	nic_fault_flags = 0x4;
	ti->ti_txring[prod].host_addr =  KVTOIOADDR_DATA(ti, nic_fault_addr);
	ti->ti_txring[prod].w0.w0 =  (nic_fault_len << 16) | nic_fault_flags;
	prod = INCRING(prod, SEND_RING_ENTRIES);
	W_REG(ti->ti_shmem->sendProdIndex, prod);
}
#endif



void
if_eginit(void)
{
#if DEBUG && ATTACH_DEBUG
	cmn_err(CE_CONT, "if_eginit()");
#endif
	/* XXX rags: assert ring sizes etc */

	pciio_driver_register(TG_VENDOR, TG_DEVICE, "if_eg", 0);

	idbg_addfunc("eg_dump", (void (*)())eg_dump);
	idbg_addfunc("eg_checkFW", (void (*)())eg_checkFW);
}



int
if_egattach(vertex_hdl_t conn_vhdl)
{
	graph_error_t	rc;
	struct eg_info	*ti;
	vertex_hdl_t enet_vhdl;
	device_desc_t eg_dev_desc;
	struct etheraddr eg_ea;


#if DEBUG && ATTACH_DEBUG
	cmn_err(CE_CONT, "%v: if_egattach()", conn_vhdl);
#endif

	ti = NULL;

	/* add our vertex to the graph */
	if ((rc = hwgraph_char_device_add(conn_vhdl, "eg", "if_eg", &enet_vhdl))
		!= GRAPH_SUCCESS) {
	        cmn_err(CE_ALERT, "%v: if_egattach: hwgraph_char_device_add error %d", conn_vhdl, rc);
		goto bad;
	}

	/* allocate our private per-device data structure */
	if ((ti = (struct eg_info*) kmem_zalloc(sizeof (struct eg_info), KM_SLEEP)) == NULL) {
		cmn_err(CE_ALERT, "%v: if_egattach: kmem_zalloc failure", enet_vhdl);
		goto bad;
	}

	/* associate ti and hwgraph vertex with each other */
	ti->ti_conn_vhdl = conn_vhdl;
	ti->ti_enet_vhdl = enet_vhdl;
	eg_info_set(enet_vhdl, ti);

	/* map in PIO shared memory segement from NIC */
	if ((ti->ti_shmem = (tg_shmem_t *) pciio_piotrans_addr(conn_vhdl, 0, 
		PCIIO_SPACE_WIN(0), 0, sizeof (tg_shmem_t), 0)) == NULL) {
		cmn_err(CE_ALERT, "%v: if_egattach: shmem piotrans_addr failure", enet_vhdl);
		goto bad;
	}

	/* fix up device descriptor */
	eg_dev_desc = device_desc_dup(enet_vhdl);
	device_desc_intr_name_set(eg_dev_desc, "GBit Ethernet");
	device_desc_default_set(enet_vhdl,eg_dev_desc);
#ifdef IP32
	device_desc_intr_swlevel_set(eg_dev_desc, (ilvl_t)splimp);
#endif

	/* we don't know our unit# yet */
	ti->ti_unit = -1;

	/* XXX rags: is it really necessary to stop card again here */
	if (eg_resetcard(ti)) {
		cmn_err(CE_ALERT, "%v: if_egattach: self-diag failure", enet_vhdl);
		goto bad;
	}

	/* allocate memory for rings, geninfo struct etc */
	if (eg_allocti(ti)) {
		cmn_err(CE_ALERT, "%v: if_egattach: eg_allocti failure", enet_vhdl);
		goto bad;
	}

	/*  register our interrupt routine */
	ti->ti_intr = pciio_intr_alloc(conn_vhdl, eg_dev_desc, PCIIO_INTR_LINE_A, enet_vhdl);
	ASSERT(ti->ti_intr);
	pciio_intr_connect(ti->ti_intr, (intr_func_t) eg_intr, (intr_arg_t) ti, (void *)0);

#ifndef GE_FICUS
	/* create a fast pciio dmamap */
	if ((ti->ti_fastmap = pciio_dmamap_alloc(conn_vhdl, eg_dev_desc, sizeof (int),
		PCIIO_DMA_DATA | PCIIO_DMA_A64 | PCIIO_FIXED)) == NULL) {
		cmn_err(CE_ALERT, "%v: if_egattach: pciio_dmamap_alloc() failure", enet_vhdl);
		goto bad;
	}
#endif
	device_inventory_add(enet_vhdl, INV_NETWORK, INV_NET_ETHER, INV_ETHER_GE, -1, 0);



	eg_printmagicnum(ti);

	/* get the macaddr from EEPROM and put in shmem first */
	if (eg_getmacaddr(ti, &eg_ea)) {
		cmn_err(CE_ALERT, "%v: if_egattach: eg_getmacaddr() failure", enet_vhdl);
		goto bad;
	}

	/*  update the ifnet structure */
	bcopy(eg_ea.ea_vec, ti->ti_addr.ea_vec, 6);
        bcopy(eg_ea.ea_vec, ti->ti_curaddr, 6);


	if (showconfig)
		cmn_err(CE_CONT,"%v: hardware ethernet address %s\n",
			ti->ti_enet_vhdl, ether_sprintf(ti->ti_curaddr));

	init_bitlock(&ti->ti_flags, TIF_TLOCK, "eg drv tx lock", 0);
	init_bitlock(&ti->ti_rflags, TIF_RLOCK, "eg drv rx lock", 0);
	sv_init(&ti->ti_sv, SV_DEFAULT, "eg sv");

#if DEBUG
	cmn_err(CE_CONT,"%v: hardware ethernet address %s\n",
		ti->ti_enet_vhdl, ether_sprintf(ti->ti_curaddr));
	
	printf("eg_attach successful, shmem:0x%x\n", ti->ti_shmem);
#endif

	return (0);

bad:
	eg_detach(enet_vhdl);
	return (ENOMEM);
}

static int
eg_detach(vertex_hdl_t enet_vhdl)
{
	struct eg_info *ti;

	if (enet_vhdl == GRAPH_VERTEX_NONE)
		return (0);

	ti = eg_info_get(enet_vhdl);
	if (ti == NULL)
		return (0);

	eg_info_set(enet_vhdl, NULL);
	eg_deallocti(ti);

	/* pciio_piountrans_addr() of ti_shmem would go here */

	/* XXX remove the enet vhdl */

	kmem_free((caddr_t) ti, sizeof (struct eg_info));

	return (0);
}

/*
 * Allocate eg_info fields.  Return 0 or errno.
 */
static int
eg_allocti(struct eg_info *ti)
{
	int	size;
	int	npgs;
	pgno_t	pgno;

	if (ti->ti_txring)
		return 0;

	/*
	 * policy: use contig_memalloc for DMA accessable memory 
	 * and kmem_zalloc for host private datastructures
	 */

	/* allocate gen_info */
	size = sizeof(struct tg_gen_info);
	npgs = (size + NBPP - 1)/NBPP;
	if ((pgno = contig_memalloc(npgs, 1, VM_DIRECT)) == NULL)
		goto fail;
	ti->ti_geninfo = (struct tg_gen_info *)small_pfntova_K0(pgno);


	/* XXX do I need to bzero contig_memalloc'ed pages */

	/* allocate event ring */
	size = EVENT_RING_ENTRIES * sizeof(struct tg_event);
	npgs = (size + NBPP - 1)/NBPP;
	if ((pgno = contig_memalloc(npgs, 1, VM_DIRECT)) == NULL)
		goto fail;
	ti->ti_evring = (struct tg_event *)small_pfntova_K0(pgno);


	/* allocate send ring */
	size = SEND_RING_ENTRIES * sizeof(struct send_bd);
	npgs = (size + NBPP - 1)/NBPP;
	if ((pgno = contig_memalloc(npgs, 1, VM_DIRECT)) == NULL)
		goto fail;
	ti->ti_txring = (struct send_bd *)small_pfntova_K0(pgno);

	/* allocate recv1 ring */
	size = RECV_STANDARD_RING_ENTRIES * sizeof(struct recv_bd);
	npgs = (size + NBPP - 1)/NBPP;
	if ((pgno = contig_memalloc(npgs, 1, VM_DIRECT)) == NULL)
		goto fail;
	ti->ti_rxring_std = (struct recv_bd *)small_pfntova_K0(pgno);

	/* allocate recv2 ring */
	size = RECV_RETURN_RING_ENTRIES * sizeof(struct recv_bd);
	npgs = (size + NBPP - 1)/NBPP;
	if ((pgno = contig_memalloc(npgs, 1, VM_DIRECT)) == NULL)
		goto fail;
	ti->ti_rxring_rtn = (struct recv_bd *)small_pfntova_K0(pgno);

	/* allocate jumbo ring */
	size = RECV_JUMBO_RING_ENTRIES * sizeof(struct recv_bd);
	npgs = (size + NBPP - 1)/NBPP;
	if ((pgno = contig_memalloc(npgs, 1, VM_DIRECT)) == NULL)
		goto fail;
	ti->ti_rxring_jumbo = (struct recv_bd *)small_pfntova_K0(pgno);

	/* allocate transmit pending-mbuf vector */
	size = SEND_RING_ENTRIES * sizeof (struct mbuf*);
	if ((ti->ti_txm = (struct mbuf**) kmem_zalloc(size, KM_SLEEP | VM_CACHEALIGN)) == NULL)
		goto fail;

	/* allocate receive pending-mbuf vector */
	size = RECV_STANDARD_RING_ENTRIES * sizeof (struct mbuf*);
	if ((ti->ti_rxm_std = (struct mbuf**) kmem_zalloc(size, KM_SLEEP | VM_CACHEALIGN)) == NULL)
		goto fail;

	/* allocate receive jumbo pending-mbuf vector */
	size = RECV_JUMBO_RING_ENTRIES * sizeof (struct mbuf*);
	if ((ti->ti_rxm_jumbo = (struct mbuf**) kmem_zalloc(size, KM_SLEEP | VM_CACHEALIGN)) == NULL)
		goto fail;

	/* allocate space for ti_evprodptr */
	/* dont have to contig_malloc for less than pagesize */
	size = EG_CACHELINE_SIZE;
	if ((ti->ti_evprodptr = (U64 *) kmem_zalloc(size, KM_SLEEP | VM_CACHEALIGN | VM_DIRECT)) == NULL)
		goto fail;

	/* allocate space for ti_txconsptr */
	/* dont have to contig_malloc for less than pagesize */
	size = EG_CACHELINE_SIZE;
	if ((ti->ti_txconsptr = (U64 *) kmem_zalloc(size, KM_SLEEP | VM_CACHEALIGN | VM_DIRECT)) == NULL)
		goto fail;

	/* allocate space for ti_rxconsptr */
	/* dont have to contig_malloc for less than pagesize */
	size = EG_CACHELINE_SIZE;
	if ((ti->ti_rxrtn_prod = (U64 *) kmem_zalloc(size, KM_SLEEP | VM_CACHEALIGN | VM_DIRECT)) == NULL)
		goto fail;

	/* allocate space for refresh statistics */
	size = sizeof (struct tg_stats);
	if ((ti->ti_stats2 = (struct tg_stats *) kmem_zalloc(size, KM_SLEEP | VM_CACHEALIGN)) == NULL)
		goto fail;

	ti->ti_nrrbs = 2;

	return (0);
	
fail:
	/* can be called even if only some were successful */
	eg_deallocti(ti);

	return (ENOMEM);
}

/*
 * Free eg_info fields prior to unloading.
 */
static void
eg_deallocti(struct eg_info *ti)
{
	int	size;

	/* XXX free the contig pages using kpfree or something similar */

	if (ti->ti_geninfo)
		;  

	size = EVENT_RING_ENTRIES * sizeof(struct tg_event);
	if (ti->ti_evring)
		;			/* XXX no contig_memalloc_free yet */

	size = RECV_STANDARD_RING_ENTRIES * sizeof(struct recv_bd);
	if (ti->ti_rxring_std)
		;			/* XXX no contig_memalloc_free yet */
	size = RECV_RETURN_RING_ENTRIES * sizeof(struct recv_bd);
	if (ti->ti_rxring_rtn)
		;			/* XXX no contig_memalloc_free yet */

	size = SEND_RING_ENTRIES * sizeof (struct mbuf*);
	if (ti->ti_txm)
		kmem_free((void*)ti->ti_txm, size);

	size = RECV_STANDARD_RING_ENTRIES * sizeof (struct mbuf*);
	if (ti->ti_rxm_std)
		kmem_free((void*)ti->ti_rxm_std, size);

	size = RECV_JUMBO_RING_ENTRIES * sizeof (struct mbuf*);
	if (ti->ti_rxm_jumbo)
		kmem_free((void*)ti->ti_rxm_jumbo, size);

	size = EG_CACHELINE_SIZE;
	if (ti->ti_evprodptr)
		kmem_free((void*)ti->ti_evprodptr, size);

}


/*
 * initialize bunch of things in host memory:  rings, RCBs ...
 */
static int
eg_initialize(struct eg_info *ti)
{
	U64 tmp;
	U32 pci_state;

#if (TIGON_REV & TIGON_REV4)
	U32 mhc;
	mhc = R_REG(ti->ti_regs.gen_control.misc_host_control);
	W_REG(ti->ti_regs.gen_control.misc_host_control,
	      mhc | TG_MHC_WORD_SWAP & ~TG_MHC_BYTE_SWAP);
#else
	W_REG(ti->ti_regs.gen_control.misc_host_control,
	      ((TG_MHC_WORD_SWAP | TG_MHC_CLEAR_RUPT) |
	       ((TG_MHC_WORD_SWAP | TG_MHC_CLEAR_RUPT) << 24)));
#endif
	/* read it back to make sure the write is done */
	R_REG(ti->ti_regs.gen_control.misc_host_control);

#if (TIGON_REV & TIGON_REV5)
	W_REG(ti->ti_regs.gen_control.misc_local_control,
	      TG_MLC_SRAM_BANK_512K);
	W_REG(ti->ti_regs.gen_control.misc_config,
	      TG_MSC_ENA_SYNC_SRAM_TIMING);
#endif	

	pci_state = TG_PCI_STATE_DMA_READ_MAX_128 |
	  TG_PCI_STATE_DMA_WRITE_MAX_128 |
#if (TIGON_REV & TIGON_REV4)
	      TG_PCI_STATE_NO_READ_WORD_SWAP |
	      TG_PCI_STATE_NO_WRITE_WORD_SWAP |
#else
	      TG_PCI_STATE_NO_WORD_SWAP |
#endif
	      TG_PCI_STATE_USE_MEM_RD_MULT |
	      0x06000000 | 0x70000000;
	
	/* set pci_state reg */
	W_REG(ti->ti_regs.gen_control.pci_state, 
	       pci_state);

	/* set window base */
	W_REG(ti->ti_regs.gen_control.window_base, SEND_RING_PTR);

	W_REG(ti->ti_tuneparams.send_coal_ticks, eg_send_coal_ticks);
	W_REG(ti->ti_tuneparams.recv_coal_ticks, eg_recv_coal_ticks[ti->ti_unit]);
	W_REG(ti->ti_tuneparams.send_max_coalesced_bds, eg_send_coal_bd);
	W_REG(ti->ti_tuneparams.recv_max_coalesced_bds, eg_recv_coal_bd);

	/* make sure MBOX 0 is cleared so we can get interrupts */
	W_REG(ti->ti_shmem->mailbox[0].mbox_low, 0);

	/* set Tx buffer space to 1/4 Rx buffer space */
	W_REG(ti->ti_gencom.tx_buf_ratio,16);

	/* set byte swapping */
	W_REG(ti->ti_gencom.mode_status,  TG_CFG_MODE_WARN | 
	      TG_CFG_MODE_FATAL | TG_CFG_MODE_WORD_SWAP_BD);

	/* set the dma transfer size to 128 bytes */
	W_REG(ti->ti_gencom.dma_write_config, TG_DMA_STATE_THRESH_16W);
	W_REG(ti->ti_gencom.dma_read_config, TG_DMA_STATE_THRESH_16W);

	/* clear maskRupt field and leave it at zero */
	W_REG(ti->ti_gencom.maskRupts, 0);

	/* tell NIC about geninfo area */
	tmp = KVTOIOADDR_CMD(ti, ti->ti_geninfo);
	W_REG64(ti->ti_gencom.gen_info_ptr, tmp);

	/* set up the info for the stats */
	W_REG(ti->ti_gencom.ifIndex, ti->ti_unit);
	W_REG(ti->ti_gencom.ifMTU, ti->ti_if.if_mtu + 18);

	/* initialize some of the indices in gencom */
	W_REG(ti->ti_gencom.recv_return_cons_index, 0);
	W_REG(ti->ti_gencom.event_cons_index, 0);

	/* program the geninfo area */
	ti->ti_geninfo->event_producer_ptr = KVTOIOADDR_CMD(ti, ti->ti_evprodptr);
	ti->ti_geninfo->recv_return_producer_ptr = KVTOIOADDR_CMD(ti, ti->ti_rxrtn_prod);
	ti->ti_geninfo->send_consumer_ptr = KVTOIOADDR_CMD(ti, ti->ti_txconsptr);
	ti->ti_geninfo->event_rcb.ring_addr = KVTOIOADDR_CMD(ti, ti->ti_evring);
	ti->ti_geninfo->recv_standard_rcb.ring_addr = KVTOIOADDR_CMD(ti, ti->ti_rxring_std);
	ti->ti_geninfo->recv_jumbo_rcb.ring_addr = KVTOIOADDR_CMD(ti, ti->ti_rxring_jumbo);
        ti->ti_geninfo->stats2_ptr = KVTOIOADDR_CMD(ti, ti->ti_stats2);

	/* setup return ring */
        ti->ti_geninfo->recv_return_rcb.ring_addr = KVTOIOADDR_CMD(ti, ti->ti_rxring_rtn);
	ti->ti_geninfo->recv_return_rcb.RCB_max_len = RECV_RETURN_RING_ENTRIES;

	/* don't use mini ring */
	ti->ti_geninfo->recv_mini_rcb.ring_addr = NULL;
	ti->ti_geninfo->recv_mini_rcb.RCB_flags = RCB_FLAG_RING_DISABLED;

	/* 
	 * cmd ring is on the NIC.  Dont know why we need
	 * to initialize RCBs for these.  But, the reference code
	 * does it. 
	 */
        ti->ti_geninfo->command_rcb.ring_addr = TG_COMMAND_RING;

	/* setup send ring */
	ti->ti_geninfo->send_rcb.ring_addr = KVTOIOADDR_CMD(ti, ti->ti_txring);
	ti->ti_geninfo->send_rcb.RCB_max_len = SEND_RING_ENTRIES;

        eg_tuneparams(ti);

	return (0);
}

/*
 * Driver xxopen() routine exists only to take unit# which
 * has now been assigned to the vertex by ioconfig
 * and if_attach() the device.
 */
/* ARGSUSED */
int
if_egopen(dev_t *devp, int flag, int otyp, struct cred *crp)
{
	vertex_hdl_t enet_vhdl;
	struct eg_info *ti;
	int unit;

	enet_vhdl = dev_to_vhdl(*devp);

	if ((ti = eg_info_get(enet_vhdl)) == NULL)
		return (EIO);

	/* if already if_attached, just return */
	if (ti->ti_unit != -1)
		return (0);

	if ((unit = device_controller_num_get(enet_vhdl)) < 0) {
		cmn_err(CE_ALERT, "%v: eg_open: vertex missing ctlr number", enet_vhdl);
		return (EIO);
	}

	eg_ifattach(ti, unit);

	return (0);
}

static void
eg_ifattach(struct eg_info *ti, int unit)
{
	ti->ti_unit = unit;

	if (ti->ti_enet_vhdl != GRAPH_VERTEX_NONE) {
		char edge_name[8];
		sprintf(edge_name, "%s%d", "eg", unit);
		(void) if_hwgraph_alias_add(ti->ti_enet_vhdl, edge_name);
	}

	ether_attach(&ti->ti_eif, "eg", ti->ti_unit, (caddr_t)ti, &egops, &ti->ti_addr, INV_ETHER_GE, 0);
	ti->ti_if.if_flags |= IFF_DRVRLOCK;
	ti->ti_if.if_mtu = eg_mtu[ti->ti_unit];
#ifndef GE_FICUS
	ti->ti_if.if_baudrate.ifs_value = 250*1000*1000;
	ti->ti_if.if_baudrate.ifs_log2 = 2;
	ti->ti_if.if_sendspace = 192 * 1024;
	ti->ti_if.if_recvspace = 192 * 1024;
#endif
}


static struct mbuf *
eg_reclaim(struct eg_info *ti, struct ifqueue *qp)
{
	int	i;
	U32 txcons;

	
	/* txcons = HI32(*ti->ti_txconsptr); */

	/* bug fix */
	txcons = HI32(*ti->ti_txconsptr);

	ASSERT(ti->ti_flags & TIF_TLOCK);
	ASSERT(txcons < SEND_RING_ENTRIES);

	EG_PRINT(ti, TX, 5, ("eg_reclaim: new_txcons: %d,  old_txcons: %d\n", txcons, ti->ti_txcons_shad))


	/* free the mbuf and zap the bd */
	for (i = ti->ti_txcons_shad; i != txcons; i = INCRING(i, SEND_RING_ENTRIES)) {

		ASSERT(i != ti->ti_txprod);
		if (ti->ti_txm[i]) {
			GOODMP(ti->ti_txm[i]);
			GOODMT(ti->ti_txm[i]->m_type);
			IF_ENQUEUE_NOLOCK(qp, ti->ti_txm[i]);
			ti->ti_txm[i] = NULL;
		}
	}

	ti->ti_txcons_shad = txcons;

	return (qp->ifq_head);
}


/*
 * Wrapper for eg_init - just acquires the private driver bitlock
 * before calling eg_init().
 */
static int
eg_xinit(struct etherif *eif,
	int flags)
{
	struct eg_info *ti;
	int rc;
	int s, s2;

	for (rc=0; rc < 512; rc++)
	{
		eg_std_hist[rc]=0;
	}

	for (rc=0; rc < 256; rc++)
	{
		eg_jum_hist[rc]=0;
	}

	ti = eiftoti(eif);

	s = mutex_bitlock(&ti->ti_flags, TIF_TLOCK);
	s2 = mutex_bitlock(&ti->ti_rflags, TIF_RLOCK);
	rc = eg_init(eif, flags);
	mutex_bitunlock(&ti->ti_rflags, TIF_RLOCK, s2);
	mutex_bitunlock(&ti->ti_flags, TIF_TLOCK, s);

	return (rc);
}

/*
 * Wrapper for eg_reset - just acquires the private driver bitlock
 * before calling eg_reset().
 */
static void
eg_xreset(struct etherif *eif)
{
	struct eg_info *ti;
	int s, s2;
	
	ti = eiftoti(eif);

	s = mutex_bitlock(&ti->ti_flags, TIF_TLOCK);
	s2 = mutex_bitlock(&ti->ti_rflags, TIF_RLOCK);
	eg_reset(eif);
	mutex_bitunlock(&ti->ti_rflags, TIF_RLOCK, s2);
	mutex_bitunlock(&ti->ti_flags, TIF_TLOCK, s);
}

static int
eg_init(struct etherif *eif,
	int flags)
{
	struct eg_info *ti;
	int error;
	inventory_t *inv_ptr = NULL;
	struct etheraddr eg_ea;
#ifndef IP32
	int r, vchan0, vchan1, i;
#endif

	ti = eiftoti(eif);

	if (ti->ti_FWrefreshes == 0) {
		cmn_err(CE_WARN, "eg%d: No Firmware found", ti->ti_unit);
		return (EIO);
	}

#ifndef IP32
	if (ti->ti_gotrrbs == 0) {
		eg_checkbus(ti);
		/* Try to get as many RRBs as possible, but no fewer than 2 */
		for (i = ti->ti_nrrbs; i >= 2; i--) {
			vchan0 = i;
			vchan1 = 0;
			r = pcibr_rrb_alloc(ti->ti_conn_vhdl, &vchan0, &vchan1);
			if (r == 0) {
				break;
			}
			if ((i == 2) && r) {
				/* annoying, but not fatal */
				cmn_err(CE_ALERT, "%v: if_egattach: pcibr_rrb_alloc() failed to allocate 2 RRBs", ti->ti_enet_vhdl);
			}
		}
		if (r == 0) {
			EG_PRINT(ti, LNK, 2, 
				("eg%d: snagged %d RRBs\n", ti->ti_unit, i))
		}
		ti->ti_gotrrbs = i;
	}
#endif

#ifdef NEW_INIT

	/* if the firmware loading and initialization has been
	 * done before,  just do the relevant stuff according
	 * to the flags passed,  instead of restarting from
	 * scratch everytime.
	 */

	if (!ti->ti_tigonFwText || !ti->ti_tigonFwData || !ti->ti_tigonFwRodata) {

		if (flags & IFF_PROMISC) {
			if (error = eg_docmd(ti, TG_CMD_SET_PROMISC_MODE, 0, TG_CMD_CODE_PROMISC_ENABLE))
			return (error);
		} else {
			if (error = eg_docmd(ti, TG_CMD_SET_PROMISC_MODE, 0, TG_CMD_CODE_PROMISC_DISABLE))
			return (error);
		}
		if (flags & IFF_ALLMULTI) {
			if (error = eg_docmd(ti, TG_CMD_SET_MULTICAST_MODE, 0, TG_CMD_CODE_MULTICAST_ENABLE))
			return (error);
		} else {
			if (error = eg_docmd(ti, TG_CMD_SET_MULTICAST_MODE, 0, TG_CMD_CODE_MULTICAST_DISABLE))
			return (error);
		}
		if (!(flags & IFF_RUNNING)) {
			/* enable stack to start receiving packets */
			if (error = eg_docmd(ti, TG_CMD_HOST_STATE, 0, TG_CMD_CODE_STACK_UP))
				return (error);

			/* turn on watchdog */
			ti->ti_if.if_timer = 1;
		}

		return (0);
	}

#endif /* NEW_INIT */
	
	/* rest of the code is executed the first time system is up
	 * or everytime new firmware is loaded
	 */

	/* stop FW */
	eg_stopFW(ti);

	/* free old mbufs */
	eg_free(ti);

	/* clear out indices and rings */
	ti->ti_txprod = ti->ti_txcons_shad = 0;
	ti->ti_rxstd_prod = ti->ti_rxstd_cons = 0;
	ti->ti_rxrtn_cons_real = ti->ti_rxrtn_cons_shad = 0;
	ti->ti_rxjumbo_prod = ti->ti_rxjumbo_cons = 0;
	ti->ti_cmdprod = ti->ti_cmdcons_shad = 0;
	W_REG(ti->ti_cmdcons, 0);
	W_REG(ti->ti_shmem->command_prod_index, 0);
	*(ti->ti_evprodptr) = ti->ti_evcons = 0;
	*(ti->ti_rxrtn_prod) = 0;
	*(ti->ti_txconsptr) = 0;
	W_REG(ti->ti_shmemevcons, 0);
	W_REG(ti->ti_shmem->sendProdIndex, 0);

	bzero((caddr_t) ti->ti_rxring_std, sizeof(struct recv_bd) * RECV_STANDARD_RING_ENTRIES);
	bzero((caddr_t) ti->ti_rxring_jumbo, sizeof(struct recv_bd) * RECV_JUMBO_RING_ENTRIES);
	bzero((caddr_t) ti->ti_rxring_rtn, sizeof(struct recv_bd) * RECV_RETURN_RING_ENTRIES);
	bzero((caddr_t) ti->ti_evring, sizeof(struct tg_event) * EVENT_RING_ENTRIES);
	
	/* zero out geninfo area */
	bzero((caddr_t) ti->ti_geninfo, sizeof(struct tg_gen_info));

	/* initialize the registers, shared memory etc */
	eg_initialize(ti);

	eg_getmacaddr(ti, &eg_ea);

   	ti->ti_flags |= TIF_CKSUM_RX + TIF_CKSUM_TX;

	if (ti->ti_flags & TIF_CKSUM_TX) {
		ti->ti_if.if_flags |= IFF_CKSUM;
		ti->ti_geninfo->send_rcb.RCB_flags = RCB_FLAG_TCP_UDP_CKSUM | RCB_FLAG_HOST_RING; 
	} else {
		ti->ti_if.if_flags &= ~IFF_CKSUM;
		ti->ti_geninfo->send_rcb.RCB_flags = RCB_FLAG_HOST_RING; 
	}
	
	if (ti->ti_flags & TIF_CKSUM_RX) {
		ti->ti_geninfo->recv_standard_rcb.RCB_flags = RCB_FLAG_TCP_UDP_CKSUM;

		ti->ti_geninfo->recv_jumbo_rcb.RCB_flags = RCB_FLAG_TCP_UDP_CKSUM;

		ti->ti_geninfo->recv_return_rcb.RCB_flags = RCB_FLAG_TCP_UDP_CKSUM;

	}

	ti->ti_geninfo->recv_standard_rcb.RCB_max_len = MAXBLOCKSIZE_STD;
	ti->ti_geninfo->recv_jumbo_rcb.RCB_max_len = MAXBLOCKSIZE_JMB;

	/* set ifnet snd queue max size */
	ti->ti_if.if_snd.ifq_maxlen = eg_ifqlen;

	/* load and start FW */
	if (error = eg_startcard(ti))
		return (error);

	/* NIC is up now */
	ti->ti_flags &= ~TIF_FWLOADING;

	/* update the hinv database */
	if (!(inv_ptr = find_inventory(inv_ptr, INV_NETWORK, INV_NET_ETHER,
			       INV_ETHER_GE, ti->ti_unit, -1))) {
		device_inventory_add(ti->ti_enet_vhdl, INV_NETWORK, 
				     INV_NET_ETHER, INV_ETHER_GE, 
				     ti->ti_unit, ti->ti_rev);
	} else {
		replace_in_inventory(inv_ptr, INV_NETWORK, INV_NET_ETHER, 
				     INV_ETHER_GE, ti->ti_unit, ti->ti_rev);
	}

	/* post receive buffers */
	eg_fill (ti);

	DBG_PRINT (("ti addr = 0x%x, ring1 addr = 0x%x, ring jumbo addr = 0x%x, mbuff1 addr = 0x%x, mbuff jmb addr = 0x%x\n",
		ti, ti->ti_rxring_std, ti->ti_rxring_jumbo, ti->ti_rxm_std, ti->ti_rxm_jumbo) );

	/* XXX: for Tigon-1, send explicit cmd */
	/* XXX ted: for tigon-2? */
	if (error = eg_docmd(ti, TG_CMD_LINK_NEGOTIATION, 0, 0))
		return (error);

#if 0
	/* XXX SCA; removed this code since it was blowing the stats buffer
	 * The right thing to do might be to re-write the stats buffer after
	 * this, but I'm not sure.
	 */
	/* reload stats from last time */
	if (error = eg_docmd(ti, TG_CMD_REFRESH_STATS, 0, 0))
		return (error);
#endif

	/* set promisc */
	if (flags & IFF_PROMISC) {
		if (error = eg_docmd(ti, TG_CMD_SET_PROMISC_MODE, 0, TG_CMD_CODE_PROMISC_ENABLE))
		return (error);
	} else {
		if (error = eg_docmd(ti, TG_CMD_SET_PROMISC_MODE, 0, TG_CMD_CODE_PROMISC_DISABLE))
		return (error);
	}

	if (flags & IFF_ALLMULTI) {
		if (error = eg_docmd(ti, TG_CMD_SET_MULTICAST_MODE, 0, TG_CMD_CODE_MULTICAST_ENABLE))
		return (error);
	} else {
		if (error = eg_docmd(ti, TG_CMD_SET_MULTICAST_MODE, 0, TG_CMD_CODE_MULTICAST_DISABLE))
		return (error);
	}

	/* enable stack to start receiving packets */
	if (error = eg_docmd(ti, TG_CMD_HOST_STATE, 0, TG_CMD_CODE_STACK_UP))
		return (error);

/* XXX:  bug??? check with alteon */
	ti->ti_shmem->mailbox[0].mbox_low = 0;

	/* turn on watchdog */
	ti->ti_if.if_timer = 1;

#ifndef GE_FICUS
	/* register this driver with RSVP packet scheduler */
	eg_ps_register(ti, 0);
#endif /* GE_FICUS */

	EG_PRINT(ti, LNK, 2, ("eg%d: full init done\n", ti->ti_unit))

	return (0);
}

/*
 * Reset the interface, leaving it disabled.
 */
static void
eg_reset(struct etherif *eif)
{
	struct eg_info *ti;

	ti = eiftoti(eif);

	ASSERT(ti->ti_flags & TIF_TLOCK);

	/* stop watchdog */
	eiftoifp(eif)->if_timer = 0;

	/* snarf stats before resetting chip  */
	/* XXX ted:  might have to clear out some parts? */
	bcopy((caddr_t)&ti->ti_geninfo->stats, 
	      (caddr_t)ti->ti_stats2, 
	      sizeof(struct tg_stats));

	/* do stack_down first.  If that doesnt work, stop FW */
	if (eg_docmd(ti, TG_CMD_HOST_STATE, 0, TG_CMD_CODE_STACK_DOWN))
		eg_stopFW(ti);

#ifndef NEW_INIT
	eg_free(ti);
#endif

	if (eg_hardreset)
		eg_resetcard(ti);
}
 
static void
eg_free(struct eg_info *ti)
{
	int i;

	/* free any pending transmit mbufs */
	for (i = 0; i < SEND_RING_ENTRIES; i++)
		if (ti->ti_txm[i]) {
			m_freem(ti->ti_txm[i]);
			ti->ti_txm[i] = NULL;
		}

	/* free the posted rbufs */
	for (i = 0; i < RECV_STANDARD_RING_ENTRIES; i++) {
		if (ti->ti_rxm_std[i]) {
			m_freem(ti->ti_rxm_std[i]);
			ti->ti_rxm_std[i] = NULL;
		}
	}

	for (i = 0; i < RECV_JUMBO_RING_ENTRIES; i++) {
		if (ti->ti_rxm_jumbo[i]) {
			m_freem(ti->ti_rxm_jumbo[i]);
			ti->ti_rxm_jumbo[i] = NULL;
		}
	}
}

/*
 *	Guts of eg_fill (separated to support jumbo frames)
 */
static int
eg_fill_buffs ( struct eg_info *ti, struct mbuf **pool, struct recv_bd *ring, int prod, int n,
		int mbsize, int mdsize, int mod)
{
	volatile struct egrxbuf *rb;
	struct mbuf *m;
	int	i;

	for (i = 0; i < n; i++) {
	        if ((m = m_vget(M_DONTWAIT, mbsize, MT_DATA)) == NULL) {
			DBG_PRINT(("eg%d: eg_fill: out of mbufs\n", ti->ti_unit));
			ti->ti_nombufs++;

			break;
		}
		ASSERT(ALIGNED(mtod(m, caddr_t), 4));

		rb = mtod(m, struct egrxbuf*);

		ASSERT(pool[prod] == NULL);

		pool[prod] = m;
		
		ASSERT(ring[prod].BD_host_addr == 0);

		ring[prod].BD_host_addr = KVTOIOADDR_DATA(ti, (char *)rb + EHDROFFSET);
		ring[prod].BD_index = prod;
		ring[prod].BD_len = mdsize + ETHER_HDRLEN;
		ring[prod].BD_flags = 0;

#ifdef IP32
		/*
		 * Invalidate cache, which on R10K de-maps the page to avoid
		 * speculative stores corrupting it.  The page will be
		 * re-validated after DMA completes.
		 */
		__vm_dcache_inval(mtod(m, caddr_t), mbsize);
#else
		/*
		 * For Heart: this actually does a writeback and the code
		 * depends on it
		 */
		CACHE_INVAL((void *)rb, sizeof(__uint32_t) * 2);
		CACHE_WB(mtod(m, caddr_t), mbsize);
#endif

		prod = INCRING(prod, mod);
	}

	return (prod);
}

/*
 * Allocate and post receive buffers.
 */
static void
eg_fill_std(struct eg_info *ti)
{
	int	prod, cons;
	int	n;

	ASSERT(ti->ti_rflags & TIF_RLOCK);

	prod = ti->ti_rxstd_prod;
	cons = ti->ti_rxstd_cons;

	n = (RECV_STANDARD_RING_ENTRIES-1) - DELTARXD(prod, cons);

	eg_std_hist[n]++;

	if (n < eg_fill_thresh) return;

	EG_PRINT(ti, RX, 6, ("eg_fill: prod=%d, cons=%d, n= %d\n",prod, cons, n))


	ti->ti_rxstd_prod = eg_fill_buffs (ti, ti->ti_rxm_std, ti->ti_rxring_std, prod, n, 
					   BUF_SIZE_STD, EG_RDATA_SZ_STD, RECV_STANDARD_RING_ENTRIES);

	(void) eg_docmd(ti, TG_CMD_SET_RECV_PRODUCER_INDEX, ti->ti_rxstd_prod, 0);
}

static void
eg_fill_jmb(struct eg_info *ti)
{
	int	prod, cons;
	int	n;

	ASSERT(ti->ti_rflags & TIF_RLOCK);

	prod = ti->ti_rxjumbo_prod;
	cons = ti->ti_rxjumbo_cons;

	n = (RECV_JUMBO_RING_ENTRIES-1) - DELTARXJUMBO(prod, cons);

	eg_jum_hist[n]++;

	if (n < eg_fill_thresh) return;

	EG_PRINT(ti, RX, 6, ("eg_fill: prod=%d, cons=%d, n= %d\n",prod, cons, n))


	ti->ti_rxjumbo_prod = eg_fill_buffs (ti, ti->ti_rxm_jumbo, ti->ti_rxring_jumbo, prod, n,
					     BUF_SIZE_JMB, EG_RDATA_SZ_JMB, RECV_JUMBO_RING_ENTRIES);

	(void) eg_docmd(ti, TG_CMD_SET_RECV_JUMBO_PRODUCER_INDEX, ti->ti_rxjumbo_prod, 0);
	
}

/*
 * Allocate and post receive buffers.
 */
static void
eg_fill(struct eg_info *ti)
{
	eg_fill_std (ti);
	
	eg_fill_jmb (ti);
}

static int
eg_ioctl(struct etherif *eif,
	int cmd,
	void *data)
{
	struct eg_info *ti;
	struct mfreq *mfr;
	struct eg_treq *tfr;
	struct eg_dreq *dfr;
	struct eg_rreq *rfr;
	struct eg_mreq *memfr;
	struct eg_fwreq *egFw;
	struct ifnet *ifp;
	struct ifreq *ifr;
	union mkey *key;
	U32 macspot[2];
	int rc;
	int s;
	static U32 traceBuf[ALT_TRACE_SIZE*ALT_TRACE_ELEM_SIZE];
	U32 nicTb, curNicTb, endNicTb;
	char *tracePtr;
	char *membuf;

	macspot[0] = 0;
	
	rc = 0;
	ti = eiftoti(eif);
	ifp = eiftoifp(eif);

	ASSERT(IFNET_ISLOCKED(&ti->ti_if));

	s = mutex_bitlock(&ti->ti_flags, TIF_TLOCK);

	mfr = (struct mfreq*) data;
	key = mfr->mfr_key;


	switch (cmd) {
	case SIOCADDMULTI:
		mfr = (struct mfreq*) data;
		key = mfr->mfr_key;
		mfr->mfr_value = CalcIndex(key->mk_dhost, sizeof(key->mk_dhost));
		/* only one SIOCADDMULTI at a time because this
		 * lock protected by IFNET lock
		 */
		ASSERT(!(ti->ti_flags & TIF_MULTI_ACTIVE));

		if (ti->ti_nmulti+1 >= MAX_MCAST_ENTRIES) {
		    rc = EINVAL;
		    break;
		}

		bcopy(key->mk_dhost, (char *)macspot+2, 6);
		eg_memcpy((caddr_t)&ti->ti_gencom.mcast_xfer_buf, (caddr_t)macspot, 8);

		if (eg_docmd(ti, TG_CMD_ADD_MULTICAST_ADDR, 0, 0)) { 
			ti->ti_flags &= ~TIF_MULTI_ACTIVE;
			rc = EINVAL;
			break;
		}

		if (!(ifp->if_flags & IFF_UP)) {
			/* won't get an ack, so just finish up */
			goto finadd;
		}

	        ti->ti_flags |= TIF_MULTI_ACTIVE;

		/* wait for the acknowlegement to come back */
		ti->ti_waittime = lbolt;
#ifdef GE_FICUS
		if (!curthreadp) {
			/* no thread; bail out */
			ti->ti_flags &= ~TIF_MULTI_ACTIVE;
			ti->ti_waittime = 0;
			goto finadd;
		}
		if (!KT_ISUPROC(curthreadp)) {
			sv_bitlock_wait(&ti->ti_sv, PZERO, &ti->ti_flags, 
				TIF_TLOCK, s);
		} else 
#endif
		if (sv_bitlock_wait_sig(&ti->ti_sv, PZERO, &ti->ti_flags, TIF_TLOCK, s) == -1) {
			s = mutex_bitlock(&ti->ti_flags, TIF_TLOCK);
			ti->ti_flags &= ~TIF_MULTI_ACTIVE;
			ti->ti_waittime = 0;
			rc = EINTR;
			break;
		}

		s = mutex_bitlock(&ti->ti_flags, TIF_TLOCK);
		ti->ti_waittime = 0;

finadd:
		if (ti->ti_flags & TIF_MULTI_ACTIVE) {
			/* firmware did not send an ACK.
			 * watchdog woke us up instead */
			ti->ti_flags &= ~TIF_MULTI_ACTIVE;
			rc = EINVAL;
		}
		if (ti->ti_nmulti == 0)
			ti->ti_if.if_flags |= IFF_FILTMULTI;
		ti->ti_nmulti++;

		break;

	case SIOCDELMULTI:
		mfr = (struct mfreq*) data;
		key = mfr->mfr_key;
		if (mfr->mfr_value
                    != CalcIndex(key->mk_dhost, sizeof(key->mk_dhost))) {
			rc = EINVAL;
                        break;
		}
                if (mfhasvalue(&eif->eif_mfilter, mfr->mfr_value))
                        break;

		/* only one SIOCADDMULTI at a time because this
		 * lock protected by IFNET lock
		 */
		ASSERT(!(ti->ti_flags & TIF_MULTI_ACTIVE));

		bcopy(key->mk_dhost, (char *)macspot+2, 6);
		eg_memcpy((caddr_t)&ti->ti_gencom.mcast_xfer_buf, (caddr_t)macspot, 8);
		if (eg_docmd(ti, TG_CMD_DEL_MULTICAST_ADDR, 0, 0)) {
			ti->ti_flags &= ~TIF_MULTI_ACTIVE;
			rc = EINVAL;
			break;
		}

		if (!(ifp->if_flags & IFF_UP)) {
			/* won't get an ack, so just finish up */
			goto findel;
		}

	        ti->ti_flags |= TIF_MULTI_ACTIVE;

		/* wait for the acknowlegement to come back */
		ti->ti_waittime = lbolt;
#ifdef GE_FICUS
		if (!curthreadp) {
			/* no thread; bail out */
			ti->ti_flags &= ~TIF_MULTI_ACTIVE;
			ti->ti_waittime = 0;
			goto findel;
		}
		if (!KT_ISUPROC(curthreadp)) {
			sv_bitlock_wait(&ti->ti_sv, PZERO, &ti->ti_flags, 
				TIF_TLOCK, s);
		} else 
#endif
		if (sv_bitlock_wait_sig(&ti->ti_sv, PZERO, &ti->ti_flags, TIF_TLOCK, s) == -1) {
			s = mutex_bitlock(&ti->ti_flags, TIF_TLOCK);
			ti->ti_flags &= ~TIF_MULTI_ACTIVE;
			ti->ti_waittime = 0;
			rc = EINTR;
			break;
		}

		s = mutex_bitlock(&ti->ti_flags, TIF_TLOCK);
		ti->ti_waittime = 0;

findel:
		if (ti->ti_flags & TIF_MULTI_ACTIVE) {
			/* firmware did not send an ACK.
			 * watchdog woke us up instead */
			ti->ti_flags &= ~TIF_MULTI_ACTIVE;
			rc = EINVAL;
		}

		ti->ti_nmulti--;
		if (ti->ti_nmulti == 0)
			ti->ti_if.if_flags &= ~IFF_FILTMULTI;
		break;

	case ALT_SETDEBUG:
	        dfr = (struct eg_dreq *)data;
		ti->ti_dbg_mask = dfr->debug_mask;
		ti->ti_dbg_level = dfr->debug_level;
		break;

	case ALT_SETTRACE:
	        tfr = (struct eg_treq *)data;
		eg_trace = tfr->cmd;
		W_REG(ti->ti_tuneparams.tracing, tfr->cmd);
		break;

	case ALT_NO_LINK_NEG:
		ifr = (struct ifreq *)data;
		if ((ifr->ifr_metric < 0) || (ifr->ifr_metric > 1)) {
			rc = EINVAL;
			break;
		}
		eg_autoneg_array[ti->ti_unit] = ifr->ifr_metric;
		break;

	case ALT_NUM_RRBS:
	        ifr = (struct ifreq *)data;
		if ((ifr->ifr_metric < 2) || (ifr->ifr_metric > 6)) {
			rc = EINVAL;
			break;
		}
		ti->ti_nrrbs = ifr->ifr_metric;
		break;

	case ALT_GETNTRACE:
	        tfr = (struct eg_treq *)data;

		/* nic_trace_index points into a 4k page, aligned, get pieces */
		curNicTb = ti->ti_gencom.nic_trace_ptr;
		nicTb = ti->ti_gencom.nic_trace_start;
		endNicTb = nicTb + ti->ti_gencom.nic_trace_len;
		tracePtr = (char *)traceBuf;

		eg_readshmem(ti, 
			     curNicTb + (ALT_TRACE_ELEM_SIZE * sizeof(U32)),
			     tracePtr, 
			     endNicTb - curNicTb - (ALT_TRACE_ELEM_SIZE * sizeof(U32)));

		eg_readshmem(ti, nicTb,
                      (tracePtr - ALT_TRACE_ELEM_SIZE * sizeof(U32)) + endNicTb - curNicTb,
                      curNicTb - nicTb);
		if (copyout((caddr_t)tracePtr, (caddr_t)tfr->tbufp, 
			ALT_TRACE_SIZE * ALT_TRACE_ELEM_SIZE * sizeof(U32))) {
			rc = EFAULT;
		}

		break;


	case ALT_GET_NIC_STATS:
	        tfr = (struct eg_treq *)data;
		if (copyout((caddr_t)&ti->ti_geninfo->stats, 
			(caddr_t)tfr->tbufp, sizeof(struct tg_stats))) {
			rc = EFAULT;
		}

		break;

	case ALT_CLEARPROFILE:
	        if (eg_docmd(ti, TG_CMD_CLEAR_PROFILE, 0, 0))
		   rc = EIO;

		break;

	case ALT_CLEARSTATS:
	        if (eg_docmd(ti, TG_CMD_CLEAR_STATS, 0, 0))
		   rc = EIO;

		break;

	case ALT_SETPROMISC:
	        if (eg_docmd(ti,  TG_CMD_SET_PROMISC_MODE, 0, TG_CMD_CODE_PROMISC_ENABLE))
		   rc = EIO;

		break;

	case ALT_UNSETPROMISC:
	        if (eg_docmd(ti,  TG_CMD_SET_PROMISC_MODE, 0, TG_CMD_CODE_PROMISC_DISABLE))
		   rc = EIO;

		break;

	case ALT_CACHE_FW:
	        egFw = (struct eg_fwreq *)data;

		/* Verify firmware level	*/
		if (egFw->releaseMajor != 12) {
			printf ("egconfig driver mismatch, driver needs rev 12, egconfig is rev %d\n",
				egFw->releaseMajor);

			rc = EBADF;
			break;
		}

		/* free old cached copy */
		if (ti->ti_tigonFwText || ti->ti_tigonFwData || ti->ti_tigonFwRodata)
			eg_freeFW(ti);

		/* allocate memory for text and data and copyin */
		ti->ti_tigonFwText = (U32 *)kmem_zalloc(egFw->textLen, KM_SLEEP);
		ti->ti_tigonFwData = (U32 *)kmem_zalloc(egFw->dataLen, KM_SLEEP);
		ti->ti_tigonFwRodata = (U32 *)kmem_zalloc(egFw->rodataLen, KM_SLEEP);

		if (!ti->ti_tigonFwText || !ti->ti_tigonFwData || !ti->ti_tigonFwRodata) {
			eg_freeFW(ti);
			rc = ENOMEM;
			break;
		}

		if (copyin((caddr_t)egFw->fwText, ti->ti_tigonFwText, egFw->textLen))
			rc = EFAULT;
		if (copyin((caddr_t)egFw->fwData, ti->ti_tigonFwData, egFw->dataLen))
			rc = EFAULT;
		if (copyin((caddr_t)egFw->fwRodata, ti->ti_tigonFwRodata, egFw->rodataLen))
			rc = EFAULT;

		ti->ti_rev = egFw->releaseMajor*10000 + 
				egFw->releaseMinor * 100 +
				egFw->releaseFix;
		ti->ti_tigonFwStartAddr = egFw->startAddr;
		ti->ti_tigonFwTextAddr = egFw->textAddr;
		ti->ti_tigonFwTextLen = egFw->textLen;
		ti->ti_tigonFwDataAddr = egFw->dataAddr;
		ti->ti_tigonFwDataLen = egFw->dataLen;
		ti->ti_tigonFwRodataAddr = egFw->rodataAddr;
		ti->ti_tigonFwRodataLen = egFw->rodataLen;
		ti->ti_tigonFwBssAddr = egFw->bssAddr;
		ti->ti_tigonFwBssLen = egFw->bssLen;
		ti->ti_tigonFwSbssAddr = egFw->sbssAddr;
		ti->ti_tigonFwSbssLen = egFw->sbssLen;

		if (rc)
			eg_freeFW(ti);

		ti->ti_FWrefreshes++;

		break;
	case ALT_READMEM:
		memfr = (struct eg_mreq *)data;
		if (!(membuf = (char *)kmem_zalloc(memfr->len, KM_SLEEP))) {
			rc = ENOMEM;
			break;
		}

		if ( memfr->egAddr + memfr->len <= TG_END_SRAM) {
			eg_readshmem(ti, memfr->egAddr, (caddr_t)membuf, memfr->len);
			copyout((caddr_t)membuf, (caddr_t)memfr->userAddr, memfr->len);
		} else
			rc = EINVAL;

		kmem_free((caddr_t)membuf, memfr->len);
		break;

	case ALT_READ_EG_REG:
	        rfr = (struct eg_rreq *)data;
		if (rfr->addr > TG_MAX_REG) {
			rc = EINVAL;
			break;
		}
		rfr->data = R_REGp( (char *)ti->ti_shmem + rfr->addr);
		break;

	case ALT_WRITE_EG_REG:
	        rfr = (struct eg_rreq *)data;
		if (rfr->addr > TG_MAX_REG) {
			rc = EINVAL;
			break;
		}
		W_REGp( (char *)ti->ti_shmem + rfr->addr, rfr->data);
		break;

	default:
	        rc = EINVAL;
		break;
	}

	mutex_bitunlock(&ti->ti_flags, TIF_TLOCK, s);

	return (rc);
}


/* process the event ring */
/* ARGSUSED */
static void
#ifdef IP32
eg_intr(eframe_t *ef, struct eg_info *ti)
#else
eg_intr(struct eg_info *ti)
#endif
{
	struct mbuf *mact, *mf, *m;
	U32 evcons;
	U32 evprod;
	struct tg_event eventinfo;
	U32 event, code;
	struct ifqueue qtot, qrecl;
	int s, s2;
	int new_event;
	int reclaimed;

	s2 = mutex_bitlock(&ti->ti_flags, TIF_TLOCK);
	/* take TLOCK to check FWLOADING flag safely */
	if (iff_dead(ti->ti_if.if_flags) || (ti->ti_flags & TIF_FWLOADING)) {
		mutex_bitunlock(&ti->ti_flags, TIF_TLOCK, s2);
		return;
	}
	mutex_bitunlock(&ti->ti_flags, TIF_TLOCK, s2);

tryagain:

	new_event = 0;
	reclaimed = 0;
	mf = NULL;
	mact = NULL;

	qrecl.ifq_head = qrecl.ifq_tail = NULL;
	qrecl.ifq_len = 0;

	qtot.ifq_head =  qtot.ifq_tail = NULL;
	qtot.ifq_len = 0;


	s2 = mutex_bitlock(&ti->ti_rflags, TIF_RLOCK);

	/* first check for received packets */
	if (HI32(*ti->ti_rxrtn_prod) != ti->ti_rxrtn_cons_real) {
		eg_recv(ti, HI32(*ti->ti_rxrtn_prod), &qtot);
	}		
	
	mutex_bitunlock(&ti->ti_rflags, TIF_RLOCK, s2);

	s = mutex_bitlock(&ti->ti_flags, TIF_TLOCK);

	/* reclaim transmitted mbufs if there are enough to be reclaimed */
	if (DELTATXD(HI32(*ti->ti_txconsptr), ti->ti_txcons_shad) > eg_recl_thresh)
		eg_reclaim(ti, &qrecl);
	
	evcons = ti->ti_evcons;

/* XXX: remove this definitely,  it has PIO reads */
	EG_PRINT(ti, INTR, 9, ("eg_intr: evcons: %d, ti_shmemevcons: %d, cmdcons_shad: %d, ti_cmdcons: %d\n", evcons, ti->ti_shmemevcons, ti->ti_cmdcons_shad, ti->ti_cmdcons))

	evprod = HI32(*ti->ti_evprodptr);

	if (evprod != evcons) {
		new_event = 1;
		EG_PRINT(ti, INTR, 8, ("eg_intr: evcons:%d, evprod:%d\n", evcons, evprod))
	}

	/*
	 * process all the events
	 */
	for ( ; evcons != evprod; evcons = INCRING(evcons, EVENT_RING_ENTRIES)) {
		eventinfo.TG_EVENT_w0 = ti->ti_evring[evcons].TG_EVENT_w0;
		event = eventinfo.TG_EVENT_event;
		code = eventinfo.TG_EVENT_code;

#if DEBUG_BEEF
     ti->ti_evring[evcons].TG_EVENT_w0 = 0xdeadbeef;
#endif

		switch (event) {

			case TG_EVENT_LINK_STATE_CHANGED:

			    eg_linkstate(ti, code);
			    break;

			case TG_EVENT_ERROR:
			    cmn_err(CE_WARN, "eg%d: error event %d", ti->ti_unit, code);
			    break;
#if 0			    
		        case TG_EVENT_CODE_ERR_UNIMP_CMD:
			    cmn_err(CE_WARN, "eg%d: unimplemented cmd error", ti->ti_unit);
			    break;
#endif			
			case TG_EVENT_MULTICAST_LIST_UPDATED:
			    ti->ti_flags &= ~TIF_MULTI_ACTIVE;
			    
			    /* wake up the waiting ioctl */
			    sv_signal(&ti->ti_sv);

			    break;

			case TG_EVENT_STATS_UPDATED:
			    EG_PRINT(ti, INTR, 4, ("eg_intr:TG_EVENT_STATS_UPDATED\n"))
			    eg_errupdate(ti);
			    break;

			case TG_EVENT_FIRMWARE_OPERATIONAL:
			    /* XXX should never get this here */
			    ASSERT(0);
			    break;

			default:
			    cmn_err(CE_WARN, "eg%d: Unknown event: 0x%x, evcons:%d\n", 
				ti->ti_unit, eventinfo, evcons);
			    break;
		}
		/* rags: get latest evprod here */
		evprod = HI32(*ti->ti_evprodptr);
	}

	if (new_event) {
		/* update my event_consumer_index */
		ti->ti_evcons = evcons;

		/* update the NIC event_consumer_index */
		W_REG(ti->ti_shmemevcons, evcons);
	}
	
	/* unmask interrupt generation */
	ti->ti_shmem->mailbox[0].mbox_low = 0;


	mutex_bitunlock(&ti->ti_flags, TIF_TLOCK, s);

#ifndef GE_FICUS
	/*
	 * If packet scheduling/RSVP is enabled, update the packet scheduler.
	 * Get the most accurate picture of the number of free txd's in the
	 * transmit ring by looking at txconsptr.
	 */
	if (ti->ti_flags & TIF_PSENABLED)
	     ps_txq_stat(eiftoifp(&ti->ti_eif),
	      SEND_RING_ENTRIES - DELTATXD(ti->ti_txprod, HI32(*ti->ti_txconsptr)));
#endif  /* GE_FICUS */

	/* send up any received packets */
	mact = qtot.ifq_head;
        while (mact) {
                m = mact;
                mact = mact->m_act;
#ifdef GE_FICUS
		ether_input(&ti->ti_eif, 0, m);
#else
                ether_input(&ti->ti_eif, mact ? SN_MORETOCOME : 0, m); 
		/* ether_input(&ti->ti_eif, 0, m); */
#endif
        }

	/* free completed trasmit mbuf chains */
	mf = qrecl.ifq_head;
	while (mf) {
		reclaimed++;
		m = mf;
		mf = mf->m_act;
		m_freem(m);  
	}
	if (reclaimed && ti->ti_if.if_snd.ifq_len) {
		struct etheraddr nuladdr;
		eg_transmit(&ti->ti_eif, &nuladdr, &nuladdr, 0, 
			(struct mbuf *)0);
	}

	if (HI32(*ti->ti_rxrtn_prod) != ti->ti_rxrtn_cons_real)
		goto tryagain;
}


static void
eg_errupdate(struct eg_info *ti)
{
	struct tg_stats *statp = &ti->ti_geninfo->stats;
	ti->ti_if.if_ierrors = statp->ifInErrors;

	/* XXX: if jumbo is enabled, undo this */
	ti->ti_giant = statp->dot3StatsFrameTooLongs;
		
	/* XXX: anything else */
}


static void
eg_linkstate(struct eg_info *ti, U32 code)
{
	ASSERT(ti->ti_flags & TIF_TLOCK);

	EG_PRINT(ti, LNK, 1, ("TG_EVENT_LINK_STATE_CHANGED: "))

	switch (code) {
	case TG_EVENT_CODE_LINK_UP:
		EG_PRINT(ti, LNK, 1, ("UP\n"))
		if ((ti->ti_flags & TIF_LINKDOWN)) {

			cmn_err(CE_NOTE, "eg%d: link up", ti->ti_unit);
			ti->ti_flags &= ~TIF_LINKDOWN;
		}
#ifndef GE_FICUS
		if (ti->ti_gencom.linkState & LNK_FULL_DUPLEX) {
			ti->ti_if.if_flags |= IFF_LINK0;
		} else {
			ti->ti_if.if_flags &= ~IFF_LINK0;
		}
#endif
		break;
		
	case TG_EVENT_CODE_LINK_DOWN:
		EG_PRINT(ti, LNK, 1, ("DOWN\n"))
		if ((ti->ti_flags & TIF_LINKDOWN) == 0) {
			cmn_err(CE_WARN, "eg%d: link down", ti->ti_unit);
			ti->ti_flags |= TIF_LINKDOWN;
		}
		break;
		
	default:
		ASSERT(0);
		break;
	}
}


static struct ifqueue *
eg_recv(struct eg_info *ti, int rxrtn_prod, struct ifqueue *qp)
{
	struct egrxbuf *rb;
	struct recv_bd *rbd;
	struct mbuf *m, **mp;
	struct ether_header *eh;
	struct ip *ip;
	int snoopflags;
	int unit;
	int cons;
	int rlen;
	int index;
	U16 rflags;
	int i;
	int more_errpackets;

#ifdef IP32
	int mblen;
#endif

	ASSERT(ti->ti_rflags & TIF_RLOCK);

	ASSERT(rxrtn_prod < RECV_RETURN_RING_ENTRIES);

	EG_PRINT(ti, RX, 5, ("eg_recv: start: rxrtn_prod:%d rxrtn_cons_real:%d\n", rxrtn_prod, ti->ti_rxrtn_cons_real))

	more_errpackets = 0;

	cons = ti->ti_rxrtn_cons_real;

	unit = ti->ti_unit;

	/*
	 * Process all the received packets.
	 */
	for (i = cons; i != rxrtn_prod; i = INCRING(i, RECV_RETURN_RING_ENTRIES)){

		/* get latest value of rxrtn_prod everytime in the loop */
		rxrtn_prod = HI32(*ti->ti_rxrtn_prod);

		rbd = &ti->ti_rxring_rtn[i];

		rlen = rbd->BD_len;
		rflags = rbd->BD_flags;
		index = rbd->BD_index;

		EG_PRINT(ti, RX, 9, ("eg_recv: i %d rlen %d rflags 0x%x index %d\n", i, rlen, rflags, index))

		ASSERT(rlen != 0);

		if (rbd->BD_host_addr == ti->ti_rxring_jumbo[index].BD_host_addr) {

			ASSERT(index == ti->ti_rxring_jumbo[index].BD_index);
			ASSERT(index == ti->ti_rxjumbo_cons);
			ti->ti_rxring_jumbo[index].BD_host_addr = 0;
			ti->ti_rxring_jumbo[index].BD_flags = 0;
			ti->ti_rxring_jumbo[index].BD_len = 0;

			m = ti->ti_rxm_jumbo[index];
			mp = &(ti->ti_rxm_jumbo[index]);

			ti->ti_rxjumbo_cons = INCRING(index, RECV_JUMBO_RING_ENTRIES);
#ifdef IP32
			mblen = BUF_SIZE_JMB;
#endif
		}
		else {
			ASSERT(rbd->BD_host_addr == ti->ti_rxring_std[index].BD_host_addr);
			ASSERT(index == ti->ti_rxring_std[index].BD_index);
			ASSERT(index == ti->ti_rxstd_cons);
			ti->ti_rxring_std[index].BD_host_addr = 0;
			ti->ti_rxring_std[index].BD_flags = 0;
			ti->ti_rxring_std[index].BD_len = 0;

			m = ti->ti_rxm_std[index];
			mp = &(ti->ti_rxm_std[index]);

			ti->ti_rxstd_cons = INCRING(index, RECV_STANDARD_RING_ENTRIES);
#ifdef IP32
			mblen = BUF_SIZE_STD;
#endif
		}

#ifdef IP32
		/*
		 * R10K workaround: re-validate page
		 */
		xdki_dcache_validate(mtod(m, void *), mblen);
#endif
		GOODMP(m);
		GOODMT(m->m_type);

		CACHE_INVAL(m, MSIZE);
		rb = mtod(m, struct egrxbuf*);

		ASSERT(rbd->BD_host_addr == KVTOIOADDR_DATA(ti, (char *)rb+EHDROFFSET));

		/* clear the bd */
		rbd->BD_flags = rbd->BD_len = 0;

		*mp = NULL;

		/* 
		 * if this is not an end packet or if it is subsequent
		 * parts in the chain of a giant packet,  just
		 * discard it
		 */
		if ((rflags & BD_FLAG_END) == 0) {
			more_errpackets = 1;
			m_freem(m);
			continue;
		}

		/* this is the last part of the giant packet */
		if (more_errpackets) {
			DBG_PRINT(("eg%d: big packet received\n", ti->ti_unit));
			ASSERT(rflags & BD_FLAG_END);
			more_errpackets = 0;
			m_freem(m);
			continue;
		}

		ASSERT(more_errpackets == 0);
		ASSERT(rflags & BD_FLAG_END);

		if (rlen > (ti->ti_if.if_mtu + 18) )
			goto error;

		CACHE_INVAL((caddr_t)rb + CACHE_SLINE_SIZE, roundup(rlen, CACHE_SLINE_SIZE));

		IF_INITHEADER(&rb->rb_ebh, &ti->ti_if,
			sizeof(struct etherbufhead));

		ti->ti_if.if_ipackets++;
		ti->ti_if.if_ibytes += rlen;

		eh = &rb->rb_ebh.ebh_ether;


		EG_PRINT(ti, RX, 9, ("eg_recv: dhost: %s, ", ether_sprintf(eh->ether_dhost)))
		EG_PRINT(ti, RX, 9, (" shost: %s, type: 0x%x, len: %d\n", 
				 ether_sprintf(eh->ether_shost), eh->ether_type, rlen))

		m->m_len = (rlen - ETHER_HDRLEN) + sizeof (struct etherbufhead);

		/*
		 * Check TCP or UDP checksum for non-fragments.
		 */

		ip = (struct ip*) rb->rb_data;

		if ((ntohs(eh->ether_type) == ETHERTYPE_IP)
		    && (rlen >= 60)
		    && ((ip->ip_off & (IP_OFFMASK|IP_MF)) == 0)
		    && ((ip->ip_p == IPPROTO_TCP) || (ip->ip_p == IPPROTO_UDP))
		    && (ti->ti_flags & TIF_CKSUM_RX)) {

			if (rbd->BD_tcp_udp_cksum == 0xffff)
				m->m_flags |= M_CKSUMMED;
		}


		/*
		 * Enqueue the packet instead of calling ether_input() now
		 * since we're holding the driver bitlock.
		 */
		IF_ENQUEUE_NOLOCK(qp, m);

		continue;

error:
		snoopflags = SN_ERROR;

		if (rlen < 60) {
			snoopflags |= SNERR_TOOSMALL;
			ti->ti_runt++;
			DBG_PRINT(("eg%d: runt packet, len:%d\n", unit, rlen));

			if (rlen > ETHER_HDRLEN) {
			  eh = &rb->rb_ebh.ebh_ether;

			  EG_PRINT(ti, RX, 1, ("eg_recv: dhost: %s, ", ether_sprintf(eh->ether_dhost)))
			  EG_PRINT(ti, RX, 2, (" shost: %s, type: 0x%x, len: %d\n", 
					   ether_sprintf(eh->ether_shost), eh->ether_type, rlen))
			}

			rlen = ETHER_HDRLEN;
		}
		else if (rlen > (ti->ti_if.if_mtu + 18) ) {
			snoopflags |= SNERR_TOOBIG;
			ti->ti_giant++;
			EG_PRINT(ti, RX, 1, ("eg%d: giant packet received from %s\n", unit, ether_sprintf(eh->ether_shost)))
			rlen = ti->ti_if.if_mtu + 18;
		}


		m->m_len = (rlen - ETHER_HDRLEN) + sizeof (struct etherbufhead);
		ether_input(&ti->ti_eif, snoopflags, m);

	}

	ASSERT(i == rxrtn_prod);

	/* update rxd cons */
	ti->ti_rxrtn_cons_real = rxrtn_prod;

	/* update rx2cons on the NIC only after a threshold */
	if (DELTARXD2(ti->ti_rxrtn_cons_real, ti->ti_rxrtn_cons_shad) > eg_rx2_thresh) {
		ti->ti_rxrtn_cons_shad = ti->ti_rxrtn_cons_real;
		ti->ti_gencom.recv_return_cons_index = ti->ti_rxrtn_cons_real;
	}
		

	EG_PRINT(ti, RX, 8, ("eg_recv: rxrtn_cons_real:%d\n", i))

	/* post any necessary receive buffers */
	eg_fill(ti);

	return (qp);
}


static int
eg_transmit(struct etherif *eif,
	struct etheraddr *edst,
	struct etheraddr *esrc,
	u_short type,
	struct mbuf *m0)
{
	struct eg_info *ti;
	struct mbuf *m, *mf;
	struct ether_header *eh;
	caddr_t p;
	int prod;
	int mlen;
	int s;
	int rc=0;
	struct ifqueue qrecl, qcopy;
	int send_pk=0;


	GOODMP(m0);
	GOODMT(m0->m_type);

	ti = eiftoti(eif);

	qrecl.ifq_head = qrecl.ifq_tail = NULL;
	qcopy.ifq_head = qcopy.ifq_tail = NULL;


	EG_PRINT(ti, TX, 9, ("eg_transmit: dst: %s,", ether_sprintf(edst->ea_vec)))
	EG_PRINT(ti, TX, 9, (" src: %s type:0x%x\n",
		ether_sprintf(esrc->ea_vec), type))

	/*
	 * The basic performance idea is to keep a dense,
	 * linear fastpath and throw rare conditions out via "goto".
	 */

	if ((ti->ti_flags & TIF_LINKDOWN))
		goto linkdown;

	if (m0 == 0) {
		s = mutex_bitlock(&ti->ti_flags, TIF_TLOCK);
		if (ti->ti_if.if_snd.ifq_len == 0) {
			mutex_bitunlock(&ti->ti_flags, TIF_TLOCK, s);
			return 0;
		} else {	
			/* send as many queued packets as possible */
			ti->ti_restart++;
			goto drain_q;
		}
	}

	/* zero out the tcp/udp checksum field if hardware
	 * checksumming is used. 
	 */
	
	if (m0->m_flags & M_CKSUMMED) {
		struct ip *ip;
		int hlen;
		int sumoff;

		ip = mtod(m0, struct ip*);
		hlen = ip->ip_hl << 2;

		ASSERT((ip->ip_off & (IP_OFFMASK|IP_MF)) == 0);
		ASSERT((ip->ip_p == IPPROTO_TCP) || (ip->ip_p == IPPROTO_UDP));
		ASSERT(m0->m_len >= sizeof (struct ip*));

		if (ip->ip_p == IPPROTO_TCP)
                        sumoff = hlen + (8 * 2);
                else
                        sumoff = hlen + (3 * 2);

		ASSERT(sumoff < m0->m_len);

                /* clear out the cksum field in the tcp/udp header */
                mtod(m0, u_short*)[sumoff/sizeof (u_short)] = 0;
	}

	/* 
	 * Create ether header:
	 * 	If there is room in m0, put etherheader there.
	 * 	Otherwise, allocate a new one
	 */

#ifdef GE_FICUS
	if (!mbuf_hasroom(m0, ETHER_HDRLEN)) {
#else	
	if (!m_hasroom(m0, ETHER_HDRLEN)) {  
#endif
		if ((m = m_get(M_DONTWAIT, MT_DATA)) == NULL)
                        goto outofmbufs;

                m->m_off += ETHER_HDRLEN;
                m->m_flags |= (m0->m_flags & M_COPYFLAGS);
                m->m_next = m0;
                m0 = m;
	}

	m0->m_off -= ETHER_HDRLEN;
	m0->m_len += ETHER_HDRLEN;
	eh = mtod(m0, struct ether_header*);
	*(struct etheraddr*)eh->ether_dhost = *edst;
	*(struct etheraddr*)eh->ether_shost = *esrc;
	eh->ether_type = type;

	s = mutex_bitlock(&ti->ti_flags, TIF_TLOCK);
	
	if (IF_QFULL(&ti->ti_if.if_snd)) {
		if (INCRING(ti->ti_txprod, SEND_RING_ENTRIES) == ti->ti_txcons_shad) {
			
			EG_PRINT(ti, TX, 1, ("eg%d: eg_transmit: ifq FULL: tx_cons:%d, tx_prod:%d, tx_consptr:%d\n", ti->ti_unit, ti->ti_txcons_shad, ti->ti_txprod, HI32(*ti->ti_txconsptr) ))

			ti->ti_noq++;

			IF_DROP(&ti->ti_if.if_snd);
			mutex_bitunlock(&ti->ti_flags, TIF_TLOCK, s);
			m_freem(m0);
			return (ENOBUFS);

		}
	}

	IF_ENQUEUE_NOLOCK(&ti->ti_if.if_snd, m0);

drain_q:
	while(ti->ti_if.if_snd.ifq_len) {

		m0 = ti->ti_if.if_snd.ifq_head;

top:
		mlen = 0;
		prod = ti->ti_txprod;
		
		for (m = m0; m; m = m->m_next) {
			U32 bd_flags;

			mlen += m->m_len;
			
                        if (INCRING(prod, SEND_RING_ENTRIES) == ti->ti_txcons_shad) {
                                /* reclaim tx_bd */
                                eg_reclaim(ti, &qrecl);
                                if (INCRING(prod, SEND_RING_ENTRIES) == ti->ti_txcons_shad) {
                                        goto outoftxd;
                                }
                        }
	
			/* rare case:  force copy if mbuf crosses K2SEG boundary */
			p = mtod(m, caddr_t);
			if (m->m_len && (pnum(p) != pnum(p+m->m_len-1)))
				goto copyall;

#if 0
/* ubug fix */
if ((!m->m_next && mlen < 60) && m != m0)
  goto copyall;
#endif
			CACHE_WB(mtod(m, caddr_t), m->m_len);
			/* write into the send bd */
			ti->ti_txring[prod].host_addr =  KVTOIOADDR_DATA(ti, mtod(m, caddr_t));
			
			bd_flags = (m->m_next ? BD_FLAG_MORE : BD_FLAG_END) | 
				   ((m0->m_flags & M_CKSUMMED) ? BD_FLAG_TCP_UDP_CKSUM : 0);
	
			ti->ti_txring[prod].w0.w0 = (m->m_len << 16) | bd_flags;
					
			EG_PRINT(ti, TX, 9, ("eg_transmit: txprod:%d, length_flags= %d:%d\n", 
				prod, m->m_len, bd_flags))
			
			prod = INCRING(prod, SEND_RING_ENTRIES);
		}
		
		if (RAWIF_SNOOPING(&ti->ti_rawif)) {
			eh = mtod(m0, struct ether_header*);
			eg_selfsnoop(ti, eh, m0, mlen);
		}
		
		/* save the mbuf chain to free later */
		ASSERT(ti->ti_txm[ti->ti_txprod] == NULL);
		ti->ti_txm[ti->ti_txprod] = m0;  
		

		/* update tx produce index */
		ti->ti_txprod = prod;  
		
		send_pk++;
		
		ti->ti_if.if_obytes += mlen;
		
		EG_PRINT(ti, TX, 9, ("eg_transmit: txprod:%d len:%d\n", prod, mlen))

		IF_DEQUEUE_NOLOCK(&ti->ti_if.if_snd, m0);		
	}

	ASSERT(send_pk >=0);

	if (send_pk) 
		W_REG(ti->ti_shmem->sendProdIndex, ti->ti_txprod);

	mutex_bitunlock(&ti->ti_flags, TIF_TLOCK, s);

	rc = 0;
	goto free_q;

copyall:

	mlen = 0;
	for (m = m0; m; m = m->m_next) {
		mlen += m->m_len;
	}
	if ((m = m_vget(M_DONTWAIT, mlen, MT_DATA)) == NULL)
		goto outofmbufs;

	m_datacopy(m0, 0, mlen, mtod(m, caddr_t));
	m->m_len = mlen;
	m->m_flags |= (m0->m_flags & M_COPYFLAGS);

	IF_ENQUEUE_NOLOCK(&qcopy, m0);
	m0 = m;

	goto top;


outoftxd:
	EG_PRINT(ti, TX, 3, ("eg%d: outoftxd: trying to reclaim: tx_cons:%d, tx_prod:%d, tx_consptr:%d\n", ti->ti_unit, ti->ti_txcons_shad, ti->ti_txprod, HI32(*ti->ti_txconsptr) ))

	/* if any pks got moved from send queue, send them */
	if (send_pk)
	  W_REG(ti->ti_shmem->sendProdIndex, ti->ti_txprod);

        EG_PRINT(ti, TX, 1, ("eg%d: outoftxd\n", ti->ti_unit))
	mutex_bitunlock(&ti->ti_flags, TIF_TLOCK, s);
	rc = 0;		/* no error; all packets queued */
	goto free_q;

outofmbufs:
	ti->ti_nombufs++;
	EG_PRINT(ti, TX, 1, ("eg%d: eg_transmit: out of mbufs\n", ti->ti_unit))

	/* if any pks got moved from send queue, send them */
	if (send_pk)
	  W_REG(ti->ti_shmem->sendProdIndex, ti->ti_txprod);

	mutex_bitunlock(&ti->ti_flags, TIF_TLOCK, s);
	m_freem(m0);
	rc = ENOBUFS;
	/* goto free_q */

free_q:
	mf = qrecl.ifq_head;
	while (mf) {
                m = mf;
                mf = mf->m_act;
                m_freem(m);
        }

	mf = qcopy.ifq_head;
	while (mf) {
                m = mf;
                mf = mf->m_act;
                m_freem(m);
        }
	return (rc);

linkdown:
	ti->ti_linkdrop++;
	EG_PRINT(ti, TX, 1, ("eg%d: eg_transmit: link down\n", ti->ti_unit))
	m_freem(m0);
	return (ENETDOWN);
}


static void
eg_selfsnoop(struct eg_info *ti,
        struct ether_header *eh,
        struct mbuf *m0,
        int mlen)
{
        __uint32_t f[SNOOP_FILTERLEN];
        caddr_t h;

        h = (caddr_t) f;
	ASSERT(mlen >= ETHER_HDRLEN);

	/* 2 bytes of padding + 14 bytes of ether_header */
        m_datacopy(m0, 0, (SNOOP_FILTERLEN * sizeof (__uint32_t))-2, &h[2]);
        
        if (snoop_match(&ti->ti_rawif, &h[2], SNOOP_FILTERLEN * sizeof(__uint32_t)))
                ether_selfsnoop(&ti->ti_eif, eh, m0, ETHER_HDRLEN, mlen - ETHER_HDRLEN);
}

/*
 * Do miscellaneous periodic things.
 */
static void
eg_watchdog(struct ifnet *ifp)
{

	struct eg_info *ti = eiftoti(ifptoeif(ifp));
	int s;

	EG_PRINT(ti, INTR, 9, ("eg_watchdog\n"))
 
	if (!eg_watchdog_on) {
		EG_PRINT(ti, INTR, 9, ("eg_watchdog disabled\n"))
		return;
	}

	s = mutex_bitlock(&ti->ti_rflags, TIF_RLOCK);

	/* XXX send noop in watchdog; update noop count */
	/* if count does not increase, cmn_err and reset */

	/* XXX remove later:  used only for symmon debugging */
	W_REG(ti->ti_tuneparams.send_coal_ticks, eg_send_coal_ticks);
	W_REG(ti->ti_tuneparams.recv_coal_ticks, eg_recv_coal_ticks[ti->ti_unit]);
	W_REG(ti->ti_tuneparams.send_max_coalesced_bds, eg_send_coal_bd);
	W_REG(ti->ti_tuneparams.recv_max_coalesced_bds, eg_recv_coal_bd);
	W_REG(ti->ti_tuneparams.stat_ticks, eg_stat_ticks);

	/* refill any missing receive buffers */
	eg_fill(ti);

	ti->ti_if.if_timer = eg_watch_time;

	mutex_bitunlock(&ti->ti_rflags, TIF_RLOCK, s);
	s = mutex_bitlock(&ti->ti_flags, TIF_TLOCK);
	if (ti->ti_waittime && (lbolt > (ti->ti_waittime + HZ/2))) {
		sv_signal(&ti->ti_sv);
	}
	mutex_bitunlock(&ti->ti_flags, TIF_TLOCK, s);

}

static void
eg_dump(int unit)
{
	struct eg_info *ti;
	struct ifnet *ifp;
	char name[128];

	if (unit == -1)
		unit = 0;

	sprintf(name, "eg%d", unit);

	if ((ifp = ifunit(name)) == NULL) {
		qprintf("eg_dump: %s not found in ifnet list\n", name);
		return;
	}
	ti = eiftoti(ifptoeif(ifp));

	eg_dumpif(&ti->ti_if);
	qprintf("\n");
	eg_dumpti(ti);
	qprintf("\n");
}

static void
eg_dumpif(struct ifnet *ifp)
{
	qprintf("ifp 0x%x if_name \"%s\" if_unit %d if_mtu %d if_flags 0x%x\n",
                ifp, ifp->if_name, ifp->if_unit, ifp->if_mtu, ifp->if_flags);

	qprintf("if_timer %d ifq 0x%x ifq_len %d ifq_maxlen %d ifq_drops %d\n",
                ifp->if_timer, &ifp->if_snd, ifp->if_snd.ifq_len, ifp->if_snd.ifq_maxlen,
                ifp->if_snd.ifq_drops);
        qprintf("if_ipackets %u if_ierrors %u if_opackets %u if_oerrors %u\n",
                ifp->if_ipackets, ifp->if_ierrors,
                ifp->if_opackets, ifp->if_oerrors);
        qprintf("if_collisions %u if_ibytes %u if_obytes %u if_odrops %u\n",
                ifp->if_collisions, ifp->if_ibytes,
                ifp->if_obytes, ifp->if_odrops);
}

static void
eg_dumpti(struct eg_info *ti)
{
        qprintf("ti 0x%x ac_enaddr %s ifp 0x%x rawif 0x%x\n",
		ti, ether_sprintf(ti->ti_curaddr), &ti->ti_if, &ti->ti_rawif);

	qprintf("LINK STATE: %s\n", (ti->ti_flags & TIF_LINKDOWN)? "DOWN":"UP");

	qprintf("ti_flags 0x%x ti_flags_addr 0x%x ti_rflags 0x%x ti_rflags_addr 0x%x ti_sv 0x%x\n",
		ti->ti_flags, &ti->ti_flags, ti->ti_rflags, &ti->ti_rflags, &ti->ti_sv);

	qprintf("shmem 0x%x gencom 0x%x  geninfo 0x%x\n",
		ti->ti_shmem, &ti->ti_gencom, ti->ti_geninfo);

	qprintf("waittime %x\n",ti->ti_waittime);
	qprintf("pc 0x%x traceptr 0x%x trace_start 0x%x trace_len 0x%x \n",
		ti->ti_cpuctlregs.pc, ti->ti_gencom.nic_trace_ptr,
		ti->ti_gencom.nic_trace_start, ti->ti_gencom.nic_trace_len);

	qprintf("revision %d, nrrbs %d, gotrrbs %d\n",ti->ti_rev, 
		ti->ti_nrrbs, ti->ti_gotrrbs);
	qprintf("RINGS:\n");
	qprintf("eventring 0x%x evprod %u, evcons %u\n",
		ti->ti_evring, HI32(*ti->ti_evprodptr), ti->ti_evcons);
	qprintf("cmdring 0x%x cmdcons %u \n",
		ti->ti_cmdring, ti->ti_cmdcons);
	qprintf("evprodptr 0x%x\n",ti->ti_evprodptr);

        qprintf("txring 0x%x txprod %u txcons_shad %u txcons %u txm 0x%x\n",
                ti->ti_txring, ti->ti_txprod, ti->ti_txcons_shad,
                HI32(*ti->ti_txconsptr), ti->ti_txm);

        qprintf("rxring_std 0x%x   rxstd_prod 0x%x   rxstd_cons 0x%x  rxm_std 0x%x\n",
                ti->ti_rxring_std, ti->ti_rxstd_prod, ti->ti_rxstd_cons, ti->ti_rxm_std);

        qprintf("rxring_jumbo 0x%x   rxjumbo_prod 0x%x   rxjumbo_cons 0x%x  rxm_jumbo 0x%x\n",
                ti->ti_rxring_jumbo, ti->ti_rxjumbo_prod, ti->ti_rxjumbo_cons, ti->ti_rxm_jumbo);

	qprintf("rxring_rtn 0x%x\n", ti->ti_rxring_rtn);
	qprintf("rxrtn_prod %u val:0x%x, addr:0x%x, gencom_rxrtn_cons %u rxrtn_cons_real %u rxrtn_cons_shad %u\n",
		HI32(*ti->ti_rxrtn_prod), *(U64 *)ti->ti_rxrtn_prod, ti->ti_rxrtn_prod, 
		ti->ti_gencom.recv_return_cons_index, ti->ti_rxrtn_cons_real, ti->ti_rxrtn_cons_shad);

        qprintf("nmulti %u defers %u runt %u crc %u giant %u framerr %u\n",
                ti->ti_nmulti, ti->ti_defers, ti->ti_runt, ti->ti_crc, ti->ti_giant, ti->ti_framerr);
        qprintf("rtry %u exdefer %u lcol %u txgiant %u linkdrop %u\n",
                ti->ti_rtry, ti->ti_exdefer,
		ti->ti_lcol, ti->ti_txgiant, ti->ti_linkdrop);
	qprintf("nombufs %u notxds %u noq %u cmdoflo %u\n",
		ti->ti_nombufs, ti->ti_notxds, ti->ti_noq, ti->ti_cmdoflo);
	qprintf("restart %u FWrefreshes %u\n", ti->ti_restart, ti->ti_FWrefreshes);

	qprintf("firmware:\n");
	qprintf("\tfwstartaddr 0x%x, fwtextaddr 0x%x, fwtextlen %d, fwtext 0x%x\n",
		ti->ti_tigonFwStartAddr, ti->ti_tigonFwTextAddr,
		ti->ti_tigonFwTextLen, ti->ti_tigonFwText);
	qprintf("\tfwdataaddr 0x%x, fwdatalen %d, fwdata 0x%x\n",
		ti->ti_tigonFwDataAddr,
		ti->ti_tigonFwDataLen, ti->ti_tigonFwData);
	qprintf("\tfwrodataaddr 0x%x, fwrodatalen %d, fwrodata 0x%x\n",
		ti->ti_tigonFwRodataAddr,
		ti->ti_tigonFwRodataLen, ti->ti_tigonFwRodata);
	qprintf("\tfwbssaddr 0x%x, fwbsslen %d, fwsbssaddr 0x%x, fwsbsslen %d\n",
		ti->ti_tigonFwBssAddr, ti->ti_tigonFwBssLen, 
		ti->ti_tigonFwSbssAddr, ti->ti_tigonFwSbssLen);
}

static void
eg_tuneparams(struct eg_info *ti)
{

        ti->ti_tuneparams.recv_coal_ticks = eg_recv_coal_ticks[ti->ti_unit];
        ti->ti_tuneparams.send_coal_ticks = eg_send_coal_ticks;
	ti->ti_tuneparams.send_max_coalesced_bds = eg_send_coal_bd;
	ti->ti_tuneparams.recv_max_coalesced_bds = eg_recv_coal_bd;
	ti->ti_tuneparams.stat_ticks = eg_stat_ticks;
	ti->ti_tuneparams.tracing = eg_trace;

	ti->ti_tuneparams.link_negotiation = LNK_ENABLE        + LNK_FULL_DUPLEX   + LNK_1000MB + 
					     LNK_RX_FLOW_CTL_Y + LNK_TX_FLOW_CTL_Y;

	if (eg_autoneg_array[ti->ti_unit])
		ti->ti_tuneparams.link_negotiation |= LNK_NEGOTIATE;
}

/*
 * this resets the card and causes the card bootstrap sequence
 * to be executed.   
 * XXX: Wait for a few milliseconds
 */
static int
eg_resetcard(struct eg_info *ti)
{
	U32 cpu_state;

	ti->ti_if.if_flags &= ~IFF_RUNNING;
	W_REG(ti->ti_mhc, TG_MHC_RESET);
	DELAY(1*1000*1000);
	
	/*  make sure diags are passed */
	cpu_state = R_REG(ti->ti_regs.cpu_control.cpu_state);
	
	if (cpu_state & TG_CPU_PROM_FAILED)
		return (-1);

	return (0);
}


/*
 * loadFW and startFW
 */
static int
eg_startcard(struct eg_info *ti)
{
	int error;

	/* halt cpu and load firmware */
	if (error = eg_loadFW(ti))
		return (error);

	ASSERT(ti->ti_evcons == HI32(*ti->ti_evprodptr));

	eg_startFW(ti);

	/* wait for firmware operational event */
	error = eg_waitforevent(ti, TG_EVENT_FIRMWARE_OPERATIONAL);
	
	if (!error)
		EG_PRINT(ti, LNK, 1, ("eg%d: FIRMWARE_OPERATIONAL\n", ti->ti_unit))

	return (error);
}

static int
eg_writeshmem(struct eg_info *ti, U32 tgAddr, caddr_t userAddr, int len) {

	int tmpLen;

	/* fail if non-aligned data */
	if (((U64)userAddr & 3) || ((U64)tgAddr & 3) || (len & 3))
		return(-1);

	/* copy the first unaligned section */
	if (!WINDOW_ALIGNED(tgAddr)) {
		/* set the window */
		W_REG(ti->ti_regs.gen_control.window_base, WINDOW_PAGE(tgAddr));

		tmpLen = MIN(WINDOW_RESID(tgAddr), len);

		/* XXX alteon:  put non-aligned fix in here */
		eg_memcpy((caddr_t)(ti->ti_memwindow) + WINDOW_OFFSET(tgAddr),
			   userAddr, tmpLen);
		CACHE_WB((caddr_t)(ti->ti_memwindow) + WINDOW_OFFSET(tgAddr),
			tmpLen);

		/* adjust the pointers */
		userAddr += tmpLen;
		tgAddr += tmpLen;
		len -= tmpLen;
	}

	/* copy the rest */
	while (len > 0) {
		/* set the window */
		W_REG(ti->ti_regs.gen_control.window_base, tgAddr);
		
		tmpLen = MIN(WINDOW_LEN, len);
		
		/* XXX alteon: put non-aligned fix in here */
		eg_memcpy((caddr_t)(ti->ti_memwindow),
			   userAddr, tmpLen);

		CACHE_WB((caddr_t)ti->ti_memwindow, tmpLen);
		/* adjust the pointers */
		userAddr += tmpLen;
		tgAddr += tmpLen;
		len -= tmpLen;
	}

	W_REG(ti->ti_regs.gen_control.window_base, SEND_RING_PTR);
	return (0);
}



static int
eg_readshmem(struct eg_info *ti, U32 tgAddr, caddr_t userAddr, int len) {

	int tmpLen;

	/* fail if non-aligned data */
	if (((U64)userAddr & 3) || ((U64)tgAddr & 3) || (len & 3))
		return(-1);

	/* copy the first unaligned section */
	if (!WINDOW_ALIGNED(tgAddr)) {
		/* set the window */
		W_REG(ti->ti_regs.gen_control.window_base, WINDOW_PAGE(tgAddr));

		tmpLen = MIN(WINDOW_RESID(tgAddr), len);

		/* XXX put non-aligned fix in here */
		eg_memcpy(userAddr, 
			  (caddr_t)(ti->ti_memwindow) + WINDOW_OFFSET(tgAddr),
			   tmpLen);
		
		/* adjust the pointers */
		userAddr += tmpLen;
		tgAddr += tmpLen;
		len -= tmpLen;
	}

	/* copy the rest */
	while (len > 0) {
		/* set the window */
		W_REG(ti->ti_regs.gen_control.window_base, tgAddr);
		
		tmpLen = MIN(WINDOW_LEN, len);
		
		/* XXX put non-aligned fix in here */
		eg_memcpy(userAddr, (caddr_t)(ti->ti_memwindow), tmpLen);
		
		/* adjust the pointers */
		userAddr += tmpLen;
		tgAddr += tmpLen;
		len -= tmpLen;
	}

	W_REG(ti->ti_regs.gen_control.window_base, SEND_RING_PTR);
	return (0);
}


static void 
eg_memcpy(caddr_t toAddr, caddr_t fromAddr, int len)
{
	caddr_t locToAddr = (caddr_t) toAddr;
	caddr_t locFromAddr = (caddr_t) fromAddr;

int count=0;

	while (len > 0) {
		/* this is a W_REG */
		*(U32 *)locToAddr = *(U32 *)locFromAddr;

		/* XXX WAR: sometimes the read value is different??? */
		if (*(int *)locToAddr != *(int *)locFromAddr) {
		  if (++count < 20) {
		    continue;
		  }
		}
		count=0;

		locFromAddr += 4;
		locToAddr += 4;
		len -= 4;
	}
}


/*
 * check an arbitrary value in the loaded FW 
 * address: 0x4008 has value 0x3a0f021
 */
static void
eg_checkFW(struct eg_info *ti, U32 tgAddr)
{
	U32 got_value;

	W_REG(ti->ti_regs.gen_control.window_base, WINDOW_PAGE(tgAddr));
	got_value =  *(U32 *)((caddr_t)(ti->ti_memwindow) + WINDOW_OFFSET(tgAddr));
	qprintf("value got: 0x%x\n", got_value);
	W_REG(ti->ti_regs.gen_control.window_base, SEND_RING_PTR);
}



/*
 * set the cpu control register to HALT
 */
static void
eg_stopFW(struct eg_info *ti)
{
	U32 cpu_state;

	cpu_state = R_REG(ti->ti_cpuctlregs.cpu_state);
	W_REG(ti->ti_cpuctlregs.cpu_state, cpu_state | TG_CPU_HALT);
	/* barrier sync */
	cpu_state = R_REG(ti->ti_cpuctlregs.cpu_state);
		
}

static void
eg_freeFW(struct eg_info *ti)
{
	if (ti->ti_tigonFwText)
		kmem_free((caddr_t)ti->ti_tigonFwText, ti->ti_tigonFwTextLen);
	if (ti->ti_tigonFwData)
		kmem_free((caddr_t)ti->ti_tigonFwData, ti->ti_tigonFwDataLen);
	if (ti->ti_tigonFwRodata)
		kmem_free((caddr_t)ti->ti_tigonFwRodata, ti->ti_tigonFwRodataLen);

	ti->ti_tigonFwText = ti->ti_tigonFwData = ti->ti_tigonFwRodata = NULL;
}


static int
eg_loadFW(struct eg_info *ti)
{
	char *zerobuf = NULL;

	eg_stopFW(ti);

	/* if no new FW, just return here and let the driver
	 *  re-start the FW on the NVRAM
	 */
	if (!ti->ti_tigonFwText || !ti->ti_tigonFwData || !ti->ti_tigonFwRodata) {
		eg_freeFW(ti);
		return (0);
	}
	
	EG_PRINT(ti, LNK, 6, ("loading FwText: tigonFwTextAddr:0x%x, len: %d\n", 
		ti->ti_tigonFwTextAddr, ti->ti_tigonFwTextLen))

	/* write out the text segment */
	eg_writeshmem(ti, ti->ti_tigonFwTextAddr, (caddr_t)ti->ti_tigonFwText, ti->ti_tigonFwTextLen);

	EG_PRINT(ti, LNK, 6, ("loading FwRodata: eg_tigonFwRodataAddr:0x%x, len: %d\n", ti->ti_tigonFwRodataAddr, ti->ti_tigonFwRodataLen))

	/* write out the rodata segment */
	if (ti->ti_tigonFwRodataLen)
		eg_writeshmem(ti, ti->ti_tigonFwRodataAddr, (caddr_t)ti->ti_tigonFwRodata, ti->ti_tigonFwRodataLen);

	EG_PRINT(ti, LNK, 6, ("loading FwData: tigonFwDataAddr:0x%x, len: %d\n", ti->ti_tigonFwDataAddr, ti->ti_tigonFwDataLen))

	/* write out the data segment */
	if (ti->ti_tigonFwDataLen)
		eg_writeshmem(ti, ti->ti_tigonFwDataAddr, (caddr_t)ti->ti_tigonFwData, ti->ti_tigonFwDataLen);

	EG_PRINT(ti, LNK, 6, ("loading FwBss: tigonFwBssAddr:0x%x, len: %d\n", ti->ti_tigonFwBssAddr, ti->ti_tigonFwBssLen))

	/* write zeroed bss */
	if (ti->ti_tigonFwBssLen) {
		if ((zerobuf = (char *)kmem_zalloc(ti->ti_tigonFwBssLen, KM_SLEEP)) == NULL)
			return -1;
		eg_writeshmem(ti, ti->ti_tigonFwBssAddr, (caddr_t)zerobuf, ti->ti_tigonFwBssLen);
		kmem_free((caddr_t)zerobuf, ti->ti_tigonFwBssLen);
	}

	EG_PRINT(ti, LNK, 6, ("loading FwSbss: tigonFwSbssAddr:0x%x, len: %d\n", ti->ti_tigonFwSbssAddr, ti->ti_tigonFwSbssLen))

	/* write zeroed sbss */
	if (ti->ti_tigonFwSbssLen) {
		if ((zerobuf = (char *)kmem_zalloc(ti->ti_tigonFwSbssLen, KM_SLEEP)) == NULL)
			return -1;
		eg_writeshmem(ti, ti->ti_tigonFwSbssAddr, (caddr_t)zerobuf, ti->ti_tigonFwSbssLen);
		kmem_free((caddr_t)zerobuf, ti->ti_tigonFwSbssLen);
	}


	/* free FW memory */
	eg_freeFW(ti);

	ti->ti_flags |= TIF_FWLOADING;
	return (0);
}


static void
eg_startFW(struct eg_info *ti)
{

	U32 cpu_state;

	/* halt the cpu */
	cpu_state = R_REG(ti->ti_cpuctlregs.cpu_state);
	W_REG(ti->ti_cpuctlregs.cpu_state, cpu_state | TG_CPU_HALT);

	ti->ti_flags |= TIF_FWLOADING;

	/* load the PC */
	W_REG(ti->ti_cpuctlregs.pc, ti->ti_tigonFwStartAddr);

	/* start the cpu */
	cpu_state = R_REG(ti->ti_cpuctlregs.cpu_state);
	W_REG(ti->ti_cpuctlregs.cpu_state, 
	      cpu_state & ~(TG_CPU_HALT | TG_CPU_SINGLE));

	return;
}


/* ring manipulation routines */
static int
eg_docmd(struct eg_info *ti, U32 cmd, U32 index, U32 code)
{
	int next;
	U32 command;
	
	EG_PRINT(ti, CMD, 1, ("eg_docmd: cmd: (0x%x:%d:0x%x)\n", cmd, index, code))

	ASSERT(cmd <= TG_CMD_REFRESH_STATS);

	/* allocate next command entry */
	command = MAKE_TG_COMMAND(cmd, index, code);
	next = INCRING(ti->ti_cmdprod, COMMAND_RING_ENTRIES);
	if (next == ti->ti_cmdcons_shad)
		goto cmdoflo;

writecmd:
	W_REG(ti->ti_cmdring[ti->ti_cmdprod].TG_COMMAND_w0, command);
	ti->ti_cmdprod = next;

	W_REG(ti->ti_shmem->command_prod_index, next);
	
	return (0);

cmdoflo:
	/* update our cmd consume index */
	ti->ti_cmdcons_shad = R_REG(ti->ti_cmdcons);

	/* try again with updated value of cmd consume index */
	if (next != ti->ti_cmdcons_shad)
		goto writecmd;
	
	DBG_PRINT(("eg%d: cmd ring overflow\n", ti->ti_unit));
	ti->ti_cmdoflo++;

	return (ENOMEM);
}

/*
 * loop for this particular event with sleeps of 100us in between
 * Error if event we are not expecting or if timeout
 */
static int
eg_waitforevent(struct eg_info *ti, U32 event)
{
	int got_event;
	int count;
	U32 evcons;
	
	count = 0;
	evcons = ti->ti_evcons;

	ASSERT(evcons < EVENT_RING_ENTRIES);
	ASSERT(event <= TG_EVENT_MULTICAST_LIST_UPDATED);

	/* wait for next event to arrive */
	while (evcons == HI32(*ti->ti_evprodptr)) {
		DELAY(100);
		if (++count >= 20000) {
			cmn_err(CE_ALERT, "eg%d: firmware did not complete initialization",
				ti->ti_unit);
			goto bad;
		}

	}
	
	got_event = ti->ti_evring[evcons].TG_EVENT_event;

	/* increment event consumer index */
	ti->ti_evcons = INCRING(evcons, EVENT_RING_ENTRIES);
	W_REG(ti->ti_shmemevcons,  evcons);

	/* compare with the expected event */
	if (got_event != event) {
		cmn_err(CE_ALERT, "eg%d: firmware init error, expected=0x%x, actual=0x%x\n",
		       ti->ti_unit, event, got_event);
		goto bad;
	}
	
	return (0);

bad:
	return (EIO);
}


/*
 *  serial eeprom routines 
 */

#define TRUE 1
#define FALSE 0

/* delays in usec (actual delays to right) */
#define ALT_SER_EEPROM_THIGH     1    /* > 0.6 us */
#define ALT_SER_EEPROM_H_THIGH   1
#define ALT_SER_EEPROM_TLOW      2    /* > 1.2 us */
#define ALT_SER_EEPROM_H_TLOW    1
#define ALT_SER_EEPROM_START_SU  1    /* > 0.6 us (setup time) */
#define ALT_SER_EEPROM_START_HO  1    /* > 0.6 us (hold time) */
#define ALT_SER_EEPROM_STOP_SU   1    /* > 0.6 us (setup time) */
#define ALT_SER_EEPROM_TBUF      2    /* > 1.2 us */

#define SER_EEPROM_WRITE_SELECT    0xa0
#define SER_EEPROM_READ_SELECT     0xa1


#define MLC_SET_BIT(bit) { \
    W_REG(ti->ti_regs.gen_control.misc_local_control,  \
          R_REG(ti->ti_regs.gen_control.misc_local_control) | (bit)); \
     R_REG(ti->ti_regs.gen_control.misc_local_control); \
		   }

#define MLC_CLEAR_BIT(bit) { \
    W_REG(ti->ti_regs.gen_control.misc_local_control, \
	  R_REG(ti->ti_regs.gen_control.misc_local_control) & ~(bit)); \
    R_REG(ti->ti_regs.gen_control.misc_local_control); \
		     }

#define IS_SER_EEPROM_BIT_SET  \
    ((R_REG(ti->ti_regs.gen_control.misc_local_control) &  \
          TG_MLC_SER_EEPROM_DATA_IN) != 0)

#define SER_EEPROM_START                        \
    DELAY(ALT_SER_EEPROM_H_TLOW);              \
    MLC_SET_BIT(TG_MLC_SER_EEPROM_DATA_OUT);    \
    MLC_SET_BIT(TG_MLC_SER_EEPROM_WRITE_ENA);   \
    DELAY(ALT_SER_EEPROM_H_TLOW);              \
    MLC_SET_BIT(TG_MLC_SER_EEPROM_CLK_OUT);     \
    DELAY(ALT_SER_EEPROM_START_SU);            \
    MLC_CLEAR_BIT(TG_MLC_SER_EEPROM_DATA_OUT);  \
    DELAY(ALT_SER_EEPROM_START_HO);            \
    MLC_CLEAR_BIT(TG_MLC_SER_EEPROM_CLK_OUT);

#define SER_EEPROM_STOP                         \
    MLC_SET_BIT(TG_MLC_SER_EEPROM_WRITE_ENA);   \
    DELAY(ALT_SER_EEPROM_H_TLOW);              \
    MLC_CLEAR_BIT(TG_MLC_SER_EEPROM_DATA_OUT);  \
    DELAY(ALT_SER_EEPROM_H_TLOW);              \
    MLC_SET_BIT(TG_MLC_SER_EEPROM_CLK_OUT);     \
    DELAY(ALT_SER_EEPROM_STOP_SU);             \
    MLC_SET_BIT(TG_MLC_SER_EEPROM_DATA_OUT);    \
    DELAY(ALT_SER_EEPROM_TBUF);                \
    MLC_CLEAR_BIT(TG_MLC_SER_EEPROM_CLK_OUT);

static int
eg_getmacaddr(struct eg_info *ti, struct etheraddr *macaddr)
{
	U32 macspot[2];
	int rc;
	int offset;
	U32 *wordp;

	macspot[0] = macspot[1] = 0;

	offset = offsetof(eeprom_layout_t, manufact) +
		offsetof(media_manufact_region_t, serial_number);

	if (rc = readTGSerEeprom(ti, TG_BEG_SER_EEPROM + offset, 
				 (char *)macspot, 8)) {
	    printf("eg%d: failed to read PROM %d\n",rc);
	    return rc;
	}

	/* copy into passed location */
	bcopy(((char*)macspot)+2, (char *)macaddr, 6);

	/* also copy into shared memory area */
	wordp = (U32 *)&ti->ti_macaddr;
	W_REG(*wordp, macspot[0]);
	wordp = (U32 *)&ti->ti_macaddr.octet[2];
	W_REG(*wordp, macspot[1]);
#if 0
	if (rc = eg_docmd(ti, TG_CMD_SET_MAC_ADDR, 0, 0)) {
	    printf("eg%d: failed to set MAC address %d\n",rc);
	}

#endif
	return 0;
}


static void
eg_printmagicnum(struct eg_info *ti)
{
	U32 magic;
	int offset;

	offset = offsetof(eeprom_layout_t, bootstrap) +
		offsetof(media_bootstrap_region_t, magic_value);

	if (readTGSerEeprom(ti, TG_BEG_SER_EEPROM + offset, 
				 (char *)&magic, 4))
	    printf("eg_printmagicnum failed\n");

#if DEBUG
	printf("if_eg: eeprom magic number: %d\n", magic);
#endif

}


static int
readTGSerEeprom(struct eg_info *ti, U32 tgAddr, caddr_t userAddr, int len)
{
    int worked;
    int rc = 0;

    /* fail if non-aligned data */
    if (((U64)userAddr & 3) || ((U64)tgAddr & 3) || (len & 3))
	return(-1);

    /* make sure firmware is stopped */
    eg_stopFW(ti);

    /* do it the slow way, word by word */
    while (len > 0) {
	/* get the value */

	    *(U32 *)userAddr = read_serial_eeprom(ti, tgAddr, &worked);
	    if (!worked) {
		rc = EIO;
		break;
	    }

	/* adjust the pointers */
	userAddr += 4;
	tgAddr += 4;
	len -= 4;
    }

    W_REG(ti->ti_regs.gen_control.window_base, SEND_RING_PTR);

    return(rc);
}



static U32
read_serial_eeprom(struct eg_info *ti, U32 addr, int *worked)
{
    U32 rc, data;

    if ((rc = get_serial_byte(ti, addr++)) != -1) {
	data = rc << 24;
	if ((rc = get_serial_byte(ti, addr++)) != -1) {
	    data += rc << 16;
	    if ((rc = get_serial_byte(ti, addr++)) != -1) {
		data += rc << 8;
		if ((rc = get_serial_byte(ti, addr)) != -1) {
		    data += rc;
		    *worked = TRUE;
		    return(htonl(data));
		}
	    }
	}
    }

    *worked = FALSE;
    return(0);
}



static void
ser_eeprom_prep(struct eg_info *ti, U8 pattern)
{
    U8 i;

    MLC_CLEAR_BIT(TG_MLC_SER_EEPROM_DATA_OUT);
    MLC_SET_BIT(TG_MLC_SER_EEPROM_WRITE_ENA);
    for (i=0; i<8; i++, pattern <<= 1) {
        DELAY(ALT_SER_EEPROM_H_TLOW);
        if (pattern & 0x80) {
            MLC_SET_BIT(TG_MLC_SER_EEPROM_DATA_OUT);
        }
        else {
            MLC_CLEAR_BIT(TG_MLC_SER_EEPROM_DATA_OUT);
        }
        DELAY(ALT_SER_EEPROM_H_TLOW);
        MLC_SET_BIT(TG_MLC_SER_EEPROM_CLK_OUT);
        DELAY(ALT_SER_EEPROM_THIGH);
        MLC_CLEAR_BIT(TG_MLC_SER_EEPROM_CLK_OUT);
        MLC_CLEAR_BIT(TG_MLC_SER_EEPROM_DATA_OUT);
    }
}

/* TRUE = acknowledge seen
 * FALSE = acknowledge not seen
 */
static int
ser_eeprom_chk_ack(struct eg_info *ti)
{
    int save;

    MLC_CLEAR_BIT(TG_MLC_SER_EEPROM_WRITE_ENA);
    DELAY(ALT_SER_EEPROM_TLOW);
    MLC_SET_BIT(TG_MLC_SER_EEPROM_CLK_OUT);
    DELAY(ALT_SER_EEPROM_H_THIGH);
    /* sample data in middle of high clk */
    save = IS_SER_EEPROM_BIT_SET;
    DELAY(ALT_SER_EEPROM_H_THIGH);
    MLC_CLEAR_BIT(TG_MLC_SER_EEPROM_CLK_OUT);
    return(!save);
}

static int
get_serial_byte(struct eg_info *ti, U32 addr)
{
    int i, val = 0;
    
    SER_EEPROM_START;
    ser_eeprom_prep(ti, SER_EEPROM_WRITE_SELECT);
    if (!ser_eeprom_chk_ack(ti)) {
        DBG_PRINT(("eg%d: SER_EEPROM_WRITE_SELECT ack failed\n", ti->ti_unit));
	return(-1);
    }
    ser_eeprom_prep(ti, (U8)(addr >> 8));
    if (!ser_eeprom_chk_ack(ti)) {
        DBG_PRINT(("eg%d: get_serial_byte.8 ack failed\n", ti->ti_unit));
	return(-1);
    }

    ser_eeprom_prep(ti, (U8)(addr & 0xff));
    if (!ser_eeprom_chk_ack(ti))
	return(-1);

    SER_EEPROM_START;
    ser_eeprom_prep(ti, SER_EEPROM_READ_SELECT);
	if (!ser_eeprom_chk_ack(ti))
	    return(-1);

    for (i=0; i<8; i++) {
	MLC_CLEAR_BIT(TG_MLC_SER_EEPROM_WRITE_ENA);
	DELAY(ALT_SER_EEPROM_TLOW);
	MLC_SET_BIT(TG_MLC_SER_EEPROM_CLK_OUT);
	DELAY(ALT_SER_EEPROM_H_THIGH);
	/* sample data mid high clk */
	val = (val << 1) | IS_SER_EEPROM_BIT_SET;
	DELAY(ALT_SER_EEPROM_H_THIGH);
	MLC_CLEAR_BIT(TG_MLC_SER_EEPROM_CLK_OUT);
	if (i == 7)
	    MLC_SET_BIT(TG_MLC_SER_EEPROM_WRITE_ENA);
    }
    MLC_SET_BIT(TG_MLC_SER_EEPROM_DATA_OUT);
    DELAY(ALT_SER_EEPROM_TLOW);
    MLC_SET_BIT(TG_MLC_SER_EEPROM_CLK_OUT);
    DELAY(ALT_SER_EEPROM_THIGH);
    MLC_CLEAR_BIT(TG_MLC_SER_EEPROM_CLK_OUT);
    SER_EEPROM_STOP;

    return(val);
}


/*
 * CalcIndex --
 *      Got from LANCE
 *      Given a multicast ethernet address, this routine maps it into
 *      a 16 bit value
 *
 *      (Modified from CMC "User's Guide K1 Kernel Software For Ethernet
 *      Node Processors", May 1986, Appendix D)
 */

mval_t
CalcIndex(unsigned char *addr,
    int len)
{
    unsigned int  crc = ~0;
    unsigned char x;
    int i;
    int j;
#define CRC_MASK        0xEDB88320

    for (i = 0; i < len; i++, addr++) {
        x = *addr;
        for (j = 0; j < 8; j++) {
            if ((x ^ crc) & 1) {
                crc = (crc >> 1) ^ CRC_MASK;
            } else {
                crc = (crc >> 1);
            }
            x >>= 1;
        }
    }
    return((mval_t) (crc >> 26));
}


#ifndef GE_FICUS

/*
 * Register driver parameters with the RSVP packet scheduler.
 *
 * For eg, the data structures keep track of transmit descriptors in the tx queue.
 * Unfortunately, the packet scheduler only deals with packets.
 * So assume each packet uses 2 tx descriptors.
 */
static void
eg_ps_register(struct eg_info *ti, int reinit)
{
	struct ps_parms ps_params;
	int bps;

	/*
	 * Set parameters according to the link speed.
	 */
	if (ti->ti_gencom.linkState & LNK_1000MB) {
	     bps = 1000000000;
	     eg_send_coal_bd = eg_send_coal_bd;
	} else if (ti->ti_gencom.linkState & LNK_100MB) {
	     bps = 100000000;
	     eg_send_coal_bd = eg_send_coal_bd >> 1;
	} else {
	     bps = 10000000;
	     eg_send_coal_bd = eg_send_coal_bd >> 3;
	}

	ps_params.bandwidth = bps;
	ps_params.flags = 0;
	ps_params.txfree = SEND_RING_ENTRIES;
	ps_params.txfree_func = eg_txfree_len;
	ps_params.state_func = eg_setstate;
	if (reinit)
		(void)ps_reinit(eiftoifp(&ti->ti_eif), &ps_params);
	else
		(void)ps_init(eiftoifp(&ti->ti_eif), &ps_params);
}


/*
 * Called by packet scheduling functions to notify driver about queueing state.
 * If setting is 1 then the driver should call ps_txq_stat around once every
 * 60-100 pkts.
 */
static void
eg_setstate(struct ifnet *ifp, int setting)
{
	struct eg_info *ti = eiftoti(ifptoeif(ifp));
	int s;

	s = mutex_bitlock(&ti->ti_flags, TIF_TLOCK);
	if (setting)
		ti->ti_flags |= TIF_PSENABLED;
	else
		ti->ti_flags &= ~TIF_PSENABLED;
	mutex_bitunlock(&ti->ti_flags, TIF_TLOCK, s);
}


/*
 * Called by packet scheduling functions to determine the current length
 * of the driver transmit queue.  (Actually, the driver should return the
 * number of slots in the queue currently free.)
 */
static int
eg_txfree_len(struct ifnet *ifp)
{
	struct eg_info *ti = eiftoti(ifptoeif(ifp));
	return (SEND_RING_ENTRIES - DELTATXD(ti->ti_txprod, HI32(*ti->ti_txconsptr)));
}

#endif  /* GE_FICUS */


#ifdef GE_FICUS
/* from bsdidbg.c of kudzu */
#define m_p             m_u.m_us.mu_p
#define m_refp          m_u.m_us.mu_refp
#define m_ref           m_u.m_us.mu_ref
#define m_size          m_u.m_us.mu_size

/* This is the m_hasroom routine from kudzu */
/*
 * Return TRUE if there are at least 'len' bytes of room available
 * at the front of the mbuf, else return FALSE.
 */
int
mbuf_hasroom(struct mbuf *m, int len)
{
        int flags;
        int room;

        ASSERT(len >= 0);

        flags = m->m_flags;
        room = 0;

        if ((flags & M_CLUSTER) == 0)
                room = m->m_off - MMINOFF;
        else if (((flags & (M_LOANED|M_SHARED)) == 0) && (*m->m_refp == 1))
                room = mtod(m, caddr_t) - m->m_p;

        return (len <= room);
}

vertex_hdl_t if_hwgraph_netvhdl = GRAPH_VERTEX_NONE;

graph_error_t
if_init_hwgraph(void)
{
	graph_error_t rc;
	if (if_hwgraph_netvhdl != GRAPH_VERTEX_NONE) {
		return GRAPH_SUCCESS;
	}
	rc = hwgraph_path_add(GRAPH_VERTEX_NONE, "net", &if_hwgraph_netvhdl);
	if (rc != GRAPH_SUCCESS) {
		cmn_err(CE_WARN, "couldn't create /hw/net vertex");
		return GRAPH_CANNOT_ALLOC;
	}
	return GRAPH_SUCCESS;
}
	
/*
 * Called to add an alias in /hw/net (for drivers with pre-existing hwgraph
 * support).
 */

graph_error_t
if_hwgraph_alias_add(vertex_hdl_t target, char *alias)
{
	graph_error_t rc;
	if (if_hwgraph_netvhdl == GRAPH_VERTEX_NONE) {
		rc = if_init_hwgraph();
		if (rc != GRAPH_SUCCESS) {
			return rc;
		}
	}
	return hwgraph_edge_add(if_hwgraph_netvhdl, target, alias);
}
#endif

#ifndef IP32
/*
 * Update the driver with the number of bus devices so that we can figure out
 * how many RRBs to grab.
 */
static void
eg_checkbus(struct eg_info *ti)
{
	vertex_hdl_t pcidev;
	int numdevs = 3;
	int slot;
	vertex_hdl_t slothdl;
	char slotbuf[4];
	graph_error_t rc;
#ifdef IP27
	int sn00_base = 0;
#endif

	if (!ti->ti_conn_vhdl) {
		ti->ti_nrrbs = 2;
		return;
	}

	pcidev = device_master_get(ti->ti_conn_vhdl);

	if (pcidev == GRAPH_VERTEX_NONE) {
		ti->ti_nrrbs = 2;
		return;
	}

	numdevs = 0;
	for (slot = 0; slot < 16; slot++) {
		sprintf(slotbuf, "%d", slot);
		rc = hwgraph_traverse(pcidev, slotbuf, &slothdl);
		if (rc == GRAPH_SUCCESS) {
#ifdef IP27
			if (private.p_sn00) {
				if (slot > 4) {
					sn00_base++;
				}
			}
#endif
			numdevs++;
		}
	}

	switch (numdevs) {
	case 0:		/* huh? */
		ti->ti_nrrbs = 2;
		break;
	case 1:
		ti->ti_nrrbs = 6;
		break;
	case 2:
		ti->ti_nrrbs = 4;
		break;
	case 3:
		/* shoebox */
		ti->ti_nrrbs = 3;
		break;
	case 4:
		/* Must be O200 base or O200 XBOX. */
		ti->ti_nrrbs = 3;
		break;
	case 5:
		/* O200 base, but we're the only non-standard thing */
		ti->ti_nrrbs = 3;
		break;
	default:
		/*
		 * must be crowded O200 base
		 * we could be clever and figure out what slot we're in, but
		 * not worth it right now
		 */
		ti->ti_nrrbs = 2;
		break;
	}
}
#endif	/* IP32 */
#endif	/* IP27 || IP30 || IP32 */
