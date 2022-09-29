/*
 * if_gfe ThunderLAN GIO Fast Ethernet driver for IP22
 *
 * Copyright 1996-7, Silicon Graphics, Inc.  All Rights Reserved.
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */
#ident "$Revision: 1.13 $"

/* NOTE: the gfe hardware requires Little Endian formats for PIO operations
 * hence the Master_swapXX macros.  The DMA traffic is swizzled in the
 * interface to the GIO bus.
 *
 * NOTE: the hardware doesn't support full duplex operation.  There are
 * some remnants of attempting to do this, and it's possible that a HW
 * revision might change this.  For now, do not set NC_DUPLEX in net_cmd
 * as it breaks things.
 *
 * TODO: under heavy bidirectional stress loads we see some small number
 * of the packets lost due to receiver overruns.  I believe this comes 
 * from taking the RX_EOC interrupt and switching to the other receive
 * buffer chain.  I don't want to rework the buffer management this
 * close to MR.  The if_ecf.c driver for the same chip manages a ring
 * of receive buffers and appears never to take an RX_EOC interrupt.
 * This occurance is not from having too small buffers (tried increasing
 * the size) or from running out of rx buffers all together (rv_full
 * was 0).  One sees RX_EOC interrupts about 1/42 of the RX_EOF interrupts
 * which squares up with the number of RX buffers found in gfe_replenish.
 * 
 * DID THIS ^^^^^^.  Left the old code (mostly) around for reference
 * under #ifdef OLDWAY.  No RXEOC interrupts anymore, and no RX overruns!
 */

#define _KERNEL	1
#if IP22        /* compile for IP22 only */

#include "sys/tcp-param.h"
#include "sys/types.h"
#include "sys/param.h"
#include "sys/systm.h"
#include "sys/sysmacros.h"
#include "sys/cmn_err.h"
#include "sys/debug.h"
#include "sys/cpu.h"
#include "sys/immu.h"
#include "sys/edt.h"
#include "sys/errno.h"
#include "sys/invent.h"
#include "sys/mbuf.h"
#include "sys/socket.h"
#include "sys/major.h"
#include "sys/giobus.h"
#include "netinet/if_ether.h"
#include "net/if.h"
#include "net/raw.h"
#include "net/rsvp.h"
#include "net/soioctl.h"
#include "ether.h"
#include "sys/if_gfe.h"
#include "sys/cred.h"
#include "sys/hwgraph.h"
#include "sys/iograph.h"
#ifdef GFEDEBUG
#include <sys/idbgentry.h>
#endif
int if_gfedevflag = D_MP;

#define EDOG            (10*IFNET_SLOWHZ)  /* watchdog duration in seconds */
#define PHYWAITMAX 500		/* cycles of DELAYBUS(10) we'll wait for PHY*/

static struct {
	u_char	addr[6];
} eaddr[4] = {0x13, 0x12, 0x11, 0x10, 0x17, 0x16,
	      0x15, 0x14, 0x1b, 0x1a, 0x19, 0x18,
	      0x1f, 0x1e, 0x1d, 0x1c, 0x23, 0x22,
	      0x21, 0x20, 0x27, 0x26, 0x25, 0x24};

/*
 * definitions of the board structure 
 */
struct gfe_struct {
	struct etherif	gfe_eif;
	
	/* board's address */
	volatile u_int	*fe_bc;		/* byte count */
	volatile u_int	*fe_stat;	/* status */
	volatile struct gfe_conf *gfe_conf;	/* configuration regester */
	volatile struct gfe_addr	*gfe_addr;	/* board's register */

	/* network statistics */
	struct gfe_net_stat	net_stat;
	/* internal tuning statistics */
	int tx_acks, tx_ints, tx_ackmax;
	int rx_acks, rx_ints, rx_ackmax;
	int tx_list;
	int tx_idle;

	/* PHY */
	int	phy;
	/* saved from autonegotiation or hard setup */
	int	speed;
	int	duplex;
	/* fault state variables. Sensed in PHY R1.
	 * 1 says we've already seen and reported the problem
	 * 0 no fault
	 */
	int	rmfault;
	int	jabber;
	int	linkfail;

	/* ethernet address */
	struct {
		u_char	addr[6];
	} eaddr[4];

	/* xmit/recv buffers */
	/* we ping pong between the 0 and 1 lists for each of these
	 * they are all for one board, a different set is for the other
	 * board if there are two GFE cards
	 */
	struct 	fake_dma_bd	*xm_dma_bd0, *xm_dma_bd1;
	struct 	fake_dma_bd	*rv_dma_bd0, *rv_dma_bd1;
	struct 	fake_dma_bd	*xm_dma_bd_cur, *rv_dma_bd_cur;
	struct 	fake_dma_bd	*r;
	struct  fake_dma_bd	*rcv_last;

	/*long	b_last;*/
	int	init_flag;
#define GFE_OFF	0
#define GFE_CONFIGED 1
#define GFE_ACTIVE 2
	int	configured;	/* board present? */

	/* debug/perf should be zero */
	struct {
		u_int	xm_drop;	/* dropped transmit frames */
		u_int	rv_full;	/* no receiver buffers available */
		u_int	rx_eoc;		/* End of Channel on receive*/
	} perf;
#define GFE_PSENABLED 1
#define GFE_PSINITIALIZED 2
	int gfe_rsvp_state;		/* RSVP support */
	int gfe_nmulti;
};

static struct gfe_struct gfe_info[2];
static int dbg_flag = DBG_SNOOP_OUT;

#define ei_ac           gfe_eif.eif_arpcom       /* common arp stuff */
#define ei_eif          ei_ac.ac_if             /* network-visible interface */
#define ei_rawif        gfe_eif.eif_rawif        /* raw domain interface */

/*
 * need very specific route
 */
struct dma_bd t_dma_bd = {EOF_LIST, TXCSTAT_REQ, 0};
struct dma_bd r_dma_bd = {EOF_LIST, RXCSTAT_REQ, SWAP_MAX_RPKT};

static int     gfe_replenish(struct gfe_struct *gfe, struct fake_dma_bd *cur);
static void    gfe_reset(struct etherif *eif);
static int     gfe_eeprom_access(struct gfe_struct *gfe, u_char addr, u_char *val, char op);
static int	if_gfeinit(struct etherif *eif, int flags);
static void	gfe_mii_negotiate(struct gfe_struct *gfe, int phy, int unit);
static void	gfe_phy_err(struct gfe_struct *gfe, int phy, int unit);
static int	check_speed(struct gfe_struct *gfe, int *speed, int *duplex, int *sts_reg );
static void	gfe_chk_link(struct gfe_struct *gfe);
static void	gfe_mii_sync(struct gfe_struct *gfe);
static void	gfe_mii_rw (struct gfe_struct *gfe, u_char dev, u_char reg, u_short *val, char op);
static int 	gfe_mii_init(struct gfe_struct *gfe,int unit);
static void 	gfe_mii_negotiate(struct gfe_struct *gfe, int phy, int unit);
static void 	gfe_phy_err(struct gfe_struct *gfe,int phy, int unit);
static void 	gfe_get_netstat(struct gfe_struct *gfe);
static int 	gfe_conf_init(struct gfe_struct *gfe);
static void 	gfe_nstat(struct gfe_struct *gfe);
static void 	gfe_adptchk(struct gfe_struct *gfe);
/*static void	gfe_rxeoc(struct gfe_struct *gfe);*/
static void 	gfe_buf_clean(struct gfe_struct *gfe);
static void 	gfe_eeprom_typed(struct gfe_struct *gfe, u_char op);
static int 	gfe_eeprom_ack(struct gfe_struct *gfe);
static void 	gfe_eeprom_sendbyte(struct gfe_struct *gfe, u_char data);
static int 	gfe_eeprom_access(struct gfe_struct *gfe, u_char addr, u_char *val, char op);
static int	gfe_rcvbuf_init(struct gfe_struct *gfe, struct fake_dma_bd *c);
#ifdef GFEDEBUG
static void	gfe_dump(void);
#endif
static void 	gfe_buf_link(struct fake_dma_bd *old, struct fake_dma_bd *new);
static void 	gfe_clean_one(struct fake_dma_bd *b, int cnt);
static void 	gfe_setstate(struct ifnet *ifp, int setting);
static int 	gfe_txfree_len(struct ifnet *ifp);


/* variables from master.d/if_gfe 
 * the ..._list_cnt variables are how many buffers to allocate
 * gfe_cfg controls whether to autonegotiate, or to force specific configs
 * gfe_phywait is how many 10 usec cycles to wait for PHY to complete
 *             autonegotiation or reset.  New PHY's allegedly take up
 *             to several seconds in autonegotiation
 */
extern int	gfe_rv_list_cnt, gfe_xm_list_cnt, gfe_cfg[2], gfe_phywait;





/*----------------------------------------------------------------------------
 * gfe_mii_sync() --
 *
 * This function is used to send Sync. pattern over the MII interface as
 * required by the MII management interface serial protocol
 *--------------------------------------------------------------------------*/
static void
gfe_mii_sync(struct gfe_struct *gfe)
{
    register int i;
    volatile struct gfe_addr *ba = gfe->gfe_addr;
    u_char sio;

    sio = Gfe_Dio_Read(u_char,NET_SIO);
    sio &= ~MTXEN;
    Gfe_Dio_Write(u_char,NET_SIO,sio);
    sio &= ~MCLK;
    Gfe_Dio_Write(u_char,NET_SIO,sio);

    /* delay for a while */
    sio = Gfe_Dio_Read(u_char,NET_SIO);

    sio |= MCLK;
    Gfe_Dio_Write(u_char,NET_SIO,sio);

    /* delay for a while */
    sio = Gfe_Dio_Read(u_char,NET_SIO);

    sio |= NMRST;
    Gfe_Dio_Write(u_char,NET_SIO,sio);

    /* provide 32 sync cycles, the PHY's pull-up resistor
     * will pull the MDIO line to a logic one for us, all
     * it needs is clocking signals on MCLK line
     */
    for (i = 0; i < 32; i++)
    {
        sio &= ~MCLK;
        Gfe_Dio_Write(u_char,NET_SIO,sio);
        sio |= MCLK;
        Gfe_Dio_Write(u_char,NET_SIO,sio);
    }
}

/*--------------------------------------------------------------------------
 * gfe_mii_rw()
 *
 * Description:
 * read word from PHY MII
 * MII frame read format:
 *
 *   start    operation  PHY      reg      turn          Data
 * delimiter    code     addr     addr    around
 * ----------------------------------------------------------------------
 * |   01   |   10   |  AAAAA  |  RRRRR  |  Z0  |  DDDD.DDDD.DDDD.DDDD  |
 * ----------------------------------------------------------------------
 *
 *
 * write word to PHY MII
 * MII frame write format:
 *
 *   start    operation  PHY      reg      turn          Data
 * delimiter    code     addr     addr    around
 * ----------------------------------------------------------------------
 * |   01   |   01   |  AAAAA  |  RRRRR  |  10  |  DDDD.DDDD.DDDD.DDDD  |
 * ----------------------------------------------------------------------
 */
static void
gfe_mii_rw (struct gfe_struct *gfe, u_char dev, u_char reg, u_short *val, char op)
{
    int i;
    volatile struct gfe_addr *ba = gfe->gfe_addr;
    u_char sio;


    /* disable MII interrupt, reset MDATA, and set MDCLK idle */
    Gfe_Netsio_Clr(MINTEN);
    Gfe_Netsio_Clr(MCLK);
    Gfe_Netsio_Set(MTXEN);

    /* Send bit stream 01 (start delimiters for MII interface) */
    Gfe_Netsio_Clr(MDATA); Gfe_Mii_Clck;
    Gfe_Netsio_Set(MDATA); Gfe_Mii_Clck;

    /* Send bit stream 10 (read operation) */

    /* send read/write operation code */
    if (op == READ_OP) {
        Gfe_Netsio_Set(MDATA); Gfe_Mii_Clck;
        Gfe_Netsio_Clr(MDATA); Gfe_Mii_Clck;
    }
    else {
        Gfe_Netsio_Clr(MDATA); Gfe_Mii_Clck;
        Gfe_Netsio_Set(MDATA); Gfe_Mii_Clck;
    }

    /* Send 5-bit device number (0/External, 0x1f/internal), MSB first */
    for (i = 0x10;i;i >>= 1)
    {
        if (i & dev)
        {
            Gfe_Netsio_Set(MDATA);
        }
        else
        {
            Gfe_Netsio_Clr(MDATA);
        }
        Gfe_Mii_Clck;
    }

    /* Send 5-bit register number, MSB first */
    for (i = 0x10;i;i >>= 1)
    {
        if (i & reg)
        {
            Gfe_Netsio_Set(MDATA);
        }
        else
        {
            Gfe_Netsio_Clr(MDATA);
        }
        Gfe_Mii_Clck;
    }

   if (op == WRITE_OP) {
            /* Turn around Send bit stream 10 */
            Gfe_Netsio_Set(MDATA); Gfe_Mii_Clck;
            Gfe_Netsio_Clr(MDATA); Gfe_Mii_Clck;

            /* write data to MII, MSB first */

            for (i = 0x8000; i; i >>= 1)
            {
                if ( *val & i )
                {
                    Gfe_Netsio_Set(MDATA);
                }
                else
                {
                    Gfe_Netsio_Clr(MDATA);
                }
                Gfe_Mii_Clck;
            }

            /* Disable MII Transmit */
            Gfe_Netsio_Clr(MTXEN);

    }
   else {    
            /* Clear TX enable, Wait "turn-around cycles" for returned data */
            Gfe_Netsio_Clr(MTXEN); Gfe_Mii_Clck;

            /* Check for Ack/Nak */
            sio = Gfe_Dio_Read(u_char,NET_SIO) & MDATA;
            Gfe_Mii_Clck;

            /* Now clock out the data regardless of Ack or Nak */
            for (*val = 0,i = 0x8000; i; i >>=1)
            {
                if ( Gfe_Dio_Read(u_char,NET_SIO) & MDATA )
                    *val |= i;
                Gfe_Mii_Clck;
            }
/*
            if ( sio )
                *val = 0xffff;
*/
    }

    /* Re-enable PHY interrupts */
    Gfe_Mii_Clck;
    Gfe_Netsio_Set(MINTEN);

    /* end MII read with sync pattern */
    for (i=0; i < 32; i++)
    {
        Gfe_Netsio_Set(MDATA);
        Gfe_Mii_Clck;
    }
}

/*----------------------------------------------------------------------------
 * gfe_mii_init() --
 *
 * This function is used to program TLAN network configuration registers
 *--------------------------------------------------------------------------*/
static int
gfe_mii_init(struct gfe_struct *gfe,int unit)
{
    int i = 0, phy;
    u_short val;


    /* Start with PHY configuration
     * First synchronize the MII....
     */
    gfe_mii_sync(gfe);
        /*gfe_mii_rw(gfe,phy,GEN_CTL, &val, READ_OP);*/

    /* now looking for external PHYs */
    for (phy=0;phy < 32; phy++) {
        val = PDOWN|ISOLATE|LOOPBK;
        gfe_mii_rw(gfe,phy,GEN_CTL, &val, WRITE_OP);
    }
    for (phy=0;phy < 32; phy++) {
        val = PHYRESET|PDOWN|ISOLATE|LOOPBK;
        gfe_mii_rw(gfe,phy,GEN_CTL, &val, WRITE_OP);
    }
    /*DELAYBUS(1*1000*1000);		*/
    for (phy=0;phy < 32; phy++) {
        gfe_mii_rw(gfe,phy,GEN_ID_HI,&val, READ_OP);
        if ( val  != PHY83840_IDHI )
            continue;
        gfe_mii_rw(gfe,phy,GEN_ID_LO,&val, READ_OP);
        if ( (val & 0xfc00)  != PHY83840_IDLO )
            continue;
        gfe->phy = phy;
	if (gfe->ei_eif.if_flags & IFF_DEBUG )
	    printf("gfe%d: PHY(DP83840) %d Found\n",unit, phy);
        break;
    }

    if ( phy == 32 )
    {
        printf("gfe%d: PHY not found, reset not done.\n",unit);
        return -1;
    }

    /* clear fault state */
    gfe->rmfault = gfe->jabber = gfe->linkfail = 0;
    gfe_mii_negotiate(gfe,phy,unit);
    if (gfe->ei_eif.if_flags & IFF_DEBUG )
    {
	for ( i = 0; i <= 0x1C; i++)
	{
	    gfe_mii_rw(gfe,gfe->phy,i,&val,READ_OP);
	    printf("PHY 0x%x = 0x%x\n",i,val);
	}
    }
    return 0;
}

/*-------------------------------------------------------------------------
 * gfe_mii_negotiate 
 * set up the PHY speed and duplex state
 *-------------------------------------------------------------------------*/
static void
gfe_mii_negotiate(gfe, phy, unit)
struct gfe_struct *gfe;
int phy, unit;
{
    int i,sts;
    u_short val;
    /*volatile struct gfe_addr *ba = gfe->gfe_addr;*/

    /* reset the PHY */
    val = PHYRESET;
    gfe_mii_rw(gfe, phy, GEN_CTL, &val, WRITE_OP);
    /* wait for it to take */
    for (i=0; i < gfe_phywait ; i++)
    {
	gfe_mii_rw(gfe, phy, GEN_CTL, &val, READ_OP);
	if (!(val & PHYRESET))  /* clears when reset completes */
	    break;
	DELAYBUS(10);
    }
    if (i >= gfe_phywait)
    {
	printf("gfe%d: PHY reset failed\n",unit);
    }
    /* if the PHY can do autonegotiation, do it, unless cfg is
     * forced (gfe_cfg[unit] != -1)
    .*/
    gfe_mii_rw(gfe,phy, GEN_STS, &val, READ_OP);
    if ( (val & AUTO_NEG) && (gfe_cfg[unit] == -1) )
    {
	if (showconfig)
	    printf("gfe%d: starting autonegotiation\n", unit);
	/* for some strange reason, R4 needs to be manually set, as it comes up 0x21
	 * so autonegotiates 10BaseT.
	 */
	val = 0xA1;  /* 100BaseT HDX; 10BaseT HDX; 802.3 protocol*/
	gfe_mii_rw(gfe, phy, AN_ADVERTISE, &val, WRITE_OP);
	/* start the negotiation */
	val = AUTORSRT | AUTOENB; 
	gfe_mii_rw(gfe,phy,0, &val, WRITE_OP);
	/* wait for it to complete */
	for (i=0; i < gfe_phywait ; i++)
	{
	    gfe_mii_rw(gfe, phy, 1, &val, READ_OP);
	    if (val & AUTOCMPLT)
		break;
	    DELAYBUS(10);
	}
	/*printf("autoneg %d\n",i);*/
	if (i >= gfe_phywait)
	{
	    printf("gfe%d: PHY autonegotiation time out\n",unit);
	/* let parallel link speed detection figure this out */
	/* had a force of a "reasonable" setting here, and it didn't work */
	}
    } else {
	if (showconfig)
	    printf("gfe%d: forcing cfg %d\n",unit,gfe_cfg[unit]);
	switch (gfe_cfg[unit])
	{
	    case 0:
	    /* no autonegotiation, no forcing.  NOOP */
		break;
	    
	    case 1:
	    /* force 100BaseT HDX */
		val = 0x2000;
		gfe_mii_rw(gfe, phy, 0, &val, WRITE_OP);
		break;

	    case 2:
	    /* force 10BaseT HDX */
		val = 0x0000;
		gfe_mii_rw(gfe, phy, 0, &val, WRITE_OP);
		break;

	    default:
		printf("gfe%d: illegal value in master.d/if_gfe(gfe_cfg[%d]=%d)\n",
		unit, unit, gfe_cfg[unit]);
		break;
	    }
    }
    if ( check_speed(gfe,&gfe->speed,&gfe->duplex,&sts) == 0 )
	if (showconfig)
	    printf("gfe%d: %d Mbs %s duplex\n",unit, gfe->speed,(gfe->duplex) ? "full":"half");
    else
	if (showconfig)
	    printf("gfe%d: link speed not available\n",unit);
}

/*
 * check and report the speed and duplex state of the link.
 * Duplicates logic in VFE driver and PCI TLAN driver.
 */
static int
check_speed(struct gfe_struct *gfe, int *speed, int *duplex, int *sts_reg )
{

    u_short r0,r1,r4,r5,r6,rx19;

    gfe_mii_rw(gfe, gfe->phy, 0, &r0, READ_OP);

    *sts_reg = r0;      /* pass status reg back */

    if ((r0 & AUTOENB) == 0) {
	/*printf("Not Auto Negotiation Enable\n");*/
	*speed = (r0 & SPEED) ? 100 : 10;
	*duplex = (r0 & DUPLEX)? 1 : 0;
	return(0);
    }

#ifdef XXX
/* this doesn't work, probably from touching R0 and not waiting */
    if ((r0 & AUTOCMPLT)  == 0) {
	*speed = -1;
	*duplex = -1;
	return(EBUSY);
    }
#endif
    gfe_mii_rw(gfe, gfe->phy, 6, &r6, READ_OP);
    if ((r6 & 0x1) == 0) {	/* is partner capable of autonegotiation?*/
	/*printf("Link Partner is Not AN Able\n");*/

	/* we know we have the DP83840 PHY, so look in reg 0x19 for the current
         * link speed, if the link is up. Used to wait for it, but that
	 * meant buzzing here when the link was down, a no no.
         */
	    gfe_mii_rw(gfe, gfe->phy, 1, &r1, READ_OP);
	    if ( r1 & LINK )	/* r19 is valid only when the link is up */
	    {
		gfe_mii_rw(gfe, gfe->phy, 0x19, &rx19, READ_OP);
		*speed = (rx19 & 0x40)? 10:100;
		*duplex = 0;
		return(0);
	    }
	    else
	    {
		/* watchdog will notice the link is down in a little while */
		*speed = -1;
		*duplex = -1;
		return(-1);
	    }
    }

    /* have autonegotiated speed and duplex, so use R4&R5 to report them */
    gfe_mii_rw(gfe, gfe->phy, 4, &r4, READ_OP);
    gfe_mii_rw(gfe, gfe->phy, 5, &r5, READ_OP);
    if ((r4 & 0x100) && (r5 & 0x100)) {
	*speed = 100;
	*duplex = 1;
    } else
    if ((r4 & 0x80) && (r5 & 0x80)) {
	*speed = 100;
	*duplex = 0;
    }
    else
    if ((r4 & 0x40) && (r5 & 0x40)) {
	*speed = 10;
	*duplex = 1;
    }
    else {
	*speed = 10;
	*duplex = 0;
    }
    return(0);
}

/*-------------------------------------------------------------------------
 * gfe_phy_err
 * Check the PHY errors.  As with much of the PHY code, this is derived
 * from if_ef, the IOC3 driver.  There are state variables for each of the
 * faults we look at.
 *-------------------------------------------------------------------------*/
static void
gfe_phy_err(gfe, phy, unit)
struct gfe_struct *gfe;
int phy, unit;
{
    u_short val;

    /* read reg 1 to look at error status */
    gfe_mii_rw(gfe, phy, 1, &val, READ_OP);

    if ( val & RFLT ) 	/* remote fault */
    {
	if ( gfe->rmfault == 0 )
	{
	    cmn_err_tag(225,CE_ALERT, "gfe%d: remote hub fault\n", unit);
	    gfe->rmfault = 1;
	} 
    } else {
	if (gfe->rmfault == 1 )
	    printf("gfe%d: remote hub restored\n",unit);
	gfe->rmfault = 0;
    }

    if (gfe->speed == 10)  	/* jabber, applies only to 10Mbs */
    {
	if (val & JABBER )
	{
	    if ( gfe->jabber == 0 )
	    {
		cmn_err_tag(226,CE_ALERT, "gfe%d: jabbering\n", unit);
		gfe->jabber = 1;
	    } 
	} else {
	    if (gfe->jabber == 1 )
		printf("gfe%d: jabbering cleared\n",unit);
	    gfe->jabber = 0;
	}
    }

    if ( !(val & LINK) ) 	/* link failure */
    {
	if ( gfe->linkfail == 0 )
	{
	    cmn_err_tag(227,CE_ALERT, "gfe%d: link fail - check ethernet cable\n", unit);
	    gfe->linkfail = 1;
	} 
    } else {
	if (gfe->linkfail == 1 )
	    cmn_err_tag(228,CE_ALERT, "gfe%d: link ok\n",unit);
	gfe->linkfail = 0;
    }
}


/*-------------------------------------------------------------------------
 * gfe_get_netstat() --
 *
 * This function is used to retrive Network statistics in the TLAN chip
 *-------------------------------------------------------------------------*/
static void
gfe_get_netstat(struct gfe_struct *gfe)
{
    	u_int	tmp;
	volatile struct gfe_addr *ba = gfe->gfe_addr;
	
	tmp = Gfe_Dio_Read(u_int, NET_SGOODTX);
        gfe->net_stat.tx_underrun += tmp>>24;
        gfe->net_stat.tx_good += tmp & 0x00ffffff;	/* little */
	
	tmp = Gfe_Dio_Read(u_int, NET_SGOODRX);
        gfe->net_stat.rx_overrun += tmp>>24;
        gfe->net_stat.rx_good += tmp & 0x00ffffff;      /* little */ 

	tmp = Gfe_Dio_Read(u_int, NET_SDEFERTX);
        gfe->net_stat.tx_deferred += tmp & 0x0000ffff;
        gfe->net_stat.crc += (tmp>>16) & 0x000000ff;
        gfe->net_stat.codeerr += tmp >> 24;

	tmp = Gfe_Dio_Read(u_int, NET_SMULTICOLLTX);
    	gfe->net_stat.tx_multicollision += tmp & 0x0000ffff;
    	gfe->net_stat.singlecollision += tmp >> 16;

	tmp = Gfe_Dio_Read(u_int, NET_SEXCESSCOLL);
    	gfe->net_stat.excesscollision += tmp & 0x000000ff;
    	gfe->net_stat.latecollision += (tmp>>8) & 0x000000ff;
    	gfe->net_stat.carrierlost += (tmp>>16) & 0x000000ff;

	gfe->ei_eif.if_ierrors = gfe->net_stat.crc + gfe->net_stat.codeerr +
				gfe->net_stat.rx_overrun + gfe->perf.rv_full;
	gfe->ei_eif.if_oerrors = gfe->net_stat.tx_underrun + gfe->perf.xm_drop;
	gfe->ei_eif.if_collisions = gfe->net_stat.latecollision;

	if (gfe->ei_eif.if_flags & IFF_DEBUG )
	{
	    printf("gfe%d: TLAN statistics-\n",gfe->ei_eif.if_unit);
	    printf("tx_good = %d, tx_underrun = %d , tx_deferred = %d\n",
		   gfe->net_stat.tx_good,gfe->net_stat.tx_underrun,
		   gfe->net_stat.tx_deferred);
	    printf("rx_good = %d, rx_overrun = %d , crc = %d\n",
		   gfe->net_stat.rx_good,gfe->net_stat.rx_overrun,
		   gfe->net_stat.crc);
	    printf("codeerr = %d, tx_multicollision = %d , singlecollision = %d\n",
		   gfe->net_stat.codeerr,gfe->net_stat.tx_multicollision,
		   gfe->net_stat.singlecollision);
	    printf("excesscollision = %d, latecollision = %d , carrierlost = %d\n",
		   gfe->net_stat.excesscollision,gfe->net_stat.latecollision,
		   gfe->net_stat.carrierlost);
	    /* software rcv buffer overflow, tx buffer not available */
	    printf("rv_full = %d, xm_drop = %d rx_eoc %d\n", 
		   gfe->perf.rv_full, gfe->perf.xm_drop,gfe->perf.rx_eoc);
	    printf("TX: ack_avg %d (%d/%d) ack_max %d\n", 
		(gfe->tx_ints) ? (gfe->tx_acks/gfe->tx_ints):0,
		gfe->tx_acks,gfe->tx_ints,
		gfe->tx_ackmax);
	    printf("RX: ack_avg %d (%d/%d) ack_max %d\n", 
		(gfe->rx_ints) ? (gfe->rx_acks/gfe->rx_ints):0,
		gfe->rx_acks,gfe->rx_ints,
		gfe->rx_ackmax);
	    printf("TX List Max: %d Tx Idle: %d (%d)\n",gfe->tx_list,gfe->tx_idle, gfe->tx_acks);
	}
}

/*----------------------------------------------------------------------------
 * gfe_conf_init() --
 *
 * This function is used to Autoconfig the PMC (PCI) device
 *--------------------------------------------------------------------------*/
static int
gfe_conf_init(struct gfe_struct *gfe)
{
        struct gfe_conf cf;
        int unit = gfe - gfe_info;

    /* check id */
    if (gfe->gfe_conf->conf.conf2.id != TL_ID)
    {
        printf("gfe%u:Autoconfig failed - Unknown Vendor Hardware:%4x!!\n",
                unit,gfe->gfe_conf->conf.conf2.id);
        return -1;
    }

    cf.conf.conf2.cmd = gfe->gfe_conf->conf.conf2.cmd;
    if ( cf.conf.conf1.status &
        (PARITYERR|SIGSYSERR|RCVMSTABT|RCVTRGABT|SIGTRGABT|MHZ66ABLE|DPARITY) )
    {
        printf("gfe%u: Reset failed - Autoconfig Status = 0x%4x!!\n",
                unit,cf.conf.conf1.status);
        return -1;
    }

/*
    cf.conf.conf2.class = gfe->gfe_conf->conf.conf2.class;
    if ( (cf.conf.conf1.baseclass != 0x02) ||
         (cf.conf.conf1.subclass != 0x80) )
    {
*/
        /* baseclass 0x02 = Network Controller,
         * Subclass  0x80 = Other Network controller than FDDI, Ether, TokenR
         */
/*
        printf("gfe%u: PCI Class/Subclass invalid - %2x/%2x!!\n",
                unit,cf.conf.conf1.baseclass,
                cf.conf.conf1.subclass);
        return -1;
    }
*/



    /* Set Base address of TLAN register space to PCI I/O space
     * don't touch baseaddr_mem reg since the PMC does not have
     * any on chip memory thus the return value of 0xfffffff0;
     */
    {
        u_int d = kvtophys((void *)gfe->gfe_addr);        /* mask for phys */
        gfe->gfe_conf->conf.conf1.baseaddr_mem = SwapInt(d);
    }

    /* Finally, enable the PCI device */
    gfe->gfe_conf->conf.conf2.cmd = MEMENABLE|MSTENABLE;
    return 0;
}

/*----------------------------------------------------------------------------
 * gfe_init() --
 *
 * This function is used to Autoconfig the PMC (PCI) device
 *--------------------------------------------------------------------------*/
int
gfe_init(struct gfe_struct *gfe, int unit)
{
	volatile struct gfe_addr *ba = gfe->gfe_addr;
	u_char net_cmd = 0;
	int i;


    if (gfe->configured == GFE_OFF)
	return -1;		/*board not present or broken */
    if (gfe_mii_init(gfe,unit) == -1)
	return -1;

    /* Configuration -
     *          Rx frame include CRC
     *          Pass up Error Frames
     *          Allow no fragmentation on Rx frames
     *    +     Allow single channel for Tx frames
     *    +     Enable TLAN internal PHY device
     */
    Gfe_Dio_Write(u_short, NET_CONFIG, (ONECHN));


    /* MAC protocol selected is 802.3 - 10/100Mbps)
     * Tx frames are *NOT* prioritized
     */

    /* Both TLAN Tx and Rx DMA data transfer burst sizes are
     * set here; Hi nibble for Tx, low nibble for Rx:
     *   0:  16 bytes
     *   1:  32 bytes
     *   2:  64 bytes (default set at RESET)
     *   3: 128 bytes
     *   4: 256 bytes
     * 5-7: 512 bytes
     *   8: reserved
     */
    Gfe_Dio_Write(u_char, BSIZE, 0x44); 		/* 512 */
    Gfe_Dio_Write(u_char, ACOMMIT, 0x50); 		/* 512 */


    /* Maximum Rx frame size is set here */
    Gfe_Dio_Write(u_short, MAXRX, Swap2B(MAX_RPKT));


    /* set up hash address registers */
    Gfe_Dio_Write(u_int, NET_HASH1, 0xffffffff);
    Gfe_Dio_Write(u_int, NET_HASH2, 0xffffffff);

    /* Program Network command register */
/*    net_cmd |= NRESET | NWRAP | CSF | TXPACE;	*/
    /*net_cmd |= NRESET | NWRAP | NC_DUPLEX | TXPACE | CSF;*/
    net_cmd = NRESET | NWRAP | TXPACE | CSF ;
    if (dbg_flag & DBG_PROMISC) {
    if (gfe->init_flag & IFF_PROMISC)
        net_cmd |= CAF;
    }

    Gfe_Dio_Write(u_char, NET_CMD, net_cmd);
    /*printf("NET_CMD=%x\n", net_cmd);*/

    /* set Local EtherNet MAC address */
    for (i=0; i<6; i++)
	Gfe_Dio_Write(u_char, eaddr[0].addr[i], gfe->eaddr[0].addr[i]);

    Gfe_Dio_Write(u_char, NET_SIO, (Gfe_Dio_Read(u_char, NET_SIO) | MINTEN));
    Gfe_Dio_Write(u_char, NET_MASK, MIIMASK);

    /*
    DELAYBUS(10);		
    gfe_mii_rw(gfe,gfe->phy,GEN_CTL,&val, READ_OP);
    gfe_mii_rw(gfe,gfe->phy,GEN_STS,&val, READ_OP);
    gfe_mii_rw(gfe,gfe->phy,GEN_STS,&val, READ_OP);
    gfe_mii_rw(gfe,gfe->phy,GEN_STS,&val, READ_OP);
    gfe_mii_rw(gfe,gfe->phy,GEN_STS,&val, READ_OP);
    gfe_mii_rw(gfe,gfe->phy,GEN_STS,&val, READ_OP);
    */
    /*printf("phy gen_sts=%x\n", val);*/
    /*DELAYBUS(2*1000*1000);*/

    return 0;
}

/*----------------------------------------------------------------------------
 * gfe_nstat() --
 *
 * This function is used to process TLAN Network status Interrupt
 *--------------------------------------------------------------------------*/
static void
gfe_nstat(struct gfe_struct *gfe)
{
    u_char   errcode;
    volatile struct gfe_addr *ba = gfe->gfe_addr;
    int unit = gfe - gfe_info;

        errcode = Gfe_Dio_Read(u_char, NET_STS);
        printf("gfe%u:Network Status ==> 0x%x \n",unit,errcode);
        if ( errcode & MIRQ )        /* MII interrupt */
        {
            u_short reg;
            gfe_mii_rw(gfe,gfe->phy,GEN_STS,&reg,READ_OP);
            printf("gfe%u:MII GEN_STS = 0x%x\n",unit,reg);
            gfe_mii_rw(gfe,gfe->phy,TLPHY_STS,&reg,READ_OP);
            printf("gfe%u:MII TLPHY_STS = 0x%x\n",unit,reg);
            gfe_mii_rw(gfe,gfe->phy,TLPHY_CTL,&reg,READ_OP);

            gfe_mii_sync(gfe);
            printf("gfe%u:MII TLPHY_CTL = 0x%x\n",unit,reg);
            if ( reg & TINT )
                reg &= ~TINT;
            gfe_mii_rw(gfe,gfe->phy,TLPHY_CTL,&reg,WRITE_OP);
        }
        /* clear NET_STS register by writing the stats byte back */
        Gfe_Dio_Write(u_char, NET_STS, errcode);

        eiftoifp(&gfe->gfe_eif)->if_flags &= ~IFF_ALIVE;
	if_gfeinit(&gfe->gfe_eif, gfe->init_flag);
        eiftoifp(&gfe->gfe_eif)->if_flags |= IFF_ALIVE;
}

/*----------------------------------------------------------------------------
 * gfe_adptchk() --
 *
 * This function is used to process TLAN Adapter Check Interrupt
 *--------------------------------------------------------------------------*/
static void
gfe_adptchk(struct gfe_struct *gfe)
{
    volatile struct gfe_addr *ba = gfe->gfe_addr;
    u_char   errcode;
    u_char   channo;
    u_int    ureg;
    int      unit = gfe - gfe_info;
    struct fake_dma_bd *x = gfe->xm_dma_bd_cur;

    ureg = ba->chan_parm;	
    errcode = (u_char)(ureg & 0xff);
    channo = (u_char)((ureg >> 21) & 0xff);

    printf("gfe%u:Adapter Check on channel =%x,  Failure Code = %x \n",unit,
            channo, errcode);

    eiftoifp(&gfe->gfe_eif)->if_flags &= ~IFF_ALIVE;

    /* toss buffers, don't know/care if they were transmitted */
    while (x && x->info == TRX_NOT)
	    x = x->next;

    while (x && x->dma_bd.cstat & FRAME_COMPLETE) {
	    m_freem(x->info);
	    x->info = TRX_NOT;	/* zero */
	    x->dma_bd.cstat = 0;
	    x = x->next;
    }
    if_gfeinit(&gfe->gfe_eif, gfe->init_flag);
    eiftoifp(&gfe->gfe_eif)->if_flags |= IFF_ALIVE;
}


/*
 * Link the xmit fake_dma_bd structs together.
 */
void
gfe_xmbuf_init(struct fake_dma_bd *cur)
{
	struct fake_dma_bd *next = cur + 1;
	int i;

	for (i=0; i < gfe_xm_list_cnt - 1; i++, cur++, next++) {
		gfe_buf_link(cur, next);
	}
}



/*----------------------------------------------------------------------------
 * gfe_replenish() --
 *
 * Initialize the receive buffer pointers
 *--------------------------------------------------------------------------*/
static int
gfe_replenish(struct gfe_struct *gfe, struct fake_dma_bd *cur)
{
	struct fake_dma_bd *p, *c;
	int i;

	p = c = cur;
	p++;
	if ( gfe_rcvbuf_init(gfe,c) == -1 ) return(-1);
	for (i = 0; i < gfe_rv_list_cnt; i++, p++, c++)
	{
	    if (gfe_rcvbuf_init(gfe, p) == -1)
	    {
		return(-1);
	    }
	    gfe_buf_link(c, p);
	}
	gfe->rcv_last = c;  /* loop post increments this so this is right*/
	return(0);
}

#ifdef OLDWAY
	/* bug fix for 473781. ipfilterd wasn't working */
        /* gfe = (cur == gfe_info[0].rv_dma_bd_cur) ? &gfe_info[0] : &gfe_info[1];
	 */
	if ((cur == (struct fake_dma_bd*)gfe_info[0].b_last) ||
	    (cur == gfe_info[0].rv_dma_bd0) ||
	    (cur == gfe_info[0].rv_dma_bd1)) {
	    gfe = &gfe_info[0];
	}
	else {
	    gfe = &gfe_info[1];
	}		
	p = c = cur;
	while (c != EOF_FAKE_LIST) {
		c->dma_bd = r_dma_bd;
		if (c->info = m_vget(M_DONTWAIT, BUF_SIZE, MT_DATA)) {
			i++;
			/* zero out the next ptr - bug fix */
			/*c->info->m_next = (struct mbuf *)0;*/
                        cp = mtod(c->info,caddr_t);
                        IF_INITHEADER(cp, &gfe->ei_eif, sizeof(struct etherbufhead));
                        c->dma_bd.list[0].count = Master_SwapInt((u_int)EHDR_LEN);
                        d = kvtophys(cp+RCVPAD);
                        c->dma_bd.list[0].addr = Master_SwapInt(d);
                        c->dma_bd.list[1].count = Master_SwapInt((u_int)EDAT_LEN) | END_BIT;
                        d = kvtophys(cp+sizeof(struct etherbufhead));
                        c->dma_bd.list[1].addr = Master_SwapInt(d);
                        dki_dcache_wbinval(cp+RCVPAD, MAX_RPKT+ETHERHDRPAD);
                        d = kvtophys(c->next);
                        c->dma_bd.next = Master_SwapInt(d);
		}
		else {
			if (p == c)
				return (-1);
			p->dma_bd.next = EOF_LIST;
		}
		p = p == c ? p : c;
		c = c->next;
	}
	return (0);
}
#endif

/*
 * gfe_rcvbuf_init
 * allocate a new mbuf to a receive buffer
 */

static char gfe_emergency[BUF_SIZE];
static int
gfe_rcvbuf_init(struct gfe_struct *gfe, struct fake_dma_bd *c)
{
    caddr_t cp;
    u_int d;

    if (c->info = m_vget(M_DONTWAIT, BUF_SIZE, MT_DATA)) {
	    cp = mtod(c->info,caddr_t);
	    IF_INITHEADER(cp, &gfe->ei_eif, sizeof(struct etherbufhead));
	    c->dma_bd.cstat = RXCSTAT_REQ;
	    c->dma_bd.size = SWAP_MAX_RPKT;
	    c->dma_bd.list[0].count = Master_SwapInt((u_int)EHDR_LEN);
	    d = kvtophys(cp+RCVPAD);
	    c->dma_bd.list[0].addr = Master_SwapInt(d);
	    c->dma_bd.list[1].count = Master_SwapInt((u_int)EDAT_LEN) | END_BIT;
	    d = kvtophys(cp+sizeof(struct etherbufhead));
	    c->dma_bd.list[1].addr = Master_SwapInt(d);
	    dki_dcache_wbinval(cp+RCVPAD, MAX_RPKT+ETHERHDRPAD);
	    return(0);
    }
    else {
	    /* egad! didn't get an mbuf
	     * zap this BD with a ptr to an emergency buffer that we ignore
	     * note that c->info is 0
	     * we assume the no mbuf condition is a transient so just
	     * silently drop them on the floor.  This has been observed
	     * only in sustained ttcp -u tests with a fast sender and
	     * a slow receiving system.
	     */
	    gfe->perf.rv_full ++;	/* count how many times we do this*/
	    c->dma_bd.cstat = RXCSTAT_REQ;
	    c->dma_bd.size = SWAP_MAX_RPKT;
	    c->dma_bd.list[0].count = Master_SwapInt((u_int)BUF_SIZE) | END_BIT;
	    d = kvtophys(gfe_emergency);
	    c->dma_bd.list[0].addr = Master_SwapInt(d);
	    return(-1);
    }
}

/*
 * gfe_buf_link
 * link a fake_dma_bd onto the end of the list
 */
static void
gfe_buf_link(struct fake_dma_bd *old, struct fake_dma_bd *new)
{
    u_int d;

    old->next = new;
    new->next = (struct fake_dma_bd *)0;
    d = kvtophys((caddr_t)new);
    old->dma_bd.next = Master_SwapInt(d);
    new->dma_bd.next = 0;
}

/*----------------------------------------------------------------------------
 * gfe_read() --
 *
 * This function is used to process input frame
 *--------------------------------------------------------------------------*/
static void
gfe_read(struct gfe_struct *gfe, struct fake_dma_bd *r)
{
        int snoopflags = 0;
        struct mbuf *m;
	int i;
        caddr_t cp, cp2;


	gfe->ei_eif.if_ipackets++;
        if ((m = r->info) == 0)
	{
	    /*printf("NO MBUF in rinfo!\n");*/
	    /*gfe_dump();*/
	    return;
	}
        m->m_len = Master_Swap2B(r->dma_bd.size);
	gfe->ei_eif.if_ibytes += m->m_len;
	m->m_len += sizeof(struct etherbufhead) - EHDR_LEN;

        cp = mtod(m, caddr_t) + sizeof(struct etherbufhead) -1 ;
        cp2 = cp - ETHERHDRPAD;
        for (i=EHDR_LEN; i; i--)
                *cp-- = *cp2--;

        ether_input(&gfe->gfe_eif, snoopflags, m);
        return;
}

#ifdef OLDWAY
/*----------------------------------------------------------------------------
 * gfe_rxeoc --
 *
 * This processes RXEOC interrupt and timeout
 *--------------------------------------------------------------------------*/
static void
gfe_rxeoc(struct gfe_struct *gfe)
{
	volatile struct gfe_addr *ba = gfe->gfe_addr;

	if (gfe_replenish((struct fake_dma_bd *)gfe->b_last) == -1)
		timeout((void (*)())gfe_rxeoc, gfe, 3600 * HZ);
	else
		ba->host_cmd = CMD_REQINT;
}
#endif

/*----------------------------------------------------------------------------
 * if_gfeintr() --
 *
 * This function is used to process TLAN interrupts
 * Get here at splimp()
 *--------------------------------------------------------------------------*/
void
if_gfeintr(int unit)
{
	struct gfe_struct *gfe = &gfe_info[unit];
	u_short	type;
	volatile struct gfe_addr *ba = gfe->gfe_addr;
	int ack_cnt;
	u_int d;
	u_short val;

	/* check for premature interrupts */
	IFNET_LOCK(&(gfe->ei_eif));
	if (gfe->configured != GFE_ACTIVE) {
		ether_stop(&gfe->gfe_eif,eiftoifp(&gfe->gfe_eif)->if_flags);
		/*printf("gfe%d: early interrupt, ignored.\n",unit);*/
		IFNET_UNLOCK(&(gfe->ei_eif));
		return;
	}

	/* turn off the TLAN Interrupts.   Didn't need this, left for ref. */
	/*ba->host_cmd = CMD_INTOFF;*/
	for (ack_cnt=0;;ack_cnt=0) {
		type = ba->host_intr;
		if (dbg_flag & DBG_PRINT)
                	printf("gfe%u:Interrupt stat = %x\n",unit,type);
		if (!type) {
			/* don't forget to turn interrupts back on */
			/*ba->host_cmd = CMD_INTON;*/
			IFNET_UNLOCK(&(gfe->ei_eif));
			return;
		}
		/* TLAN Manual (rev A, Dec 95, pg 39) recommends writing
		 * back the contents of host_intr to disable the interrupt
		 */
		ba->host_intr = type;

		switch (type) {
                case ADPCHK:        /* Adapter check */
    			gfe_mii_rw(gfe,gfe->phy,GEN_STS,&val, READ_OP);
    			/*printf("phy gen_sts=%x\n", val);*/
                    	gfe_adptchk(gfe);
			/* no need to ACK, we've reset the chip */
			continue;
		case NSTAT:	   /* Network status */
                    	gfe_nstat(gfe);
			ba->host_cmd = (type << 16) | CMD_ACK | ack_cnt;
			continue;
                case STATOV:        /* Stats. overflow */
                    	gfe_get_netstat(gfe);
			ba->host_cmd = (type << 16) | CMD_ACK | ack_cnt;
                    	continue;
		case RXEOF:	   /* Rx end-of-frame */
			{
			struct fake_dma_bd *tmpnext;
			while ( gfe->r && ((gfe->r->dma_bd.cstat) & FRAME_COMPLETE) ) {
			    ack_cnt++;
			    /* gfe_buf_link stomps on next ptr, save it */
			    tmpnext = gfe->r->next;
			    gfe_read(gfe, gfe->r);
			    gfe_rcvbuf_init(gfe, gfe->r);  /* get a new mbuf*/
			    gfe_buf_link(gfe->rcv_last,gfe->r);
			    gfe->rcv_last = gfe->r;
			    gfe->r = tmpnext;
			}
			ba->host_cmd = (type << 16) | CMD_ACK | ack_cnt;
			gfe->rx_ints++;
			gfe->rx_acks += ack_cnt;
			if ( ack_cnt > gfe->rx_ackmax)
				gfe->rx_ackmax = ack_cnt;
			continue;
#ifdef OLDWAY
			struct fake_dma_bd *r = gfe->r;
			
			while (r && r->info == TRX_NOT)
				r = r->next;

			while (r && ((r->dma_bd.cstat) & FRAME_COMPLETE) ) {
				ack_cnt++;
		    		gfe_read(gfe, r);
				r->dma_bd.cstat = RXCSTAT_REQ;
				r->info = TRX_NOT;	/* zero */
				r = r->next;
			}
			gfe->r = r;
			gfe->rx_ints++;
			gfe->rx_acks += ack_cnt;
			if ( ack_cnt > gfe->rx_ackmax)
				gfe->rx_ackmax = ack_cnt;
			break;
#endif
			}
		case DUMMY:
			ba->host_cmd = (type << 16) | CMD_ACK;
			continue;

		case RXEOC:	/* Rx end-of-channel */
			/* this should never happen */
			ba->host_cmd = (type << 16) | CMD_ACK;
			cmn_err_tag(229,CE_ALERT,"gfe%d: receive channel overrun",unit);
			gfe->perf.rx_eoc++;
			/* restart it*/
			d = kvtophys(&gfe->r->dma_bd);
			ba->chan_parm = SwapInt(d);
			ba->host_cmd = CMD_GO|CMD_RTX;
			continue;	/* have already ACK'd don't break;*/
#ifdef OLDWAY
			if (gfe->b_last & RDY) {
				gfe->r = gfe->rv_dma_bd_cur = (struct fake_dma_bd *)(gfe->b_last&~RDY);
				d = kvtophys(&gfe->rv_dma_bd_cur->dma_bd);
				ba->chan_parm = SwapInt(d);
				ba->host_cmd = CMD_GO|CMD_RTX;
				gfe->b_last = gfe->rv_dma_bd_cur == gfe->rv_dma_bd0 ? 
					(long)gfe->rv_dma_bd1 : (long)gfe->rv_dma_bd0;
				if (gfe_replenish((struct fake_dma_bd *)gfe->b_last) == -1)
					timeout((void (*)())gfe_rxeoc, gfe, 3600 * HZ);
				else
					gfe->b_last |= RDY;
			}
        		else {
                		gfe->r = gfe->rv_dma_bd_cur = RV_DMA_NOT;
				gfe->perf.rv_full++;
			}
			continue;
#endif
		case TXEOF:	/* Tx end-of-frame */
			{
			struct fake_dma_bd *x = gfe->xm_dma_bd_cur;

			while (x && x->info == TRX_NOT)
				x = x->next;

			while (x && ((x->dma_bd.cstat) & FRAME_COMPLETE)) {
				ack_cnt++;
				x->dma_bd.cstat = TXCSTAT_REQ;
				m_freem(x->info);
				x->info = TRX_NOT;	/* zero */
				x = x->next;
			}
			gfe->tx_ints++;
			gfe->tx_acks += ack_cnt;
			if ( ack_cnt > gfe->tx_ackmax)
				gfe->tx_ackmax = ack_cnt;
			ba->host_cmd = (type << 16) | CMD_ACK | ack_cnt;
			continue;
			}
		case TXEOC:	/* Tx end-of-channel */
		    	gfe->xm_dma_bd_cur = TX_DMA_AV;		/* 0 */
			ba->host_cmd = (type << 16) | CMD_ACK;
		    	if (gfe->xm_dma_bd0->info != TRX_NOT)
				gfe->xm_dma_bd_cur = gfe->xm_dma_bd0;
			if (gfe->xm_dma_bd1->info != TRX_NOT)
				gfe->xm_dma_bd_cur = gfe->xm_dma_bd1;
			if (gfe->xm_dma_bd_cur != TX_DMA_AV) {
				d = kvtophys(&gfe->xm_dma_bd_cur->dma_bd);
				ba->chan_parm = SwapInt(d);
				ba->host_cmd = CMD_GO;
			}

			/*
			 * RSVP support.  Only the current list can
			 * have packets in them since we just finished
			 * the other one.  Count them and report it to
			 * the packet scheduler.
			 */
			if (gfe->gfe_rsvp_state & GFE_PSENABLED) {
			    int qlen=0;
			    struct fake_dma_bd *x = gfe->xm_dma_bd_cur;
			    while (x && x->info) {
				    x = x->next;
				    qlen++;
			    }
			    ps_txq_stat(&(gfe->ei_eif),
					(gfe_xm_list_cnt * 2) - qlen);
		        }
			/*DELAYBUS(5);*/
			continue;
		}
		/*DELAYBUS(5);*/
		/* turn interrupts back on */
		/*ba->host_cmd = CMD_INTON;*/
		/*DELAYBUS(5);*/
	}
/* this isn't reached, you exit the for loop when type is 0, no interrupts left*/
}

/*---------------------------------------------------------------------------
 * gfe_multi_hash()  --
 *
 * This function is for TLAN multicast MAC addresses algorithm for generating 
 * hash value out of mac address is described in TLAN PG2.0 Rev 0.41 spec doc. 
 * Page 49
 *---------------------------------------------------------------------------*/
static u_short
gfe_multi_hash(struct gfe_struct *gfe,u_char *addr,int cmd)
{
    u_short reg;
    u_char hash = 0, rethash;
    u_int  HASH = 0;
    volatile struct gfe_addr *ba = gfe->gfe_addr;

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
        reg = NET_HASH1;
    HASH = Gfe_Dio_Read(u_int, reg);		/* little */

    if ( cmd == SIOCADDMULTI )
        HASH |=  (1 << hash);
    else
        HASH &= ~(1 << hash);

    Gfe_Dio_Write(u_int, reg, HASH);		/* little */

    return rethash;
}

/*--------------------------------------------------------------------
 * gfe_buf_clean()  --
 *
 * This function is to  initialize receiving/transmission descriptor and
 * free alloc'd mbufs 
 *-------------------------------------------------------------------*/ 
static void
gfe_buf_clean(struct gfe_struct *gfe)
{
	gfe_clean_one(gfe->rv_dma_bd0,gfe_rv_list_cnt);
        /* no need to clean rv_dma_bd1 anymore.  It is never initialized,
	   nor used after the 'newway' change. */
	/* gfe_clean_one(gfe->rv_dma_bd1,gfe_rv_list_cnt); */
	gfe_clean_one(gfe->xm_dma_bd0,gfe_xm_list_cnt);
	gfe_clean_one(gfe->xm_dma_bd1,gfe_xm_list_cnt);
#ifdef OLDWAY
	struct fake_dma_bd *bd;
	int fake_cnt, co = 4;

	bd = gfe->rv_dma_bd0;
	fake_cnt = gfe_rv_list_cnt;
set:
	while (fake_cnt--) {
		if (bd->info) {
			m_freem(bd->info);
			bd->info = TRX_NOT;
		}
		bd->next = bd+1;
		bd++;
	}
	bd--;
	bd->next = EOF_FAKE_LIST;
	switch (co--) {
	case 4:
		bd = gfe->rv_dma_bd1; fake_cnt = gfe_rv_list_cnt; goto set;
	case 3:
		bd = gfe->xm_dma_bd0; fake_cnt = gfe_xm_list_cnt; goto set;
	case 2:
		bd = gfe->xm_dma_bd1; fake_cnt = gfe_xm_list_cnt; goto set;
	}
#endif
}

/*
 * gfe_clean_one
 * give back mbufs in one buffer descriptor list
 *
 */
static void
gfe_clean_one(struct fake_dma_bd *b, int cnt)
{
    int i;
    struct fake_dma_bd *p = b;


    for (i = 0; i < cnt; i++, p++)
    {
	if (p->info) {
	    m_freem(p->info);
	    p->info = TRX_NOT;
	}
    }
}

/*----------------------------------------------------------------------------
 * if_gfeinit() --
 *
 * This function is the IF init support routine
 *--------------------------------------------------------------------------*/
static int
if_gfeinit(struct etherif *eif,
        int flags)
{
	struct gfe_struct *gfe = (struct gfe_struct *)eif->eif_private; 
	volatile struct gfe_addr *ba = gfe->gfe_addr;
	u_int	d;
        int     unit = gfe - gfe_info;
	struct ps_parms ps_params;	/*RSVP support*/

	if (gfe->configured == GFE_OFF)
	    return (TL_ERR_INIT);	/* board missing or broken */
	gfe_reset(eif);

        setgioconfig(unit ? GIO_SLOT_1 : GIO_SLOT_0,
                             GIO64_ARB_EXP0_RT | GIO64_ARB_EXP0_MST);

	*gfe->fe_bc = DMA_BC;

	gfe->init_flag = flags;		/* store */

	/* OK, go autoconfig the PMC device... */
	if (gfe_init(gfe,unit) == -1)
		return (TL_ERR_INIT);

	/* replenish receiving buffer */
	if (gfe_replenish(gfe,gfe->rv_dma_bd0) == -1 /*|| gfe_replenish(gfe,gfe->rv_dma_bd1) == -1*/) {
	    printf("gfe%u: receiving buffer alloc failed.\n",unit);
	    gfe_buf_clean(gfe);
	    return(ENOBUFS);
	}

	/* initialize the current pointers */
	gfe->r = gfe->rv_dma_bd_cur = gfe->rv_dma_bd0;
	gfe->xm_dma_bd_cur = TX_DMA_AV;		/* 0 */
	/*gfe->b_last = (long)gfe->rv_dma_bd1 | RDY;*/

	/*printf("gfe%u: get netstat ..\n",unit);*/
	gfe_get_netstat(gfe);

	gfe->configured = GFE_ACTIVE;

/*	ba->host_cmd = CMD_LDTMR | INT_TIMER;		*/
	ba->host_cmd = CMD_INTON | CMD_LDTHR | TX_INT_THR;

	d = kvtophys(&gfe->rv_dma_bd_cur->dma_bd);
/*	printf("gfe: receive chan_parm=%x\n", d);	*/
        ba->chan_parm = SwapInt(d);
	DELAYBUS(10);
        ba->host_cmd = CMD_GO|CMD_RTX;

	/* start the watchdog */
	eiftoifp(eif)->if_timer = EDOG;

	if (gfe->duplex) {
		eiftoifp(eif)->if_flags |= IFF_LINK0;
	} else {
		eiftoifp(eif)->if_flags &= ~IFF_LINK0;
	}
	/*
	 * RSVP initialization/reinitialization
	 * There are 2 transmit lists, each can hold gfe_xm_list_cnt pkts
	 */
	if (gfe->speed == 100) {
		eiftoifp(eif)->if_baudrate.ifs_value = 1000*1000*100;
		ps_params.bandwidth = 100000000;
	} else {
		eiftoifp(eif)->if_baudrate.ifs_value = 1000*1000*10;
		ps_params.bandwidth = 10000000;
	}
	ps_params.flags = 0;
	ps_params.txfree = gfe_xm_list_cnt * 2;
	ps_params.txfree_func = gfe_txfree_len;
	ps_params.state_func = gfe_setstate;
	if (gfe->gfe_rsvp_state & GFE_PSINITIALIZED)
	{
	    ps_reinit(&(eif->eif_arpcom.ac_if), &ps_params);
	} else {
	    ps_init(&(eif->eif_arpcom.ac_if), &ps_params);
	    gfe->gfe_rsvp_state |= GFE_PSINITIALIZED;
	}

        return 0;
}


/*----------------------------------------------------------------------------
 * gfe_reset() --
 *
 * This function is IF reset routine
 *--------------------------------------------------------------------------*/
static void
gfe_reset(struct etherif *eif)
{
	struct gfe_struct *gfe = (struct gfe_struct *)eif->eif_private;
	volatile struct gfe_addr *ba = gfe->gfe_addr;
	/* stop watchdog */
	eiftoifp(eif)->if_timer = 0;

	if ((eiftoifp(eif)->if_flags) & IFF_DEBUG)
	    printf("gfe%d: resetting adapter\n",eiftoifp(eif)->if_unit);
	/*printf("gfe: reset adapter\n");*/
	ba->host_cmd = CMD_ADPRST;
	DELAYBUS(100);
	ba->host_cmd = CMD_INTOFF;
	DELAYBUS(100);

	gfe_buf_clean(gfe);
}


/*----------------------------------------------------------------------------
 * gfe_ckk_link() --
 *
 * Check for changes in the speed or duplex character of the link
 *--------------------------------------------------------------------------*/
static void
gfe_chk_link(struct gfe_struct *gfe)
{
        int speed, duplex, sts_reg;

        if (check_speed(gfe, &speed, &duplex, &sts_reg) == 0)
	/* if speed or duplex has changed, reset and reinitialize */
	
            if ((speed != gfe->speed) || (duplex != gfe->duplex))
            {
		/* fix for 433776, we spin constantly if active bd unplugged */
		gfe->speed = speed;
		gfe->duplex = duplex;
                (void) gfe_reset(&gfe->gfe_eif);
		(void) if_gfeinit(&gfe->gfe_eif, gfe->init_flag);
            }
}

/*----------------------------------------------------------------------------
 * gfe_watchdog() --
 *
 * This function is the IF watchdog routine
 * get here at splimp with the IFNET_LOCK held for the interface
 *--------------------------------------------------------------------------*/
static void
gfe_watchdog(ifp)
struct ifnet *ifp;
{
    int unit = ifp->if_unit;
    struct gfe_struct *gfe = &gfe_info[unit];

    /* retrieve status from the chip */
    gfe_get_netstat(gfe);

    /* check for lost interrupts */
    IFNET_UNLOCK(ifp);
    (void) if_gfeintr(unit);
    IFNET_LOCK(ifp);

    /* check for PHY errors */
    gfe_phy_err(gfe,gfe->phy,unit);

    /* check for changes in speed or duplex */
    gfe_chk_link(gfe);

    /* restart watchdog */
    gfe->ei_eif.if_timer = EDOG;

}


/*----------------------------------------------------------------------------
 * gfe_transmit() --
 *
 * This function is the IF transmit routine
 *--------------------------------------------------------------------------*/
static int				/* 0 or errno */
gfe_transmit(struct etherif *eif,       /* on this interface */
	struct etheraddr *edst,		/* with these addresses */
	struct etheraddr *esrc,
	u_short type,			/* of this type */
	struct mbuf *m)			/* send this chain */
{
	register int i;
	register struct mbuf *n, *t;	/* t for error */
	register u_int len, mlen;
	register struct ether_header *eh;
	struct gfe_struct *gfe = (struct gfe_struct *)eif->eif_private; 
        int     unit = gfe - gfe_info;


	/*
	 * Count the number of buffers to go out in the packet.  Conservative
	 * estimate (may be fewer if multiple mbufs span MIN_TPKT_U).
	 */
	for (t = n = m, len = 0, i = 0; n != 0;) {
		if ((mlen = n->m_len) == 0) {
			t->m_next = n->m_next;
			m_free(n);
			n = t->m_next;
			continue;
		}
		len += mlen;
		i++;
		t = n;
		n = n->m_next;
	};
	if (len > ETHERMTU) {
		printf("gfe%u: too large(%d)\n", unit, (int)len);
		goto xmitret1;
	}

	/* make room for ethernet header */
	if (!M_HASCL(m) && m->m_off > MMINOFF+(EHDR_LEN+ETHERHDRPAD)) {
		m->m_off -= sizeof(*eh);
		m->m_len += sizeof(*eh);
	} else {
		n = m_get(M_DONTWAIT, MT_DATA);
		if (n == 0) {
			printf("gfe%u: m_get=0\n", unit);
			goto xmitret1;
		}
		n->m_len = sizeof(*eh);
		n->m_off += ETHERHDRPAD;
		n->m_next = m;
		m = n;
		i++;
	}

	eh = mtod(m, struct ether_header *);
	*(struct etheraddr *)eh->ether_dhost = *edst;
	*(struct etheraddr *)eh->ether_shost = *esrc;
	eh->ether_type = type;
	len += sizeof(*eh);

	if ((len <= MIN_TPKT_U) || (i > MAX_DMA)) {	
		int	msize;
copy:
		msize = MAX(len, MIN_TPKT_U) + ETHERHDRPAD;
		n = m_vget(M_DONTWAIT, msize, MT_DATA);
		if (!n) {
			printf("gfe_transmit%d: !mbuf\n", unit);
			goto xmitret1;
		}
		n->m_off += ETHERHDRPAD;
		if (len != m_datacopy(m,0,len,mtod(n,caddr_t))) {
			printf("gfe_transmit%d: m_datacopy failed.\n", unit);
			m_freem(n);
			goto xmitret1;
		}
		m_freem(m);
		m = n;
		if (len < MIN_TPKT_U)
			len = MIN_TPKT_U;
		m->m_len = len;
	}	

	/*
	 * self-snoop must be done prior to dma bus desc. build because
	 * bus-desc. may change mbuf len and off.
	 */
	if (dbg_flag & DBG_SNOOP_OUT) {
	if (RAWIF_SNOOPING(&gfe->ei_rawif)
	    && snoop_match(&gfe->ei_rawif, mtod(m, caddr_t), m->m_len)) {
		    ether_selfsnoop(&gfe->gfe_eif,
				    mtod(m, struct ether_header *),
				    m, sizeof(*eh), len-sizeof(*eh));
	}
	}

	/* 
	 * Device-dependent transmission setup
	 */
	{
		struct fake_dma_bd *px=0, *x; struct mbuf *n; int i;
		/*int s = splimp();*/
		u_int d;
		volatile struct gfe_addr *ba = gfe->gfe_addr;
		register caddr_t cp, cp2;
		int tx_list;
		
		x = gfe->xm_dma_bd_cur == gfe->xm_dma_bd0 ? gfe->xm_dma_bd1 : gfe->xm_dma_bd0;

		tx_list = 0;
		while (x->info != TRX_NOT) {
			if (x->next == EOF_FAKE_LIST) {
				gfe->perf.xm_drop++;
				m_freem(m);
				return (ENOBUFS);
			}
			else {
				if (x->dma_bd.next == EOF_LIST)
					px = x;
				x = x->next;
			}
			tx_list++;
		}

		if (tx_list > gfe->tx_list )
		    gfe->tx_list = tx_list;
		x->info = m;	
		x->dma_bd = t_dma_bd;		/* see above */
		x->dma_bd.size = Master_Swap2B((u_short)len);
/*		printf("tl: tx dma_bd.size=%x\n", x->dma_bd.size);	*/
		for (n = m, i = 0; n ; n = n->m_next) {
			if (i > MAX_DMA-1) {
				/*splx(s);*/
				goto copy;
			}
			cp = mtod(n,caddr_t);
			cp2 = cp + n->m_len - 1;
			d = kvtophys(cp);
			x->dma_bd.list[i].addr = Master_SwapInt(d);
			if (pnum(cp) != pnum(cp2)) {
				if (i > MAX_DMA-2) {
					goto copy;
				}
				x->dma_bd.list[i++].count = Master_SwapInt((u_int)(n->m_len-poff(cp2)-1));
				d = kvtophys(cp2) & ~POFFMASK;
				x->dma_bd.list[i].addr = Master_SwapInt(d);
				x->dma_bd.list[i++].count = Master_SwapInt((u_int)(poff(cp2)+1));
			}
			else
				x->dma_bd.list[i++].count = Master_SwapInt((u_int)n->m_len);
			dki_dcache_wbinval(cp, n->m_len);
		}
		x->dma_bd.list[i-1].count |= END_BIT;
		if (px) {
			d = kvtophys(px->next);
			px->dma_bd.next = Master_SwapInt(d);
		}
		if (gfe->xm_dma_bd_cur == TX_DMA_AV) {
		    gfe->tx_idle++;
			gfe->xm_dma_bd_cur = gfe->xm_dma_bd0;
			d = kvtophys(&gfe->xm_dma_bd_cur->dma_bd);
/*			printf("gfe: transmit chan_parm=%x, buf=%x\n", d, x->dma_bd.list[0].addr); */
        		ba->chan_parm = SwapInt(d);
        		ba->host_cmd = CMD_GO;
		}
		/*splx(s);*/
	}
	gfe->ei_eif.if_obytes += len;
	return(0);

xmitret1:
	m_freem(m);
	return(ENOBUFS);
}

/*----------------------------------------------------------------------------
 * gfe_ioctl() --
 *
 * This function is the ioctl routine
 *--------------------------------------------------------------------------*/
static int
gfe_ioctl(struct etherif *eif, int cmd, caddr_t data)
{
    struct gfe_struct *gfe = (struct gfe_struct *)eif->eif_private; 
    struct mfreq *mfr;
    union mkey *key;

    gfe = (struct gfe_struct *)eif->eif_private;

    mfr = (struct mfreq *) data;
    key = mfr->mfr_key;

    switch (cmd) {
        case SIOCADDMULTI:
		gfe->gfe_nmulti++;
		eiftoifp(eif)->if_flags |= IFF_FILTMULTI;
		mfr->mfr_value = gfe_multi_hash(gfe,key->mk_dhost,cmd);
		break;	
        case SIOCDELMULTI:
		if (mfr->mfr_value != gfe_multi_hash(gfe,key->mk_dhost,cmd)) {
			return EINVAL;
		}
		gfe->gfe_nmulti--;
		if (gfe->gfe_nmulti == 0) {
			eiftoifp(eif)->if_flags &= ~IFF_FILTMULTI;
		}
		break;
        default:
            return (EINVAL);
    }
    return 0;
}

/*
 * Opsvec for ether.[ch] interface.
 */
static struct etherifops gfeops =
        { if_gfeinit, gfe_reset, gfe_watchdog, gfe_transmit,
          (int (*)(struct etherif*,int,void*))gfe_ioctl };

/*----------------------------------------------------------------------------
 * if_gfeedtinit() --
 *
 * This function is the edtinit routine
 *--------------------------------------------------------------------------*/
void
if_gfeedtinit(struct edt *edtp)
{
	struct gfe_struct 	*gfe;
        struct etheraddr eaddr;
        u_int   unit = 0;
	u_char *c, addr;
	int max_list_cnt = (MIN_NBPP/sizeof(struct fake_dma_bd));
	volatile struct gfe_addr *ba;
	graph_error_t rc;
	char loc_str[32];
	vertex_hdl_t giovhdl, gfevhdl;


        unit = edtp->e_ctlr;
        if (unit > 1) {
                printf("gfe%u: bad EDT entry\n", unit);
                return;
        }

        gfe = &gfe_info[unit];
	gfe->configured = GFE_OFF;	/* board not present */

        /* poke register area see if device is there */
        if (badaddr((volatile unsigned long*)edtp->e_base, 4) ||
           ((*(u_long*)edtp->e_base & GIO_SID_MASK) != 0xb8)) {	/* ? GIO_ID_GTL = 0x03  set in sys/major.h */
                printf("gfe%d: missing\n", unit);
		return;
        }

	/* set up max PHY timeout (for autonegotiation and reset) */
	if ( (gfe_phywait <= 0) || (gfe_phywait > 500000) )
	    gfe_phywait = PHYWAITMAX;

        /* allocate 1 page each for receive/transmit descriptors */
        gfe->rv_dma_bd0 = kvpalloc(2,
                VM_NOSLEEP | VM_DIRECT | VM_UNCACHED, 0);
	if (!gfe->rv_dma_bd0) {
		printf("gfe%d: cannot allocate space for receive descriptors\n", unit);
		return;
	}
        gfe->rv_dma_bd1 = (struct fake_dma_bd *)((caddr_t)gfe->rv_dma_bd0 + MIN_NBPP);
	gfe_rv_list_cnt = MIN(gfe_rv_list_cnt, max_list_cnt);

        gfe->xm_dma_bd0 = kvpalloc(2,
                VM_NOSLEEP | VM_DIRECT | VM_UNCACHED, 0);
        if (!gfe->xm_dma_bd0) {
                printf("gfe%d: cannot allocate space for transmit descriptors\n", unit);
                kvpfree(gfe->rv_dma_bd0, 2);
                return;
        }
        gfe->xm_dma_bd1 = (struct fake_dma_bd *)((caddr_t)gfe->xm_dma_bd0 + MIN_NBPP);
	gfe_xm_list_cnt = MIN(gfe_xm_list_cnt, max_list_cnt);

	bzero((caddr_t)gfe->rv_dma_bd0, MIN_NBPP);
	bzero((caddr_t)gfe->rv_dma_bd1, MIN_NBPP);
	bzero((caddr_t)gfe->xm_dma_bd0, MIN_NBPP);
	bzero((caddr_t)gfe->xm_dma_bd1, MIN_NBPP);

	gfe_xmbuf_init(gfe->xm_dma_bd0);
	gfe_xmbuf_init(gfe->xm_dma_bd1);

	/*printf("tx_list_cnt=%x, rx_list_cnt=%x\n", gfe_xm_list_cnt, gfe_rv_list_cnt);*/
        /* store Base addresses */
	gfe->fe_bc = (u_int *)(edtp->e_base + FE_BC_OFF);
/*	*gfe->fe_bc = DMA_BC | LT_BIT;	*/
	*gfe->fe_bc = DMA_BC;
	gfe->fe_stat = (u_int *)(edtp->e_base + FE_STAT_OFF);
	gfe->gfe_conf = (struct gfe_conf *)(edtp->e_base + FE_CONF_OFF);
	gfe->gfe_addr = (struct gfe_addr *)(edtp->e_base + FE_ADDR_OFF);

	if (gfe_conf_init(gfe) == -1)
	{
		printf("gfe%d: autoconfiguration failure.\n", unit);
                kvpfree(gfe->rv_dma_bd0, 2);
                kvpfree(gfe->xm_dma_bd0, 2);
		return;
	}

        setgiovector(GIO_INTERRUPT_1, unit+GIO_SLOT_0, (void(*)())if_gfeintr, unit);

	/*
	 * get board's ethernet address
	 */
	/*printf("gfe%d: read eaddr=", unit); */
	for (c=eaddr.ea_vec, addr=EADDR; c<eaddr.ea_vec+ETHERADDRLEN; addr++, c++) {
		*c = 0;
		gfe_eeprom_access(gfe, addr, c, READ_OP);
		/*printf(":%x", *c);*/
	}
	/*printf("\n");*/

	/* reset the board */
	ba = gfe->gfe_addr;
	ba->host_cmd = CMD_ADPRST;
	DELAYBUS(100);
	ba->host_cmd = CMD_INTOFF;
	DELAYBUS(100);

        ether_attach(&gfe->gfe_eif, "gfe", unit, (caddr_t)gfe,
                     &gfeops, &eaddr, INV_GFE, 0);

	/* this used to be in ether_attach, but now must be done here */
	add_to_inventory(INV_NETWORK, INV_NET_ETHER, INV_GFE, unit, 0);
	/* add us to HW graph */
	rc= gio_hwgraph_lookup(edtp->e_base, &giovhdl);
	if (rc == GRAPH_SUCCESS) {
		char alias_name[8];
		sprintf(loc_str, "%s/%d", EDGE_LBL_GFE, unit);
		sprintf(alias_name, "%s%d", EDGE_LBL_GFE, unit);
		(void)if_hwgraph_add(giovhdl, loc_str, "if_gfe", alias_name,
			&gfevhdl);
	}

	bcopy(gfe->gfe_eif.eif_arpcom.ac_enaddr, gfe->eaddr[0].addr, sizeof(gfe->eaddr[0].addr));

	printf("gfe%d: GIO 100BaseTX Fast Ethernet\n", unit);
        if (showconfig)
                printf("gfe%d: hardware ethernet address %s\n",
                         unit, ether_sprintf(eaddr.ea_vec));

	gfe->configured = GFE_CONFIGED;  /*board present and initialized successfully */
	gfe->tx_acks = gfe->tx_ints = gfe->tx_ackmax = 0;
	gfe->rx_acks = gfe->rx_ints = gfe->rx_ackmax = 0;
	gfe->tx_list = 0;
	gfe->tx_idle = 0;
#ifdef GFEDEBUG
	idbg_addfunc("gfe_dump", (void (*)())gfe_dump);
#endif
}


/*-------------------------------------------------------------------------
 * gfe_eeprom_typed() --
 *
 * select EEPROM device and operation
 *------------------------------------------------------------------------*/
static void
gfe_eeprom_typed(struct gfe_struct *gfe, u_char op)
{
    volatile struct gfe_addr *ba = gfe->gfe_addr;
    u_char sio;
    u_char bit, s = 0xa0; /* { b1010000x }
                          * b4-7: 24C02 device type id
                          * b1-3:  first device
			  * b0:	  op 
                          */
    if (op == READ_OP)
	s |= 0x1;
    for (bit = 0x80; bit; bit >>= 1) {
        if (s & bit)
                Gfe_Netsio_Set(EDATA)
        else
                Gfe_Netsio_Clr(EDATA);
	Gfe_Eeprom_Clck;
    }
}

/*------------------------------------------------------------------------
 * gfe_eeprom_ack() --
 *
 * check for ack from eeprom
 *-----------------------------------------------------------------------*/
static int
gfe_eeprom_ack(struct gfe_struct *gfe)
{
    volatile struct gfe_addr *ba = gfe->gfe_addr;
    u_char sio;
    int ack;

    Gfe_Netsio_Clr(ETXEN);
    Gfe_Netsio_Set(ECLK);
    ack = (Gfe_Dio_Read(u_char,NET_SIO) & EDATA) == 0;
    Gfe_Netsio_Clr(ECLK);

    return (ack);
}

/*---------------------------------------------------------------------------
 * gfe_eeprom_sendbyte() --
 *
 * clock one byte out to eeprom
 *--------------------------------------------------------------------------*/
static void
gfe_eeprom_sendbyte(struct gfe_struct *gfe, u_char data)
{
    volatile struct gfe_addr *ba = gfe->gfe_addr;
    u_char sio;
    int bit;

    Gfe_Netsio_Set(ETXEN);
    for (bit = 0x80; bit; bit >>= 1) {
        if (data & bit)
                Gfe_Netsio_Set(EDATA)
        else
                Gfe_Netsio_Clr(EDATA);
        Gfe_Eeprom_Clck;
    }
}

/*----------------------------------------------------------------------------
 * gfe_eeprom_access() --
 *
 * This function is used to read EEPROM
 *--------------------------------------------------------------------------*/
static int
gfe_eeprom_access(struct gfe_struct *gfe, u_char addr, u_char *val, char op)
{
    volatile struct gfe_addr *ba = gfe->gfe_addr;
    u_char sio;
    u_char bit;
    /*int s = splimp();*/


    /* send eeprom start sequence */
	Gfe_Eeprom_Start_Seq;

    gfe_eeprom_typed(gfe, WRITE_OP);
    if (!gfe_eeprom_ack(gfe))
    	goto error_eeprom;

    /* Send addr in EEPROM */
    gfe_eeprom_sendbyte(gfe, addr);
    if (!gfe_eeprom_ack(gfe))
	goto error_eeprom;

   if (op == READ_OP) {
	    /* send eeprom start sequence */
	    Gfe_Eeprom_Start_Access_Seq;

	    gfe_eeprom_typed(gfe, READ_OP);
	    if (!gfe_eeprom_ack(gfe))
			goto error_eeprom;

        for (bit = 0x80; bit; bit >>= 1) {
                Gfe_Netsio_Set(ECLK);
                if (Gfe_Dio_Read(u_char,NET_SIO)&EDATA)
                        *val |= bit;
                Gfe_Netsio_Clr(ECLK);
        }
	Gfe_Eeprom_Stop_Access_Seq;
    }
    else {
        /* clock bytes out to eeprom */
	gfe_eeprom_sendbyte(gfe, *val);
        if (!gfe_eeprom_ack(gfe))
                goto error_eeprom;

        /* send eeprom stop sequence */
        Gfe_Eeprom_Stop_Seq;

        /* wait until eeprom writes the data,
         * when eeprom responds, internal write is complete
         */
        do {
                Gfe_Eeprom_Start_Seq;
                gfe_eeprom_typed(gfe, WRITE_OP);
        } while (!gfe_eeprom_ack(gfe));

    }

    return (0);

error_eeprom:
    printf("gfe: eeprom access error\n");
    return (-1);
}


/*ARGSUSED*/
int
if_gfeopen(dev_t *devp, int flag, int otyp, cred_t *crp)
{

	return 0;
}

/*ARGSUSED*/
int
if_gfeclose(dev_t dev, int flag)
{

	return 0;
}

/*ARGSUSED*/
int
if_gfeioctl(dev_t dev, unsigned int cmd, caddr_t arg, int mode, cred_t *crp,
          int *rvalp)
{
/* this hasn't been updated, used only in initial development */
#ifdef XXX
	struct gfe_struct *gfe = &gfe_info[0];
	__uint32_t	*base_addr = 0, *upper_bound;
	gfeioc_arg_t	rw;
	char	dir = 2;
	int	error = 0;
	static __uint32_t *dma_addr = 0;
	volatile u_int	*iaddr;
	volatile u_char	*caddr;
	volatile u_short	*saddr;
	u_char	addr;
	int	i;
	u_short	val;

/*	printf("gfeioctl: cmd=%x.\n", cmd); 	*/

	switch(cmd) {
	case GFEIOC_INFOGET:
		if (copyout((caddr_t)gfe, arg, sizeof(gfe_info)))
			error = EFAULT;
		break;
	case GFEIOC_UNIVERSEREAD:		/* gio addr */
	case GFEIOC_PCIREAD:
	case GFEIOC_MEMREAD:
	case GFEIOC_PHYREAD:
		if (dir>1)
			dir = READ_OP;
	case GFEIOC_UNIVERSEWRITE:
	case GFEIOC_PCIWRITE:
	case GFEIOC_MEMWRITE:
	case GFEIOC_PHYWRITE:
		if (dir>1)
			dir = WRITE_OP;

		switch(cmd) {
		case GFEIOC_UNIVERSEREAD:
		case GFEIOC_UNIVERSEWRITE:
			base_addr = (u_int *)0xbf400000;
			upper_bound = base_addr + 0x20000;	/* gfe size */
			break;
		case GFEIOC_PCIREAD:
		case GFEIOC_PCIWRITE:
			base_addr = (u_int *)0xbf480000;
			upper_bound = base_addr + 0x60000; /* tlan adds - 0x18000/4 */
			break;
		case GFEIOC_MEMREAD:
		case GFEIOC_MEMWRITE:
			base_addr = (u_int *)0x80000000;
			upper_bound = (u_int *)0xc0000000;
			break;
		case GFEIOC_PHYREAD:
		case GFEIOC_PHYWRITE:
			base_addr = (u_int *)0;
			upper_bound = (u_int *)0x1d;
		}

                if (copyin(arg, (caddr_t)&rw, sizeof(rw))) {
                                error = EFAULT;
                                break;
                }
		if (rw.count > MAX_COUNT) {
			error = EINVAL;
			break;
		}
		if (rw.gio_addr < base_addr || rw.gio_addr > upper_bound) {
				error = EIO;
				break;
		}
		saddr = (u_short *)rw.gio_addr;
		caddr = (u_char *)rw.gio_addr;
		iaddr = rw.gio_addr;
		for (i=0; i < rw.count; i++) {
			if (cmd == GFEIOC_PHYREAD) {
    				gfe_mii_rw(gfe,gfe->phy,(u_char)caddr,(u_short *)&rw.val[i], READ_OP);
				continue;
			}
			if (cmd == GFEIOC_PHYWRITE) {
    				gfe_mii_rw(gfe,gfe->phy,(u_char)caddr,(u_short *)&rw.val[i], WRITE_OP);
				continue;
			}
			switch(rw.type) {
			case UINT:
				if (dir == READ_OP)
					rw.val[i] = *iaddr++;
				else
					*iaddr++ = rw.val[i];
				break;
			case UCHAR:
				if (dir == READ_OP)
					rw.val[i] = *caddr++;
				else
					*caddr++ = rw.val[i];
				break;
			case USHORT:
				if (dir == READ_OP)
					rw.val[i] = *saddr++;
				else
					*saddr++ = rw.val[i];
			}
		}
copy_out:
		if (dir == READ_OP && copyout((caddr_t)&rw, arg, sizeof(rw))) {
			error = EFAULT;
			break;
		}
		break;
	case GFEIOC_SETDMABUF:
/*		if (copyin(arg, gfe->gfe_buffer, MAX_SIZE))	*/
			error = EFAULT;
		break;
	case GFEIOC_GETDMABUF:
/*		if (copyout(gfe->gfe_buffer, arg, MAX_SIZE)) 	*/
			error = EFAULT;
		break;
	case GFEIOC_EEPROM_READ:
		if (dir>1)
			dir = READ_OP;
	case GFEIOC_EEPROM_WRITE:
		if (dir>1)
			dir = WRITE_OP;
                if (copyin(arg, (caddr_t)&rw, sizeof(rw))) {
                                error = EFAULT;
                                break;
                }
		if (rw.count > MAX_COUNT || rw.type != UCHAR) {
			error = EINVAL;
			break;
		}
		for (i=0, addr=(u_char)rw.gio_addr; i < rw.count; addr++, i++) {
			if (dir == READ_OP) 
				rw.val[i] = 0;
			gfe_eeprom_access(gfe, addr, (u_char *)(&rw.val[i])+3, dir);
		}

		if (dir == READ_OP && copyout((caddr_t)&rw, arg, sizeof(rw))) {
			error = EFAULT;
			break;
		}
		break;		
	case GFEIOC_SET_DBGFLAG:
		if (copyin(arg, &dbg_flag, sizeof(dbg_flag)))
			error = EFAULT;
		break;
	case GFEIOC_GET_DBGFLAG:
		if (copyout(&dbg_flag, arg, sizeof(dbg_flag)))
			error = EFAULT;
		break;
	default:
		error = EINVAL;
	}

	return error;
#else
	return 0;
#endif
}

/*ARGSUSED*/
int
if_gferead(dev_t dev, char *uiop, char *crp)
{
	return EINVAL;
}

/*ARGSUSED*/
int 
if_gfewrite(dev_t dev, char *uiop, char *crp)
{
	return EINVAL;
}

#ifdef GFEDEBUG
static void
gfe_dump()
{
    struct gfe_struct *gfe;
    int i;
    struct fake_dma_bd *p;

    gfe = &gfe_info[0];
    if ( gfe->configured != GFE_ACTIVE )
	gfe = &gfe_info[1];
    if ( gfe->configured != GFE_ACTIVE )
    {
	printf("gfe not active\n");
	return;
    }

    printf("gfe->r 0x%x gfe->rcv_last 0x%x\n", gfe->r, gfe->rcv_last);
    p = gfe->rv_dma_bd0;
    for ( i = 0; i < gfe_rv_list_cnt; i++, p++)
    {
	printf("i: %d  p: 0x%x\n", i, p);
	printf("dma_bd.next: 0x%x next: 0x%x  ", p->dma_bd.next, p->next);
	printf("dma_bd.cstat: 0x%x \n", p->dma_bd.cstat);
    }
}
#endif /*GFEDEBUG*/

/*
 * RSVP
 * Called by packet scheduling functions to determine length of driver queue.
 */
static int
gfe_txfree_len(struct ifnet *ifp)
{
    int unit = ifp->if_unit;
    struct gfe_struct *gfe = &gfe_info[unit];
    struct fake_dma_bd *x;
    int qlen=0;

    if ( (x = gfe->xm_dma_bd_cur) == TX_DMA_AV)  /* we're idle */
	return (gfe_xm_list_cnt);

    /*
     * Non-idle case is more complicated.  I have to count the number
     * of packets on the current list, and then on the standby list.
     * The current list may start with a number of NULL x->info's from
     * frames that have completed.  So first skip those.
     */
    while (x && x->info == TRX_NOT)
	    x = x->next;
    while (x && x->info) {
	    qlen++;
	    x = x->next;
    }

    if (x == gfe->xm_dma_bd0)
	    x = gfe->xm_dma_bd1;
    else
	    x = gfe->xm_dma_bd0;
    while (x && x->info) {
	    x = x->next;
	    qlen++;
    }

    return((gfe_xm_list_cnt * 2) - qlen);
}

/*
 * RSVP
 * Called by packet scheduling functions to notify driver about queueing
 * state.  If setting is 1 then there are queues and driver should 
 * update the packet scheduler by calling ps_txq_stat() when the txq length
 * changes. If setting is 0, then don't call packet scheduler.
 */
static void
gfe_setstate(struct ifnet *ifp, int setting)
{
    int unit = ifp->if_unit;
    struct gfe_struct *gfe = &gfe_info[unit];

    if (setting)
	    gfe->gfe_rsvp_state |= GFE_PSENABLED;
    else
	    gfe->gfe_rsvp_state &= ~GFE_PSENABLED;
}

#endif /* IP22 */
