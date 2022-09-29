/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992-1996 Silicon Graphics, Inc.           *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * xbow.c - Hardware discovery of bridge. Only early init routines
 * go here.
 */

#include <sys/mips_addrspace.h>
#include <sys/SN/addrs.h>
#include <sys/SN/klconfig.h>
#include <sys/SN/promcfg.h>
#include <sys/SN/SN0/klhwinit.h>
#include <sys/SN/xbow.h>
#include <sys/SN/kldiag.h>
#include <sys/PCI/bridge.h>
#include <sys/PCI/PCI_defs.h>
#include <sys/gsnreg.h>		
#include <libkl.h>
#include <setjmp.h>
#include <rtc.h>
#include <sys/SN/intr.h>

extern unsigned long long int strtoull(const char *, char **, int);

struct xbow_link_credits {
	int wid_type;
	int credits;
} xb_lnk_credits[]= {
        {HUB_WIDGET_PART_NUM, 4},
        {BRIDGE_WIDGET_PART_NUM, BRIDGE_CREDIT},
        {XBOW_WIDGET_PART_NUM, XBOW_CREDIT},
        {XG_WIDGET_PART_NUM, 4},
        {HQ4_WIDGET_PART_NUM, 4},
        {GSN_A_WIDGET_PART_NUM, 4},
        {GSN_B_WIDGET_PART_NUM, 4}
};

#define DEFAULT_LINK_CREDITS 2

int
xbow_widget_credits(int wid_type)
{
	int i;

	for (i = 0; 
	     i < sizeof(xb_lnk_credits) / sizeof(xb_lnk_credits[0]); i++)
	    if (xb_lnk_credits[i].wid_type == wid_type)
		return xb_lnk_credits[i].credits;

	return DEFAULT_LINK_CREDITS;
}


#define XBOW_BASE(_n)	(__psunsigned_t)SN0_WIDGET_BASE(_n, 0)
#define XARB_LNK_HUB		0x0001
#define XARB_LNK_NETOK		0x0002
#define XARB_LNK_MASTER 	0x0004

#undef DELAY

#define DELAY(_x)	microsec_delay(_x)

#define HALF_SEC       	 500000

#define LINK_HUB(_xb, _lnk)	\
           (((_xb)->xb_link(_lnk)).link_arb_upper |= XARB_LNK_HUB)
#define LINK_NETOK(_xb, _lnk)	\
           (((_xb)->xb_link(_lnk)).link_arb_upper |= XARB_LNK_NETOK)

#define CHECK_HUB(_xb, _lnk)	\
           (((_xb)->xb_link(_lnk)).link_arb_upper & XARB_LNK_HUB)

#define CHECK_NETOK(_xb, _lnk)	\
           (((_xb)->xb_link(_lnk)).link_arb_upper & XARB_LNK_NETOK)

#define CHECK_MASTER(_xb, _lnk)	\
           (((_xb)->xb_link(_lnk)).link_arb_upper & XARB_LNK_MASTER)

#define LINK_MASTER(_xb, _lnk)	\
           (((_xb)->xb_link(_lnk)).link_arb_upper |= \
	            (XARB_LNK_MASTER | XARB_LNK_HUB))

#define ZERO_ARB(_xb, _lnk)	\
           (((_xb)->xb_link(_lnk)).link_arb_upper = 0)


int
xbow_link_alive(volatile __psunsigned_t xbow_base, int link)
{
	xb_linkregs_t 	*lp;

	lp = (xb_linkregs_t *)(xbow_base + XB_LINK_REG_BASE(link));
	if (lp->link_status & XB_STAT_LINKALIVE) 
	    return 1;

	return 0;
}

int
xbow_link_present(volatile __psunsigned_t xbow_base, int link)
{
	xb_linkregs_t 	*lp;

	lp = (xb_linkregs_t *)(xbow_base + XB_LINK_REG_BASE(link));
	if (lp->link_aux_status & XB_AUX_STAT_PRESENT)
	    return 1;

	return 0;
}


int
xbow_link_status(volatile __psunsigned_t xbow_base, int link)
{
	xb_linkregs_t 	*lp;

	lp = (xb_linkregs_t *)(xbow_base + XB_LINK_REG_BASE(link));
	if ((lp->link_status & XB_STAT_LINKALIVE) &&
	    (lp->link_aux_status & XB_AUX_STAT_PRESENT))
	    return 1;

        if ((lp->link_aux_status & XB_AUX_STAT_PRESENT) && 
		(!(lp->link_status & XB_STAT_LINKALIVE)))
		printf("WARNING: xbow_base: 0x%lx link: %d Widget present, "
			"but link not alive!\n", xbow_base, link);

	return 0;
}

int
xbow_get_active_link(__psunsigned_t xbow_base, int link)
{
	/* probe all the links */
#ifdef JONAH
	{
		if (link < 0xd)
			link = 0xd;
		else
			return -1;
#else
	for (; link < MAX_PORT_NUM; link++) {
#endif
                if (XBOW_WIDGET_IS_VALID(link)) {
			if (xbow_link_status(xbow_base, link))
			    return link;
		}
	}
	return -1;
}

void
xbow_link_init(__psunsigned_t xbow_base, int link)
{
	xb_linkregs_t 	*lp;

	lp = (xb_linkregs_t *)(xbow_base + XB_LINK_REG_BASE(link));

        lp->link_status; /* clear interrupts */
        lp->link_control = 0 ; /* NO Interrupts */
        lp->link_arb_upper = 0 ;
        lp->link_arb_lower = 0 ;
        lp->link_reset = 0 ;
}

/*
 * reset the widget and re-initialize to same config
 * by saving the link control and restoring them. 
 */
void
xbow_link_reset(__psunsigned_t xbow_base, int link)
{
	xb_linkregs_t 	*lp;
	xbowreg_t	save_link_ctrl;

	lp = (xb_linkregs_t *)(xbow_base + XB_LINK_REG_BASE(link));
        save_link_ctrl = lp->link_control;

	xbow_link_init(xbow_base, link);
	DELAY(1000);
	lp->link_control = save_link_ctrl;
}

void
xbow_set_link_credits(__psunsigned_t xbow_base, int link, int wid_type)
{
	xb_linkregs_t 	*lp;
	__uint32_t              xbow_ctrl;
	int credits;

	lp = (xb_linkregs_t *)(xbow_base + XB_LINK_REG_BASE(link));
	
	credits = xbow_widget_credits(wid_type);
        xbow_ctrl = lp->link_control;

	xbow_ctrl &= ~(XB_CTRL_WIDGET_CR_MSK);
	xbow_ctrl |= (credits << XB_CTRL_WIDGET_CR_SHFT);

	lp->link_control = xbow_ctrl;
}


	

int
xbow_get_master_hub(__psunsigned_t xbow)
{
	int link;

	for (link = BASE_XBOW_PORT; link < MAX_PORT_NUM; link++)
	    if (CHECK_MASTER((xbow_t *)xbow, link)) {
		db_printf("Check_master: link %d is master\n", link);
		return link;
	    }

	return 0;
}


int
xbow_check_link_hub(__psunsigned_t xbow, int link)
{
	return CHECK_HUB((xbow_t *)xbow, link);
}

int
xbow_check_hub_net(__psunsigned_t xbow, int link)
{
	return CHECK_NETOK((xbow_t *)xbow, link);
}


void
xbow_reset_arb_register(__psunsigned_t xbow, int link)
{
	ZERO_ARB((xbow_t *)xbow, link);
}

void
xbow_reset_arb_nasid(nasid_t nasid)
{
    int 		hub_link = hub_xbow_link(nasid);
    __psunsigned_t 	xbow_base;
    xbow_t	 	*xb ; 

    xbow_base = SN0_WIDGET_BASE(nasid, 0);
    xb = (xbow_t *)xbow_base ;
    if (XWIDGET_PART_NUM(xb->xb_wid_id) != XBOW_WIDGET_PART_NUM)
	return ;

    xbow_reset_arb_register(xbow_base, hub_link) ;
}

#define	XBOW_PEER_TOUT	(10 * 1000000)

/*
 *  With multiple hubs connected to xbow, we need one hub to probe xbow 
 *  and setup the config info. Arbitrate (loosely) as to who will become 
 *  the xbow master.
 */
int
xbow_init_arbitrate(nasid_t nasid)
{
    volatile __psunsigned_t	xbow_base = XBOW_BASE(nasid);
    int 			link = 0, master = 0, peer_hub_link;
    int 			hub_link = hub_xbow_link(nasid);
    __uint64_t ni_stat = LD(REMOTE_HUB(nasid, NI_STATUS_REV_ID));
    int net_master = 0;

    LINK_HUB((xbow_t *)xbow_base, hub_link);
    if (ni_stat & NSRI_LINKUP_MASK)
	LINK_NETOK((xbow_t *)xbow_base, hub_link);
    
    /*
     * This delay exists as a means of synchronization, so that multiple hubs
     * do not decide to become masters. Give someone else already in the
     * arbitration loop below time to figure out who should be the master.
     */

    DELAY(HALF_SEC);

    /* Now we check if the peer hub on the xbow is populated. If it is, and
     * hasn't reached the arb loop yet, we wait for a few secs and if its
     * still not here yet, we go to the arbitration.
     * Fix for Bug #430676 where a CPU takes a while in local arb before it
     * comes here
     */

    peer_hub_link = (hub_link == 0x9) ? 0xa : 0x9;

    if (!CHECK_HUB((xbow_t *)xbow_base, peer_hub_link) && 
		xbow_link_status(xbow_base, peer_hub_link)) {
        rtc_time_t 		start, end;

        start = rtc_time();
        end = start + XBOW_PEER_TOUT;

        db_printf("Xbow arbitration delay...(%d sec max)\n", 
		XBOW_PEER_TOUT / 1000000);

        while (!CHECK_HUB((xbow_t *)xbow_base, peer_hub_link) &&
		(rtc_time() < end))
            ;
    }

    /*
     * pick the lowest hub as the master, unless some other hub as already
     * indicated ownership.
     */
    for (link = BASE_XBOW_PORT; link < MAX_PORT_NUM; link++) {
	if (CHECK_HUB((xbow_t *)xbow_base, link)) {
	    if (!master) master = link;
	    if (CHECK_NETOK((xbow_t *)xbow_base, link))
		if (!net_master) net_master = link;
	    if (CHECK_MASTER((xbow_t *)xbow_base, link))
		master = net_master = link;
	}
    }
    if (net_master) {
	master = net_master;
	for (link = BASE_XBOW_PORT; link < MAX_PORT_NUM; link++) {
	    if ((CHECK_HUB((xbow_t *)xbow_base, link)) &&
		!(CHECK_NETOK((xbow_t *)xbow_base, link))) {
		    printf("XBOW LINK %d does NOT have CrayLink connection! Skipping\n", 
			   link);
		    ZERO_ARB((xbow_t *)xbow_base, link);
	    }
	}
    }
    else {
	db_printf("Master link %d. Clearing other arb registers\n", master);
	for (link = BASE_XBOW_PORT; link < MAX_PORT_NUM; link++) {
	    if (link != master) 
		ZERO_ARB((xbow_t *)xbow_base, link);
	}
    }
    
    if (master == hub_link)
	LINK_MASTER((xbow_t *)xbow_base, hub_link);

    return 1;
}

#define parse_xbow_nic_info	parse_bridge_nic_info

nic_t
xbow_get_nic(__psunsigned_t	xb)
{
	char 	xbow_nic_info[1024] ;
	char	xnic_val[32] ;
	nic_t	xnic ;

	*xbow_nic_info = *xnic_val = 0 ;

	/* XXX get the nic info into xbow_nic_info using xb */
	parse_xbow_nic_info(xbow_nic_info, "Laser", xnic_val) ;
	/* xnic = strtoull(xnic_val, 0, 16) ; */

	return (nic_t) 1 ; /* XXX xnic */
}

/*
 * Function:		peer_hub_on_xbow
 * Args:		nasid
 * Returns:		1 if a peer is found on xbow
 *			0 if no xbow or peer hub
 */

int peer_hub_on_xbow(nasid_t nasid)
{
	volatile __psunsigned_t	wid_base;
	volatile reg32_t	wid_id;
	int			peer_hub_link;
	jmp_buf			fault_buf;
	void			*prev_fault;
	extern void hubii_prb_cleanup(nasid_t, int);

	if (setfault(fault_buf, &prev_fault)) {
		printf("Reading link 0 (addr 0x%lx) failed\n", 
			(SN0_WIDGET_BASE(nasid, 0) + WIDGET_ID));
		hubii_prb_cleanup(nasid, 0);
		restorefault(prev_fault);
		return 0;
	}



	wid_base = SN0_WIDGET_BASE(nasid, 0);
	wid_id = LD32(wid_base + WIDGET_ID);

	restorefault(prev_fault);


	if (XWIDGET_PART_NUM(wid_id) != XBOW_WIDGET_PART_NUM)
		return 0;

	
	peer_hub_link = (hub_xbow_link(nasid) == 0x9) ? 0xa : 0x9;
	
	/* See if the xbow link is alive */
	if (!xbow_link_present(wid_base, peer_hub_link))
		return 0;

	/* Check if the widget on this xbow link is actually a hub */
	return xbow_check_link_hub(wid_base,peer_hub_link);
}

void
xbow_cleanup_link(__psunsigned_t xbow_base, int link)
{
	xb_linkregs_t 	*lp;

	lp = (xb_linkregs_t *)(xbow_base + XB_LINK_REG_BASE(link));

        lp->link_status_clr; /* read&clear to flush status */
}
