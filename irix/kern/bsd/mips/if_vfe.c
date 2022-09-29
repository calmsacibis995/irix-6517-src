/*
 * VME Fast Ethernet Driver, for IP19/IP21/IP25
 *   - InterPhase IP-6200 PCI 100BaseT ethernet board
 *   - TI ThunderLAN chip
 *   - Newbridge Universe VME-PCI bridge chip with DMA controller
 *
 * Copyright 1996, Silicon Graphics, Inc.
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

#include <sys/types.h>
#include <sys/sema.h>
#include <sys/errno.h>
#include <sys/edt.h>
#include <sys/immu.h>
#include <sys/vmereg.h>
#include <sys/pio.h>
#include <sys/dmamap.h>
#include <sys/EVEREST/io4.h>
#include <sys/cred.h>
#include <sys/param.h>
#include <sys/sysmacros.h>

#include <sys/mbuf.h>
#include <sys/time.h>
#include <sys/cmn_err.h>
/* #include <sys/debug.h> */
#include <netinet/if_ether.h>
#include <net/soioctl.h>
#include <sys/misc/ether.h>
/* #include <sys/tlan.h>
#include <sys/pmc.h>
#include <sys/fenet.h>
*/
#include <sys/if_vfe.h>
#include <sys/pda.h>
#include <sys/invent.h>
#include <sys/systm.h>
#include <sys/iograph.h>
#include <sys/hwgraph.h>
#include <net/rsvp.h>


#define  DP printf


#define ASSERT(x)	\
	{		\
	    if (!(x))	\
		cmn_err_tag(212,CE_PANIC,"line %d, file %s\n",__LINE__, __FILE__); \
	}

/* Forward declaration */

int if_vfeintr(int);
iamap_t *iamap_get(int, int);
int tl_eeprom_read(pmc_dev_t *,u_char, u_char *);
static int if_vfeinit(struct etherif *,int);
static int vfe_init(struct etherif *,int);
static void vfe_reset(struct etherif *);
static void vfe_watchdog(struct ifnet *ifp);
void vfe_read(struct etherif *, struct mbuf *);
volatile tlan_txbd_t  * tl_get_txbd(pmc_dev_t *,int );
void tl_receive_enable(pmc_dev_t *);
int tl_start_transmit(pmc_dev_t *);
void tl_pmc_init(pmc_dev_t *);
int tl_end_txlist(pmc_dev_t *);
void tl_rebuild_struct(pmc_dev_t *);
void tl_receive_enable(pmc_dev_t *);
static void vfe_check_link(pmc_dev_t *);
int check_speed(pmc_dev_t *, int *, int *);
static void vfe_setstate(struct ifnet *ifp, int setting);	/* RSVP */
static int vfe_txfree_len(struct ifnet *);



extern int showconfig;
extern int vfe_ldtmr; /* tunale through systune; see mtune/bsd */

#define VFEDMA_INITLOCK(fe)       initlock(&fe->vfedma_lock)
#define VFEDMA_LOCK(fe, s)           s = mutex_spinlock(&fe->vfedma_lock)
#define VFEDMA_UNLOCK(fe, s)         mutex_spinunlock(&fe->vfedma_lock, s)

#define EDOG    (10*IFNET_SLOWHZ)          /* watchdog timeout in sec */


/*----------------------------------------------------------------------------
 * vfenet static variables area
 *----------------------------------------------------------------------------
 */
char vfe_verid[] = "IP-6200 100BaseTX EtherNet Ver 2.1 (B.17)";
fenet_dev_t *vfe_tbl[ MAXBOARDS ] = {0,};

static char  *adptchk_errmsg[] = {
	"Invalid Adapter Check code",
	"Data Parity Error",
	"Address Parity Error",
	"Master Abort",
	"Target Abort",
	"List Error",
	"Ack Error",
	"Int Overflow Error"
};

int vfe_debug = 0;
u_int stats_overflow = 0;
u_int   rx_overrun_all  = 0;
u_int   crc_frm_all = 0;
u_int   codeerr_all  = 0;

#if VFE_DEBUG == 1
struct Trc_bd_tbl {
volatile	__uint32_t bdp1;
volatile	__uint32_t bdp2;
volatile	__uint32_t bdp3;
	int        idx;
};

struct Trc_dma_tbl {
volatile	__uint32_t val1;
volatile	__uint32_t val2;
volatile	__uint32_t val3;
volatile	__uint32_t val4;
volatile	__uint32_t val5;
volatile	__uint32_t val6;
volatile	__uint32_t val7;
volatile	__uint32_t val8;
};

#define  PACKDMA_DATA	(volatile __uint32_t )0xeeeeeeee
#define  PACKDMA_RX	(volatile __uint32_t )0x11111111
#define  PACKDMA_TX	(volatile __uint32_t )0x22222222
#define  DMADONE_RX     (volatile __uint32_t )0x33333333
#define  DMADONE_TX     (volatile __uint32_t )0x44444444
#define  DMASTARTED     (volatile __uint32_t )0xDDDDDDDD

struct Trc_bd_tbl Trc_bd_buf[128] = {0,};
int    Trc_bd_idx = 0;

struct Trc_dma_tbl Trc_dma_buf[32] = {0,};
int    Trc_dma_idx = 0;

struct vfe_diag {
    int out_alloc;
    int out_copy;
    int out_drop;
    int dma_wait;
    int aggr_out;
    int aggr_in;
    int o_packet;
    int i_packet;
};

struct vfe_diag vfediag = {0,};

#define TRC_UVDMA(i,bd1,bd2,bd3) { \
	Trc_bd_buf[Trc_bd_idx].idx = i; \
	Trc_bd_buf[Trc_bd_idx].bdp1 = (__uint32_t)bd1; \
	Trc_bd_buf[Trc_bd_idx].bdp2 = (__uint32_t)bd2; \
	Trc_bd_buf[Trc_bd_idx].bdp3 = (__uint32_t)bd3; \
	Trc_bd_idx ++; \
	if (Trc_bd_idx == 128) Trc_bd_idx = 0; \
}

#define TRC_UVDMALD(vfe) { \
	Trc_dma_buf[Trc_dma_idx].val1 = (volatile __uint32_t)(vfe)->dctl; \
	Trc_dma_buf[Trc_dma_idx].val2 = (volatile __uint32_t)(vfe)->dtbc; \
	Trc_dma_buf[Trc_dma_idx].val3 = (volatile __uint32_t)(vfe)->dla; \
	Trc_dma_buf[Trc_dma_idx].val4 = (volatile __uint32_t)(vfe)->dva; \
	Trc_dma_buf[Trc_dma_idx].val5 = (volatile __uint32_t)(vfe)->dcpp; \
	Trc_dma_buf[Trc_dma_idx].val6 = (volatile __uint32_t)Trc_bd_idx; \
	Trc_dma_idx ++; \
	if (Trc_dma_idx == 32) Trc_dma_idx = 0; \
	Trc_dma_buf[Trc_dma_idx].val1 = (volatile __uint32_t)0xdeadbeef; \
}
#else
#define TRC_UVDMA(i,bd1,bd2,bd3)
#define TRC_UVDMALD(vfe)
#endif

/* for RSVP support */
#define  VFE_GET_TXFREE(cnt, last, next) { \
        if (last > next) \
                cnt = - (next - last); \
        else \
                cnt = (TX_QUEUE_NO -1) - (next - last); \
}

struct 	ifqueue txfullq;
int	vfe_droprate = 10;

/*----------------------------------------------------------------------------
 * Procedure/Function naming convention:
 * Function name begins with
 *
 *   uv_xxx - Universe related functions that has to do either controlling or
 *            using services of the Universe chip.
 *   tl_xxx - TI ThunderLAN chip related functions that has to do with the
 *            TLAN chip operation.
 *----------------------------------------------------------------------------
 */

/*----------------------------------------------------------------------------
 * uv_create_dmalist() --
 *
 * This function is used to create Universe DMA command packet list.
 * Input arguments:
 *     vfep     - Device struct, since each board has its own DMA list.
 *     headp    - Starting address of the in-core DMA command list
 *     pci      - Starting address of the PCI DMA command list
 *     size     - Size of the list (number of command packets per list).
 * Return:
 *     pointer to the tail of the list.
 * Remark:
 *     It is important to know the absolute address boundry in the PCIbus
 *     memory space of the DMA list to be created because the forward
 *     pointers in the command packet structure must be reference from
 *     the PCI bus. Do not be confused by the forward pointers with
 *     the list pointers in the VME slave image. All pointers here are
 *     referenced in VME space except the forward pointers in the list.
 *--------------------------------------------------------------------------*/
u_char *
uv_create_dmalist(fenet_dev_t *fenetp,uv_dmacmd_t *headp,uv_dmacmd_t *pci,int size)
{
    uv_dmacmd_t *tailp,*pcihead;
    int  i;
    /* int dmasize;*/

    if((__psunsigned_t)headp & (__psunsigned_t)0x07)
        ASSERT(0);    /* DMA command packet must be long aligned */

    pcihead = pci;
    for (i = 0, tailp = headp; i < size; i ++)
    {
        tailp->dctl = D_SWAP(DCTL_L2V | DCTL_VDW_32 | DCTL_VAS_A32 | 
		      DCTL_PGM_D | DCTL_SUP_S | DCTL_VCT_SNG);
        tailp->dtbc =
        tailp->dla =
        tailp->resv1 =
        tailp->dva =
        tailp->resv2 = 0;

        /* Note, the forward pointer dcpp must be specified as in PCIbus
         * space
         */
        tailp->dcpp = D_SWAP(PCIADDR(fenetp,(pci+1)));
        tailp ++;
        pci ++;
    }
    /* we'll terminate the list at the last packet */
    (tailp - 1)->dcpp = 0;
    fenetp->uv_dmalistp = pcihead;

    /* map the in-core dma cmmand block */
    (void) dma_map(fenetp->uv_dmamap[0], (caddr_t)headp, 
		      sizeof(fenetp->uv_dmacmd));
    /*printf("uv_create_dmalist: incore_dma_size = %x\n",dmasize);*/
    pcihead->dva =
    headp->dva   = D_SWAP(dma_mapaddr(fenetp->uv_dmamap[0],
			  (caddr_t)(headp+1)));
    headp->dla   = (pciaddr_t)((fenetp->SYNC_RAM) + sizeof(uv_dmacmd_t));
    pcihead->dla = D_SWAP(headp->dla);
    pcihead->dctl =
    headp->dctl   = D_SWAP(DCTL_V2L | DCTL_VDW_64 | DCTL_VAS_A32 | 
			   DCTL_VCT_BLK | DCTL_LD64EN);

/* debug */
    /*printf("in-core = %x, pcihead->dctl=%x, pcihead->dla=%x, pcihead->dva=%x\n",
	    headp,pcihead->dctl,pcihead->dla,pcihead->dva);*/


    return ((u_char *)pci);
}


/* Transmit buffer PCI address alignment lookup table */

static __uint32_t pci_align_tbl[8] = {
0x00000000, 0x01000000, 0x02000000, 0x03000000, 
0x04000000, 0x05000000, 0x06000000, 0x07000000 };

/*----------------------------------------------------------------------------
 * uv_write_dmablock() --
 *
 * This function is used to program a dma block for DMA list operation
 * Input arguments:
 *     dmacmd     - pointer to the DMA block
 *     vfep     - Device struct, since each board has its own DMA list.
 *     pciaddr	  - buffer addr on the PCI side
 *     vmeaddr    - buffer addr on the VME side
 *     size	  - number of bytes to be moved
 *     dir	  - direction of data transfer
 * Return:
 *     Number of bytes out of word alignment
 * Note  :  pciaddr is in little edian format, !! DON'T swap 4 bytes !!
 *--------------------------------------------------------------------------*/
pciaddr_t
uv_write_dmablock(volatile uv_dmacmd_t *dma,
		  dmamap_t *dmamap,
		  pciaddr_t pciaddr,
		  caddr_t vmeaddr,
		  int size,
		  __uint32_t dir)
{
    int dmasize, offset;

/*
    if ( dir == DCTL_L2V )
        printf("uv_write_dmablock(L->V): vmeaddr=%x, pciaddr=%x,size = %d\n",
		vmeaddr,pciaddr,size);
    else
        printf("uv_write_dmablock(V->L): vmeaddr=%x, pciaddr=%x,size = %d\n",
		vmeaddr,pciaddr,size);
*/
    offset = (int)((__psunsigned_t)(vmeaddr) & (__psunsigned_t)0x07);

    /* keep the dma map at 8 byte boundary */
    dmasize = dma_map(dmamap,(vmeaddr-offset),(size+offset));

    if ( dmasize == 0 ) {
	cmn_err_tag(213,CE_ALERT, "vfe: uv_write_dmablock: dmasize(%d) != size (%d)\n",dmasize,size);
	cmn_err_tag(214,CE_ALERT, "vfe: uv_write_dmablock: vmeaddr = %x pciaddr = %x, dir = %x\n",
		vmeaddr, pciaddr, dir);
    }

    /* realign PCI address with vme address for UV_DMA operation */
    if ( dir == DCTL_V2L ) {
	pciaddr += pci_align_tbl[offset];
    }


    dma->dva = D_SWAP(dma_mapaddr(dmamap,vmeaddr));
    dma->dcpp &= ~(PROCESSED|NULLBIT);

    dma->dctl = D_SWAP(dir | DCTL_VDW_64 | DCTL_VAS_A32 | DCTL_VCT_BLK | DCTL_LD64EN);
    dma->dtbc = D_SWAP(size & 0x00ffffff);
    dma->dla = pciaddr;
/*
    printf("uv_write_dmablock:dla=%x <--> dva=%x <--> vmeaddr=%x, align = %x\n",
		dma->dla, dma->dva,vmeaddr,align);
*/
    TRC_UVDMA(size,pciaddr,vmeaddr,PACKDMA_DATA);
    return (pciaddr);
}

/*----------------------------------------------------------------------------
 * uv_start_dma() --
 *
 * This function is used to activate UNIVERSE DMA operation for series of DMA
 * requests queued in the DMA command list.
 * Input arguments:
 *     vfep     - Device struct, since each board has its own DMA list.
 *     test     - test DMA channel, return -1 if busy
 * Return:
 *     0 - succeed.   -1 - device busy
 * Remark:
 *     ????  DCPP value (pointer to the linked list) should be PCI bus memory
 *           space or VME bus space???  The manual is not clear on this...
 *--------------------------------------------------------------------------*/
int
uv_start_dma(fenet_dev_t *fe)
{
    volatile __uint32_t ureg;
    ureg = 0;
    UVREG_WR(fe,DTBC,ureg);
    ureg = (__uint32_t) (PCIADDR(fe,(fe->uv_dmalistp)));
    UVREG_WR(fe,DCPP,ureg); 	/* program the DMA cmd packet pointer */
    ureg = DGCS_GO|DGCS_CHAIN|DGCS_IDONE|DGCS_ILERR|DGCS_IVERR|DGCS_IMERR;
    UVREG_WR(fe,DGCS,ureg);	/* Write to DMA General control register */

    TRC_UVDMA(0,0,0,DMASTARTED);
    TRC_UVDMALD(fe->uv_dmalistp);

    return 0;
}



/*----------------------------------------------------------------------------
 * uv_restart_dma() --
 *
 * This function is called when DMA error conditions are encountered
 * Input arguments:
 *     vfep     - Device struct, since each board has its own DMA list.
 * Return:
 *     0 - succeed.   -1 - device busy
 * Remark:
 *     When DMA error is detected by the Universe chip, the DMA command list
 *     should still be good for restarting the DMA if we can go into the list
 *     fix the "P" (Processed) bits.
 *--------------------------------------------------------------------------*/
void
uv_restart_dma(fenet_dev_t *fe)
{
    volatile uv_dmacmd_t *dma;
    volatile __uint32_t  dla,dva,dgcs;
    int i;


    UVREG_RD(fe,DLA,dla);
    UVREG_RD(fe,DVA,dva);
    UVREG_RD(fe,DGCS,dgcs);
    cmn_err_tag(215,CE_ALERT, "vfe board %d: dma_error at DGCS = %x, DVA = %x, DLA = %x\n",fe->uv_unit, dgcs,dva,dla);
    for (i=0;i < 20; i++) {
	/* printf("dctl = %x, dtbc = %x, dla = %x, dva = %x, dcpp = %x\n", 
		dma->dctl,dma->dtbc,dma->dla,dma->dva,dma->dcpp);
	*/
	dma->dcpp &= ~PROCESSED;	/* mark this block not processed */
	dma ++;
    } 
/*  uv_start_dma(fe); */
    printf("uv_restart_dma: Restarting DMA...\n");
}



/*----------------------------------------------------------------------------
 * uv_rcv_dmadone() -
 *
 * This function is called when a DMA operation associated with the TLAN 
 * receive list is complete.
 * Input arguments:
 *     dma     - The dma request block associated with the receive bd list
 * Return:
 *     0 - succeed, -1 - ???
 * Algorithm:
 *     The request block contains the pointer to the pmc port structure,
 *     the BD tied to the operation with which there should be enough info
 *     to perform the receive list clean up/recycling steps.
 *--------------------------------------------------------------------------*/
void
uv_rcv_dmadone(uv_dmatbl_t *dma,int abort, u_short *type)
{
    volatile tlan_rxbd_t *bdp;
    pmc_dev_t 	       *pmc;
    struct ether_header *eh;

    /* Since the receive list is not a circular list, to reclaim the BD
     * when the DMA is done, we shall put those BDs back to the list by
     * "extending the end of the list to include those BDs
     */ 
    pmc = dma->pmc;
    bdp = pmc->tl_rxbdlist[RX_NEXT];


    /* relink the list at the end(pointed by RX_NEXT)
     * and terminate the list at the end of the new BD
     */
    bdp->bd_next = (pciaddr_t) D_SWAP(PCIADDR(pmc->feinfo,dma->bd));
    dma->bd->bd_next = 0;
    INC_BD(pmc->tl_rxbdlist[RX_BASE],bdp,pmc->tl_rxbdlist[RX_LIMT]);

    TRC_UVDMA(0,bdp,dma->bd,DMADONE_RX);

/*  ASSERT ( bdp == (tlan_rxbd_t *)dma->bd ); make sure we don't mess up the tail */
    if( bdp != (tlan_rxbd_t *)dma->bd )  {
        cmn_err_tag(216,CE_ALERT, "vfe%d: About to die .. bdp = %x, dmabd = %x\n",pmc->vfe_num, bdp,dma->bd);
        ASSERT(0);
    }


    /* fix up the BD header to have it recycled
    cstat = RXCSTAT_REQ;
    SWAP_2(cstat);  
    dma->bd->bd_cstat = RXCSTAT_REQ; */
    dma->bd->bd_frame_sz = 0;         /* filled by TLAN */
    /* not sure if TLAN would update bd_data_cnt field??, otherwise
     * the field should have been set at its creation time....
    dma->bd->bd_data_cnt = D_SWAP(BUFFSIZE | F_LASTFRAG);
     */

    /* remember, RX_NEXT always points at the end of receive list */
    pmc->tl_rxbdlist[RX_NEXT] = bdp;

    if ((abort)||(dma->mb == 0))
	return ;

    /* Now, let's worry about the mbuf which had just received the frame */
    pmc->pmc_if.if_ipackets++;

    /* maybe we should check this in rcv_eof(), to save
     * time doing Rcv_DMA at all...
     */

    if (iff_dead(eiftoifp(&pmc->pmcif)->if_flags)) {
       if ( eiftoifp(&pmc->pmcif)->if_flags & IFF_DEBUG )  
	  printf("uv_rcv_dmadone: iff died!!\n"); 
	m_freem(dma->mb);
	return;
    }

    /* before send the packets up to ether_input, the packet
     * type will be saved. if it is ARP request, the ARP respond
     * will be xmit right away. bug#571220
     */
    eh=&mtod(dma->mb, struct etherbufhead *)->ebh_ether;
    *type = ntohs(eh->ether_type);


    /* snoopflag is always 0, we don't report bad frames,
     * since bad frames are blocked even at the PCI memory
     * side, they were not DMA over to the kernel space.
     * we only DMA the good frames...
     */
    ether_input(&pmc->pmcif,0,dma->mb);
    return ;
}
    

/*----------------------------------------------------------------------------
 * uv_packdma_packet() -
 *
 * This function is used to construct a DMA command list for Data transfer on
 * both the receive and transmit side of the PMC.
 * Input arguments:
 *     pmc      - TLAN control block
 * Return:
 *     0 - succeed, -1 - ???
 * Algorithm:
 *     pmc control block contains both receive and transmit list window point-
 *     ers. By looking up the pointer sets, we know exactly how many receive
 *     or transmit buffer need to be transferred over the VME bus. All vital
 *     information about the individual frames are stored away to be used when
 *     the DMA done interrupt comes back.
 *     For receive frame, the receive frames to be moved are identified 
 *     between RX_CURR and RX_LAST pointers.
 *--------------------------------------------------------------------------*/
int
uv_packdma_packet(pmc_dev_t *pmc)
{
    volatile tlan_rxbd_t *rxbdp;	/* pointer to RX BD list */
    volatile tlan_txbd_t *txbdp;	/* pointer to TX BD list */
    volatile uv_dmacmd_t *dmap; /* pointer to the DMA list */
    fenet_dev_t *fe;		/* device pointer */
    struct mbuf *mb; 		/* message buffer pointer */
    u_short      fsize;
    register s;

    pmc_dev_t *nextpmc = 0, *orig_pmc = pmc;
    int i;
    /* bug 651621 */
    int tx_list = 1, rcv_list = 1;
    int tx_list_max, rcv_list_max;
    int nextpmc_up = 0;

   

    fe = pmc->feinfo;
    fe->uv_lastpmc = pmc;

    VFEDMA_LOCK(fe, s);
    if (fe->uv_stat & UV_DMARDY) {
        fe->uv_stat &= ~UV_DMARDY;
    } else {
        VFEDMA_UNLOCK(fe, s);
        return 0;
    }

    VFEDMA_UNLOCK(fe, s);

    /* start with [1] element of the in-core dma_cmd_array... */
    i = 1;
    dmap = &fe->uv_dmacmd[i];

    /* bug 651621 */
    nextpmc = OTHERPMC(pmc);
    nextpmc_up = 0;
    if (eiftoifp(&nextpmc->pmcif)->if_flags & IFF_UP )
    {
        nextpmc_up = 1;
        tx_list_max = DMA_LIST_NO/2 - 6;
        rcv_list_max = DMA_LIST_NO/2 - tx_list_max;
    } else
    {
        tx_list_max = DMA_LIST_NO - 12;
        rcv_list_max = DMA_LIST_NO - tx_list_max;
    }

next_pmc:
    /* Pack the DMA list with the transmit list , pick up the first xmt mbuf at
     * txq_geti and stops at txq_puti. Don't use up all the DMA list, leave
     * a few for the receive frames as well.
     */
    for (; i < DMA_LIST_NO;)
    {
	struct mbuf *m0;
	int len, first;


	if ( pmc->txq_geti == pmc->txq_puti )
	    /* Transmit queue is empty, nothing to send */
	    break;
	/* bug 651621 */
        if (vfe_debug && nextpmc)
                printf("nextpmc: something in TX \n");

        mb = pmc->txq_list[pmc->txq_geti];

	/*
	 * Count the number of mbuf to go out in the packet.
	 */
	for (m0 = mb, len = 0,first = i; m0; m0 = m0->m_next) {
            len += m0->m_len;
	    first ++;
	}
	/* we can't have the transmit queue out last DMA list */
	if ( first >= DMA_LIST_NO ) {
/*printf("uv_packdma_packet: Not enough DMA list to transmit packet %d, %d...\n",first,i);  */
/*	    cmn_err(CE_ALERT, "uv_packdma_packet: Not enough DMA list to transmit packet %d, %d...\n",first,i); */
	    break;
	}

	if ( len >= BUFFSIZE ) {
printf("uv_packdma_packet: Tx packet length too great: %d\n",len);
            if ( eiftoifp(&pmc->pmcif)->if_flags & IFF_DEBUG )  
	       printf("uv_packdma_packet: Tx packet length too great: %d\n",len);
	    m_freem(mb);
	    continue;
	}

	if ((txbdp = tl_get_txbd(pmc,len)) == 0 )
/*
printf("uv_packdma_packet: transmit list is currently full. try later\n");
*/
	    /* transmit list is currently full. try later */
	    break;
	pmc->txq_geti = (pmc->txq_geti + 1) % (TX_QUEUE_NO - 1);

        /* go set up the DMA command packet */
	{
	    /* Transmit mbuf may be chained. Need to walk through the chain
 	     * and set up DMA command packet for each mbuf in the chain
	     */
	
	    pciaddr_t bufp;
	    int bufseg;

	    bufseg = 0;
	    first = 1;
    	    pmc->pmc_if.if_obytes += len;
	    bufp=(__uint32_t)txbdp->bdbuf[0].bd_bufp;
	    bufp &= 0xf8ffffff;
	    for (; mb; mb = mb->m_next)
	    {
		if (mb->m_len)
		{
		    fsize = mb->m_len;
		    /* go set up the DMA command packet */
		    bufp = uv_write_dmablock(dmap,fe->uv_dmamap[i],bufp,
					      mtod(mb,caddr_t),
			  		      fsize,DCTL_V2L);
		    ASSERT(len >= fsize);
		    len -= fsize;
		    if ( first ) {
			first = 0;
        	        fe->uv_dmarec[i].event = TX_FRST;
		    }
		    else {
        	        fe->uv_dmarec[i].event = TX_MORE;
		    }

		    /* realign databuf pointer in the BD... */
		    txbdp->bdbuf[bufseg].bd_bufp = bufp;
		    
		    if (len > 0)
			/* there are more coming.... */
			txbdp->bdbuf[bufseg].bd_data_cnt = D_SWAP(fsize);
		    else
			/* This is the last mbuf in the chain */
			txbdp->bdbuf[bufseg].bd_data_cnt = D_SWAP(fsize|F_LASTFRAG);

	   	    bufp = D_SWAP(bufp);

        	    /* keep a record of the DMA command */
		    fe->uv_dmarec[i].pmc = pmc;
        	    fe->uv_dmarec[i].bd = (tlan_txbd_t *)txbdp;
        	    fe->uv_dmarec[i].mb = mb;
        	    fe->uv_dmarec[i].dbuf = bufp;

        	    TRC_UVDMA(i,txbdp,0,PACKDMA_TX);

        	    dmap ++; i ++; bufseg ++;
		    tx_list++;	 /* bug 651621 */

		    bufp += fsize;
		    while ( bufp & 0x07 )
			bufp ++;
	   	    bufp = D_SWAP(bufp);
		}
		/* thie first mbuf in chain must have something */
		ASSERT ( first == 0); 
	    }
	    /* we can't have the transmit queue out last DMA list */
	    /* bug 651621 */
            if (tx_list >= tx_list_max)
                break;
            /* if ( i > (DMA_LIST_NO - 8) ) break; */

	}
    }
    /* let the receive list use the reset of the DMA list if any left.
     * Starting with the receive BD at RX_LAST
     */
    rxbdp = pmc->tl_rxbdlist[RX_LAST];
    for (; i < DMA_LIST_NO;)
    {
	struct rcvbuf *rif_buf;
	if ( rxbdp == pmc->tl_rxbdlist[RX_CURR] )
	    /* There is nothing in the receive list for DMA work */
	    break;
	fsize = rxbdp->bd_frame_sz;
        SWAP_2(fsize);
	fsize -= CRC_LEN;	/* discard CRC bytes */
/*	ASSERT(fsize < MAX_RPKT);  */
	if (! (mb = m_vget(M_DONTWAIT,sizeof(struct rcvbuf),MT_DATA)))
	    break;
	rif_buf = mtod(mb, struct rcvbuf *);
	IF_INITHEADER(rif_buf, &pmc->pmc_if, sizeof(struct etherbufhead));
	/* go set up the DMA command packet */
	uv_write_dmablock(dmap,fe->uv_dmamap[i],(__uint32_t)rxbdp->bd_bufp,
			  (caddr_t)&rif_buf->ebh.ebh_ether, 
			  fsize,DCTL_L2V);

	/* keep a record of the DMA command */
	fe->uv_dmarec[i].pmc = pmc;
	fe->uv_dmarec[i].bd = (tlan_txbd_t *)rxbdp;
	fe->uv_dmarec[i].mb = mb;
	fe->uv_dmarec[i].dbuf = rxbdp->bd_bufp;
	fe->uv_dmarec[i].event = RX_CMP;

        TRC_UVDMA(i,rxbdp,pmc->tl_rxbdlist[RX_CURR],PACKDMA_RX);

        dmap ++;
	i ++;
	rcv_list++; /* bug 651621 */
        INC_BD(pmc->tl_rxbdlist[RX_BASE],rxbdp,pmc->tl_rxbdlist[RX_LIMT]);
    	pmc->pmc_if.if_ibytes += fsize;
	mb->m_len = (fsize - EHDR_LEN ) + sizeof(struct etherbufhead);
	/* bug 651621 */
	if (rcv_list >= rcv_list_max)
                break;

    }
    /* move the receive list window up */
    pmc->tl_rxbdlist[RX_LAST] = rxbdp;
    /* bug 651621 */
    if (nextpmc_up) {
        nextpmc =
        pmc     = OTHERPMC(pmc);
        tx_list =  rcv_list = 1;
        nextpmc_up = 0; /* reset it, as we don't want to go back next time */
        goto next_pmc;
    }
/*
    if (nextpmc == 0 ) {
	nextpmc =
	pmc 	= OTHERPMC(pmc);
	goto next_pmc;
    }
*/

    if ( dmap == &fe->uv_dmacmd[1] ) {
	VFEDMA_LOCK(fe, s);
        fe->uv_stat |= UV_DMARDY;
        VFEDMA_UNLOCK(fe, s);
	return 0;
    } else {
	VFEDMA_LOCK(fe, s);
	fe->uv_lastpmc = orig_pmc;
	VFEDMA_UNLOCK(fe, s);
    }

    (dmap-1)->dcpp |= NULLBIT;		/* mark for last command packet */
    fe->uv_dmarec[i].pmc = 0;		/* mark the end or the array */

    fe->uv_dmalistp->dtbc = D_SWAP((i-1) * sizeof(uv_dmacmd_t));
    fe->uv_dmalistp->dcpp = D_SWAP(PCIADDR(fe,(fe->uv_dmalistp + 1)));
    /* OK, I think the DMA list is built. Now, send it to  Universe for
     * action. The DMA command list pointer better be in PCI address space
     */
    uv_start_dma(fe);

    return 1;
}


/*----------------------------------------------------------------------------
 * uv_xmt_dmadone() -
 *
 * This function is called when a DMA operation associated with the TLAN
 * transmit list is complete.
 * Input arguments:
 *     dma     - The dma request block associated with the receive bd list
 * Return:
 *     0 - succeed, -1 - ???
 * Algorithm:
 *     The request block contains the pointer to the pmc port structure,
 *     the BD tied to the operation with which there should be enough info
 *     to prepare the transmit list for TLAN transmission. At this time we
 *     have no need for the transmit mbuf. Transmit BD header should have
 *     been filled out prior the DMA operation (in tl_get_txbd).
 *--------------------------------------------------------------------------*/
void
uv_xmt_dmadone(uv_dmatbl_t *dma)
{
    if (dma->event == TX_FRST)
	m_freem(dma->mb);
    /* else nop, the mbuf was part of the mbuf chain freed eariler */
    dma->mb = 0;

    TRC_UVDMA(0,dma->bd,0,DMADONE_TX);
}

/*----------------------------------------------------------------------------
 * uv_dma_done() -
 *
 * This function is called by the interrupt routine when Universe DMA command
 * list is complete. The DMA command list is to be cleaned up, BD lists are
 * to be recycled, buffers are to be freed..
 * Input arguments:
 *     fe      - Device control block
 *     stat    - DMA complete status
 * Return:
 *     0 - succeed, -1 - ???
 * Algorithm:
 *     When the DMA command list is built (in uv_packdma_packet) the DMA req
 *     records are also kept in the array:v_dmarec[] of the fe structure.
 *     When the DMA operation is done, we shall walk through the array for
 *     clean up procedures:
 *     For Rcv frames, the receive BD list window needs updated. Buffers are
 *     to be released by marking the BD headers.
 *     For Xmt frames, We need to prepare the transmit list for TLAN trans-
 *     mission; release those transmit mbufs...
 * Note: uv_packdma_packet() is called on per port (PMC) basis, however, the
 *       Universe DMA operation has no such concept. The two PMC ports
 *       will simply have to in turn use the same DMA channel. 
 *--------------------------------------------------------------------------*/
int
uv_dma_done(fenet_dev_t *fe,int abort, u_short *type, struct ifqueue * localqp)
{
    pmc_dev_t *pmc;
    uv_dmatbl_t *drec;  	 /* pointer to the DMA req. records */
    int i;

    drec = &fe->uv_dmarec[1];	/* get the DMA operation records */
    ASSERT (drec->pmc);		/* There better be something in the array */

    while (drec->pmc)
    {
        pmc = drec->pmc;
	switch (drec->event)
	{
	    case RX_CMP:
		/* this is when one receive frame has been DMA into the 
	 	 * holding mbuf, the receive is complete.
		 */
		uv_rcv_dmadone(drec,abort,type);
		break;
            case TX_FRST:
                /* this is when the transmit mbuf has been DMA over into the
                 * holding TLAN transmit list. Should fix up the list for
                 * TLAN transmission.
                 *
                 * local queued and replace uv_xmt_dmadone(drec);
                 */
                IF_ENQUEUE_NOLOCK(localqp, drec->mb);
                /* fall thru */
            case TX_MORE:
                drec->mb = 0;
                TRC_UVDMA(0,dma->bd,0,DMADONE_TX);
		break;
	    default:
		ASSERT (0);	/* should never get here */
	}
	drec->pmc = 0;		/* nill out the record */
	drec ++;
    }

    for (i=0; i < 2; i++) {  
    
	pmc = &fe->pmc[i];

	if ( pmc->pend_reset ) {
       		if ( eiftoifp(&pmc->pmcif)->if_flags & IFF_DEBUG )  
			printf("uv_dma_done: do pmc->pend_reset\n");
		pmc->pend_reset = 0;
		tl_rebuild_struct(pmc);
		if ( pmc->pend_init ) {
		    pmc->pend_init = 0;
       		    if ( eiftoifp(&pmc->pmcif)->if_flags & IFF_DEBUG )  
			printf("uv_dma_done: do pend_init\n");
		    vfe_init(&pmc->pmcif,0);
		}
		continue;
	}
	if (pmc->pmc_opstat & PMC_RXOVRUN) {
	    tl_receive_enable(pmc);
	    pmc->pmc_opstat &= ~PMC_RXOVRUN;
	}
        if ( (pmc->pmc_opstat & PMC_TXBSY) || 
	     (tl_end_txlist(pmc) == -1 ) )
	    continue;
	else {
	    if ( tl_start_transmit(pmc) == -1 )
                if ( eiftoifp(&pmc->pmcif)->if_flags & IFF_DEBUG )  
        	   printf("dma_done: lan port is not up or Tx channel is busy !\n");
	}
    }

    return 0;
}



/*----------------------------------------------------------------------------
 * tl_dio_write() --
 *
 * This function is used to DIO write to TLAN internal registers
 * Input arguments:
 *     pmc      - TLAN control block
 *     reg      - DIO register to write to
 *     val      - byte vaule to write to the register
 *     type     - DIO Rd/Wr type
 * Return:	n/a
 *
 * Remark:
 *	    The DIO_DATA access uses 32 bit PCI data transfers with full byte
 *	    control, following the normal PCI conventions.  
 *--------------------------------------------------------------------------*/
void
tl_dio_write(pmc_dev_t *pmc,u_short reg, u_int val,int type)
{
    dio_reg diodata;
    volatile u_char *daddr;

    diodata.word = (ushort)reg;
    SWAP_2(diodata.word);
    pmc->tlan_reg->dio_addr = diodata.word;
    daddr = (u_char *)&pmc->tlan_reg->dio_data;
    switch ( type )
    {
	case DIO_BYTE:
	    diodata.byte = (u_char) val;
	    daddr += (reg & 0x03);
            *(u_char *)daddr = diodata.byte;
	    break;
	case DIO_WORD:
	    diodata.word = (ushort) val;
            SWAP_2(diodata.word);
	    daddr += (reg & 0x02);
            *(ushort *)daddr = diodata.word;
	    break;
	default:
	    ASSERT(0);
    }
}

/*----------------------------------------------------------------------------
 * tl_dio_readB() --
 *
 * This function is used to DIO BYTE read from TLAN internal registers
 * Input arguments:
 *     pmc      - TLAN control block
 *     reg      - DIO register to write to
 * Return:	val - byte vaule read from the register
 *
 * Remark:
 *--------------------------------------------------------------------------*/
u_char
tl_dio_readB(pmc_dev_t *pmc,u_short reg)
{
    u_char diodata;
    u_short dreg;
    volatile u_char *daddr;

    dreg = reg;
    SWAP_2(dreg);
    pmc->tlan_reg->dio_addr = dreg;
    daddr = (u_char *)&pmc->tlan_reg->dio_data + (reg & 0x03);
	
    diodata = *daddr;
    return diodata;
}


/*----------------------------------------------------------------------------
 * tl_dio_readW() --
 *
 * This function is used to DIO WORD read from TLAN internal registers
 * Input arguments:
 *     pmc      - TLAN control block
 *     reg      - DIO register to write to
 * Return:	val - byte vaule read from the register
 *
 * Remark:
 *--------------------------------------------------------------------------*/
u_short
tl_dio_readW(pmc_dev_t *pmc,u_short reg)
{
    ushort diodata;
    u_short dreg;
    volatile u_char *daddr;

    dreg = reg;
    SWAP_2(dreg);
    pmc->tlan_reg->dio_addr = dreg;
    daddr = (u_char *)&pmc->tlan_reg->dio_data + (reg & 0x02);
    diodata = (u_short)*daddr;
    SWAP_2(diodata);
    return diodata;
}


/*----------------------------------------------------------------------------
 * tl_dio_readL() --
 *
 * This function is used to DIO LONG word (4 byte) read from TLAN internal 
 * registers.
 * Input arguments:
 *     pmc      - TLAN control block
 *     reg      - DIO register to write to
 * Return:	n/a
 *
 * Remark:
 *--------------------------------------------------------------------------*/
u_int
tl_dio_readL(pmc_dev_t *pmc,u_short reg)
{
    u_short dreg;
    u_int diodata, swapped;

    if (reg & 0x03)  return (0);	/* reg must be long aligned */
    dreg = reg;
    SWAP_2(dreg);
    pmc->tlan_reg->dio_addr = dreg;
    
    diodata = (u_int)(pmc->tlan_reg->dio_data);
    swapped = (u_int)D_SWAP(diodata);
    return(swapped);

}


/*----------------------------------------------------------------------------
 * tl_mii_sync() --
 *
 * This function is used to send Sync. pattern over the MII interface as 
 * required by the MII management interface serial protocol
 * Input arguments:
 *     pmc      - TLAN control block
 * Return:	n/a
 *
 * Remark:
 *	    sync pattern is a series of 32 logic ones on MDIO pin.
 *
 *--------------------------------------------------------------------------*/
void
tl_mii_sync(pmc_dev_t *pmc)
{
    u_char diodata;
    register int i;

    diodata = tl_dio_readB(pmc,NET_SIO);
    diodata &= ~MTXEN;
    tl_dio_write(pmc,NET_SIO,diodata,DIO_BYTE);
    diodata &= ~MCLK;
    tl_dio_write(pmc,NET_SIO,diodata,DIO_BYTE);

    /* delay for a while */
    diodata = tl_dio_readB(pmc,NET_SIO);

    diodata |= MCLK;
    tl_dio_write(pmc,NET_SIO,diodata,DIO_BYTE);

    /* delay for a while */
    diodata = tl_dio_readB(pmc,NET_SIO);

    diodata |= NMRST;
    tl_dio_write(pmc,NET_SIO,diodata,DIO_BYTE);

    /* provide 32 sync cycles, the PHY's pull-up resistor
     * will pull the MDIO line to a logic one for us, all
     * it needs is clocking signals on MCLK line  
     */
    for (i = 0; i < 32; i++)
    {
	diodata &= ~MCLK;
	tl_dio_write(pmc,NET_SIO,diodata,DIO_BYTE);
	diodata |= MCLK;
	tl_dio_write(pmc,NET_SIO,diodata,DIO_BYTE);
    }
}

/*----------------------------------------------------------------------------
 * tl_mii_read() --
 *
 * This function is used to do MII frame read from the PHY device
 * Input arguments:
 *     pmc      - TLAN control block
 *     dev      - PHY device number
 *     reg      - PHY register to read from
 *    *pval     - storage for data read
 * Return:	OK (0) on success, NO_ACK (1) on failure
 *
 * Remark:
 *	    See ThunderLAN Spec PG2.0 Rev 0.41, starting at page 64 for
 *	    detail programming guide.
 *
 * read word from PHY MII
 * MII frame read format:
 *
 *   start    operation  PHY      reg      turn          Data
 * delimiter    code     addr     addr    around
 * ----------------------------------------------------------------------
 * |   01   |   10   |  AAAAA  |  RRRRR  |  Z0  |  DDDD.DDDD.DDDD.DDDD  |
 * ----------------------------------------------------------------------
 *
 *--------------------------------------------------------------------------*/
int
tl_mii_read(pmc_dev_t *pmc,u_char dev,u_char reg, u_short *pval)
{
    u_char diodata, sio;
    int i;

    /* Disable MII and PHY interrupts; enable MII transmit */
    TLAN_MII_CLR(pmc,MINTEN);
    TLAN_MII_CLR(pmc,MCLK);
    TLAN_MII_SET(pmc,MTXEN);

    /* Send bit stream 01 (start delimiters for MII interface) */
    TLAN_MII_CLR(pmc,MDATA); TLAN_MII_CLCK(pmc);
    TLAN_MII_SET(pmc,MDATA); TLAN_MII_CLCK(pmc);

    /* Send bit stream 10 (read operation) */
    TLAN_MII_SET(pmc,MDATA); TLAN_MII_CLCK(pmc);
    TLAN_MII_CLR(pmc,MDATA); TLAN_MII_CLCK(pmc);

    /* Send 5-bit device number (0/External, 0x1f/internal), MSB first */
    for (i = 0x10;i;i >>= 1)
    {
        if (i & dev)
	{
            TLAN_MII_SET(pmc,MDATA);
	}
        else
	{
            TLAN_MII_CLR(pmc,MDATA);
	}
        TLAN_MII_CLCK(pmc);
    }

    /* Send 5-bit register number, MSB first */
    for (i = 0x10;i;i >>= 1)
    {
        if (i & reg)
	{
            TLAN_MII_SET(pmc,MDATA);
	}
        else
	{
            TLAN_MII_CLR(pmc,MDATA);
	}
        TLAN_MII_CLCK(pmc);
    }

    /* Clear TX enable, Wait "turn-around cycles" for returned data */
    TLAN_MII_CLR(pmc,MTXEN); TLAN_MII_CLCK(pmc);

    /* Check for Ack/Nak */
    sio = tl_dio_readB(pmc,NET_SIO) & MDATA;
    TLAN_MII_CLCK(pmc);

    /* Now clock out the data regardless of Ack or Nak */
    for (*pval = 0,i = 0x8000; i; i >>=1)
    {
        if ( tl_dio_readB(pmc,NET_SIO) & MDATA )
            *pval |= i;
        TLAN_MII_CLCK(pmc);
    }

    /* Re-enable PHY interrupts */
    TLAN_MII_CLCK(pmc);
    TLAN_MII_SET(pmc,MINTEN);

    /* end MII read with sync pattern */
    for (i=0; i < 32; i++)
    {
        TLAN_MII_SET(pmc,MDATA);
        TLAN_MII_CLCK(pmc);
    }
    if ( sio )
    {
        *pval = 0xffff;
        return (-1);
    }
    return (0);
}


/*----------------------------------------------------------------------------
 * tl_mii_write() --
 *
 * This function is used to do MII frame write to the PHY device
 * Input arguments:
 *     pmc      - TLAN control block
 *     dev      - PHY device number
 *     reg      - PHY register to read from
 *     data     - storage for data read
 * Return:	n/a
 *
 * Remark:
 *	    See ThunderLAN Spec PG2.0 Rev 0.41, starting at page 64 for
 *	    detail programming guide.
 *
  write word to PHY MII
 * MII frame write format:
 *
 *   start    operation  PHY      reg      turn          Data
 * delimiter    code     addr     addr    around
 * ----------------------------------------------------------------------
 * |   01   |   01   |  AAAAA  |  RRRRR  |  10  |  DDDD.DDDD.DDDD.DDDD  |
 * ----------------------------------------------------------------------
 *--------------------------------------------------------------------------*/
void
tl_mii_write(pmc_dev_t *pmc,u_char dev,u_char reg, u_short data)
{
    u_char diodata;
    int i;

    /* Disable MII and PHY interrupts; enable MII transmit */
    TLAN_MII_CLR(pmc,MINTEN);
    TLAN_MII_CLR(pmc,MCLK);
    TLAN_MII_SET(pmc,MTXEN);

    /* Send bit stream 01 (start delimiters for MII interface) */
    TLAN_MII_CLR(pmc,MDATA); TLAN_MII_CLCK(pmc);
    TLAN_MII_SET(pmc,MDATA); TLAN_MII_CLCK(pmc);

    /* Send bit stream 01 (write operation) */
    TLAN_MII_CLR(pmc,MDATA); TLAN_MII_CLCK(pmc);
    TLAN_MII_SET(pmc,MDATA); TLAN_MII_CLCK(pmc);

    /* Send 5-bit device number (0/External, 0x1f/internal), MSB first */
    for (i = 0x10;i;i >>= 1)
    {
        if (i & dev)
	{
            TLAN_MII_SET(pmc,MDATA);
	}
        else
	{
            TLAN_MII_CLR(pmc,MDATA);
	}
        TLAN_MII_CLCK(pmc);
    }

    /* Send 5-bit register number, MSB first */
    for (i = 0x10;i;i >>= 1)
    {
        if (i & reg)
	{
            TLAN_MII_SET(pmc,MDATA);
	}
        else
	{
            TLAN_MII_CLR(pmc,MDATA);
	}
        TLAN_MII_CLCK(pmc);
    }

    /* Turn around Send bit stream 10 */
    TLAN_MII_SET(pmc,MDATA); TLAN_MII_CLCK(pmc);
    TLAN_MII_CLR(pmc,MDATA); TLAN_MII_CLCK(pmc);

    /* write data to MII, MSB first */

    for (i = 0x8000; i; i >>= 1)
    {
        if ( data & i )
	{
            TLAN_MII_SET(pmc,MDATA);
	}
        else
	{
            TLAN_MII_CLR(pmc,MDATA);
	}
        TLAN_MII_CLCK(pmc);
    }

    /* Disable MII Transmit */
    TLAN_MII_CLR(pmc,MTXEN);

    /* Re-enable PHY interrupts */
    TLAN_MII_CLCK(pmc);
    TLAN_MII_SET(pmc,MINTEN);

    /* end MII read with sync pattern */
    for (i=0; i < 32; i++)
    {
        TLAN_MII_SET(pmc,MDATA);
        TLAN_MII_CLCK(pmc);
    }
}



/*----------------------------------------------------------------------------
 * tl_get_phyreg() --
 *
 * This function is used to read PHY registers via  MII management interface 
 * serial protocol
 * Input arguments:
 *     pmc      - TLAN control block
 * Return:	n/a
 *
 * Remark:
 *--------------------------------------------------------------------------*/
void
tl_get_phyreg(pmc_dev_t *pmc, uint_t *valp, int print_console)
{
    u_short miireg;
    /*u_char  eeprom,promaddr;*/
    uint_t *temp = valp;
    int i;


/*
    for (promaddr=0; promaddr< 0xcf; promaddr++)
    {
    if ( tl_eeprom_read(pmc,promaddr,&eeprom) == -1 )
	printf("EEPROM read at 0x%x error!!\n",promaddr);
    else 
	printf("EEPROM read at 0x%x = 0x%x\n",promaddr,eeprom);
    }
*/

    for (i = 0; i < MAX_COUNT; i++)
    {
	tl_mii_read(pmc, pmc->phy,i,&miireg);	
	if (print_console)
		printf("MII 0x%x = 0x%x\n",i, miireg);
	*temp++ = miireg;
    }

/*
    for (i=0; i<4; i++)
    {
	u_char mac;

	printf("MAC[%x]: ",i);

	for (j=0; j<6; j++)
	{

	    mac = tl_dio_readB(pmc,NET_AREG0+(6 * i)+j);
	    printf(" %x ", mac);
	}
	printf("\n");
    }
*/

}




/*----------------------------------------------------------------------------
 * tl_eeprom_read() --
 *
 * This function is used to read the contents in the EEPROM
 * Input arguments:
 *     pmc      - TLAN control block
 *     addr     - EEPROM address to read
 *    *pval     - storage for data read
 * Return:	OK (0) on success, NO_ACK (1) on failure
 *
 * Remark:
 *	    There is no information about how EEPROM access is done,
 *	    the following code is taken from if_ecf driver 
 *--------------------------------------------------------------------------*/
int
tl_eeprom_read(pmc_dev_t *pmc,u_char addr, u_char *pval)
{
    u_char diodata, bits,sio;

    *pval = 0xaa;
    /* send eeprom start sequence */
        TLAN_EEPROM_SET(pmc, ECLOK);
        TLAN_EEPROM_SET(pmc, EDATA);
        TLAN_EEPROM_SET(pmc, ETXEN);
        TLAN_EEPROM_CLR(pmc, EDATA);
        TLAN_EEPROM_CLR(pmc, ECLOK);


    /* Send type (1010), addr (000) and write (0) */
    bits = 0xa0;
   
    /* write device id */
    for (sio = 0x80; sio; sio >>= 1) {
        if (bits & sio)
                {TLAN_EEPROM_SET(pmc, EDATA);}
        else
                {TLAN_EEPROM_CLR(pmc, EDATA);}

        TLAN_EEPROM_CLCK(pmc);
    }

    /* Check for ack */
    TLAN_EEPROM_CLR(pmc, ETXEN);
    TLAN_EEPROM_SET(pmc, ECLOK);
    sio = tl_dio_readB(pmc,NET_SIO) & EDATA;
    TLAN_EEPROM_CLR(pmc, ECLOK);

    if ( sio != 0 ) goto bad_eeprom;

    /* Send addr in EEPROM */
    TLAN_EEPROM_SET(pmc, ETXEN);
    for ( sio = 0x80; sio; sio >>= 1) {
        if (addr & sio)
                {TLAN_EEPROM_SET(pmc, EDATA);}
        else
                {TLAN_EEPROM_CLR(pmc, EDATA);}

        TLAN_EEPROM_CLCK(pmc);
    }

    /* Check for another ack */
    TLAN_EEPROM_CLR(pmc, ETXEN);
    TLAN_EEPROM_SET(pmc, ECLOK);
    sio = tl_dio_readB(pmc,NET_SIO) & EDATA;
    TLAN_EEPROM_CLR(pmc, ECLOK);

    if ( sio != 0 ) goto bad_eeprom;

    /* send eeprom start sequence */
    TLAN_EEPROM_START_SEQ(pmc);
    
    /* Send type (1010), addr (000) and read (1) */
    bits = 0xa1;
   
    /* write device id */
    for (sio = 0x80; sio; sio >>= 1) {
        if (bits & sio)
                {TLAN_EEPROM_SET(pmc, EDATA);}
        else
                {TLAN_EEPROM_CLR(pmc, EDATA);}

        TLAN_EEPROM_CLCK(pmc);
    }

    /* Check for ack */
    TLAN_EEPROM_CLR(pmc, ETXEN);
    TLAN_EEPROM_SET(pmc, ECLOK);
    sio = tl_dio_readB(pmc,NET_SIO) & EDATA;
    TLAN_EEPROM_CLR(pmc, ECLOK);

    if ( sio != 0 ) goto bad_eeprom;

    for (*pval=0, sio = 0x80; sio; sio>>=1)
    {
	TLAN_EEPROM_SET(pmc, ECLOK);
        if ( tl_dio_readB(pmc,NET_SIO) & EDATA )
            *pval |= sio;
	TLAN_EEPROM_CLR(pmc, ECLOK);
    }
    TLAN_EEPROM_CLCK(pmc);

    TLAN_EEPROM_STOP_SEQ(pmc)

    return (0);

bad_eeprom:
    return (-1);
}


/*----------------------------------------------------------------------------
 * tl_lan_config() --
 *
 * This function is used to program TLAN network configuration registers
 * Input arguments:
 *     pmc      - TLAN control block
 * Return:  -1 failed	0: succeeded
 *
 * Remark:
 *--------------------------------------------------------------------------*/
int
tl_lan_config(pmc_dev_t *pmc)
{
    int phy;
    dio_reg reg;
    u_short data;


    /* Start with PHY configuration
     * First synchronize the MII.... 
     */
    tl_mii_sync(pmc);

    /* now looking for external PHYs */
    for (phy=0;phy < 32; phy++) {
        tl_mii_write(pmc,phy,GEN_CTL, PDOWN|ISOLATE|LOOPBK);
    }
    for (phy=0;phy < 32; phy++) {
	tl_mii_write(pmc,phy,GEN_CTL, PHYRESET|PDOWN|ISOLATE|LOOPBK);
    }
    DELAY(1*1000*1000);
    for (phy=0;phy < 32; phy++) {
        tl_mii_read(pmc,phy,GEN_ID_HI,&reg.word);
        if ( reg.word  != PHY83840_IDHI )
	    continue;
        tl_mii_read(pmc,phy,GEN_ID_LO,&reg.word);
        if ( (reg.word & 0xfc00)  != PHY83840_IDLO )
	    continue;
        pmc->phy = phy;
/*      printf("PHY(DP83840) %d Found !!\n",phy); */
        break;
    }

    if ( phy == 32 )
    {
        cmn_err_tag(217,CE_ALERT, "vfe board %d: PHY not found, Reset not done...\n", pmc->feinfo->uv_unit);  /* pmc is not yet attached so won't have the vfe_num assigned */
        return -1;
    }

    data = 0xe1; /* 100 TX Half */
    tl_mii_write(pmc, pmc->phy, 0x4, data);

    tl_mii_write(pmc,pmc->phy,GEN_CTL, AUTORSRT|AUTOENB);

    /* Configuration -
     * 		Rx frame includes CRC
     * 		Pass up Error Frames
     *		Allow no fragmentation on Rx frames
     *		Allow single channel for Tx frames
     * 		Disable TLAN internal PHY device
     *		Specify CSMA/CD (802.3 - 10/100Mbps) MAC protocol 
     */
    reg.word = RXCRC | PEF | ONEFRAG | ONECHN | (MAC_SELECT_MASK & MACP_CSMACD);
    tl_dio_write(pmc,NET_CONFIG,reg.word,DIO_WORD);

    /* Both TLAN Tx and Rx DMA data transfer burst sizes are 
     * set here; Hi nibble for Tx, low nibble for Rx:
     * 	 0:  16 bytes
     * 	 1:  32 bytes
     * 	 2:  64 bytes (default set at RESET)
     * 	 3: 128 bytes
     * 	 4: 256 bytes
     * 5-7: 512 bytes
     *   8: reserved
     */
    reg.byte = 0x44;	/* used to be 0x03 */
    tl_dio_write(pmc,BSIZE,reg.byte,DIO_BYTE);
/*  take this out  - causing ierrs in some cases 
    reg.byte = 0x50;	
    tl_dio_write(pmc,ACOMMIT,reg.byte,DIO_BYTE);
*/

    /* Maximum Rx frame size is set here */

    reg.word = BUFFSIZE - 16;
    tl_dio_write(pmc,MAXRX,reg.word,DIO_WORD);

    /* Program Network command register */
    reg.byte = NRESET | NWRAP | TXPACE |CSF;
    tl_dio_write(pmc,NET_CMD,reg.byte,DIO_BYTE);

    /* set Local EtherNet MAC address */
    TLAN_SET_MACADDR(pmc,0,pmc->macaddr);
    TLAN_SET_MACADDR(pmc,1,pmc->macaddr);
    TLAN_SET_MACADDR(pmc,2,pmc->macaddr);
    TLAN_SET_MACADDR(pmc,3,pmc->macaddr);

    /* The lan chip is all configured, now stop
     * all transmit and receive channel and mark
     * the LAN port to be DOWN 
     */
    pmc->tlan_reg->host_cmd = D_SWAP(CMD_STP | CMD_RTX); /* stop RX first */
    pmc->tlan_reg->host_cmd = D_SWAP(CMD_STP);		 /* stop TX */
    DELAY(1*1000*1000);

    /* bug 687478 */
    /* vfe_ldtmr is systune'able, within the range of 0x50 to 0x200, 
       default to 0x90 */
    /* Setting this number too small, or too large, will both affect
       performance.  When it's set too small, during heavy traffic when
       packets were pumped in, and out (since in our case uv dma chip will
       generate interrupts for both tx and rcv sides), interrupts will keep
       happening, leaving no time to digest/process the packets.  In some
       cases, systems resulted in crashing, with dma errors.
       On the other hand, when this timer is set too large, interrupt being
       hold for too long, there's the potential for packets being lost.

       The default is set to 0x90 (144), which seems to be agreeable to most
       sites.  When crashing happened as described above, setting this number
       to 0x100 (256) has helped. 

	The description of this timer:
	"Load Interrupt Timer: Writing a one to this bit causes the interrupt 
	holdoff timer to be loaded from the Ack Count field. Ack Count 
	should indicate the time-out period in 4us units (based on a 33Mhz 
	PCI clock).  The interrupt holdoff timer is used to pace interrupts 
	to the host.  Host interrupts are disabled for the time-out period 
	of the timer after an Act bit write."

     */


    /* Load interrupt timer to ~6 PCI cycles */
    pmc->tlan_reg->host_cmd = D_SWAP(CMD_LDTMR | vfe_ldtmr);
    /* pmc->tlan_reg->host_cmd = D_SWAP(CMD_LDTMR | 0x90); */

    /* Write back STS register to reset STOP bits */
    reg.byte = TXSTOP | RXSTOP; 
    tl_dio_write(pmc,NET_STS,reg.byte,DIO_BYTE);

    /* Program Network status mask register
     * to enable all network status interrupts
     */
    reg.byte = MIIMASK /*| HEARTBEATMASK */ | TRANSMITSTOPMASK | RECEIVESTOPMASK;
    tl_dio_write(pmc,NET_MASK,reg.byte,DIO_BYTE);

    pmc->pmc_opstat = PMC_UP;
    return 0;
}


/*----------------------------------------------------------------------------
 * tl_start_transmit() --
 *
 * This function is used to enlist a transmit list and start the Transmit channel
 * Input arguments:
 *     pmc      - TLAN control block
 * Return:  -1 failed.   succeeded otherwise
 *
 * Note  : Currently, the driver is able to perform single channel transmission
 * 	   only.Tx channel select is not allowed. 
 *  	   Transmit list always starts at tl_txbdlist[TX_CURR] pointer.
 *--------------------------------------------------------------------------*/
int
tl_start_transmit(pmc_dev_t *pmc)
{
    u_char tl_replenish_txlist();

   if ( pmc->pmc_opstat & PMC_TXBSY )
	return -1;

    pmc->tlan_reg->chan_parm = D_SWAP(PCIADDR(pmc->feinfo,pmc->tl_txbdlist[TX_CURR]));
    pmc->tlan_reg->host_cmd  = D_SWAP(CMD_GO|CMD_EOC);
    pmc->pmc_opstat |= PMC_TXBSY;
    return 0;
}



/*----------------------------------------------------------------------------
 * tl_receive() -
 *
 * This function is called in the interrupt routine when TLAN HOST_INT(Rx_Eof)
 * is received. 
 * Argument:
 *     pmc      - TLAN control block
 * Return:
 *     0 - succeed, -1 - ???
 * Algorithm:
 *     pmc control block contains both receive and transmit list window point-
 *     ers. The BD to receive next incoming frame is always pointed by RX_CURR 
 *     list pointer. The received frames to be DMA over to the OS starts at
 *     RX_LAST pointer. The receive list is not a ring list, Eventually, we
 *     will receive HOST_INT(Rx_Eoc) interrupt indicating the receive list is
 *     all used up. For now, we should just update the window pointers and
 *     try to initiate the DMA engin if it's idle. 
 *     At this time, the received frame is *not* really in the kernel space
 *     yet, it takes one more DMA step to move the packet over. So we shall
 *     check the integrity of the frame, if bad frame we shall discard the
 *     frame and not bothered with the DMA process.  Thus we shall not report
 *     such bad frames received...
 *--------------------------------------------------------------------------*/
int
tl_receive(pmc_dev_t *pmc)
{
    volatile tlan_rxbd_t *bdp;    /* pointer to BD list */
    u_short fz;
    int ackcnt;
    void tl_receive_disable();

    bdp = pmc->tl_rxbdlist[RX_CURR];

    for (ackcnt=0;;) {
	if (!(bdp->bd_cstat & FRAME_COMPLETE)) 
	    break;

	bdp->bd_cstat = RXCSTAT_REQ;

        fz = bdp->bd_frame_sz; SWAP_2(fz);
        ASSERT (fz > 0);
	ackcnt ++;
	INC_BD(pmc->tl_rxbdlist[RX_BASE],bdp, pmc->tl_rxbdlist[RX_LIMT]);
        if (bdp == pmc->tl_rxbdlist[RX_LAST]) {
	    /* what do we do if the receiver out runs DMA ?? */
	    DEC_BD(pmc->tl_rxbdlist[RX_BASE],bdp, pmc->tl_rxbdlist[RX_LIMT]);
	    break;
	}
    }
    pmc->tl_rxbdlist[RX_CURR] = bdp ;
    return ackcnt;
}




/*----------------------------------------------------------------------------
 * tl_receive_enable() --
 *
 * This function is used to enlist a receive list and start the receive channel
 * Input arguments:
 *     pmc      - TLAN control block
 * Return:      -  n/a
 *
 * Note  : 
 *--------------------------------------------------------------------------*/
void
tl_receive_enable(pmc_dev_t *pmc)
{

    pmc->tlan_reg->chan_parm = D_SWAP(PCIADDR(pmc->feinfo,pmc->tl_rxbdlist[RX_CURR]));
    pmc->tlan_reg->host_cmd  = D_SWAP(CMD_GO|CMD_RTX);
    pmc->pmc_opstat |= PMC_RXRDY;
}


/*----------------------------------------------------------------------------
 * tl_receive_disable() --
 *
 * This function is used to stop the PMC receive channel
 * Input arguments:
 *     pmc      - TLAN control block
 * Return:      -  n/a
 *
 * Note  : 
 *--------------------------------------------------------------------------*/
void
tl_receive_disable(pmc_dev_t *pmc)
{

    /* make sure there is no pending receive packet .. */
    pmc->tlan_reg->host_cmd = D_SWAP(CMD_STP|CMD_RTX);
    pmc->pmc_opstat &= ~PMC_RXRDY;
}

/*----------------------------------------------------------------------------
 * tl_get_netstat() --
 *
 * This function is used to retrive Network statistics in the TLAN chip
 * Input arguments:
 *     pmc      - TLAN control block
 * Return:	n/a
 *
 * Remark:
 *--------------------------------------------------------------------------*/
void
tl_get_netstat(pmc_dev_t *pmc)
{
    volatile int stat;
    u_int reg_buf[5];
    u_int rx_overrun_frm, crc_frm, codeerr_frm;

    bzero(reg_buf,20);
    reg_buf[0] = tl_dio_readL(pmc, NET_SGOODTX);
    reg_buf[1] = tl_dio_readL(pmc, NET_SGOODRX);
    reg_buf[2] = tl_dio_readL(pmc, NET_SDEFERTX);
    reg_buf[3] = tl_dio_readL(pmc, NET_SMULTICOLLTX);
    reg_buf[4] = tl_dio_readL(pmc, NET_SEXCESSCOLL);

    pmc->net_stat.tx_good_frm += reg_buf[0] & 0x00ffffff;
    pmc->net_stat.tx_underrun_frm += reg_buf[0] >> 24;
    pmc->net_stat.rx_good_frm += reg_buf[1] & 0x00ffffff;
    pmc->net_stat.rx_overrun_frm += (rx_overrun_frm = reg_buf[1] >> 24);
    pmc->net_stat.tx_deferred_frm += reg_buf[2] & 0x0000ffff;
    pmc->net_stat.crc_frm += (crc_frm = (reg_buf[2] >> 16) & 0x00ff);
    pmc->net_stat.codeerr_frm += (codeerr_frm = (reg_buf[2] >> 24) & 0x00ff);
    pmc->net_stat.tx_multicollision_frm += reg_buf[3] & 0x0000ffff;
    pmc->net_stat.singlecollision_frm += (reg_buf[3] >> 16) & 0x00ffff;
    pmc->net_stat.excesscollision_frm += reg_buf[4] & 0x00ff;
    pmc->net_stat.latecollision_frm += (reg_buf[4] >> 8) & 0x00ff;
    pmc->net_stat.carrierlost_frm += (reg_buf[4] >> 16) & 0x00ff;

    pmc->pmc_if.if_collisions = pmc->net_stat.tx_multicollision_frm +
                                pmc->net_stat.singlecollision_frm +
                                pmc->net_stat.excesscollision_frm +
                                pmc->net_stat.latecollision_frm;

    pmc->pmc_if.if_ierrors += rx_overrun_frm;

    rx_overrun_all += rx_overrun_frm;
    crc_frm_all += crc_frm;
    codeerr_all += codeerr_frm;

    pmc->pmc_if.if_oerrors = pmc->net_stat.tx_underrun_frm;
/*
                             pmc->net_stat.tx_deferred_frm;
*/


/* watchdog timer is on so status will get updated periodically */
    if ( eiftoifp(&pmc->pmcif)->if_flags & IFF_DEBUG ) {
    printf("tx_good_frm = %d, tx_underrun_frm = %d , tx_deferred_frm = %d\n",
	   pmc->net_stat.tx_good_frm,pmc->net_stat.tx_underrun_frm,
	   pmc->net_stat.tx_deferred_frm);
    printf("rx_good_frm = %d, rx_overrun_frm = %d , crc_frm = %d\n",
	   pmc->net_stat.rx_good_frm,pmc->net_stat.rx_overrun_frm,
	   pmc->net_stat.crc_frm);
    printf("codeerr_frm = %d, tx_multicollision_frm = %d , singlecollision_frm = %d\n",
	   pmc->net_stat.codeerr_frm,pmc->net_stat.tx_multicollision_frm,
	   pmc->net_stat.singlecollision_frm);
    printf("excesscollision_frm = %d, latecollision_frm = %d , carrierlost_frm = %d\n",
	   pmc->net_stat.excesscollision_frm,pmc->net_stat.latecollision_frm,
	   pmc->net_stat.carrierlost_frm);
    }
}

/*----------------------------------------------------------------------------
 * tl_pmc_init() --
 *
 * This function is used to initialize and configure LAN port
 * Input arguments:
 *     pmc      - TLAN control block
 * Return:
 *     
 * Remark:  This should be good place to access EEPROM on the PMC for
 *          MAC address..
 *--------------------------------------------------------------------------*/
void
tl_pmc_init(pmc_dev_t *pmc)
{
    pmc->tlan_reg->host_cmd = D_SWAP(CMD_ADPRST); 

/* follow the steps as suggested by the blue manual: rest, and turn off
   interrupt.  Though interrupt has already been turned off in if_vfeintr()
   routine, we'd rather play it safe.  */
    DELAY(1*1000*1000);
    pmc->tlan_reg->host_cmd = D_SWAP(CMD_INTOFF);

    DELAY(1*1000*1000);
    pmc->pmc_opstat = PMC_RESET;

}


/*----------------------------------------------------------------------------
 * tl_autoconfig() --
 *
 * This function is used to Autoconfig the PMC (PCI) device
 * Input arguments:
 *     pmc      - TLAN control block
 *     conf     - PCI Autoconfig space pointer
 * Return:
 *     
 * Remark:
 *--------------------------------------------------------------------------*/
int
tl_autoconfig(pmc_dev_t *pmc,volatile tlan_conf_t *conf, uint regbase, uint pno)
{
    fenet_dev_t * fenetp;


    fenetp = pmc->feinfo;

    pmc->auto_config.vend_id = conf->vend_id; SWAP_2(pmc->auto_config.vend_id);
    pmc->auto_config.dev_id = conf->dev_id; SWAP_2(pmc->auto_config.dev_id);

    /* check id */
    if ( (pmc->auto_config.vend_id != TLAN_VEND_ID) ||
	 (pmc->auto_config.dev_id != TLAN_DEV_ID) )
    {
	printf("vfe board %d:PMC Autoconfig failed - Unknown Vendor Hardware:%4x-%4x!!",
		pmc->feinfo->uv_unit, pmc->auto_config.vend_id,pmc->auto_config.dev_id);
	printf("  Please re-check each board.\n");
	return -1;
    }
    pmc->auto_config.status = conf->status; 
    pmc->auto_config.rev = conf->rev; 
    pmc->auto_config.Prog_if = conf->Prog_if; 
    pmc->auto_config.subclass = conf->subclass; 
    pmc->auto_config.baseclass = conf->baseclass; 
    pmc->auto_config.chahesize = conf->chahesize; 
    pmc->auto_config.latencytmr = conf->latencytmr; 

/*  set it as suggested by the blue manual */
    conf->latencytmr = 0xff;
    pmc->auto_config.latencytmr = conf->latencytmr;
    


    pmc->auto_config.nvram = conf->nvram; 

    pmc->auto_config.int_line = conf->int_line; 
    pmc->auto_config.int_pin = conf->int_pin; 
    pmc->auto_config.min_gnt = conf->min_gnt; 
    pmc->auto_config.max_lat = conf->max_lat; 

    /* probe baseaddr reg to get info on size and type of memory
     * required by PMC card
     */
    conf->baseaddr_io = 0xffffffff;
    pmc->auto_config.baseaddr_io = D_SWAP(conf->baseaddr_io); 
    conf->baseaddr_mem = 0xffffffff;
    pmc->auto_config.baseaddr_mem = D_SWAP(conf->baseaddr_mem); 
    pmc->auto_config.bios_romaddr = D_SWAP(conf->bios_romaddr); 

    /* write back status byte to clear bits */
    conf->status = pmc->auto_config.status; SWAP_2(pmc->auto_config.status);

    if ( pmc->auto_config.status & 
	(PARITYERR|SIGSYSERR|RCVMSTABT|RCVTRGABT|SIGTRGABT|MHZ66ABLE|DPARITY) )
    {
	printf("vfe board %u port %u:PMC Reset failed - Autoconfig Status = :%4x!!\n",
		fenetp->uv_unit,pno, pmc->auto_config.status);
	return -1;
    }
    /* ??? What do we do with the revision Register ??? */

    if ( (pmc->auto_config.baseclass != 0x02) || 
/*	 (pmc->auto_config.subclass != 0x80) ) */
	 (pmc->auto_config.subclass != 0x0) )
    {
	/* baseclass 0x02 = Network Controller,
	 * Subclass  0x80 = Other Network controller than FDDI, Ether, TokenR 
	 */
	printf("vfe board %u port %u:PMC PCI Class/Subclass invalid - %2x/%2x!!\n",
		fenetp->uv_unit,pno,pmc->auto_config.baseclass,
		pmc->auto_config.subclass);
	return -1;
    }

    /* ??? What do we do with the Cache line size Register ??? */
    /* ??? What do we do with the Latency Timer Register ??? */

    /* Set Base address of TLAN register space to PCI I/O space
     * don't touch baseaddr_mem reg since the PMC does not have
     * any on chip memory thus the return value of 0xfffffff0;
     */
    ASSERT(!(pmc->auto_config.baseaddr_mem & 0x01));
    pmc->auto_config.baseaddr_mem = regbase & 0xfffffff0;
    EN_SWAP(pmc->auto_config.baseaddr_mem);
    conf->baseaddr_mem = pmc->auto_config.baseaddr_mem;

    /* Finally, enable the PCI device */
    pmc->auto_config.cmd = MEMENABLE|MSTENABLE;
    SWAP_2(pmc->auto_config.cmd);
    conf->cmd = pmc->auto_config.cmd;

    return 0;
}


/*----------------------------------------------------------------------------
 * tl_create_rxlist() --
 *
 * This function is used to create TLAN Receive BD list.
 * Input arguments:
 *     pmc      - TLAN control block
 *     headp    - Starting address of the BD list.
 *     size     - Size of the list (number of BD per list).
 * Return:
 *     pointer to the tail of the list.
 * Remark:
 *     It is important to know the absolute address boundry in the PCIbus
 *     memory space of the BD list to be created because the forward
 *     pointers in the BD block must be reference from the PCI bus. 
 *     Do not be confused by the forward pointers with the list pointers 
 *     in the VME slave image. All pointers here are
 *     referenced in VME space except the forward pointers in the list.
 *--------------------------------------------------------------------------*/
u_char *
tl_create_rxlist(pmc_dev_t *pmc,tlan_rxbd_t *headp,int size)
{
    volatile tlan_rxbd_t *tailp;
    fenet_dev_t * fenetp;
    int  i;

    if((__psunsigned_t)headp & (__psunsigned_t)0x07)
        ASSERT(0);    /* BD block must be long aligned */

    fenetp = pmc->feinfo;

    for (i = 0, tailp = headp; i < size; i ++)
    {
	tailp->bd_cstat = RXCSTAT_REQ;
/*	SWAP_2(tailp->bd_cstat); */
        tailp->bd_frame_sz = 0;		/* filled by TLAN */
        tailp->bd_data_cnt = D_SWAP(F_LASTFRAG);
        /* Don't attach data buffers just yet, will do that later.
         * Note, the forward pointer bd_next must be specified as in PCIbus
         * space
         */
        tailp->bd_next = D_SWAP(PCIADDR(fenetp,(tailp+1)));
	tailp++;
    }
    /* we'll terminate the list at the last BD */
    (tailp-1)->bd_next = 0;
    pmc->tl_rxbdlist[RX_BASE] = 
    pmc->tl_rxbdlist[RX_CURR] =
    pmc->tl_rxbdlist[RX_LAST] = headp;
    pmc->tl_rxbdlist[RX_NEXT] = 
    pmc->tl_rxbdlist[RX_LIMT] = tailp-1;

    return ((u_char *)tailp);
}


/*----------------------------------------------------------------------------
 * tl_create_txlist() --
 *
 * This function is used to create TLAN Transmit BD list.
 * Input arguments:
 *     pmc      - TLAN control block
 *     headp    - Starting address of the BD list.
 *     size     - Size of the list (number of BD per list).
 * Return:
 *     pointer to the tail of the list.
 * Remark:
 *     It is important to know the absolute address boundry in the PCIbus
 *     memory space of the BD list to be created because the forward
 *     pointers in the BD block must be reference from the PCI bus. 
 *     Do not be confused by the forward pointers with the list pointers 
 *     in the VME slave image. All pointers here are
 *     referenced in VME space except the forward pointers in the list.
 *--------------------------------------------------------------------------*/
u_char *
tl_create_txlist(pmc_dev_t *pmc,tlan_txbd_t *headp,int size)
{
    volatile tlan_txbd_t *tailp;
    fenet_dev_t * fenetp;
    int  i, j;

    if ( (__psunsigned_t)headp & (__psunsigned_t)0x07 )
        ASSERT(0);    /* BD block must be long aligned */

    fenetp = pmc->feinfo;

    for (i = 0, tailp = headp; i < size; i ++)
    {
        tailp->bd_cstat = TXCSTAT_REQ;
/*      SWAP_2(tailp->bd_cstat); */
        tailp->bd_frame_sz = 0;		/* filled by TLAN */
        tailp->bd_next = D_SWAP(PCIADDR(fenetp,(tailp+1)));
	for (j=0; j < 8; j++)
	{
	    tailp->bdbuf[j].bd_data_cnt = 0;
	}
        tailp->bdbuf[j].bd_data_cnt = D_SWAP(F_LASTFRAG);
	tailp++;
    }
    /* we'll terminate the list at the last BD */
    (tailp - 1)->bd_next = D_SWAP(PCIADDR(fenetp,headp));
    pmc->tl_txbdlist[TX_BASE] = 
    pmc->tl_txbdlist[TX_CURR] =
    pmc->tl_txbdlist[TX_LAST] =
    pmc->tl_txbdlist[TX_NEXT] = headp;
    pmc->tl_txbdlist[TX_LIMT] = (tlan_txbd_t *)tailp-1;

    return ((u_char *)tailp);
}


/*----------------------------------------------------------------------------
 * tl_attach_rxlist() --
 *
 * This function is used to attach data buffers to the receive list
 * Input arguments:
 *     pmc      - TLAN control block
 *     frmsz    - Max. frame size.
 *     cnt      - Size of the list (number of BD per list).
 * Return:
 *     pointer to the tail of the Data buffer pool.
 * Remark:
 *     It is important to know the absolute address boundry in the PCIbus
 *     memory space of the Data buffer area because the buffer
 *     pointers in the BD block must be reference from the PCI bus.
 *     Data buffer to be attached always begins with pmc->rx_bufp
 *--------------------------------------------------------------------------*/
pciaddr_t
tl_attach_rxlist(pmc_dev_t *pmc,int frmsz,int cnt)
{
    volatile tlan_rxbd_t *tailp;
    pciaddr_t bufp;
    int  i, f;

    tailp = pmc->tl_rxbdlist[RX_CURR];

    bufp = pmc->rx_bufp;
    f = D_SWAP(frmsz);
    for (i = 0; i < cnt; i ++)
    {
        tailp->bd_bufp = D_SWAP((__uint32_t)bufp+2); /* keep aligned with mbuf */
        tailp->bd_data_cnt |= f;
  	tailp ++;
        bufp += frmsz;
    }
    return (bufp);
}


/*----------------------------------------------------------------------------
 * tl_attach_txlist() --
 *
 * This function is used to attach data buffers to the transmit list
 * Input arguments:
 *     pmc      - TLAN control block
 *     frmsz    - Max. frame size.
 *     cnt      - Size of the list (number of BD per list).
 * Return:
 *     pointer to the tail of the Data buffer pool.
 * Remark:
 *     It is important to know the absolute address boundry in the PCIbus
 *     memory space of the Data buffer area because the buffer
 *     pointers in the BD block must be reference from the PCI bus.
 *--------------------------------------------------------------------------*/
pciaddr_t
tl_attach_txlist(pmc_dev_t *pmc,int frmsz,int cnt)
{
    volatile tlan_txbd_t *tailp;
    pciaddr_t bufp;
    int  i, j;

    tailp = pmc->tl_txbdlist[TX_CURR];

    bufp = pmc->tx_bufp;
    for (i = 0; i < cnt; i ++)
    {
	tailp->bdbuf[0].bd_bufp = D_SWAP(bufp);
	for (j = 1; j < 9; j++)
	{
            tailp->bdbuf[j].bd_bufp = 0;	
            tailp->bdbuf[j].bd_data_cnt = 0;	
	}
  	tailp ++;
	bufp += frmsz;
	ASSERT (!((__uint32_t)bufp & 0x07));
    }
    return (bufp);
}

/*----------------------------------------------------------------------------
 * tl_rebuild_struct -
 *
 *--------------------------------------------------------------------------*/
void
tl_rebuild_struct(pmc_dev_t *pmc)
{
    struct mbuf *mb;

    /* Go fix up the BD lists for the PMC ??? do we need to do that ??? */
    tl_create_rxlist(pmc,(tlan_rxbd_t *)pmc->tl_rxbdlist[RX_BASE],RBD_LIST_NO);
    tl_create_txlist(pmc,(tlan_txbd_t *)pmc->tl_txbdlist[TX_BASE],TBD_LIST_NO);
    tl_attach_rxlist(pmc,BUFFSIZE,RBD_LIST_NO);
    tl_attach_txlist(pmc,BUFFSIZE,TBD_LIST_NO);

    /* flush all queued transmit mbufs */
    while (pmc->txq_geti != pmc->txq_puti)
    {
        mb = pmc->txq_list[pmc->txq_geti];
        pmc->txq_geti = (pmc->txq_geti + 1) % (TX_QUEUE_NO - 1);
	if ( mb )
	    m_freem(mb);
    }
    pmc->txq_geti = pmc->txq_puti = 0;
    pmc->pmc_opstat = PMC_DOWN;
    if ( eiftoifp(&pmc->pmcif)->if_flags & IFF_DEBUG )  
    	printf("vfe%d: tl_rebuild_struct: set PMC_DOWN\n", pmc->vfe_num);
}


/*----------------------------------------------------------------------------
 * tl_get_txbd() --
 *
 * This function is used to obtain a transmit BD in the transmit list for
 * a transmit frame. 
 * Input arguments:
 *     pmc      - TLAN control block
 *     frmsz    - transmit frame size.
 * Return:
 *     pointer to the transmit BD just checked out. 0 - Tx list full.
 * Algorithm:
 *     The transmit frame pointed by dbufp is always attached to the BD
 *     pointed by tl_txbdlist[TX_NEXT], the pointer should wrap if it hits 
 *     the list limit. The list becomes full when TX_NEXT pointer
 *     catches up with TX_CURR pointer. Usually this routine is called
 *     when preparing the DMA command list for transmit frame data transfer
 *--------------------------------------------------------------------------*/
volatile tlan_txbd_t  *
tl_get_txbd(pmc_dev_t *pmc,int frmsz)
{
    volatile tlan_txbd_t  *bdp, *newbdp;
    short  len = frmsz;
    /*uint   dcnt;*/

    newbdp = 
    bdp    = pmc->tl_txbdlist[TX_NEXT];

    INC_BD(pmc->tl_txbdlist[TX_BASE],newbdp,pmc->tl_txbdlist[TX_LIMT]);

    if ( newbdp == pmc->tl_txbdlist[TX_CURR] )
	return ((tlan_txbd_t  *)0);	/* the list is full */
/*
    len = bdp->bd_cstat ;
    SWAP_2(len);
    ASSERT(len == TXCSTAT_REQ);	 sanity check should be removed later 
*/
    bdp->bd_cstat = TXCSTAT_REQ;
    SWAP_2(len);
    bdp->bd_frame_sz = len;
    pmc->tl_txbdlist[TX_NEXT] = newbdp;
   
    return (bdp);
}

/*----------------------------------------------------------------------------
 * tl_end_txlist() --
 *
 * This function is used to complete a transmit list for TLAN transmission
 * Input arguments:
 *     pmc      - TLAN control block
 * Return:
 *     0 - succeed, -1 - Transmit list is empty
 * Algorithm:
 *     The transmit list always begins with TX_LAST pointer and ends
 *     with TX_NEXT pointer. TX_CURR is assigned to TX_LAST to hand off the
 *     list for transmission. TX_LAST is then updated with TX_NEXT -1.
 *     Store the link(forward) pointer of TX_LAST in TX_PREV and break the
 *     link at TX_LAST.
 *--------------------------------------------------------------------------*/
int
tl_end_txlist(pmc_dev_t *pmc)
{

    volatile tlan_txbd_t  *bdp;

    if ( pmc->tl_txbdlist[TX_NEXT] == pmc->tl_txbdlist[TX_LAST] )
    /* There is nothing in the list to transmit */
   	return (-1);

    bdp = pmc->tl_txbdlist[TX_NEXT] - 1;

    if ( bdp < pmc->tl_txbdlist[TX_BASE] )
	bdp = pmc->tl_txbdlist[TX_LIMT];	/* hit the top, wrap around */

    pmc->tl_txbdlist[TX_CURR] = pmc->tl_txbdlist[TX_LAST];
    pmc->tl_txbdlist[TX_LAST] = pmc->tl_txbdlist[TX_NEXT];
    /* terminate the list */
    bdp->bd_next = 0;
   
    return (0);
}


/*----------------------------------------------------------------------------
 * tl_replenish_txlist() --
 *
 * This function is used to replenish a transmit list when transmission of the
 * list is complete. 
 * Input arguments:
 *     pmc      - TLAN control block
 * Return:
 *     0 - succeed, -1 - ???
 * Algorithm:
 *     The to be replenished transmit list always starts with TX_CURR pointer
 *     The list is terminated earlier when the list is prepared for transmit.
 *     work throught the list and stop at the end of list. Buffers are freed
 *     by manipulating the buffer management pointer set.
 *--------------------------------------------------------------------------*/
u_char
tl_replenish_txlist(pmc_dev_t *pmc)
{

    volatile tlan_txbd_t  *bdp;
    u_char ackcnt = 0;
    volatile pciaddr_t bufp;

    bdp = pmc->tl_txbdlist[TX_CURR];
    while (bdp->bd_next)
    {
/*
	bufp = bdp->bdbuf[0].bd_bufp;
	bufp &= 0xf8ffffff;
	bdp->bdbuf[0].bd_bufp = bufp;
	cstat = bdp->bd_cstat;
	SWAP_2(cstat);
	ASSERT(cstat & FRAME_COMPLETE); 
	bdp->bd_cstat = TXCSTAT_REQ;
	SWAP_2(bdp->bd_cstat); 
	bdp->bd_frame_sz = 0;
	bdp->bd_data_cnt = D_SWAP(F_LASTFRAG);
*/
	INC_BD(pmc->tl_txbdlist[TX_BASE],bdp,pmc->tl_txbdlist[TX_LIMT]);
	ackcnt ++;
    }
    /* we should reconnect the list back to main transmit list 
       which we broke off at TX_LAST pointer */
    bdp->bd_next = D_SWAP(PCIADDR(pmc->feinfo,pmc->tl_txbdlist[TX_LAST]));

/*  cstat = bdp->bd_cstat;
    SWAP_2(cstat);
    ASSERT( cstat & FRAME_COMPLETE ); 
    bufp = bdp->bdbuf[0].bd_bufp;
    bufp &= 0xf8ffffff;
    bdp->bdbuf[0].bd_bufp = bufp;
    cstat = TXCSTAT_REQ;
    SWAP_2(cstat);
    bdp->bd_cstat = TXCSTAT_REQ;
    bdp->bd_frame_sz = 0;
    bdp->bd_data_cnt = D_SWAP(F_LASTFRAG);
*/
    INC_BD(pmc->tl_txbdlist[TX_BASE],bdp,pmc->tl_txbdlist[TX_LIMT]);
/*  ASSERT( bdp == pmc->tl_txbdlist[TX_LAST] ); */
    if( bdp != pmc->tl_txbdlist[TX_LAST] ) {
	cmn_err_tag(218,CE_ALERT, "vfe%d: About to die.. bdp = %x, LAST = %x\n",pmc->vfe_num, bdp,pmc->tl_txbdlist[TX_LAST]);
	ASSERT(0);
    }
    pmc->tl_txbdlist[TX_CURR] = bdp;

    ackcnt ++;
    return (ackcnt);
}



/*----------------------------------------------------------------------------
 * tl_adptchk_error() --
 *
 * This function is used to process TLAN Adapter Check Interrupt from TLAN
 * Input arguments:
 *     pmc      - TLAN control block
 *     stat     - Interrupt status
 *     pno      - PMC number (0 or 1) of the PCI controller
 * Return:
 *     0 - succeed, -1 - ???
 * Note:
 *     Refer to TLAN PG2.0 Rev 0.41 Page 35 for detail information.
 *     ??? What should we do when getting this interrupt???
 *--------------------------------------------------------------------------*/
void
tl_adptchk_error( pmc_dev_t *pmc,ushort stat, int pno)
{
    volatile __uint32_t ureg;
    u_char   errcode;
    /*u_char   channo;*/
    int vfe_no;


    vfe_no = pmc->vfe_num;  /* get the correct vfe interface number */

    if ((stat & 0x1fe0) == 0)        /* Is this Network status interrupt ? */
    {
        errcode = tl_dio_readB(pmc,NET_STS);
    	/* if ( eiftoifp(&pmc->pmcif)->if_flags & IFF_DEBUG ) { */
	if ( errcode & MIRQ )        /* MII interrupt */
	{
	    dio_reg reg;
	    tl_mii_read(pmc,pmc->phy,GEN_STS,&reg.word);
	    printf("vfe%u:MII GEN_STS = 0x%x\n",vfe_no,reg.word);
	    tl_mii_read(pmc,pmc->phy,TLPHY_STS,&reg.word);
	    printf("vfe%u:MII TLPHY_STS = 0x%x\n",vfe_no,reg.word);
	    tl_mii_read(pmc,pmc->phy,TLPHY_CTL,&reg.word);
	    tl_mii_sync(pmc);
	    printf("vfe%u:MII TLPHY_CTL = 0x%x\n",vfe_no,reg.word);
	    if ( reg.word & TINT )
		reg.word &= ~TINT;
	    tl_mii_write(pmc,pmc->phy,TLPHY_CTL,reg.word);
	}
	if ( errcode & HBEAT )        /* HBEAT interrupt */
	{
	    /* what else do we do about this ?? */
	    printf("vfe%u:Heart Beat Error\n",vfe_no);
	}
	if ( errcode & TXSTOP )        /* TXSTOP interrupt */
	{
	    /* what else do we do about this ?? */
	    printf("vfe%u:Transmiter Stopped\n",vfe_no);
	}
	if ( errcode & RXSTOP )        /* RXSTOP interrupt */
	{
	    /* what else do we do about this ?? */
	    printf("vfe%u:Receiver stopped\n",vfe_no);
	}
	/* clear NET_STS register by writing the stats byte back */
	tl_dio_write(pmc,NET_STS,errcode,DIO_BYTE);
        return;
	/*}*/
    }
    ureg = D_SWAP(pmc->tlan_reg->chan_parm);
    errcode = (u_char)(ureg & 0xff);
    /*channo = (u_char)((ureg >> 21) & 0xff);*/

/*   if ( eiftoifp(&pmc->pmcif)->if_flags & IFF_DEBUG ) { */
    printf("vfe%u:Adapter Check on PMC[%u]==> Failure Code = %s \n",vfe_no,
            pno,adptchk_errmsg[errcode]);
    printf("      Type: %s/%s, During %s\n",ADPTCHK_TYPE(ureg),
           ADPTCHK_CHOP(ureg), ADPTCHK_RWOP(ureg));
    printf("	Driver recovering..\n");
/*  } */
/*  vfe_reset(&pmc->pmcif); */

}


/*----------------------------------------------------------------------------
 * uv_init() --
 *
 * This procedure is used to initialize the IP-6200 specific resources. All
 * related init. steps should be done here.
 * Input arguments:
 *     fep      - Device info struct. This is a per board info structure.
 * Return:
 *     0 - Fine.  -1 - Fail.
 * Remark:
 *--------------------------------------------------------------------------*/
int
uv_init(fenet_dev_t *fep)
{
	volatile __uint32_t ureg;
	volatile __uint32_t *pcimem;
	u_char    *tail;
	int i;
	tlan_conf_t *tlanbase;
	pciaddr_t pcitmp;

	
	/* Start off by reading the PCI_ID register in UNIVERSE to get id */
	UVREG_RD(fep,PCI_ID,ureg);

	if ( ureg != UNIVERSE_ID)
	{
	    printf("vfe board%u: Invalid UNIVERSE PCI_ID: 0x%x \n",fep->uv_unit, ureg);
	    return -1;
	}

	/* Before anything else, RESET both PCI and VME bus !!
	   Note: this code cause hardware error on VME bus, 
	   But when do it in user mode it's OK. Don't know why ???
	UVREG_RD(fep, MISC_CTL,ureg);
	printf("vfe board%u: MISC_CTL = %x \n",fep->uv_unit, ureg);
	ureg |= SW_LRST;
	UVREG_WR(fep, MISC_CTL,ureg); */

	/* Program UNIVERSE chip */
	/* ureg = CSR_IOS | CSR_MS | CSR_BM | CSR_DEVSEL; */
	UVREG_WR(fep,PCI_CSR,CSR_IOS | CSR_MS | CSR_BM | CSR_DEVSEL);

        /* for some reason, the kernel would crash if we don't do this
         * dummy steps...
         */
        ureg = (__uint32_t)fep->VME_ADDR;
        UVREG_WR(fep,VSI0_BS,ureg);
        /* Set Bound address to be 16K page aligned!! */
        ureg = (__uint32_t)(fep->VME_ADDR + 0x80000);
        UVREG_WR(fep,VSI0_BD,ureg);
        
        ureg = (__uint32_t)fep->SYNC_RAM -
                (__uint32_t)fep->VME_ADDR;
        UVREG_WR(fep,VSI0_TO,ureg);
        /* Control = VSI_EN+VSI_BOTH+VSI_SUPUR+VSI_VAS32+VSI_LAS_CFG */
        ureg = VSI_EN | VSI_BOTH | VSI_SUPUR |VSI_VAS32|VSI_LAS_MEM;
        UVREG_WR(fep,VSI0_CTL,ureg);
        ureg = 0;       /* disable the image */
        UVREG_WR(fep,VSI0_CTL,ureg);

	/* Now create series of VME slave images to map various PCI bus
	 * memory space. Starting with Image #0 for PMC autoconfig space.
	 */
	ureg = (__uint32_t)fep->VME_ADDR;
	UVREG_WR(fep,VSI0_BS,ureg);
	/* Set Bound address to be 16K page aligned!! */
	ureg =(__uint32_t) GET_TLANHOST0(fep);
	UVREG_WR(fep,VSI0_BD,ureg);
	/* no need for TO register for PCI Autoconfig space
	ureg = TLAN0_REG_MEM - 
		(__uint32_t)fep->VME_ADDR;
	UVREG_WR(fep,VSI0_TO,ureg); */
	/* Control = VSI_EN+VSI_BOTH+VSI_SUPUR+VSI_VAS32+VSI_LAS_CFG */
	ureg = VSI_EN | VSI_BOTH | VSI_SUPUR |VSI_VAS32|VSI_LAS_CFG;
	UVREG_WR(fep,VSI0_CTL,ureg);

	/* OK, go autoconfig the PMC device... */
	/*fep->pmc[0].tlan_reg = (host_reg_t *)TLAN0_REG_MEM;*/
	tlanbase = (tlan_conf_t *)(fep->VME_SLAVE +
		(__uint32_t)0x9000);

	if (tl_autoconfig(&fep->pmc[0],tlanbase,(uint)TLAN0_REG_MEM, 0) == 0)
		fep->pmc[0].pmc_opstat = PMC_INIT;
	else {
		fep->pmc[0].pmc_opstat = PMC_NOP;
		return -1;
	}
	tlanbase = (tlan_conf_t *)(fep->VME_SLAVE + 
		(__uint32_t)0x8800);
	if (tl_autoconfig(&fep->pmc[1],tlanbase,(uint)TLAN1_REG_MEM, 1) == 0)
		fep->pmc[1].pmc_opstat = PMC_INIT;
	else {
		fep->pmc[1].pmc_opstat = PMC_NOP;
		return -1;
	}

        ureg = 0;	/* disable the image */
        UVREG_WR(fep,VSI0_CTL,ureg);

 	/* Slave image #0 is for TLAN Host Register of PMC0 */
        ureg = GET_TLANHOST0(fep);
        UVREG_WR(fep,VSI0_BS,ureg);

        /* Bound address MUST be 64K page aligned!! */
        ureg = GET_MAPEND(fep);
        UVREG_WR(fep,VSI0_BD,ureg);

        ureg = (__uint32_t)TLAN0_REG_MEM -
               GET_TLANHOST0(fep);
	if ( GET_TLANHOST0(fep) > TLAN0_REG_MEM )
            ureg |= 0xf0000000;
	/*  map of UV for slave image 0, refering to PCI SYNC RAM */ 
        UVREG_WR(fep,VSI0_TO,ureg);

        ureg = VSI_EN | VSI_BOTH | VSI_SUPUR |VSI_VAS32|VSI_LAS_MEM;
        UVREG_WR(fep,VSI0_CTL,ureg);

	fep->pmc[0].tlan_reg = (host_reg_t *)GET_TLANHOST0_SLAVE(fep);
	fep->pmc[1].tlan_reg = (host_reg_t *)GET_TLANHOST1_SLAVE(fep);
			
	/* Now should be good time to allocate, init. and setup all the
	 * "lists" for TLAN operation. First we need to create a VME slave
 	 * image to map in the PCI local bus Sync. RAM as image #1
	 */
	
        ureg = GET_DMALIST(fep);
        UVREG_WR(fep,VSI1_BS,ureg);

        /* Bound address MUST be 64K page aligned!! */
	ureg = GET_TLANHOST0(fep);
        UVREG_WR(fep,VSI1_BD,ureg);

        ureg = (__uint32_t)fep->SYNC_RAM - 
		GET_DMALIST(fep);
        UVREG_WR(fep,VSI1_TO,ureg);

        /* Control = VSI_EN+VSI_BOTH+VSI_SUPUR+VSI_VAS32+VSI_LAS_MEM */
        ureg = VSI_EN | VSI_BOTH | VSI_SUPUR |VSI_VAS32|VSI_LAS_MEM;
        UVREG_WR(fep,VSI1_CTL,ureg);

/*	printf("uv_init: Initializing PCI Sync RAM....\n"); */
        for ( i = 0, 
	      pcimem = (__uint32_t *)fep->VME_SLAVE; 
	      i < 0x10000/4; i++ ) {
	    *pcimem = 0;
	     pcimem ++;
	} 

	/* Now create UNIVERSE DMA Command list, TLAN Transmit/Receive BD
	 * list pairs for each PMC ports....
	 */

	tail = uv_create_dmalist(fep,fep->uv_dmacmd,
			(uv_dmacmd_t *)GET_DMALIST_SLAVE(fep), DMA_LIST_NO);
        if (fep->pmc[0].pmc_opstat == PMC_INIT) {
	tail = tl_create_rxlist(&fep->pmc[0],(tlan_rxbd_t *)tail,RBD_LIST_NO);
	tail = tl_create_txlist(&fep->pmc[0],(tlan_txbd_t *)tail,TBD_LIST_NO);
	}
	if (fep->pmc[1].pmc_opstat == PMC_INIT) {
	tail = tl_create_rxlist(&fep->pmc[1],(tlan_rxbd_t *)tail,RBD_LIST_NO);
	tail = tl_create_txlist(&fep->pmc[1],(tlan_txbd_t *)tail,TBD_LIST_NO);
	}

	/* All lists are created, attach pre-allocate buffers to those BDs
	 * tail should now points to the beginning of buffer memory area */

	if (fep->pmc[0].pmc_opstat == PMC_INIT) {
	fep->pmc[0].rx_bufp = (pciaddr_t)PCIADDR(fep,tail);
	fep->pmc[0].tx_bufp = tl_attach_rxlist(&fep->pmc[0], BUFFSIZE,RBD_LIST_NO);
	pcitmp                = tl_attach_txlist(&fep->pmc[0], BUFFSIZE,TBD_LIST_NO);
	} else 
	pcitmp                = (pciaddr_t)PCIADDR(fep,tail);
	if (fep->pmc[1].pmc_opstat == PMC_INIT) {
	fep->pmc[1].rx_bufp = pcitmp;
	fep->pmc[1].tx_bufp = tl_attach_rxlist(&fep->pmc[1], BUFFSIZE,RBD_LIST_NO);
	pcitmp              = tl_attach_txlist(&fep->pmc[1], BUFFSIZE,TBD_LIST_NO);
	}

	/* Program UNIVERSE interrupt related mechanisms:
	 * Interrupt source enabled:
	 * 	LINT4, LINT0 - TLAN chip is hardwired to generate interrupts
	 *		       on PCI INTA# line only which is on LINT0 and
 	 *		       LINT4 of PCI bus respeceively.
  	 *      DMA, LERR and VERR for UNIVERSE DMA operation.
	 * All those interrupt sources are mapped to VME bus VIRQ[4].
	 */
	ureg = EN_LINT0 | EN_LINT4 | EN_DMA | EN_LERR | EN_VERR ;
	UVREG_WR(fep,VINT_EN,ureg);
	
	ureg = 0x00040004;	/* VME brl interrupt level = 4 for LINT0/4*/
	UVREG_WR(fep,VINT_MAP0,ureg);
	/* VME brl interrupt level = 4 for DMA, VERR and LERR */
	ureg = 0x00000444;	
	UVREG_WR(fep,VINT_MAP1,ureg);
	ureg = (fep->uv_intvec << 24) & 0xfe000000;  /* see page App A-47 */
	UVREG_WR(fep,VINT_STATID,ureg);

	fep->uv_stat = UV_DMARDY;
	return (0);
}



int
if_vfeintr(int ctlr)
{
	fenet_dev_t *fe = vfe_tbl[ctlr];
	volatile __uint32_t	vint_stat;
	volatile __uint32_t	ureg;
	int dmadone_flag = 0;
        pmc_dev_t * bad_pmc = 0;
	pmc_dev_t *pmc;
	register s;
	u_short type;
        struct ifqueue localq;
        struct mbuf *m, *mf;

#define INT_SPLX(s) splx(s);

    localq.ifq_head = localq.ifq_tail = NULL;

    IFNET_LOCK(&fe->pmc[0].pmc_if); 
    IFNET_LOCK(&fe->pmc[1].pmc_if); 
	while (1) {
	UVREG_RD(fe,VINT_STAT,vint_stat);
	if ( vint_stat == 0 ) 
	    break;
#ifdef JJ_XXX
	if ( vint_stat & (EN_LERR|EN_VERR) )
        {
	    /* What else we need to do here ?? */
            printf("vfe%u: BUS_ERROR!!\n",ctlr);
        }
#endif
	if ( (vint_stat & EN_LINT0) || (vint_stat & EN_LINT4) )
	{
	    pmc_dev_t *pmc;
	    /* Interrupt from PMC0 TLAN 
	     * Go read the interrupt register in TLAN (HOST_INT) for
	     * interrupt vector and interrupt type then acknowledge 
	     * the interrupt
	     */
	    volatile ushort     int_stat;
	    volatile __uint32_t host_cmd;
	    int pmcno;
	    u_char  ackcnt;

	    pmcno = (vint_stat & EN_LINT0) ? 0 : 1;
next_pmc: 
	    pmc = &fe->pmc[pmcno];

	    for (ackcnt = 0; ;ackcnt = 0) {
	    int_stat = pmc->tlan_reg->host_intr;
	    if ( int_stat == 0 )
		break;
	    
	    /* now, go analyz the interrupt types */
                /* TLAN Manual (rev A, Dec 95, pg 39) recommends writing
                 * back the contents of host_intr to disable the interrupt
                 */
            pmc->tlan_reg->host_intr = int_stat;


	    switch ( (int_stat & 0x1c00) )
	    {
		case INT_TXEOF:	/* TX Eof */
		    printf("vfe board%u[%d]:TX Eof Interrupt received\n",ctlr,pmcno);
		    break;
		case INT_STATOV:	/* Stats. overflow */
/*		    printf("vfe board%u[%d]:STATS OVFLW Interrupt received\n",ctlr,pmcno); */
		    stats_overflow++;
		    tl_get_netstat(pmc);
		    break;
		case INT_RXEOF:	/* RX Eof */
/*  		    printf("vfe board%u[%d]:RX Eof Interrupt received\n",ctlr,pmcno); */
		    ackcnt = tl_receive(pmc);
		    break;
		case INT_DUMMY:	/* Dummy int */
		    if (vfe_debug)
		    printf("vfe board%u[%d]:Dummy Interrupt received\n",ctlr,pmcno);
		    break;
		case INT_TXEOC:	/* TX Eoc */
		    ackcnt = tl_replenish_txlist(pmc);
    		    pmc->pmc_if.if_opackets += ackcnt;
		    pmc->pmc_opstat &= ~PMC_TXBSY;
		    break;
		case INT_ADPCHK:	/* Adapter check */
/*		    printf("vfe board%u[%d]:ADPCHK Interrupt received, stat = %x\n",ctlr,pmcno,int_stat); */
		    /* according to the manual, we should stop INT first */
    		    pmc->tlan_reg->host_cmd = D_SWAP(CMD_INTOFF);
		    tl_adptchk_error(pmc,int_stat,pmcno);
		    /*
		    eiftoifp(&pmc->pmcif)->if_flags &= ~IFF_ALIVE;
		    */
		    bad_pmc = pmc;
		    goto no_tlan_ack;
		case INT_RXEOC:	/* RX Eoc */

/* 		    printf("vfe board%u[%d]:RX Eoc Interrupt received\n",ctlr,pmcno);
		    ackcnt = tl_receive(pmc);
*/
    		    pmc->pmc_opstat |= PMC_RXOVRUN;
		    break;
	    }
	    /* Ack the interrupt by writing HOST_CMD reg. */
	    pmc->tlan_reg->host_cmd = CMD_ACK | (__uint32_t)int_stat | 
				      (ackcnt << 24);
	    }
            TRC_UVDMA(0x33333333,cpuid(),0,PACKDMA_ENTER);
	    uv_packdma_packet(pmc);
no_tlan_ack:
	    if ( (pmcno == 0) && (vint_stat & EN_LINT4) )
	    {
		pmcno = 1;
		goto next_pmc;
	    }
        }
#define D_DONE  1
#define D_ERROR 2
	if ( vint_stat & EN_DMA )
	{
	    /* Interrupt from UNIVERSE, go read the status register for
	     * interrupt conditions
	     */
	    volatile __uint32_t dma_stat;

	    UVREG_RD(fe,DGCS,dma_stat);
/*	    ASSERT (!(dma_stat & DGCS_ACT)); */
	    if ( (dma_stat & DGCS_STOP) || (dma_stat & DGCS_HALT) )
	    {
                printf("vfe board%u: DMA_ERROR - Operation Stopped or Halted!!\n",ctlr);
	        ASSERT(0);
	    }
	    if ( (dma_stat & DGCS_LERR) || (dma_stat & DGCS_VERR) ||
	        (dma_stat & DGCS_PERR) )
	    {
                printf("vfe board%u: DMA_ERROR - Status = 0x%x!!\n",ctlr,dma_stat);
		dmadone_flag = D_ERROR;
	        uv_restart_dma(fe);
	    }
            /*
             * UNIVERSE USER MANUAL page 2-66:
             * 6 possible interrupt (DGCS_STOP/DGCS_HALT/DGCS_LERR/
             * DGCS_VERR/DGCS_PERR/DGCS_DONE).
             * uv_dma_done() is called only for DMA DGCS_DONE.
             */
	    if (dma_stat & DGCS_DONE)
	    {  
                /* must be DMA_DONE
                 * ASSERT ( dma_stat & DGCS_DONE );
                 */
                uv_dma_done(fe,0,&type,&localq);
		dmadone_flag = D_DONE;
	    }
/*	    ASSERT (fe->uv_stat & UV_DMARDY); */
	    dma_stat &= DGCS_STOP|DGCS_HALT|DGCS_DONE|DGCS_LERR|DGCS_VERR|DGCS_PERR;
	    UVREG_WR(fe,DGCS,dma_stat);
	}
	UVREG_WR(fe,VINT_STAT,vint_stat);/* clear interrupt */
 	if ( bad_pmc ) {
	    	if_vfeinit(&bad_pmc->pmcif, 0);
		bad_pmc = 0;

	/*      break; */
	}
	}
	if (dmadone_flag == D_DONE) {
	    VFEDMA_LOCK(fe, s);
            fe->uv_stat |= UV_DMARDY;   
            VFEDMA_UNLOCK(fe, s);

            /* if it is an ARP request packet, the ARP respond
             * will be xmit right away. bug#571220
             */
            if ( type == ETHERTYPE_ARP)
                        uv_packdma_packet(fe->uv_lastpmc);

	    pmc = fe->uv_lastpmc;

	    if (pmc->vfe_rsvp_state & VFE_PSENABLED) {
		    int rval, txfree;
		    /*
		     * Notify RSVP packet sheduler of new packet length.
		     * But let the other channel go first, otherwise,
		     * one channel could starve the other.
		     */
		    VFE_GET_TXFREE(txfree, pmc->txq_geti, pmc->txq_puti);
		    ps_txq_stat(eiftoifp(&pmc->pmcif), txfree);

		    rval = uv_packdma_packet(OTHERPMC(pmc));
		    if (rval == 0)
			    uv_packdma_packet(pmc);
	    }
	}
/*
	else if (dmadone_flag == D_ERROR) {
	    uv_restart_dma(fe);
	}
*/
        IFNET_UNLOCK(&fe->pmc[0].pmc_if); 
        IFNET_UNLOCK(&fe->pmc[1].pmc_if); 

      	/*
         * reclaim the transmit mbufs.
         */
        mf = localq.ifq_head;
        while (mf) {
                m = mf;
                mf = mf->m_act;
                m_freem(m);
        }
	mf=txfullq.ifq_head;
        while (mf) {
		if (vfe_debug)
			printf("reclaim txfullq\n");
                m = mf;
                mf = mf->m_act;
                m_freem(m);
	}

	return 0;
}


/*
 * Queue a transmit mbuf in the transmit Q 
 */
int 
vfe_txque(pmc_dev_t *pmc,struct mbuf *mb,int test)
{
    register idx = pmc->txq_puti;

    idx = (idx+1) % (TX_QUEUE_NO - 1);
    if ( idx == pmc->txq_geti )
	/* transmit Q is full !! */
	return (-1);
    if ( test ) return (0);
    pmc->txq_list[pmc->txq_puti] = mb;
    pmc->txq_puti = idx;

    return 0;
}

/*
 * Hash function for TLAN multicast MAC addresses
 * Algorithm for generating hash value out of mac 
 * address is described in TLAN PG2.0 Rev 0.41
 * spec doc. Page 49
 */
static u_short 
vfe_multi_hash(pmc_dev_t *pmc,u_char *addr,int cmd)
{
    u_short reg;
    u_char hash = 0, rethash;
    u_int  HASH = 0;

    hash ^= addr[5] & 0x3f;
    hash ^= (addr[5] >> 6) | ((addr[4] << 2) & 0x3f);
    hash ^= (addr[4] >> 4) | ((addr[3] << 4) & 0x3f);
    hash ^= (addr[3] >> 2);
    hash ^= addr[2] & 0x3f;
    hash ^= (addr[2] >> 6) | ((addr[1] << 2) & 0x3f);
    hash ^= (addr[1] >> 4) | ((addr[0] << 4) & 0x3f);
    hash ^= (addr[0] >> 2);

    rethash = hash;
    /* read back the hash table before updating it */
    if ( hash > 31 ) {
	reg =  NET_HASH2 ;
	hash -= 32;
    } else  
	reg =  NET_HASH1;
    HASH  = (u_int) tl_dio_readW(pmc,reg);
    HASH |= (u_int) (tl_dio_readW(pmc,reg+2) << 16);

    if ( cmd == SIOCADDMULTI )
	HASH |=  (1 << hash);
    else
	HASH &= ~(1 << hash);
	
    tl_dio_write(pmc,reg  ,(HASH & 0x00ffff),DIO_WORD);
    tl_dio_write(pmc,reg+2,(HASH >> 16),DIO_WORD);

    return rethash;
}

static int
if_vfeinit(struct etherif *eif, int flags)
{
	int rc;

        (void) vfe_reset(eif);
        rc = vfe_init(eif,flags);
	return(rc);
	
}


/* 
 * IF Init support routine...
 */
/*ARGSUSED*/
static int
vfe_init(struct etherif *eif,
        int flags)
{
    register pmc_dev_t   *pmc;
    int timeout, ospeed, oduplex;
    struct ps_parms ps_params; /* RSVP support */



    pmc = (pmc_dev_t *)eif->eif_private;

    ospeed = pmc->speed;
    oduplex = pmc->duplex;


    if ( pmc->pmc_opstat != PMC_DOWN && !pmc->pend_reset) 
    {
       if ( eiftoifp(&pmc->pmcif)->if_flags & IFF_DEBUG )  
		printf("vfe%d: vfe_init: !PMC_DOWN, and !pend_reset, return\n", pmc->vfe_num);
	return (EBUSY);
    }
/* if I can be here, means either pmc is down, or I've been to
   vfe_reset, and it is going down, but not yet complete */
 
/*  printf("vfe_init entered PMC=%x..\n",pmc); */ 
    
    /* if (!(pmc->feinfo->uv_stat & UV_DMARDY)) { */
    /* when pend_reset was set, UV_DMARDY has already been checked; and
       since we'd be protected by the ip lock, no other thread would
       have come in. */

    if (pmc->pend_reset) {
       if ( eiftoifp(&pmc->pmcif)->if_flags & IFF_DEBUG )  
		printf("vfe_init: set pend_init, !uv_dmardy, return\n");
        pmc->pend_init = 1;
	return (EBUSY);
    }

    if (tl_lan_config(pmc) == -1) {
        printf("vfe board %d: Lan config failed..\n", pmc->feinfo->uv_unit);
        return (EIO);
    }

    timeout = 500;
    while (check_speed(pmc, &pmc->speed, &pmc->duplex)  == EBUSY) {
                DELAY(100);
                if (timeout-- <= 0)
                        break;
    }

    if ((pmc->speed == -1) || (pmc->duplex == -1)) {
        if ( eiftoifp(&pmc->pmcif)->if_flags & IFF_DEBUG ) 
           printf("vfe%d: auto-negotiation did not complete\n",pmc->vfe_num );
        tl_mii_write(pmc,pmc->phy,GEN_CTL, AUTOENB);
        pmc->speed = 10;
        pmc->duplex = 0;
    }
   if ( eiftoifp(&pmc->pmcif)->if_flags & IFF_DEBUG || showconfig ) 
      printf("vfe%d: speed  %d Mb/s duplex  %sduplex\n", pmc->vfe_num, pmc->speed, pmc->duplex? "full": "half");
    if ((pmc->speed != ospeed) || (pmc->duplex != oduplex))
        printf("vfe%d: %d Mb/s %sduplex\n",
                pmc->vfe_num,
                pmc->speed,
                pmc->duplex? "full": "half");


    pmc->tlan_reg->host_cmd = D_SWAP(CMD_INTON);

    /* Start TLAN Receive channel */
    tl_receive_enable(pmc);

    pmc->pmc_opstat = PMC_UP | PMC_RXRDY;
/*
    eiftoifp(&pmc->pmcif)->if_flags |= IFF_ALIVE;
*/
    eiftoifp(&pmc->pmcif)->if_timer = EDOG;  /* turn on watchdog */
        if ( eiftoifp(&pmc->pmcif)->if_flags & IFF_DEBUG ) 
	printf("vfe%d: watchdog timer turned on\n", pmc->vfe_num);

   if (pmc->duplex) {
	eiftoifp(&pmc->pmcif)->if_flags |= IFF_LINK0;
   } else {
	eiftoifp(&pmc->pmcif)->if_flags &= ~IFF_LINK0;
   }
   /* RSVP support */
    if (pmc->speed == 100) {
	eiftoifp(&pmc->pmcif)->if_baudrate.ifs_value = 1000*1000*100;
	ps_params.bandwidth = 100000000;
    } else {
	eiftoifp(&pmc->pmcif)->if_baudrate.ifs_value = 1000*1000*10;
	ps_params.bandwidth = 10000000;
    }
    ps_params.flags = 0;
    ps_params.txfree = TX_QUEUE_NO - 1;
    ps_params.txfree_func = vfe_txfree_len;
    ps_params.state_func = vfe_setstate;
    if (pmc->vfe_rsvp_state & VFE_PSINITIALIZED)
    {
        ps_reinit(&(eif->eif_arpcom.ac_if), &ps_params);
    } else {
            ps_init(&(eif->eif_arpcom.ac_if), &ps_params);
            pmc->vfe_rsvp_state |= VFE_PSINITIALIZED;
    }

    return 0;
}


/*
 * Reset the interface, leaving it disabled.
 */
static void
vfe_reset(struct etherif *eif)
{
    register pmc_dev_t   *pmc;

    pmc = (pmc_dev_t *)eif->eif_private;
    /* stop watchdog */
    /* ??? */
    eiftoifp(&pmc->pmcif)->if_timer = 0;
    if ( eiftoifp(&pmc->pmcif)->if_flags & IFF_DEBUG ) 
	printf("vfe%d: watchdog stopped\n", pmc->vfe_num);
   /* or eiftoifp(eif)->if_timer = 0; ?? */

    if ( pmc->pmc_opstat == PMC_DOWN) 
    {
       if ( eiftoifp(&pmc->pmcif)->if_flags & IFF_DEBUG )  
           printf("vfe%d: vfe_reset PMC_DOWN already, return\n", pmc->vfe_num);

	return;
    }
/*  printf("vfe_reset entered PMC=%x..\n",pmc); */

    pmc->tlan_reg->host_cmd = D_SWAP(CMD_INTOFF);
    tl_pmc_init(pmc);

    if (!(pmc->feinfo->uv_stat & UV_DMARDY)) {
        pmc->pend_reset = 1;
       if ( eiftoifp(&pmc->pmcif)->if_flags & IFF_DEBUG )  
		printf("vfe%d: vfe_reset: set pend_reset; !uv_dmardy\n", pmc->vfe_num);
	return;
    }
    tl_rebuild_struct(pmc);
}




/*
 * IF watchdog routine...
 */
/*ARGSUSED*/
static void
vfe_watchdog(struct ifnet *ifp)
{
    int i, unit;

    fenet_dev_t         *fe;
    pmc_dev_t   *pmc;
    int ctrl;

    ASSERT(IFNET_ISLOCKED(ifp));

    unit = ifp->if_unit;
    fe = vfe_tbl[unit];

    /* NOP....
    printf("vfe_watchdog(%d) entered...\n",unit);*/
    if (unit < 2) {
        fe = vfe_tbl[0];
        i = unit;
        pmc = &fe->pmc[i];
        ctrl = 0;
    }
    else {      /* currently it supports 2 boards, 4 ports*/
        i = unit % 2;
        fe = vfe_tbl[1];
        pmc = &fe->pmc[i];
        ctrl = 1;
    }
    tl_get_netstat(pmc);

    vfe_check_link(pmc);

    eiftoifp(&pmc->pmcif)->if_timer = EDOG;  /* turn on watchdog */

    /* check for missed interrupt */
    IFNET_UNLOCK(&fe->pmc[i].pmc_if); 
    (void) if_vfeintr(ctrl);
    IFNET_LOCK(&fe->pmc[i].pmc_if); 

/* debug:
 if (pmc->txfull != 0 )
printf("txfull in 10 secondes = %d\n", pmc->txfull);
*/
    /*if drop rate 10 frams over 10 seconds, reset the board*/
    if (pmc->txfull > vfe_droprate ) {
	if (vfe_debug)
		printf("txq_full: in 10 secondes drop packets = %d\n", pmc->txfull);
	(void) if_vfeinit(&pmc->pmcif, 0);
    }
    pmc->txfull = 0;

}

int
check_speed(pmc_dev_t *pmc, int *speed, int *duplex )
{
        u_short miireg;
        uint_t val0, val1, val6, val4, val5, val25;

        tl_mii_read(pmc, pmc->phy, 0, &miireg);
        val0 = miireg;


        if ((val0 & AUTOENB) == 0) {
                if (vfe_debug)
                   printf("Not Auto Negotiation Enable\n");
                *speed = (val0 & SPEED100MB) ? 100 : 10;
                *duplex = (val0 & DUPLEX)? 1 : 0;
                return(0);
         }
        tl_mii_read(pmc, pmc->phy, 1, &miireg);
        val1 = miireg;      /* pass status reg back */

         if ((val1 & AUTOCMPLT)  == 0) {
             *speed = -1;
             *duplex = -1;
             return(EBUSY);

         }
        tl_mii_read(pmc, pmc->phy, 6, &miireg);
        val6 = miireg;
        if ((val6 & R6_LPNWABLE) == 0) {
            if (vfe_debug)
               printf("Link Partner is Not AN Able\n");

        /* phy type is DP83840 */
                tl_mii_read(pmc, pmc->phy, 25, &miireg);
                val25 = miireg;
                *speed = (val25 & DP83840_R25_SPEED_10)? 10:100;
                *duplex = 0;

            return(0);
         }

        tl_mii_read(pmc, pmc->phy, 4, &miireg);
        val4 = miireg;
        tl_mii_read(pmc, pmc->phy, 5, &miireg);
        val5 = miireg;
        if ((val4 & R4_TXFD) && (val5 & R5_TXFD)) {
           *speed = 100;
           *duplex = 1;
        } else
           if ((val4 & R4_TX) && (val5 & R5_TX)) {
              *speed = 100;
              *duplex = 0;
        }
        else
           if ((val4 & R4_10FD) && (val5 & R5_10FD)) {
              *speed = 10;
                 *duplex = 0;
        }
       return(0);

}

static void
vfe_check_link(pmc_dev_t *pmc)
{
        int speed, duplex, sts_reg, interface_no = pmc->vfe_num;
        ushort_t miireg;

        tl_mii_read(pmc, pmc->phy, 1, &miireg);
        sts_reg  = miireg;

        if (sts_reg & RFLT) {
                if (pmc->rem_fault == 0)
                        cmn_err_tag(219,CE_ALERT, "vfe%d: remote hub fault\n", interface_no);
                pmc->rem_fault = 1;
        } else {
                if (pmc->rem_fault)
                        if ( eiftoifp(&pmc->pmcif)->if_flags & IFF_DEBUG )
                        printf("vfe%d: remote hub ok\n", interface_no);
                pmc->rem_fault = 0;
        }

        if ((sts_reg & JABBER) && (pmc->speed != 100)) {

                if (pmc->jabbering == 0)
                        cmn_err_tag(220,CE_ALERT, "vfe%d: jabbering\n", interface_no);
                pmc->jabbering = 1;
        } else {
                if (pmc->jabbering)
                        if ( eiftoifp(&pmc->pmcif)->if_flags & IFF_DEBUG )
                        printf("vfe%d: jabber ok\n", interface_no);
                pmc->jabbering = 0;
        }
        if (sts_reg & LINK) {
                if (pmc->link_down)
                {
                        cmn_err_tag(221,CE_ALERT, "vfe%d: link ok", interface_no);
                        /* check to see if speed has changed */
                        if (check_speed(pmc, &speed, &duplex) == 0)
                        {
                                if (vfe_debug)
                                        printf("speed change here\n");

                                if ((speed != pmc->speed) || (duplex != pmc->duplex))
                                    (void) if_vfeinit(&pmc->pmcif, 0);

                        }
                }
                pmc->link_down = 0;
        } else {
                if (pmc->link_down == 0)
                        cmn_err_tag(222,CE_ALERT, "vfe%d: link fail - check ethernet cable", interface_no);
                pmc->link_down = 1;
        }
}

/*
 * copy from mbuf chain to transmitter buffer in FXP memory.
 */
static int                           /* 0 or errno */
vfe_transmit(struct etherif *eif,    /* on this interface */
             struct etheraddr *edst, /* with these addresses */
             struct etheraddr *esrc,
             u_short type,           /* of this type */
             struct mbuf *mb)        /* send this chain */
{
    register pmc_dev_t   *pmc;
    register struct mbuf *m0;
    register int len, i;
    register struct ether_header *ehead;

    pmc = (pmc_dev_t *)eif->eif_private;
/*  printf("vfe_transmit%d: Entered - 0x%x\n",pmc->pmc_if.if_unit,pmc); */

    /* intend to queue the mbuf for aggregate DMA operation */
    if ( vfe_txque(pmc,mb,1) == -1 ) {
	/* the queue is full for some reason, drop the packet */
	if ( eiftoifp(eif)->if_flags & IFF_DEBUG )
  	    printf("vfe_transmit%d: Transmit queue full\n", 
			pmc->pmc_if.if_unit);
	IF_DROP(&pmc->pmc_if.if_snd);
	pmc->txfull++;
        uv_packdma_packet(pmc);
        goto xmitret1;
    }

    /*
     * Count the number of buffers to go out in the packet.  Conservative
     * estimate (may be fewer if multiple mbufs span MIN_TPKT_U).
     */
    for (m0 = mb, len = 0, i = 0; m0; m0 = m0->m_next) {
	len += m0->m_len;
	i ++;
    }

    if ( len > (BUFFSIZE-16)) {
  	printf("vfe_transmit%d: Packet too large(%d)\n",
		pmc->pmc_if.if_unit, len);
	IF_DROP(&pmc->pmc_if.if_snd);
	goto xmitret1;
    }
    /* make room for ethernet header */
    if (!M_HASCL(mb) && mb->m_off > MMINOFF+(EHDR_LEN+ETHERHDRPAD)) {
        mb->m_off -= sizeof(*ehead);
        mb->m_len += sizeof(*ehead);
    } else {
	m0 = m_get(M_DONTWAIT, MT_DATA);
	if ( m0 == 0 ) {
  	    printf("vfe_transmit%d: m_get failed\n", pmc->pmc_if.if_unit);
	    IF_DROP(&pmc->pmc_if.if_snd);
	    goto xmitret1;
	}
	m0->m_len = sizeof(*ehead);
	m0->m_off+= ETHERHDRPAD;
	m0->m_next = mb;
	mb = m0;
	i ++;
    }
    /* copyin DA and SA of the packet into Enet header */
    ehead = mtod(mb, struct ether_header *);
    *(struct etheraddr *)ehead->ether_dhost = *edst;
    *(struct etheraddr *)ehead->ether_shost = *esrc;
    ehead->ether_type = type;
    len += sizeof(*ehead);

    /* If the packet is made of many mbuf segements, we better reconstruct
     * them into one big cluster mbuf 
     */
    if ((len < MIN_TPKT)  || (i >= MBUF_SEG-1)) {
	m0 = m_vget(M_DONTWAIT, len+ETHERHDRPAD, MT_DATA);
	if (! m0 ) {
	    printf("vfe_transmit%d: No Cluster mbufs for transmit\n",
		    pmc->pmc_if.if_unit);
	    IF_DROP(&pmc->pmc_if.if_snd);
	    goto xmitret1;
	}
	m0->m_off += ETHERHDRPAD;
	if ( len != m_datacopy(mb,0,len,mtod(m0,caddr_t))) {
	    printf("vfe_transmit%d: m_datacopy failed.\n",
		    pmc->pmc_if.if_unit);
	    m_freem(m0);
	    IF_DROP(&pmc->pmc_if.if_snd);
	    goto xmitret1;
	}
	/* We've got a new mbuf, release the org. one */
	m_freem(mb);
	mb = m0;
	if (len < MIN_TPKT)
            len = MIN_TPKT;
        mb->m_len = len;
    }

    /*
     * self-snoop must be done prior to dma bus desc. build because
     * bus-desc. may change mbuf len and off.
     */
    if (RAWIF_SNOOPING(&pmc->pmc_rawif)
        && snoop_match(&pmc->pmc_rawif, mtod(mb, caddr_t), mb->m_len))
            ether_selfsnoop(&pmc->pmcif, mtod(mb, struct ether_header *),
                            mb, sizeof(*ehead), len);

    ASSERT(vfe_txque(pmc,mb,0) == 0);
    /* Go see if the DMA channel is busy, if not, start DMA and
       transfer this packet already */
    uv_packdma_packet(pmc);
    return (0);
xmitret1:
    m_freem(mb);
    return(ENOBUFS);
}



/*
 * Process an IP ioctl request 
 */
static int
vfe_ioctl(struct etherif *eif,
        int cmd,
        caddr_t data)
{
    register pmc_dev_t   *pmc;
    struct mfreq *mfr;
    union mkey *key;

    pmc = (pmc_dev_t *)eif->eif_private;

    mfr = (struct mfreq *) data;
    key = mfr->mfr_key;

    switch (cmd) {
	case SIOCADDMULTI:
	    pmc->vfe_nmulti++;
	    eiftoifp(eif)->if_flags |= IFF_FILTMULTI;
	    mfr->mfr_value = vfe_multi_hash(pmc,key->mk_dhost,SIOCADDMULTI);
	    break;

	case SIOCDELMULTI:
	    if (mfr->mfr_value != vfe_multi_hash(pmc,key->mk_dhost,SIOCDELMULTI))
		return (EINVAL);
	    pmc->vfe_nmulti--;
	    if (pmc->vfe_nmulti == 0) {
		    eiftoifp(eif)->if_flags &= ~IFF_FILTMULTI;
	    }
	    break;
 
	default:
            printf("vfe_ioctl(???) entered PMC=%x..\n",pmc);
            return (EINVAL);;
    }
    return (0);
}


#ifdef DEBUG
/* ARGSUSED */
int
if_vfeioctl(dev_t dev, unsigned int cmd, caddr_t arg, int mode, cred_t *crp, 
		int *rvalp)
{
	pmc_dev_t *pmc;
	fenet_dev_t *fe;
	tlioc_arg_t rw;
	int error = 0;
	int  board,port, print_console = 0;


	switch(cmd) {

	case TLIOC_GETPHYREG:
		if (copyin(arg, (caddr_t)&rw, sizeof(rw))) {
			error = EFAULT;
			break;
		}
		board = rw.val[0];		/* board number */
		port = rw.val[1];		/* port number */
	   	if (rw.val[2] != 0xff && rw.val[2] == 1)
			print_console = 1;
		fe = vfe_tbl[board];
		pmc = &(fe->pmc[port]);

		tl_get_phyreg(pmc, rw.val, print_console);
		if (copyout((caddr_t)&rw, arg, sizeof(rw))) {
			error = EFAULT;
			break;
		}
		break;

	case TLIOC_VMEDMASET:
	case TLIOC_UNIVERSEREAD:      
	case TLIOC_UNIVERSEWRITE:
	case TLIOC_PCIREAD:
	case TLIOC_PCIWRITE:
	case TLIOC_SETDMABUF:
	case TLIOC_GETDMABUF:
	case TLIOC_QUIT:
		break;

	default:
		error = EINVAL;
	}
	return error;
}
#endif

int	if_vfedevflag = D_MP;

/*
 * Opsvec for ether.[ch] interface.
 */
static struct etherifops vfeops =
        { if_vfeinit, vfe_reset, vfe_watchdog, vfe_transmit,
          (int (*)(struct etherif*,int,void*))vfe_ioctl };


void
if_vfeedtinit(struct edt *edtp)
{
	__uint32_t	 *io, *mem;
	fenet_dev_t 	 *fe;
	struct vme_intrs *intp;
	piomap_t	 *piomap, *memmap;
	struct etheraddr eaddr;
	register struct mbuf *m;
	u_int	unit;
	int     i;

	graph_error_t rc;
	vertex_hdl_t vmevhdl;
	vertex_hdl_t vfevhdl;
	char loc_str[32];

        intp = (vme_intrs_t *)edtp->e_bus_info;
        unit = edtp->e_ctlr;
	
        if (unit >= MAXBOARDS) {
                printf("vfe board%u: bad EDT entry\n", unit);
                return;
        }

	m = m_vget(M_DONTWAIT, sizeof(fenet_dev_t), MT_DATA);
	bzero(mtod(m, u_char *),sizeof(fenet_dev_t));
	vfe_tbl[unit] = 
        fe =  mtod(m, fenet_dev_t *);
        if (0 != fe->uv_boardaddr) {
                printf("vfe board%u: duplicate EDT entry\n", unit);
                return;
        }

        fe->uv_adapter = edtp->e_adap;
	fe->uv_unit = unit;

	/* e_space[0] is VMEbus IP-6200 A16 memory space for accessing
	 * UNIVERSE chip register set in PIO mode
	 */

        piomap = pio_mapalloc(edtp->e_bus_type, fe->uv_adapter,
                              &edtp->e_space[0], PIOMAP_FIXED, "vfe");
        if (!piomap) {
                printf("vfe board%u: UNIVERSE Register PIO map failed\n", unit);
                return;
        }
        io = (__uint32_t *)pio_mapaddr(piomap, edtp->e_iobase);

	/* e_space[1] is VMEbus IP-6200 A32 memory space for accessing
	 * the PCI local sync RAM and the PMC TLAN chip register sets.
	 */
        memmap = pio_mapalloc(edtp->e_bus_type, fe->uv_adapter,
                              &edtp->e_space[1], PIOMAP_FIXED, "vfe");
        if (!memmap) {
                printf("vfe board%u: PCIbus MEM map failed\n", unit);
                return;
        }

        mem = (__uint32_t *)pio_mapaddr(memmap, edtp->e_iobase2);

        /* Since VMECC resets all VME devices, should give enough
         * time for the board to settle down.
        DELAY(20*1000*1000);
         */

	/* poke UNIVERSE register area see if device is there */
        if (badaddr(io, sizeof(uint))) {
                pio_mapfree(piomap);
                pio_mapfree(memmap);
                printf("vfe board%d: UNIVERSE missing\n", unit);
                return;
        }

	/* store the IP-6200 VME A32 memory space Base address away */

	fe->VME_ADDR  = (vmebusaddr_t)edtp->e_iobase2;
        fe->VME_SLAVE = (caddr_t)mem;
        fe->SYNC_RAM  = (pciaddr_t)UV_PCI_MEM;
	fe->uv_boardaddr = fe->uv_regp = io;
        fe->pmc[0].feinfo = fe->pmc[1].feinfo = fe;
	/* Get DMA maps .... */
      
	for (i=0; i< DMA_LIST_NO; i++)
	{
	fe->uv_dmamap[i] = dma_mapalloc(DMA_A32VME,fe->uv_adapter,2,VM_NOSLEEP);
	if (!fe->uv_dmamap[i]) 
	{
	    	printf("vfe board%u: DMA mapping failed\n",unit);
		for (; i >= 0; i --)
		    dma_mapfree(fe->uv_dmamap[i]);
		return;
	}
	}
	fe->uv_iamap = iamap_get(fe->uv_adapter,DMA_A32VME);
	
        if (!intp->v_vec) {
ivec_alloc_again:
                intp->v_vec = vme_ivec_alloc(fe->uv_adapter);
                if (!intp->v_vec) {
                        intp->v_vec = vme_ivec_alloc(fe->uv_adapter);
                        if (!intp->v_vec) {
                                printf("vfe board%u: COME ON GIMMEA BREAK NULL-IVEC\n", unit);
                                return;
                        }
                }
        }

        if (intp->v_vec == (unsigned char)-1) {
                printf("vfe board%u: no interrupt vector\n", unit);
                return;
        }
	/* STATID register (offset 0x320) of UNIVERSE requires the interrupt
  	 * vector to be ODD numbers because the LSB of the vector byte
	 * is reserved by UNIVERSE for software interrupt(SW_IACK) purpose.
	 * Refer to Page App A-47 of the UNIVERSE user manual
	 */
	if ((intp->v_vec & 0x1) != 1)
	/* if we don't get an odd number, waste this one and try again */
		goto ivec_alloc_again;
        vme_ivec_set(fe->uv_adapter, intp->v_vec, if_vfeintr, unit);
/*      DP("vfe%u: IVEC set, Vector = %d!\n", unit,intp->v_vec); */

        fe->uv_intvec = intp->v_vec;       /* remember interrupt vector */
        fe->uv_intbrl = intp->v_brl;       /* and VME bus request level */

	/* Go perform the device specific initialization steps */

	if (uv_init(fe) == -1) {
                printf("vfe board%u: Universe initialization failed!!\n", unit);
		fe->pmc[0].pmc_opstat = fe->pmc[1].pmc_opstat = PMC_NOP;
                return;
        }

	for (i=0; i < 2; i ++)
	{
	    int j;
	    u_char  promaddr;
	    pmc_dev_t *pmc = &fe->pmc[i];
	    
	    /* Get the MAC addr from the EEPROM  */
	    for (promaddr=TLAN_MACADDR,j=0; j < 6; promaddr++,j++)
	    {
		if ( tl_eeprom_read(pmc,promaddr,&pmc->macaddr[j]) == -1 ) {
		   printf("vfe board%u EEPROM MACADDR read failed!! All 0x00 assumed\n",unit);
		   pmc->macaddr[0] =
		   pmc->macaddr[1] =
		   pmc->macaddr[2] =
		   pmc->macaddr[3] =
		   pmc->macaddr[4] =
		   pmc->macaddr[5] =  0x00;
		   break;
		}
	    }
	    bzero(&eaddr, sizeof(eaddr));
	    for (j=0; j < 6; j++)
		eaddr.ea_vec[j] = pmc->macaddr[j];
	    ether_attach(&pmc->pmcif, "vfe", (unit*2)+i,
		     (caddr_t)pmc, &vfeops, &eaddr,
		     INV_VFE,0);
            pmc->vfe_num = (unit*2)+i;


	}
	add_to_inventory(INV_NETWORK, INV_NET_ETHER, INV_VFE, unit, 0);
	rc = vme_hwgraph_lookup(fe->uv_adapter, &vmevhdl);

	if (rc == GRAPH_SUCCESS) {
	   char alias_name[8];
	   sprintf(loc_str, "%s/%d", EDGE_LBL_VFE, unit);
	   sprintf(alias_name, "%s%d", EDGE_LBL_VFE, unit);
	   rc = if_hwgraph_add(vmevhdl, loc_str, "if_vfe", alias_name,
		&vfevhdl);
	}

	txfullq.ifq_head = NULL;
	txfullq.ifq_tail = NULL;

	VFEDMA_INITLOCK(fe);


        /* if (showconfig) */ {
            printf("vfe board%u: %s - A16[0x%x], A32[0x%x]\n",unit,vfe_verid,edtp->e_iobase,edtp->e_iobase2);
        }
}


/* RSVP support
 * Called by pkt scheduling functions to determine length of driver queue -
 * how many tx buffers to allocate.
 */
static int
vfe_txfree_len(struct ifnet *ifp)
{
        int unit = ifp->if_unit;
	int vfe_txfree_cnt;
        fenet_dev_t *fe;
        pmc_dev_t *pmc;

        int i;

        fe = vfe_tbl[unit];
        if (unit < 2) {
                fe = vfe_tbl[0];
                pmc = &fe->pmc[unit];
        }
        else {
                i = unit %2 ;
                pmc = &fe->pmc[i];
        }
        VFE_GET_TXFREE(vfe_txfree_cnt, pmc->txq_geti, pmc->txq_puti);
        return(vfe_txfree_cnt);
}

/*
 * RSVP
 * Called by packet scheduling functions to notify driver about queueing state.
 * If setting is 1 then there are queues and driver should try to minimize
 * delay (ie take intr per packet).  If 0 then driver can optimize for
 * throughput (ie. take as few intr as possible).
 */

static void
vfe_setstate(struct ifnet *ifp, int setting)
{
            int i, unit = ifp->if_unit;
            pmc_dev_t *pmc;
            fenet_dev_t *fe;

            fe = vfe_tbl[unit];
            if (unit < 2) {
                fe = vfe_tbl[0];
                pmc = &fe->pmc[unit];
            } else {
                fe = vfe_tbl[1];
                i = unit % 2;
                pmc = &fe->pmc[i];
            }


            if (setting)
                    pmc->vfe_rsvp_state |= VFE_PSENABLED;
            else
                    pmc->vfe_rsvp_state &= ~VFE_PSENABLED;
}

