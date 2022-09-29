/*
 *
 * Copyright 1996, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
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

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>
#include <sys/immu.h>
#include <sys/kmem.h>
#include <sys/graph.h>
#include <sys/hwgraph.h>
#include <sys/iograph.h>
#include <sys/errno.h>
#include <sys/runq.h>
#include <sys/nodepda.h>
#include <sys/graph.h>
#include <sys/hwgraph.h>
#include <sys/invent.h>
#include <ksys/sthread.h>
#include <sys/SN/router.h>
#include <sys/SN/vector.h>
#include <sys/SN/agent.h>
#include <sys/SN/klconfig.h>
#include <sys/SN/error.h>
#if defined (SN0)
#include <sys/SN/SN0/sn0drv.h>
#include <sys/SN/SN0/ip27config.h>
#endif /* SN0 */
#include "sn_private.h"
#include <sys/nic.h>
#include "error_private.h"


#define UTIL_FACTOR	50	/* If port is active 1/UTIL_FACTOR of the
				 * time, turn on its LED
				 */
#define RTR_ERR_THRESH	500	/* Number of errors per minute that will
				 * trigger a warning.
				 */

extern int gather_craylink_routerstats;

typedef struct ulan_s {
	router_info_t	*rip;
	router_reg_t	val;
} ulan_t;


extern int craylnk_bypass_disable;

extern int hasmetarouter;
extern int rpparm_maxburst_val;


/* @@: PV# 535320 - Need to disregard retry errors on internal links
 * external_router_port_info: utility routine.
 * Checks whether the port connected to this router is an internal
 * or external link. This is determined by whether or not the router
 * is connected to some other router on the port, and the module of
 * each router is different from the module which we are checking that
 * is attached to this router.
 */
static int
external_router_port(router_info_t* rip, int port)
{
  klrou_t* routep ;
  lboard_t* brd ;
  if(rip->ri_brd){
    routep = (klrou_t*)find_first_component(rip->ri_brd,KLSTRUCT_ROU);
    if(routep){
      if(routep->rou_port[port].port_nasid != INVALID_NASID){
	brd = (lboard_t*)NODE_OFFSET_TO_K1(routep->rou_port[port].port_nasid,
					  routep->rou_port[port].port_offset);
	return (brd->brd_module != rip->ri_module);
      }
      return 0;
    }
    return 1;
  }
  return 1;
}

/* In klgraph.c */

extern invent_generic_t * 
klhwg_invent_alloc(cnodeid_t cnode, int class, int size);


/*

 * Get all the fields we need for our inventory structure and fill them in.
 */
/* ARGSUSED */
void
get_router_invent_info(invent_routerinfo_t** inventpp, router_info_t* rip)
{
  int port;
  char tmpbuf[4];
  invent_routerinfo_t* routerinfo = *inventpp;
  ASSERT(routerinfo);
  sprintf(routerinfo->rip.portmap,"%s", "[");

  for (port = 1; port <= MAX_ROUTER_PORTS; port++) {
    if (rip->ri_portmask & (1 << (port - 1))){  /* Found an active port */
      if(port != MAX_ROUTER_PORTS)
	sprintf(tmpbuf, "%d,", port);
      else 
	sprintf(tmpbuf, "%d", port); /* last one doesn't get a comma */
      strcat(routerinfo->rip.portmap, tmpbuf);
    }
  }
  strcat(routerinfo->rip.portmap, "]");
  /* Active ports done, now get topology and revid */
  routerinfo->rip.rev = 
    (rip->ri_stat_rev_id & RSRI_CHIPREV_MASK) >> RSRI_CHIPREV_SHFT;
}

/* @@: PV# 5432243 -routers do not show up in hinv.
 * build_invent_info: utility routine.
 * Get detailed inventory information for the router 
 * "attached" to this hub.
 */
/* ARGSUSED */
void 
build_router_invent_info(router_info_t* rip)
{
  int s;
  invent_routerinfo_t* invinfo = NULL;	   
  s = mutex_spinlock(&(rip->ri_lock));

  invinfo =(invent_routerinfo_t*) 
    klhwg_invent_alloc(rip->ri_cnode,INV_ROUTER, 
		       sizeof(invent_routerinfo_t));
  if(invinfo){
    invinfo->im_gen.ig_flag = INVENT_ENABLED;
    get_router_invent_info(&invinfo, rip);
    /* Add our inventory information to this vertex */
    hwgraph_info_add_LBL(rip->ri_vertex, INFO_LBL_DETAIL_INVENT, 
			 (arbitrary_info_t)invinfo);    
    hwgraph_info_export_LBL(rip->ri_vertex, INFO_LBL_DETAIL_INVENT,
			    sizeof(invent_routerinfo_t));
  }
  mutex_spinunlock(&(rip->ri_lock), s);
} 

#ifdef DEBUG
/*
 * Since this code is MT, we need to grab the lock when we print this
 * out so that people can actually read it.
 * Shows which ports we are monitoring.
 */
static int 
print_router_monitoring(router_info_t* rip)
{
  int i,s;
  char tmpbuf[4];
  char bufmsg[100];

  if (gather_craylink_routerstats == 0)
    return 0;

  sprintf(bufmsg, "O2k router (%s): external ports [ ",rip->ri_name);
  for(i = 1; i <= MAX_ROUTER_PORTS; i++)
    if(external_router_port(rip,i)){
      sprintf(tmpbuf, "%d ", i);
      strcat(bufmsg, tmpbuf);
    }
  strcat(bufmsg,"]");

  s = mutex_spinlock(&(rip->ri_lock));
  cmn_err(CE_CONT,"%s\n", bufmsg);
  mutex_spinunlock(&(rip->ri_lock), s);

  return 1;
}
#endif /* DEBUG */

/*
 * Read a router register given a router info pointer and a register number
 * See sys/SN/vector.h for return values.
 */
int
router_reg_read(router_info_t *rip, int regno, router_reg_t *val)
{
	return hub_vector_read(rip->ri_cnode, rip->ri_vector, rip->ri_writeid,
				regno, val);
}


/*
 * Write a router register given a router info pointer and a register number
 * See sys/SN/vector.h for return values.
 */
int
router_reg_write(router_info_t *rip, int regno, router_reg_t val)
{
	return hub_vector_write(rip->ri_cnode, rip->ri_vector, rip->ri_writeid,
				regno, val);
}


/*
 * Router access routine for NIC
 */

static int router_nic_access(nic_data_t data,
			     int pulse, int sample, int delay)
{
	ulan_t *ulan = (ulan_t *)data;

	ulan->val = nic_get_phase_bits() | MCR_PACK(pulse, sample);

    	router_reg_write(ulan->rip, RR_NIC_ULAN, ulan->val);

	while (router_reg_read(ulan->rip, RR_NIC_ULAN, &ulan->val) >= 0)
	    if (MCR_DONE(ulan->val))
		break;

	DELAY(delay);

    	return MCR_DATA(ulan->val);
}

/*
 * Set the LEDs on a router to the value stores in the router_info_t
 * structure.  Don't store again if we stored the asame value last time.
 * Vector writes take awhile.
 */
int
router_set_leds(router_info_t *rip)
{
	router_reg_t status;

	if (rip->ri_ledcache == rip->ri_leds)
		return 0;

	rip->ri_ledcache = rip->ri_leds;

	/* Maintain the chip outputs while setting the LEDs */
	status = (rip->ri_stat_rev_id & RSRI_CHIPOUT_MASK |
		  (router_reg_t)(rip->ri_leds & 0x3f)
			<< RSRI_SWLED_SHFT);

	return (router_reg_write(rip, RR_STATUS_REV_ID, status));
}


/*
 * Take the port error stuff from native router format and put it
 * into our absolute counters.
 */
/* ARGSUSED */
void
router_decode_port_errors(router_port_info_t *rpp)
{
	/* Get the stuff out of the register format and add it to the counts. */
	struct rr_status_error_fmt *error;
	int overflow = 0;

	error = (struct rr_status_error_fmt *)&(rpp->rp_port_error);

	if (error->rserr_retrycnt == RSERR_RETRYCNT_MASK >> RSERR_RETRYCNT_SHFT)
		overflow++;

	rpp->rp_retry_errors += error->rserr_retrycnt;
	if (error->rserr_snerrcnt == RSERR_SNERRCNT_MASK >> RSERR_SNERRCNT_SHFT)
		overflow++;
	rpp->rp_sn_errors += error->rserr_snerrcnt;

	if (error->rserr_cberrcnt == RSERR_RETRYCNT_MASK >> RSERR_CBERRCNT_SHFT)
		overflow++;
	rpp->rp_cb_errors += error->rserr_cberrcnt;

	if (overflow)
		rpp->rp_overflows++;
	
	return;
}

/* How much activity is required to turn on a router LED */
int router_util_factor = UTIL_FACTOR;

/*
 * Go to a router port and get the information stored there.
 */
int
router_acquire_port_info(router_info_t *rip, int port, int just_blink)
{
	router_port_info_t *rpp = &(rip->ri_port[port - 1]);
	/*REFERENCED*/
	int rc;
	int i;

	if (!just_blink) {
		/* Get the histogram register */
		rc = router_reg_read(rip, RR_HISTOGRAM(port), &rpp->rp_histograms);
#ifdef DEBUG
		if (rc)
			cmn_err(CE_NOTE, "Router port read failed: %s, p %d, err %d <%s>",
				rip->ri_name, port, rc, net_errmsg(rc));
#endif

		/* Get the port errors */
		rc = router_reg_read(rip, RR_ERROR_CLEAR(port), &rpp->rp_port_error);
#ifdef DEBUG
		if (rc)
			cmn_err(CE_NOTE, "Router port read failed: %s, p %d, err %d <%s>",
				rip->ri_name, port, rc, net_errmsg(rc));
#endif

		/* Decode the error info */
		router_decode_port_errors(rpp);

	}

	if (rpp->rp_excess_err) {
		if (just_blink)
			rip->ri_leds ^= (1 << (port - 1));
		else
			rip->ri_leds |= ((1 << (port - 1)) ^
					(rip->ri_ledcache & (1 << (port - 1))));
		/* Error rate excessive */
		return 1;
	} else  if (rip->ri_hist_type == RPPARM_HISTSEL_UTIL) {
		router_reg_t total_packets;
	
		total_packets = RHIST_GET_BUCKET(3, rpp->rp_histograms);
		rpp->rp_util[RP_TOTAL_PKTS] = total_packets ;

		if (total_packets) {
			rpp->rp_util[RP_BYPASS_UTIL] =
				(RHIST_GET_BUCKET(0, rpp->rp_histograms) *
				RR_UTIL_SCALE) / total_packets;

			rpp->rp_util[RP_RCV_UTIL] =
				(RHIST_GET_BUCKET(1, rpp->rp_histograms) *
				RR_UTIL_SCALE) / total_packets;

			rpp->rp_util[RP_SEND_UTIL] =
				(RHIST_GET_BUCKET(2, rpp->rp_histograms) *
				RR_UTIL_SCALE) / total_packets;

		}

		/* Decide whether to set the LEDs. */
		if ((rpp->rp_util[RP_RCV_UTIL] >= RR_UTIL_SCALE / UTIL_FACTOR)
		 || (rpp->rp_util[RP_SEND_UTIL] >= RR_UTIL_SCALE / UTIL_FACTOR))
			/* Turn on our port LED */
			rip->ri_leds |= (1 << (port - 1));
		/* Error rate OK */
	}
	else {
		for (i = 0; i < RP_NUM_BUCKETS; i++)
		    rpp->rp_util[i] = RHIST_GET_BUCKET(i, rpp->rp_histograms);
	}
	return 0;
}


/*
 * Get the statistics for a particular router and set its LEDs.
 */
int
router_acquire_info(router_info_t *rip)
{
	int port;
	/*REFERENCED*/
	int rc;
	int error = 0;
	int s;
	int just_blink = rip->ri_just_blink;


	if (gather_craylink_routerstats == 0)
		return 0;

	s = mutex_spinlock(&(rip->ri_lock));

	if (!just_blink) {
		/* Timestamp this entry */
		rip->ri_timestamp = absolute_rtc_current_time();

		/* Initialize LEDs to 0 */
		rip->ri_leds = 0;

		/*
		 * Turn off bypassing while we're doing vector ops
		 * to work around arouter bug.
		 * If bypassing is enabled, turn it off here.
		 */
		if (rip->ri_glbl_parms & RGPARM_BYPEN_MASK)
		    rc = router_reg_write(rip, RR_GLOBAL_PARMS,
					  rip->ri_glbl_parms &
					  ~RGPARM_BYPEN_MASK);
	}

	for (port = 1; port <= MAX_ROUTER_PORTS; port++) {
		/* Only do this for ports that are connected. */
		if (!(rip->ri_portmask & (1 << (port - 1)))) {
#if 0
			printf("Skipping port %d on router %s: "
				"pmask 0x%x, myport: 0x%x\n", port, 
				rip->ri_name, rip->ri_portmask,
				(1 << (port - 1)));
#endif
			continue;
		}

		/* Get port information */
		error += router_acquire_port_info(rip, port,
						  just_blink);
	}

	/* Set the LEDs. */
	rip->ri_leds &= rip->ri_portmask;
	rc = router_set_leds(rip);
#ifdef DEBUG
	if (rc)
		cmn_err(CE_NOTE, "Router write failed: rip 0x%x, err %d <%s>",
			rip, rc, net_errmsg(rc));
#endif

	/*
	 * Turn bypassing back on if we're not experiencing excessive
	 * link errors.  Otherwise, just leave them off.
	 * Link errors exacerbate the router bypassing bug.
	 * do this only if bypassing was initially enabled.
	 *
	 * If there is a metarouter, then do not enable bypass.
	 */
	if (craylnk_bypass_disable || hasmetarouter)
	    rip->ri_glbl_parms &= ~RGPARM_BYPEN_MASK;
	else
	    rip->ri_glbl_parms |=  RGPARM_BYPEN_MASK;

	if (rip->ri_glbl_parms & RGPARM_BYPEN_MASK)
	    if (!just_blink && !error) {
		    rc = router_reg_write(rip, RR_GLOBAL_PARMS,
					  rip->ri_glbl_parms);
	    }

#ifdef DEBUG
	/* How long did it take to gather the info? */
	rip->ri_deltatime = absolute_rtc_current_time() - rip->ri_timestamp;
#endif
	mutex_spinunlock(&(rip->ri_lock), s);

	return error;
}

static char *error_flag_to_type(unsigned char error_flag)
{
    switch(error_flag) {
    case 0x1: return ("retries");
    case 0x2: return ("SN0 errors");
    case 0x4: return ("CB errors");
    default: return ("Errors");
    }
}

static int
router_print_error(router_info_t *rip, router_reg_t reg,
		__int64_t delta, unsigned char error_flag, int port)
{
	__int64_t rate;

	reg *= rip->ri_per_minute;	/* Convert to minutes. */
	rate = reg / delta;

	if (rate > RTR_ERR_THRESH)  {
		/* Send a CE_MAINTENANCE only once per port per router */
		if(rip->ri_port_maint[port - 1] & error_flag) 
		{
		cmn_err(CE_WARN,
			"Excessive %s (%d/min) on %s, port %d", 
			error_flag_to_type(error_flag), rate,
			rip->ri_name, port); 
		}
		else  
		{
		rip->ri_port_maint[port -1] |= error_flag;
		cmn_err(CE_WARN | CE_MAINTENANCE,
			"Excessive %s (%d/min) on %s, port %d", 
			error_flag_to_type(error_flag), rate,
			rip->ri_name, port); 
		}
		return 1;
	} else {
		return 0;
	}
}


int
router_check_port(router_info_t *rip, int port)
{
	router_port_info_t *rpp = &(rip->ri_port[port - 1]);
	int printed = 0;
	__int64_t delta = rip->ri_timestamp - rip->ri_timebase;



	/*
	 * See PV # 535320
	 * The basic rules are as follows:
	 * 
	 * 1) Always watch for Excessive Checkbit errors.  This represents a
	 * real hardware problem with a board.
	 * 
	 * 2) Excessive retries on router ports 4, 5, and 6 (those within a
	 * module) can be ignored.
	 * 
	 * 3) Excessive retries on router ports 1, 2, and 3 (those connecting
	 * different modules) should be watched.  This indicates a probable
	 * frequency mismatch between the modules.
	 * 
	 *
	 * Sequence number errors are ignored by software.
	 * This involves checking which port on the router is connected
	 * to an external port. To do this, we traverse the hardware graph
	 * and find all external router ports. (See external_router_port())
	 *
	 * Some known external ports on typical configurations:
	 *
	 * Origin 200 HA = Speedo + Sprouter : check all links 
	 * Speedo - no router                
	 * O2K: normal -port 4,5,6 internal 
	 * O2k: star   -port 3,4,5,6 internal.
	 * O2k: metarouter - unknown.
	 */

	if(external_router_port(rip,port))
	  printed += router_print_error(rip, rpp->rp_retry_errors, delta,
					  0x1, port);
#if 0
	/* On Hub 1, sequence number errors are meaningless */
	printed += router_print_error(rip, rpp->rp_sn_errors, delta,
				      0x2, port);
#endif /* 0 */
	printed += router_print_error(rip, rpp->rp_cb_errors, delta,
				      0x4, port);

	rpp->rp_excess_err = printed;

	return printed;
}


static int
router_check_errors(router_info_t *rip)
{
	int port;
	int printed = 0;


	if (gather_craylink_routerstats == 0)
		return 0;

	for (port = 1; port <= MAX_ROUTER_PORTS; port++)
		printed += router_check_port(rip, port);

	return printed;
}


/*
 * Set up a router port to prepare it to gather information.
 */
int
router_port_init(router_info_t *rip, int port)
{
	router_reg_t reg;
	int rc;

	/* Select the port utilization histogram */
	router_reg_read(rip, RR_PORT_PARMS(port), &reg);
	reg &= ~(RPPARM_HISTSEL_MASK);
	reg |= (RPPARM_HISTSEL_UTIL << RPPARM_HISTSEL_SHFT);
	if (hasmetarouter && port == 2 && rip->ri_module < 80) {
		reg = (reg & ~(RPPARM_MAXBURST_MASK)) | rpparm_maxburst_val;
	}

	rc = router_reg_write(rip, RR_PORT_PARMS(port), reg);

	/* Clear the histograms */
	router_reg_write(rip, RR_HISTOGRAM(port), 0);

	/* Clear out the errors */
	router_reg_read(rip, RR_ERROR_CLEAR(port), &reg);

	return rc;
}


extern cpu_cookie_t setnoderun(cnodeid_t);

extern void restorenoderun(cpu_cookie_t);

int
router_port_hist_reselect(router_info_t *rip, int port, __int64_t type)
{
	router_reg_t reg;
	int rc;
	cpu_cookie_t c;

	if (CNODE_TO_CPU_BASE(rip->ri_cnode) == CPU_NONE)
	    return EINVAL;

	c = setnoderun(rip->ri_cnode);
	/* Select the port utilization histogram */
	router_reg_read(rip, RR_PORT_PARMS(port), &reg);
	reg &= ~(RPPARM_HISTSEL_MASK);
	reg |= (type << RPPARM_HISTSEL_SHFT);

	rc = router_reg_write(rip, RR_PORT_PARMS(port), reg);

	/* Clear the histograms */
	router_reg_write(rip, RR_HISTOGRAM(port), 0);

	restorenoderun(c);

	return rc;
}


int 
router_hist_reselect(router_info_t *rip, __int64_t type)
{
	int i;
        cpu_cookie_t c;

	if (rip->ri_hist_type == type) 
	    return 0;

	for (i = 1; i <= MAX_ROUTER_PORTS; i++) {
		int rc;
		rc = router_port_hist_reselect(rip, i, type);
		if (rc)
			cmn_err(CE_WARN, "Router port histogram reselect failed, cnode %d, %v, p 0x%x, err %d <%s>", rip->ri_cnode, rip->ri_name, i, rc, net_errmsg(rc));
	}
	rip->ri_hist_type = type;

	/*
	 * If we want to sync up the router stats,
	 * capture the router info once here so that
	 * anyone doing a GET INFO immedeately after
	 * the SET HIST TYPE ioctl, will see a set
	 * of valid values.
	 */

        if (CNODE_TO_CPU_BASE(rip->ri_cnode) != CPU_NONE) {
        	c = setnoderun(rip->ri_cnode);
		router_acquire_info(rip) ;
        	restorenoderun(c);
	}

	return 0;
}

/*
 * router_set_fences
 *
 * 	Explicitly disable reset from ports that are down no in case
 *	someone plugs in or turns on another module.
 *	Step thro' all ports. If a port is not up, disable its resetok in
 *	rr_prot_cfg. If its up, disable all resetoks for its rr_reset_mask
 *	for ports which aren't up.
 */
void
router_set_fences(router_info_t *rip)
{
	int port, rst_port;
	router_reg_t protection_config;
	router_reg_t port_reset_mask;

	router_reg_read(rip, RR_PROT_CONF, &protection_config);

	for (port = 1; port <= MAX_ROUTER_PORTS; port++) {

		if (!(rip->ri_portmask & (1 << (port - 1)))) {
			protection_config &= ~(RPCONF_RESETOK(port));
			continue;
		}

		/* port is UP! */
		router_reg_read(rip, RR_RESET_MASK(port), &port_reset_mask);

		for (rst_port = 1; rst_port <= MAX_ROUTER_PORTS; rst_port++)
			if (!(rip->ri_portmask & (1 << (rst_port - 1))))
				port_reset_mask &= ~(RRM_RESETOK(rst_port));

		router_reg_write(rip, RR_RESET_MASK(port), port_reset_mask);
	}

	rip->ri_prot_conf = protection_config;

	router_reg_write(rip, RR_PROT_CONF, protection_config);
}


/*
 * How many clock ticks between router polls.
 */
volatile int router_poll_ticks = 2 * HZ;
volatile int router_print_usecs = 600 * USEC_PER_SEC;

/* 
 * This system thread gathers router information and stores it in the
 * router structure where we can get it later.
 */
void
capture_router_stats(router_info_t *rip)
{
	int error;

	error = router_acquire_info(rip);

	if (rip->ri_just_blink || error)
		rip->ri_just_blink ^= 1;

	/* See if we should print excessive error rates */
	if (rip->ri_print) {
		/* Keep an eye on the error rates. */
		if (router_check_errors(rip)) {
			rip->ri_last_print = absolute_rtc_current_time();
			rip->ri_print = 0;
		}
	} else {
		if ((absolute_rtc_current_time() -
		     rip->ri_last_print) > router_print_usecs)
			rip->ri_print = 1;
	}

	private.p_routertick = (error ? router_poll_ticks / 2 :
					router_poll_ticks);
}

lboard_t *
vertex_to_router_board_nasid(nasid_t nasid, vertex_hdl_t routerv)
{
	lboard_t *brd;
	char path[128];

	brd = find_lboard_class((lboard_t *)KL_CONFIG_INFO(nasid), KLTYPE_ROUTER);

	/* Try to match this vertex against the boards out there. */
	while (brd) {
		board_to_path(brd, path);

		if ((routerv == hwgraph_path_to_vertex(path)) &&
		    !(brd->brd_flags & DUPLICATE_BOARD))
			return brd;

		brd = find_lboard_class(KLCF_NEXT(brd), KLTYPE_ROUTER);
	}

	/* Couldn't find it. */
	return (lboard_t *)NULL;
}


lboard_t *
vertex_to_router_board(nasid_t nasid, vertex_hdl_t routerv)
{
	lboard_t *brd;
	cnodeid_t cnode;

	if (brd = vertex_to_router_board_nasid(nasid, routerv))
		return brd;

	/* If we can't find it on our node, check the rest! */
#if 0
	cmn_err(CE_NOTE, "Didn't see router %v on nasid %d, trying others.",
		routerv, nasid);
#endif
	for (cnode = 0; cnode < numnodes; cnode++) {
		if (brd = vertex_to_router_board_nasid(COMPACT_TO_NASID_NODEID(cnode), routerv)) {
#if 0
			cmn_err(CE_NOTE, "Found router %v on nasid %d.",
				routerv, COMPACT_TO_NASID_NODEID(cnode));
#endif
			return brd;
		}
	}

	return (lboard_t *)NULL;
}


/*
 * Create a router_info_t structure, set it up, initialize a router,
 * read its NIC, set up router monitoring, and add inventory info.
 */
int
router_init(cnodeid_t cnode,int writeid, nodepda_router_info_t *npda_rip)
{
	int rc;
	int i;
	__uint64_t age;
	router_info_t *rip,**ripp = &(npda_rip->router_infop);
	ulan_t ulan;
	char slotname[SLOTNUM_MAXLENGTH];
	net_vec_t vector = npda_rip->router_vector;
	vertex_hdl_t routerv = npda_rip->router_vhdl;
	
	/* 
	 * Justification behind this assert:
	 * 1.  	A router ALWAYS gets a heady (non-headless) node
	 * 	as guardian. Router guardian assignment code is
	 * 	meant to assure this. 
	 *	i.e. This node is guaranteed to have at least
	 *	one cpu enabled.
	 * 2.	router_init is done during io_init_node. 
	 *  	io_init_node does a setnoderun which ensures that
	 *	io initialization thread runs on a cpu on this
	 * 	node if any cpu is enabled on this node.	
	 * 3.	From 1 & 2 it is clear that router initialization
	 * 	thread is run on one of  the guardian node's enabled
	 *	cpus.
	 */
	ASSERT(cnode == cnodeid());

	rip = *ripp = (router_info_t *)kmem_zalloc_node(sizeof(router_info_t),
		VM_DIRECT|KM_NOSLEEP, cnode);
	ASSERT(rip);

	rc = hwgraph_info_add_LBL(routerv,INFO_LBL_ROUTER_INFO,
				  (arbitrary_info_t)rip);
	/* Router initialization must be done only once */
	ASSERT(rc == GRAPH_SUCCESS);

	/*
	 * Set up a mutex so we can do atomic updates of the whole
	 * structure.
	 */
	spinlock_init(&rip->ri_lock, "routerinfolock");
	rip->ri_ledcache = -1;	/* an impossible value so we write them */
	rip->ri_cnode = cnode;
	rip->ri_nasid = COMPACT_TO_NASID_NODEID(cnode);
	rip->ri_guardian = cnodeid_to_vertex(cnode);
	rip->ri_vector = vector;
	rip->ri_writeid = writeid;
	rip->ri_timebase = absolute_rtc_current_time();
	rip->ri_timestamp = rip->ri_timebase;
	rip->ri_vertex = routerv;
	rip->ri_per_minute = (long long)IP27_RTC_FREQ * 1000LL * 60LL;
	rip->ri_hist_type = RPPARM_HISTSEL_UTIL ; /* on reset */

	/*
	 * Get RR_STATUS_REV_ID so we know which ports are active and
	 * what to store when we set the LEDs.
	 */
	rc = router_reg_read(rip, RR_STATUS_REV_ID, &rip->ri_stat_rev_id);
	if (rc) {
		cmn_err(CE_ALERT, "Router init read failed: %v, "
			" cnode %d vec 0x%x err %d <%s>",
			routerv,cnode,vector, rc, net_errmsg(rc));
		return rc;
	}

	rc = router_reg_read(rip, RR_GLOBAL_PARMS, &rip->ri_glbl_parms);
	if (rc) {
		cmn_err(CE_ALERT, "Router init read failed: %v, err %d <%s>",
			routerv, rc, net_errmsg(rc));
		return rc;
	}

	age = (rip->ri_glbl_parms & RGPARM_AGEWRAP_MASK)>>RGPARM_AGEWRAP_SHFT;
	if (age != RGPARM_AGEWRAP_DEFAULT) {
		rip->ri_glbl_parms &= ~RGPARM_AGEWRAP_MASK;
		rip->ri_glbl_parms |= ((__uint64_t)RGPARM_AGEWRAP_DEFAULT << RGPARM_AGEWRAP_SHFT);
#ifdef DEBUG
		cmn_err(CE_WARN, "Router init: %v agewrap changed from %d to %d",
			routerv,  age,
			((rip->ri_glbl_parms & RGPARM_AGEWRAP_MASK) >>
			 RGPARM_AGEWRAP_SHFT));
#endif
	}


	/*
	 * The PROM boot with bypass disabled. If we are running with
	 * metarouters or if craylnk_bypass_disable is set, we leave it
	 * disabled. Otherwise, enable bypass. 
	 * Part of the spurious message workaround.
	 */
	if (craylnk_bypass_disable || hasmetarouter)
	    rip->ri_glbl_parms &= ~RGPARM_BYPEN_MASK;
	else
	    rip->ri_glbl_parms |=  RGPARM_BYPEN_MASK;


	/* set the router tail timeout wrap value to 0xafe so that it 
	 * is ~10ms instead of ~15ms.  Do this so that its timeout
	 * value is less then the timeout value of the hub tail timeout,
	 * because if the hub tailto goes off before the router, much
	 * worse things happen (hub tail timeout is ~15ms).
	 */
	rip->ri_glbl_parms &= ~(RGPARM_TTOWRAP_MASK);
	rip->ri_glbl_parms |= 0xafeL << RGPARM_TTOWRAP_SHFT;

	rc = router_reg_write(rip, RR_GLOBAL_PARMS, rip->ri_glbl_parms);

	npda_rip->router_port = (rip->ri_stat_rev_id & RSRI_INPORT_MASK)
					>> RSRI_INPORT_SHFT;
	if (((rip->ri_stat_rev_id & RSRI_CHIPID_MASK) >> RSRI_CHIPID_SHFT) !=
	    CHIPID_ROUTER) {
		/* We're not a router, return */
		cmn_err(CE_WARN, "Not a router, %v\n", routerv);
		*ripp = (router_info_t *)NULL;
		kmem_free(rip, sizeof(router_info_t));
	}

	rip->ri_slotnum = npda_rip->router_slot;
	rip->ri_module = npda_rip->router_module;

	/* Allocate space for vectors from each node to this router. */
	rip->ri_vecarray = (net_vec_t *)kmem_alloc_node(
		sizeof(net_vec_t) * numnodes,
                VM_DIRECT|KM_NOSLEEP, cnode);
	ASSERT_ALWAYS(rip->ri_vecarray);

	rip->ri_brd = vertex_to_router_board(rip->ri_nasid, routerv);

	rip->ri_name = kern_malloc_node(MAX_ROUTER_PATH, cnode);
	ASSERT_ALWAYS(rip->ri_name);
	strcpy(rip->ri_name, "/hw/");
	board_to_path(rip->ri_brd, rip->ri_name + strlen(rip->ri_name));

	if (!rip->ri_brd) {
		get_slotname(rip->ri_slotnum, slotname);
		cmn_err(CE_WARN, "Couldn't locate board structure for %v\n",
			routerv);
	} else {
		klinfo_t *component;
		klrouter_err_t *rr_error_info;
		component = component = KLCF_COMP(rip->ri_brd, 0);
		ASSERT(KLCF_COMP_TYPE(component) == KLSTRUCT_ROU);
		rr_error_info = ROU_COMP_ERROR(rip->ri_brd, component);
		for (i = 1; i <= MAX_ROUTER_PORTS; i++) {
			rr_error_info->re_status_error[i] = 0;
		}
	}

	if (SLOTNUM_GETCLASS(rip->ri_slotnum) != SLOTNUM_ROUTER_CLASS)
		cmn_err(CE_WARN | CE_CONFIGERROR,
			"Incorrectly jumpered router board in module %d",
			rip->ri_module);

	/* Set up router ports */
	rip->ri_portmask = 0;
	for (i = 1; i <= MAX_ROUTER_PORTS; i++) {
		int rc;
		if (RSRI_LINKWORKING(i) & rip->ri_stat_rev_id)
			rip->ri_portmask |=  1 << (i - 1);
		rc = router_port_init(rip, i);
		if (rc)
			cmn_err(CE_WARN, "Router port init failed, cnode %d, %v, p 0x%x, err %d <%s>", cnode, routerv, i, rc, net_errmsg(rc));
		rip->ri_port_maint[i-1] = 0;
		
	}

	/* Turn off resets from ports that are down right now. */
	router_set_fences(rip);

	/* Read the NIC here, attach info to routerv. */
        ulan.rip = rip;
        ulan.val = 0;
        nic_vertex_info_set(router_nic_access,
			    (nic_data_t)&ulan,
			    routerv);

	/* Prime the pump. */
	router_acquire_info(rip);

	/*
	 * Setting the version number also indicates that the structure
	 * is initialized
	 */
	rip->ri_version = ROUTER_INFO_VERSION;
	/* 
	 * @@: remove in production code, this would be stupid to print out.
	 */
#ifdef DEBUG
	print_router_monitoring(rip);
#endif
	/* Now that our router info is setup, make inventory information
	 */
	build_router_invent_info(rip);



	/* Attach our driver */
	sn0drv_attach(routerv);




#ifndef NO_ROUTER_THREADS
	private.p_routertick = router_poll_ticks;
#endif
	return rc;
}

/*
 * Copy the stored information into a router_info_t pointer.
 */
int
router_get_info(vertex_hdl_t routerv, router_info_t *destp, int maxlen)
{
	router_info_t *rip;
	int s;

	if (routerv == GRAPH_VERTEX_NONE)
		return ENODEV;

	(void)hwgraph_info_get_LBL(routerv, INFO_LBL_ROUTER_INFO,
					(arbitrary_info_t *)&rip);

	s = mutex_spinlock(&(rip->ri_lock));

	bcopy(rip, destp, MIN(sizeof(router_info_t), maxlen));

	mutex_spinunlock(&(rip->ri_lock), s);

	return 0;
}
/* Do a time out based poll on register until a particular
 * condition is met or the timeout has expired.
 * nasid   	- nasid of the hub on which the register resides
 * reg   	- register offset
 * valid_mask	- termination condition
 * max_polls	- max # of polls
 * poll_interval- microsecs between two consecutive polls
 *
 * Basically timeout period  = max_polls * poll_interval
 */
void
timeout_poll(nasid_t 	nasid,
	     __uint64_t reg,
	     __uint64_t valid_mask,
	     int 	max_polls,
	     __uint64_t poll_interval)
{
	int		poll;
	__uint64_t	status;

	for(poll = 0 ; poll < max_polls; poll++) {
		/* Delay "poll_interval" usecs */
		us_delay(poll_interval);
		/* Read the register and check for the
		 * valid condition.
		 */
		status = REMOTE_HUB_L(nasid,reg);
		if (status & valid_mask)
			break;
	}
}

#define VALID_STATUS_ERROR_MASK ((UINT64_CAST 1 << 34) - 1)
/* 
 * router_state_empty
 *	Check if there is at least one error bit set in the router state
 *	Very useful in masking unnecessary router error states.
 */
static int
router_state_empty(router_info_t *rip) 
{
	router_reg_t	reg = 0;
	int		empty = 1;	/* Boolean to decide whether
					 * we have a valid error state
					 * for this router.
					 */
	int		port;
	/*
	 * Check if any of the links failed after reset.
	 * If so we have a valid error state.
	 */
	router_reg_read(rip, RR_STATUS_REV_ID, &reg);
	for(port = 1; port <= MAX_ROUTER_PORTS; port++) {
		if (!((1 << (port - 1)) & rip->ri_portmask))
			continue;
		if (reg & RSRI_LINKRESETFAIL(port))
			empty = 0;
	}

	/*
	 * Check if any of the local status error bits are set.
	 * If so we have a valid error state.
	 */

	if (reg & RSRI_LOCAL_MASK )
		empty = 0;
	/* 
	 * Check if any of the ports have error bits set in their
	 * status registers.
	 * If so we have a valid error state.
	 */
	for (port = 1; port <= MAX_ROUTER_PORTS; port++) {
		if (!((1 << (port - 1)) & rip->ri_portmask))
			continue;
		reg = 0;
		router_reg_read(rip, RR_STATUS_ERROR(port), &reg);
		if (reg & VALID_STATUS_ERROR_MASK)
			empty = 0;
	}

	return(empty);
}

void
router_print_state_specific(router_info_t *rip, int level,
		   void (*pf)(int, char *, ...),int print_where)
{
	int port;
	router_reg_t reg1;
	char slotname[100];
	int s;
	nodepda_t	*npdap;


	ASSERT(rip);
	/* Acquire exclusive access to the router info */
	ROUTER_INFO_LOCK(rip,s);
	/* Check if this router info has already been printed
	 * by some other cpu 
	 */
	if (IS_ROUTER_INFO_PRINTED(rip,print_where)) {
		ROUTER_INFO_UNLOCK(rip,s);
		return;
	} else {
		/* Remember the fact that this router info has been 
		 * printed
		 */
		ROUTER_INFO_PRINT(rip,print_where);
		ROUTER_INFO_UNLOCK(rip,s);
	}
	/* Check if the node's vector unit is in use.
	 * In that case wait till the vector operation
	 * is completed.
	 */
	npdap = NODEPDA(rip->ri_cnode);
	if (npdap->vector_unit_busy) {

		pf(level,"vector unit busy for cnode %d.Waiting....\n",
		   rip->ri_cnode);
		/* Do time out poll on NI_VECTOR_STATUS checking the
		 * valid bit.
		 * The polling is done every '1' microsecond for
		 * a total of max(VEC_POLLS_R,VEC_POLLS_W)
		 * polls.
		 */
		timeout_poll(COMPACT_TO_NASID_NODEID(rip->ri_cnode),
			     NI_VECTOR_STATUS,
			     NVS_VALID,
			     max(VEC_POLLS_R,VEC_POLLS_W),
			     1);
		npdap->vector_unit_busy = 0;
	}

	if (!router_state_empty(rip)) {
		pf(level, "ROUTER ERROR STATE:\n");
		get_slotname(rip->ri_slotnum, slotname);
		pf(level, "/hw/module/%d/slot/%s\n", rip->ri_module, slotname);

		router_reg_read(rip, RR_STATUS_REV_ID, &reg1);
		show_rr_status_rev_id(reg1, rip->ri_portmask, level, pf);

#ifdef DEBUG
		router_reg_read(rip, RR_GLOBAL_PARMS, &reg1);
		pf(level, "   RR_GLOBAL_PARMS = 0x%x\n", reg1);
		router_reg_read(rip, RR_DIAG_PARMS, &reg1);
		pf(level, "   RR_DIAG_PARMS = 0x%x\n", reg1);
#endif
		for (port = 1; port <= MAX_ROUTER_PORTS; port++) {
			if (!((1 << (port - 1)) & rip->ri_portmask))
				continue;
			router_reg_read(rip, RR_STATUS_ERROR(port), &reg1);
			show_rr_error_status(reg1 & VALID_STATUS_ERROR_MASK, 
					     port, level, pf, 0);
#if defined(DEBUG)
			router_reg_read(rip, RR_PORT_PARMS(port), &reg1);
			pf(level, "   Port %d: RR_PORT_PARMS 0x%x\n", 
			   port, reg1);
#endif /* DEBUG */
		} /* for */
	} /* !router_state_empty() */

}
void
router_print_state(router_info_t *rip, int level,
		   void (*pf)(int, char *, ...),int print_where)
{
	nodepda_router_info_t 	*npda_rip;
	nodepda_t		*npda;

	if (rip)
		router_print_state_specific(rip,level,pf,print_where);
	else {
		/* Go through the list of dependent routers' info
		 * and print out the router state for each one
		 */
		npda = NODEPDA(cnodeid());
		npda_rip = npda->npda_rip_first;
		while(npda_rip) {
			rip = npda_rip->router_infop;
			if (!rip)
				/* We don't have a router at all */
				continue;
			rip->ri_cnode = cnodeid();
			rip->ri_nasid = COMPACT_TO_NASID_NODEID(rip->ri_cnode);
			rip->ri_writeid = 0; /* XXX - Won't work with 
					      * partitioning.
					      * Will need partition writeid 
					      */
			router_print_state_specific(rip,level,pf,print_where);
			npda_rip = npda_rip->router_next;

		}

	}
}
/*
 * This routine is useful to setup for reading
 * the router nic.
 */

static int 
router_nic_probe(nic_data_t data,
		 int pulse, int sample, int delay)
{
	__uint64_t		value;

	value = nic_get_phase_bits() | MCR_PACK(pulse, sample);

	vector_write((net_vec_t) data, 0, RR_NIC_ULAN, value);

	while (vector_read((net_vec_t) data, 0, RR_NIC_ULAN, &value) >= 0)
		if (MCR_DONE(value))
			break;

	DELAY(delay);

	return MCR_DATA(value);
}
/*
 * Read the router nic 
 */
int
router_nic_get(net_vec_t dest, __uint64_t *nic)
{
	nic_state_t		ns;
	int			r;


	if ((r = nic_setup(&ns,
			   router_nic_probe,
			   (nic_data_t)dest)) < 0)
		goto done;

	*nic = 0;				/* Clear two MS bytes */

	r = nic_next(&ns, (char *) nic + 2, (char *) 0, (char *) 0);

done:
	return r;
}

/* Given the board type of a router get the router name.
 * This is useful in debugging routines to indicate the
 * type of router we are looking at 
 */
void
get_routername(unsigned char router_type,char *name)
{
	switch(router_type) {
	case KLTYPE_ROUTER:
		strcpy(name,NORMAL_ROUTER_NAME);
		break;
	case KLTYPE_NULL_ROUTER:
		strcpy(name,NULL_ROUTER_NAME);
		break;
	case KLTYPE_META_ROUTER:
		strcpy(name,META_ROUTER_NAME);
		break;
	default:
		strcpy(name,UNKNOWN_ROUTER_NAME);
		break;
	}
}
