/**************************************************************************
 *									  *
 * 		 Copyright (C) 1990-1993, Silicon Graphics, Inc		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 *************************************************************************
 *
 * $Id: adp78.c,v 1.40 1999/08/19 03:07:45 gwh Exp $
 *
 * Driver for the Adaptec 7800 family of SCSI adapters (sometimes
 * refered to as controllers.)
 */

#include "sys/types.h"
#include "sys/param.h"
#include "sys/buf.h"
#include "sys/reg.h"
#include "sys/edt.h"
#include "sys/sbd.h"
#include "sys/sysmacros.h"
#include "sys/debug.h"
#include "sys/kmem.h"
#include "sys/immu.h"
#include "sys/ksynch.h"
#include "sys/pfdat.h"
#include "sys/var.h"
#include "sys/sema.h"
#include "sys/scsi.h"
#include "sys/invent.h"
#include "sys/time.h"
#include "sys/cmn_err.h"
#include "sys/errno.h"
#include "sys/kopt.h"
#include "sys/PCI/pciio.h"
#include "sys/PCI/PCI_defs.h"
#include "adphim/osm.h"
#include "adphim/him_scb.h"
#include "adphim/him_equ.h"
#include "sys/adp78.h"
#include "sys/giobus.h"
#include "sys/iograph.h"
#include <sys/ddi.h>		/* for drv_usecwait(), btop() */
#include <ksys/hwg.h>
#include "sys/failover.h"

#undef ALENLIST

#ifdef ALENLIST
#include "sys/alenlist.h"
#endif

extern char arg_wd93_syncenable[];
extern char arg_wd93hostid[];
extern int adp_num_sg[], adp_num_ha[];
extern uint16_t adp_noinq_map[];
extern u_char adp78_print_over_under_run;
extern u_char adp_option_termination[];
extern int scsi_intr_pri;

/*
 * PCI infrastructure interface functions.
 */
int adp78attach(vertex_hdl_t);
int adp78detach(vertex_hdl_t);
int adp78error(vertex_hdl_t, int, ioerror_mode_t, ioerror_t *);

extern int IP32_hinv_location(vertex_hdl_t);
extern int pcimh_flush_buffers(vertex_hdl_t);


/*
 * SCSI interface functions which this driver provides to higher layer
 * modules such as dksc and dsreq.  The functions are passed through
 * pointers.
 */
static void adp78command(scsi_request_t *req);
static int adp78alloc(vertex_hdl_t vhdl, int opt, void(*cb)());
static void adp78free(vertex_hdl_t, void (*cb)());
static struct scsi_target_info *adp78info(vertex_hdl_t);
static int adp78dump(vertex_hdl_t);
static int adp78abort(scsi_request_t *req_p);
static int adp78ioctl(vertex_hdl_t, unsigned int cmd, struct scsi_ha_op *op);

/*
 * Private functions
 */
static int init_adapter(adp78_adap_t *adp);
static int init_cfp_struct(adp78_adap_t *adp, cfp_struct *cfg);
static void init_cfp_struct_user(int adap, cfp_struct *cfg);
static void adjust_free_ha(adp78_adap_t *, int);
static void init_scb(sp_struct *scb_p, cfp_struct *cfg_p, adp78_ha_t *ha_p, char);

static void	adp_issue_cmds(adp78_adap_t *);
static void	adp_return_cmds(adp78_adap_t *);
static void	adp_timeout_cmds(adp78_adap_t *);
static void	adp_reset_cmds(adp78_adap_t *);
static int	adp_hinv(adp78_adap_t *ad_p, int all_ids);
static void	fill_scb(sp_struct *, scsi_request_t *, adp78_adap_t *);
static int	fill_sglist(adp78_ha_t *, scsi_request_t *);
#ifdef ALENLIST
static int alen_to_sg(adp78_ha_t *ha_p, alenlist_t alenlist, uint remain_bytes, uint view);
#else
static int adp_dma_map(adp78_ha_t *ha_p, caddr_t addr, uint remain_bytes, uint view);
static int adp_dma_mapbp(adp78_ha_t *ha_p, buf_t *bp, uint remain_bytes, uint view);
#ifdef ADP_STATS
static int adp_dma_maptile(adp78_ha_t *ha_p, caddr_t addr, uint remain_bytes, uint view);
#endif
#endif
static u_char *adp_inquiry(vertex_hdl_t, adp78_adap_t *ad_p, u_char targ, u_char lun, u_char do_abort);
static vertex_hdl_t adp78_ctlr_vhdl_get(adp78_adap_t *ad_p);
static void adp_inquiry_done(scsi_request_t *req_p);
static uint convert_hastatus(UBYTE hastatus);

static void		ha_enq_tail(adp78_ha_t **, adp78_ha_t *, u_char);
static adp78_ha_t	*ha_deq_head(adp78_ha_t **);
static adp78_ha_t	*ha_deq_ha(adp78_ha_t **, adp78_ha_t *);
static adp78_ha_t	*ha_deq_key(adp78_ha_t **, int (match_func(adp78_ha_t *, int)), int);

#ifdef qerr1
static void		ha_qerr_abort(adp78_adap_t *ad_p, int target);
#endif

static void		sr_enq_tail(adp78_adap_t *ad_p, scsi_request_t *req_p);
static int		sr_is_runable(adp78_adap_t *, scsi_request_t *);
static scsi_request_t * sr_deq_runable(adp78_adap_t *ad_p);
static void		sr_set_attn(adp78_adap_t *ad_p);
static void		sr_return_attn(adp78_adap_t *ad_p);

static int adp_poll_status(adp78_adap_t *);
#if defined(IP32)
static void adp78intr(eframe_t *, intr_arg_t);
#endif
void		adp_check_intstat(adp78_adap_t *);
void		adp_reset(adp78_adap_t *, int, int);
void		adp_reset_timer(adp78_adap_t *ad_p);
void		adp_timer(adp78_adap_t *ad_p);

/*
 * Statistics and performance functions.
 */
static void	adp_update_stats(adp78_adap_t *, scsi_request_t *);
int		adp_return_stats(int, char *);
int		adp_perftest(int, int, int, int, int);
int		adp_cmdlog(int, int, int, int, char *);

static void dump_cdb(u_char *cmd, int len);

extern int cachewrback;
extern int showconfig;

#ifdef ALENLIST
extern alenlist_t buf_to_alenlist(buf_t *);
#endif

int     adp78devflag = D_MP;

/*
 * adp78maxid is the
 * largest ctrl id found on the system.  The motherboard SCSI controllers
 * are 0 and 1, controller in PCI slot 0 is called 2, controller in PCI
 * slot 1 is called 3, etc.
 */
int adp78maxid=7;

adp78_adap_t **adp78_array=NULL;	/* array of pointers to adapters */

int adp_coredump=0;
int adp_poll=0;

#ifdef qerr1
static u_char adp_qerr_sense[] = {0x70, 0, 0xA, 0, 0, 0, 0, 0xA, 0, 0, 0, 0,
			  0x2F, 0, 0, 0, 0, 0};
static adp_qerr_senselen = sizeof(adp_qerr_sense) / sizeof(u_char);
#endif

int root_adapter=0;

#define IPLFUNC		splhintr

/*
 * configuration variables and arrays.  Tunable from master.d/adp78
 */
extern u_char adp78_printsense;
extern u_char adp_device_options[ADP78_NUMADAPTERS][16];
extern u_char adp_allow_cdrom_negotiations;
extern u_char adp_probe_all_luns[ADP78_NUMADAPTERS][16];
/*Disallow the turning off of parity checking from master.d/adp78*/
/*extern u_char adp_parity[ADP78_NUMADAPTERS];*/
extern u_char adp_adapter_id[ADP78_NUMADAPTERS];
extern u_char adp_master_latency[ADP78_NUMADAPTERS];
extern u_char adp_select_timeout[ADP78_NUMADAPTERS];
extern u_char adp78_mintimeout;
extern uint adp78reset_delay;

int adp_verbose=0;

#ifdef ADP_STATS
/* statistics counters */
uint adps_bigcnt=0;
uint adps_varcnt=0;
uint adps_cmd=0;
uint adps_cmd_queued=0;
uint adps_cmd_multiple=0;
uint adps_map1_chain=0;
uint adps_map2_chain=0;
uint adps_mapbp_chain=0;
#define ADP_STATS_INCR(a)	a++;
#else  /* ADP_STATS */
#define ADP_STATS_INCR(a)
#endif /* ADP_STATS */

#define SCSI_INFO_BYTE		7
#define SCSI_CTQ_BIT		2

#define SCSI_REQ_IS_TAGGED(a)	((a) & (SC_TAG_SIMPLE|SC_TAG_HEAD|SC_TAG_ORDERED))

/*
 ***************************************************************************
 * 
 *    Defines for mutex locks
 *
 ***************************************************************************
 */
#define ADP78PRI 0         /* Doesn't matter, since pri isn't used anymore */

#define INITMLOCK(l,n,s) init_mutex(l, MUTEX_DEFAULT, n, s)
#define MLOCK(l,s)     mutex_lock(&(l), ADP78PRI)
#define MTRYLOCK(l,s)     mutex_trylock(&(l))
#define MUNLOCK(l,s)   mutex_unlock(&(l))

/*
 *****************************************************************************
 *
 * Initialization functions
 *
 *****************************************************************************
 */

/*
 * Call the pci infrastructure to let it know we are here.
 */
#define ADAPTEC_VENDOR_ID	0x9004
#define ADAPTEC_DEVICE_ID1	0x8078
#define ADAPTEC_DEVICE_ID2	0x8178

/*ARGSUSED*/
void
adp78init(struct edt *e)
{
	/* driver's interested in either of these device ids */
	pciio_driver_register(ADAPTEC_VENDOR_ID,
				ADAPTEC_DEVICE_ID1,
				"adp78", 0);
	pciio_driver_register(ADAPTEC_VENDOR_ID,
				ADAPTEC_DEVICE_ID2,
				"adp78", 0);
}

/*
 * Return the scsi controller number based on the PCI slot number.
 * There is a one-to-one mapping between the PCI slot number and
 * the device number.
 */
get_adapter_number(int slot)
{
	/*
	 * On Moosehead and Roadrunner, the pci devices
	 * are numbered 0 for bridge, 1, 2, 3, and 4 for the Adaptec
	 * scsi controllers.
	 */
	return (slot-1);
}

/*
 * Do initialization functions.
 * Probe the SCSI buses to find out what devices are attached.
 *
 * Returns 0 if everything O.K., -1 otherwise.
 */
int
adp78attach(vertex_hdl_t vertex)
{
	adp78_adap_t *adp;
	int adap;
	int slot, num_ha;
	pciio_info_t info_p;
	device_desc_t adp78_dev_desc;
	vertex_hdl_t ctlr_vhdl;
	scsi_ctlr_info_t *ctlr_info;
	graph_error_t rv;
	char loc_str[LOC_STR_MAX_LEN];

	adp78_dev_desc = device_desc_dup(vertex);
	device_desc_intr_name_set(adp78_dev_desc, "Adaptec SCSI");
	device_desc_default_set(vertex, adp78_dev_desc);

	info_p = pciio_info_get(vertex);
	slot = pciio_info_slot_get(info_p);
	adap = get_adapter_number(slot);

	if (adp_verbose)
	       printf("adp78attach: slot: %d, device: 0x%x\n", slot,
			pciio_info_device_id_get(info_p));

	sprintf(loc_str,"%s/%d",EDGE_LBL_SCSI_CTLR,SCSI_EXT_CTLR(adap));
	rv = hwgraph_path_add(vertex, loc_str, &ctlr_vhdl);
	ASSERT(rv == GRAPH_SUCCESS); rv = rv;

        /* If we found vertex we need links to /hw/scsi_ctlr */
        if (vertex != GRAPH_VERTEX_NONE) {
                char src_name[10], edge_name[5];

                sprintf(loc_str, "%s/%s/%s/%d/%s/%d",EDGE_LBL_NODE,EDGE_LBL_IO,
                        EDGE_LBL_PCI,SCSI_EXT_CTLR(adap) + 1,EDGE_LBL_SCSI_CTLR,SCSI_EXT_CTLR(adap));
                sprintf(src_name,"%s",EDGE_LBL_SCSI_CTLR);
                sprintf(edge_name,"%d",SCSI_EXT_CTLR(adap));

                hwgraph_link_add(loc_str, src_name, edge_name);
	}


	ctlr_info = (scsi_ctlr_info_t *)hwgraph_fastinfo_get(ctlr_vhdl);
	if (!ctlr_info) {
		ctlr_info = scsi_ctlr_info_init();

		SCI_ADAP(ctlr_info)		= adap;
		SCI_CTLR_VHDL(ctlr_info)	= ctlr_vhdl;

		SCI_ALLOC(ctlr_info)	= adp78alloc;
		SCI_COMMAND(ctlr_info)	= adp78command;
		SCI_FREE(ctlr_info)	= adp78free;
		SCI_DUMP(ctlr_info)	= adp78dump;
		SCI_INQ(ctlr_info)	= adp78info;
		SCI_IOCTL(ctlr_info)	= adp78ioctl;
		SCI_ABORT(ctlr_info)	= adp78abort;
#if 0
		/* XXX put adp78_array[adap] info here?? */
		SCI_INFO(ctlr_info)         = ci;
#endif
		scsi_ctlr_info_put(ctlr_vhdl, ctlr_info);
		scsi_bus_create(ctlr_vhdl);
	}

	if (adp78_array == NULL) {
		adp78_array = kmem_zalloc(((adp78maxid+1) * sizeof(caddr_t)),
					  KM_SLEEP);
		ASSERT(adp78_array);
	}

	adp = kmem_zalloc(sizeof(adp78_adap_t), KM_SLEEP);
	ASSERT(adp);
	adp78_array[adap] = adp;
	adp->ad_ctlrid = adap;
	adp->ad_pcibusnum = 0;
	adp->ad_pcidevnum = slot;
	adp->ad_vertex = vertex;

	if (pciio_info_device_id_get(info_p) == 0x8178)
		adp->ad_flags |= ADF_OPTIONAL_PCI;

	/*
	 * Something may have already set the map value in the
	 * config. register.  The kernel may actually need to
	 * read the value out.
	 */
	adp->ad_baseaddr = (char *)pciio_piotrans_addr
		(vertex, 0,             /* device and (override) dev_info */
		 PCIIO_SPACE_WIN(1),    /* select configuration address space */
		 0,                     /* from the start of space, */
		 1024, 			/* byte count */
		 0);                    /* flag word */

	adp->ad_cfgaddr = (char *)pciio_piotrans_addr
		(vertex, 0,             /* device and (override) dev_info */
		PCIIO_SPACE_CFG,       /* select configuration address space */
		0,                     /* from the start of space, */
		PCI_CFG_VEND_SPECIFIC, /* .. up to the vendor specific stuff. */
		0);                    /* flag word */

	adp->ad_rev = pciio_config_get(adp->ad_vertex, PCI_CFG_REV_ID, 1);

	adp->ad_normsg_numsg = adp_num_sg[adp->ad_ctlrid];

	if (init_adapter(adp)) {
		cmn_err_tag(1, CE_ALERT, "SCSI controller %d initialization failed.",
			adp->ad_ctlrid);
		kmem_free(adp, sizeof(adp78_adap_t));
		adp78_array[adap] = NULL;
		return -1;
	}

	adp->ad_scsiid = adp->ad_himcfg->Cf_ScsiId;

	/*
	 * Probe for SCSI devices on this adapter.  adp_hinv will
	 * add them to hinv.  Now allocate the number of ha's based on the
	 * types of devices found on the bus or based on user's
	 * specification.
	 */
	num_ha = adp_hinv(adp, 0);
	if (adp_num_ha[adp->ad_ctlrid])
		num_ha = adp_num_ha[adp->ad_ctlrid];
	adjust_free_ha(adp, num_ha);

	if (showconfig) {
		printf("SCSI controller %d has scsi id %d\n", adp->ad_ctlrid,
		       adp->ad_scsiid);
		if (adp->ad_himcfg->CFP_EnableFast20)
			printf("Ultra-SCSI enabled, ");
		else
			printf("Ultra-SCSI disabled, ");
		printf("%d outstanding commands, %d entries per sg list.\n",
		       adp->ad_numha, adp->ad_normsg_numsg);
	}

	device_inventory_add(ctlr_vhdl, INV_DISK, INV_SCSICONTROL,
			     adp->ad_ctlrid, adp->ad_rev, INV_ADP7880);
	return 0;
}

/*ARGSUSED*/
int adp78detach(vertex_hdl_t vrtx)
{
#ifdef DEBUG
	printf("adp78detach: don't know how to detach\n");
#endif
	return -1;
}

/*ARGSUSED -- XXX hmm, could we do better with this?*/
int adp78error(vertex_hdl_t vrtx, int error_code, ioerror_mode_t mode, ioerror_t *ioerror)
{
#ifdef DEBUG
        printf("adp78pcierror: error_code: 0x%x\n", error_code);
	if (IOERROR_FIELDVALID(ioerror, busaddr))
		printf("adp78:ecf_pcierror: PCI bus address: 0x%x\n",
			IOERROR_GETVALUE(ioerror, busaddr));
#endif
	return 0;
}


/*
 * Initialze the adapter structure.  Allocate scb's, adp78_ha_t's, and the
 * HIM cfp_struct and hsp_structs.  Chain them all up appropriately.
 *
 * Returns 0 if all O.K. -1 on error.
 */
static int
init_adapter(adp78_adap_t *adp)
{
	int initerror;
	cfp_struct *cfgp;
	pciio_intr_t int_handle;        /* interrupt handle */
	device_desc_t dev_desc;
	int i;

	/*
	 * Allocate and initialize MP aware locks.
	 */
	adp->ad_adap_lk = LOCK_ALLOC(-1, IPLFUNC, (lkinfo_t *) NULL, 0);
	LOCK_INIT(adp->ad_adap_lk, -1, IPLFUNC, (lkinfo_t *) NULL);

	adp->ad_req_lk = LOCK_ALLOC(-1, IPLFUNC, (lkinfo_t *) NULL, 0);
	LOCK_INIT(adp->ad_req_lk, -1, IPLFUNC, (lkinfo_t *) NULL);

	adp->ad_haq_lk = LOCK_ALLOC(-1, IPLFUNC, (lkinfo_t *) NULL, 0);
	LOCK_INIT(adp->ad_haq_lk, -1, IPLFUNC, (lkinfo_t *) NULL);

	INITMLOCK(&adp->ad_him_lk, "adp78_him", adp->ad_vertex);

	INITMLOCK(&adp->ad_probe_lk, "adp78_probe", adp->ad_vertex);

	INITMLOCK(&adp->ad_abort_lk, "adp78_abort", adp->ad_vertex);

        for (i = 0; i < ADP78_MAXTARG; i++)
  	   INITMLOCK(&adp->ad_targ_open_lk[i], "adp78_targ_open", adp->ad_vertex);

	/*
	 * Allocate a large sg list buffer.  The large sg list must
	 * be able to hold the maximum sized DMA buffer.  (I have to
	 * add 1 to maxdmasz because of the way sr_is_runable calculates
	 * the size)
	 */
	adp->ad_lrgsg_numsg = maxdmasz + 1;
	adp->ad_lrgsg = kmem_alloc(adp->ad_lrgsg_numsg * SG_ELEMENT_SIZE,
				   KM_SLEEP|KM_PHYSCONTIG);

	/*
	 * Turn sequencer reset off, configure HIM data structures, etc..
	 * Setup interrupt and enable interrupts.  The order here is tricky.
	 * Lock out any interrupts before the structures are configured.
	 */
	dev_desc = device_desc_default_get(adp->ad_vertex);
	/* XXX ithread infrastructure will change this */
	device_desc_intr_swlevel_set(dev_desc, (ilvl_t)splhintr);

	int_handle = pciio_intr_alloc(adp->ad_vertex, dev_desc, 
				PCIIO_INTR_LINE_A, adp->ad_vertex);

	cfgp = kmem_zalloc(sizeof(cfp_struct), KM_SLEEP);

        MLOCK(adp->ad_him_lk, s);

	pciio_intr_connect(int_handle, (void(*)())adp78intr, 
				(intr_arg_t)adp->ad_ctlrid, NULL);

	PH_WriteHcntrl((void *) adp->ad_baseaddr, PAUSE);
	initerror = init_cfp_struct(adp, cfgp);

        MUNLOCK(adp->ad_him_lk, s);

	if (initerror) {
		kmem_free(cfgp, sizeof(cfp_struct));
		return (initerror);
	} else {
		adp->ad_himcfg = cfgp;
	}

	/*
	 * Reset the bus to clear any conditions from the prom
	 */
	adp_reset(adp, 1, 0);

	/*
	 * Just allocate minimum ha's initially.  They will be used for
	 * probing the bus to determine how many ha's we really need.
	 */
	adjust_free_ha(adp, ADP_MINSCBS);

	timeout_pri(adp_timer, adp, ADP_TIMER_VAL, scsi_intr_pri+1);

	adp->ad_stats = (adp_stats_t *)
		        kmem_zalloc(sizeof(adp_stats_t), KM_SLEEP);
	return 0;
}

/*
 * Initialize the cfp struct for the adaptec.  This structure is used by
 * the HIM.  Fill it in with some info. here, then call the HIM function
 * PH_GetConfig to do the rest.
 */
static int
init_cfp_struct(adp78_adap_t *adp, cfp_struct *cfgp)
{
	int adap, initerror;
	int val;
	hsp_struct *hspp;

	adap = adp->ad_ctlrid;

	ASSERT(adap < ADP78_NUMADAPTERS);

	/* initialize number of scbs before calling PH_GetConfig */
	cfgp->Cf_BusNumber = adp->ad_pcibusnum;
	cfgp->Cf_DeviceNumber = adp->ad_pcidevnum;
	cfgp->Cf_MaxNonTagScbs = 2;
	cfgp->Cf_NumberScbs = ADP78_NUMSCBS;
	cfgp->Cf_MaxTagScbs = ADP78_NUMSCBS;
	cfgp->CFP_SuppressNego = 0;
	cfgp->CFP_Base = (struct aic7870_reg *) adp->ad_baseaddr;
	cfgp->Cf_AdapterId = pciio_config_get(adp->ad_vertex,
		PCI_CFG_DEVICE_ID, 2);
	cfgp->CFP_InitNeeded = 1;
	cfgp->CFP_ResetBus = 1;
	cfgp->Cf_MaxTargets = ADP78_MAXTARG;
	cfgp->Cf_flags.flags_struct.ResvByte3 = 0xaa;
	cfgp->CFP_DriverIdle = 1;
	cfgp->CFP_BiosActive = 0;
	cfgp->Cf_AccessMode = 2;
	cfgp->CFP_MultiTaskLun = 1;
	cfgp->CFP_ScsiParity = 1;

	if (adp->ad_flags & ADF_OPTIONAL_PCI) {
		val = pciio_config_get(adp->ad_vertex, DEVCONFIG, 4);
		if (!(val & REXTVALID))
			cfgp->CFP_EnableFast20 = 0;
		else
			cfgp->CFP_EnableFast20 = 1;
	} else {
		cfgp->CFP_EnableFast20 = 1;
	}

	init_cfp_struct_user(adap, cfgp);

	hspp = kmem_zalloc(sizeof(hsp_struct), KM_SLEEP);
	cfgp->CFP_HaDataPtr = hspp;
	cfgp->Cf_HaDataPhy = kvtophys(hspp);

	initerror = PH_init_him(cfgp);

	return initerror;
}

static int
prom_atoi(char *str)
{
	int i, base, val=0;

	if ((str[0] == '0' && str[1] == 'x') ||
	    (str[0] == '0' && str[1] == 'X')) {
		base = 16;
		i = 2;
	} else if (str[0] == '0') {
		base = 8;
		i = 1;
	} else {
		base = 10;
		i = 0;
	}
	while (str[i] != 0) {
		if (str[i] <= '9' && str[i] >= '0')
			val = (val * base) +
				str[i] - '0';
		else if (str[i] <= 'F' && str[i] >= 'A')
			val = (val * base) +
				str[i] - 'A' + 10;
		else if (str[i] <= 'f' && str[i] >= 'a')
			val = (val * base) +
				str[i] - 'a' + 10;
		i++;
	}

	return val;
}

#define PROM_HAID_SET		0x1
#define PROM_SYNCEN_SET		0x2
/*
 * If the scsihostid and/or scsi_syncenable is set, set it here
 * and find out what the prom adapter is.  If they are not set
 * there is no use in finding out the prom adapter, so set it 
 * to -1 (so there is no possibility of matching.)
 */
static int
get_prom_settings(int *prom_adap, int *prom_haid, int *prom_sync)
{
	char *hostidstr = arg_wd93hostid;
	char *syncenstr = arg_wd93_syncenable;
	int rval = 0;

	if ((hostidstr[0] == 0) && (syncenstr[0] == 0)) {
		*prom_adap = -1;
		return rval;
	}

	/* set prom_haid and prom_sync, and find out what the
	 * boot adapter was.  root has precedence over system partition
	 */
	if (hostidstr[0] != 0) {
		*prom_haid = (int) atoi(hostidstr);
		rval |= PROM_HAID_SET;
	}

	if (syncenstr[0] != 0) {
		*prom_sync = prom_atoi(syncenstr);
		rval |= PROM_SYNCEN_SET;
	}

	if (arg_root[0] != 0) {
		if (strncmp(arg_root, "dks", 3)) {
#ifdef DEBUG
			printf("SCSI badly formatted kernel variable root=%s.  Defaulting root controller to 0.\n",
				arg_root);
#endif
			*prom_adap = 0;
		} else {
			*prom_adap = arg_root[3] - '0';
			if (arg_root[4] <= '9' && arg_root[4] >= '0')
				*prom_adap = (*prom_adap * 10) +
					     arg_root[4] - '0';
		}
	} else {
		if (strncmp(arg_syspart, "scsi", 4)) {
#ifdef DEBUG
			printf("SCSI badly formatted kernel variable SystemPartition=%s.  Defaulting root controller to 0.\n",
				arg_syspart);
#endif
			*prom_adap = 0;
		} else {
			*prom_adap = arg_syspart[5] - '0';
			if (arg_syspart[6] <= '9' && arg_syspart[6] >= '0')
				*prom_adap = (*prom_adap * 10) +
					     arg_syspart[6] - '0';
		}
	}

	return rval;
}

/*
 * Initialize the part of the cfp struct that is user configurable.
 */
static void
init_cfp_struct_user(int adap, cfp_struct *cfgp)
{
	int prom_adap, prom_haid, prom_syncenable, prom_setflags;
	int targ;
	unsigned short disconn_map=0;
        u_char dev_options;
	adp78_adap_t *adp;

	/*
	 * See which adapter was used as the boot adapter and use
	 * the hostid and syncenable environment variable settings,
	 * if any.  Use the configuration info from master.d/adp78 
	 * for the others.  Warn about mis-match between prom setting
	 * and master.d settings.
	 */
	prom_setflags = get_prom_settings(&prom_adap, &prom_haid,
					  &prom_syncenable);

	if (prom_adap != -1) {
		root_adapter = prom_adap;
		if (adp_verbose)
			printf("setting root adapter to %d\n", root_adapter);
	}

	if((adap == prom_adap) && (prom_setflags & PROM_HAID_SET)) {
		cfgp->Cf_ScsiId = prom_haid;
	} else {
		cfgp->Cf_ScsiId = adp_adapter_id[adap];
	}

	adp = adp78_array[adap];
	cfgp->Cf_Adapter = adp;
	cfgp->Cf_ConnVhdl = adp78_array[adap]->ad_vertex;
	cfgp->CFP_ScsiTimeOut = adp_select_timeout[adap];

	for(targ = 0; targ < cfgp->Cf_MaxTargets; targ++) {
		/*
		 * Deal with synchronous transfer stuff.
		 */

  	        cfgp->Cf_ScsiOption[targ].byte =
		  adp_device_options[adap][targ] & ADP_SYNC_RATE;

		if ((adap == prom_adap) && (prom_setflags & PROM_SYNCEN_SET)) {
			if (prom_syncenable & (1 << targ)) {
				cfgp->Cf_ScsiOption[targ].u.SyncMode = 1;
			} else {
				cfgp->Cf_ScsiOption[targ].u.SyncMode = 0;
			}
		}

		/*
		 * Deal with wide transfer stuff.
		 */
                adp->ad_negotiate[targ].all_params = 0;

                dev_options = adp_device_options[adap][targ];
                if (dev_options & (ADP_SYNC_RATE | ADP_ALLOW_SYNC)) /* sync enabled */
		  adp->ad_negotiate[targ].params.allow_sync = 1;

                if (dev_options & ADP_ALLOW_WIDE) /* wide enabled */
		  adp->ad_negotiate[targ].params.allow_wide = 1;
		
                if (dev_options & ADP_FORCE_SYNC) /* force sync nego */
		  adp->ad_negotiate[targ].params.force_sync = 1;

                if (dev_options & ADP_FORCE_WIDE) /* force wide nego */
		  adp->ad_negotiate[targ].params.force_wide = 1;

		/*
		 * Deal with connect/disconnect stuff.
		 */
		if (adp_device_options[adap][targ] & ADP_ALLOW_DISCONNECT)
			disconn_map |= 1 << targ;
	}

	cfgp->CFP_AllowDscnt = disconn_map;

	if (adp_option_termination[adap] & 1)
		cfgp->CFP_TerminationLow = 1;
	if (adp_option_termination[adap] & 2)
		cfgp->CFP_TerminationHigh = 1;
}


/*
 * Adjust the number of free ha's to the given number.
 * XXX for now, it just knows how to add more ha, not remove excess ha.
 *
 */
static void
adjust_free_ha(adp78_adap_t *ad_p, int num)
{
	int i, scb_num;
	adp78_ha_t *ha_p;
	sp_struct *scb_p;
	cfp_struct *cfg_p;

	if (num > ADP78_NUMSCBS)
		num = ADP78_NUMSCBS;

	/* XXX For now, can't reduce number of ha's */
	if (ad_p->ad_numha >= num)
		return;

	cfg_p = ad_p->ad_himcfg;

	scb_num = ad_p->ad_numha;
	i = ad_p->ad_numha + 1;
	while (i <= num) {
		ha_p = kmem_zalloc(sizeof(adp78_ha_t), 0);
		scb_p = kmem_zalloc(sizeof(sp_struct), KM_PHYSCONTIG|KM_CACHEALIGN);

		ha_p->ha_id = ad_p->ad_ctlrid;
		ha_p->ha_scb = scb_p;
		ha_p->ha_numsg = ad_p->ad_normsg_numsg;
		ha_p->ha_sg = kmem_zalloc(ad_p->ad_normsg_numsg * SG_ELEMENT_SIZE,
					 KM_PHYSCONTIG|KM_CACHEALIGN);

		init_scb(scb_p, cfg_p, ha_p, scb_num);
		i++;
		scb_num++;

		ad_p->ad_haq_ospl = LOCK(ad_p->ad_haq_lk, IPLFUNC);
		ha_enq_tail(&(ad_p->ad_freeha), ha_p, HAQ_FREE);
		UNLOCK(ad_p->ad_haq_lk, ad_p->ad_haq_ospl);
	}

	ad_p->ad_numha = num;
}

static void
init_scb(sp_struct *scb_p, cfp_struct *cfg_p, adp78_ha_t *ha_p, char scb_num)
{
	scb_p->SP_ConfigPtr = cfg_p;
	scb_p->Sp_OSspecific = ha_p;
	scb_p->SP_Cmd = EXEC_SCB;
	scb_p->Sp_SenseLen = 0;
	scb_p->SP_AutoSense = 1;	/* Auto Request Sense Enable */
	scb_p->Sp_scb_num = scb_num;
	scb_p->Sp_ctrl = ha_p->ha_id;
	scb_p->SP_CDBPtr = kvtophys(scb_p->Sp_CDB) & ~PCI_NATIVE_VIEW;

#ifdef IP32
	scb_p->SP_SegPtr = kvtophys(ha_p->ha_sg) | PCI_NATIVE_VIEW;
	scb_p->Sp_Sensesgp = kvtophys(&(scb_p->Sp_SensePtr)) | PCI_NATIVE_VIEW;
#else
	scb_p->SP_SegPtr = kvtophys(ha_p->ha_sg);
	scb_p->Sp_Sensesgp = kvtophys(&(scb_p->Sp_SensePtr));
#endif
}

/*
 *****************************************************************************
 *
 * Generic SCSI functions/entry points
 *
 *****************************************************************************
 */

static void
adp78command(scsi_request_t *req_p)
{
	adp78_adap_t *ad_p;
	int reject_cmds = 0;

	ad_p = adp78_array[req_p->sr_ctlr];
	if (ad_p == NULL) {
		req_p->sr_status = SC_REQUEST;
		(*req_p->sr_notify)(req_p);
		return;
	}

	/*
	 * If we have done a reset and the req does not have AEN_ACK set,
	 * mark it for return to sender with SC_ATTN.
	 * Just queue the scsi request in the list of requests and bump
	 * the adp_issue_cmds loop to send out any and all ready cmds.
	 */
	if (ad_p->ad_flags & ADF_RESET) {
		if (req_p->sr_flags & SRF_AEN_ACK) {

			ad_p->ad_adap_ospl = LOCK(ad_p->ad_adap_lk, IPLFUNC);
			ad_p->ad_flags &= ~ADF_RESET;
			UNLOCK(ad_p->ad_adap_lk, ad_p->ad_adap_ospl);

			req_p->sr_status = 0;
		} else {
			req_p->sr_status = SC_ATTN;
			req_p->sr_scsi_status = 0;
			req_p->sr_ha_flags = 0;
			req_p->sr_sensegotten = 0;
			req_p->sr_resid = req_p->sr_buflen;

			reject_cmds = 1;
		}
	} else {
		req_p->sr_status = 0;
	}

	req_p->sr_status = SC_GOOD;
	req_p->sr_ha = 0;
	req_p->sr_spare = 0;

	ad_p->ad_req_ospl = LOCK(ad_p->ad_req_lk, IPLFUNC);
	sr_enq_tail(ad_p, req_p);
	UNLOCK(ad_p->ad_req_lk, ad_p->ad_req_ospl);

	adp_issue_cmds(ad_p);

	if (reject_cmds) {
		ad_p->ad_req_ospl = LOCK(ad_p->ad_req_lk, IPLFUNC);
		sr_return_attn(ad_p);
		UNLOCK(ad_p->ad_req_lk, ad_p->ad_req_ospl);
	}

	/*
	 * If this is a core dump, wait for scsi commands to complete
	 * before returning.
	 */
	if (adp_coredump || adp_poll)
		adp_poll_status(ad_p);
}

/*ARGSUSED3 -- XXX we should use cb? */
static int
adp78alloc(vertex_hdl_t lun_vhdl, int opt, void(*cb)())
{
	adp78_adap_t *ad_p;
	u_char ctlr, targ, lun;
	scsi_ctlr_info_t *ctlr_info;
	scsi_lun_info_t *lun_info;
	int status;
	
	lun_info = scsi_lun_info_get(lun_vhdl);
	ctlr_info = scsi_ctlr_info_get(SLI_CTLR_VHDL(lun_info));
	targ = SLI_TARG(lun_info);
	lun = SLI_LUN(lun_info);
	ctlr = SCI_ADAP(ctlr_info);

	if (ctlr > adp78maxid || targ >= ADP78_MAXTARG || lun >= ADP78_MAXLUN)
		return ENODEV;

	ad_p = adp78_array[ctlr];
	if (ad_p == NULL)
		return ENODEV;
	if (targ == ad_p->ad_scsiid)
		return ENODEV;

	/*
	 * Exclusive locks increment ad_alloc by 255.
	 * All other locks increment ad_alloc by 1 up to 254.
	 */
	ad_p->ad_adap_ospl = LOCK(ad_p->ad_adap_lk, IPLFUNC);
	if (opt & SCSIALLOC_EXCLUSIVE) {
		if (ad_p->ad_alloc[targ][lun] == 0) {
			status = SCSIALLOCOK;
			ad_p->ad_alloc[targ][lun] = 255;
		} else {
			status = EBUSY;
			goto _allocout;
		}
	
	} else {
		if (ad_p->ad_alloc[targ][lun] < 254) {
			status = SCSIALLOCOK;
			ad_p->ad_alloc[targ][lun] += 1;
		} else {
			status = EBUSY;
			goto _allocout;
		}
	}
	if (cb) {
		if (ad_p->adp_sense_callback[targ][lun] &&
		    ad_p->adp_sense_callback[targ][lun] != cb) {
			status = EINVAL;
			goto _allocout;
		} else
		    	ad_p->adp_sense_callback[targ][lun] = cb;
	}

_allocout:
	UNLOCK(ad_p->ad_adap_lk, ad_p->ad_adap_ospl);
	return status;
}

/*ARGSUSED2 -- XXX we should use cb? */
static void
adp78free(vertex_hdl_t lun_vhdl, void (*cb)())
{
	u_char ctlr, targ, lun;
	scsi_lun_info_t *lun_info;
	adp78_adap_t *ad_p;
	scsi_ctlr_info_t *ctlr_info;

	lun_info = scsi_lun_info_get(lun_vhdl);
	ctlr_info = scsi_ctlr_info_get(SLI_CTLR_VHDL(lun_info));
	targ = SLI_TARG(lun_info);
	lun = SLI_LUN(lun_info);
	ctlr = SCI_ADAP(ctlr_info);

	if (ctlr > adp78maxid || targ >= ADP78_MAXTARG || lun >= ADP78_MAXLUN)
		return;

	ad_p = adp78_array[ctlr];
	if (ad_p == NULL)
		return;

	/*
	 * If ad_alloc is 255, then it was exclusively alloc'd.  Set to 0.
	 * If non-zero but not 255, then it was shared alloc'd.  Decrement.
	 */
	ad_p->ad_adap_ospl = LOCK(ad_p->ad_adap_lk, IPLFUNC);
	if (ad_p->ad_alloc[targ][lun] == 255)
		ad_p->ad_alloc[targ][lun] = 0;
	else if (ad_p->ad_alloc[targ][lun] > 0)
		ad_p->ad_alloc[targ][lun] -= 1;

	if (cb)
	  if (ad_p->adp_sense_callback[targ][lun] == cb)
	    ad_p->adp_sense_callback[targ][lun] = NULL;
	  else
	    cmn_err(CE_ALERT, "adp78free: callback 0x%x not active, vertex = %d, target = %d, lun = %d\n",
		    cb, lun_vhdl, targ, lun); 
	    
	UNLOCK(ad_p->ad_adap_lk, ad_p->ad_adap_ospl);
}


static struct scsi_target_info *
adp78info(vertex_hdl_t lun_vhdl)
{
	scsi_target_info_t *info_p;
	u_char ctlr, targ, lun;
	scsi_lun_info_t *lun_info;
	scsi_ctlr_info_t *ctlr_info;
	adp78_adap_t *ad_p;
	AIC_7870 *base;
	UBYTE xfer_option;

	lun_info = scsi_lun_info_get(lun_vhdl);
	ctlr_info = scsi_ctlr_info_get(SLI_CTLR_VHDL(lun_info));
	targ = SLI_TARG(lun_info);
	lun = SLI_LUN(lun_info);
	ctlr = SCI_ADAP(ctlr_info);

	if (adp_verbose)
		printf("adp78info: adapter %d targ %d\n", ctlr, targ);

	if (ctlr > adp78maxid || targ >= ADP78_MAXTARG || lun >= ADP78_MAXLUN)
		return NULL;

	/* no adapter at adap */
	ad_p = adp78_array[ctlr];
	if (ad_p == NULL)
		return NULL;

	if (adp_inquiry(lun_vhdl, ad_p, targ, lun, 0) != NULL) {
		u_short disconn_map;

		info_p = &ad_p->ad_info[targ][lun];
		info_p->si_ha_status = 0;

		disconn_map = 1 << targ;
		if (ad_p->ad_himcfg->CFP_AllowDscnt & disconn_map)
			info_p->si_ha_status |= SRH_DISC;
		
		if (ad_p->ad_himcfg->Cf_ScsiOption[targ].u.SyncMode)
			info_p->si_ha_status |= SRH_TRIEDSYNC;

		base = ad_p->ad_himcfg->CFP_Base;
		xfer_option = OSM_CONV_BYTEPTR(XFER_OPTION) + targ;
		xfer_option = OSM_CONV_BYTEPTR(xfer_option);
		if (INBYTE(AIC7870[xfer_option]) & SYNC_RATE)
			info_p->si_ha_status |= SRH_SYNCXFR;

		if (info_p->si_inq[SCSI_INFO_BYTE] & SCSI_CTQ_BIT) {
			info_p->si_ha_status |= (SRH_TAGQ | SRH_QERR0);
			info_p->si_maxq = ad_p->ad_numha;
			info_p->si_qdepth = 0;
			info_p->si_qlimit = 0;
		}

		if (adp_verbose)
			printf("ha_status = 0x%x\n", info_p->si_ha_status);

	} else {
		info_p = NULL;
	}


	return info_p;
}


/*
 * Send an ABORT message to the device.  (See MTABORT in tpsc.c)
 * XXX Just put a stub here for now.  This function is not implemented.
 * XXX BUG #430775
 */
/*ARGSUSED*/
static int
adp78abort(scsi_request_t *req_p)
{
  adp78_adap_t *ad_p;
  vertex_hdl_t ctlr_vhdl;
  u_char targ;
  u_char lun;
  vertex_hdl_t lun_vhdl;
  int retval = 1;

  ad_p = adp78_array[req_p->sr_ctlr];
  if (ad_p == NULL) {
    req_p->sr_status = SC_REQUEST;
    (*req_p->sr_notify)(req_p);
    return 0;
  }
  MLOCK(ad_p->ad_abort_lk, s);

  ctlr_vhdl = adp78_ctlr_vhdl_get(ad_p);
  targ = req_p->sr_target;
  lun = req_p->sr_lun;
  
  if ((lun_vhdl = scsi_lun_vhdl_get(ctlr_vhdl, targ, lun)) == GRAPH_VERTEX_NONE)
    {
      printf("lun_vhdl doesn't exist in abort sub; lun_vhdl = %x, ctrl_vhdl = %x, targ = %d, lun = %d\n", lun_vhdl, ctlr_vhdl, targ, lun);
      retval = 0;
    }
  else
    {
      ad_p->ad_negotiate[targ].params.do_abort = 1;
      adp_inquiry(lun_vhdl, ad_p, targ, lun, 1);
    }
  MUNLOCK(ad_p->ad_abort_lk, s);
  return retval;

}


/*
 * Kernel tells us to go into core dump mode.  Turn off interrupts, we'll
 * single thread all commands now and poll for completion.
 */
static int
adp78dump(vertex_hdl_t ctlr_vhdl)
{
	scsi_ctlr_info_t *ctlr_info;
	adp78_adap_t *ad_p;

	ctlr_info = scsi_ctlr_info_get(ctlr_vhdl);
	ad_p = adp78_array[SCI_ADAP(ctlr_info)];

	ASSERT(ad_p);

	adp_coredump = 1;

        MLOCK(ad_p->ad_him_lk, s);
	PH_WriteHcntrl(ad_p->ad_himcfg->CFP_Base, (UBYTE) 0);
        MUNLOCK(ad_p->ad_him_lk, s);

#if defined(IP32)
	setcrimevector(MACE_INTR(ad_p->ad_ctlrid + 8), SPLHINTR, NULL, 0, 0);
#endif
	return 1;
}


int adp7895test_present=0;
void (*adp7895testfunc)();

/*
 * SCSI controller (scsiha) entry point
 */
static int
adp78ioctl(vertex_hdl_t ctlr_vhdl, unsigned int cmd, struct scsi_ha_op *op)
{
	int rval;
	int start;
	int adap;
	scsi_ctlr_info_t *ctlr_info;

	ctlr_info = (scsi_ctlr_info_t *)hwgraph_fastinfo_get(ctlr_vhdl);
	adap = SCI_ADAP(ctlr_info);

	switch(cmd) {
	case SOP_ADP_GETSTATS:
		rval = adp_return_stats(adap, (char *) op->sb_addr);
		break;

	case SOP_ADP_RESETBUS:
	case SOP_RESET:
		adp_reset(adp78_array[adap], 1, 1);
		rval = 0;
		break;

	case SOP_GET_SCSI_PARMS:
	{
	        struct scsi_parms sp;
		int               targ;
		adp78_adap_t      *ad_p;

		bzero(&sp, sizeof(struct scsi_parms));
		ad_p = adp78_array[adap];
		sp.sp_scsi_host_id = (u_short)ad_p->ad_scsiid;
		sp.sp_selection_timeout = 256000;
		
		for (targ = 0; targ < ADP78_MAXTARG; ++targ) 
		{
			if (targ == ad_p->ad_scsiid)
				continue;
			if (scsi_lun_vhdl_get(ctlr_vhdl, targ, 0) == GRAPH_VERTEX_NONE)
				continue;
			sp.sp_target_parms[targ].stp_is_present = 1;
			if (ad_p->ad_negotiate[targ].params.offset != 0) {
			  sp.sp_target_parms[targ].stp_is_sync = 1;
			  sp.sp_target_parms[targ].stp_sync_offset =
			    ad_p->ad_negotiate[targ].params.offset;
			  sp.sp_target_parms[targ].stp_sync_period =
			    ad_p->ad_negotiate[targ].params.period;
			}
			sp.sp_target_parms[targ].stp_is_wide =
			  ad_p->ad_negotiate[targ].params.width;

		}
		copyout(&sp, (void *)op->sb_addr, sizeof(struct scsi_parms));
		rval = 0;
		break;
	}
	case SOP_SCAN:
	  {
 	        int num_ha=0;
		adp78_adap_t      *ad_p;

		ad_p = adp78_array[adap];
		MLOCK(ad_p->ad_probe_lk, s);
		num_ha = adp_hinv(ad_p, 1);
		if (adp_num_ha[ad_p->ad_ctlrid])
		  num_ha = adp_num_ha[ad_p->ad_ctlrid];
		ad_p->ad_adap_ospl = LOCK(ad_p->ad_adap_lk, IPLFUNC);
		adjust_free_ha(ad_p, num_ha);
        	UNLOCK(ad_p->ad_adap_lk, ad_p->ad_adap_ospl);
		rval = 0;
        	MUNLOCK(ad_p->ad_probe_lk, s);
		break;
	  }
	case SOP_ADP_PERFTEST:
#ifdef debug
		printf("SOP_ADP_PERFTEST: adap %d ");
		printf("startblock 0x%x ", op->sb_opt);
		printf("iosize %d ", op->sb_arg);
		printf("numdisks %d\n", op->sb_addr);
#endif
		/*
		 * The size of the file (per disk) is hardcoded for now
		 * to 409600 blocks (2048KB)
		 */
		rval = adp_perftest(adap, op->sb_opt, 409600, op->sb_arg,
					 (int) op->sb_addr);
		break;

	case SOP_ADP_CMDLOG_START:
	case SOP_ADP_CMDLOG_STOP:
		if (cmd == SOP_ADP_CMDLOG_START)
			start = 1;
		else
			start = 0;
		rval = adp_cmdlog(start, adap, op->sb_opt, op->sb_arg,
				    (char *) op->sb_addr);
		break;

	case SOP_ADP_7895TEST:
		if (adp7895test_present) {
			(*adp7895testfunc)();
			rval = 0;
		} else {
			rval = -1;
		}
		break;

	default:
		rval = -1;
	}

	return rval;
}


/*
 *****************************************************************************
 *
 * Private functions
 *
 *****************************************************************************
 */


/*
 * Check for free ha's and waiting scsi requests on idle targets and
 * issue those commands.
 * This may be called from adp78command or adp78intr.
 */
void
adp_issue_cmds(adp78_adap_t *ad_p)
{
	extern int ip32_pci_enable_prefetch, ip32_pci_flush_prefetch;
	scsi_request_t *req_p;
	adp78_ha_t *ha_p;

	/*
	 * Don't send out new commands while reset is in progress.
	 */
	if (ad_p->ad_flags & ADF_RESET_IN_PROG)
		return;

	/*
	 * Clear the list of targs with requests who's sg list is large and
	 * are waiting for memory to hold the sg list (ad_memwaitmap).
	 * Clear the list of targs which has a non-tagged cmd waiting
	 * for tagged cmds to finish before executing (ad_tagholdmap).
	 * Both of these lists can start fresh everytime we come into
	 * this function and they are updated as a side affect of
	 * calling sr_is_runable().
	 */
	ad_p->ad_adap_ospl = LOCK(ad_p->ad_adap_lk, IPLFUNC);
	ad_p->ad_memwaitmap = 0;
	ad_p->ad_tagholdmap = 0;

	/*
	 * First get a ha.  ha's are easier to put back into the
	 * list then scsi reqs.  Once we get a scsi req, we are
	 * committed to sending it out.
	 */
	ad_p->ad_haq_ospl = LOCK(ad_p->ad_haq_lk, IPLFUNC);
	ha_p = ha_deq_head(&(ad_p->ad_freeha));
	UNLOCK(ad_p->ad_haq_lk, ad_p->ad_haq_ospl);

	while (ha_p) {

		/* Find a runable scsi request.  If there are none, put the
		 * ha back on the list.  We're done.
		 */
		ad_p->ad_req_ospl = LOCK(ad_p->ad_req_lk, IPLFUNC);
		req_p = sr_deq_runable(ad_p);
		UNLOCK(ad_p->ad_req_lk, ad_p->ad_req_ospl);

		if (req_p == NULL) {

			ad_p->ad_haq_ospl = LOCK(ad_p->ad_haq_lk, IPLFUNC);
			ha_enq_tail(&(ad_p->ad_freeha), ha_p, HAQ_FREE);
			UNLOCK(ad_p->ad_haq_lk, ad_p->ad_haq_ospl);

			break;
		}

		/*
		 * If this req needs a large sg list, its passed back in sr_spare.
		 * Save the large ha for use by adp_dma_map and adp_dma_mapbp
		 */
		if (req_p->sr_spare) {
			ha_p->ha_lrgsg = req_p->sr_spare;
			ha_p->ha_lrgsg_numsg = ad_p->ad_lrgsg_numsg;
			req_p->sr_spare = 0;
		}

		ha_p->ha_req = req_p;
		req_p->sr_ha = ha_p;

		/*
		 * If sr_status is good, fill in the scb and send it.
		 * If sr_status is not good, put the ha on the done queue
		 * for return.
		 */
		if (req_p->sr_status == SC_GOOD) {
		    int do_negotiate = 0;
		    u_char targ;
		    AIC_7870 *base;
		    cfp_struct *cfg_p;

		    cfg_p = ad_p->ad_himcfg;

 		    targ = req_p->sr_target;
		        if ((req_p->sr_command[0] == INQUIRY_CMD) &&
			    (ad_p->ad_negotiate[targ].params.first_inq_done)) {
			  MLOCK(ad_p->ad_him_lk, s);

			  /* if this is inquiry that is marked to send the
			   * abort message, fill in the appropriate fields.
			   * mark the target for a negotiation. Then send
			   * abort message instead of ext msgs. When bus then
			   * goes BUS_FREE flag HAF_ABORT will tell error
			   * handler that this is OK.
			   */
			  if (ad_p->ad_negotiate[targ].params.do_abort == 1)
			    {
			      cfg_p->Cf_ScsiOption[targ].u.AbortMsg = 1;
			      ha_p->ha_flags |= HAF_ABORT;
			      do_negotiate = 1;
			    }

			  /* if a cdrom we don't want to negotiate unless
			   * adp_allow_cdrom_negotiations is on and either sync
			   * or wide is enabled.
			   */
			  else if (!ad_p->ad_negotiate[targ].params.is_cdrom ||
				(adp_allow_cdrom_negotiations &&
				 (ad_p->ad_negotiate[targ].params.allow_sync ||
				  ad_p->ad_negotiate[targ].params.allow_wide)))
			    {
			      if (ad_p->ad_negotiate[targ].params.allow_sync &&
				  !cfg_p->Cf_ScsiOption[targ].u.SyncMode)
				{
				  cfg_p->Cf_ScsiOption[targ].u.AsyncFrc = 1;
				  do_negotiate = 1;
				}

			      if (ad_p->ad_negotiate[targ].params.allow_wide &&
				  !cfg_p->Cf_ScsiOption[targ].u.WideMode)
				{
				  cfg_p->Cf_ScsiOption[targ].u.NarrowFrc = 1;
				  do_negotiate = 1;
				}

			      if (cfg_p->Cf_ScsiOption[targ].u.WideMode ||
				  cfg_p->Cf_ScsiOption[targ].u.SyncMode)
				do_negotiate = 1;
			    }
			  MUNLOCK(ad_p->ad_him_lk, s);
			}  /* end if cmd = inquiry */

			if (req_p->sr_flags & SRF_NEG_SYNC) {
			  ad_p->ad_negotiate[targ].params.allow_sync = 1;
			  ad_p->ad_negotiate[targ].params.force_sync = 1;
			  if (!cfg_p->Cf_ScsiOption[targ].u.SyncMode) {
			    MLOCK(ad_p->ad_him_lk, s);
			    cfg_p->Cf_ScsiOption[targ].u.SyncMode = 1;
			    MUNLOCK(ad_p->ad_him_lk, s);
			    do_negotiate = 1;
			  }
			}

			if (req_p->sr_flags & SRF_NEG_ASYNC) {
			  ad_p->ad_negotiate[targ].params.allow_sync = 0;
			  ad_p->ad_negotiate[targ].params.force_sync = 0;
			  if (cfg_p->Cf_ScsiOption[targ].u.SyncMode) {
			    MLOCK(ad_p->ad_him_lk, s);
			    cfg_p->Cf_ScsiOption[targ].u.SyncMode = 0;
			    cfg_p->Cf_ScsiOption[targ].u.AsyncFrc = 1;
			    do_negotiate = 1;
			    MUNLOCK(ad_p->ad_him_lk, s);
			  }
			}

			if (do_negotiate)
			  {
			    int was_paused;
		
			    base = ad_p->ad_himcfg->CFP_Base;
			    MLOCK(ad_p->ad_him_lk, s);
			    was_paused = PH_Pause_rtn(base);

			    PH_SetNeedNego(base, targ, 0); /* force negotiation */
			    if (!was_paused)
			      PH_UnPause(base);
			    MUNLOCK(ad_p->ad_him_lk, s);
			  }

			ad_p->ad_haq_ospl = LOCK(ad_p->ad_haq_lk, IPLFUNC);
			ha_enq_tail(&(ad_p->ad_busyha), ha_p, HAQ_BUSY);
			UNLOCK(ad_p->ad_haq_lk, ad_p->ad_haq_ospl);

			fill_scb(ha_p->ha_scb, req_p, ad_p);
			if (adp_verbose) 
				printf("adp_issue_cmds: ScbSend ha 0x%x cmd 0x%x to (%d,%d)\n",
				       ha_p, req_p->sr_command[0],
				       req_p->sr_ctlr, req_p->sr_target);
			ha_p->ha_start = lbolt;

			if (ip32_pci_enable_prefetch ||
			    ip32_pci_flush_prefetch == 1)
				pcimh_flush_buffers(ad_p->ad_vertex);

			MLOCK(ad_p->ad_him_lk, s);
			PH_ScbSend(ha_p->ha_scb);
			MUNLOCK(ad_p->ad_him_lk, s);
		} else {
			ad_p->ad_haq_ospl = LOCK(ad_p->ad_haq_lk, IPLFUNC);
			ha_enq_tail(&(ad_p->ad_doneha), ha_p, HAQ_DONE);
			UNLOCK(ad_p->ad_haq_lk, ad_p->ad_haq_ospl);
		}

		ad_p->ad_haq_ospl = LOCK(ad_p->ad_haq_lk, IPLFUNC);
		ha_p = ha_deq_head(&(ad_p->ad_freeha));
		UNLOCK(ad_p->ad_haq_lk, ad_p->ad_haq_ospl);
	}

	UNLOCK(ad_p->ad_adap_lk, ad_p->ad_adap_ospl);
}

/*
 * The cmd has been completed by the SCSI chip.  Do some final software
 * bookkeeping.
 *
 * Take ha's off the done queue and
 * - clear the target busy map
 * - return the ha's large sg list, if it has it
 * - call adp_issue_cmds to send out any queued/waiting scsi cmds
 * - sr_notify the callers
 * - put the ha on the free queue.
 */
static void
adp_return_cmds(adp78_adap_t *ad_p)
{
	scsi_request_t *req_p;
	adp78_ha_t *ha_p;

	ad_p->ad_haq_ospl = LOCK(ad_p->ad_haq_lk, IPLFUNC);
	ha_p = ha_deq_head(&ad_p->ad_doneha);
	UNLOCK(ad_p->ad_haq_lk, ad_p->ad_haq_ospl);

	while (ha_p) {
		req_p = ha_p->ha_req;
		req_p->sr_ha = NULL;
		ha_p->ha_req = NULL;
		ha_p->ha_flags = 0;

		/*
		 * If the ha has a lrgsg, return it.
		 * Point the scb sg pointer back to the normal one.
		 */
		if (ha_p->ha_lrgsg) {
			ad_p->ad_lrgsg = ha_p->ha_lrgsg;
			ha_p->ha_lrgsg = 0;
			ha_p->ha_lrgsg_numsg = 0;
#ifdef IP32
			ha_p->ha_scb->SP_SegPtr = kvtophys(ha_p->ha_sg) | PCI_NATIVE_VIEW;
#else
			ha_p->ha_scb->SP_SegPtr = kvtophys(ha_p->ha_sg);
#endif
		}

		ad_p->ad_adap_ospl = LOCK(ad_p->ad_adap_lk, IPLFUNC);
		if (SCSI_REQ_IS_TAGGED(req_p->sr_tag))
			ad_p->ad_tagcmds[req_p->sr_target]--;
		else
			ad_p->ad_busymap &= ~(1 << req_p->sr_target);
		UNLOCK(ad_p->ad_adap_lk, ad_p->ad_adap_ospl);
					
		ad_p->ad_haq_ospl = LOCK(ad_p->ad_haq_lk, IPLFUNC);
		ha_enq_tail(&ad_p->ad_freeha, ha_p, HAQ_FREE);
		UNLOCK(ad_p->ad_haq_lk, ad_p->ad_haq_ospl);

		adp_issue_cmds(ad_p);

		if (req_p->sr_notify)
			(*req_p->sr_notify)(req_p);

		ad_p->ad_haq_ospl = LOCK(ad_p->ad_haq_lk, IPLFUNC);
		ha_p = ha_deq_head(&ad_p->ad_doneha);
		UNLOCK(ad_p->ad_haq_lk, ad_p->ad_haq_ospl);
	}
}


/*
 * A comparison function for adp_timeout_cmds
 * Return 1 if the SCSI command is too old and should be timed out.
 */
/*ARGSUSED2 -- used in callback*/
static int
timeout_cmp_func(adp78_ha_t *ha_p, int key)
{

	if ((ha_p->ha_start + ha_p->ha_req->sr_timeout < lbolt) &&
	    ((ha_p->ha_flags & HAF_TIMED_OUT) == 0)) {
		cmn_err_tag(2, CE_ALERT, "SCSI command on (%d,%d,%d) timed out after %d secs.",
			ha_p->ha_id, ha_p->ha_req->sr_target, ha_p->ha_req->sr_lun,
			ha_p->ha_req->sr_timeout / HZ);
		return 1;
	}

	return 0;
}

/*
 * Check for commands that need to be timed out.
 */
static void
adp_timeout_cmds(adp78_adap_t *ad_p)
{
	adp78_ha_t *ha_p;
	int timeout=0;

	ad_p->ad_haq_ospl = LOCK(ad_p->ad_haq_lk, IPLFUNC);
	ha_p = ha_deq_key(&ad_p->ad_busyha, timeout_cmp_func, 0);
	if (ha_p) {

#ifdef DEBUG
		if (1) {
			int now=lbolt;
			printf("start time %d; (approx) time now %d\n",
			       ha_p->ha_start, now);
			dump_cdb(ha_p->ha_req->sr_command,
				 ha_p->ha_req->sr_cmdlen);
		}
#endif

		/*
		 * Since this ha is going back to the busy queue,
		 * I don't want to time out on this one again.
		 */
		ha_p->ha_flags = HAF_TIMED_OUT;

		ha_enq_tail(&ad_p->ad_busyha, ha_p, HAQ_BUSY);
		timeout = 1;
	}
	UNLOCK(ad_p->ad_haq_lk, ad_p->ad_haq_ospl);

	if (timeout)
		adp_reset(ad_p, 0, 1);
}


/*
 * The SCSI bus is being reset.  Clean up the busy queue and waiting request
 * queues.
 * - Return done cmds to higher level
 * - return all busy cmds
 * - return all waiting requests
 */
static void
adp_reset_cmds(adp78_adap_t *ad_p)
{
	adp78_ha_t *ha_p;

	/*
	 * Mark all currently queued reqs with SC_ATTN.
	 * They will be returned to the caller without being executed.
	 */
	ad_p->ad_req_ospl = LOCK(ad_p->ad_req_lk, IPLFUNC);
	sr_set_attn(ad_p);
	UNLOCK(ad_p->ad_req_lk, ad_p->ad_req_ospl);

	/* return cmds that have successfully completed */
	adp_return_cmds(ad_p);

	/*
	 * abort all busy commands.  Since the sequencer has been
	 * reset, I don't need to abort the cmd with the sequencer.
	 * I just need to move the ha's from the busy queue to the
	 * done queue, set the right return codes, and return them
	 * back up to the caller.
	 */
	ad_p->ad_haq_ospl = LOCK(ad_p->ad_haq_lk, IPLFUNC);
	ha_p = ha_deq_head(&ad_p->ad_busyha);
	UNLOCK(ad_p->ad_haq_lk, ad_p->ad_haq_ospl);

	while (ha_p) {
	        if (ha_p->ha_scb->SP_HaStat != HOST_NO_STATUS)
		  ha_p->ha_req->sr_status = SC_CMDTIME;
		else
		  ha_p->ha_req->sr_status = SC_ATTN;
		ha_p->ha_req->sr_scsi_status = 0;
		ha_p->ha_req->sr_ha_flags = 0;
		ha_p->ha_req->sr_sensegotten = 0;
		ha_p->ha_req->sr_resid = ha_p->ha_req->sr_buflen;

		ad_p->ad_haq_ospl = LOCK(ad_p->ad_haq_lk, IPLFUNC);
		ha_enq_tail(&ad_p->ad_doneha, ha_p, HAQ_DONE);
		ha_p = ha_deq_head(&ad_p->ad_busyha);
		UNLOCK(ad_p->ad_haq_lk, ad_p->ad_haq_ospl);
	}

	adp_return_cmds(ad_p);


	/* return the queued requests with SC_ATTN */
	ad_p->ad_req_ospl = LOCK(ad_p->ad_req_lk, IPLFUNC);
	sr_return_attn(ad_p);
	UNLOCK(ad_p->ad_req_lk, ad_p->ad_req_ospl);
}

static vertex_hdl_t
adp78_ctlr_vhdl_get(adp78_adap_t *ad_p)
{
	graph_error_t rv;
	char loc_str[LOC_STR_MAX_LEN];
	vertex_hdl_t ctlr_vhdl;

        sprintf(loc_str,"%s/%d",
		EDGE_LBL_SCSI_CTLR, SCSI_EXT_CTLR(ad_p->ad_ctlrid));
        rv = hwgraph_path_add(ad_p->ad_vertex, loc_str, &ctlr_vhdl);

        ASSERT((rv == GRAPH_SUCCESS) && (ctlr_vhdl != GRAPH_VERTEX_NONE));
        return(ctlr_vhdl);
}

/*
 * Add all SCSI devices on adap to hinv.  Its faster to do it here than to
 * have each scsi device driver probe each adapter.  Do not probe LUN's other
 * than 0; it turns out that some devices (including the Tandberg QIC tape
 * drive) do not handle it correctly.
 *
 * Has the side affect of returning the number of ha's that should be allocated
 * for this controller.  Each hard disk gets ADP_SCB_HDISK_WEIGHT ha's.
 * All other devices get ADP_SCB_DEV_WEIGHT each.  Return at least ADP_MINSCBS.
 */
static int
adp_hinv(adp78_adap_t *ad_p, int all_ids)
{
	u_char *inv;
	u_char targ;
	int num_ha=0;
	int lun;
	u_char ctlr;
	vertex_hdl_t ctlr_vhdl, lun_vhdl;

	/*
	 * IBM Scorpion needs over 128 ms for selection, so I have
	 * no choice but to stay with 256ms for selection.
	 */
	/* PH_Set_Selto(ad_p->ad_himcfg, STIMESEL0); */

	ctlr_vhdl = adp78_ctlr_vhdl_get(ad_p);
	ctlr = ad_p->ad_ctlrid;
	for (targ = 0; targ < ADP78_MAXTARG; targ++) {
		if (targ == ad_p->ad_scsiid)
			continue;

		if ((adp_noinq_map[ad_p->ad_ctlrid] & (1 << targ)) && !all_ids)
			continue;

		if ((lun_vhdl = scsi_lun_vhdl_get(ctlr_vhdl, targ, lun)) == GRAPH_VERTEX_NONE)
		  lun_vhdl = scsi_device_add(ctlr_vhdl, targ, 0);

		if (!(inv = adp_inquiry(lun_vhdl, ad_p, targ, 0, 0)))
		  {
		    /*
		     * If target no longer exists, remove all the LUN vertices
		     * also. Make sure no one has the LUN open before removing it --
		     * check ref count.
		     */
		    for (lun = 0; lun < ADP78_MAXLUN; lun ++) 
		      {

			if ((lun_vhdl = scsi_lun_vhdl_get(ctlr_vhdl, targ, lun)) != GRAPH_VERTEX_NONE) 
			  {
			    if (ad_p->ad_alloc[targ][lun] == 0)
			      scsi_device_remove(ctlr_vhdl, targ, lun);
			  }
		      }
		    continue;
		  }

		/* it is a valid target. Search for luns */

		if (adp_probe_all_luns[ctlr][targ]) 
		  {
		    for(lun = 1 ; lun < ADP78_MAXLUN; lun ++) 
		      {
			if ((lun_vhdl = scsi_lun_vhdl_get(ctlr_vhdl, targ, lun)) == GRAPH_VERTEX_NONE)
			  lun_vhdl = scsi_device_add(ctlr_vhdl, targ, lun);
			if (!adp_inquiry(lun_vhdl, ad_p, targ, lun, 0)) 
			  {
			    if (ad_p->ad_alloc[targ][lun] == 0)
			      scsi_device_remove(ctlr_vhdl, targ, lun);
			  }
		      }
		  }

		/*
		 * Adding devices to hinv is now done by a common function
		 * in scsi.c  But I still have to take a quick look at the
		 * inquiry data for my own purposes.
		 */

		switch(inv[0] & 0x1F) {
		case 0:
			/* hard disk, floppy, RAID, CDROM */
			if (cdrom_inquiry_test(&inv[8])) {
				num_ha += ADP_SCB_DEV_WEIGHT;

			} else if (inv[1] & 0x80) {
				/* removable media */
				num_ha += ADP_SCB_DEV_WEIGHT;

			} else if (!strncmp((char *) &inv[16], "U144 RAID    SGI", 16)) {
				num_ha += ADP78_NUMSCBS;

			} else {
				num_ha += ADP_SCB_HDISK_WEIGHT;
			}
			break;

		case 1:
			/* tape */
			num_ha += ADP_SCB_DEV_WEIGHT;
			break;

		case 5:
			/* CDROM */
			num_ha += ADP_SCB_DEV_WEIGHT;
			break;

		default:
			num_ha += ADP_SCB_DEV_WEIGHT;
			break;
		}
	}

/*	PH_Set_Selto(ad_p->ad_himcfg, STIMESEL1|STIMESEL0); */

	if (num_ha == 0)
		num_ha = ADP_MINSCBS;
	else
		num_ha = MAX(ADP_MINSCBS, num_ha);

	return num_ha;
}


/*
 * Takes a scsi_request_t, which is what IRIX uses to convey its scsi
 * requests, and fills out a sp_struct, which is what Adaptec sequencer
 * understands.
 *
 * This function never fails, thus has no return value.
 */
static void
fill_scb(sp_struct *scb_p, scsi_request_t *req_p, adp78_adap_t *ad_p)
{
	adp78_ha_t *ha_p= (adp78_ha_t *) scb_p->Sp_OSspecific;	
	unsigned int bit_map = 1;
	int segcnt;
	cfp_struct *cfp_p;

	if (req_p->sr_buflen == 0) {
		scb_p->SP_SegCnt = 0;
	} else {
		segcnt = fill_sglist(ha_p, req_p);

		if (segcnt > 255)
			scb_p->SP_SegCnt = 255;
		else
			scb_p->SP_SegCnt = segcnt;
	}

	scb_p->SP_NoUnderrun = 0;
	scb_p->SP_NegoInProg = 0;
	scb_p->SP_ResCnt = 0;
	scb_p->SP_HaStat = 0;
	scb_p->SP_TargStat = 0;
	scb_p->SP_RejectMDP = 0;
	
	cfp_p = scb_p->SP_ConfigPtr;
	if (ad_p->ad_negotiate[req_p->sr_target].params.is_cdrom &&
	    (!adp_allow_cdrom_negotiations ||
	     (!cfp_p->Cf_ScsiOption[req_p->sr_target].u.WideMode &&
	      !cfp_p->Cf_ScsiOption[req_p->sr_target].u.SyncMode)))
          scb_p->SP_NoNegotiation = 1;
	else
          scb_p->SP_NoNegotiation = 0;

	/*
	 * If the ha has a lrgsg, then it hold the sg list.  Point the
	 * scb's segptr at that.
	 */
	if (ha_p->ha_lrgsg)
#ifdef IP32
		scb_p->SP_SegPtr = kvtophys(ha_p->ha_lrgsg) | PCI_NATIVE_VIEW;
#else
	        scb_p->SP_SegPtr = kvtophys(ha_p->ha_lrgsg);
#endif


	/*
	 * Harry Yang says this must be at least 1.  If no segments are
	 * there (for commands such as Test Unit Ready, the len in the
	 * s/g list should be 0.
	 */
	if (scb_p->SP_SegCnt == 0) {
		scb_p->SP_SegCnt = 1;
		ha_p->ha_sg[0].data_ptr = 0;
		ha_p->ha_sg[0].data_len = SG_LAST_ELEMENT;
		DCACHE_WB(ha_p->ha_sg, sizeof(SG_ELEMENT));
	}


	bit_map = 1 << (req_p->sr_target);
	if((cfp_p->CFP_AllowDscnt & bit_map) == bit_map) 
		scb_p->SP_DisEnable = 1;
	else
		scb_p->SP_DisEnable = 0;

	if(req_p->sr_tag == SC_TAG_SIMPLE ||
	   req_p->sr_tag == SC_TAG_HEAD ||
	   req_p->sr_tag == SC_TAG_ORDERED) {
		/* Disconnect and Tag-queueing must both enabled */
		if((cfp_p->CFP_AllowDscnt & bit_map) == 0)
			cmn_err (CE_ALERT, "SCSI disconnection must be enabled in order for tag-queueing to work (%d,%d,%d).",
				 req_p->sr_ctlr, req_p->sr_target, req_p->sr_lun);

		scb_p->SP_TagEnable = 1;
		scb_p->SP_TagType = req_p->sr_tag & 0x3;
	} else {
		scb_p->SP_TagEnable = 0;
		scb_p->SP_TagType = SC_TAG_SIMPLE & 0x3;
	}
	scb_p->SP_Tarlun = (req_p->sr_target << 4) | (req_p->sr_lun);

	scb_p->SP_CDBLen = req_p->sr_cmdlen;
	ASSERT(((req_p->sr_cmdlen+3)/4)*4 <= MAX_CDB_LEN);
	bcopy(req_p->sr_command, scb_p->Sp_CDB, ((req_p->sr_cmdlen+3)/4) * 4);

	/*
	 * Sp_SensePtr and Sp_SenseLen are next to each other in the
	 * data structure and actually forms a one-element SG list.
	 * So for the length, I have to set the SG_LAST_ELEMENT bit.
	 * Delay flushing until we actually need to (in OptimaRequestSense)
	 */
	scb_p->Sp_SensePtr = kvtophys(req_p->sr_sense);
	scb_p->Sp_SenseLen = req_p->sr_senselen | SG_LAST_ELEMENT;
	scb_p->Sp_Sensebuf = req_p->sr_sense;
}

#ifdef DEBUG

/*
 * write garbage in the sg list
 */
void
dirty_sglist(SG_ELEMENT *listp, int numsg)
{
	int i=0;

	while (i < numsg) {
		listp[i].data_ptr = (void *) (0x7f000000 | i);
		listp[i].data_len = 0x397 | SG_LAST_ELEMENT;
		i++;
	}

	DCACHE_WB(listp, numsg * sizeof(SG_ELEMENT));
}


int check_delay_val = 0;

/*
 * Make sure the list is the correct length
 * Look at the phys memory
 */
void
check_sglist(SG_ELEMENT *listp, int count)
{
	int listcount=0;
	SG_ELEMENT *phys_listp;

	phys_listp = (SG_ELEMENT *) PHYS_TO_K1(kvtophys(listp));
	if (! IS_KSEG1(phys_listp))
		return;

	while (1) {
		/*
		 * Each paddr in the sglist should be going
		 * through the byte swapper in MACE.
		 */
		ASSERT((uint)phys_listp->data_ptr < PCI_NATIVE_VIEW);
		listcount += phys_listp->data_len & (~SG_LAST_ELEMENT);
		if (phys_listp->data_len & SG_LAST_ELEMENT)
			break;
		phys_listp++;
	}
	if (count != listcount)
		cmn_err(CE_PANIC, "SCSI list count %d count %d.",
			listcount, count);

	us_delay(check_delay_val);
}
#endif  /* DEBUG */

/*
 * Fills the scatter/gather list from the buf_t.
 *
 * Does the invalidation or wb invalidation of the data buffers as needed.
 * wbinvalidates the s/g list.
 *
 * Returns the number of s/g elements filled.  This function never fails.
 */
static int
fill_sglist(adp78_ha_t *ha_p, scsi_request_t *req_p)
{
	sp_struct *scb_p = ha_p->ha_scb;
	buf_t *bp = req_p->sr_bp;
	int segcnt;
	uint view;
#ifdef ALENLIST
	alenlist_t alenlist;
#endif
	ushort flags = req_p->sr_flags;

	ASSERT(ha_p->ha_sg);
	ASSERT(scb_p->Sp_OSspecific == ha_p);

	view = 0;

#ifdef DEBUG
	if (ha_p->ha_lrgsg)
		dirty_sglist(ha_p->ha_lrgsg, ha_p->ha_lrgsg_numsg);
	else
		dirty_sglist(ha_p->ha_sg, ha_p->ha_numsg);
#endif

#ifdef ADP_STATS
	if (flags == 0x4000) {
		/*
		 * Special hack flag for playing with tiles.
		 * See scsi_stats.c
		 */
		segcnt = adp_dma_maptile(ha_p, req_p->sr_buffer,
					 req_p->sr_buflen, view);

	} else
#endif

	if (flags & SRF_MAPBP){
#ifdef ALENLIST
		alenlist = buf_to_alenlist(bp);
		segcnt = alen_to_sg(ha_p, alenlist, req_p->sr_buflen, view);
#else
		segcnt = adp_dma_mapbp(ha_p, bp, req_p->sr_buflen, view); 
#endif
	} else {
#ifdef ALENLIST
		alenlist = kvaddr_to_alenlist(req_p->sr_buffer, req_p->sr_buflen);
		segcnt = alen_to_sg(ha_p, alenlist, req_p->sr_buflen, view);
#else
		segcnt = adp_dma_map(ha_p, (caddr_t)req_p->sr_buffer, 
				     req_p->sr_buflen, view); 
#endif
	}

#ifdef DEBUG
	if (ha_p->ha_lrgsg)
		check_sglist(ha_p->ha_lrgsg, req_p->sr_buflen);
	else
		check_sglist(ha_p->ha_sg, req_p->sr_buflen);
#endif

	if ((segcnt >= 0) && (flags & SRF_FLUSH) && req_p->sr_buflen) {
		if (flags & SRF_DIR_IN)
			DCACHE_INVAL(req_p->sr_buffer, req_p->sr_buflen);
		else if (cachewrback)
			DCACHE_WB(req_p->sr_buffer, req_p->sr_buflen);
	}

#ifdef ALENLIST
	alenlist_destroy(alenlist);
#endif
	return segcnt;
}

/*
 * Do some macro/casting magic to make the compiler happy
 */
#define SET_VIEW(a)   (((uint)(a) & ~PCI_NATIVE_VIEW) | view)

#ifndef ALENLIST
/*
 * Fills out the s/g (or DMA descriptor) list from the mapped bp.
 * This can be simplified by just calling kvtophys over all the addresses,
 * but I suppose this saves a few function calls.
 *
 * Returns the number of s/g elements filled.  This function never fails.
 */
int
adp_dma_map(adp78_ha_t *ha_p, caddr_t addr, uint remain_bytes, uint view)
{
	uint actual_bytes, len_in_page, do_trans, segcnt=0;
	pde_t *pde;
	SG_ELEMENT *sg;
	int numsg;

	if (IS_KSEG2(addr)) {
		pde = kvtokptbl(addr);
		do_trans = 1;
	} else {
		if (IS_KSEGDM(addr))
			addr = (caddr_t) KDM_TO_PHYS(addr);
		do_trans = 0;
	}

	/*
	 * The ha is guarenteed to have enough room for this request.
	 */
	if (ha_p->ha_lrgsg) {
		sg = ha_p->ha_lrgsg;
		numsg = ha_p->ha_lrgsg_numsg;
	} else {
		sg = ha_p->ha_sg;
		numsg = ha_p->ha_numsg;
	}

	/*
	 * map the first pageful of data, which may not be page aligned.
	 */
	len_in_page = NBPC - poff(addr);
	actual_bytes = MIN(remain_bytes, len_in_page);

	if (do_trans) {
		sg[segcnt].data_ptr = (void *)
			SET_VIEW(pdetophys(pde++) + poff(addr));
	} else {
		sg[segcnt].data_ptr = (void *) SET_VIEW(addr);
	}
	sg[segcnt].data_len = actual_bytes;

	remain_bytes -= actual_bytes;
	addr += actual_bytes;
	segcnt++;

	/*
	 * map the rest of the pages, which are page aligned.
	 */
	while (remain_bytes > 0) {

		if (segcnt >= numsg)
			cmn_err(CE_PANIC, "SCSI dma_map segcnt too large (%d).",
				segcnt);

		actual_bytes = MIN(remain_bytes, NBPC);

		sg[segcnt].data_ptr = do_trans ?
			(void *) SET_VIEW(pdetophys(pde)) : (void *) SET_VIEW(addr);
		sg[segcnt].data_len = actual_bytes;
		remain_bytes -= actual_bytes;
		addr += actual_bytes;

		if (do_trans)
			pde++;
		segcnt++;

	}

	sg[segcnt - 1].data_len |= SG_LAST_ELEMENT;

	DCACHE_WB(sg, segcnt * sizeof(SG_ELEMENT));
	return segcnt;
}

/*
 * Fills out the s/g (or DMA descriptor) list from the unmapped bp.
 *
 * Returns the number of s/g elements filled.  This function never fails.
 */

int
adp_dma_mapbp(adp78_ha_t *ha_p, buf_t *bp, uint remain_bytes, uint view)
{
	uint actual_bytes, segcnt=0;
	struct pfdat *pfd = NULL;
	SG_ELEMENT *sg;
	int numsg;

	ASSERT(bp->b_flags & B_PAGEIO);

	/*
	 * The ha is guarenteed to have enough room for this request.
	 */
	if (ha_p->ha_lrgsg) {
		sg = ha_p->ha_lrgsg;
		numsg = ha_p->ha_lrgsg_numsg;
	} else {
		sg = ha_p->ha_sg;
		numsg = ha_p->ha_numsg;
	}

	while (remain_bytes > 0) {

		if (segcnt >= numsg)
			cmn_err(CE_PANIC,
				"SCSI dma_mapbp segcnt too large (%d).",segcnt);

		pfd = getnextpg(bp, pfd);
		ASSERT(pfd);
		actual_bytes = MIN(remain_bytes, NBPC);

		sg[segcnt].data_ptr = (void *) SET_VIEW(pfdattophys(pfd));
		sg[segcnt].data_len = actual_bytes;
		remain_bytes -= actual_bytes;

		segcnt++;
	}

	sg[segcnt - 1].data_len |= SG_LAST_ELEMENT;

	DCACHE_WB(sg, segcnt * sizeof(SG_ELEMENT));
	return segcnt;
}


#ifdef ADP_STATS
/*
 * A list of pre-built sg lists are passed in addr
 * remain bytes is actually the number of sg elements
 * (right now just handles a single 64K tile.)
 * Returns the number of s/g elements filled.  This function never fails.
 */
static int
adp_dma_maptile(adp78_ha_t *ha_p, caddr_t addr, uint remain_bytes, uint view)
{
	uint segcnt=remain_bytes;

	bcopy(addr, ha_p->ha_sg, segcnt * SG_ELEMENT_SIZE);
	DCACHE_WB(ha_p->ha_sg, segcnt * SG_ELEMENT_SIZE);
	return segcnt;
}
#endif


#else /* ALENLIST */

int
alen_to_sg(adp78_ha_t *ha_p, alenlist_t alenlist, uint remain_bytes, uint view)
{
	alenlist_cursor_t   cursor;
	alenaddr_t          address, last_addr = 0;
	size_t              length, last_len = 0;
	SG_ELEMENT *sg;
	int numsg;
	uint actual_bytes, do_trans, segcnt=0;

	/*
	 * The ha is guarenteed to have enough room for this request.
	 */
	if (ha_p->ha_lrgsg) {
		sg = ha_p->ha_lrgsg;
		numsg = ha_p->ha_lrgsg_numsg;
	} else {
		sg = ha_p->ha_sg;
		numsg = ha_p->ha_numsg;
	}

	(void)alenlist_cursor_init(alenlist, 0, &cursor);
	while (alenlist_get(alenlist, &cursor, IO_NBPP, &address,
		&length) == ALENLIST_SUCCESS) {

		if (segcnt >= numsg)
			cmn_err(CE_PANIC,
				"SCSI alen_to_sg segcnt too large (%d).",
				segcnt);

		actual_bytes = MIN(remain_bytes, NBPC);
		actual_bytes = MIN(length, actual_bytes);

		sg[segcnt].data_ptr = (void *) SET_VIEW(kvtophys((void *)address));
		sg[segcnt].data_len = actual_bytes;
		remain_bytes -= actual_bytes;

		segcnt++;
	}

	sg[segcnt - 1].data_len |= SG_LAST_ELEMENT;

	DCACHE_WB(sg, segcnt * sizeof(SG_ELEMENT));
	return segcnt;
}
#endif /* ALENLIST */


/*
 * Do inquiry.  Returns a buffer to the returned inquiry info, or NULL
 * on error.
 */
static u_char *
adp_inquiry(vertex_hdl_t lun_vhdl, adp78_adap_t *ad_p, u_char targ, u_char lun, u_char do_abort)
{
	scsi_request_t *req_p;
	scsi_target_info_t *info_p;
	sema_t *donesem;
	int tries=4;
	u_char *ret;
	adp78_adap_t *adp;
	cfp_struct *cfg_p;
	int do_negotiate = 0;
        AIC_7870 *base;
	uint targ_arg;

	if (adp78alloc(lun_vhdl, 0, 0) == SCSIDRIVER_NULL)
		return NULL;

	targ_arg = targ;
	MLOCK(ad_p->ad_targ_open_lk[targ_arg], s);
	
	info_p = &ad_p->ad_info[targ][lun];
	if (info_p->si_inq == NULL)
		info_p->si_inq = kmem_alloc(SCSI_INQUIRY_LEN,
					  KM_PHYSCONTIG|KM_CACHEALIGN);
	if (info_p->si_sense == NULL)
		info_p->si_sense = kmem_alloc(SCSI_SENSE_LEN,
					    KM_PHYSCONTIG|KM_CACHEALIGN);

	req_p = (scsi_request_t *)kern_calloc(sizeof(*req_p) + SC_CLASS0_SZ, 1);
	req_p->sr_command = (u_char *)&req_p[1];
	req_p->sr_sense = info_p->si_sense;
	req_p->sr_senselen = SCSI_SENSE_LEN;

	req_p->sr_notify = adp_inquiry_done;
	req_p->sr_command[0] = 0x12;
	req_p->sr_command[1] = lun << 5;
	req_p->sr_command[4] = SCSI_INQUIRY_LEN;
	req_p->sr_cmdlen = SC_CLASS0_SZ;
	req_p->sr_timeout = 10 * HZ;
	req_p->sr_ctlr = ad_p->ad_ctlrid;
	req_p->sr_target = targ;
	req_p->sr_lun = lun;

	donesem = (sema_t *) kern_calloc(sizeof(*donesem), 1);
	init_sema(donesem, 0, "inq", targ);
	req_p->sr_dev = (void *) donesem;

inq_again:
	bzero(info_p->si_inq, SCSI_INQUIRY_LEN);
	req_p->sr_buffer = info_p->si_inq;
	req_p->sr_buflen = SCSI_INQUIRY_LEN;
	/* set AEN_ACK in case AEN still set from an earlier command;
	 * we have to assume that when this routine is called, that a
	 * reqsense has already been done... */
	req_p->sr_flags = SRF_DIR_IN | SRF_MAP | SRF_AEN_ACK | SRF_FLUSH;
	adp78command(req_p);
	psema(donesem, PRIBIO);

	if((req_p->sr_status == SC_GOOD) &&
	   (req_p->sr_scsi_status == ST_GOOD) &&
	   (req_p->sr_sensegotten == 0))
	  {
	    ret = info_p->si_inq;

	   if (do_abort ||
	       ((info_p->si_inq[0] & 0xC0) == 0x40 && 
		!fo_is_failover_candidate(info_p->si_inq, lun_vhdl)))
	       
	     {	
	       ret = NULL;
	     }
	    

	   else {
	     scsi_device_update(info_p->si_inq, lun_vhdl);

	     ad_p->ad_negotiate[targ].params.first_inq_done = 1;

	    /*
	     * Does the device support WIDE?
	     */
	    adp = adp78_array[req_p->sr_ctlr];
	    
	    adp->ad_negotiate[targ].params.is_cdrom = 
	      cdrom_inquiry_test(&info_p->si_inq[8]);
	    cfg_p = adp->ad_himcfg;
	    base = cfg_p->CFP_Base;
	    
	    targ = req_p->sr_target;
	    
	    if ((info_p->si_inq[SCSI_INFO_BYTE] & 0x20) ||
		adp->ad_negotiate[targ].params.force_wide)
	      { 
		if (!adp->ad_negotiate[targ].params.is_cdrom ||
		    adp_allow_cdrom_negotiations ||
		    adp->ad_negotiate[targ].params.force_wide)
		  {
		    if (adp->ad_negotiate[targ].params.allow_wide &&
			!cfg_p->Cf_ScsiOption[targ].u.WideMode)
		      {
			cfg_p->Cf_ScsiOption[targ].u.WideMode = 1;
			do_negotiate = 1;
		      }
		  }
	      }
	    else    /* not wide capable and not forcing wide */
	      {
		adp->ad_negotiate[targ].params.allow_wide = 0;
		if (cfg_p->Cf_ScsiOption[targ].u.WideMode)
		  {
		    cfg_p->Cf_ScsiOption[targ].u.WideMode = 0;
		    cfg_p->Cf_ScsiOption[targ].u.NarrowFrc = 1;
		    do_negotiate = 1;
		  }
	      }
	    
	    /*
	     * Does the device support SYNC?
	     */
	    if ((info_p->si_inq[SCSI_INFO_BYTE] & 0x10) ||
		adp->ad_negotiate[targ].params.force_sync)
	      {
		if (!adp->ad_negotiate[targ].params.is_cdrom ||
		    adp_allow_cdrom_negotiations ||
		    adp->ad_negotiate[targ].params.force_sync)
		  {
		    if (adp->ad_negotiate[targ].params.allow_sync &&
			!cfg_p->Cf_ScsiOption[targ].u.SyncMode)
		      {
			cfg_p->Cf_ScsiOption[targ].u.SyncMode = 1;
			do_negotiate = 1;
		      }
		  }
	      }
	    else    /* not capable and not forcing sync */
	      {
		adp->ad_negotiate[targ].params.allow_sync = 0;
		if (cfg_p->Cf_ScsiOption[targ].u.SyncMode)
		  {
		    cfg_p->Cf_ScsiOption[targ].u.SyncMode = 0;
		    cfg_p->Cf_ScsiOption[targ].u.AsyncFrc = 1;
		    do_negotiate = 1;
		  }
	      }
	    if (do_negotiate) 
	      {
		int was_paused;
		
		base = ad_p->ad_himcfg->CFP_Base;
		MLOCK(ad_p->ad_him_lk, s);
		was_paused = PH_Pause_rtn(base);

		PH_SetNeedNego(base, targ, 0); /* force negotiation */
		if (!was_paused)
		  PH_UnPause(base);
		MUNLOCK(ad_p->ad_him_lk, s);
	      }
 	    }
	  }	
	else if (tries-- && req_p->sr_status != SC_TIMEOUT &&
		 req_p->sr_status != SC_CMDTIME) {
		delay(HZ/4);
		goto inq_again;

	} else {
		kmem_free(info_p->si_inq, SCSI_INQUIRY_LEN);
		kmem_free(info_p->si_sense, SCSI_SENSE_LEN);
		info_p->si_inq = NULL;
		info_p->si_sense = NULL;
		ret = NULL;
	}

	kern_free(req_p);
	freesema(donesem);
	kern_free(donesem);

	adp78free(lun_vhdl, NULL);

	MUNLOCK(ad_p->ad_targ_open_lk[targ_arg], s);
	return ret;
}

/*
 * adp_inquiry is sleeping on the semaphore pointed to by sr_dev.
 */
static void
adp_inquiry_done(scsi_request_t *req_p)
{
	vsema(req_p->sr_dev);
}


/*
 * Convert a UBYTE from scb->HaStat to a uint that can be put in 
 * scsi_request.sr_status
 */
static uint
convert_hastatus(UBYTE hastatus)
{
	uint stat;

	switch(hastatus) {
	case HOST_ABT_HOST:
	case HOST_ABT_HA:
	case HOST_INV_LINK:
	case HOST_SNS_FAIL:
	case HOST_TAG_REJ:
	case HOST_NOAVL_INDEX:
	case HOST_NO_STATUS:
	case HOST_BUS_FREE:
	case HOST_PHASE_ERR:
	case HOST_HW_ERROR:
	case HOST_RST_HA:
	case HOST_RST_OTHER:
	case HOST_DU_DO:
		stat = SC_HARDERR;
		break;

	case HOST_SEL_TO:
		stat = SC_TIMEOUT;
		break;

	case HOST_ABT_FAIL:
		stat = SC_CMDTIME;
		break;

	default:
		stat = SC_HARDERR;
		break;
		
	}

	return (stat);
}

/*
 * Check the completion of a command, or some status which requires the HIM
 * to do stuff, by polling the INTSTAT register.
 *
 * This function will return a 0 if the command was completed successfully.
 * -1 if not.
 */
int
adp_poll_status(adp78_adap_t *ad_p)
{
	unsigned char rval, intstat;
	unsigned long count=0;

	if (adp_verbose)
		printf("polling on %d\n", ad_p->ad_ctlrid);
	
	while (1) {
		MLOCK(ad_p->ad_him_lk, s);
		intstat = PH_ReadIntstat(ad_p->ad_himcfg->CFP_Base);
		MUNLOCK(ad_p->ad_him_lk, s);

		if (intstat & ANYINT) {
			drv_usecwait(POLL_WAIT*3);	/* wait for DMA */

			MLOCK(ad_p->ad_him_lk, s);
			rval = PH_IntHandler(ad_p->ad_himcfg);
			MUNLOCK(ad_p->ad_him_lk, s);

			if ((rval & ANYINT) != 0)
				break;
		}
		drv_usecwait(POLL_WAIT);
		count++;

		/* XXX should put something to timeout a command */
	}

	adp_return_cmds(ad_p);

	if (adp_verbose)
		printf("finished polling\n");	

	return 0;
}


/*
 *****************************************************************************
 *
 * These functions must be called with interrupts raised (and locks acquired
 * on MP).
 *
 *****************************************************************************
 */

/*
 * Enqueue the ha_p on end of the queue.
 */
void
ha_enq_tail(adp78_ha_t **ha_pp, adp78_ha_t *ha_p, u_char qflag)
{
	ASSERT(ha_p->ha_qflag == 0);
	ha_p->ha_qflag = qflag;

	if (*ha_pp == NULL) {
		ha_p->ha_forw = ha_p;
		ha_p->ha_back = ha_p;
		*ha_pp = ha_p;
	} else {
		adp78_ha_t *haq = *ha_pp;
		ha_p->ha_forw = haq;
		ha_p->ha_back = haq->ha_back;
		haq->ha_back = ha_p;
		ha_p->ha_back->ha_forw = ha_p;
	}
}


/*
 * de-queue a ha from the head of the list and return it.
 * Returns NULL if none are found.
 */
adp78_ha_t *
ha_deq_head(adp78_ha_t **ha_pp)
{
	adp78_ha_t *ha_p;

	/* list empty */
	if (*ha_pp == NULL)
		return NULL;

	ha_p = *ha_pp;

	/* only 1 ha left */
	if (ha_p->ha_forw == ha_p) {
		ASSERT(ha_p->ha_back == ha_p);
		*ha_pp = NULL;
	} else {
		ha_p->ha_forw->ha_back = ha_p->ha_back;
		ha_p->ha_back->ha_forw = ha_p->ha_forw;
		*ha_pp = ha_p->ha_forw;
	}

	ha_p->ha_qflag = 0;	
	ha_p->ha_forw = NULL;
	ha_p->ha_back = NULL;

	return ha_p;
}

/*
 * dequeue the given ha from the list and return it.
 * Returns NULL if the given ha was NULL, otherwise, the ha must be on
 * the list.
 */
static adp78_ha_t *
ha_deq_ha(adp78_ha_t **ha_pp, adp78_ha_t *ha_p)
{
#ifdef DEBUG
	adp78_ha_t *dha_p;
	int found=0;

	/* make sure the given ha is on the given list */
	ASSERT(*ha_pp);
	dha_p = *ha_pp;
	do {
		if (dha_p == ha_p) {
			found = 1;
			break;
		}
		dha_p = dha_p->ha_forw;
	} while (dha_p != *ha_pp);
	ASSERT(found);
#endif

	if (ha_p == NULL)
		return NULL;

	/*
	 * Remove the ha from the list.  3 possibilities:
	 * head of queue (*ha_pp) points to this ha and it is the only one on the list
	 * head of queue points to this ha and it is not the only one on the list
	 * head of queue does not point to his ha, implying that there are more than
	 * one ha on this list
	 */
	if (ha_p == *ha_pp) {
		if (ha_p->ha_forw == ha_p) {    
			ASSERT(ha_p->ha_back == ha_p);
			*ha_pp = NULL;
		} else {
			ha_p->ha_forw->ha_back = ha_p->ha_back;
			ha_p->ha_back->ha_forw = ha_p->ha_forw;
			*ha_pp = ha_p->ha_forw;
		}
	} else {
		ha_p->ha_forw->ha_back = ha_p->ha_back;
		ha_p->ha_back->ha_forw = ha_p->ha_forw;
	}

	ha_p->ha_qflag = 0;
	ha_p->ha_forw = NULL;
	ha_p->ha_back = NULL;

	return ha_p;
}


/*
 * Generalized "find the ha" in this queue function.  Each ha is passed
 * to the given match_func along with the key.  If match_func returns 1,
 * then that ha is taken off the queue and returned.  If no ha is found,
 * return NULL.
 */
static adp78_ha_t *
ha_deq_key(adp78_ha_t **ha_pp, int (match_func(adp78_ha_t *, int)), int key)
{
	adp78_ha_t *ha_p = *ha_pp;

	if (ha_p == NULL)
		return NULL;

	do {
		if (match_func(ha_p, key)) {
			ha_deq_ha(ha_pp, ha_p);
			return ha_p;
		}
		ha_p = ha_p->ha_forw;
	} while (ha_p != *ha_pp);
	    
	return NULL;
}

#ifdef qerr1
/*
 * Target is in contingent allegiance.
 * Abort all tagged commands for that target.  See 7.3.3.1
 */
static void
ha_qerr_abort(adp78_adap_t *ad_p, int target)
{
	adp78_ha_t *ha_p;
	scsi_request_t *req_p;

	if (ad_p->ad_busyha == NULL)
		return;

	ha_p = ad_p->ad_busyha;
	do {
		ASSERT(ha_p->ha_qflag & HAQ_BUSY);
		req_p = ha_p->ha_req;
		if ((SCSI_REQ_IS_TAGGED(req_p->sr_tag)) &&
		    (req_p->sr_target == target)) {
			/*
			 * Abort the command by setting SC_ATTN with the
			 * correct sense info.  Put it on the done queue.
			 */
			req_p->sr_status = SC_ATTN;
			req_p->sr_scsi_status = 0;
			bcopy(req_p->sr_sense, adp_qerr_sense,
			      MIN(req_p->sr_senselen, adp_qerr_senselen));
			req_p->sr_sensegotten = adp_qerr_senselen;
			req_p->sr_resid = req_p->sr_buflen;

			/*
			 * XXX can't just take things off the busy queue
			 * and put them on the done queue.  The scb must
			 * be aborted in the HIM.  see the timeout code.
			 */
			ad_p->ad_haq_ospl = LOCK(ad_p->ad_haq_lk, IPLFUNC);
			ha_deq_ha(&ad_p->ad_busyha, ha_p);
			ha_enq_tail(&ad_p->ad_doneha, ha_p, HAQ_DONE);
			UNLOCK(ad_p->ad_haq_lk, ad_p->ad_haq_ospl);

			/*
			 * start from the beginning again.
			 * Yuck, n^2.  Fortunately, n is usually small.
			 */
			ha_p = ad_p->ad_busyha;
			
		} else {
			ha_p = ha_p->ha_forw;
		}

	} while ((ha_p != ad_p->ad_busyha) && (ad_p->ad_busyha != NULL));
}
#endif

/*
 * Enqueue a scsi_request_t on the ad_waithead.  The new scsi_request_t
 * is put on the tail.
 */
static void
sr_enq_tail(adp78_adap_t *ad_p, scsi_request_t *req_p)
{

	if (ad_p->ad_waithead == NULL) {
		ASSERT(ad_p->ad_waittail == NULL);
		ad_p->ad_waithead = req_p;
		ad_p->ad_waittail = req_p;
	} else {
		ad_p->ad_waittail->sr_ha = req_p;
		ad_p->ad_waittail = req_p;
	}
}


/*
 * Return 1 if the scsi_request is runable.  0 if it is not.
 *
 * "runable" is defined as:
 * If the command is not tagged, then no other commands is outstanding
 * on that target,
 * AND
 * No requests ahead of this one for the same target is waiting for
 * more memory for its sg list
 * AND
 * This amount of data for this request can be mapped to a sg list
 * currently available.
 *
 * If the request needs to use a large sg list, it is passed back
 * in the sr_spare pointer.
 */
static int
sr_is_runable(adp78_adap_t *ad_p, scsi_request_t *req_p)
{
	uint busymap, numsg, runable;

	/*
	 * Cannot send a command to this target because it already has a
	 * non-tagged command and/or there is another command ahead of this
	 * one that is stuck waiting for memory.
	 */
	busymap = ad_p->ad_busymap | ad_p->ad_memwaitmap;
	if ((1 << req_p->sr_target) & busymap)
		return 0;

	/*
	 * Cannot send a non-tagged command to a target that currently
	 * is executing one or more tagged commands.  Return not runable
	 * and also request future tagged commands be held off until
	 * this is one is executed.
	 */
	if (!SCSI_REQ_IS_TAGGED(req_p->sr_tag) &&
	    (ad_p->ad_tagcmds[req_p->sr_target] > 0)) {
		ad_p->ad_tagholdmap |= (1 << req_p->sr_target);
		return 0;
	}

	/*
	 * Conservatively estimate the number of sg elements needed to
	 * map this scsi request.  Its a conservative estimate because
	 * is assumes one sg element is needed per page, which is not true
	 * for tiles and large phys pages.  Also, assume worst case
	 * page alignment, where there is a partial page at the beginning
	 * and partial page at the end of the data.
	 */
	numsg = btop(req_p->sr_buflen) + 2;

	/*
	 * Easy case.  We can use the normal sized sg list.
	 * This one is runable.
	 */
	if (numsg <= ad_p->ad_normsg_numsg) {
		runable = 1;
		goto runable_out;
	}

	/*
	 * Check if this buffer is bigger than what 
	 * lrgsg can hold.
	 */
	if (numsg > ad_p->ad_lrgsg_numsg) {
		cmn_err(CE_ALERT, "SCSI command 0x%x for (%d,%d,%d) rejected because its too large, increase maxdmasz.",
			req_p->sr_command[0],
			req_p->sr_ctlr,
			req_p->sr_target,
			req_p->sr_lun);
		req_p->sr_status = SC_REQUEST;
		runable = 1;
		goto runable_out;
	}

	/*
	 * If the lrgsg is available for this req to use,
	 * then grab it.  Else, mark this target as busy
	 * because there is a command waiting for memory.
	 */
	if (ad_p->ad_lrgsg) {
		req_p->sr_spare = ad_p->ad_lrgsg;
		ad_p->ad_lrgsg = 0;
		runable = 1;
	} else {
		ad_p->ad_memwaitmap |= (1 << req_p->sr_target);
		runable = 0;
	}

	/*
	 * If this non-tagged cmd is not runable, hold off other tagged
	 * cmds until this one executes.
	 */
	if ((!runable) && (!SCSI_REQ_IS_TAGGED(req_p->sr_tag)))
		ad_p->ad_tagholdmap |= (1 << req_p->sr_target);

runable_out:

	/*
	 * Update maps for this new runable request.
	 */
	if (runable) {
		if (SCSI_REQ_IS_TAGGED(req_p->sr_tag))
			ad_p->ad_tagcmds[req_p->sr_target]++;
		else
			ad_p->ad_busymap |= (1 << req_p->sr_target);
	}

	return runable;
}


/*
 * Call sr_is_runable to find the first scsi request that can be sent out.
 * Deq that request and return it.
 * If no runable requests are found, return NULL.
 */
static scsi_request_t *
sr_deq_runable(adp78_adap_t *ad_p)
{
	scsi_request_t *reqhead, *reqcurr;

	if (ad_p->ad_waithead == NULL) {
		ASSERT(ad_p->ad_waittail == NULL);
		return NULL;
	}

	reqhead = ad_p->ad_waithead;
	if (sr_is_runable(ad_p, reqhead)) {
		/* first one is OK */
		if (ad_p->ad_waithead == ad_p->ad_waittail) {
			ad_p->ad_waithead = NULL;
			ad_p->ad_waittail = NULL;
		} else {
			ad_p->ad_waithead = reqhead->sr_ha;
			ASSERT(ad_p->ad_waithead != NULL);
		}
		reqhead->sr_ha = NULL;
		return reqhead;
	}

	while (reqhead->sr_ha != NULL) {
		reqcurr = reqhead->sr_ha;
		if (sr_is_runable(ad_p, reqcurr)) {
			/* reqcurr is OK */
			if (reqcurr == ad_p->ad_waittail) {
				/* reqcurr is at the end of the list */
				ad_p->ad_waittail = reqhead;
				reqhead->sr_ha = NULL;
			} else {
				/* reqcurr is in the middle of the list */
				reqhead->sr_ha = reqcurr->sr_ha;
			}
			reqcurr->sr_ha = NULL;
			return reqcurr;
		}

		reqhead = reqhead->sr_ha;
	}

	return NULL;
}


/*
 * Set all requests in the queue with SC_ATTN
 */
static void
sr_set_attn(adp78_adap_t *ad_p)
{
	scsi_request_t *req_p;

	req_p = ad_p->ad_waithead;
	while (req_p) {
		req_p->sr_status = SC_ATTN;
		req_p = req_p->sr_ha;
	}
}


/*
 * Go down the queued scsi request chain and return all commands that
 * have sr_status set to SC_ATTN
 */
static void
sr_return_attn(adp78_adap_t *ad_p)
{
	scsi_request_t *req_p, *last_req_p = NULL, *return_req_p;

	req_p = ad_p->ad_waithead;
	while (req_p) {
		if (req_p->sr_status == SC_ATTN) {

			return_req_p = req_p;

			/*
			 * Remove the req_p from the list and update
			 * the queue pointers.
			 */
			if (req_p == ad_p->ad_waithead) {
				/* first one is marked for return */
				if (ad_p->ad_waithead == ad_p->ad_waittail) {
					ad_p->ad_waithead = NULL;
					ad_p->ad_waittail = NULL;
					req_p = NULL;
				} else {
					ad_p->ad_waithead = req_p->sr_ha;
					req_p = ad_p->ad_waithead;
				}

			} else if (req_p == ad_p->ad_waittail) {
				ad_p->ad_waittail = last_req_p;
				last_req_p->sr_ha = NULL;
				req_p = NULL;

			} else {
				last_req_p->sr_ha = req_p->sr_ha;
				req_p = last_req_p->sr_ha;
			}

			return_req_p->sr_ha = NULL;
			(*return_req_p->sr_notify)(return_req_p);

		} else {
			last_req_p = req_p;
			req_p = req_p->sr_ha;
		}
	}
}


/**************************************************************************
 *
 * Interrupt handler.
 *
 **************************************************************************/
#if defined(IP32)
/*ARGSUSED1*/
static void
adp78intr(eframe_t *ep, intr_arg_t generic)
{
	__psint_t adap = (__psint_t)generic;
	adp78_adap_t *ad_p;
	unsigned char rval;

	if (adp_verbose)
		printf("adp78intr on %d\n", adap);

	/*
	 * On Moosehead, we will know what adapter this interrupt came
	 * from if the card is on the motherboard.  If not, we have to
	 * resort to polling (XXX polling not implemented.)
	 */
	ad_p = adp78_array[adap];
	ASSERT(ad_p);

	MLOCK(ad_p->ad_him_lk, s);
	rval = PH_IntHandler(ad_p->ad_himcfg);
	MUNLOCK(ad_p->ad_him_lk, s);

	if (rval == 0)
		ad_p->ad_strayintr++;

	adp_return_cmds(ad_p);
}

#endif /* IP32 */


/*
 * This will be called from PH_IntHandler if it finds out that a
 * scb is done.
 */
void
PH_ScbCompleted(sp_struct *scb_p)
{
	scsi_request_t	*req_p;
	adp78_adap_t *ad_p;
	adp78_ha_t *ha_p;

	ha_p = scb_p->Sp_OSspecific;
	ASSERT(ha_p);
	ASSERT(ha_p->ha_req);

	if (adp_verbose)
		printf("PH_ScbCompleted: ha_p 0x%x scb_ptr 0x%x SP_Stat 0x%x\n",
		       ha_p, scb_p, scb_p->SP_Stat);

	req_p = ha_p->ha_req;
	ad_p = adp78_array[req_p->sr_ctlr];
	ASSERT(ad_p);

	switch(scb_p->SP_Stat) {
	case SCB_COMP	:
		req_p->sr_status = SC_GOOD;
		req_p->sr_scsi_status = 0;
		req_p->sr_sensegotten = 0;
		req_p->sr_resid = 0;
		break;

	case SCB_ABORTED:
		if (adp_verbose) {
			printf("SCSI command aborted\n");
			dump_cdb(req_p->sr_command, req_p->sr_cmdlen);
		}

		ha_p->ha_req->sr_status = SC_HARDERR;
		ha_p->ha_req->sr_scsi_status = 0;
		ha_p->ha_req->sr_ha_flags = 0;
		ha_p->ha_req->sr_sensegotten = 0;
		ha_p->ha_req->sr_resid = ha_p->ha_req->sr_buflen;
		break;


	case SCB_ERR:
		req_p->sr_status = convert_hastatus(scb_p->SP_HaStat);
		req_p->sr_scsi_status = scb_p->SP_TargStat;
		req_p->sr_sensegotten = 0;
		req_p->sr_resid = req_p->sr_buflen;

		/*
		 * Ph_OptimaRequestSense and Ph_OptimaAbortActive
		 * over-writes the scb.  This means that the phys addr of
		 * the s/g list must be put back into SP_SegPtr
		 */
#ifdef IP32
		scb_p->SP_SegPtr = kvtophys(ha_p->ha_sg) | PCI_NATIVE_VIEW;
#else
		scb_p->SP_SegPtr = kvtophys(ha_p->ha_sg);
#endif

		if (scb_p->SP_TargStat == UNIT_CHECK) {
			req_p->sr_sensegotten = req_p->sr_senselen;
			req_p->sr_status = 0;
			req_p->sr_scsi_status = 0;

			/*
			 * The target is in contingent allegiance.
			 * If the command was tagged, and QERR is set then
			 * abort all other tagged commands for this target.
			 * The current plan is all drives on new SGI systems
			 * have qerr=0. So don't abort queued cmds.
			 */
#ifdef qerr1
			if (SCSI_REQ_IS_TAGGED(req_p->sr_tag))
				ha_qerr_abort(ad_p, req_p->sr_target);
#endif

			if (ad_p->adp_sense_callback[req_p->sr_target][req_p->sr_lun])
			  (*ad_p->adp_sense_callback[req_p->sr_target][req_p->sr_lun])
			    (req_p->sr_lun_vhdl, req_p->sr_sense);

			/*
			 * The Adaptec chip does not tell us how many
			 * bytes of sense were actually read in.  So I
			 * just have to print out the whole thing.
			 */
			if (adp78_printsense) {
				int k;
				printf("SCSI adp78 got sense on (%d,%d)\n",
				       req_p->sr_ctlr, req_p->sr_target);
				printf("command: ");
				dump_cdb(req_p->sr_command, req_p->sr_cmdlen);
				printf("sense buffer 0x%x\n", req_p->sr_sense);
				printf("sense info ");
				for (k=0; k < req_p->sr_sensegotten; k++) {
					printf("0x%x ",req_p->sr_sense[k]);
				}
				printf("\n");
			}
			break;
		}

		if ((req_p->sr_status == SC_HARDERR) &&
		    (scb_p->SP_TargStat == ST_BUSY)) {
 			req_p->sr_status = SC_GOOD;
 			req_p->sr_scsi_status = ST_BUSY;
 			break;
		}

		if ((req_p->sr_status == SC_HARDERR) &&
		    (scb_p->SP_HaStat == HOST_DU_DO)) {
			if (adp78_print_over_under_run) {
				printf("overrun or underrun on (%d,%d) cmd 0x%x\n",
				       ha_p->ha_id,
				       ((UBYTE) scb_p->SP_Tarlun) >> 4,
				       scb_p->Sp_CDB[0]);
				if (scb_p->SP_ResCnt == -1)
					printf("target still in data phase after host data buffer exhausted.\n");
				else
					printf("target moved to status phase before host data buffer exhausted.\n");
			}
			
			req_p->sr_resid = scb_p->SP_ResCnt;      /* zero if overrun */

			if (scb_p->SP_ResCnt != 0) {           /* we have an underrun */
			  req_p->sr_status = SC_GOOD;
			  req_p->sr_scsi_status = 0;
			  req_p->sr_sensegotten = 0;
			}

			else {                                 /* here if overrun */
			cmn_err_tag(3, CE_ALERT, "SCSI hard error - overrun on (%d,%d). scb 0x%x",
				ha_p->ha_id,
				((UBYTE) scb_p->SP_Tarlun) >> 4, scb_p);
			}
			break;
		}

		if ((req_p->sr_status == SC_HARDERR) &&
		    (scb_p->SP_HaStat != HOST_DU_DO)) {
			cmn_err(CE_ALERT, "SCSI hard error on (%d,%d). scb 0x%x",
				ha_p->ha_id,
				((UBYTE) scb_p->SP_Tarlun) >> 4, scb_p);
		}

		break;

	default:
#ifdef DEBUG
		printf("PH_ScbCompleted: unknown code 0x%x\n",
			scb_p->SP_Stat);
#endif
		req_p->sr_status = SC_HARDERR;
		req_p->sr_scsi_status = 0;
		req_p->sr_sensegotten = 0;
		break;
	}

	/*
	 * Take the ha off the busy queue and put it on the done queue.
	 * adp78intr or adp_poll_status will sr_notify the caller.
	 */
	ad_p->ad_haq_ospl = LOCK(ad_p->ad_haq_lk, IPLFUNC);
	ha_deq_ha(&ad_p->ad_busyha, ha_p);
	ha_enq_tail(&ad_p->ad_doneha, ha_p, HAQ_DONE);
	UNLOCK(ad_p->ad_haq_lk, ad_p->ad_haq_ospl);


	if (ad_p->ad_stats && (scb_p->SP_Stat == SCB_COMP))
		adp_update_stats(ad_p, req_p);
}


/*
 * There are race conditions in the order qout_ptr_array is
 * checked and the command complete interrupt is cleared.
 * So occasionally check for completed commands if not
 * already in HIM code.
 */
void
adp_check_intstat(adp78_adap_t *ad_p)
{
        if (MTRYLOCK(ad_p->ad_him_lk, s))
	  {
	    PH_IntHandler(ad_p->ad_himcfg);
	    MUNLOCK(ad_p->ad_him_lk, s);
	  }
}


/*
 * This function is used by both lower layer (HIM) and upper layer
 * (usually dksc or scsi_stat) to request a bus reset.
 * If the request is from lower layer, from_upper should be set to 0.
 * If the request is from upper layer, from_upper should be set to 1.
 */
void
adp_reset(adp78_adap_t *ad_p, int from_upper, int notify_user)
{
	u_char adap = ad_p->ad_ctlrid;

	ad_p->ad_adap_ospl = LOCK(ad_p->ad_adap_lk, IPLFUNC);
	if (ad_p->ad_flags & ADF_RESET_IN_PROG) {
		UNLOCK(ad_p->ad_adap_lk, ad_p->ad_adap_ospl);
		return;
	}
	ad_p->ad_flags |= ADF_RESET_IN_PROG | ADF_RESET | ADF_RESET_PART_1;
	UNLOCK(ad_p->ad_adap_lk, ad_p->ad_adap_ospl);

	if (notify_user)
		cmn_err_tag(4, CE_ALERT, "SCSI bus reset on controller %d.", adap);

	/*
	 * If the reset request came from the upper layers, then
	 * lock the HIM before going in.  If reset was requested
	 * by HIM, then HIM is already locked on its way in from 
	 * adp78intr, and will be unlocked on its way out.
	 */
	if (from_upper) {
		MLOCK(ad_p->ad_him_lk, s);
		PH_Pause(ad_p->ad_himcfg->Cf_base_addr);
		PH_abort_seq_cmds(ad_p->ad_himcfg);
		PH_reset_scsi_bus(ad_p->ad_himcfg, 1);
		MUNLOCK(ad_p->ad_him_lk, s);
	} else {
		PH_abort_seq_cmds(ad_p->ad_himcfg);
		PH_reset_scsi_bus(ad_p->ad_himcfg, 1);
	}

	/*
	 * Set the timer for 70ms and return.
	 */
	itimeout(adp_reset_timer, ad_p, ADP_RESET_T1, plbase);
}


void
adp_reset_timer(adp78_adap_t *ad_p)
{
	if (ad_p->ad_flags & ADF_RESET_PART_1) {

		/*
		 * We've held RST on the SCSI bus for about 70ms now.
		 * Release RST and wait 250ms before trying to do
		 * anything on the SCSI bus.
		 */

		ad_p->ad_adap_ospl = LOCK(ad_p->ad_adap_lk, IPLFUNC);
		ad_p->ad_flags &= ~ADF_RESET_PART_1;
		ad_p->ad_flags |= ADF_RESET_PART_2;
		UNLOCK(ad_p->ad_adap_lk, ad_p->ad_adap_ospl);

		MLOCK(ad_p->ad_him_lk, s);
		PH_reset_scsi_bus(ad_p->ad_himcfg, 0);
		PH_reset_him(ad_p->ad_himcfg);
		PH_UnPause_hard(ad_p->ad_himcfg->Cf_base_addr);
		MUNLOCK(ad_p->ad_him_lk, s);

		/*
		 * Now set the timer for delaying commands after reset.
		 * This value is defaulted to 1 sec. and configurable
		 * from master.d/adp78
		 */
		itimeout(adp_reset_timer, ad_p, adp78reset_delay, plbase);

	} else if (ad_p->ad_flags & ADF_RESET_PART_2) {

		/*
		 * We've waited for adp78reset_delay after deassertion of RST.
		 * Now we can do stuff again.
		 */

		adp_reset_cmds(ad_p);

		ad_p->ad_adap_ospl = LOCK(ad_p->ad_adap_lk, IPLFUNC);
		ad_p->ad_flags &= ~(ADF_RESET_IN_PROG | ADF_RESET_PART_2);
		UNLOCK(ad_p->ad_adap_lk, ad_p->ad_adap_ospl);
		ad_p->ad_resettime = lbolt;
		if (adp_verbose)
			printf("reset done %d busymap 0x%x tagholdmap 0x%x\n\n\n",
			       ad_p->ad_resettime, ad_p->ad_busymap, ad_p->ad_tagholdmap);

		/*
		 * Its a brand new day.  Start issuing cmds again.
		 */
		adp_issue_cmds(ad_p);
	}
}


/*
 * Do maintainence functions on the controller.
 * - Check for timed out commands.
 * - There is a chance that an abnormal condition is overlooked in
 *   PH_IntHandler.  Check the intstat register periodically.
 * - Check for commands that need to be returned.  This must be done for 
 *   a couple of reasons:
 *     +  If a req is rejected because its sr_buflen is too large, it is
 *        put on the doneq.
 *     +  adp_check_intstat() can put commands on the doneq.
 */
void
adp_timer(adp78_adap_t *ad_p)
{
	itimeout(adp_timer, ad_p, ADP_TIMER_VAL, plbase);
	adp_timeout_cmds(ad_p);
	adp_check_intstat(ad_p);
	adp_return_cmds(ad_p);
}


/*
 * =========================================================================
 *
 * Statistics functions
 *
 * =========================================================================
 */

static uint start_block, end_block, curr_block, iosize, outstanding_cmds;

/*
 * When a command is finished, this function is called.
 * Just keep track of how many outstanding (striping) cmds are
 * left and release the semaphore when all are in.
 */
static void
perf_test_done(scsi_request_t *req_p)
{
	outstanding_cmds--;

	if (outstanding_cmds == 0)
		vsema(req_p->sr_dev);
}


/*
 * Initialize the common parts of the scsi request structure
 */
static void
init_scsi_req(scsi_request_t *req_p, int adap, int targ, int tags, int iosize)
{
	req_p->sr_command = kmem_zalloc(10, KM_SLEEP);
	req_p->sr_sense = kmem_zalloc(64, KM_SLEEP|KM_PHYSCONTIG|KM_CACHEALIGN);
	req_p->sr_senselen = 64;

	req_p->sr_notify = perf_test_done;
	req_p->sr_command[0] = 0x28;
	req_p->sr_command[1] = 0 << 5;

	req_p->sr_command[6] = (iosize >> 16) & 0xff;
	req_p->sr_command[7] = (iosize >> 8) & 0xff;
	req_p->sr_command[8] = iosize & 0xff;
	req_p->sr_cmdlen = SC_CLASS1_SZ;
	req_p->sr_timeout = 10 * HZ;
	req_p->sr_ctlr = adap;
	req_p->sr_target = targ;
	req_p->sr_lun = 0;

	req_p->sr_buflen = iosize * 512;
	req_p->sr_flags = SRF_MAP;

	if (tags > 1)
		req_p->sr_tag = SC_TAG_SIMPLE;
}


int
adp_perftest(int perf_adap, int start, int block_count, int incr, int disks)
{
	int s, i, alloc_flags;
	char *buf;
	sema_t *perf_test_sema;
	scsi_request_t *req_array;
	int adap;

	adap = perf_adap;
	iosize = incr;
	start_block = start;
	end_block = start_block + block_count;
	curr_block = start_block;

	if (disks < 1 || disks > 4) {
		printf("disks must be between 1 and 4 (disks = %d) \n",
		       disks);
		return -1;
	}

	alloc_flags = KM_SLEEP|KM_CACHEALIGN;
		
	buf = kmem_alloc(iosize * 512, alloc_flags);
	if (buf == NULL) {
		printf("Could not allocate %d bytes\n", block_count * 512);
		return -1;
	}

	perf_test_sema = kmem_zalloc(sizeof(sema_t), KM_SLEEP);
	init_sema(perf_test_sema, 0, "perf", 0);

	req_array = (scsi_request_t *)
		kmem_zalloc(sizeof(scsi_request_t) * disks, KM_SLEEP);


	for (i=0; i < disks; i++) {
		init_scsi_req(&req_array[i], adap, i+4, 0, iosize);
		req_array[i].sr_dev = perf_test_sema;
		req_array[i].sr_buffer = (u_char *) buf;
	}

	/*
	 * Issue a batch of commands and then sleep waiting for them to be
	 * done.  When they are, issue the next batch.
	 * This adds a semaphore wakeup overhead per batch of commands,
	 * which seems quite expensive actually.  (Before, I sent out the
	 * next command from the notify routine and only used the semaphore
	 * wakeup for the end of the entire file.)  It looks like each wakeup
	 * may cost from 1 to 5ms.
	 */
	while (curr_block < end_block) {
		s = splhi();
		outstanding_cmds = disks;

		for (i=0; i < disks; i++) {
			req_array[i].sr_command[3] = (curr_block >> 16) & 0xff;
			req_array[i].sr_command[4] = (curr_block >> 8)  & 0xff;
			req_array[i].sr_command[5] = (curr_block)       & 0xff;
			adp78command(&req_array[i]);
		}

		curr_block += iosize;
		splx(s);
		psema(perf_test_sema, PRIBIO);
	}

	for (i=0; i < disks; i++)
		kmem_free(req_array[i].sr_command, 10);
	kmem_free(req_array, sizeof(scsi_request_t) * disks);
	kmem_free(buf, iosize * 512);
	freesema(perf_test_sema);
	kmem_free(perf_test_sema, sizeof(sema_t));

	return 0;
}


static void
adp_update_stats(adp78_adap_t *ad_p, scsi_request_t *req_p)
{
	uint blocks, addr;

	if (req_p->sr_command[0] != 0x28 &&
	    req_p->sr_command[0] != 0x2a) 
		return;

	blocks = req_p->sr_command[6] << 16 |
		req_p->sr_command[7] << 8 |
		req_p->sr_command[8];

	atomicAddUint(&(ad_p->ad_stats->cmds), 1);
	atomicAddUint(&(ad_p->ad_stats->blocks), blocks);

	if ((ad_p->ad_cmdbuf) && (ad_p->ad_cmdbufcur < ad_p->ad_cmdbuflen)) {
		register cmdlog_entry_t *ce_p;
		struct timeval end;

		addr = req_p->sr_command[2] << 24 |
			req_p->sr_command[3] << 16 |
			req_p->sr_command[4] << 8 |
			req_p->sr_command[5];
		
		ce_p = &(ad_p->ad_cmdbuf[ad_p->ad_cmdbufcur]);
		ce_p->cmd = req_p->sr_command[0];
		ce_p->targ = req_p->sr_target;
		ce_p->addr = addr;
		if (blocks > 0xffff)
			ce_p->blocks = 0xffff;
		else
			ce_p->blocks = blocks;
		microtime(&end);
		ce_p->sec = end.tv_sec;
		ce_p->usec = end.tv_usec;

		ad_p->ad_cmdbufcur++;
	}
}


/*
 * Copyout the stats for this adapter to the user buffer.
 * Returns -1 on error, 0 if OK.
 */
int
adp_return_stats(int adap, char *statsbuf)
{
	adp78_adap_t *ad_p;

	ad_p = adp78_array[adap];
	if (ad_p == NULL)
		return -1;

	if (copyout(ad_p->ad_stats, statsbuf, sizeof(adp_stats_t)))
		return -1;
	else
		return 0;
}


/*
 * Start or stop cmd logging.
 * Returns -1 on error, 0 if OK.
 */
/*ARGSUSED3*/
int
adp_cmdlog(int start, int adap, int targ, int num_entries, char *cmdlogbuf)
{
	adp78_adap_t	*ad_p;
	char		*buf;
	int rval=0;

	ad_p = adp78_array[adap];
	if (ad_p == NULL) {
		printf("adp_cmdlog: invalid adap number (%d)\n", adap);
		return -1;
	}

	if (start) {
		if (ad_p->ad_cmdbuf) {
			kmem_free(ad_p->ad_cmdbuf,
				  ad_p->ad_cmdbuflen * sizeof(cmdlog_entry_t));
			ad_p->ad_cmdbuf = 0;
			ad_p->ad_cmdbufcur = 0;
			ad_p->ad_cmdbuflen = 0;
		}


		buf = kmem_zalloc(num_entries * sizeof(cmdlog_entry_t),
				  KM_SLEEP);
		if (buf == NULL) {
			printf("adp_cmdlog: could not allocate %d entries\n",
			       num_entries);
			return -1;
		}

		/* initialize buffer variables.  cmdbufcur is how many we have so far */
		ad_p->ad_cmdbuf = (cmdlog_entry_t *) buf;
		ad_p->ad_cmdbufcur = 0;
		ad_p->ad_cmdbuflen = num_entries;

	} else {
		if (copyout(ad_p->ad_cmdbuf, cmdlogbuf,
			    ad_p->ad_cmdbuflen * sizeof(cmdlog_entry_t)))
			rval = -1;

		kmem_free(ad_p->ad_cmdbuf,
			  ad_p->ad_cmdbuflen * sizeof(cmdlog_entry_t));

		/* clear all buf variables */
		ad_p->ad_cmdbuf = 0;
		ad_p->ad_cmdbufcur = 0;
		ad_p->ad_cmdbuflen = 0;
	}

	return rval;
}






/*
 * =========================================================================
 *
 * Debug functions
 *
 * =========================================================================
 */

static void
dump_cdb(u_char *cdb, int len)
{
	int i;
	if (cdb == NULL) {
		printf("\n");
		return;
	}

	for (i=0; i < len; i++)
		printf("%2x ", cdb[i]);
	printf("\n");
}
