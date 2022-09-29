/*
 * IOC3 PCI Fast Ethernet Driver
 *
 * This driver supports the following known instantiations
 * of the SGI IOC3 Fast Ethernet chip:
 *   - SN0 (O2000) IO6 "baseio" xtalk card
 *   - SN0 (O2000) MENET xtalk card
 *   - SN00 (O200) motherboard
 *   - IP30 (Octane) motherboard
 *   - PCI IOC3 ethernet card
 *
 * Refer to the SGI IOC3 ASIC Specification Revision 4.5 (2/23/96) .
 *
 * Copyright 1995, Silicon Graphics, Inc.  All rights reserved.
 */

#ident "$Revision: 1.73 $"

#include <sys/types.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/mload.h>
#include <sys/errno.h>
#include <sys/hwgraph.h>
#include <sys/invent.h>
#include <sys/iograph.h>
#include <sys/mbuf.h>
#include <sys/kmem.h>
#include <sys/PCI/pciio.h>
#include <sys/PCI/ioc3.h>
#include <sys/PCI/bridge.h>
#include <sys/idbgentry.h>
#include <sys/clksupport.h>
#include <sys/pda.h>
#include <sys/ksa.h>
#if HEART_COHERENCY_WAR || HEART_INVALIDATE_WAR
#include <sys/cpu.h>
#endif
#include <net/if.h>
#include <net/raw.h>
#include <net/soioctl.h>
#include <net/rsvp.h>
#include <ether.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <sys/if_ef.h>
#include <stddef.h>

/*
 * Declarations for variables in master.d/if_ef
 */
extern uint_t	ef_nrbuf10;
extern uint_t	ef_nrbuf100;
extern uint_t	ef_nrbuf10_promisc;
extern uint_t	ef_nrbuf100_promisc;
extern uint_t	ef_rxdelay;
extern uint_t	ef_nrbufthreshdiv;
extern uint_t	ef_halfduplex_ipg[];
extern uint_t	ef_fullduplex_ipg[];
extern uint_t	ef_cksum;
extern struct phyerrata	ef_phyerrata[];

/*
 * Other externs.
 */
extern int	nic_get_eaddr(__uint32_t *mcr, __uint32_t *gpcr_s, char *eaddr);

/*
 * Function Table of Contents
 */
int		if_efopen(dev_t *devp, int flag, int otyp, struct cred *crp);
void		ef_ifattach(struct ef_info *ei, int unit);
void		if_efinit(void); 
int		if_efattach(vertex_hdl_t conn_vhdl);
int		ef_detach(vertex_hdl_t enet_vhdl);
static int	ef_ioc3eaddr(struct ef_info *ei, char *e);
void		ssram_discovery(struct ef_info *ei,
				volatile __uint32_t *ssram);
static int	ef_allocef(struct ef_info *ei);
static void	ef_deallocef(struct ef_info *ei);
static struct mbuf *ef_reclaim(struct ef_info *ei);
static int	ef_xinit(struct etherif *eif, int flags);
static int	ef_init(struct etherif *eif, int flags, int s);
static int	ef_ioc3init(struct ef_info *ei, int flags, int s);
static void	ef_phyunreset(struct ef_info *ei);
static int	ef_phyprobe(struct ef_info *ei);
static int	ef_phymode(struct ef_info *ei, int *speed100, int *fullduplex);
static int	ef_phyreset(struct ef_info *ei);
static int	ef_phyinit(struct ef_info *ei);
static void	ef_phyupdate(struct ef_info *ei, int s);
static void	ef_phyerr(struct ef_info *ei, int s);
static int	ef_phyget(struct ef_info *ei, int reg);
static void	ef_phyput(struct ef_info *ei, int reg, u_short val);
static void	ef_phyerrat(struct ef_info *ei);
static void	ef_phyrmw(struct ef_info *ei, int reg, int mask, int val);
static void	ef_fill(struct ef_info *ei);
static void	ef_xreset(struct etherif *eif);
static struct mbuf *ef_reset(struct etherif *eif);
static void	ef_ioc3reset(struct ef_info *ei);
static void	ef_watchdog(struct ifnet *ifp);
static void	ef_getcdc(struct ef_info *ei);
static void	ef_intr(struct ef_info *ei);
static struct mbuf *ef_recv(struct ef_info *ei);
static int	ef_transmit(struct etherif *eif,
			    struct etheraddr *edst,
			    struct etheraddr *esrc,
			    u_short type, struct mbuf *m0);
static void	ef_selfsnoop(struct ef_info *ei,
			 struct ether_header *eh,
			 struct mbuf *m0, int len);

static int	ef_ioctl(struct etherif *eif, int cmd, void *data);
static int	ef_lafhash(u_char *addr, int len);
#ifdef EFMLOAD
int		if_efunload();
static int	ef_reload(struct ifnet *ifp);
#endif
static void	ef_dump(int unit);
static void	ef_dumpif(struct ifnet *ifp);
static void	ef_dumpei(struct ef_info *ei);
static void	ef_dumpregs(struct ef_info *ei);
static void	ef_dumpphy(struct ef_info *ei);
static void	ef_hexdump(char *msg, char *cp, int len);

#define	W_REG(reg,val)		(reg) = (val)
#define R_REG(reg)		(reg)
#define AND_REG(reg,val) 	(reg) &= (val)
#define OR_REG(reg,val) 	(reg) |= (val)

/*
 * Misc defines.
 */

#define DBG_PRINT(a)		if (ei->ei_if.if_flags & IFF_DEBUG) printf a

#if HEART_INVALIDATE_WAR
#define	CACHE_INVAL(addr, len)	heart_invalidate_war((addr),(len))
#else
#define CACHE_INVAL(addr, len)
#endif	/* HEART_INVALIDATE_WAR */

#if HEART_COHERENCY_WAR
#define CACHE_WB(addr, len)     heart_dcache_wb_inval((addr),(len))
#else
#define	CACHE_WB(addr, len)
#endif	/* HEART_COHERENCY_WAR */

#define	 KVTOIOADDR_CMD(ei, addr)	pciio_dmatrans_addr((ei)->ei_conn_vhdl, 0, \
		kvtophys((caddr_t)(addr)), sizeof(int), PCIIO_DMA_A64 | PCIIO_DMA_CMD)
#define	KVTOIOADDR_DATA(ei, addr)	pciio_dmamap_addr((ei)->ei_fastmap, \
		kvtophys((caddr_t)(addr)), sizeof (int))

/* get sysinfo for a given cpu */
#define cpuid_to_sysinfo(cpuid) (&pdaindr[cpuid].pda->ksaptr->si)

/*
 * Non-rx, non-tx, "other" interrupt conditions
 */
#define	EISR_OTHER \
	(EISR_RXOFLO | EISR_RXBUFOFLO | EISR_RXMEMERR | EISR_RXPARERR | \
	EISR_TXRTRY | EISR_TXEXDEF | EISR_TXLCOL | EISR_TXGIANT | \
	EISR_TXBUFUFLO | EISR_TXCOLLWRAP | EISR_TXDEFERWRAP | \
	EISR_TXMEMERR | EISR_TXPARERR | EISR_TXEXPLICIT)

#define	NEXTTXD(i)	(((i)+1) & (NTXDBIG - 1))
#define	NEXTRXD(i)	(((i)+1) & (NRXD - 1))
#define	DELTATXD(h, t)	((h - t) & (NTXDBIG - 1))
#define	DELTARXD(h, t)	((t - h) & (NRXD - 1))

/*
 * Since the ioc3 compares the rx consume and produce indices
 * modulo-16, we increase number of receive buffers by 15.
 */
#define	NRBUFADJ(n)	(n + 15)

#define	TXDBUFSZ	(ETXD_DATALEN - sizeof (struct ether_header))

#define	ETHER_HDRLEN	(sizeof (struct ether_header))
#define	ETHER_CRCSZ	(4)			/* # bytes ethernet crc */

#define	EFDOG		(IFNET_SLOWHZ)		/* watchdog timeout */

#define	ALIGNED(a, n)	(((u_long)(a) & ((n)-1)) == 0)
#define	MALIGN2(m)	ALIGNED(mtod((m), u_long), 2)
#define	MALIGN128(m)	ALIGNED(mtod((m), u_long), 128)
#define	MLEN2(m)	(((m)->m_len > 0) && ((m)->m_len & 1) == 0)

#define LAF_TSTBIT(laf, bit) \
	((laf)->laf_vec[(bit) >> 3] & (1 << ((bit) & 0x7)))
#define LAF_SETBIT(laf, bit) \
	((laf)->laf_vec[(bit) >> 3] |= (1 << ((bit) & 0x7)))
#define LAF_CLRBIT(laf, bit) \
	((laf)->laf_vec[(bit) >> 3] &= ~(1 << ((bit) & 0x7)))

char	*if_efmversion = M_VERSION;
int	if_efdevflag = D_MP;

static struct efrxbuf ef_dummyrbuf;

/*
 * Opsvec for ether.[ch] interface.
 */
static struct etherifops efops =
	{ ef_xinit, ef_xreset, ef_watchdog, ef_transmit, ef_ioctl };

/*
 * Called by packet scheduling functions to determine length of driver queue.
 */
static int
ef_txfree_len(struct ifnet *ifp)
{
	struct ef_info *ei = eiftoei(ifptoeif(ifp));
	int s;
	struct mbuf *m, *mf;

	s = mutex_bitlock(&ei->ei_flags, EIF_LOCK);
	mf = ef_reclaim(ei);
	mutex_bitunlock(&ei->ei_flags, EIF_LOCK, s);

	/* free any reclaimed packets */
	while (mf) {
		m = mf;
		mf = mf->m_act;
		m_freem(m);
	}

	return (NTXDBIG - DELTATXD(ei->ei_txhead, ei->ei_txtail));
}

/*
 * Called by packet scheduling functions to notify driver about queueing state.
 * If setting is 1 then there are queues and driver should try to minimize
 * delay (ie take intr per packet).  If 0 then driver can optimize for
 * throughput (ie. take as few intr as possible).
 */
static void
ef_setstate(struct ifnet *ifp, int setting)
{
	struct ef_info *ei = eiftoei(ifptoeif(ifp));

	if (setting)
		ei->ei_flags |= EIF_PSENABLED;
	else
		ei->ei_flags &= ~EIF_PSENABLED;
}

/*
 * Register driver parameters with the RSVP packet scheduler.
 */
static void
ef_ps_register(struct ef_info *ei, int reinit)
{
	struct ps_parms ps_params;

	if (ei->ei_speed100) {
		ps_params.bandwidth = 100000000;
		eiftoifp(&ei->ei_eif)->if_baudrate.ifs_value = 1000*1000*100;
		ei->ei_intfreq = 0xf;
	} else {
		ps_params.bandwidth = 10000000;
		eiftoifp(&ei->ei_eif)->if_baudrate.ifs_value = 1000*1000*10;
		ei->ei_intfreq = 1;
	}
	ps_params.flags = 0;
	ps_params.txfree = NTXDBIG;
	ps_params.txfree_func = ef_txfree_len;
	ps_params.state_func = ef_setstate;
	if (reinit)
		(void)ps_reinit(eiftoifp(&ei->ei_eif), &ps_params);
	else
		(void)ps_init(eiftoifp(&ei->ei_eif), &ps_params);
}

#define	ef_info_set(v,i)	hwgraph_fastinfo_set((v),(arbitrary_info_t)(i))
#define	ef_info_get(v)		((struct ef_info *)hwgraph_fastinfo_get((v)))

/*
 * Driver xxopen() routine exists only to take unit# which
 * has now been assigned to the vertex by ioconfig
 * and if_attach() the device.
 */
/* ARGSUSED */
int
if_efopen(dev_t *devp, int flag, int otyp, struct cred *crp)
{
	vertex_hdl_t enet_vhdl;
	struct ef_info *ei;
	int unit;

	enet_vhdl = dev_to_vhdl(*devp);

	if ((ei = ef_info_get(enet_vhdl)) == NULL)
		return (EIO);

	/* if already if_attached, just return */
	if (ei->ei_unit != -1)
		return (0);

	if ((unit = device_controller_num_get(enet_vhdl)) < 0) {
		cmn_err(CE_ALERT, "ef_open: vertex missing ctlr number");
		return (EIO);
	}

	ef_ifattach(ei, unit);

	return (0);
}

void
ef_ifattach(struct ef_info *ei, int unit)
{
	ei->ei_unit = unit;

	if (ei->ei_enet_vhdl != GRAPH_VERTEX_NONE) {
		char edge_name[8];
		sprintf(edge_name, "%s%d", EDGE_LBL_EF, unit);
		(void) if_hwgraph_alias_add(ei->ei_enet_vhdl, edge_name);
	}

	ether_attach(&ei->ei_eif, "ef", ei->ei_unit, (caddr_t)ei, &efops, &ei->ei_addr, INV_ETHER_EF, ei->ei_chiprev);
	ei->ei_if.if_flags |= IFF_DRVRLOCK;

	ef_ps_register(ei, 0);
}

void
if_efinit(void)
{
#if DEBUG && ATTACH_DEBUG
	cmn_err(CE_CONT, "if_efinit()\n");
#endif
	pciio_driver_register(IOC3_VENDOR_ID_NUM,
			      IOC3_DEVICE_ID_NUM,
			      "if_ef",
			      0);
}

int
if_efattach(vertex_hdl_t conn_vhdl)
{
	graph_error_t	rc;
	struct ef_info	*ei = NULL;
	struct etheraddr ea;
	vertex_hdl_t ioc3_vhdl;
	vertex_hdl_t enet_vhdl;
	volatile __uint32_t *ssram;
	device_desc_t ef_dev_desc;
	ioc3_cfg_t      *cfg;

	if (!ioc3_subdev_enabled(conn_vhdl, ioc3_subdev_ether))
		return -1;	/* not brought out to a connector */

#if DEBUG && ATTACH_DEBUG
	cmn_err(CE_CONT, "%v: if_efattach()\n", conn_vhdl);
#endif

	/*
	 * If our IOC3 is in dual-slot mode, we
	 * want to connect to the HOST vertex.
	 * If the HOST edge does not exist, this
	 * leaves conn_vhdl unmodified.
	 */
	hwgraph_traverse(conn_vhdl, EDGE_LBL_HOST, &conn_vhdl);

	/* add the ethernet driver vertex to the graph */
	if ((rc = hwgraph_char_device_add(conn_vhdl, EDGE_LBL_EF, "if_ef", &enet_vhdl))
		!= GRAPH_SUCCESS)
		cmn_err(CE_ALERT, "if_efattach: hwgraph_char_device_add error %d", rc);

	/* allocate our private per-device data structure */
	if ((ei = (struct ef_info*) kmem_zalloc(sizeof (struct ef_info), KM_SLEEP)) == NULL)
		goto bad;

	ei->ei_conn_vhdl = conn_vhdl;
	ei->ei_enet_vhdl = enet_vhdl;
	ef_info_set(enet_vhdl, ei);

	/* map in ethernet registers */
	if ((ei->ei_regs = (ioc3_eregs_t*) pciio_piotrans_addr(conn_vhdl, 0, PCIIO_SPACE_WIN(0),
		IOC3_REG_OFF, sizeof (ioc3_eregs_t), 0)) == NULL)
		goto bad;

	/* map in all ioc3 registers */
	if ((ei->ei_ioc3regs = (ioc3_mem_t*) pciio_piotrans_addr(conn_vhdl, 0, PCIIO_SPACE_WIN(0),
		0, sizeof (ioc3_mem_t), 0)) == NULL)
		goto bad;

	/* locate the ioc3 vertex */
	rc = hwgraph_traverse(conn_vhdl, "ioc3", &ioc3_vhdl);
	if (rc != GRAPH_SUCCESS) {
		cmn_err(CE_ALERT, "if_efattach(%v): unable to find ioc3 vertex",conn_vhdl);
		goto bad;
	}
	ei->ei_ioc3_vhdl = ioc3_vhdl;

	/* fix up device descriptor */
	ef_dev_desc = device_desc_dup(enet_vhdl);
	device_desc_intr_name_set(ef_dev_desc, "IOC3 Ethernet");
	device_desc_default_set(enet_vhdl,ef_dev_desc);

	/* map in ssram for ssram_discovery() */
	if ((ssram = (volatile __uint32_t *) pciio_piotrans_addr(conn_vhdl, 0, PCIIO_SPACE_WIN(0),
		IOC3_RAM_OFF, (128 * 1024), 0)) == NULL)
		goto bad;

	ssram_discovery(ei, ssram);

        /* map in ioc3 config registers to determine chip rev */
        if ((cfg = (ioc3_cfg_t *) pciio_piotrans_addr(conn_vhdl, 0, PCIIO_SPACE_CFG,
                0, sizeof (*cfg), 0)) == NULL)
                goto bad;
        ei->ei_chiprev = cfg->pci_rev & 0xff;

	/* we don't know our unit# yet */
	ei->ei_unit = -1;

	ef_phyunreset(ei);

	ef_ioc3reset(ei);

	if (ef_ioc3eaddr(ei, (char*)ea.ea_vec))
		goto bad;
	bcopy(ea.ea_vec, ei->ei_addr.ea_vec, 6);
	bcopy(ea.ea_vec, ei->ei_curaddr, 6);

	if (showconfig)
		cmn_err(CE_CONT,"%v: hardware ethernet address %s\n",
			enet_vhdl, ether_sprintf(ea.ea_vec));

	if (ef_allocef(ei))
		goto bad;

	ei->ei_speed100 = -1;
	ei->ei_fullduplex = -1;
	ei->ei_phyunit = -1;

	/* register our interrupt routine */
	ei->ei_intr = pciio_intr_alloc(conn_vhdl, ef_dev_desc,
				       PCIIO_INTR_LINE_A, enet_vhdl);
	ASSERT(ei->ei_intr);
	pciio_intr_connect(ei->ei_intr, (intr_func_t) ef_intr, (intr_arg_t) ei, (void *)0);

	/* create a fast pciio dmamap */
	if ((ei->ei_fastmap = pciio_dmamap_alloc(conn_vhdl, ef_dev_desc, sizeof (int),
		PCIIO_DMA_DATA | PCIIO_DMA_A64 | PCIIO_FIXED)) == NULL) {
		cmn_err(CE_ALERT, "%v: pciio_dmamap_alloc() failed!", enet_vhdl);
		goto bad;
	}

	device_inventory_add(enet_vhdl, INV_NETWORK, INV_NET_ETHER, INV_ETHER_EF, -1, ei->ei_chiprev);

	idbg_addfunc("ef_dump", (void (*)())ef_dump);

	init_bitlock(&ei->ei_flags, EIF_LOCK, "ef drv lock", 0);

	return (0);

bad:
	ef_detach(enet_vhdl);
	return (ENOMEM);
}

int
ef_detach(vertex_hdl_t enet_vhdl)
{
	struct ef_info *ei;

	idbg_delfunc((void (*)())ef_dump);

	if (enet_vhdl == GRAPH_VERTEX_NONE)
	    return 0;

	ei = ef_info_get(enet_vhdl);
	if (ei == NULL)
	    return 0;

	/* ether_detach() would go here */

	ef_info_set(enet_vhdl, NULL);
	ef_deallocef(ei);

	/* pciio_piountrans_addr() of ei_regs and ei_ssram would go here */

	kmem_free((caddr_t) ei, sizeof (struct ef_info));

	return (0);
}

static int
ef_ioc3eaddr(struct ef_info *ei,
	char *e)
{
	if (nic_get_eaddr((__uint32_t*)&ei->ei_ioc3regs->mcr,
		(__uint32_t*)&ei->ei_ioc3regs->gpcr_s, e)) {
		cmn_err(CE_ALERT, "%v: nic_get_eaddr failed", ei->ei_conn_vhdl);
		return (EIO);
	}

	return (0);
}

#define	PATTERN	0x5555
void
ssram_discovery(struct ef_info *ei,
	volatile __uint32_t *ssram)
{
	volatile __uint32_t *ssram_addr0 = &ssram[0];
	volatile __uint32_t *ssram_addr1 =
		&ssram[IOC3_SSRAM_LEN / (2 * sizeof(__uint32_t))];

	/*
	 * the ssram can be either 64kb or 128kb so we use
	 * a write-write-read sequence to find out which.
	 */


	/* assume the larger size SSRAM and enable parity checking */
        OR_REG(ei->ei_regs->emcr,EMCR_BUFSIZ | EMCR_RAMPAR);

        W_REG(*ssram_addr0, PATTERN);
        W_REG(*ssram_addr1, (~PATTERN & IOC3_SSRAM_DM));

        if ( (R_REG(*ssram_addr0) & IOC3_SSRAM_DM) != PATTERN ||
             (R_REG(*ssram_addr1) & IOC3_SSRAM_DM) != (~PATTERN & IOC3_SSRAM_DM)) {
		/* set ssram size to 64 KB */
		ei->ei_ssram_bits = EMCR_RAMPAR;
                AND_REG(ei->ei_regs->emcr, ~EMCR_BUFSIZ);
        } else
                ei->ei_ssram_bits = EMCR_BUFSIZ | EMCR_RAMPAR;

}

/*
 * Allocate ef_info fields.  Return 0 or errno.
 */
static int
ef_allocef(struct ef_info *ei)
{
	int	size;
        int	npgs;
        pgno_t	pgno;

	if (ei->ei_txd)
		return 0;

	/* allocate transmit descriptor ring.
	 * must be 16k aligned for small (128 entry) ring
	 * must be 64k aligned for large (512 entry) ring
	 * we align to 64k for now...
	 */
	size = NTXDBIG * TXDSZ;
	npgs = (size+NBPP-1)/NBPP;
	if ((pgno = contig_memalloc(npgs, ETBR_ALIGNMENT/NBPP, VM_DIRECT)) == NULL)
		goto fail;
	ei->ei_txd = (struct eftxd*)small_pfntova_K0(pgno);

	/* allocate transmit pending-mbuf vector */
	size = NTXDBIG * sizeof (struct mbuf*);
	if ((ei->ei_txm = (struct mbuf**) kmem_zalloc(size, KM_SLEEP)) == NULL)
		goto fail;

	/* allocate receive descriptor ring, must be 4k aligned */
	size = NRXD * RXDSZ;
	npgs = (size+NBPP-1)/NBPP;
	if ((pgno = contig_memalloc(npgs, 1, VM_DIRECT)) == NULL)
		goto fail;
	ei->ei_rxd = (__uint64_t*)small_pfntova_K0(pgno);

	/* allocate receive pending-mbuf vector */
	size = NRXD * sizeof (struct mbuf*);
	if ((ei->ei_rxm = (struct mbuf**) kmem_zalloc(size, KM_SLEEP)) == NULL)
		goto fail;

	return (0);

fail:
	/* can be called even if only some were successful */
	ef_deallocef(ei);

	return (ENOMEM);
}

/*
 * Free ef_info fields prior to unloading.
 */
static void
ef_deallocef(struct ef_info *ei)
{
	int	size;

	size = NTXDBIG * TXDSZ;
	if (ei->ei_txd)
          ;			/* XXX no contig_memalloc_free yet */

	size = NTXDBIG * sizeof (struct mbuf*);
	if (ei->ei_txm)
          kmem_free((void*)ei->ei_txm, size);

	size = NRXD * RXDSZ;
	if (ei->ei_rxd)
          ;			/* XXX no contig_memalloc_free yet */

	size = NRXD * sizeof (struct mbuf*);
	if (ei->ei_rxm)
	  kmem_free((void*)ei->ei_rxm, size);

}

/*
 * Reclaim all the completed tx descriptors/mbufs.
 */
static struct mbuf *
ef_reclaim(struct ef_info *ei)
{
	int	i;
	int	consume;
	struct ifqueue q;

	ASSERT(ei->ei_flags & EIF_LOCK);

	consume = (R_REG(ei->ei_regs->etcir) & ETCIR_TXCONSUME_MASK)/TXDSZ;

	ASSERT(consume < NTXDBIG);

	q.ifq_head = NULL;
	q.ifq_tail = NULL;

	for (i = ei->ei_txtail; i != consume; i = NEXTTXD(i)) {
		GOODMP(ei->ei_txm[i]);
		IF_ENQUEUE_NOLOCK(&q, ei->ei_txm[i]);
		ei->ei_txm[i] = NULL;
	}

	ei->ei_txtail = i;
	return (q.ifq_head);
}

/*
 * Wrapper for ef_init - just acquires the private driver bitlock
 * before calling ef_init().
 */
static int
ef_xinit(struct etherif *eif,
	int flags)
{
	struct ef_info *ei;
	int rc;
	int s;

	ei = eiftoei(eif);

	s = mutex_bitlock(&ei->ei_flags, EIF_LOCK);
	rc = ef_init(eif, flags, s);
	mutex_bitunlock(&ei->ei_flags, EIF_LOCK, s);

	return (rc);
}

/*
 * Initialize the interface.  Interrupts must already be off.
 * Return 0 or errno.
 */
static int
ef_init(struct etherif *eif,
	int flags,
	int s)
{
	struct ef_info *ei;
	int rc=0;
	struct mbuf *mf, *m;

	ei = eiftoei(eif);

	ASSERT(ei->ei_flags & EIF_LOCK);

	/* reset the chip */
	mf = ef_reset(eif);

	/* clear transmit descriptor ring. */
	bzero((caddr_t) ei->ei_txd, NTXDBIG * TXDSZ);
	ei->ei_txhead = ei->ei_txtail = 0;

	/* clear receive descriptor ring. */
	bzero((caddr_t) ei->ei_rxd, NRXD * RXDSZ);
	ei->ei_rxhead = ei->ei_rxtail = 0;

	/* IOC3 initialization */
	if (rc = ef_ioc3init(ei, flags, s))
		goto done;

	/*
	 * Only call ps_reinit if ps_init has been called.
	 */
	if (ei->ei_unit != -1)
		ef_ps_register(ei, 1);

	ei->ei_flags = (ei->ei_flags & ~EIF_CKSUM_MASK) | (ef_cksum & EIF_CKSUM_MASK);

        /*
         * The ioc3 rev 0 has a hw bug in the tx ip checksum logic -
         * it fails to take the *one's complement* of the sum
         * before stuffing it into the header checksum field.
         */
        if (ei->ei_chiprev < 1)
                ei->ei_flags &= ~EIF_CKSUM_TX;

	if (ei->ei_flags & EIF_CKSUM_TX)
		ei->ei_if.if_flags |= IFF_CKSUM;
	else
		ei->ei_if.if_flags &= ~IFF_CKSUM;

	/* post receive buffers */
	ef_fill(ei);

	/* arm the rx interrupt */
	W_REG(ei->ei_regs->erpir, (ERPIR_ARM | (ei->ei_rxtail * RXDSZ)));

	/* set quick pointer to head receive buffer */
	if (ei->ei_rxm[ei->ei_rxhead])
		ei->ei_rb = mtod(ei->ei_rxm[ei->ei_rxhead], struct efrxbuf*);
	else
		ei->ei_rb = &ef_dummyrbuf;

	/* enable IOC3 tx and rx */
	OR_REG(ei->ei_regs->emcr, EMCR_TXEN | EMCR_TXDMAEN | EMCR_RXEN | EMCR_RXDMAEN);

	ei->ei_if.if_timer = EFDOG;	/* turn on watchdog */

done:
	mutex_bitunlock(&ei->ei_flags, EIF_LOCK, s);
	/* free any reclaimed packets */
	while (mf) {
		m = mf;
		mf = mf->m_act;
		m_freem(m);
	}
	s = mutex_bitlock(&ei->ei_flags, EIF_LOCK);

	return rc;
}

static int
ef_ioc3init(struct ef_info *ei,
	int flags,
	int s)
{
	ioc3_eregs_t	*regs = ei->ei_regs;
	iopaddr_t ioaddr;
	__uint32_t	w, val;
	__uint32_t off;
	struct timeval tv;
	int	rc;

	/* phy initialization first */
	if (rc = ef_phyinit(ei))
		return (rc);

	/* check phy for errors */
	ef_phyerr(ei, s);

	/* determine number of rbufs to post */
	if (flags & IFF_PROMISC)
		ei->ei_nrbuf = ei->ei_speed100? ef_nrbuf100_promisc: ef_nrbuf10_promisc;
	else
		ei->ei_nrbuf = ei->ei_speed100? ef_nrbuf100: ef_nrbuf10;
	ei->ei_nrbuf = NRBUFADJ(ei->ei_nrbuf);

	if (ei->ei_speed100) {
		eiftoifp(&ei->ei_eif)->if_baudrate.ifs_value = 1000*1000*100;
	} else {
		eiftoifp(&ei->ei_eif)->if_baudrate.ifs_value = 1000*1000*10;
	}
	if (ei->ei_fullduplex) {
		eiftoifp(&ei->ei_eif)->if_flags |= IFF_LINK0;
	} else {
		eiftoifp(&ei->ei_eif)->if_flags &= ~IFF_LINK0;
	}
	if (ei->ei_fullduplex)
		w = (ef_fullduplex_ipg[2] << ETCSR_IPGR2_SHIFT)
			| (ef_fullduplex_ipg[1] << ETCSR_IPGR1_SHIFT)
			| ef_fullduplex_ipg[0];
	else
		w = (ef_halfduplex_ipg[2] << ETCSR_IPGR2_SHIFT)
			| (ef_halfduplex_ipg[1] << ETCSR_IPGR1_SHIFT)
			| ef_halfduplex_ipg[0];
	W_REG(regs->etcsr, w);

	/* set random seed register */
	microtime(&tv);
	W_REG(regs->ersr, tv.tv_usec);

        /* set receive barrier register */
#if SN0 || IP30
        W_REG(regs->erbar, (int)(PCI64_ATTR_BAR >> 32));
#else
	W_REG(regs->erbar, 0);
#endif

        /* set receive interrupt threshold */
        W_REG(regs->ercsr, ((ei->ei_nrbuf / ef_nrbufthreshdiv) & ERCSR_THRESH_MASK));

        /* set receive interrupt delay */
        W_REG(regs->ertr, 0);

        /* set our individual ethernet address */
        w = (ei->ei_curaddr[3] << 24) | (ei->ei_curaddr[2] << 16)
                | (ei->ei_curaddr[1] << 8) | ei->ei_curaddr[0];
        W_REG(regs->emar_l, w);
        w = (ei->ei_curaddr[5] << 8) | ei->ei_curaddr[4];
        W_REG(regs->emar_h, w);

	/* set the multicast filter */
	if (flags & IFF_ALLMULTI) {
		W_REG(regs->ehar_l,0xffffffff);
		W_REG(regs->ehar_h, 0xffffffff);
	} else {
		w = (ei->ei_laf.laf_vec[3] << 24) | (ei->ei_laf.laf_vec[2] << 16)
		| (ei->ei_laf.laf_vec[1] << 8) | ei->ei_laf.laf_vec[0];
		W_REG(regs->ehar_l, w);
		w = (ei->ei_laf.laf_vec[7] << 24) | (ei->ei_laf.laf_vec[6] << 16)
                | (ei->ei_laf.laf_vec[5] << 8) | ei->ei_laf.laf_vec[4];
                W_REG(regs->ehar_h, w);
	}

	/* set the receive ring base address */
	ioaddr = KVTOIOADDR_CMD(ei, ei->ei_rxd);
        W_REG(regs->erbr_l, (__uint32_t) (ioaddr & 0xffffffff));
        W_REG(regs->erbr_h, (__uint32_t) ((ioaddr & 0xffffffff00000000LL) >> 32));

        /* set the transmit ring base address */
	ioaddr = KVTOIOADDR_CMD(ei, ei->ei_txd);
	W_REG(regs->etbr_l, (__uint32_t) (ioaddr & 0xffffffff));
        W_REG(regs->etbr_h, (__uint32_t) ((ioaddr & 0xffffffff00000000LL) >> 32));

        /* set the transmit ring size */
	OR_REG(regs->etbr_l, ETBR_L_RINGSZ512);

        /*
         * Initialize the MAC CSR register to reflect the
         * phy autonegotiated values and previously-determined
         * sram parity and sram size bits.
         * Leave the tx and rx channel bits OFF for now.
         */

	w = 0;

	if (ei->ei_fullduplex)
		w |= EMCR_DUPLEX;

        if (flags & IFF_PROMISC)
                w |= EMCR_PROMISC;

        w |= EMCR_PADEN;

	/* set rx offset in halfwords */
	off = offsetof(struct efrxbuf,rb_ebh.ebh_ether) / 2;
        w |= (off << EMCR_RXOFF_SHIFT);

        w |= ei->ei_ssram_bits;

        W_REG(regs->emcr, w);

	/*
	 * enable all interrupts except TXEMPTY.
	 */

        val = EISR_RXTIMERINT | EISR_RXTHRESHINT | EISR_RXOFLO
                | EISR_RXBUFOFLO | EISR_RXMEMERR | EISR_RXPARERR
                | EISR_TXRTRY | EISR_TXEXDEF | EISR_TXLCOL
                | EISR_TXGIANT | EISR_TXBUFUFLO | EISR_TXEXPLICIT
                | EISR_TXCOLLWRAP | EISR_TXDEFERWRAP | EISR_TXMEMERR
                | EISR_TXPARERR;

	W_REG(regs->eier, val);

	return (0);
}

static void
ef_phyunreset(struct ef_info *ei)
{
	volatile ioc3reg_t dummy;

	/*
	 * IOC3 general purpose i/o pin 5 is ANDed with the system reset signal,
	 * inverted, and connected to the PHY chip reset input.
	 *
	 * The order is important:
	 * - write 0 to IOC3 gppr_5 (0=reset,1=nonreset)
	 * - write to IOC3 gpcr to configure tristate pin 5 to be output
	 * - delay a few microseconds to ensure reset
	 * - write 1 to IOC3 gppr_5 to take it out of reset
	 * - delay 500us before attempting another MII operation
	 *
	 * Speedo is different:
	 * The speedo motherboard IOC3 has the HUB MD_LED0 pin connected
	 * to the PHY reset input.  Code to take the speedo motherboard ethernet
	 * out of reset is in ml/SN0 sn00_vmc() .
	 */

	W_REG(ei->ei_ioc3regs->gppr_5, 0);
	dummy = R_REG(ei->ei_ioc3regs->gppr_5);

	W_REG(ei->ei_ioc3regs->gpcr_s, GPCR_PHY_RESET);
	dummy = R_REG(ei->ei_ioc3regs->gpcr_s);
	DELAY(10);

	W_REG(ei->ei_ioc3regs->gppr_5, 1);
	dummy = R_REG(ei->ei_ioc3regs->gppr_5);
	DELAY(500);
}

static int
ef_phyprobe(struct ef_info *ei)
{
	int i;
	int val;
	int r2, r3;

	if (ei->ei_phyunit != -1)
		return (ei->ei_phytype);

	for (i = 0; i < 32; i++) {
		ei->ei_phyunit = i;
		r2 = ef_phyget(ei, 2);
		r3 = ef_phyget(ei, 3);
		val = (r2 << 12) | (r3 >> 4);
		switch (val) {
		case PHY_QS6612X:
		case PHY_ICS1889:
		case PHY_ICS1890:
		case PHY_ICS1892:
		case PHY_DP83840:
			ei->ei_phyrev = r3 & 0xf;
			ei->ei_phytype = val;
			return (val);
		}
	}

	ei->ei_phyunit = -1;

	cmn_err(CE_ALERT, "ef%d: PHY not found", ei->ei_unit);

	return (-1);
}

/*
 * Determine the current phy speed and duplex.
 * Return 0 on success, EBUSY if autonegotiation hasn't completed.
 */
static int
ef_phymode(struct ef_info *ei,
	int *speed100,
	int *fullduplex)
{
	int r0, r1, r4, r5, r6;
	int v;

	r0 = ef_phyget(ei, 0);
	r1 = ef_phyget(ei, 1);
	r6 = ef_phyget(ei, 6);

	/* If hardwired speed/duplex */
	if ((r0 & MII_R0_AUTOEN) == 0) {
		*speed100 = (r0 & MII_R0_SPEEDSEL)? 1: 0;
		*fullduplex = (r0 & MII_R0_DUPLEX)? 1: 0;
		return (0);
	}

	/* If AN not yet complete */
	if ((r1 & MII_R1_AUTODONE) == 0) {
		if (ei->ei_phytype == PHY_ICS1890) {
		/* ICS1890 WAR - refer to rev J version 3 release note */
                /* ICS1890 with legacy 100Base-TX partner(no AN capability)
		** may prevent AN from completing. Corrected by disabling,
 		** then re-enabling AN. */
                        v = ef_phyget(ei, 17);
			v &= ICS1890_R17_PD_MASK;
                        if ( v == ICS1890_R17_PD_FAULT ) {
                                ef_phyput(ei, 0, ~MII_R0_AUTOEN);
                                ef_phyput(ei, 0, MII_R0_AUTOEN);
                        }
                }
		*speed100 = -1;
		*fullduplex = -1;
		return (EBUSY);
	}

	/* If remote not capable of AN */
	if ((r6 & MII_R6_LPNWABLE) == 0) {
		switch (ei->ei_phytype) {
		case PHY_DP83840:
			*speed100 = !(ef_phyget(ei, 25) & DP83840_R25_SPEED_10);
			*fullduplex = 0;
			break;
		case PHY_ICS1889:
		case PHY_ICS1890:
		case PHY_ICS1892:   
			v = ef_phyget(ei, 17);
			*speed100 = (v & ICS1890_R17_SPEED_100)? 1:0;
			*fullduplex = (v & ICS1890_R17_FULL_DUPLEX)? 1:0;
			break;
		case PHY_QS6612X:
			v = ef_phyget(ei, 31);
			switch (v & QS6612X_R31_OPMODE_MASK) {
			case QS6612X_R31_10:
				*speed100 = 0;
				*fullduplex = 0;
				break;
			case QS6612X_R31_100:
				*speed100 = 1;
				*fullduplex = 0;
				break;
			case QS6612X_R31_10FD:
				*speed100 = 0;
				*fullduplex = 1;
				break;
			case QS6612X_R31_100FD:
				*speed100 = 1;
				*fullduplex = 1;
				break;
			}
		}

		return (0);
	}

	r4 = ef_phyget(ei, 4);
	r5 = ef_phyget(ei, 5);

	if ((r4 & MII_R4_TXFD) && (r5 & MII_R5_TXFD)) {
		*speed100 = 1;
		*fullduplex = 1;
	}
	else if ((r4 & MII_R4_TX) && (r5 & MII_R5_TX)) {
		*speed100 = 1;
		*fullduplex = 0;
	}
	else if ((r4 & MII_R4_10FD) && (r5 & MII_R5_10FD)) {
		*speed100 = 0;
		*fullduplex = 1;
	}
	else if ((r4 & MII_R4_10) && (r5 & MII_R5_10)) {
		*speed100 = 0;
		*fullduplex = 0;
	}

	return (0);
}

static int
ef_phyreset(struct ef_info *ei)
{
	if (ef_phyprobe(ei) == -1)
		return (-1);

	/* XXX No longer actually reset the PHY chip */

	/* apply any phy-specific workarounds */
	ef_phyerrat(ei);

	return (0);
}

static int
ef_phyinit(struct ef_info *ei)
{
	int timeout;
	int unit;

	unit = ei->ei_unit;

	if (ef_phyreset(ei) != 0)
		return (EIO);

	timeout = 20000;
	while (ef_phymode(ei, &ei->ei_speed100, &ei->ei_fullduplex) == EBUSY) {
		DELAY(100);
		if (timeout-- <= 0)
			break;
	}

	/*
	 * Eat transient link fail that happens about 500us after Autodone.
	 */
	DELAY(1000);
	(void) ef_phyget(ei, 1);

	/*
	 * If autonegotiation didn't complete then assume 10 mbit half duplex.
	 */
	if ((ei->ei_speed100 == -1) || (ei->ei_fullduplex == -1)) {
		DBG_PRINT(("ef%d: auto-negotiation did not complete\n", unit));
		ef_phyput(ei, 0, MII_R0_AUTOEN);
		ei->ei_speed100 = 0;
		ei->ei_fullduplex = 0;
	}

	DBG_PRINT(("ef%d: %d Mbits/s %sduplex\n",
		ei->ei_unit, ei->ei_speed100? 100: 10, ei->ei_fullduplex? "full": "half"));

	return (0);
}

static void
ef_phyupdate(struct ef_info *ei, int s)
{
	int speed100, fullduplex;

	/* If the phy changes mode, re-init */
	if (ef_phymode(ei, &speed100, &fullduplex) == 0)
		if ((speed100 != ei->ei_speed100) || (fullduplex != ei->ei_fullduplex))
			(void) ef_init(&ei->ei_eif, ei->ei_if.if_flags, s);
}

/*
 * Check PHY register 1 for errors.
 *
 * We maintain a last-known-value for RemoteFault, Jabber,
 * and LinkStatus and use this to detect a change in the state.
 * We don't want to check the link status *during* phy
 * initialization, only afterwards.
 */
static void
ef_phyerr(struct ef_info *ei, int s)
{
	int unit;
	int reg1;

	unit = ei->ei_unit;
	reg1 = ef_phyget(ei, 1);

	if (reg1 & MII_R1_REMFAULT) {
		if (ei->ei_remfaulting == 0)
			cmn_err(CE_WARN, "ef%d: remote hub fault", unit);
		ei->ei_remfaulting = 1;
	} else {
		if (ei->ei_remfaulting)
			DBG_PRINT(("ef%d: remote hub ok\n", unit));
		ei->ei_remfaulting = 0;
	}

	if ((reg1 & MII_R1_JABBER) &&
	  !(ei->ei_flags & EIF_LINKDOWN) && !ei->ei_speed100) {
		if (ei->ei_jabbering == 0)
			cmn_err(CE_WARN, "ef%d: jabbering", unit);
		ei->ei_jabbering = 1;
	} else {
		if (ei->ei_jabbering)
			DBG_PRINT(("ef%d: jabber ok\n", unit));
		ei->ei_jabbering = 0;
	}

	if (reg1 & MII_R1_LINKSTAT) {
		if (ei->ei_flags & EIF_LINKDOWN) {
			cmn_err(CE_NOTE, "ef%d: link ok", unit);
			ei->ei_flags &= ~EIF_LINKDOWN;
			ef_phyupdate(ei, s);
		}
		ei->ei_flags &= ~EIF_LINKDOWN;
	} else {
		if (!(ei->ei_flags & EIF_LINKDOWN))
			cmn_err(CE_WARN, "ef%d: link fail - check ethernet cable", unit);
		ei->ei_flags |= EIF_LINKDOWN;
	}
}

/*
 * Read a PHY register.
 *
 * Several error bits in register 1 are read-clear.
 * We don't care about these bits until after phy
 * initialization is over, then we explicit test for
 * them.
 */
static int
ef_phyget(struct ef_info *ei,
	int reg)
{
	ioc3_eregs_t	*regs = ei->ei_regs;

	/* XXX infinite loop on busy for now */
	while (regs->micr & MICR_BUSY) {
		DELAYBUS(1);
	}
        regs->micr = MICR_READTRIG | (ei->ei_phyunit << MICR_PHYADDR_SHIFT)
                | reg;
        while (regs->micr & MICR_BUSY)
                DELAYBUS(1);

        return ((regs->midr_r & MIDR_DATA_MASK));
}

/*
 * Write a PHY register.
 */
static void
ef_phyput(struct ef_info *ei,
	int reg,
	u_short val)
{
	ioc3_eregs_t	*regs = ei->ei_regs;

	while (regs->micr & MICR_BUSY)
		DELAYBUS(1);

	regs->midr_w = val;
        regs->micr = (ei->ei_phyunit << MICR_PHYADDR_SHIFT) | reg;

	while (regs->micr & MICR_BUSY)
                DELAYBUS(1);
}

static void
ef_phyrmw(struct ef_info *ei,
	int reg,
	int mask,
	int val)
{
	int rval, wval;

	rval = ef_phyget(ei, reg);
	wval = (rval & ~mask) | (val & mask);
	ef_phyput(ei, reg, (u_short)wval);
}

static void
ef_phyerrat(struct ef_info *ei)
{
	struct phyerrata *pe;

	for (pe = ef_phyerrata; pe->type != 0; pe++) {
		if ((pe->type != ei->ei_phytype) && 
		    !((pe->type == PHY_ICS1890) &&
		      (ei->ei_phytype == PHY_ICS1892)))
			continue;
		if ((pe->rev != -1) && (pe->rev != ei->ei_phyrev))
			continue;
		ef_phyrmw(ei, pe->reg, pe->mask, pe->val);
	}
}

/*
 * Allocate and post receive buffers.
 */
static void
ef_fill(struct ef_info *ei)
{
	volatile struct efrxbuf *rb;
	struct mbuf *m;
	int	head, tail;
	int	n;
	int	i;
#if HEART_COHERENCY_WAR
	int	old_tail;
#endif  /* HEART_COHERENCY_WAR */

	ASSERT(ei->ei_flags & EIF_LOCK);

	/*
	 * Determine how many receive buffers we're lacking
	 * from the full complement, allocate, initialize,
	 * and post them, then update the chip rx produce index.
	 * Deal with m_vget() failure.
	 */

	head = ei->ei_rxhead;
	tail = ei->ei_rxtail;
#if HEART_COHERENCY_WAR
	old_tail = tail;
#endif  /* HEART_COHERENCY_WAR */

	n = ei->ei_nrbuf - DELTARXD(head, tail);

	for (i = 0; i < n; i++) {
		if ((m = m_vget(M_DONTWAIT, sizeof (struct efrxbuf),
				MT_DATA)) == NULL) {
#pragma mips_frequency_hint NEVER
			DBG_PRINT(("ef%d: ef_fill: out of mbufs\n", ei->ei_unit));
			ei->ei_nombufs++;
			break;
		}
		ASSERT(MALIGN128(m));

		rb = mtod(m, struct efrxbuf*);
		rb->rb_rxb.w0 = 0;
		rb->rb_rxb.err = 0;
		ei->ei_rxm[tail] = m;
		ei->ei_rxd[tail] = KVTOIOADDR_DATA(ei, rb);

		/*
		 * For Heart: this actually does a writeback and the code depends on it
		 */
		CACHE_INVAL((void *)rb, sizeof(__uint32_t) * 2);

#if HEART_COHERENCY_WAR
		heart_write_dcache_wb_inval(mtod(m, caddr_t),
			sizeof (struct efrxbuf));
#else
		CACHE_WB(mtod(m, caddr_t), sizeof (struct efrxbuf));
#endif	/* HEART_COHERENCY_WAR */

		tail = NEXTRXD(tail);
	}

	ei->ei_rxtail = tail;

#if HEART_COHERENCY_WAR
	if (tail >= old_tail)
		CACHE_WB((void *)&ei->ei_rxd[old_tail],
			(tail - old_tail) * RXDSZ);
	else {
		CACHE_WB((void *)&ei->ei_rxd[old_tail],
			(NRXD - old_tail) * RXDSZ);
		CACHE_WB((void *)&ei->ei_rxd[0], tail * RXDSZ);
	}
#endif  /* HEART_COHERENCY_WAR */

	/*
	 * Update the chip rx produce pointer
	 * without (!) rearming the rx countdown timer.
	 */
	W_REG(ei->ei_regs->erpir, tail * RXDSZ);
}

/*
 * Wrapper for ef_reset - just acquires the private driver bitlock
 * before calling ef_reset().
 */
static void
ef_xreset(struct etherif *eif)
{
	struct ef_info *ei;
	int s;
	struct mbuf *mf, *m;

	ei = eiftoei(eif);

	s = mutex_bitlock(&ei->ei_flags, EIF_LOCK);
	mf = ef_reset(eif);
	mutex_bitunlock(&ei->ei_flags, EIF_LOCK, s);

	/* free any reclaimed packets */
	while (mf) {
		m = mf;
		mf = mf->m_act;
		m_freem(m);
	}
}

/*
 * Reset the interface, leaving it disabled.
 */
static struct mbuf *
ef_reset(struct etherif *eif)
{
	struct ef_info *ei;
	int i;
	struct ifqueue q;

	ei = eiftoei(eif);

	ASSERT(ei->ei_flags & EIF_LOCK);

	q.ifq_head = NULL;
	q.ifq_tail = NULL;

	/* stop watchdog */
	eiftoifp(eif)->if_timer = 0;

	/* remember to snarf the collision and defer counts */
	ef_getcdc(ei);

	/* reset the ioc3 ethernet state */
	ef_ioc3reset(ei);

	/* clear to prevent race with ef_reclaim via ef_watchdog - 687069 */
	ei->ei_txtail = 0;

	/* free any pending transmit mbufs */
	for (i = 0; i < NTXDBIG; i++)
		if (ei->ei_txm[i]) {
			IF_ENQUEUE_NOLOCK(&q, ei->ei_txm[i]);
			ei->ei_txm[i] = NULL;
		}

	/* free the posted rbufs */
	for (i = 0; i < NRXD; i++)
		if (ei->ei_rxm[i]) {
			IF_ENQUEUE_NOLOCK(&q, ei->ei_rxm[i]);
			ei->ei_rxm[i] = NULL;
		}

	ei->ei_rb = &ef_dummyrbuf;
	return (q.ifq_head);
}

/*
 * Reset ethernet portions of IOC3 chip.
 */
static void
ef_ioc3reset(struct ef_info *ei)
{
	time_t timeout;

	/*
	 * Write RESET bit, read it back to stall until the write buffer drains,
	 * spin until EMCR_ARB_DIAG_IDLE bit is set, then clear the RESET bit.
	 */
	W_REG(ei->ei_regs->emcr, EMCR_RST);
        R_REG(ei->ei_regs->emcr);

	timeout = time + 2;
	while ((R_REG(ei->ei_regs->emcr) & EMCR_ARB_DIAG_IDLE) == 0) {
		DELAY(1);
		if (time > timeout) {
			cmn_err(CE_ALERT, "ef%d: IOC3 reset timed out", ei->ei_unit);
			break;
		}
	}

        W_REG(ei->ei_regs->emcr,0x0);

	/*
	 * Errata from IOC3 4.4 spec,
	 * must explicitly clear tx/rx consume and produce pointers.
	 */
	W_REG(ei->ei_regs->etcir, 0x0);
	W_REG(ei->ei_regs->ercir, 0x0);
	W_REG(ei->ei_regs->etpir, 0x0);
	W_REG(ei->ei_regs->erpir, 0x0);
}

/*
 * Do miscellaneous periodic things.
 */
static void
ef_watchdog(struct ifnet *ifp)
{
	struct ef_info *ei = eiftoei(ifptoeif(ifp));
	struct mbuf *mf, *m;
	int s;
	int delay;
	int intrcpuid;
	struct sysinfo *si;
	int cpu_util;

	/* get the cpu utilization */
	intrcpuid =  cpuvertex_to_cpuid(pciio_intr_cpu_get(ei->ei_intr));

	s = mutex_bitlock(&ei->ei_flags, EIF_LOCK);

	if (intrcpuid == -1)
		delay = ef_rxdelay;  
	else {	
		si = cpuid_to_sysinfo(intrcpuid);
		cpu_util = si->cpu[CPU_USER] + si->cpu[CPU_KERNEL] + 
		           si->cpu[CPU_INTR];

		if ((cpu_util - ei->ei_cpu_util) < 75) {
			/* No delay when less than 75% busy on this CPU */
			delay = 0;
		} else {
			delay = (ei->ei_if.if_ipackets - ei->ei_ipackets - 1024);
			delay *= ef_rxdelay;
			delay /= 512;
			if (delay > 100) delay = 100;
			if (delay < 0) delay = 0;
		}
	}

	W_REG(ei->ei_regs->ertr, delay);
	ei->ei_ipackets = ei->ei_if.if_ipackets;
	ei->ei_cpu_util = cpu_util;

	/* update our collision and defer counts */
	ef_getcdc(ei);

	/* reclaim any completed tx buffers */
	mf = ef_reclaim(ei);

	/* refill any missing receive buffers */
	ef_fill(ei);

	/* recover from earlier outofmbufs condition */
	if ((ei->ei_rb == &ef_dummyrbuf) && ei->ei_rxm[ei->ei_rxhead])
		ei->ei_rb = mtod(ei->ei_rxm[ei->ei_rxhead], struct efrxbuf*);

	ei->ei_if.if_timer = EFDOG;

	/* check for phy errors */
	ef_phyerr(ei, s);

	mutex_bitunlock(&ei->ei_flags, EIF_LOCK, s);

	/* free any reclaimed packets */
	while (mf) {
		m = mf;
		mf = mf->m_act;
		m_freem(m);
	}

	/* check for missed interrupt */
	ef_intr(ei);
}

/*
 * Read chip collision and defer counters and update ifnet stats.
 */
static void
ef_getcdc(struct ef_info *ei)
{
	int etcdc;

	etcdc = R_REG(ei->ei_regs->etcdc);
	ei->ei_if.if_collisions += (etcdc & ETCDC_COLLCNT_MASK);
	ei->ei_defers += (etcdc >> ETCDC_DEFERCNT_SHIFT);
}

/*
 * generic ethernet interface interrupt.  Come here at splimp().
 */
static void
ef_intr(register struct ef_info *ei)
{
	int	unit;
	int	isr;
	int	fatal;
	int	s;
	struct mbuf *mact, *m;

	unit = ei->ei_unit;

	if ((ei->ei_txd == NULL) || iff_dead(ei->ei_if.if_flags)) {
		return;
	}

	s = mutex_bitlock(&ei->ei_flags, EIF_LOCK);

	/* read and clear all interrupt bits */
	isr = R_REG(ei->ei_regs->eisr);
	W_REG(ei->ei_regs->eisr, isr);

	/* rearm the rx interrupt */
	W_REG(ei->ei_regs->erpir, (ERPIR_ARM | (ei->ei_rxtail * RXDSZ)));

	/* flush receive buffer's first word */
 	CACHE_INVAL(ei->ei_rb, CACHE_SLINE_SIZE);

	mact = NULL;
	if (ei->ei_rb->rb_rxb.w0 & ERXBUF_V) {
		mact = ef_recv(ei);
	}

        if ((isr & EISR_OTHER) == 0) {
done:
		mutex_bitunlock(&ei->ei_flags, EIF_LOCK, s);

		/* send up any received packets */
		while (mact) {
			m = mact;
			mact = mact->m_act;
			ether_input(&ei->ei_eif, mact ? SN_MORETOCOME : 0, m);
		}
		return;
	}

#pragma mips_frequency_hint NEVER
	/* These should rarely happen */
	fatal = 0;

	if (isr & EISR_RXOFLO) {
		ei->ei_idrop++;
		ei->ei_if.if_ierrors++;
		fatal = 1;
		DBG_PRINT(("ef%d: dropped packets\n", unit));
	}

	if (isr & EISR_RXBUFOFLO) {
		ei->ei_ssramoflo++;
		ei->ei_if.if_ierrors++;
		fatal = 1;
		DBG_PRINT(("ef%d: receive overflow\n", unit));
	}

	if (isr & EISR_RXMEMERR) {
		ei->ei_memerr++;
		ei->ei_if.if_ierrors++;
		fatal = 1;
		cmn_err(CE_ALERT, "ef%d: receive PCI error", unit);
	}

	if (isr & EISR_RXPARERR) {
		ei->ei_parityerr++;
		ei->ei_if.if_ierrors++;
		fatal = 1;
		cmn_err(CE_ALERT, "ef%d:  rx SSRAM hardware parity error", unit);
	}

	if (isr & EISR_TXRTRY) {
		ei->ei_rtry++;
		ei->ei_if.if_oerrors++;
		DBG_PRINT(("ef%d: excessive collisions\n", unit));
	}

	if (isr & EISR_TXEXDEF) {
		ei->ei_exdefer++;
		ei->ei_if.if_oerrors++;
		DBG_PRINT(("ef%d: excessive defers\n", unit));
	}

	if (isr & EISR_TXLCOL) {
		ei->ei_lcol++;
		ei->ei_if.if_oerrors++;
		DBG_PRINT(("ef%d: late collision\n", unit));
	}

	if (isr & EISR_TXGIANT) {
		ei->ei_txgiant++;
		ei->ei_if.if_oerrors++;
		DBG_PRINT(("ef%d: transmit giant packet error\n", unit));
	}

	if (isr & EISR_TXBUFUFLO) {
		ei->ei_ssramuflo++;
		ei->ei_if.if_oerrors++;
		fatal = 1;
		DBG_PRINT(("ef%d: transmit underflow\n", unit));
	}

	if (isr & EISR_TXEXPLICIT) {
		if (ei->ei_flags & EIF_PSENABLED) {
			struct mbuf *mf;

			mf = ef_reclaim(ei);
			mutex_bitunlock(&ei->ei_flags, EIF_LOCK, s);
			ps_txq_stat(eiftoifp(&ei->ei_eif),
			    NTXDBIG - DELTATXD(ei->ei_txhead, ei->ei_txtail));
			/* free any reclaimed RSVP packets */
			while (mf) {
				m = mf;
				mf = mf->m_act;
				m_freem(m);
			}
			s = mutex_bitlock(&ei->ei_flags, EIF_LOCK);
		} else {
			DBG_PRINT(("ef%d: stray TXEXPLICIT interrupt\n", unit));
		}
	}

	if (isr & (EISR_TXCOLLWRAP | EISR_TXDEFERWRAP)) {
		ef_getcdc(ei);
		DBG_PRINT(("ef%d: collision or defer count wrapped\n", unit));
	}

	if (isr & EISR_TXMEMERR) {
		ei->ei_memerr++;
		ei->ei_if.if_oerrors++;
		fatal = 1;
		cmn_err(CE_WARN, "ef%d: transmit PCI error", unit);
	}

	if (isr & EISR_TXPARERR) {
		ei->ei_parityerr++;
		ei->ei_if.if_oerrors++;
		fatal = 1;
		cmn_err(CE_ALERT, "ef%d:  tx SSRAM hardware parity error", unit);
	}

	if (fatal) {
		(void) ef_init(&ei->ei_eif, ei->ei_if.if_flags, s);
	}
	goto done;
}

/*
 * Process received packets.
 * Return a list of mbufs to send up.
 */
static struct mbuf *
ef_recv(struct ef_info *ei)
{
	struct efrxbuf *rb;
	struct mbuf *m;
	struct ifqueue q;
	struct ether_header *eh;
	__uint32_t cksum;
	__uint32_t x;
	struct ip *ip;
	char *crc;
	int hlen;
	int snoopflags;
	int unit;
	int head, tail;
	int w0;
	int err;
	int rlen;
	int i;

	ASSERT(ei->ei_flags & EIF_LOCK);

	q.ifq_len = 0;
	q.ifq_head = NULL;
	q.ifq_tail = NULL;

	head = ei->ei_rxhead;
	tail = ei->ei_rxtail;

	unit = ei->ei_unit;

	/*
	 * Process all the received packets.
	 */
	for (i = head; i != tail; i = NEXTRXD(i)) {
		m = ei->ei_rxm[i];
		GOODMP(m);
		GOODMT(m->m_type);

		rb = mtod(m, struct efrxbuf*);

		CACHE_INVAL(rb, CACHE_SLINE_SIZE);

		w0 = rb->rb_rxb.w0;
		err = rb->rb_rxb.err;

		if ((w0 & ERXBUF_V) == 0)
			break;

		ei->ei_rxm[i] = NULL;

		rlen = ((w0 & ERXBUF_BYTECNT_MASK) >> ERXBUF_BYTECNT_SHIFT) - ETHER_CRCSZ;

		CACHE_INVAL((caddr_t)rb + CACHE_SLINE_SIZE, roundup(rlen, CACHE_SLINE_SIZE));

		m->m_off += sizeof (struct ioc3_erxbuf);

		IF_INITHEADER(&rb->rb_ebh, &ei->ei_if,
			sizeof(struct etherbufhead));

		ei->ei_if.if_ipackets++;
		ei->ei_if.if_ibytes += rlen;

		eh = &rb->rb_ebh.ebh_ether;

		if (!(err & ERXBUF_GOODPKT)) {
			goto error;
		}

		m->m_len = (rlen - ETHER_HDRLEN) + sizeof (struct etherbufhead);

		/*
		 * Finish TCP or UDP checksum on non-fragments.
		 */

		cksum = w0 & ERXBUF_IPCKSUM_MASK;
		ip = (struct ip*) rb->rb_data;
		hlen = ip->ip_hl << 2;

		if ((ntohs(eh->ether_type) == ETHERTYPE_IP)
			&& (rlen >= 64)
			&& ((ip->ip_off & (IP_OFFMASK|IP_MF)) == 0)
			&& ((ip->ip_p == IPPROTO_TCP) || (ip->ip_p == IPPROTO_UDP))
			&& (ei->ei_flags & EIF_CKSUM_RX)) {

			/* compute checksum of the pseudo-header */
			cksum += ((ip->ip_len - hlen)
				  + htons((ushort)ip->ip_p)
				  + (ip->ip_src.s_addr >> 16)
				  + (ip->ip_src.s_addr & 0xffff)
				  + (ip->ip_dst.s_addr >> 16)
				  + (ip->ip_dst.s_addr & 0xffff));

			/*
			 * Subtract the ether header from the checksum.
			 * The IP header will sum to logical zero if it's correct
			 * so we don't need to include it here.  This is safe since,
			 * if it's incorrect, ip input code will toss it anyway.
			 */
			x = ((u_short*)eh)[0]
				+ ((u_short*)eh)[1]
				+ ((u_short*)eh)[2]
				+ ((u_short*)eh)[3]
				+ ((u_short*)eh)[4]
				+ ((u_short*)eh)[5]
				+ ((u_short*)eh)[6];
			x = (x & 0xffff) + (x >> 16);
			x = 0xffff & (x + (x >> 16));
			cksum += 0xffff ^ x;

			/* subtract CRC portion that is not part of checksum */
			crc = (char*) &rb->rb_data[rlen - ETHER_HDRLEN];
			if (rlen & 1) {	/* odd */
				cksum += 0xffff ^ (u_short) ((crc[1] << 8) | crc[0]);
				cksum += 0xffff ^ (u_short) ((crc[3] << 8) | crc[2]);
			} else { /* even */
				cksum += 0xffff ^ (u_short) ((crc[0] << 8) | crc[1]);
				cksum += 0xffff ^ (u_short) ((crc[2] << 8) | crc[3]);
			}

			cksum = (cksum & 0xffff) + (cksum >> 16);
			cksum = 0xffff & (cksum + (cksum >> 16));

			if (cksum == 0xffff)
				m->m_flags |= M_CKSUMMED;
		}

		/*
		 * Enqueue the packet instead of calling ether_input() now
		 * since we're holding the driver bitlock.
		 */
		IF_ENQUEUE_NOLOCK(&q, m);

		continue;

error:
#pragma mips_frequency_hint NEVER
		snoopflags = SN_ERROR;

		if (rlen + ETHER_CRCSZ < 64) {
			snoopflags |= SNERR_TOOSMALL;
			ei->ei_runt++;
			DBG_PRINT(("ef%d: runt packet\n", unit));
			rlen = ETHER_HDRLEN;
		}

		if (err & ERXBUF_CRCERR) {
			if (rlen + ETHER_CRCSZ == GIANT_PACKET_SIZE) {
				snoopflags |= SNERR_TOOBIG;
				ei->ei_giant++;
				DBG_PRINT(("ef%d: giant packet received from %s\n", unit, ether_sprintf(eh->ether_shost)));
				rlen = ETHERMTU + ETHER_HDRLEN;
			}
			else {
				snoopflags |= SNERR_CHECKSUM;
				ei->ei_crc++;
				DBG_PRINT(("ef%d: crc error from %s\n", unit, ether_sprintf(eh->ether_shost)));
			}
		}
		else if (rlen + ETHER_CRCSZ > 1518) {
			snoopflags |= SNERR_TOOBIG;
			ei->ei_giant++;
			DBG_PRINT(("ef%d: giant packet received from %s\n", unit, ether_sprintf(eh->ether_shost)));
			rlen = ETHERMTU + ETHER_HDRLEN;
		}

		/*
		 * Skip tests for ERXBUF_FRAMERR and ERXBUF_INVPREAMB because
		 * there's no way that either of these can be the only error since,
		 * by themselves, they aren't considered "bad packet" conditions.
		 * If these bits -are- set in a bad packet, they are secondary effects
		 * of the real error, so it really just muddies the water to report
		 * them.
		 */

		if (err & ERXBUF_CODERR) {
			snoopflags |= SNERR_FRAME;
			ei->ei_framerr++;
			DBG_PRINT(("ef%d: framing error\n", unit));
		}

		m->m_len = (rlen - ETHER_HDRLEN) + sizeof (struct etherbufhead);
		ether_input(&ei->ei_eif, snoopflags, m);
	}

	/* update rxd head */
	ei->ei_rxhead = i;

	/* post any necessary receive buffers */
	ef_fill(ei);

	/* update quick rbuf pointer */
	if (ei->ei_rxm[i])
		ei->ei_rb = mtod(ei->ei_rxm[i], struct efrxbuf*);
	else
		ei->ei_rb = &ef_dummyrbuf;

	return (q.ifq_head);
}

static int
ef_transmit(
	struct etherif *eif,
	struct etheraddr *edst,
	struct etheraddr *esrc,
	u_short type,
	struct mbuf *m0)
{
	struct ef_info *ei;
	struct mbuf *m;
	struct mbuf *m1, *m2, *m3;
	struct mbuf *mf;
	volatile struct eftxd	*txd;
	struct ether_header *eh;
	int mlen;
	int mcnt;
	int s;
	caddr_t p;
	int total;
	__uint32_t x;
	int sumoff;

	GOODMP(m0);
	GOODMT(m0->m_type);

	ei = eiftoei(eif);

	mlen = mcnt = 0;
	for (m = m0; m; m = m->m_next) {
		mlen += m->m_len;
		mcnt++;
		/* Force copy if mbuf crosses page boundary. */ 
		p = mtod(m, caddr_t);
		if (m->m_len && (pnum(p) != pnum(p+m->m_len-1)))
			mcnt = 100;
	}

	if (mlen > ETHERMTU)
		goto toobig;

	total = mlen + sizeof (*eh);

	x = 0;
	if (m0->m_flags & M_CKSUMMED) {
		__uint32_t cksum;
		struct ip *ip;
		int hlen;

		/*
		 * Start computing the checksum outside the critical
		 * section, before the spin-lock.
		 */
		ip = mtod(m0, struct ip*);
		hlen = ip->ip_hl << 2;

		ASSERT(ei->ei_flags & EIF_CKSUM_TX);
		ASSERT(type == ETHERTYPE_IP);
		ASSERT((ip->ip_off & (IP_OFFMASK|IP_MF)) == 0);
		ASSERT((ip->ip_p == IPPROTO_TCP) || (ip->ip_p == IPPROTO_UDP));
		ASSERT(m0->m_len >= sizeof (struct ip*));

		/* compute checksum of the pseudo-header */
		cksum = ((ip->ip_len - hlen)
			  + htons((ushort)ip->ip_p)
			  + (ip->ip_src.s_addr >> 16)
			  + (ip->ip_src.s_addr & 0xffff)
			  + (ip->ip_dst.s_addr >> 16)
			  + (ip->ip_dst.s_addr & 0xffff));

		if (ip->ip_p == IPPROTO_TCP)
			sumoff = hlen + (8 * 2);
		else
			sumoff = hlen + (3 * 2);

		/*
		 * Subtract the ether header from the checksum.
		 * The IP header will sum to logical zero
		 * so we don't need to include it here.
		 */
		x = type
			+ ((u_short*)esrc)[0]
			+ ((u_short*)esrc)[1]
			+ ((u_short*)esrc)[2]
			+ ((u_short*)edst)[0]
			+ ((u_short*)edst)[1]
			+ ((u_short*)edst)[2];
		x = (x & 0xffff) + (x >> 16);
		x = 0xffff & (x + (x >> 16));
		cksum += 0xffff ^ x;

		cksum = (cksum & 0xffff) + (cksum >> 16);
		cksum = 0xffff & (cksum + (cksum >> 16));

		ASSERT(sumoff < m0->m_len);

		/* stuff the cksum adjustment into the tcp/udp header */
		mtod(m0, u_short*)[sumoff/sizeof (u_short)] = cksum;
		sumoff += sizeof (*eh);
	}

	s = mutex_bitlock(&ei->ei_flags, EIF_LOCK);

	/*
	 * The basic performance idea is to keep a dense,
	 * linear fastpath and throw rare conditions out via "goto".
	 */

	if (ei->ei_flags & EIF_LINKDOWN)
		goto linkdown;

top:
	/* allocate tx descriptor */
	txd = &ei->ei_txd[ei->ei_txhead];
	if (NEXTTXD(ei->ei_txhead) == ei->ei_txtail)
		goto outoftxd;

	/*
	 * Each 128byte IOC3 tx descriptor describes a single
	 * packet and contains up to 104 bytes of data locally,
	 * plus two pointers to noncontiguous data.
	 *
	 * The IOC3 tx has the following restrictions:
	 * - if data0 and buf1 are used, then data0 must
	 *   be an even number of bytes in length.
	 * - if used, buf1 must always start on an even byte address.
	 * - if used, buf2 must start on a 128byte address
	 *   and buf1 must be an even number of bytes in length
	 * - neither buf1 nor buf2 may cross a page boundary
	 */

	/* create ether header */
	eh = (struct ether_header*) &txd->eh;
	*(struct etheraddr*)eh->ether_dhost = *edst;
	*(struct etheraddr*)eh->ether_shost = *esrc;
	eh->ether_type = type;

	/*
	 * If the M_CKSUMMED flag is set, the IOC3 will do the TCP/UDP checksum.
	 */
	if (m0->m_flags & M_CKSUMMED) {

		/* init txd dochecksum bits for later */
		x = (sumoff << ETXD_CHKOFF_SHIFT) | ETXD_DOCHECKSUM;
	}

	if (mlen <= TXDATASZ) {	/* if small, copy it into txd */
  		m_datacopy(m0, 0, mlen, (caddr_t)txd->data);
		txd->cmd = ETXD_D0V | total;
		txd->bufcnt = total;
	}
	else if (mcnt == 1) {	/* point buf1 at it */
		if (!MALIGN2(m0))
			goto copyall;
		txd->cmd = ETXD_D0V | ETXD_B1V | total;
		txd->bufcnt = (m0->m_len << ETXD_B1CNT_SHIFT) | sizeof (*eh);
		txd->p1 = KVTOIOADDR_DATA(ei, mtod(m0, caddr_t));
		CACHE_WB(mtod(m0, caddr_t), m0->m_len);
	}
	else if (mcnt == 2) {	/* (point buf1 and buf2) or (copy m1 and point buf1) */
		m1 = m0;
		m2 = m1->m_next;
	
		if (!MALIGN2(m1) || !MLEN2(m1) || !MALIGN2(m2)
			|| (!MALIGN128(m2) && (m1->m_len > TXDATASZ)))
			goto copyall;
		if (MALIGN128(m2)) {	/* point buf1 at m1 and buf2 at m2 */
			txd->cmd = ETXD_D0V | ETXD_B1V | ETXD_B2V | total;
			txd->bufcnt = (m1->m_len << ETXD_B1CNT_SHIFT) |
				(m2->m_len << ETXD_B2CNT_SHIFT) | sizeof (*eh);
			txd->p1 = KVTOIOADDR_DATA(ei, mtod(m1, caddr_t));
			txd->p2 = KVTOIOADDR_DATA(ei, mtod(m2, caddr_t));
		}
		else {	/* copy m1 and point buf1 at m2 */
			bcopy(mtod(m1, caddr_t), (caddr_t)txd->data, m1->m_len);
			txd->cmd = ETXD_D0V | ETXD_B1V | total;
			txd->bufcnt = (m2->m_len << ETXD_B1CNT_SHIFT) | (m1->m_len + sizeof (*eh));
			txd->p1 = KVTOIOADDR_DATA(ei, mtod(m2, caddr_t));
		}
		CACHE_WB(mtod(m1, caddr_t), m1->m_len);
		CACHE_WB(mtod(m2, caddr_t), m2->m_len);
	}
	else if (mcnt == 3) {	/* copy m1, point buf1 and buf2 */
		m1 = m0;
		m2 = m1->m_next;
		m3 = m2->m_next;

		if ((m1->m_len > TXDATASZ) || !MLEN2(m1)
			|| !MALIGN2(m2) || !MLEN2(m2)
			|| !MALIGN128(m3))
			goto copyall;
		m_datacopy(m1, 0, m1->m_len, (caddr_t)txd->data);

		txd->cmd = ETXD_D0V | ETXD_B1V | ETXD_B2V | total;
		txd->bufcnt = (m2->m_len << ETXD_B1CNT_SHIFT) |
				(m3->m_len << ETXD_B2CNT_SHIFT) |
				m1->m_len + sizeof (*eh);

		txd->p1 = KVTOIOADDR_DATA(ei, mtod(m2, caddr_t));
		txd->p2 = KVTOIOADDR_DATA(ei, mtod(m3, caddr_t));
		CACHE_WB(mtod(m2, caddr_t), m2->m_len);
                CACHE_WB(mtod(m3, caddr_t), m3->m_len);
	}
	else {	/* copy it all into a single new large mbuf */
copyall:
#pragma mips_frequency_hint NEVER
		if ((m = m_vget(M_DONTWAIT, mlen, MT_DATA)) == NULL)
			goto outofmbufs;
		ASSERT(MALIGN2(m));
		m_datacopy(m0, 0, mlen, mtod(m, caddr_t));
		m->m_len = mlen;
		m_freem(m0);
		m0 = m;
		txd->cmd = ETXD_D0V | ETXD_B1V | total;
		txd->bufcnt = (m0->m_len << ETXD_B1CNT_SHIFT) | sizeof (*eh);
		txd->p1 = KVTOIOADDR_DATA(ei, mtod(m0, caddr_t));
		CACHE_WB(mtod(m0, caddr_t), m0->m_len);
	}

	/*
	 * RSVP.  If packet sheduling is enabled, kick the packet scheduler
	 * every few ms by setting the ETXD_INTWHENDONE -> EISR_TXEXPLICIT
	 */
	if ((ei->ei_flags & EIF_PSENABLED) &&
	    ((ei->ei_txhead & ei->ei_intfreq) == 0)) {
#pragma mips_frequency_hint NEVER
		x |= ETXD_INTWHENDONE;
	}

	/* enable tx checksum */
	txd->cmd |= x;

        CACHE_WB((void *)txd, TXDSZ);

	if (RAWIF_SNOOPING(&ei->ei_rawif))
		ef_selfsnoop(ei, eh, m0, mlen);

	/* save mbuf chain to free later */
	ASSERT(ei->ei_txm[ei->ei_txhead] == NULL);
	ei->ei_txm[ei->ei_txhead] = m0;

	/* increment index of txd head */
	ei->ei_txhead = NEXTTXD(ei->ei_txhead);

	/* go */
	W_REG(ei->ei_regs->etpir, (ei->ei_txhead * TXDSZ));

	ei->ei_if.if_obytes += total;

	/* don't allow too many unreclaimed txd's to build up */
	if (DELTATXD(ei->ei_txhead, ei->ei_txtail) > 20) {
		mf = ef_reclaim(ei);
	} else {
		mf = NULL;
	}

	mutex_bitunlock(&ei->ei_flags, EIF_LOCK, s);

	/* free any reclaimed packets */
	while (mf) {
		m = mf;
		mf = mf->m_act;
		m_freem(m);
	}
	return (0);

toobig:
#pragma mips_frequency_hint NEVER
	ei->ei_toobig++;
	m_freem(m0);
	DBG_PRINT(("ef%d: ef_transmit: invalid mbuf len: %d\n", ei->ei_unit, mlen));
	return (E2BIG);

outoftxd:
#pragma mips_frequency_hint NEVER
	mf = ef_reclaim(ei);
	mutex_bitunlock(&ei->ei_flags, EIF_LOCK, s);
	/* free any reclaimed packets */
	while (mf) {
		m = mf;
		mf = mf->m_act;
		m_freem(m);
	}
	s = mutex_bitlock(&ei->ei_flags, EIF_LOCK);
	if (NEXTTXD(ei->ei_txhead) != ei->ei_txtail)
		goto top;
	ei->ei_notxds++;
	m_freem(m0);
	DBG_PRINT(("ef%d: ef_transmit: out of tx descriptors\n", ei->ei_unit));
	mutex_bitunlock(&ei->ei_flags, EIF_LOCK, s);
	return (ENOBUFS);

outofmbufs:
#pragma mips_frequency_hint NEVER
	ei->ei_nombufs++;
	m_freem(m0);
	DBG_PRINT(("ef%d: ef_transmit: out of mbufs\n", ei->ei_unit));
	mutex_bitunlock(&ei->ei_flags, EIF_LOCK, s);
	return (ENOBUFS);

linkdown:
#pragma mips_frequency_hint NEVER
	ei->ei_linkdrop++;
	m_freem(m0);
	mutex_bitunlock(&ei->ei_flags, EIF_LOCK, s);
	return (ENETDOWN);
}

/* We are doing something different than other ethernet drivers
 * because we are passing down the snoop_match() result to
 * snoop_input(). We also don't want to change other drivers.
 * The only choice seems to be to do what ether_selfsnoop()
 * does in this routine.
 */
static void
ef_selfsnoop(struct ef_info *ei,
        struct ether_header *eh,
        struct mbuf *m0,
        int mlen)
{
        __uint32_t f[SNOOP_FILTERLEN];
        caddr_t h;
	struct mbuf *m;
	struct etherbufhead *ebh;
	struct snoopfilter *sf;
	struct etherif *eif = &ei->ei_eif;

        h = (caddr_t) f;
	
	/* 2 bytes of padding + 14 bytes of ether_header */
        bcopy((caddr_t)eh, &h[2], sizeof (struct ether_header));
        m_datacopy(m0, 0, (SNOOP_FILTERLEN-4) * sizeof (__uint32_t), &h[16]);
        
        sf = snoop_match(&ei->ei_rawif, &h[2], 
			SNOOP_FILTERLEN * sizeof(__uint32_t));
	if (!sf)
	        return;

	m = m_vget(M_DONTWAIT, sizeof *ebh + mlen, MT_DATA);
	if (m == 0) {
		snoop_drop2(&eif->eif_rawif, SN_PROMISC, (caddr_t) eh, mlen, sf);
		return;
	}
	ebh = mtod(m, struct etherbufhead *);
	IF_INITHEADER(ebh, eiftoifp(eif), sizeof *ebh);
	(void) m_datacopy(m0, 0, mlen, (caddr_t)(ebh + 1));
	ebh->ebh_ether = *eh;
	snoop_input2(&eif->eif_rawif, SN_PROMISC, (caddr_t) &ebh->ebh_ether, m,
		    (mlen < ETHERMIN) ? ETHERMIN : mlen, sf);
}


static int
ef_ioctl(struct etherif *eif,
	int cmd,
	void *data)
{
	struct ef_info *ei;
	struct mfreq *mfr;
	union mkey *key;
	int rc;
	int s;

	ei = eiftoei(eif);

	ASSERT(IFNET_ISLOCKED(&ei->ei_if));

	s = mutex_bitlock(&ei->ei_flags, EIF_LOCK);

	mfr = (struct mfreq*) data;
	key = mfr->mfr_key;
	rc = 0;

	switch (cmd) {
	case SIOCADDMULTI:
		mfr->mfr_value = ef_lafhash(key->mk_dhost,
			sizeof(key->mk_dhost));
		if (LAF_TSTBIT(&ei->ei_laf, mfr->mfr_value)) {
			ASSERT(mfhasvalue(&eif->eif_mfilter, mfr->mfr_value));
			break;
		}
		ASSERT(!mfhasvalue(&eif->eif_mfilter, mfr->mfr_value));
		LAF_SETBIT(&ei->ei_laf, mfr->mfr_value);
		if (ei->ei_nmulti == 0)
			ei->ei_if.if_flags |= IFF_FILTMULTI;
		ei->ei_nmulti++;
		rc = ef_init(eif, ei->ei_if.if_flags, s);
		break;

	case SIOCDELMULTI:
		if (mfr->mfr_value
		    != ef_lafhash(key->mk_dhost, sizeof(key->mk_dhost))) {
			rc = EINVAL;
			break;
		}
		if (mfhasvalue(&eif->eif_mfilter, mfr->mfr_value))
			break;
		/*
	 	 * If this multicast address is the last one to map
		 * the bit numbered by mfr->mfr_value in the filter,
		 * clear that bit and update the chip.
	 	 */
		LAF_CLRBIT(&ei->ei_laf, mfr->mfr_value);
		ei->ei_nmulti--;
		if (ei->ei_nmulti == 0)
			ei->ei_if.if_flags &= ~IFF_FILTMULTI;
		rc = ef_init(eif, ei->ei_if.if_flags, s);
		break;

	default:
		rc = EINVAL;
		break;
	}

	mutex_bitunlock(&ei->ei_flags, EIF_LOCK, s);

	return (rc);
}

/*
 * Given a multicast ethernet address, this routine calculates the
 * address's bit index in the logical address filter mask
 */
#define CRC_MASK        0xEDB88320

static int
ef_lafhash(u_char *addr, int len)
{
        u_int crc;
        u_char byte;
        int bits;
        u_int temp = 0;

        for (crc = ~0; --len >= 0; addr++) {
                byte = *addr;
                for (bits = 8; --bits >= 0; ) {
                        if ((byte ^ crc) & 1)
                                crc = (crc >> 1) ^ CRC_MASK;
                        else
                                crc >>= 1;
                        byte >>= 1;
                }
        }

        crc &= 0x3f;    /* bit reverse lowest 6 bits for hash index */
        for (bits = 6; --bits >= 0; ) {
                temp <<= 1;
                temp |= (crc & 0x1);
                crc >>= 1;
        }

        return temp;
}

#ifdef EFMLOAD

int
if_efunload()
{
	int i;

	/* currently unsupported */

	idbg_delfunc((void (*)())ef_dump);

	return (ENXIO);
}

static int
ef_reload(struct ifnet *ifp)
{
	struct etherif *eif;
	struct ef_info *ei;
	int i;

	ef_info = (struct ef_info*) ifp;

	/* currently unsupported */

	idbg_addfunc("ef_dump", (void (*)()) ef_dump);

	return (ENXIO);
}

#endif	/* EFMLOAD */

static char *
ef_phystr(int phytype)
{
	char *pname = "";

	switch(phytype) {
	    case PHY_QS6612X:
		pname = "QS6612";
		break;
	    case PHY_ICS1889:
		pname = "ICS1889";
		break;
	    case PHY_ICS1890:
		pname = "ICS1890";
		break;
	    case PHY_ICS1892:
		pname = "ICS1892";
		break;
	    case PHY_DP83840:
		pname = "DP83840";
		break;
	}

	return pname;
}

static void
ef_dump(int unit)
{
        struct ef_info *ei;
	struct ifnet *ifp;
	char name[128];

	if (unit == -1)
		unit = 0;

	sprintf(name, "ef%d", unit);

	if ((ifp = ifunit(name)) == NULL) {
		qprintf("ef_dump: %s not found in ifnet list\n", name);
		return;
	}
	ei = eiftoei(ifptoeif(ifp));

        ef_dumpif(&ei->ei_if);
	qprintf("\n");
	ef_dumpei(ei);
	qprintf("\n");
	ef_dumpregs(ei);
	qprintf("\n");
	ef_dumpphy(ei);
}

static void
ef_dumpif(struct ifnet *ifp)
{
        qprintf("ifp 0x%x if_name \"%s\" if_unit %d if_mtu %d if_flags 0x%x\n",
		ifp, ifp->if_name, ifp->if_unit, ifp->if_mtu, ifp->if_flags);
        qprintf("if_timer %d ifq_len %d ifq_maxlen %d ifq_drops %d\n",
                ifp->if_timer, ifp->if_snd.ifq_len, ifp->if_snd.ifq_maxlen,
                ifp->if_snd.ifq_drops);
        qprintf("if_ipackets %u if_ierrors %u if_opackets %u if_oerrors %u\n",
                ifp->if_ipackets, ifp->if_ierrors,
                ifp->if_opackets, ifp->if_oerrors);
        qprintf("if_collisions %u if_ibytes %u if_obytes %u if_odrops %u\n",
                ifp->if_collisions, ifp->if_ibytes,
                ifp->if_obytes, ifp->if_odrops);
}

static void
ef_dumpei(struct ef_info *ei)
{
        qprintf("ei 0x%x ac_enaddr %s chiprev %d rawif 0x%x\n",
		ei, ether_sprintf(ei->ei_curaddr), ei->ei_chiprev, &ei->ei_rawif);
	qprintf("phyunit %u phytype %s phyrev %u\n",
		ei->ei_phyunit, ef_phystr(ei->ei_phytype), ei->ei_phyrev);
        qprintf("txd 0x%x txhead %u txtail %u txm 0x%x flags 0x%x\n",
                ei->ei_txd, ei->ei_txhead, ei->ei_txtail,
                ei->ei_txm, ei->ei_flags);
        qprintf("nrbuf %u rxd 0x%x rxhead %u rxtail %u rb 0x%x rxm 0x%x\n",
                ei->ei_nrbuf, ei->ei_rxd, ei->ei_rxhead, ei->ei_rxtail,
                ei->ei_rb, ei->ei_rxm);
	qprintf("nmulti %u ssram_bits 0x%x ", ei->ei_nmulti, ei->ei_ssram_bits);
	ef_hexdump("laf", (char*) &ei->ei_laf, sizeof (struct lafilter));
        qprintf("speed100 %d fullduplex %d remfaulting %u jabbering %u\n",
                ei->ei_speed100, ei->ei_fullduplex, ei->ei_remfaulting, ei->ei_jabbering);
        qprintf("defers %u runt %u crc %u giant %u framerr %u\n",
                ei->ei_defers, ei->ei_runt, ei->ei_crc, ei->ei_giant, ei->ei_framerr);
        qprintf("idrop %u ssramoflo %u ssramuflo %u memerr %u\n",
                ei->ei_idrop, ei->ei_ssramoflo, ei->ei_ssramuflo, ei->ei_memerr);
        qprintf("parityerr %u rtry %u exdefer %u lcol %u txgiant %u linkdrop %u\n",
                ei->ei_parityerr, ei->ei_rtry, ei->ei_exdefer,
		ei->ei_lcol, ei->ei_txgiant, ei->ei_linkdrop);
	qprintf("nombufs %u notxds %u toobig %u\n",
		ei->ei_nombufs, ei->ei_notxds, ei->ei_toobig);
}

static void
ef_dumpregs(struct ef_info *ei)
{
        ioc3_eregs_t *regs = ei->ei_regs;

        qprintf("ei 0x%x ", ei);
        qprintf("ei_regs 0x%x\n", regs);
        qprintf("emcr 0x%x eisr 0x%x eier 0x%x\n",
                regs->emcr, regs->eisr, regs->eier);
        qprintf("ercsr 0x%x erbr_l 0x%x erbr_h 0x%x erbar 0x%x\n",
                regs->ercsr, regs->erbr_l, regs->erbr_h, regs->erbar);
        qprintf("ercir 0x%x erpir 0x%x ertr 0x%x etcsr 0x%x etcdc 0x%x\n",
                regs->ercir, regs->erpir, regs->ertr,
                regs->etcsr, regs->etcdc);
        qprintf("etbr_l 0x%x etbr_h 0x%x etcir 0x%x etpir 0x%x\n",
                regs->etbr_l, regs->etbr_h, regs->etcir, regs->etpir);
        qprintf("ebir 0x%x emar_l 0x%x emar_h 0x%x ehar_l 0x%x ehar_h 0x%x\n",
                regs->ebir, regs->emar_l, regs->emar_h, regs->ehar_l, regs->ehar_h);
}

static void
ef_dumpphy(struct ef_info *ei)
{
	int i;
	int nl = 0;

	for (i = 0; i < 32; i++) {
		qprintf("reg%02d 0x%04x ", i, ef_phyget(ei, i));
		if (++nl >= 4) {
			qprintf("\n");
			nl = 0;
		}
	}
	if (nl != 0)
		qprintf("\n");
}

/*
 * print msg, then variable number of hex bytes.
 */
static void
ef_hexdump(char *msg, char *cp, int len)
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
	qprintf("%s: %s\n", msg, qstr);
}
