/*  Edit history:
**  01/04/95  robertn   started port to sgi.  This driver is base on
**                      the Qlogic SCO driver
*/

/******************************************************************************/

/****************************************************************/
/*								*/
/*          Compile time options                                */
/*								*/
/****************************************************************/
#define QL1240_SWAP_WAR
#define BRIDGE_B_PREFETCH_DATACORR_WAR 

#if defined(SN) || defined(IP30)
#define A64_BIT_OPERATION			/* operate ISP in 64 bit mode */
#ifndef MAPPED_SIZE
#define	MAPPED_SIZE	0x4000
#endif
#else
#define	MAPPED_SIZE	IO_NBPC
#endif /* SN || IP30 */

#define LOAD_FIRMWARE				/* define to load new firmware */
#define MAILBOX_TEST				/* test mailboxes (firmware running?) */

#ifdef SN
#define SLOT_ONE			0x01
#define QL_SRAM_PARITY			1      	/* enable sram parity checking */
#define PROBE_ALL_LUNS			1
#define BUS_RESET_ON_INIT		1
#define USE_MAPPED_CONTROL_STREAM		
#undef  USE_IOSPACE				/* Use MEM space for mapping QL registers */
#undef  LOAD_VIA_DMA						/* load firmware with DMA else a word at a time */
#undef  FLUSH		
#endif	/* SN */

#ifdef IP30
#define QL_SRAM_PARITY			1		/* enable sram parity checking */
#define PROBE_ALL_LUNS			1
#define BUS_RESET_ON_INIT		0
#define USE_MAPPED_CONTROL_STREAM
#define USE_IOSPACE				/* Use IO space for mapping QL registers */
#undef  LOAD_VIA_DMA				/* load firmware with DMA else a word at a time */

#if HEART_COHERENCY_WAR
#define  FLUSH
#endif
#endif  /* IP30 */

/****************************************************************/
/*								*/
/* DEBUG Compile time options                                   */
/* Add the following to DBOPTS in klocaldefs                    */
/*    -DPIO_TRACE     - enable QL PIO tracing                   */
/*    -DQL_INTR_DEBUG - enable "missed" interrupt debugging     */
/*    -DQL_TIMEOUT_DEBUG - enable "forced" timeout debugging    */
/*    -DQL_STATS      - include statistics gathering code       */
/*    -DSENSE_PRINT   - print sense following request sense     */
/*								*/
/****************************************************************/

/*********************************************************
*      NOTES ON HOW WE CONFIGURE THE BRIDGE FOR QL	 
*
*  There are two virtual DMA channel
*  Virtual channel zero is the Command Channel
*     This channel is pmu mapped with one rrb		
*     32 bit address are generated on the pci bus 
*     64 bit translation is done by the PMU
*
*  Virtual channel one is the data channel	
*     This channel is 64bit direct mapped with prefetching
*     and two rrbs.
*     DAC(64bit) address are generated on the pci bus
*
**********************************************************/

/**********************************************************
**
**          Include files
*/
#include "sys/types.h"
#include <sys/debug.h>
#include "sys/param.h"
#include "sys/ddi.h"

#include "sys/sema.h"  
#include "sys/iograph.h"  

#include "sys/scsi.h"
#include "sys/edt.h"
#include "sys/cpu.h"
#include "sys/atomic_ops.h"
#include "sys/PCI/bridge.h"
#include "sys/PCI/PCI_defs.h"
#include "sys/PCI/pciio.h"
#include "sys/time.h"
#include "sys/scsi_stats.h"

#include <sys/conf.h>
#include <sys/var.h>

int	qldevflag = D_MP | D_ASYNC_ATTACH;

#include "sys/buf.h"
#include "sys/pfdat.h"
#include "sys/dkio.h"
#include <sys/invent.h>
#include <sys/mtio.h>
#include "sys/tpsc.h"
#include "sys/cmn_err.h"
#include "sys/sysmacros.h"
#include "sys/kmem.h"
#include "sys/failover.h"
#include "sys/driver.h"
#include "sys/immu.h"
#include "ksys/hwg.h"

#ifdef DEBUG
#include <sys/debug.h>
#endif


#include "sys/systm.h"
#include "sys/dir.h"
#include "ksys/vfile.h"
#include "sys/xlate.h"
#include "sys/immu.h"
#include "sys/errno.h"


#ifdef FLUSH
#include "ksys/cacheops.h"
#if HEART_COHERENCY_WAR
/* flush both memory reads and writes */
#define MR_DCACHE_WB_INVAL(a,l)		heart_dcache_wb_inval(a,l)
#define MRW_DCACHE_WB_INVAL(a,l)	heart_write_dcache_wb_inval(a,l)
#define DCACHE_INVAL(a,l)		heart_dcache_inval(a,l)
#else
#define MR_DCACHE_WB_INVAL(a,l)		__dcache_wb_inval(a,l)
#define MRW_DCACHE_WB_INVAL(a,l)	__dcache_wb_inval(a,l)
#define DCACHE_INVAL(a,l)		__dcache_inval(a,l)
#endif
#endif /* FLUSH */

#if defined(IP30) || defined(SN)
#include "sys/PCI/pcibr.h"
#endif

#include <sys/alenlist.h>
#include <sys/pci_intf.h>

#include <sys/ql_firmware.h>   /* ISP firmware */

#include <sys/ql.h>   /* structure definitions for adapter */

#ifdef PIO_TRACE
ushort
get_ql_reg(pHA_INFO ha, volatile u_short *rp) {
	register uint *logp, log_cnt;
	u_short	sval;
	sval = *rp;
	if (ha && (logp = ha->chip_info->ql_log_vals) && ((((__psint_t)rp & 0x7fff) != 0x0008) || (sval != 0))) {
	    if ((log_cnt = *logp) == 0)
		log_cnt = 1;
	    logp[log_cnt++] = (((__psint_t)rp & 0x7fff) << 16) | sval;
	    if (log_cnt >= QL_LOG_CNT)
		log_cnt = 1;
	    *logp = log_cnt;
	}
	return sval;
}

void
set_ql_reg(pHA_INFO ha, volatile u_short *rp, u_short sval) {
	register uint *logp, log_cnt;
	if (ha && (logp = ha->chip_info->ql_log_vals)) {
	    if ((log_cnt = *logp) == 0)
		log_cnt = 1;
	    logp[log_cnt++] = (((__psint_t)rp & 0x7fff) << 16) | (1 << 31) | sval;
	    if (log_cnt >= QL_LOG_CNT)
		log_cnt = 1;
	    *logp = log_cnt;
	}
	*rp = sval;
}

#define	QL_PCI_INH(x)		get_ql_reg(ha, x)
#define	QL_PCI_OUTH(x,y)	set_ql_reg(ha, x, y)
#else
#define	QL_PCI_INH(x)		PCI_INH(x)
#define	QL_PCI_OUTH(x,y)	PCI_OUTH(x,y)

#endif /* PIO_TRACE */

#define KILL_WITH_TIMEOUT	1
#define KILL_WITHOUT_TIMEOUT	0


/****************************************************************/
/*								*/
/* Function Prototypes						*/
/*								*/
/****************************************************************/

/* Primary entry points */

int			qlalloc(vertex_hdl_t, int, void (*)());
void			qlcommand(struct scsi_request *req);
void			qlfree(vertex_hdl_t, void (*)());
int			qldump(vertex_hdl_t);
struct scsi_target_info	*qlinfo(vertex_hdl_t ql_lun_vhdl);
int			qlioctl(vertex_hdl_t ql_ctlr_vhdl, uint cmd, scsi_ha_op_t *op);
int			qlabort(scsi_request_t *scsi_request);
#ifdef IP30
int			qlreset(vertex_hdl_t ql_ctlr_vhdl);
#endif
void			qlintr(pCHIP_INFO);
void			qlinit(void);
int			qlattach(vertex_hdl_t);
int			qldetach(struct pci_record *, edt_t *, __int32_t);
int			qlerror(struct pci_record *, edt_t *, __int32_t);
void			qlhalt(void);

/* Locally used routines */
static scsi_target_info_t *	ql_scsi_targ_info_init(void);
static ql_local_info_t *	ql_local_info_init(void);
static vertex_hdl_t		ql_device_add(vertex_hdl_t, int, int);
static void			ql_device_remove(vertex_hdl_t, int, int);
pHA_INFO			ql_ha_info_from_ctlr_get(vertex_hdl_t);
static pHA_INFO			ql_ha_info_from_lun_get(vertex_hdl_t);
static void			ql_ha_info_put(vertex_hdl_t, pHA_INFO);
static scsi_target_info_t *	ql_lun_target_info_get(vertex_hdl_t);
static int			ql_init(vertex_hdl_t , vertex_hdl_t, pPCI_REG, pISP_REGS);
static int			ql_init_board(vertex_hdl_t , vertex_hdl_t, pPCI_REG, pISP_REGS);
static int			ql_init_isp(pCHIP_INFO);
static void			ql_enable_intrs(pCHIP_INFO);
#ifdef QL_SRAM_PARITY
static int			ql_enable_sram_parity(pCHIP_INFO, int);
#endif
static int			ql_entry(scsi_request_t *);
static void			ql_service_interrupt(pCHIP_INFO);
static void			ql_notify_responses(pCHIP_INFO, scsi_request_t *);
static void			ql_start_scsi(pCHIP_INFO);
static void			ql_queue_space(pCHIP_INFO, pISP_REGS);
static int			ql_mbox_cmd(pCHIP_INFO, u_short *, u_char, u_char,
					    u_short, u_short, u_short, u_short,
					    u_short, u_short, u_short, u_short);
static void			qldone(scsi_request_t *);
static int			ql_reset(vertex_hdl_t);
static void			ql_quiesce_bus(pHA_INFO, long);
static vertex_hdl_t		ql_ctlr_add(vertex_hdl_t, int);
static void			fill_sg_elem(pHA_INFO, data_seg *, alenaddr_t, size_t, int);
static void			munge(u_char *, int);
#ifdef QL1240_SWAP_WAR	
static void			munge_iocb(u_char *, int);
#endif
static void			ql_set_defaults(pHA_INFO);
#ifdef QL_STATS
static void			add_deltatime(struct timeval *, struct timeval *, struct timeval *);
#endif
static void			scan_bus(vertex_hdl_t);
static int			reqi_get_slot(pHA_INFO, int);
static void			kill_quiesce(pHA_INFO, int);
static int			ql_download_fw(pCHIP_INFO);
static int			ql_reset_interface(pCHIP_INFO, int);
static void			ql_flush_queue(pHA_INFO, uint);
static void			set_new_timebase(pHA_INFO, int);
#ifdef QL_INTR_DEBUG
static void			ql_interrupt_timer(void);
#endif
static void			ql_watchdog_timer(void);
static int			ql_error_handler(void *, int, ioerror_mode_t, ioerror_t *);
static void			ql_service_mbox_interrupt(pCHIP_INFO);
static int			ql_diffsense(pHA_INFO);
static int			ql_probe_bus(pHA_INFO, int, int);
static void			ql_cpmsg(dev_t, char *, ...);
static void			ql_tpmsg(dev_t, u_char, char *, ...);
static void			ql_lpmsg(dev_t, u_char, u_char, char *, ...);
static void			DBG(int level, char *format, ...);
static char *			ql_completion_status_msg(uint completion_status);
static void			ql_sensepr(vertex_hdl_t, int, u_char *, int);
static int			ql_set_bus_reset(pCHIP_INFO, u_short *, u_short);
static int			ql_set_scsi_id(pCHIP_INFO, u_short *);
static int			ql_set_selection_timeout(pCHIP_INFO, u_short *);
static int			ql_set_retry_count(pCHIP_INFO, u_short *);
static int			ql_set_tag_age_limit(pCHIP_INFO, u_short *);
static int			ql_set_clock_rate(pCHIP_INFO, u_short *);
static int			ql_set_negation_state(pCHIP_INFO, u_short *);
static int			ql_set_asynchronous_data_setup_time(pCHIP_INFO, u_short *);
static int			ql_set_pci_control_parameters(pCHIP_INFO, u_short *);
static int			ql_set_scsi_parameters(pCHIP_INFO, u_short *, u_short, u_short);
static int			ql_set_device_queue_parameters(pCHIP_INFO, u_short *, u_short, u_short, u_short);
static int			ql_set_reset_delay_parameters(pCHIP_INFO, u_short *);
static int			ql_set_system_parameters(pCHIP_INFO, u_short *);
static int			ql_set_firmware_features(pCHIP_INFO, u_short *);
#if 0
static int			ql_set_host_data(pCHIP_INFO, u_short *);
#endif
static int			ql_set_data_over_recovery_mode(pCHIP_INFO, u_short *);
static int			ql_set_abort_device(pCHIP_INFO, u_short *, u_short, u_short, u_short);
#ifdef DEBUG
static int			ql_get_firmware_status(pCHIP_INFO, u_short *);
static int			ql_get_scsi_id(pCHIP_INFO, u_short *);
static int			ql_get_selection_timeout(pCHIP_INFO, u_short *);
static int			ql_get_retry_count(pCHIP_INFO, u_short *);
static int			ql_get_tag_age_limit(pCHIP_INFO, u_short *);
static int			ql_get_clock_rate(pCHIP_INFO, u_short *);
static int			ql_get_negation_state(pCHIP_INFO, u_short *);
static int			ql_get_asynchronous_data_setup_time(pCHIP_INFO, u_short *);
static int			ql_get_pci_control_parameters(pCHIP_INFO, u_short *);
#endif
static int 			ql_get_scsi_parameters(pCHIP_INFO, u_short *, u_short, u_short);
#ifdef DEBUG
static int			ql_get_device_queue_parameters(pCHIP_INFO, u_short *, u_short, u_short, u_short);
static int			ql_get_reset_delay_parameters(pCHIP_INFO, u_short *);
static int			ql_get_system_parameters(pCHIP_INFO, u_short *);
static int			ql_get_firmware_features(pCHIP_INFO, u_short *);
static int			ql_get_host_data(pCHIP_INFO, u_short *);
static int			ql_get_data_over_recovery_mode(pCHIP_INFO, u_short *);
#endif
#ifdef DEBUG
static void			dump_registers(pCHIP_INFO, pISP_REGS);
static void			dump_parameters(vertex_hdl_t);
static void			dump_iocb(command_entry *);
static void			dump_iorb(status_entry *);
static void			dump_continuation(continuation_entry *);
static void			dump_marker(marker_entry *);
static void			dump_cmd_dma(pHA_INFO);
#endif
#ifdef BRIDGE_B_DATACORR_WAR
int				ql_bridge_rev_b_war(vertex_hdl_t);
#endif

#define QL_CPMSGS   if (showconfig) ql_cpmsg
#define QL_TPMSGS   if (showconfig) ql_tpmsg

#if defined(IP30) || defined(SN)
#define MAKE_DIRECT_MAPPED_2GIG(x) ((x) | 0x80000000)
#define CLEAR_DIRECT_MAPPED_2GIG(x) ((x) & 0x7fffffff )
#else
#define MAKE_DIRECT_MAPPED_2GIG(x) (x)
#define CLEAR_DIRECT_MAPPED_2GIG(x) (x)
#endif

#define	QL_KMEM_ZALLOC(_size, _flags)	kmem_zalloc((_size), (_flags))
#define QL_KVPALLOC(_s, _f, _c)		kvpalloc((_s), (_f), (_c))

#ifdef SN
#define TIMEOUT_SPL     plbase
#else /* !SN */
#define TIMEOUT_SPL     plhi
#endif /* !SN */

/****************************************************************/
/*								*/
/*          Global Kernel Information                           */
/*								*/
/****************************************************************/
static int	ql_pci_slot_number ; 
pHA_INFO	ql_ha_list ;
static mutex_t	ql_driver_lock;
static int	ql_watchdog_timer_inited;


/* extern from os/kopt.c */
extern char	arg_qlhostid[];

/* extern from master.d/ql */
extern int	ql_bus_reset_on_boot;
extern int 	ql_bus_reset_delay;
extern int	ql_allow_negotiation_on_cdroms;
extern int	ql_printsense;

static int     	ql_debug  = 0;

#undef QL_WATCHDOG_TIMER 

#undef QL_TIMEOUT_DEBUG 

#ifdef QL_TIMEOUT_DEBUG
#define QL_TIMEOUT_DEBUG_DEFAULT_FREQ 100
static int ql_timeout_debug_lunvhdl = 4;
static int ql_timeout_debug_timer = QL_TIMEOUT_DEBUG_DEFAULT_FREQ;
static int ql_timeout_debug_frequency = QL_TIMEOUT_DEBUG_DEFAULT_FREQ;
#endif

/****************************************************************/
/****************************************************************/
/*
 * create the info structure associated with a 
 * lun vertex
 */
static scsi_target_info_t *
ql_scsi_targ_info_init(void)
{
	scsi_target_info_t 	*info;

	/*
	 * allocate memory
	 */
	info = (scsi_target_info_t *)kern_calloc(1,sizeof(scsi_target_info_t));
	ASSERT(info);
	return(info);
}

/*
 * allocate and initialize the ql specific part of lun info
 */
static ql_local_info_t *
ql_local_info_init(void)
{
	ql_local_info_t 	*info;

	info 	= (ql_local_info_t *)kern_calloc(1,sizeof(*info));
	ASSERT(info);

	/*
	 * By default assume the QERR bit of a device is set to 0,
	 * unless told otherwise. It's safer that way since assuming
	 * the opposite could lead to command timeouts.
	 * However, the disk driver is setting the QERR bit depends on the 
  	 * support of the target.  Setting the QERR to 0 is the default 

	info->qli_dev_flags = DFLG_BEHAVE_QERR1;
	 */

	return info;
}

/*
 * add a ql device vertex to the hardware graph 
 */
static vertex_hdl_t
ql_device_add(vertex_hdl_t	ctlr_vhdl,
	      int		targ,
	      int		lun)
{
	vertex_hdl_t		lun_vhdl;
	vertex_hdl_t		targ_vhdl;
	scsi_lun_info_t		*lun_info;
	char			*dev_admin;
	pHA_INFO                ha = ql_ha_info_from_ctlr_get(ctlr_vhdl);

	/*
	 * add the target vertex
	 */
	
	targ_vhdl = scsi_targ_vertex_add(ctlr_vhdl,targ);

	/* 
	 * check for DEVICE_ADMIN variables that are set on a per 
	 * device basis
	 */
	if (ha->chip_info->revision > REV_ISP1040A) 
	{
		if (dev_admin = device_admin_info_get(targ_vhdl, "ql_disconnect_allowed"))
		{
			ha->ql_defaults.Id[targ].Capability.bits.Disconnect_Allowed = atoi(dev_admin) & 0x1;
			QL_TPMSGS(ha->ctlr_vhdl, targ, 
				  "DEVICE ADMIN: ql_disconnect_allowed changed to %d.\n",
				  ha->ql_defaults.Id[targ].Capability.bits.Disconnect_Allowed);
		}
	
		if (dev_admin = device_admin_info_get(targ_vhdl, "ql_ctq_enable"))
		{
			ha->ql_defaults.Id[targ].Capability.bits.Tagged_Queuing = atoi(dev_admin) & 0x1;
			QL_TPMSGS(ha->ctlr_vhdl, targ, 
				  "DEVICE ADMIN: ql_ctq_enable changed to %d.\n",
				  ha->ql_defaults.Id[targ].Capability.bits.Tagged_Queuing);
		}
	}

        if (dev_admin = device_admin_info_get(targ_vhdl, "ql_sync_enable"))
        {
		ha->ql_defaults.Id[targ].Sync_Allowed = atoi(dev_admin) & 0x1;
		ha->ql_defaults.Id[targ].Capability.bits.Sync_Data_Transfers = 0;
		QL_TPMSGS(ha->ctlr_vhdl, targ, 
			  "DEVICE ADMIN: ql_sync_enable changed to %d.\n",
			  ha->ql_defaults.Id[targ].Sync_Allowed);
        }
	
	if (dev_admin = device_admin_info_get(targ_vhdl, "ql_force_sync_negotiation"))
        {
		ha->ql_defaults.Id[targ].Force_Sync = atoi(dev_admin) & 0x1;
		QL_TPMSGS(ha->ctlr_vhdl, targ, 
			  "DEVICE ADMIN: ql_force_sync_negotiation changed to %d.\n",
			  ha->ql_defaults.Id[targ].Force_Sync);
        }
	else
		ha->ql_defaults.Id[targ].Force_Sync = 0;

	if (!(ha->host_flags & AFLG_HA_FAST_NARROW)) 
	{
		if (dev_admin = device_admin_info_get(targ_vhdl, "ql_wide_enable"))
		{
			ha->ql_defaults.Id[targ].Wide_Allowed = atoi(dev_admin) & 0x1;
			ha->ql_defaults.Id[targ].Capability.bits.Wide_Data_Transfers = 0;
			QL_TPMSGS(ha->ctlr_vhdl, targ, 
				  "DEVICE ADMIN: ql_wide_enable changed to %d.\n",
				  ha->ql_defaults.Id[targ].Wide_Allowed);
		}

		if (dev_admin = device_admin_info_get(targ_vhdl, "ql_sync_period"))
		{
			ha->ql_defaults.Id[targ].Sync_Period = atoi(dev_admin);
			if (ha->chip_info->clock_rate == FAST20_CLOCK_RATE) {
				if (	(ha->ql_defaults.Id[targ].Sync_Period != 12) &&
			    		(ha->ql_defaults.Id[targ].Sync_Period != 16) &&
			    		(ha->ql_defaults.Id[targ].Sync_Period != 20) &&
			    		(ha->ql_defaults.Id[targ].Sync_Period != 25) &&
			    		(ha->ql_defaults.Id[targ].Sync_Period != 29) &&
			    		(ha->ql_defaults.Id[targ].Sync_Period != 33) &&
			    		(ha->ql_defaults.Id[targ].Sync_Period != 37) &&
			    		(ha->ql_defaults.Id[targ].Sync_Period != 41) &&
			    		(ha->ql_defaults.Id[targ].Sync_Period != 45) &&
			    		(ha->ql_defaults.Id[targ].Sync_Period != 50) &&
			    		(ha->ql_defaults.Id[targ].Sync_Period != 54) &&
			    		(ha->ql_defaults.Id[targ].Sync_Period != 58) &&
			    		(ha->ql_defaults.Id[targ].Sync_Period != 62) &&
			    		(ha->ql_defaults.Id[targ].Sync_Period != 66))
				{
					ha->ql_defaults.Id[targ].Sync_Period = SYNC_PERIOD_FAST20;
				}
			} else {
            			if (	(ha->ql_defaults.Id[targ].Sync_Period != 10) &&
                            		(ha->ql_defaults.Id[targ].Sync_Period != 12) &&
                            		(ha->ql_defaults.Id[targ].Sync_Period != 25) &&
                            		(ha->ql_defaults.Id[targ].Sync_Period != 27) &&
                            		(ha->ql_defaults.Id[targ].Sync_Period != 30) &&
                            		(ha->ql_defaults.Id[targ].Sync_Period != 32) &&
                            		(ha->ql_defaults.Id[targ].Sync_Period != 35) &&
                            		(ha->ql_defaults.Id[targ].Sync_Period != 37) &&
                            		(ha->ql_defaults.Id[targ].Sync_Period != 40))
                        	{
                                        ha->ql_defaults.Id[targ].Sync_Period = SYNC_PERIOD_FAST40;
                        	}
			}
                       	QL_TPMSGS(ha->ctlr_vhdl, targ, "DEVICE ADMIN: ql_sync_period changed to %d.\n",
                               ha->ql_defaults.Id[targ].Sync_Period);

		}
	}

	/*
	 * add the lun vertex
	 */

	lun_vhdl = scsi_lun_vertex_add(targ_vhdl,lun);
	
	/*
	 * get the lun info pointer
	 */

	lun_info = scsi_lun_info_get(lun_vhdl);

	/*
	 * initialize the ql specific lun info 
	 */

	SLI_INFO(lun_info) = ql_local_info_init();

	init_mutex(&OPEN_MUTEX(lun_info),MUTEX_DEFAULT,"ql_open", lun_vhdl);
	init_mutex(&LUN_MUTEX(lun_info),MUTEX_DEFAULT,"ql_lun", lun_vhdl);

	TINFO(lun_info)	= ql_scsi_targ_info_init();
	return lun_vhdl;
}
	
/*
 * remove the ql device vertex and the path leading to it from
 * controller vertex ( target/#/lun/#).
 * 
 * the basic idea is to remove the last vertex in target/#/lun/# subpath.
 * if this is the last vertex to be removed we don't need the "lun" part of
 * the path and we can remove that. this part of the logic is handled by
 * scsi_lun_vertex_remove
 * 
 * if the "lun" vertex of the above path has been removed 
 * scsi_lun_vertex_remove will return GRAPH_VERTEX_NONE in which case
 * we have targ/# subpath left and we can go ahead and remove the last
 * vertex corr. to specific target number.
 * as before we then check if there are any specific target number vertices.
 * if there aren't any we can remove the "target" vertex also. this part of
 * the logic is handled by scsi_targ_vertex_remove
 *
 */

static void
ql_device_remove(vertex_hdl_t 	ctlr_vhdl,
		 int		targ,
		 int		lun)
{
	vertex_hdl_t	 targ_vhdl, lun_vhdl;
	/* REFERENCED */
	graph_error_t	 rv;
	char		 loc_str[20];
	scsi_lun_info_t	*lun_info;

 	sprintf(loc_str, TARG_NUM_FORMAT, targ);
	rv = hwgraph_traverse(ctlr_vhdl, loc_str, &targ_vhdl);
	ASSERT(rv == GRAPH_SUCCESS);
	hwgraph_vertex_unref(targ_vhdl);
	/*
	 * Cleanup the ql specific lun info 
	 */
	sprintf(loc_str, LUN_NUM_FORMAT, lun);
	rv = hwgraph_traverse(targ_vhdl, loc_str, &lun_vhdl);
	ASSERT((rv == GRAPH_SUCCESS) && (lun_vhdl != GRAPH_VERTEX_NONE));
	hwgraph_vertex_unref(lun_vhdl);
	lun_info = scsi_lun_info_get(lun_vhdl);
	mutex_destroy(&OPEN_MUTEX(lun_info));
	mutex_destroy(&LUN_MUTEX(lun_info));
	kern_free(TINFO(lun_info));
	kern_free(SLI_INFO(lun_info));
	SLI_INFO(lun_info) = NULL;
	
	if (scsi_lun_vertex_remove(targ_vhdl,lun) == GRAPH_VERTEX_NONE)
		(void)scsi_targ_vertex_remove(ctlr_vhdl,targ);
}


/*
 * get the pointer to host adapter info given the
 * vertex handle of the vertex  corr. to the 
 * controller
 */
pHA_INFO
ql_ha_info_from_ctlr_get(vertex_hdl_t 	ctlr_vhdl)
{
	scsi_ctlr_info_t		*ctlr_info;

	if (ctlr_vhdl == GRAPH_VERTEX_NONE)
		return NULL;
	ctlr_info = scsi_ctlr_info_get(ctlr_vhdl);
	ASSERT(ctlr_info);
	return(SCI_HA_INFO(ctlr_info));
}

/* 
 * get the pointer to host adapter info given the
 * ql device vertex handle
 */
static pHA_INFO
ql_ha_info_from_lun_get(vertex_hdl_t 	lun_vhdl)
{

	scsi_lun_info_t		*lun_info;

	lun_info = scsi_lun_info_get(lun_vhdl);
	ASSERT(lun_info);
	ASSERT(SLI_CTLR_INFO(lun_info));
	
	return(SLI_HA_INFO(lun_info));

}
/*
 * put the pointer to host adapter structure into the info
 * associated with the controller vertex
 */
static void
ql_ha_info_put(vertex_hdl_t	ctlr_vhdl,
	       pHA_INFO		ha)
{
	scsi_ctlr_info_t		*ctlr_info;

	/*
	 * the info structure associated with the controller
	 * vertex has been created when the vertex was added
	 */

	ctlr_info = scsi_ctlr_info_get(ctlr_vhdl);
	ASSERT(ctlr_info);
	ASSERT(!SCI_HA_INFO(ctlr_info));

	
	SCI_INFO(ctlr_info) = ha;
}
	
/*
 * scsi target info is the info associated with the 
 * lun vertex.get this info .  
 */
static scsi_target_info_t *
ql_lun_target_info_get(vertex_hdl_t	lun_vhdl)
{
	scsi_lun_info_t		*lun_info = NULL;
	scsi_target_info_t	*tinfo;

	lun_info = scsi_lun_info_get(lun_vhdl);
	ASSERT(lun_info);
	
	tinfo = TINFO(lun_info);
	ASSERT(tinfo);
	
	return tinfo;
}

/* Entry-Point
** Function:    ql_init
**
** Description: Initialize all of the ISP's on the PCI bus.
*/

static int
ql_init(vertex_hdl_t	ql_ctlr_vhdl,
	vertex_hdl_t	ql_pci_vhdl,
	pPCI_REG	config_addr, 
	pISP_REGS 	mem_map_addr)
{

    if (!ql_watchdog_timer_inited)
    {
	    ql_ha_list = NULL;

	    ql_watchdog_timer_inited = 1;
	    itimeout(ql_watchdog_timer, NULL, WATCHDOG_TIME * HZ, TIMEOUT_SPL);
#ifdef QL_INTR_DEBUG
	    fast_itimeout(ql_interrupt_timer, NULL, 1, TIMEOUT_SPL);
#endif
    }


    /*
     ** If this Adapter has already been configured in, then ignore
     ** the request. This will take care of duplicates.
     */
    if (!ql_ha_info_from_ctlr_get(ql_ctlr_vhdl)) {
	    /*
	     ** Initialize this board.
	     ** If this fails, then it won't be marked as valid (ignore).
	     */
	    if (ql_init_board(ql_ctlr_vhdl,
			      ql_pci_vhdl,
			      config_addr, /* confiuration space address */
			      mem_map_addr))
	    {
		    return(1);
	    }
    }
    
    return(0);
}


/* Function:    ql_init_board
**
** Description: Initialize specific ISP board.
** Returns: 0 = success
**      1 = failure
*/
static int 
ql_init_board(vertex_hdl_t ql_ctlr_vhdl,
	      vertex_hdl_t ql_pci_vhdl,
	      pPCI_REG     config_addr,
	      pISP_REGS    mem_map_addr)
{
    pHA_INFO      ha=NULL;
    pCHIP_INFO    chip_info=NULL;
    pISP_REGS     Base_Address;
    device_desc_t ql_dev_desc;
    pPCI_REG	  conf = (pPCI_REG)config_addr;
    u_short       vendor_id=0;
    u_short       dev_id=0;
    u_short	  rev_id=0;
    u_short	  clock_rate;
    int           rc = 0;
    int		  channel_count=0;
    int	    	  i;

    /* Make sure this is a ISP.  */
    vendor_id = PCI_INH(&conf->Vendor_Id);
    dev_id = PCI_INH(&conf->Device_Id);
    rev_id = PCI_INB(&conf->Rev_Id);

    /* If this device is not ours, just leave.  */
    if (vendor_id == QLogic_VENDOR_ID  )
    {
	if (dev_id == QLogic_1040_DEVICE_ID) {
		channel_count = 1;
                rev_id += ISP1040_STARTING_REV_NUM;
		clock_rate = FAST20_CLOCK_RATE;
	} 
	else 
	if (dev_id == QLogic_1240_DEVICE_ID) {
		channel_count = 2;
		rev_id += ISP1240_STARTING_REV_NUM;
		clock_rate = FAST20_CLOCK_RATE;
	}
	else
	if (dev_id == QLogic_1080_DEVICE_ID) {
		channel_count = 1;
		rev_id += ISP1080_STARTING_REV_NUM;
		clock_rate = FAST40_CLOCK_RATE;
	}
	else
	if (dev_id == QLogic_1280_DEVICE_ID) {
		channel_count = 2;
		rev_id += ISP1280_STARTING_REV_NUM;
		clock_rate = FAST40_CLOCK_RATE;
	}
	else 
	{
		rc = 1;
		goto init_failed;
	}
	
    } else {
	rc = 1;
	goto init_failed;
    }

    /* Use the addresses they specified. */
    /* Note: this doesn't care whether this is a memory address */
    /* or an I/O address (its whatever they specified).  */

    Base_Address = mem_map_addr ;

    /* Allocate and init memory for the adapter structure.  */
    {
	u_int       id,req_cnt;
	pREQ_INFO   r_ptr;
	int	    i;


	chip_info = (pCHIP_INFO)QL_KMEM_ZALLOC(sizeof(CHIP_INFO),
				     VM_CACHEALIGN | VM_NOSLEEP | VM_DIRECT);


#ifdef PIO_TRACE
	chip_info->ql_log_vals = (uint *)QL_KMEM_ZALLOC(sizeof(uint) * QL_LOG_CNT,
		VM_CACHEALIGN | VM_NOSLEEP | VM_DIRECT);
#endif
	/* pci revision id */
	chip_info->revision = rev_id;

	/* pci device id */
	chip_info->device_id = dev_id;

 
#ifdef BRIDGE_B_PREFETCH_DATACORR_WAR
	chip_info->bridge_revnum = pcibr_asic_rev(ql_pci_vhdl);
#endif
	chip_info->ha_base = (caddr_t) Base_Address;
	chip_info->ha_config_base = (caddr_t) config_addr;
	chip_info->pci_vhdl = ql_pci_vhdl;
	chip_info->channel_count = channel_count;
	chip_info->clock_rate = clock_rate;
	chip_info->chip_flags = 0;
	chip_info->req_rotor = 0;


	/* needs to load seperate microcode for 1040 and (1240, 1080 and 1280) */ 
	if (dev_id == QLogic_1040_DEVICE_ID) {
		chip_info->risc_code = risc_1040_code01;
		chip_info->risc_code_length = risc_1040_code_length01;
		chip_info->risc_code_addr = risc_1040_code_addr01;
	} else {
		chip_info->risc_code = risc_1080_code01;
		chip_info->risc_code_length = risc_1080_code_length01;
		chip_info->risc_code_addr = risc_1080_code_addr01;
	}

	INITLOCK(&chip_info->res_lock, "ql_response", ql_ctlr_vhdl);
	INITLOCK(&chip_info->req_lock, "ql_request", ql_ctlr_vhdl);
	INITLOCK(&chip_info->mbox_lock, "ql_mbox_lock", ql_ctlr_vhdl);
	INITLOCK(&chip_info->probe_lock, "ql_probe_lock", ql_ctlr_vhdl);

	initnsema(&chip_info->mbox_done_sema, 0, "ql_mboxsema");
	
	/* put together the alenlist */
	chip_info->alen_p = alenlist_create(0);
	alenlist_grow(chip_info->alen_p, v.v_maxdmasz);

	for (i=0; i<channel_count; i++) {

		/* alloc memory buffer from ha */
		ha = (pHA_INFO)QL_KMEM_ZALLOC(sizeof(HA_INFO),
				     VM_CACHEALIGN | VM_NOSLEEP | VM_DIRECT);

		/* setup the chip info */
		ha->chip_info = (pCHIP_INFO)chip_info;
		ha->chip_info->revision = PCI_INB(&conf->Rev_Id);
		ha->chip_info->ha_info[i] = ha;
		ha->host_flags = 0;

		/* assign the channel id */
		ha->channel_id = i;

		/* we have already assign the first one */
		if (i > 0)
			ha->ctlr_vhdl = ql_ctlr_add(ql_pci_vhdl, i);
		else
			ha->ctlr_vhdl = ql_ctlr_vhdl;

		(void)ql_ha_info_put(ha->ctlr_vhdl, ha);

		ha->chip_info->ctlr_vhdl[i] = ha->ctlr_vhdl;

		LOCK(ql_driver_lock);
		/* Put this adapter on the ha_list */
		ha->next_ha = (ql_ha_list) ? ql_ha_list : NULL;
		ql_ha_list = ha;	
		UNLOCK(ql_driver_lock);



		/* Init the raw request queue to empty.  */
		ha->req_forw = NULL;
		ha->req_back = NULL;

		INITLOCK(&ha->waitQ_lock, "ql_waitQ_lock", ha->ctlr_vhdl);

#ifdef QL_STATS
		ha->adp_stats = (adapter_stats_t *)QL_KMEM_ZALLOC(sizeof(adapter_stats_t),
							  VM_CACHEALIGN|VM_NOSLEEP);
		/* flag these statistics as Q-logic type */
		ha->adp_stats->flags |= ADAP_FLAG_QL;
	
		/* Setup the busy and idle timers */
		microtime(&ha->adp_stats->last_idle);
		microtime(&ha->adp_stats->last_busy);
#endif
	
		QL_MUTEX_ENTER(ha->chip_info);
    		ql_set_defaults(ha);
		QL_MUTEX_EXIT(ha->chip_info);

		for (id = 0; id < QL_MAXTARG; id++)
		{
#ifdef QL_STATS
			microtime(&ha->adp_stats->disk[id].last_idle);
			microtime(&ha->adp_stats->disk[id].last_busy);
			ha->adp_stats->disk[id].disk_flags |= DISK_FLAG_QL;
#endif

			/* Initialize the Req_Info free list.  */
			INITLOCK(&(ha->timeout_info[id].per_tgt_tlock), "ql_timeout", ql_ctlr_vhdl);

			IOLOCK(ha->timeout_info[id].per_tgt_tlock,s);
			ha->timeout_info[id].ql_dups = 0;
			ha->timeout_info[id].current_timeout = 0;
			ha->timeout_info[id].watchdog_timer = 0;
			ha->timeout_info[id].time_base = 0;
			ha->timeout_info[id].ql_tcmd = 0;
			ha->timeout_info[id].timeout_state=0;
			r_ptr = &ha->reqi_block[id][0];
			ha->reqi_cntr[id] = 1;
			for (req_cnt=0; req_cnt <= (MAX_REQ_INFO); req_cnt++, r_ptr++)
			{
				r_ptr->req = NULL;
				r_ptr->ha_p = (void *)ha;
			}
			IOUNLOCK(ha->timeout_info[id].per_tgt_tlock,s);
		}
		atomicClearInt(&(ha->ql_ncmd), 0xffffffff);

		

#ifdef IP30	/* remember this board for ACFAIL */
		baseio_qlsave(ha->ctlr_vhdl);
#endif
	}
    }

    ql_dev_desc = device_desc_default_get(vhdl_to_dev(ql_pci_vhdl));
    chip_info->intr_info = pciio_intr_alloc(ql_pci_vhdl, ql_dev_desc, 
		PCIIO_INTR_LINE_A, ql_pci_vhdl);
    pciio_intr_connect(chip_info->intr_info, (intr_func_t)qlintr,
			(intr_arg_t)chip_info, (void *)NULL);

    if (ql_init_isp(chip_info))
    {
	    chip_info->chip_flags = AFLG_CHIP_SHUTDOWN;
	    rc = 2;
	    goto init_failed;
    }

    /* We added a parameter that can be used to LOGICALLY */
    /* disable the board.  If they set this flag, then we shouldn't */
    /* publish this adapter board to the SCSI drivers.  However, */
    /* we do need to be able to open the device in order to update */ 
    /* the parameters to re-enable this board. */
    /* So say we failed the initialization, and check whether the */
    /* actual initialization of the adapter failed (with INITIALIZED */
    /* flag) in open routine.  */
    for (i=0; i<chip_info->channel_count;i++){
	ha = chip_info->ha_info[i];
    	if (!ha->ql_defaults.HA_Enable)
    	{
	    rc = 3;
	    goto init_failed;
    	}
    }

    /* Indicate we found the board */
    return(0);
 init_failed:
    ql_cpmsg(ha->ctlr_vhdl, "ql_init_board failed (%d)\n", rc);
    return(rc); 	
}



/* Function:    ql_init_isp()
**
** Description: Initialize the ISP chip on a specific board.
** Returns: 0 = success
**      1 = failure
*/

static int 
ql_init_isp(pCHIP_INFO chip_info)
{
#ifdef QL_STATS
    	int		count;
#endif
	int		rc = 0;
	int		bus_reset_on_init;

	
	QL_MUTEX_ENTER(chip_info);

	chip_info->response_base = (queue_entry *)QL_KVPALLOC(((sizeof(queue_entry)*chip_info->ql_response_queue_depth)/NBPP),
						       VM_UNCACHED | VM_DIRECT | VM_NOSLEEP | VM_PHYSCONTIG,
						       0);
    	if (chip_info->response_base == NULL) 
	{
		rc = 1;
		goto init_done;
    	}

	chip_info->request_base = (queue_entry *)QL_KVPALLOC(((sizeof(queue_entry)*chip_info->ql_request_queue_depth)/NBPP),
						      VM_UNCACHED | VM_DIRECT | VM_NOSLEEP | VM_PHYSCONTIG,
						      0);
    	if(chip_info->request_base == NULL) 
	{
		rc = 2;
		goto init_done;
    	}

    	chip_info->request_ptr = chip_info->request_base;
    	chip_info->response_ptr = chip_info->response_base;
    	chip_info->response_out = 0;

	/* Initialize request queue indexes.  */

    	chip_info->queue_space = 0;
    	chip_info->request_in  = 0;
    	chip_info->request_out = 0;
    	if (ql_download_fw(chip_info)) 
	{
        	rc = 3;
		goto init_done;
    	}

    	/*
	 * Determine whether a SCSI bus reset is to be done
	 */
	if (ql_bus_reset_on_boot == -1)
		bus_reset_on_init = 0;
	else
	if (ql_bus_reset_on_boot == +1)
		bus_reset_on_init = 1;
	else
		bus_reset_on_init = BUS_RESET_ON_INIT;

    	if (ql_reset_interface(chip_info, bus_reset_on_init)) 
	{
		rc = 4;
		goto init_done;
    	}

 init_done:
	QL_MUTEX_EXIT(chip_info);
	if (rc) {
	        ql_cpmsg(chip_info->ctlr_vhdl[0], "ql_init_isp failed (%d)\n", rc);
		if (chip_info->channel_count > 1 )
	        	ql_cpmsg(chip_info->ctlr_vhdl[1], "ql_init_isp failed (%d)\n", rc);
	}
    	return(rc);
}



/* Function:    ql_enable_intrs()
**
** Description: Enable interrupts for our board.
*/
static void 
ql_enable_intrs(pCHIP_INFO chip_info)
{
	pISP_REGS isp = (pISP_REGS)chip_info->ha_base;

	/* We're all initialized, LET's have them interrupts BABY! */
	QL_PCI_OUTH(&isp->bus_icr, (ICR_ENABLE_RISC_INT | ICR_ENABLE_ALL_INTS));
	
	
	/* Remember that we enabled the interrupts.  */
	atomicSetUint(&chip_info->chip_flags, AFLG_CHIP_INTERRUPTS_ENABLED);
}


#ifdef QL_SRAM_PARITY
/* Function:    ql_enable_sram_parity()
**
** Description: Enable sram parity checking.
** initialize the sram if requested to
*/
static int 
ql_enable_sram_parity(pCHIP_INFO chip_info,int init_sram)
{
    	pISP_REGS isp = (pISP_REGS)chip_info->ha_base;
        u_short mbox_sts[8];
        u_short mailbox0;
	u_short hccr, result;
	int	i;
	int	read_fail = 0;
	
	if (chip_info->chip_flags & AFLG_CHIP_SRAM_PARITY_ENABLE) {	
    		QL_PCI_OUTH(&isp->hccr, HCCR_SRAM_PARITY_ENABLE | HCCR_SRAM_PARITY_BANK0);
		QL_CPMSGS(chip_info->pci_vhdl,"Enabled SRAM parity checking.\n");
	}

	if(init_sram) {
		atomicSetUint(&chip_info->chip_flags, AFLG_CHIP_INITIAL_SRAM_PARITY);	

    		QL_PCI_OUTH(&isp->hccr, HCCR_CMD_PAUSE);

		if (chip_info->device_id == QLogic_1040_DEVICE_ID) {
			QL_PCI_OUTH(&isp->r3,0xbeef); /* data pattern */
			/* no. of words to initialize */
       			QL_PCI_OUTH(&isp->r4,0x8000); 
       	 		QL_PCI_OUTH(&isp->r8,0x0016); /* start addr of routine */
       	 		QL_PCI_OUTH(&isp->r9,0x0449); /* exit point after mailbox */
        		QL_PCI_OUTH(&isp->rar1,0x1000); /* first address to clear memory */ 
        		QL_PCI_OUTH(&isp->control_dma_address_counter_0,0x43a4);  
			/* movb	r3,[rar1++]	; clear memory */
       	 		QL_PCI_OUTH(&isp->control_dma_address_counter_1,0x8421);
			/* dec	r4		; decrement counter */
        		QL_PCI_OUTH(&isp->control_dma_address_counter_2,0x08c2);
			/* jnz	[r8]		; JIF not done with all addresses */
        		QL_PCI_OUTH(&isp->control_dma_address_counter_3,0x097a);
			/* jmp	[r9]		; goto Exit point when done */
		} else { 
			/* do a bank switch into the dma */ 
                	/* 1080, 1240, and 1280 only and we double the register name with other banks */
                	u_short tmp_val;

			/* switch to risc register bank */ 
	               	tmp_val = QL_PCI_INH(&isp->bus_config1);
			QL_PCI_OUTH(&isp->bus_config1, tmp_val&0xfcf7);
			flushbus();

			/* create data pattern */
			QL_PCI_OUTH(&isp->r3, 0xbeef);
                	flushbus();
			
			/* words to initialize */
			QL_PCI_OUTH(&isp->r4, 0x8000);
                	flushbus();
	
			/* starting address of the routine */
			QL_PCI_OUTH(&isp->r8, 0x0016);
                	flushbus();

			/* exit point after the mailbox */
			QL_PCI_OUTH(&isp->r9, 0x0449);
                	flushbus();

			/* first address to clear memory */	
			QL_PCI_OUTH(&isp->rar1, 0x1000);	
                	flushbus();

                	/* switch to DMA bank registers file */
                	QL_PCI_OUTH(&isp->bus_config1, tmp_val|CONF_1_1240_DMA_SEL);
                	flushbus();

        		/* QL_PCI_OUTH(&isp->control_dma_address_counter_0,0x43a4); */ 
			/* movb	r3,[rar1++]	; clear memory */
                	QL_PCI_OUTH(&isp->r6, 0x43a4);
                	flushbus(); 
				
			/* QL_PCI_OUTH(&isp->control_dma_address_counter_1,0x8421); */ 
			/* dec	r4		; decrement counter */
			QL_PCI_OUTH(&isp->r7, 0x8421);
                	flushbus(); 

	
        		/* QL_PCI_OUTH(&isp->control_dma_address_counter_2,0x08c2); */
			/* jnz	[r8]		; JIF not done with all addresses */
			QL_PCI_OUTH(&isp->r8, 0x08c2);
                	flushbus(); 

			
        		/* QL_PCI_OUTH(&isp->control_dma_address_counter_3,0x097a); */
			/* jmp	[r9]		; goto Exit point when done */
			QL_PCI_OUTH(&isp->r9, 0x097a);
                	flushbus(); 


                	/* switch back the registers bank */
                	QL_PCI_OUTH(&isp->bus_config1, tmp_val);
                	flushbus();

		}
    		QL_PCI_OUTH(&isp->hccr, HCCR_CMD_RELEASE);

		/* 
		 *  issue mbox command and poll until the command is done 
		 */
    		LOCK(chip_info->mbox_lock);

		/* set the starting address of the program to execute */
		QL_PCI_OUTH(&isp->mailbox1, 0x0016);
		/* set the command to execute the firmware */
		QL_PCI_OUTH(&isp->mailbox0, MBOX_CMD_EXECUTE_FIRMWARE);
		/* Wake up the isp.  */
		QL_PCI_OUTH(&isp->hccr, HCCR_CMD_SET_HOST_INT);
		/* poll for command to be done */
		for (i = 0;i < QL_COMPUTE_PARITY_MEMORY; i++) {
			mailbox0 = QL_PCI_INH(&isp->mailbox0);
			QL_CPMSGS(chip_info->pci_vhdl, 
				"ql_enable_sram_parity -  mailbox0 = %x\n", mailbox0);
			if (mailbox0 ==  MBOX_STS_COMMAND_COMPLETE)
				break;
			DELAY(1000);
		}

		UNLOCK(chip_info->mbox_lock);

		/* reset the semaphore */
		QL_PCI_OUTH(&isp->bus_sema, 0);

		/* reset the interrupt */
		QL_PCI_OUTH(&isp->bus_isr, 0);

		/* reset the risc interrupt */
		QL_PCI_OUTH(&isp->hccr, HCCR_CMD_CLEAR_RISC_INT);

		/* release the risc process */
		QL_PCI_OUTH(&isp->hccr, HCCR_CMD_RELEASE);

		/* 
                 * check to see if sram parity is supported by reading back the data pattern
  		 * and compare if data pattern is correct and no parity error.
		 */


		/* try to read first 32 words of the data from address 0x1000 */ 
		for (i=0; i<32; i++) {

			if (ql_mbox_cmd(chip_info, mbox_sts, 2, 3,
				MBOX_CMD_READ_RAM_WORD,
				(u_short) (0x1000 + i),
				0, 0, 0, 0, 0, 0))
				read_fail =1;


			hccr = QL_PCI_INH(&isp->hccr);		

			if (chip_info->device_id == QLogic_1040_DEVICE_ID) 
				result = hccr & HCCR_SRAM_PARITY_ERROR_DETECTED_1040;
			else
				result = hccr & HCCR_SRAM_PARITY_ERROR_DETECTED_1240;
			if (result || read_fail) {
				/* check to make sure the data is corrected */	
				/* if data is corrected then the board does support parity */

				/* controller does not support parity */
				atomicClearUint(&chip_info->chip_flags, AFLG_CHIP_INITIAL_SRAM_PARITY);
				atomicClearUint(&chip_info->chip_flags, AFLG_CHIP_SRAM_PARITY_ENABLE);
			
				QL_PCI_OUTH(&isp->bus_icr, ICR_ENABLE_RISC_INT | ICR_SOFT_RESET);
				flushbus();

				DELAY(2); 

				QL_PCI_OUTH(&isp->hccr, HCCR_CMD_RESET);


				/* disable the sram parity checking */
				hccr &= ~(HCCR_SRAM_PARITY_ENABLE|HCCR_SRAM_PARITY_BANK0);
				QL_PCI_OUTH(&isp->hccr, hccr);
					
				/* clear the interrupt */
				QL_PCI_OUTH(&isp->hccr, HCCR_CMD_CLEAR_RISC_INT);

				/* release the risc process */
				QL_PCI_OUTH(&isp->hccr, HCCR_CMD_RELEASE);

				/* return to caller */
				return(0);
			}

					
		}

		/* done with initializing the sram parity */
		atomicClearUint(&chip_info->chip_flags, AFLG_CHIP_INITIAL_SRAM_PARITY);
	}	

	/* return to caller */
	return(0);
	
}
#endif /* QL_SRAM_PARITY */


/* Entry-Point
** Function:    qlioctl
**
** Description: Perform host adapter I/O control operations.
*/
int 
qlioctl(vertex_hdl_t 	ql_ctlr_vhdl, 
	uint 		cmd, 
	scsi_ha_op_t 	*op)
{
	pHA_INFO    ha = ql_ha_info_from_ctlr_get(ql_ctlr_vhdl);
	pCHIP_INFO  chip_info = ha->chip_info;
        u_short mbox_sts[8];
        int err = 0; /* assume no errors */
	int state = NO_QUIESCE_IN_PROGRESS;
	
	/* Now, execute passed command ...  */
    	switch(cmd)
        {
    		case SOP_RESET:
			if (ql_reset(ql_ctlr_vhdl)) {
				err = EIO;
				ql_cpmsg(ql_ctlr_vhdl, "SCSI bus reset failed\n");
			} 
			/*
			 * Reset Target parameters to their default
			 * values.  
			 */
			{
				int id;
				
				for (id = 0; id < QL_MAXTARG; ++id) {
					if (id == ha->ql_defaults.Initiator_SCSI_Id)
						continue;
					if (scsi_lun_vhdl_get(ql_ctlr_vhdl, id, 0) == GRAPH_VERTEX_NONE)
						continue;
					QL_MUTEX_ENTER(chip_info);
					if (ql_set_scsi_parameters(ha->chip_info, mbox_sts, 
					    ha->channel_id, id) ) {
						ql_tpmsg(ql_ctlr_vhdl, id, "MBOX_CMD_SET_TARGET_PARAMETERS failed\n");
					}

					QL_MUTEX_EXIT(chip_info);
				}
			}
			break;

        	case SOP_SCAN:
	    		scan_bus(ql_ctlr_vhdl);
	    		break;

    		case SOP_DEBUGLEVEL:      /* set debug print count */
			ql_debug = op->sb_arg;
	    		break;

        	case SOP_QUIESCE:

			/* 
			 * set the quiesce in progress flag in the ha structure
			 * set two timeouts
			 *     1. timeout to succesfully quiesce bus
			 *     2. timeout for max time to allow quiesce 
			 * if quiesce in progress replace timeouts with 
			 * new ones
			 * sb_opt = time for quiesce attempt		
			 * sb_arg = time for quiesce to be active	
			 */
	
	    		if( (op->sb_opt == 0) || (op->sb_arg == 0) ) {
				err = EINVAL;
				break;
	    		}
	    		/* is this to extend the quiesce */
	    		if(ha->host_flags & AFLG_HA_QUIESCE_COMPLETE) {
				untimeout(ha->quiesce_id);
				ha->quiesce_id = 
				timeout(kill_quiesce, ha, op->sb_arg, KILL_WITHOUT_TIMEOUT);
				break;
	    		}

	    		if (ha->ql_ncmd == 0) {
				atomicSetUint(&ha->host_flags, AFLG_HA_QUIESCE_COMPLETE);
                		atomicClearUint(&ha->host_flags, AFLG_HA_QUIESCE_IN_PROGRESS);
				ha->quiesce_id = timeout(kill_quiesce, ha, op->sb_arg, KILL_WITHOUT_TIMEOUT);
	     		} else {
	         		atomicSetUint(&ha->host_flags, AFLG_HA_QUIESCE_IN_PROGRESS);
	         		ha->quiesce_in_progress_id = 
					timeout(kill_quiesce,ha, op->sb_opt, KILL_WITHOUT_TIMEOUT);
                 		ha->quiesce_time = op->sb_arg;
	     		}
	     		break;


	        case SOP_UN_QUIESCE:
        		/* 
			 * reset the in progress and complete flags     
        		 * reset the timeouts                           
			 * reset the controller state and rescan the bus
			 * attempt to start pending commands		
			 */
	    		kill_quiesce(ha, KILL_WITH_TIMEOUT); 
            		break;		

        	case SOP_QUIESCE_STATE:
        		/* report the queice state		*/
	    		if (ha->host_flags & AFLG_HA_QUIESCE_IN_PROGRESS)
				state = QUIESCE_IN_PROGRESS;
			else 
	    		if (ha->host_flags & AFLG_HA_QUIESCE_COMPLETE)
				state = QUIESCE_IS_COMPLETE;
			
            		copyout(&state,(void *)op->sb_addr,sizeof(int));
	    		break;

		case SOP_GET_SCSI_PARMS:
		{
			struct scsi_parms sp;
			int               targ;
			int               diff_sense;

			QL_MUTEX_ENTER(chip_info);
			bzero(&sp, sizeof(struct scsi_parms));
			if ((diff_sense = ql_diffsense(ha)) == -1) 
			{
				err = EIO;
				goto get_done;
			}
			else
				sp.sp_is_diff = diff_sense;
	
			if ((sp.sp_selection_timeout = (u_short)ha->ql_defaults.Selection_Timeout) >= 250)
				sp.sp_selection_timeout *= 1000;
			sp.sp_scsi_host_id = (u_short)ha->ql_defaults.Initiator_SCSI_Id;

			for (targ = 0; targ < QL_MAXTARG; ++targ) 
			{
				if (targ == ha->ql_defaults.Initiator_SCSI_Id)
					continue;
				if (scsi_lun_vhdl_get(ql_ctlr_vhdl, targ, 0) == GRAPH_VERTEX_NONE)
					continue;
				sp.sp_target_parms[targ].stp_is_present = 1;
				if (ql_get_scsi_parameters(chip_info, mbox_sts, ha->channel_id, 
					targ))
				{
					ql_tpmsg(ha->ctlr_vhdl, targ, "MBOX_CMD_GET_TARGET_PARAMETERS command failed\n");
					err = EIO;
				} 
				else 
				{
					if ((mbox_sts[2] >> 8) & CAP_SYNC_DATA_TRANSFERS) 
					{
						sp.sp_target_parms[targ].stp_is_sync = 1;
						if ( (ha->chip_info->clock_rate == FAST40_CLOCK_RATE) && 
							((mbox_sts[3] & 0xFF) == SYNC_PERIOD_FAST40) )
							sp.sp_target_parms[targ].stp_sync_period = 25;
						else 
							sp.sp_target_parms[targ].stp_sync_period = (mbox_sts[3] & 0xFF) * 4;
						sp.sp_target_parms[targ].stp_sync_offset = (mbox_sts[3] & 0xFF00) >> 8;
					}
				}
				sp.sp_target_parms[targ].stp_is_wide = 
					(((mbox_sts[2] >> 8) & CAP_WIDE_DATA_TRANSFERS) == CAP_WIDE_DATA_TRANSFERS);
			}
			copyout(&sp, (void *)op->sb_addr, sizeof(struct scsi_parms));
		    get_done:
			QL_MUTEX_EXIT(chip_info);
			break;
		} /* SOP_GET_SCSI_PARMS */

		case SOP_SET_BEHAVE:
		{
			vertex_hdl_t	   lun_vhdl;
			ql_local_info_t   *qli;
		  
			switch(op->sb_opt) {
				case LUN_QERR:
				{
					u_char targ = (op->sb_arg >> SOP_QERR_TARGET_SHFT) & 0xFF;
					u_char lun  = (op->sb_arg >> SOP_QERR_LUN_SHFT) & 0xFF;
					u_char qerr = (op->sb_arg >> SOP_QERR_VAL_SHFT) & 0xFF;


					if ((lun_vhdl = scsi_lun_vhdl_get(ql_ctlr_vhdl, targ, lun)) == GRAPH_VERTEX_NONE) {
						ql_cpmsg(ha->ctlr_vhdl, 
							"SOP_SET_BEHAVE LUN_QERR failed: targ/lun %d/%d not valid\n", targ, lun);
						err = EINVAL;
						break;
					}
					qli = SLI_INFO(scsi_lun_info_get(lun_vhdl));
					ASSERT(qli);
					if (qerr)
						atomicSetInt(&qli->qli_dev_flags, DFLG_BEHAVE_QERR1);
					else
						atomicClearInt(&qli->qli_dev_flags, DFLG_BEHAVE_QERR1);
				}
				break;
				default:
					ql_cpmsg(ha->ctlr_vhdl, "Illegal SOP_SET_BEHAVE sub-code 0x%x\n", op->sb_opt);
					err = EINVAL;
			}
			
			break;
		} /* SOP_SET_BEHAVE */

		default:
			err = EINVAL;
    	}
    	return(err);     /* nothing to return */
}

/* Entry-Point
** Function:    ql_entry
**
** Description: Execute a SCSI request. 
**
** Returns:  0 = success (i.e. I understood the request)
**      -1 = failure (what the heck was that?)
*/
static int 
ql_entry(scsi_request_t* request)
{
    	u_short          id  = request->sr_target;
    	u_short          lun = request->sr_lun;
	ql_local_info_t *qli = SLI_INFO(scsi_lun_info_get(request->sr_lun_vhdl));
    	pHA_INFO	 ha = ql_ha_info_from_lun_get(request->sr_lun_vhdl); 
	pCHIP_INFO	 chip_info = ha->chip_info;
     	SPL_DECL(s)

	/* Before processing the request, make sure we will return good status.  */
	request->sr_status = 0;
	request->sr_scsi_status = 0;
	request->sr_ha_flags = 0;
	request->sr_sensegotten = 0;
	request->sr_spare = ha;

        /* Error if the adapter specified is invalid.  */
    	if ((ha == NULL) || (chip_info->chip_flags & AFLG_CHIP_SHUTDOWN)) {
		request->sr_status = SC_REQUEST;
		(*request->sr_notify)(request);
		return (0);
    	}

	/*
	 * Ensure buffer alignment constraints are enforced. Only need
	 * to check for the case of KV or UV addresses; PAGEIO is
	 * guaranteed to be page aligned.  
	 */
	if (request->sr_flags & (SRF_MAPUSER | SRF_MAP) && 
	    request->sr_buflen != 0 && 
	    ((uintptr_t) request->sr_buffer & 3))
	{
		request->sr_status = SC_ALIGN;
		(*request->sr_notify)(request);
		return(0);
	}


        if (request->sr_buflen != 0 && !(request->sr_flags & SRF_ALENLIST))
        {
                int offset;
                if (request->sr_flags & SRF_MAPBP)
                        offset = 0;
                else
                        offset = poff(request->sr_buffer);
                if (btoc(request->sr_buflen + offset) > v.v_maxdmasz)
                {
                        request->sr_status = SC_REQUEST;
                        (*request->sr_notify) (request);
                        return (0);
                }
        }

	/*
	 * If CONTINGENT ALLEGIANCE in effect and requestor hasn't set
	 * the AEN_ACK flag, then bail out.
	 */
        if (qli->qli_dev_flags & DFLG_CONTINGENT_ALLEGIANCE) {
	    if (request->sr_flags & SRF_AEN_ACK) {
		atomicClearInt(&qli->qli_dev_flags, DFLG_CONTINGENT_ALLEGIANCE);
            }  
            else {
                request->sr_status = SC_ATTN;
                (*request->sr_notify)(request);
                return(0);
            }
	}

	/* 
	 * If the LUN is being aborted, stick the command into the LUN
	 * abort wait Q.  
	 */
	if (qli->qli_dev_flags & DFLG_ABORT_IN_PROGRESS) 
	{
		IOLOCK(qli->qli_lun_mutex, s);

	        /* 
		 * After grabbing the lock, check again whether the
		 * LUN is still being aborted. There could be a race
		 * here which could cause commands to be "forgotten"
		 * if they're stuck in the LUN wait Q after the LUN
		 * abort is complete.
		 */
	        if ((qli->qli_dev_flags & DFLG_ABORT_IN_PROGRESS) == 0) {
			IOUNLOCK(qli->qli_lun_mutex, s);
			goto lun_abort_done;
		}
		if (qli->qli_awaitf)
			qli->qli_awaitb->sr_ha = request;
		else
			qli->qli_awaitf = request;
		qli->qli_awaitb = request;
		request->sr_ha = NULL;
		++qli->qli_cmd_awcnt;
		IOUNLOCK(qli->qli_lun_mutex, s);
		return(0);
	} 
 lun_abort_done:
	/* 
	 * If LUN is being initialized, stick the command into the LUN
	 * init. Q.  
	 */
	if (qli->qli_dev_flags & DFLG_INIT_IN_PROGRESS)
	{
		IOLOCK(qli->qli_lun_mutex, s);

	        /* 
		 * After grabbing the lock, check again whether the
		 * LUN is still being inited. There could be a race
		 * here which could cause commands to be "forgotten"
		 * if they're stuck in the LUN wait Q after the LUN
		 * init. is complete.  
		 */
	        if ((qli->qli_dev_flags & DFLG_INIT_IN_PROGRESS) == 0) {
			IOUNLOCK(qli->qli_lun_mutex, s);
			goto lun_init_done;
		}
		if (qli->qli_iwaitf)
			qli->qli_iwaitb->sr_ha = request;
		else
			qli->qli_iwaitf = request;
		qli->qli_iwaitb = request;
		request->sr_ha = NULL;
		++qli->qli_cmd_iwcnt;
		IOUNLOCK(qli->qli_lun_mutex, s);
		return(0);
	}

	/*
	 * Check that the device has been initialized 
	 */
	if (!(qli->qli_dev_flags & DFLG_INITIALIZED) ) {
		u_short mbox_sts[8];
		
		IOLOCK(qli->qli_lun_mutex, s);
		atomicSetInt(&qli->qli_dev_flags, DFLG_INIT_IN_PROGRESS);

		/* 
		 * Check the flag again, in case someone beat us to it.
		 */
		if (qli->qli_dev_flags & DFLG_INITIALIZED) {
		        atomicClearInt(&qli->qli_dev_flags, DFLG_INIT_IN_PROGRESS);
			IOUNLOCK(qli->qli_lun_mutex, s);
			goto lun_init_done;
		}

		QL_MUTEX_ENTER(chip_info);		    

		if (ql_set_device_queue_parameters(chip_info, mbox_sts, 
		    ha->channel_id, id, lun)) {
			ql_tpmsg(ha->ctlr_vhdl, id, "MBOX_CMD_SET_TARGET_PARAMETERS command failed\n");
		}

		if (ql_set_scsi_parameters(chip_info, mbox_sts, ha->channel_id, id)) {
			ql_tpmsg(ha->ctlr_vhdl, id, "MBOX_CMD_SET_TARGET_PARAMETERS command failed\n");
		}

		/* Device has now been initialized! */
		atomicSetInt(&qli->qli_dev_flags, DFLG_INITIALIZED);
		atomicClearInt(&qli->qli_dev_flags, DFLG_INIT_IN_PROGRESS);
		QL_MUTEX_EXIT(chip_info);

		/*
		 * Move commands waiting in LUN init. queue into the wait Q
		 */
	        {
			scsi_request_t *lun_forw;
			scsi_request_t *lun_back;

			lun_forw = qli->qli_iwaitf;
			lun_back = qli->qli_iwaitb;
			qli->qli_iwaitf = qli->qli_iwaitb = NULL;
			qli->qli_cmd_iwcnt = 0;
			IOUNLOCK(qli->qli_lun_mutex, s);

			IOLOCK(ha->waitQ_lock, s);
			if (lun_forw)
			{	
				if (ha->waitf) 
				{
					ha->waitb->sr_ha = lun_forw;
					ha->waitb = lun_back;
				} 
				else
				{
					ha->waitf = lun_forw;
					ha->waitb = lun_back;
				}
				ha->waitb->sr_ha = NULL;
			}
			IOUNLOCK(ha->waitQ_lock, s);
		}
    	}

 lun_init_done:
	/* 
	 * Stick the command into the LUN Wait Q
	 */
	IOLOCK(ha->waitQ_lock,s);
	if (ha->waitf)
		ha->waitb->sr_ha = request;
	else
		ha->waitf = request;
	ha->waitb      = request;
	request->sr_ha = NULL;
	IOUNLOCK(ha->waitQ_lock,s);

        /* Start the command (if possible).  */
        if (!(ha->host_flags & AFLG_HA_DRAIN_IO))
		ql_start_scsi(chip_info);

    	if (chip_info->chip_flags & AFLG_CHIP_DUMPING) {
		pISP_REGS   isp = (pISP_REGS)chip_info->ha_base;
		while (!(QL_PCI_INH(&isp->bus_isr) & BUS_ISR_RISC_INT)) {
			DELAY(25);
		}	
		qlintr(chip_info);
    	}

    	/* Return a status to indicate we understood the request.  */
    	return(0);
}



/* Entry-Point
** Function:    qlintr
**
** Description: This is the interrupt service routine.  This is called
**      by the system to process a hardware interrupt for our
**      board, or by one of our internal routines that need to
**      check for an interrupt by polling.  These routines MUST
**      lock out interrupts (splx) before calling here.
*/

void
qlintr(pCHIP_INFO chip_info)
{
	pISP_REGS	isp = (pISP_REGS)chip_info->ha_base;
        u_short		isr = 0;
	
    	LOCK(chip_info->res_lock);
	if (chip_info->chip_flags & AFLG_CHIP_SHUTDOWN)
		goto done;
	
	ASSERT((chip_info->chip_flags & AFLG_CHIP_IN_INTR) == 0);
	atomicSetInt(&chip_info->chip_flags, AFLG_CHIP_IN_INTR);
	
        /* Determine if this board caused the (an) interrupt.  */
	isr = QL_PCI_INH(&isp->bus_isr);
	
        if (isr & BUS_ISR_RISC_INT) {
	    	/* We have the interrupter...try to service the request.  */

#ifdef QL_STATS
	    	ha->adp_stats->intrs++;
#endif
	        ql_service_interrupt(chip_info);

	}
 done:
	
	ASSERT(chip_info->chip_flags & AFLG_CHIP_IN_INTR);
	atomicClearInt(&chip_info->chip_flags, AFLG_CHIP_IN_INTR);
	
        UNLOCK(chip_info->res_lock);
}


/*
** Function:    ql_service_interrupt
**
** Description: This routine actually services the interrupt.
**      There are two kinds of interrupts: 1) Responses available
**      and 2) a Mailbox request completed.  ALWAYS service the
**      Mailbox interrupts first.  Then process all responses we
**      have (may be more than one).  Do this by pulling off all
**      the completed responses and putting them in a linked list.
**      Then after we have them all, tell the adapter we have them
**      and do completion processing on the list.  This limits
**      the amount of time the adapter has to wait to post new
**      responses.
**
**	The driver should then read mailbox 0. If Mailbox 0 contains a
**	value of 0x8000 or greater then the Interrupt is for an Asynchronous
**	Event. If Mailbox 0 contains a value between 0x4000 and 0x7fff
**	then the value is reporting the results of a mailbox command.
**
**	If the interrupt is for a Mailbox Command completion or an
**	Asynchronous Event the driver must write a 0 to the Semaphore
**	register when it has finished reading the data from the Mailboxes.
**	Clearing the Semaphore informs the ISP that the driver has read
**	all the information associated with the Mailbox Command or Asynch.
**	Event. When the Semaphore is 0 the ISP is free to update the contents
**	of the Mailbox registers. The driver may clear the semaphore before
**	or after clearing the interrupt from the ISP. 
**
**	If the Semaphore register contains a 0 then the interrupt is
**	reporting that the ISP has added an entry or entries to the
**	response queue and the driver should read mailbox 5 to determine
**	how many entries were added. The ISP will not add anymore entries
**	to the queue or update mailbox 5 until the driver has cleared the
**	interrupt from the ISP.
**
**	To clear an interrupt from the ISP the driver must write a value of
**	0x7000 to the Host Command and Control (HCCR) register of the ISP.
**
**	If the ISP has reported an interrupt to the host it will not modify
**	the contents of the Semaphore register, or Mailbox registers 0-3,5-7.
**	Until the driver has cleared the interrupt. However, the ISP will 
**	continu to remove entries from the Request Queue so it is possible
** 	that the ISP may modify the contents of Mailbox 4 while an interrupt
**	 is in progress.
**
*/
static void 
ql_service_interrupt(pCHIP_INFO chip_info)
{
    	status_entry    *q_ptr;
    	scsi_request_t  *request;
	ql_local_info_t *qli;
    	pREQ_INFO        r_ptr;
	pHA_INFO	 ha;
    	pISP_REGS        isp = (pISP_REGS)chip_info->ha_base;
	
    	scsi_request_t*  cmp_forw = NULL;
    	scsi_request_t*  cmp_back = NULL;
	
    	int		 port, tgt, idx;
    	u_short          isr=0;

    	/* Process all the mailbox interrupts and all responses.  */
    	do  {			/* TAG 1 */
		u_short response_in=0;
		u_short bus_sema=0;
#ifdef QL_SRAM_PARITY
		u_short hccr=0;
		u_short result;
		int i;
#endif
		/* 
		 * Check for async mailbox events. 
		 */
		bus_sema = QL_PCI_INH(&isp->bus_sema);
		if (bus_sema & BUS_SEMA_LOCK)
		{
			ql_service_mbox_interrupt(chip_info);
		}	

		/*
		 * Mailbox command processing may have seem IOCB
		 * completions. Clear the flag
		 */
		atomicClearInt(&chip_info->chip_flags, AFLG_CHIP_ASYNC_RESPONSE_IN);

		/* 
		 * If there aren't any completed responses then don't do any 
		 * of the following. i.e. it was just a mailbox interrupt or
		 * spurious becuase right now I poll on mailbox commands. 
		 */
		
		response_in = chip_info->response_in = QL_PCI_INH(&isp->mailbox5);
		if ((response_in == chip_info->response_out) || !(chip_info->chip_flags & AFLG_CHIP_INITIALIZED)) {
		
#ifdef QL_SRAM_PARITY
			/* check for an sram parity error condition */
			hccr = QL_PCI_INH(&isp->hccr);
			if (chip_info->device_id == QLogic_1040_DEVICE_ID)
				result = hccr & HCCR_SRAM_PARITY_ERROR_DETECTED_1040;
			else 
				result = hccr & HCCR_SRAM_PARITY_ERROR_DETECTED_1240; 
			if(result)	{
				if (!(chip_info->chip_flags & AFLG_CHIP_DOWNLOAD_FIRMWARE)) {
					cmn_err_tag(5,CE_WARN,"\n%v Q-logic sram parity error detected\n",
					chip_info->pci_vhdl);
					if(hccr & HCCR_SRAM_PARITY_BANK0)
						cmn_err_tag(6,CE_WARN,"%v Q-logic sram parity error on bank 0 hccr=%x\n",
						chip_info->pci_vhdl, hccr);
					if(hccr & HCCR_SRAM_PARITY_BANK1)
						cmn_err_tag(7,CE_WARN,"%v Q-logic sram parity error on bank 1\n",
						chip_info->pci_vhdl);
#ifdef DEBUG
					dump_registers(chip_info,isp);
#endif
					IOUNLOCK(chip_info->res_lock,s);
					QL_MUTEX_ENTER(chip_info);
					ql_reset_interface(chip_info,1);
					for(i=0; i<chip_info->channel_count; i++){
						ha = chip_info->ha_info[i];
						ql_flush_queue(ha, SC_HARDERR);
					}
					QL_MUTEX_EXIT(chip_info);
					IOUNLOCK(chip_info->res_lock,s);
				} else {
					/* 
					 * Parity error will occurred when downloading 
					 * firmware.  Ignore the interrupt with the clear and
					 * release the risc.
					 * step 1.  disable parity checking 
					 * step 2.  clear interrupt 
					 * step 3.  reenable parity checking
					 * step 4.  release the risc processor
					 */
					  
					  /* disable parity checking */
					  hccr &=  ~(HCCR_SRAM_PARITY_ENABLE|HCCR_SRAM_PARITY_BANK0);
    				          QL_PCI_OUTH(&isp->hccr, hccr);

					  /* clear the interrupt */
					  QL_PCI_OUTH(&isp->hccr, HCCR_CMD_CLEAR_RISC_INT);

					  /* enable the parity checking */
    				          QL_PCI_OUTH(&isp->hccr, HCCR_SRAM_PARITY_ENABLE | HCCR_SRAM_PARITY_BANK0);

					  /* release the risc processor */
					  QL_PCI_OUTH(&isp->hccr, HCCR_CMD_RELEASE); 
						
					  /* exit this routine */
					  return;

				}

			}
#endif /* QL_SRAM_PARITY */	
#ifdef QL_STATS
			/*
			 * mailbox commands are not spurrious
			 */
			if(!(bus_sema & BUS_SEMA_LOCK))  {
				ha->adp_stats->intrs_spur++;
			}
#endif
			QL_PCI_OUTH(&isp->hccr, HCCR_CMD_CLEAR_RISC_INT);
			return;
		}

		/* 
		 * Remove all of the responses from the queue.  
		 */
		while ((chip_info->response_out != response_in)) {
#ifdef FLUSH
			status_entry	ql_iorb;
			ql_iorb = *(status_entry *)chip_info->response_ptr;
			/*
			 * there is a small race condition between a ql write to the
			 * second half of the cacheline and the cacheline invalidation
			 */
			DCACHE_INVAL(
				     (void *)((__psunsigned_t)chip_info->response_ptr & CACHE_SLINE_MASK),
				     CACHE_SLINE_SIZE);
			
			/* 
			 * Get a pointer to the next response entry.  
			 */
			q_ptr = &ql_iorb;
#else
			q_ptr = (status_entry *)chip_info->response_ptr;
#endif	/* FLUSH */

#ifdef QL1240_SWAP_WAR
			if ( (chip_info->device_id == QLogic_1240_DEVICE_ID) ||
			     (chip_info->device_id == QLogic_1080_DEVICE_ID) ||
			     (chip_info->device_id == QLogic_1280_DEVICE_ID) )
				munge_iocb((u_char *)q_ptr, 64);
#endif

#ifdef QL_INTR_DEBUG
			if (chip_info->ha_interrupt_pending && ((lbolt - chip_info->ha_interrupt_pending) > 2))
				cmn_err(CE_DEBUG, "ha 0x%x -- CMD interrupt pending for %dmS: %d %d\n", 
					chip_info, (lbolt-chip_info->ha_interrupt_pending)*10, lbolt, chip_info->ha_interrupt_pending);
			chip_info->ha_interrupt_pending = 0;
#endif
			
#if (defined(SN) || defined(IP30)) && !defined(USE_MAPPED_CONTROL_STREAM)
			munge ((u_char *)q_ptr,64);
#endif
#ifdef DEBUG
                        if (ql_debug >= 2)
                           	dump_iorb(q_ptr);
#endif

			/*
			 * Check for DEAD response. If dead response, then exit
			 * the response processing loop. The LEGO architecure
			 * guarantees no relative completion order vs. issue order
			 * of write DMAs and PIOs. Consequently, the PIO to read
			 * the response head pointer register may actually
			 * complete before the DMA write to update the
			 * corresponding response block completes. Imagine the
			 * following scenario: Command A completes and causes an
			 * interrupt. While in the ISR command B completes, which
			 * we know about by reading the response tail pointer
			 * (mailbox 5), but the DMA write to update the response
			 * block hasn't yet completed. If at this point we bail
			 * out and await the next interrupt, we'll pick up the
			 * response for command B then. An interrupt is a barrier
			 * operation so we're guaranteed that the DMA write will
			 * have completed by the time we get the interrupt.  
			 */
			if (((volatile status_entry *)q_ptr)->handle == 0xDEAD) 
			{
#ifdef DEBUG
				cmn_err(CE_WARN, "Premature response seen: handle 0x%x, q_ptr 0x%x.\n",
					q_ptr->handle, q_ptr);
#endif
				/* Update the ISP's response tail pointer. */
				QL_PCI_OUTH(&isp->mailbox5, chip_info->response_out);
		    
				/* Notify caller of command competions */
				ql_notify_responses(chip_info, cmp_forw);
				cmp_forw = cmp_back = NULL;
		    
				goto got_responses;
			}

			/* 
			 * Move the pointers for the next entry.  
			 */
			if (chip_info->response_out == (chip_info->ql_response_queue_depth - 1))
			{
				chip_info->response_out = 0;
				chip_info->response_ptr = chip_info->response_base;
			}
			else
			{
				chip_info->response_out++;
				chip_info->response_ptr++;
			}

			/* 
			 * Make sure the driver issued the command right.  
			 */
			if (q_ptr->hdr.flags & EF_ERROR_MASK) {
				if(q_ptr->hdr.flags & EF_BUSY)
					ql_cpmsg(chip_info->pci_vhdl, "Detected busy condition\n");
				if(q_ptr->hdr.flags & EF_BAD_HEADER)
					ql_cpmsg(chip_info->pci_vhdl, "Detected a bad header\n");
				if(q_ptr->hdr.flags & EF_BAD_PAYLOAD)
					ql_cpmsg(chip_info->pci_vhdl, "Detected a bad payload\n");
				continue;

			}

			if (q_ptr->hdr.entry_type != ET_STATUS) 
			{
				ql_cpmsg(chip_info->pci_vhdl, "Invalid driver command issued: "
					 "entry_cnt 0x%x, entry_type 0x%x, flags 0x%x, sys_def_1 0x%x, handle 0x%x\n",
					 q_ptr->hdr.entry_cnt, q_ptr->hdr.entry_type,
					 q_ptr->hdr.flags, q_ptr->hdr.sys_def_1,
					 q_ptr->handle);
#ifdef DEBUG
				if (ql_debug >= 2)
					dump_iorb(q_ptr);
#endif
				continue;
			}

			ASSERT((q_ptr->handle > 0) && (q_ptr->handle <= MAX_HANDLE_NO)); 
			port = (q_ptr->handle & 0x00001000) >> 12;
			if (port >= chip_info->channel_count)
				panic("port is greater or equal channel count");
#if 0 
			tgt = q_ptr->handle / (MAX_REQ_INFO + 1);
			idx = q_ptr->handle % (MAX_REQ_INFO + 1) ;
#endif
			tgt = (q_ptr->handle & 0x00000f00) >> 8;
			idx = (q_ptr->handle & 0x000000ff);
			ha = chip_info->ha_info[port];
			r_ptr   = &ha->reqi_block[tgt][idx];

#ifdef QL_TIMEOUT_DEBUG
			request = r_ptr->req;
			if (request->sr_lun_vhdl == ql_timeout_debug_lunvhdl &&
			    --ql_timeout_debug_timer <= 0)
			{
				ql_lpmsg(ha->ctlr_vhdl, request->sr_target, request->sr_lun, 
					 "ql_timeout_debug: Dropping request 0x%x\n", request);
				ql_timeout_debug_timer = ql_timeout_debug_frequency;
				continue;
			}
#endif

			/* check for timeout */

			atomicAddInt(&(ha->ql_ncmd),-1); 
			IOLOCK(ha->timeout_info[tgt].per_tgt_tlock,s);
			ha->timeout_info[tgt].ql_tcmd--;


			if (r_ptr->timeout >= ha->timeout_info[tgt].time_base)  {
				if (--(ha->timeout_info[tgt].ql_dups) >= 0) {
					/* reset the timeout to current time base */
					/* there is  a problem with this  */
					ha->timeout_info[tgt].current_timeout =
						ha->timeout_info[tgt].time_base;
				} else {
					if ( ha->timeout_info[tgt].ql_tcmd ) {
						set_new_timebase(ha,tgt);
						ha->timeout_info[tgt].current_timeout =
							ha->timeout_info[tgt].time_base;
					} else {
						ha->timeout_info[tgt].current_timeout =
							ha->timeout_info[tgt].time_base = 0;
					}
				}
			} else {
				ha->timeout_info[tgt].ql_dups = 0;
				ha->timeout_info[tgt].current_timeout =
					ha->timeout_info[tgt].time_base;
			}

			if ((ha->host_flags & AFLG_HA_DRAIN_IO) && (ha->timeout_info[tgt].ql_tcmd == 0))
				ha->timeout_info[tgt].timeout_state = 0;
			IOUNLOCK(ha->timeout_info[tgt].per_tgt_tlock,s);
			if ((ha->host_flags & AFLG_HA_DRAIN_IO) && (ha->ql_ncmd == 0))
				atomicClearInt(&ha->host_flags, AFLG_HA_DRAIN_IO);
			ha->timeout_info[tgt].t_last_intr = lbolt/HZ;
			ha->chip_info->ha_last_intr = lbolt/HZ;
			
			request = r_ptr->req;
			
			/* XXX - Temporary hack to identify occurance of bug #450305 */
			if (request == NULL)
			{
				ql_cpmsg(ha->ctlr_vhdl, "Bogus response handle: handle 0x%x, q_ptr 0x%x\n",
					 q_ptr->handle, q_ptr);
				panic("");
			}

			/*
			 * Mark handle as "DEAD"
			 */
			q_ptr->handle = 0xDEAD;

			/*
			 * Update LUN status - if all aborted commands have been
			 * returned, transfer all commands queued during the abort
			 * phase into the wait Q.  
			 */
			qli = SLI_INFO(scsi_lun_info_get(request->sr_lun_vhdl));
			atomicAddInt(&qli->qli_cmd_rcnt, -1);
			
			if ((qli->qli_dev_flags & DFLG_ABORT_IN_PROGRESS) && (qli->qli_cmd_rcnt == 0)) 
			{
				scsi_request_t *lun_forw;
				scsi_request_t *lun_back;

				IOLOCK(qli->qli_lun_mutex, s);

				lun_forw = qli->qli_awaitf;
				lun_back = qli->qli_awaitb;
				qli->qli_awaitf = qli->qli_awaitb = NULL;
				qli->qli_cmd_awcnt = 0;
				atomicClearInt(&qli->qli_dev_flags, DFLG_ABORT_IN_PROGRESS);
				atomicSetInt(&qli->qli_dev_flags, DFLG_SEND_MARKER);
				IOUNLOCK(qli->qli_lun_mutex, s);

				IOLOCK(ha->waitQ_lock, s);

				if (lun_forw)
				{	
					if (ha->waitf) 
					{
						ha->waitb->sr_ha = lun_forw;
						ha->waitb = lun_back;
					} 
					else
					{
						ha->waitf = lun_forw;
						ha->waitb = lun_back;
					}
					ha->waitb->sr_ha = NULL;
				}
				IOUNLOCK(ha->waitQ_lock, s);
			}

			request->sr_status = SC_GOOD;
			request->sr_scsi_status = q_ptr->scsi_status;
			request->sr_resid = q_ptr->residual;

#ifdef QL_STATS
			ha->adp_stats->disk[request->sr_target].cmds_completed++;
			ha->adp_stats->cmds_completed++;

			/* 
			 * if there are now no outstanding commands for this controller then
			 * transition from a busy to idle state.  Same for the device.
			 * transition means:
			 *   1. add time to the busy time.
			 *   2. put new time stamp in last_idle field
			 */
			if (!(ha->adp_stats->cmds - ha->adp_stats->cmds_completed)) 
			{
				/* 
				 * no outstanding commands for this adapter
				 * going from busy to idle
				 * add delta time to busy time and update new idle time
				 */
				microtime(&ha->adp_stats->last_idle);
				add_deltatime(&ha->adp_stats->last_idle,
					      &ha->adp_stats->last_busy,
					      &ha->adp_stats->busy);
			}
			if (!(ha->adp_stats->disk[request->sr_target].cmds - ha->adp_stats->disk[request->sr_target].cmds_completed)) 
			{
				microtime(&ha->adp_stats->disk[request->sr_target].last_idle);
				add_deltatime(&ha->adp_stats->disk[request->sr_target].last_idle,
					      &ha->adp_stats->disk[request->sr_target].last_busy,
					      &ha->adp_stats->disk[request->sr_target].busy);
			}
#endif /* QL_STATS */

			switch(q_ptr->completion_status) 
			{
			case SCS_COMPLETE:	/* OK completion */
			case SCS_DATA_UNDERRUN:	/* Underruns are OK if request more data than delivered */
				break;

			case SCS_INCOMPLETE:
				if (!(q_ptr->state_flags & SS_GOT_TARGET) &&
				    (q_ptr->status_flags & SST_TIMEOUT)) 
				{
					if (!(ha->host_flags & AFLG_HA_BUS_SCAN_IN_PROGRESS))
						ql_tpmsg(ha->ctlr_vhdl, request->sr_target, "Selection timeout\n");
					request->sr_status = SC_TIMEOUT;
				}
				else
					goto comp_hard_error;
				break;

			case SCS_DMA_ERROR:
#ifdef A64_BIT_OPERATION
				if (q_ptr->state_flags & SS_TRANSFER_COMPLETE) 
					ql_tpmsg(ha->ctlr_vhdl, request->sr_target,
						 "INVALID DMA ERROR: "
						 "completion status [0x%x] (%s), "
						 "scsi status[0x%x], state flags[0x%x], status flags[0x%x]\n", 
						 q_ptr->completion_status,
						 ql_completion_status_msg(q_ptr->completion_status),
						 q_ptr->scsi_status,
						 q_ptr->state_flags,
						 q_ptr->status_flags);
				else
#endif
					goto comp_hard_error;
				break;

			case SCS_RESET_OCCURRED:
				request->sr_status = SC_ATTN;
				break;

			case SCS_ABORTED:
			{
				ql_local_info_t   *qli = SLI_INFO(scsi_lun_info_get(request->sr_lun_vhdl));

				request->sr_status = SC_ATTN;
				atomicSetInt(&qli->qli_dev_flags, DFLG_SEND_MARKER);
				break;
			}

			case SCS_TIMEOUT:
				request->sr_status = SC_CMDTIME;
				break;

		comp_hard_error:
			case SCS_TRANSPORT_ERROR:
			case SCS_DATA_OVERRUN:
			case SCS_COMMAND_OVERRUN:
			case SCS_STATUS_OVERRUN:
			case SCS_BAD_MESSAGE:
			case SCS_NO_MESSAGE_OUT:
			case SCS_EXT_ID_FAILED:
			case SCS_IDE_MSG_FAILED:
			case SCS_ABORT_MSG_FAILED:
			case SCS_REJECT_MSG_FAILED:
			case SCS_NOP_MSG_FAILED:
			case SCS_PARITY_ERROR_MSG_FAILED:
			case SCS_ID_MSG_FAILED:
			case SCS_UNEXPECTED_BUS_FREE:
			default:
				ql_tpmsg(ha->ctlr_vhdl, request->sr_target,
					 "HARD ERROR: "
					 "completion status [0x%x] (%s), "
					 "scsi status[0x%x], state flags[0x%x], status flags[0x%x]\n", 
					 q_ptr->completion_status,
					 ql_completion_status_msg(q_ptr->completion_status),
					 q_ptr->scsi_status,
					 q_ptr->state_flags,
					 q_ptr->status_flags);
				request->sr_status = SC_HARDERR;
				break;
			} /* switch (q_ptr->completion_status) */
#ifdef QL_STATS
			if(q_ptr->completion_status) 
				ha->adp_stats->disk[request->sr_target].adap_spec.ql.errors++;
#endif

			/*
			 * return this reqblock to free state 
			 */
			r_ptr->req = NULL;

			/* 
			 * does this completeion complete our QUIESCE_IN_PROGRESS 
			 */
			if ((ha->host_flags & AFLG_HA_QUIESCE_IN_PROGRESS) &&
			    (ha->ql_ncmd == 0)) 
			{
				atomicSetInt(&ha->host_flags, AFLG_HA_QUIESCE_COMPLETE);
				atomicClearInt(&ha->host_flags, AFLG_HA_QUIESCE_IN_PROGRESS);
				untimeout(ha->quiesce_in_progress_id);
				ha->quiesce_id = timeout(kill_quiesce, ha, ha->quiesce_time, KILL_WITHOUT_TIMEOUT);
			}

			request->sr_scsi_status = q_ptr->scsi_status;

			/* 
			 * If a SCSI bus reset occurred, then we need to 
			 * issue a marker before issuing any new requests. 
			 */
			if (q_ptr->completion_status == SCS_RESET_OCCURRED)
				atomicSetInt(&ha->host_flags, AFLG_HA_SEND_MARKER);

			/*
			 * Save any request sense info.  
			 */
			if (q_ptr->state_flags & SS_GOT_SENSE) 
			{
				atomicSetInt(&qli->qli_dev_flags, DFLG_CONTINGENT_ALLEGIANCE);

				/*
				 * Go abort all other queued commands if QERR=1
				 */
				if ((qli->qli_dev_flags & DFLG_BEHAVE_QERR1) && 
				    ((qli->qli_dev_flags & DFLG_ABORT_IN_PROGRESS) == 0) &&
				    (qli->qli_cmd_rcnt > 0)) {
					u_short mbox_sts[8];
					
					atomicSetInt(&qli->qli_dev_flags, DFLG_ABORT_IN_PROGRESS);
					
					IOLOCK(ha->chip_info->req_lock, s);
					if (ql_set_abort_device(ha->chip_info, mbox_sts, 
					  ha->channel_id, request->sr_target, request->sr_lun)) {
						ql_lpmsg(ha->ctlr_vhdl, request->sr_target, request->sr_lun, 
							 "MBOX_CMD_ABORT_DEVICE Command failed\n");
					}

					IOUNLOCK(ha->chip_info->req_lock, s);
				}

				munge(q_ptr->req_sense_data, REQ_SENSE_DATA_LENGTH);

				if (ql_printsense) {

					ql_sensepr(ha->ctlr_vhdl,
				   		request->sr_target,
				   		q_ptr->req_sense_data,
						q_ptr->req_sense_length);

					if (ql_printsense > 1) {
						int slen = q_ptr->req_sense_length;
						u_char *sptr = q_ptr->req_sense_data;

						cmn_err(CE_CONT, "Sense bytes in hex: ");

						while (slen-- > 0) 
							cmn_err(CE_CONT, "%x ", *sptr++);

						cmn_err(CE_CONT, "\n\n");
					}
				}

				if (request->sr_sense == NULL) {

					ql_tpmsg(ha->ctlr_vhdl,
						 request->sr_target,
						 "No place for sense data (CDB 0x%x)\n",
						 request->sr_command[0]);
				} 
				else 
				{ 
					struct scsi_target_info *tinfo;


					request->sr_sensegotten = q_ptr->req_sense_length;
									                                
					bcopy(q_ptr->req_sense_data,
					      request->sr_sense,
					      request->sr_sensegotten);

					tinfo = ql_lun_target_info_get(request->sr_lun_vhdl);
					bcopy(request->sr_sense,
					      tinfo->si_sense,
					      request->sr_sensegotten);
					if (qli->qli_sense_callback)
						(*qli->qli_sense_callback)(request->sr_lun_vhdl, tinfo->si_sense);
				}
			} /* if (q_ptr->state_flags & SS_GOT_SENSE) */
			
			/* 
			 * Link this request and return them later.  
			 */
			if (cmp_forw) {
				cmp_back->sr_ha = request;
				cmp_back        = request;
				request->sr_ha  = NULL;
			}
			else
			{
				cmp_forw = request;
				cmp_back = request;
			}
			request->sr_ha = NULL;
		} /* while ((chip_info->response_out != response_in)) */

		/* 
		 * Update the ISP's response tail pointer
		 */
		QL_PCI_OUTH(&isp->mailbox5, chip_info->response_out);

		QL_PCI_OUTH(&isp->hccr, HCCR_CMD_CLEAR_RISC_INT);

		/* Notify caller of command competions */
		ql_notify_responses(chip_info, cmp_forw);
		cmp_forw = cmp_back = NULL;

		/* Get a new copy of the ISR. */
		isr = QL_PCI_INH(&isp->bus_isr);

	} while ((isr & BUS_ISR_RISC_INT) || (chip_info->chip_flags & AFLG_CHIP_ASYNC_RESPONSE_IN)); /* TAG 1 */

 got_responses:
	/* 
	 * Some resources may have become available. See if there
	 * are any requests to send.  
	 */
	{
		int i;
		int cmds=0;

		for (i=0; i< chip_info->channel_count; i++) {
			ha = chip_info->ha_info[i];
			if (!(ha->host_flags & AFLG_HA_DRAIN_IO) && ((ha->waitf) || (ha->req_forw)))
				++cmds;
		}
		if (cmds)
			ql_start_scsi(chip_info);
	}
}

static void
ql_notify_responses(pCHIP_INFO		chip_info, 
		    scsi_request_t	*forwp)
{
	scsi_request_t	*request;
	
	UNLOCK(chip_info->res_lock);
	while (request = forwp) 
	{
		forwp = request->sr_ha;

#ifdef HEART_INVALIDATE_WAR
		/* if this IO resulted in a DMA to memory from IO (disk read) 	*/
		/* then invalide cache copies of the data 			*/
		if (request->sr_flags & SRF_DIR_IN) 
		{
			if (request->sr_flags & SRF_MAPBP) 
			{
				bp_heart_invalidate_war(request->sr_bp);
			}
			else 
			{
				heart_invalidate_war(request->sr_buffer,request->sr_buflen);
			}
		}
#endif /* HEART_INVALIDATE WAR */

		/* 
		 * Callback now - As a result of callback processing we
		 * may have executed a mailbox command, which may have
		 * seen some IOCB completions. AFLG_ASYNC_RESPONSE_IN will
		 * be set if so.
		 */
		(*request->sr_notify)(request);

	} /* while (request = forwp) */
	LOCK(chip_info->res_lock);
}

/* Function:    ql_start_scsi
**
** Description: This routine tries to start up any new requests we have
**      pending on our internal queue.  This may be new commands
**      or it may be ones that we couldn't start berfore because
**      the adapter queues were full.
*/
static void	
ql_start_scsi(pCHIP_INFO chip_info)
{
    	pISP_REGS	isp = (pISP_REGS)chip_info->ha_base;
	register int	zero_cnt;

	/* REFERENCED */
	int		s;

	/* 
	 * Some other cpu is running this code so our job will get
	 * start by that cpu.  
	 */
	if (mutex_trylock(&chip_info->req_lock) == 0) {
		return;
	}

	/*
         * Check the wait queue(s). If there's something in there,
         * move it all over to the request queue.  Check the request
         * queues for any requests. If nothing, then bail out now.
         */
        {
                register int            i;
                register int            cmds = 0;
                register pHA_INFO       ha;

                for (i = 0; i < chip_info->channel_count; ++i) {
                        ha = chip_info->ha_info[i];
                        IOLOCK(ha->waitQ_lock,s);
                        if (ha->waitf)
                        {
                                if (ha->req_forw)
                                        ha->req_back->sr_ha = ha->waitf;
                                else
                                        ha->req_forw = ha->waitf;
                                ha->req_back = ha->waitb;
                                ha->waitf = ha->waitb = NULL;
                                ha->req_back->sr_ha = NULL;
                        }
                        IOUNLOCK(ha->waitQ_lock,s);
                        if (ha->req_forw && ((ha->host_flags & AFLG_HA_QUEUE_PROGRESS_FLAGS) == 0))
                                ++cmds;
                }
                if (cmds == 0) {
                        IOUNLOCK(chip_info->req_lock, s);
                        return;
                }
        }




	/*
	** If we're going to do a scatter/gather operation we may need
	** to use more than one queue entry (one for the request and one
	** or more for the scatter/gather table).  So, if we have less than
	** two queue slots available (might need one for a marker too)
	** then zap the number of queue entries so we'll go look again.
	** This is kindof a kludge, but ...  we's does whats we needs to do.
	*/
    	if ( chip_info->queue_space <= 2) {
		chip_info->queue_space = 0;    /* go look for "number queue slots" */
    	}

	/*
	** Determine how many entries are available on the request queue.
	**
	** This is kinda neat -- instead of always checking if there are
	** any new slots RIGHT NOW, we keep track of how many slots there
	** were the last time we checked.  Since we're the only one filling
	** these, there will always be AT LEAST that many still available.
	** Once we reach that number, theres no sense in issuing any more
	** until he's taken some.  So, when we filled up the number of slots
	** available on our last check, THEN we check again (to see if there
	** is anymore slots available).
	**
	** If we filled all the slots on the last pass, then check to see
	** how many slots there are NOW!
	*/
    	if (chip_info->queue_space == 0) {
        	ql_queue_space(chip_info, isp);
    	}

	 /*
         * We now loop thru all available channels, taking one command
         * off each channel's requests queue in turn, and sending it
         * if there's space in the request queue. We use a "rotor" to
         * identify the next channel a command is to be sent to. When
         * all channels have no remaining commands, we exit the loop
         */
        zero_cnt = 0;
        while (chip_info->queue_space && zero_cnt < chip_info->channel_count)
        {
                int                     i;
                pHA_INFO                ha;
                scsi_request_t          *request;
                ql_local_info_t         *qli;
                command_entry           *q_ptr;
                pREQ_INFO               r_ptr;
                alenlist_t              alenlist;
                alenaddr_t              address;
                size_t                  length;
                data_seg                dseg;
                int                     tgt;
                int                     entries = 0, index, ri_slot;
                int                     allow_prefetch;

                i = chip_info->req_rotor++;
                chip_info->req_rotor %= chip_info->channel_count;

		if (i >= chip_info->channel_count)
			panic("i is greater than the channel count");
                ha = chip_info->ha_info[i];
                if (((request = ha->req_forw) == NULL) ||
                    (ha->host_flags & AFLG_HA_QUEUE_PROGRESS_FLAGS) ||
                    ((ri_slot = reqi_get_slot(ha, (int)request->sr_target)) == 0))
                {
                        ++zero_cnt;
                        continue;
                }
                zero_cnt = 0;



		/* 
	 	* If we need to send a global Marker do it now
	 	*/
    		if (ha->host_flags & AFLG_HA_SEND_MARKER) {

			register marker_entry * q_ptr;

			atomicClearInt(&ha->host_flags, AFLG_HA_SEND_MARKER);
			q_ptr = (marker_entry * )chip_info->request_ptr;
			if (chip_info->request_in == (chip_info->ql_request_queue_depth - 1)) {
	    			chip_info->request_in  = 0;
	    			chip_info->request_ptr = chip_info->request_base;
			} else {
	    			chip_info->request_in++;
	    			chip_info->request_ptr++;
			}

			bzero(q_ptr, sizeof(marker_entry));

			q_ptr->hdr.entry_type   = ET_MARKER;
			q_ptr->hdr.entry_cnt    = 1;
			q_ptr->hdr.flags    = 0;
			q_ptr->hdr.sys_def_1    = chip_info->request_in;

			q_ptr->reserved0    = 0;    /* just in case */
			q_ptr->channel_id   = ha->channel_id;
			q_ptr->target_id    = 0;
			q_ptr->target_lun   = 0;
			q_ptr->reserved1    = 0;    /* just in case */
			q_ptr->modifier     = MM_SYNC_ALL;

#if (defined(IP30) || defined(SN)) && !defined(USE_MAPPED_CONTROL_STREAM)
			munge((u_char * )q_ptr, 64);
#endif
#ifdef DEBUG
			if (ql_debug >= 2)
				dump_marker(q_ptr);
#endif

#ifdef QL1240_SWAP_WAR
			if ((chip_info->device_id == QLogic_1240_DEVICE_ID) ||
			    (chip_info->device_id == QLogic_1080_DEVICE_ID) ||
			    (chip_info->device_id == QLogic_1280_DEVICE_ID) )
                                munge_iocb((u_char * )q_ptr, 64);
#endif

			/* flush the entry out of the cache */
#ifdef FLUSH
			MR_DCACHE_WB_INVAL(q_ptr, sizeof(marker_entry));
#endif
			/* Tell isp it's got a new I/O request...  */
			QL_PCI_OUTH(&isp->mailbox4, chip_info->request_in);

			/* One less I/O slot.  */
			chip_info->queue_space--;

#ifdef QL_STATS
			ha->adp_stats->adap_spec.ql.markers++;
#endif
    		} /* if (ha->host_flags & AFLG_SEND_MARKER) */

		/*
		** See if we can start one or more requests.
		** This means there's room in the queue, we have another request
		** to start, and we have a request structure for the request.
		*/

		r_ptr = &ha->reqi_block[request->sr_target][ri_slot% (MAX_REQ_INFO + 1)];
		qli = SLI_INFO(scsi_lun_info_get(request->sr_lun_vhdl));

		/*
		 * Send a DEVICE SYNCH marker if we need to
		 */
		if ((qli->qli_dev_flags & (DFLG_SEND_MARKER|DFLG_ABORT_IN_PROGRESS)) == DFLG_SEND_MARKER) {
			register marker_entry *q_ptr;

			atomicClearInt(&qli->qli_dev_flags, DFLG_SEND_MARKER);

			q_ptr = (marker_entry * )chip_info->request_ptr;
			if (chip_info->request_in == (chip_info->ql_request_queue_depth - 1)) {
				chip_info->request_in  = 0;
				chip_info->request_ptr = chip_info->request_base;
			} else {
				chip_info->request_in++;
				chip_info->request_ptr++;
			}

			bzero(q_ptr, sizeof(marker_entry));

			q_ptr->hdr.entry_type   = ET_MARKER;
			q_ptr->hdr.entry_cnt    = 1;
			q_ptr->hdr.flags        = 0;
			q_ptr->hdr.sys_def_1    = chip_info->request_in;

			q_ptr->reserved0    = 0;    /* just in case */
			q_ptr->channel_id   = ha->channel_id;
			q_ptr->target_id    = request->sr_target;
			q_ptr->target_lun   = request->sr_lun;
			q_ptr->reserved1    = 0;    /* just in case */
			q_ptr->modifier     = MM_SYNC_DEVICE;

#if (defined(IP30) || defined(SN)) && !defined(USE_MAPPED_CONTROL_STREAM)
			munge((u_char * )q_ptr, 64);
#endif
 
#ifdef QL1240_SWAP_WAR
			if ((chip_info->device_id == QLogic_1240_DEVICE_ID) ||
			    (chip_info->device_id == QLogic_1080_DEVICE_ID) || 
			    (chip_info->device_id == QLogic_1280_DEVICE_ID) )
                                munge_iocb((u_char * )q_ptr, 64);
#endif

#ifdef FLUSH
			/* flush the entry out of the cache */
			MR_DCACHE_WB_INVAL(q_ptr, sizeof(marker_entry));
#endif

			/* Tell isp it's got a new I/O request...  */
			QL_PCI_OUTH(&isp->mailbox4, chip_info->request_in);

			/* One less I/O slot.  */
			chip_info->queue_space--;

#ifdef QL_STATS
			ha->adp_stats->adap_spec.ql.markers++;
#endif
		}

		allow_prefetch = 1;

		if (request->sr_buflen) 
		{
			/*
			 * If buf describes a user virtual address,
			 * dksc will have constructed a alenlist,
			 * stuck it in bp->b_private and set the
			 * SRF_ALENLIST flag.
			 */
			if (request->sr_flags & SRF_ALENLIST) 
			{
				if (!IS_KUSEG(request->sr_buffer))
					panic("ql_start_scsi: address not UVADDR\n");
				alenlist = (alenlist_t)((buf_t *)(request->sr_bp))->b_private;
				alenlist_cursor_init(alenlist, 0, NULL);
			}
			else
	    		if ( !(request->sr_flags & SRF_MAP) && (request->sr_flags & SRF_MAPBP)) 
			{
				if (BP_ISMAPPED(((buf_t *)(request->sr_bp))))
					panic("ql_start_scsi: buffer is mapped\n");
				alenlist = chip_info->alen_p;
#ifdef FLUSH
				bp_dcache_wbinval((buf_t *)(request->sr_bp));
#endif
				buf_to_alenlist(alenlist, request->sr_bp, AL_NOCOMPACT);
	    		} 
			else 
			{
				alenlist = chip_info->alen_p;
#ifdef FLUSH
				if (request->sr_flags & SRF_DIR_IN)
		    			DCACHE_INVAL(request->sr_buffer, request->sr_buflen);
				else
		    			MR_DCACHE_WB_INVAL(request->sr_buffer, request->sr_buflen);
#endif

				if (IS_KUSEG(request->sr_buffer))
					panic("ql_start_scsi: address is a UVADDR\n");
				kvaddr_to_alenlist(alenlist,
						   (caddr_t)request->sr_buffer,
						   request->sr_buflen, AL_NOCOMPACT);
	    		}
	    		entries = alenlist_size(alenlist);

			/*
			 * Ensure request doesn't span more than 254
			 * continuation entries, i.e. length doesn't
			 * exceed 20.8 MB.  If it exceeds the
			 * max. length, unlink the request and toss it
			 * now.
			 */
			if (((entries - IOCB_SEGS) / CONTINUATION_SEGS + 1) > MAX_CONTINUATION_ENTRIES) 
			{ 
				ql_cpmsg(ha->ctlr_vhdl, "Transfer length (0x%x) too large; "
					 "spans more than %d continuation entries.\n",
					 request->sr_buflen, MAX_CONTINUATION_ENTRIES);
				ha->req_forw  = request->sr_ha;
				if (ha->req_forw == NULL)
					ha->req_back = NULL;
				request->sr_status = SC_REQUEST;
				(*request->sr_notify)(request);
				continue;
			}		

			
			if ((((entries - IOCB_SEGS) / CONTINUATION_SEGS) + 1) >= chip_info->queue_space) 
			{
		    		/* better check the real space available before  */
                    		/* giving up                                     */
		    		ql_queue_space(chip_info, isp);
			}
			if ((((entries - IOCB_SEGS) / CONTINUATION_SEGS) + 1) >= chip_info->queue_space) 
			{
		    		chip_info->queue_space = 0;
		    		break;
			}

#ifdef BRIDGE_B_PREFETCH_DATACORR_WAR
			/* 
			 * If rev B bridge, disable prefetch
			 * for all pages comprising that I/O
			 */
			if ( chip_info->bridge_revnum == (u_short)-1 || chip_info->bridge_revnum <= BRIDGE_REV_B )
				allow_prefetch = 0;
#endif
		}

		ASSERT(ha == (pHA_INFO)request->sr_spare);

	    	/* Unhook the request and the request info structure.  */
	    	ha->req_forw  = request->sr_ha;
		if (ha->req_forw == NULL)
		        ha->req_back = NULL;

	    	/* Initialize the request info structure for this request.  */
	    	r_ptr->req = request;

	    	r_ptr->timeout = (request->sr_timeout / HZ) + WATCHDOG_TIME;
	    	r_ptr->cmd_age = 0;
		r_ptr->starttime = lbolt/HZ; 
	    	tgt = request->sr_target;
	    	IOLOCK(ha->timeout_info[tgt].per_tgt_tlock,s);
	    	i = r_ptr->timeout - ha->timeout_info[tgt].time_base;
	    	if ( i == 0) {
			ha->timeout_info[tgt].ql_dups++;
			ha->timeout_info[tgt].current_timeout = 
			ha->timeout_info[tgt].time_base;
	    	}
	    	if ( i > 0 ) {
			ha->timeout_info[tgt].ql_dups = 0;
			ha->timeout_info[tgt].time_base = 
			ha->timeout_info[tgt].current_timeout = r_ptr->timeout;
	    	}
		
	    	ha->timeout_info[tgt].ql_tcmd++;
	    	IOUNLOCK(ha->timeout_info[tgt].per_tgt_tlock,s);
	    	atomicAddInt(&(ha->ql_ncmd),1);

		/*
		 * Bump the LUN command issued count
		 */
		atomicAddInt(&qli->qli_cmd_rcnt, 1);

		/* Get a pointer to the queue entry for the command.  */
		q_ptr = (command_entry * )chip_info->request_ptr;


		/* Move the internal pointers for the request queue.  */
		if (chip_info->request_in == (chip_info->ql_request_queue_depth - 1)) {
		    	chip_info->request_in  = 0;
		    	chip_info->request_ptr = chip_info->request_base;

		} else {
		    	chip_info->request_in++;
		    	chip_info->request_ptr++;
		}

		bzero(q_ptr, sizeof(queue_entry));
		/* Fill in the command header.  */
		q_ptr->hdr.entry_type   = ET_COMMAND;
		q_ptr->hdr.entry_cnt    = 1;
		q_ptr->hdr.flags    	= 0;
		q_ptr->hdr.sys_def_1    = chip_info->request_in;

		/* Fill in the rest of the command data.  */
		q_ptr->handle       = ri_slot | (ha->channel_id << 12);
		q_ptr->channel_id   = ha->channel_id;
		q_ptr->target_id    = request->sr_target;
		ASSERT(request->sr_target != ha->ql_defaults.Initiator_SCSI_Id);
		q_ptr->target_lun   = request->sr_lun;
		q_ptr->control_flags = 0;
#ifndef A64_BIT_OPERATION
		q_ptr->reserved     = 0;
#endif
		if (request->sr_timeout)
		    	q_ptr->time_out = 0; /* timeouts are handle in the driver */
		else 
		    	q_ptr->time_out = DEFAULT_TIMEOUT;

		if (request->sr_command[0] == INQUIRY_CMD) 
		{
			if (ha->ql_defaults.Id[request->sr_target].Capability.bits.Sync_Data_Transfers == 1)
				q_ptr->control_flags |= CF_FORCE_SYNCHONOUS;
			else
			if (!ha->ql_defaults.Id[request->sr_target].Is_CDROM || ql_allow_negotiation_on_cdroms)
				q_ptr->control_flags |= CF_FORCE_ASYNCHONOUS;

			if (ha->ql_defaults.Id[request->sr_target].Capability.bits.Wide_Data_Transfers == 1)
				q_ptr->control_flags |= CF_INITIATE_WIDE;
		}

		/* Copy over the requests SCSI cdb.  */
		q_ptr->cdb_length = request->sr_cmdlen;

                /*
                 * If vendor unique command is of say 7 bytes,
                 * we will copy 3 extra garbage bytes below. The
                 * assumption is that it is an unusual occurrence.
                 */

                if (request->sr_cmdlen > 10) {
                        q_ptr->cdb11 = request->sr_command[11];
                        q_ptr->cdb10 = request->sr_command[10];
                }

                if (request->sr_cmdlen > 6) {
                        q_ptr->cdb9 = request->sr_command[9];
                        q_ptr->cdb8 = request->sr_command[8];
                        q_ptr->cdb7 = request->sr_command[7];
                        q_ptr->cdb6 = request->sr_command[6];
                }

                q_ptr->cdb5 = request->sr_command[5];
                q_ptr->cdb4 = request->sr_command[4];
                q_ptr->cdb3 = request->sr_command[3];
                q_ptr->cdb2 = request->sr_command[2];
                q_ptr->cdb1 = request->sr_command[1];
                q_ptr->cdb0 = request->sr_command[0];

		if (request->sr_buflen) {
		    	if (!(request->sr_flags & SRF_DIR_IN)) {
			    q_ptr->control_flags |= CF_WRITE;

#ifdef FLUSH
			    if ( (request->sr_flags & SRF_MAP) && 
			   	!(request->sr_flags & SRF_MAPBP)) 
			    {
					if (request->sr_buffer)
	    			    		MR_DCACHE_WB_INVAL(request->sr_buffer,
							 request->sr_buflen);
    			    }	

#endif
			} else {
			    q_ptr->control_flags |= CF_READ;
#ifdef FLUSH
			    if (request->sr_buffer)
			        DCACHE_INVAL(request->sr_buffer,
					     request->sr_buflen);
#endif
			}
		    } else {
			q_ptr->segment_cnt = 0;
		    }

		    if (request->sr_flags & SRF_NEG_SYNC) 
		    {
			    ha->ql_defaults.Id[request->sr_target].Sync_Allowed = 1;
			    ha->ql_defaults.Id[request->sr_target].Force_Sync = 1;
			    if (ha->ql_defaults.Id[request->sr_target].Capability.bits.Sync_Data_Transfers == 0) 
			    {
				    ha->ql_defaults.Id[request->sr_target].Capability.bits.Sync_Data_Transfers = 1;
				    atomicClearInt(&qli->qli_dev_flags, DFLG_INITIALIZED);
			    }
			    q_ptr->control_flags |= CF_FORCE_SYNCHONOUS;
		    }
		    else 
		    if (request->sr_flags & SRF_NEG_ASYNC) {
			    ha->ql_defaults.Id[request->sr_target].Sync_Allowed = 0;
			    ha->ql_defaults.Id[request->sr_target].Force_Sync = 0;
			    if (ha->ql_defaults.Id[request->sr_target].Capability.bits.Sync_Data_Transfers == 1) 
			    {
				    ha->ql_defaults.Id[request->sr_target].Capability.bits.Sync_Data_Transfers = 0;
				    atomicClearInt(&qli->qli_dev_flags, DFLG_INITIALIZED);
			    }
			    q_ptr->control_flags |= CF_FORCE_ASYNCHONOUS;
		    }

		    /* If we said we support tagged commands */
		    /* and the drive is configured for tagged support, */
		    /* then they pass us the tag information in the */
		    /* hostadapter command parameter. */
		    /* Pass it along to the host adapter. */
		    if (ha->ql_defaults.Id[request->sr_target].Capability.bits.Tagged_Queuing) {
				switch (request->sr_tag) {
					case SC_TAG_HEAD:
			    			q_ptr->control_flags |= CF_HEAD_TAG;
			    			break;
					case SC_TAG_ORDERED:
			    			q_ptr->control_flags |= CF_ORDERED_TAG;
			    			break;
					case SC_TAG_SIMPLE:
			    			q_ptr->control_flags |= CF_SIMPLE_TAG;
			    			break;
				}
		    }

		    /* If this is a scatter/gather request, then we need */
		    /* to adjust the data address and lengths for the    */
		    /* scatter entries. 				 */

		    /* If there are more than IOCB_SEGS entries in the scatter*/
		    /* gather list, then we're going to be using some         */
		    /* contiuation queue entries.  So, tell the command entry */
		    /* how many continuations to expect.  Add two to include  */
		    /* the command.*/
		    if (entries > IOCB_SEGS) {
				int	i = 1;
				if ((entries - IOCB_SEGS) % CONTINUATION_SEGS) 
			    		i = 2;
				q_ptr->hdr.entry_cnt = ((entries - IOCB_SEGS) / CONTINUATION_SEGS) + i;
		    }

		    /* Also, update the segment count with the number of */
		    /* entries in the scatter/gather list.  */

		    q_ptr->segment_cnt = entries;

		    /* The command queue entry has room for four         */
		    /* scatter/gather entries.  Fill them in.            */
		    for (i = 0; i < IOCB_SEGS && entries; i++, entries--) {
				data_seg	*dsp;

				if (alenlist_get(alenlist, NULL, (size_t)(1<<30),
						 &address, &length, 0) != ALENLIST_SUCCESS)
				{
					panic("ql - bad alen conversion, alen 0x0x, sreq 0x%x, br 0x%x\n", 
					      alenlist, request, request->sr_bp);    
				}
				dsp = &dseg;
				fill_sg_elem(ha, dsp, address, length, allow_prefetch);
#ifdef A64_BIT_OPERATION
				q_ptr->dseg[i].base_high_32 = dsp->base >>32;
#endif
				q_ptr->dseg[i].base_low_32  = dsp->base;
				q_ptr->dseg[i].count = dsp->count;
		    }
		    /* If there is still more entries in the scatter/gather */
		    /* list, then we need to use some continuation queue    */
		    /* entries.  While there are more entries in the        */
		    /* scatter/gather table, allocate a continuation queue  */
	            /* entry and fill it in.  */

		    index = IOCB_SEGS; /* set the index for the */

		    /* next sg element pair  */
		    while (entries) {
				register continuation_entry * c_ptr;

			/* Get a pointer to the next queue entry for the */
			/* continuation of the command.  */
				c_ptr = (continuation_entry * )chip_info->request_ptr;

			/* Move the internal pointers for the request queue.  */
				if (chip_info->request_in == (chip_info->ql_request_queue_depth - 1)) {
			    		chip_info->request_in  = 0;
			    		chip_info->request_ptr = chip_info->request_base;

				} else {
			    		chip_info->request_in++;
			    		chip_info->request_ptr++;
				}

			/* Fill in the continuation header.  */
			/*zero out the entry. might take out later */
				bzero(c_ptr, sizeof(continuation_entry));
				c_ptr->hdr.entry_type   = ET_CONTINUATION;
				c_ptr->hdr.entry_cnt    = 1;
				c_ptr->hdr.flags    = 0;
				c_ptr->hdr.sys_def_1    = chip_info->request_in;
#ifndef A64_BIT_OPERATION
				c_ptr->reserved     = 0;
#endif
			/* Fill in the scatter entries for this continuation. */
				for (i = 0; i < CONTINUATION_SEGS && entries; i++, entries--) {
			    		data_seg	*dsp;

			    		if (alenlist_get(alenlist, NULL, (size_t)(1<<30),
					    &address, &length, 0) != ALENLIST_SUCCESS) {
					printf("ql - bad alen conversion\n");
			    		}
			    		dsp = &dseg;
			    		fill_sg_elem(ha, dsp, address, length, allow_prefetch);
#ifdef A64_BIT_OPERATION
			    		c_ptr->dseg[i].base_high_32 = dsp->base >> 32;
#endif
			    		c_ptr->dseg[i].base_low_32  = dsp->base;
			    		c_ptr->dseg[i].count = dsp->count;
			    		index++;
				}
#ifdef DEBUG
                        	if (ql_debug >= 2) {
					dump_continuation((continuation_entry * )c_ptr);
                        	}
#endif

				/* One less I/O slot.  */
				chip_info->queue_space--;

				/*flush the entry out of the cache */
#if (defined(IP30) || defined(SN)) && !defined(USE_MAPPED_CONTROL_STREAM)
				munge((u_char * )c_ptr, 64);
#endif

#ifdef QL1240_SWAP_WAR
			if ((chip_info->device_id == QLogic_1240_DEVICE_ID) ||
			    (chip_info->device_id == QLogic_1080_DEVICE_ID) ||
			    (chip_info->device_id == QLogic_1280_DEVICE_ID) )
                                	munge_iocb((u_char * )c_ptr, 64);
#endif

#ifdef FLUSH
				MR_DCACHE_WB_INVAL(c_ptr, sizeof(continuation_entry));
#endif
		    } /* while entries */
#ifdef DEBUG
                    if (ql_debug >= 2) {
			dump_iocb((command_entry * )q_ptr);   
                    }
#endif

#if (defined(IP30) || defined(SN)) && !defined(USE_MAPPED_CONTROL_STREAM)

		    munge((u_char * )q_ptr, 64);
#endif
#ifdef DEBUG
		    if (ql_debug >= 2) 
			    dump_iocb(q_ptr);
#endif

#ifdef QL1240_SWAP_WAR
			if ((chip_info->device_id == QLogic_1240_DEVICE_ID) ||
			    (chip_info->device_id == QLogic_1080_DEVICE_ID) ||
			    (chip_info->device_id == QLogic_1280_DEVICE_ID) )
                                munge_iocb((u_char * )q_ptr, 64);
#endif

		    /*flush the entry out of the cache */
#ifdef FLUSH
		    MR_DCACHE_WB_INVAL(q_ptr, sizeof(command_entry));
#endif

		    /* Tell isp it's got a new I/O request... */
		    /* Wake up */
		    QL_PCI_OUTH(&isp->mailbox4, chip_info->request_in);

		    /* One less I/O slot.  */
		    chip_info->queue_space--;

#ifdef QL_STATS

		    /* if there are no outstanding commands for this  */
		    /* controller then transition from idle to busy state.*/
		    /* Same for the device. transition means:     */
		    /*   1. add time to the idle time.     */
		    /*   2. put new time stamp in last_busy field     */
		    if (!(ha->adp_stats->cmds - ha->adp_stats->cmds_completed)) {
			/* no outstanding commands for this adapter 	*/
			/* going from idle to busy 			*/
			/* add delta time to idle time and update new   */
			/* busy time */
			microtime(&ha->adp_stats->last_busy);
			add_deltatime(&ha->adp_stats->last_busy, &ha->adp_stats->last_idle, 
			&ha->adp_stats->idle);

		    }
		    if (!(ha->adp_stats->disk[q_ptr->target_id].cmds - 
		        ha->adp_stats->disk[q_ptr->target_id].cmds_completed)) {

			microtime(&ha->adp_stats->disk[q_ptr->target_id].last_busy);
			add_deltatime(&ha->adp_stats->disk[q_ptr->target_id].last_busy, 
			&ha->adp_stats->disk[q_ptr->target_id].last_idle,
		  	        &ha->adp_stats->disk[q_ptr->target_id].idle);
		    }


		    ha->adp_stats->disk[q_ptr->target_id].cmds++;
		    ha->adp_stats->cmds++;
#endif /* !defined(_STANDALONE) && defined(QL_STATS) */

		/*
		 * Check the waitQ to see if something's been queued
		 * while we held the req_lock and since the last time
		 * we checked the waitQ.
		 */
		if (ha->req_forw == NULL) {
			IOLOCK(ha->waitQ_lock, s);
			if (ha->waitf) {
				ha->req_forw = ha->waitf;
				ha->req_back = ha->waitb;
			}
			ha->waitf = ha->waitb = NULL;
			IOUNLOCK(ha->waitQ_lock, s);
		}
	}
	IOUNLOCK(chip_info->req_lock, s);
}

/* Function: ql_queue_space */
static void 
ql_queue_space(pCHIP_INFO chip_info, pISP_REGS isp)
{
    	u_short mailbox4 = 0;

    	mailbox4 = QL_PCI_INH(&isp->mailbox4);

    	chip_info->request_out = mailbox4;

	/*
	** If the pointers are the same, then ALL the slots are available.
	** We have to account for wrap condition, so check both conditions.
	*/
    	if (chip_info->request_in == chip_info->request_out) {
        	chip_info->queue_space = chip_info->ql_request_queue_depth - 1;
    	} else if (chip_info->request_in > chip_info->request_out) {
        	chip_info->queue_space = ((chip_info->ql_request_queue_depth - 1) -
            	(chip_info->request_in - chip_info->request_out));
    	} else {
        	chip_info->queue_space = (chip_info->request_out - chip_info->request_in) - 1;
    	}
}

/* Function:    ql_mbox_cmd()
**
** Return:  0   no error
**      non-0   error
**
** Description: This routine issues a mailbox command.
**      We're passed a list of mailboxes and the number of
**      mailboxes to read and the number to write.
**
** Note: I've added a special case (that may not be any problem)
** to skip loading mailbox 4 if the command is INITIALIZE RESPONSE QUEUE.
*/

static int	
ql_mbox_cmd(pCHIP_INFO chip,
	    u_short *mbox_sts,
	    u_char out_cnt, u_char in_cnt,
	    u_short reg0, u_short reg1, u_short reg2, u_short reg3, 
	    u_short reg4, u_short reg5, u_short reg6, u_short reg7)
{
    	pISP_REGS isp      = (pISP_REGS)chip->ha_base;
    	int	  complete  = 0;
    	int	  retry_cnt = 0;
	int       intr_retry_cnt = 0;
    	u_short   isr;
	u_short   bus_sema;
	u_short   hccr;
    
	QL_MUTEX_EXIT(chip);

    	LOCK(chip->mbox_lock);

	QL_MUTEX_ENTER(chip);

/*
** Just for grins, clear out ALL the return status.
** This might catch a bug later where I try to use a return
** parameter that I wasn't given (would be left over from previous).
*/
    	mbox_sts[0] = 0;
    	mbox_sts[1] = 0;
    	mbox_sts[2] = 0;
    	mbox_sts[3] = 0;
    	mbox_sts[4] = 0;
    	mbox_sts[5] = 0;
    	mbox_sts[6] = 0;
    	mbox_sts[7] = 0;

	switch (out_cnt) {
	    case 8:
		QL_PCI_OUTH(&isp->mailbox7, reg7);
	    case 7:
		QL_PCI_OUTH(&isp->mailbox6, reg6);
	    case 6:
		QL_PCI_OUTH(&isp->mailbox5, reg5);
	    case 5:
		if ( (reg0 != MBOX_CMD_INIT_RESPONSE_QUEUE) && 
			(reg0 != MBOX_CMD_SET_RETRY_COUNT)  &&
			(reg0 != MBOX_CMD_INIT_RESPONSE_QUEUE_64) )  
		    QL_PCI_OUTH(&isp->mailbox4, reg4);
	    case 4:
		QL_PCI_OUTH(&isp->mailbox3, reg3);
	    case 3:
		QL_PCI_OUTH(&isp->mailbox2, reg2);
	    case 2:
		QL_PCI_OUTH(&isp->mailbox1, reg1);
	    case 1:
		QL_PCI_OUTH(&isp->mailbox0, reg0);
	}

	/*
	 * If called from ISR, then we'll need to poll for mbox
	 * completion. Else wait on semaphore.
	 */
	if (chip->chip_flags & (AFLG_CHIP_IN_INTR | AFLG_CHIP_DUMPING | AFLG_CHIP_WATCHDOG_TIMER | AFLG_CHIP_INITIAL_SRAM_PARITY)) {
		atomicSetInt(&chip->chip_flags, AFLG_CHIP_MAIL_BOX_POLLING);
		atomicClearInt(&chip->chip_flags, AFLG_CHIP_MAIL_BOX_WAITING);
	}
	else {
		chip->mbox_timeout_info = 10 + WATCHDOG_TIME; /* mailbox timeout is at least 10 seconds */
		atomicSetInt(&chip->chip_flags, AFLG_CHIP_MAIL_BOX_WAITING);
		atomicClearInt(&chip->chip_flags, AFLG_CHIP_MAIL_BOX_POLLING);
	}
	atomicClearInt(&chip->chip_flags, AFLG_CHIP_MAIL_BOX_DONE);

	/* Wake up the isp.  */
	QL_PCI_OUTH(&isp->hccr, HCCR_CMD_SET_HOST_INT);

	
	QL_MUTEX_EXIT(chip);

	/*
	 * Wait for mailbox done semaphore ......
	 */
	if (chip->chip_flags & AFLG_CHIP_MAIL_BOX_WAITING) {
		psema(&chip->mbox_done_sema, PRIBIO);
		atomicClearInt(&chip->chip_flags, AFLG_CHIP_MAIL_BOX_WAITING);
		if (!(chip->chip_flags & AFLG_CHIP_MAIL_BOX_DONE)) {
			reg0 = QL_PCI_INH(&isp->mailbox0);
			isr = QL_PCI_INH(&isp->bus_isr);
			bus_sema = QL_PCI_INH(&isp->bus_sema);
			hccr = QL_PCI_INH(&isp->hccr);
			complete = 1;
			ql_cpmsg(chip->pci_vhdl,
				 "Mailbox timeout: m0 0x%x, isr 0x%x, bus_sema 0x%x, hccr 0x%x\n",
				 reg0, isr, bus_sema, hccr);
		}
		goto done;
	}
		
	/* 
	 * .... or poll for completion
	 */
      again:
	retry_cnt = 100000;
	intr_retry_cnt = 100;
	while( !(chip->chip_flags & AFLG_CHIP_MAIL_BOX_DONE) ) {
		DELAY(10);
		if ((isr = QL_PCI_INH(&isp->bus_isr)) & BUS_ISR_RISC_INT) {
			LOCK(chip->res_lock);
			if (chip->chip_flags & AFLG_CHIP_MAIL_BOX_DONE) {
				UNLOCK(chip->res_lock);
				continue;
			}
			if ((bus_sema = QL_PCI_INH(&isp->bus_sema)) & BUS_SEMA_LOCK) {
				if ((chip->chip_flags & (AFLG_CHIP_IN_INTR | AFLG_CHIP_DUMPING | AFLG_CHIP_WATCHDOG_TIMER | AFLG_CHIP_INITIAL_SRAM_PARITY)) || (--intr_retry_cnt <= 0)) {
					ql_service_mbox_interrupt(chip);
					if (chip->chip_flags & (AFLG_CHIP_DUMPING | AFLG_CHIP_WATCHDOG_TIMER | AFLG_CHIP_INITIAL_SRAM_PARITY)) 
						QL_PCI_OUTH(&isp->hccr, HCCR_CMD_CLEAR_RISC_INT);
				}
			}
			else {
				u_short response_in_new;

				response_in_new = QL_PCI_INH(&isp->mailbox5);
				
				if (chip->response_in != response_in_new) {
					chip->response_in = response_in_new;
					atomicSetInt(&chip->chip_flags, AFLG_CHIP_ASYNC_RESPONSE_IN);
				}
				QL_PCI_OUTH(&isp->hccr, HCCR_CMD_CLEAR_RISC_INT);				
			}
			UNLOCK(chip->res_lock);
		}

		/* check for the sram parity condition which the host is in interrupt mode
		 * and pause mode.
		 */
		hccr = QL_PCI_INH(&isp->hccr);
		if ( (hccr & HCCR_HOST_INT) && (hccr & HCCR_PAUSE) ) {

			QL_CPMSGS(chip->pci_vhdl, "mailbox with host interrupt hccr 0x%x\n", hccr);
			goto done;
		}


		/* check for timeout */
		if ( --retry_cnt <= 0) {
			reg0 = QL_PCI_INH(&isp->mailbox0);
			isr = QL_PCI_INH(&isp->bus_isr);
			bus_sema = QL_PCI_INH(&isp->bus_sema);
			if ((hccr & HCCR_HOST_INT) && (isr & BUS_ISR_RISC_INT)) {
				ql_cpmsg(chip->pci_vhdl, "Mailbox timeout "
					                "(m0 0x%x, isr 0x%x, bus_sema 0x%x, hccr 0x%x) "
					                "resetting ISR and retrying\n", 
					 reg0, isr, bus_sema, hccr);
				QL_PCI_OUTH(&isp->hccr, HCCR_CMD_CLEAR_RISC_INT);
				goto again;
			}
			complete = 1;
			ql_cpmsg(chip->pci_vhdl,
				 "Mailbox timeout: intr_retry_cnt %d, m0 0x%x, isr 0x%x, bus_sema 0x%x, hccr 0x%x\n",
				 intr_retry_cnt, reg0, isr, bus_sema, hccr);
			goto done;
		}
	}	
     done:
	switch (in_cnt) {

		case 8:
		    mbox_sts[7] = chip->mailbox[7];

		case 7:
		    mbox_sts[6] = chip->mailbox[6];
		case 6:
		    mbox_sts[5] = chip->mailbox[5];
		case 5:
		    if ( (reg0 != MBOX_CMD_INIT_RESPONSE_QUEUE) && 
			(reg0 != MBOX_CMD_SET_RETRY_COUNT)  &&
			(reg0 != MBOX_CMD_INIT_RESPONSE_QUEUE_64) )  
			mbox_sts[4] = chip->mailbox[4];
		case 4:
		    mbox_sts[3] = chip->mailbox[3];
		case 3:
		    mbox_sts[2] = chip->mailbox[2];
		case 2:
		    mbox_sts[1] = chip->mailbox[1];
		case 1:
		    mbox_sts[0] = chip->mailbox[0];
	}
	QL_MUTEX_ENTER(chip);
	UNLOCK(chip->mbox_lock);

    	return(complete);
}

void	
qlcommand(struct scsi_request *req)
{
    ql_entry(req);
}

static void	
qldone(scsi_request_t *request)
{
    vsema(request->sr_dev);
}

struct scsi_target_info *
qlinfo(vertex_hdl_t 	ql_lun_vhdl)
{
	struct scsi_request     *req;
	static u_char            scsi[6];
	sema_t                  *sema;
	struct scsi_target_info *retval;
	scsi_target_info_t      *info;
	scsi_lun_info_t	        *lun_info;
	ql_local_info_t         *qli;
	pHA_INFO    		 ha;
	int                      disc_allow = 1;
	u_char                  *sensep;
	int                      retry = 2;
	int			 busy_retry = 8;

	ha = ql_ha_info_from_lun_get(ql_lun_vhdl);
	ASSERT(ha != 0);
	if ((ha->chip_info->device_id == QLogic_1040_DEVICE_ID) &&
		(ha->chip_info->revision <= REV_ISP1040A))
		disc_allow = 0;
	lun_info = scsi_lun_info_get(ql_lun_vhdl);
	qli = SLI_INFO(lun_info);
	ASSERT(&OPEN_MUTEX(lun_info) != NULL);
	mutex_lock(&OPEN_MUTEX(lun_info), PRIBIO);

	info = TINFO(lun_info);
	if (info->si_inq == NULL) {
		info->si_inq = (u_char * ) QL_KMEM_ZALLOC(SCSI_INQUIRY_LEN,
					VM_CACHEALIGN | VM_NOSLEEP | VM_DIRECT);
		info->si_sense = (u_char * )QL_KMEM_ZALLOC(SCSI_SENSE_LEN,
					VM_CACHEALIGN | VM_NOSLEEP | VM_DIRECT);
	}


	sensep  = kern_calloc(1,REQ_SENSE_DATA_LENGTH);
	sema = kern_calloc(1, sizeof(sema_t));
	init_sema(sema, 0, "ql", ql_lun_vhdl);

	req = kern_calloc(1, sizeof(*req));

	do {
		bzero(req, sizeof(*req));
		bzero(info->si_inq, SCSI_INQUIRY_LEN);

		scsi[0] = INQUIRY_CMD; 
		scsi[1] = (SLI_LUN(lun_info)) << 5;
		scsi[2] = scsi[3] = scsi[5] = 0;
		scsi[4] = SCSI_INQUIRY_LEN;
		req->sr_lun_vhdl 	= ql_lun_vhdl;
		req->sr_target 		= SLI_TARG(lun_info);
		req->sr_lun 		= SLI_LUN(lun_info);

		req->sr_command = scsi;
		req->sr_cmdlen = 6;     /* Inquiry command length */
		req->sr_flags = SRF_DIR_IN | SRF_AEN_ACK;
		req->sr_timeout = 10 * HZ;
		req->sr_buffer = info->si_inq;
		req->sr_buflen = SCSI_INQUIRY_LEN;
		req->sr_sense = sensep;
		req->sr_notify = qldone;
		req->sr_dev = sema;
#ifdef FLUSH
		DCACHE_INVAL(info->si_inq, SCSI_INQUIRY_LEN);
#endif

		qlcommand(req);

		psema(sema, PRIBIO);
		if (req->sr_scsi_status == ST_BUSY)
			delay(HZ/2);
	} while (((req->sr_status == SC_ATTN || req->sr_status == SC_CMDTIME) && retry--) ||
		 (req->sr_scsi_status == ST_BUSY && busy_retry--));

	if (req->sr_status == SC_GOOD && req->sr_scsi_status == ST_GOOD) {

		retval = info;

		/*
		 * If the LUN is not supported, then return
		 * NULL. However, if this LUN is a candidate for
		 * failover we still may want to treat it as a valid
		 * device.
		 */
		if ((info->si_inq[0] & 0xC0) == 0x40 && 
		    !fo_is_failover_candidate(info->si_inq, ql_lun_vhdl))
		{	
			retval = NULL;
		}
		else
		{
			scsi_device_update(info->si_inq,ql_lun_vhdl);
			if (info->si_inq[SCSI_INFO_BYTE] & SCSI_CTQ_BIT)
			{
				if (disc_allow) 
				{
					info->si_ha_status |= SRH_MAPUSER | SRH_ALENLIST | 
						              SRH_WIDE | SRH_DISC | 
							      SRH_TAGQ | SRH_QERR0 | SRH_QERR1;
					info->si_maxq = MAX_QUEUE_DEPTH-1;
				}
				else
				{
					info->si_ha_status |= SRH_WIDE;
					info->si_maxq = 1;
				}
			}	

			/*
			 * Does the device support WIDE?
			 */
			if (info->si_inq[SCSI_INFO_BYTE] & 0x20)
			{ 
				if (ha->ql_defaults.Id[req->sr_target].Wide_Allowed &&
				    ha->ql_defaults.Id[req->sr_target].Capability.bits.Wide_Data_Transfers == 0)
				{
					ha->ql_defaults.Id[req->sr_target].Capability.bits.Wide_Data_Transfers = 1;
					atomicClearInt(&qli->qli_dev_flags, DFLG_INITIALIZED);
				}
			}
			else
			{
				if (ha->ql_defaults.Id[req->sr_target].Capability.bits.Wide_Data_Transfers == 1)
				{
					ha->ql_defaults.Id[req->sr_target].Capability.bits.Wide_Data_Transfers = 0;
					atomicClearInt(&qli->qli_dev_flags, DFLG_INITIALIZED);
				}
			}
			
			/*
			 * Does the device support SYNC? The logic to
			 * decide whether to allow sync negotiations
			 * is a little complicated. We allow sync
			 * negotiations when:
			 *     sync bit in inquiry string is on
			 *       AND device is NOT a CDROM
			 *  OR Force_Sync bit is set.
			 *
			 * A little clarification is required. (a) We
			 * have seen problems with 12x CDROMs (wrong
			 * data being returned on block reads) if we
			 * do too many sync negotiations. (b) Some
			 * SCSI-1 devices don't set the sync bit in
			 * the inquiry string even though they are
			 * capable of doing sync. The
			 * ql_force_sync_negotiation DEVICE ADMIN
			 * varible will force sync negotiation in such
			 * cases.
			 */
			ha->ql_defaults.Id[req->sr_target].Is_CDROM = cdrom_inquiry_test(&info->si_inq[8]);
			if (((info->si_inq[SCSI_INFO_BYTE] & 0x10) && 
			     (!ha->ql_defaults.Id[req->sr_target].Is_CDROM || ql_allow_negotiation_on_cdroms)) ||
			    ha->ql_defaults.Id[req->sr_target].Force_Sync)
			{
				if (ha->ql_defaults.Id[req->sr_target].Sync_Allowed &&
				    ha->ql_defaults.Id[req->sr_target].Capability.bits.Sync_Data_Transfers == 0)
				{
					ha->ql_defaults.Id[req->sr_target].Capability.bits.Sync_Data_Transfers = 1;
					atomicClearInt(&qli->qli_dev_flags, DFLG_INITIALIZED);
				}
			}
			else
			{
				if (ha->ql_defaults.Id[req->sr_target].Capability.bits.Sync_Data_Transfers == 1)
				{
					ha->ql_defaults.Id[req->sr_target].Capability.bits.Sync_Data_Transfers = 0;
					atomicClearInt(&qli->qli_dev_flags, DFLG_INITIALIZED);
				}
			}
		}
	} 
	else
	{
		retval = NULL;
	}

	freesema(sema);
	kern_free(sema);
	kern_free(req);
	kern_free(sensep);

	/* Do not keep si_inq data for non-existant scsi devices.
	*/
	if (retval == 0) {
		if (info->si_inq != NULL) {
			kmem_free(info->si_inq,SCSI_INQUIRY_LEN);
			info->si_inq = NULL;
		}
	}

	mutex_unlock(&OPEN_MUTEX(lun_info));

	return retval;
}


void qlfree(vertex_hdl_t 	ql_lun_vhdl, 
	    void 		(*cb)())

{
        scsi_lun_info_t	*lun_info = scsi_lun_info_get(ql_lun_vhdl);
	ql_local_info_t *qli = SLI_INFO(lun_info);
	pHA_INFO	 ha = ql_ha_info_from_lun_get(ql_lun_vhdl);

	if (ha == NULL)
		return;

	if (qli->qli_dev_flags & DFLG_EXCLUSIVE)
		atomicClearInt(&qli->qli_dev_flags, DFLG_EXCLUSIVE);
        if (qli->qli_dev_flags & DFLG_CONTINGENT_ALLEGIANCE)
		atomicClearInt(&qli->qli_dev_flags, DFLG_CONTINGENT_ALLEGIANCE);
	if (qli->qli_ref_count != 0)
		atomicAddInt(&qli->qli_ref_count, -1); 

	if (cb) {
		mutex_lock(&qli->qli_open_mutex, PRIBIO);
		if (qli->qli_sense_callback == cb)
	    		qli->qli_sense_callback = NULL;
		else
			ql_lpmsg(ha->ctlr_vhdl, SLI_TARG(lun_info), SLI_LUN(lun_info), 
				 "qlfree: callback 0x%x not active\n", cb);
		mutex_unlock(&qli->qli_open_mutex);
	}

}

int
qlalloc(vertex_hdl_t 	ql_lun_vhdl, 
	int 		opt,
	void 		(*cb)())
{
	int	         retval = SCSIALLOCOK;
	scsi_lun_info_t	*lun_info = scsi_lun_info_get(ql_lun_vhdl);
	ql_local_info_t *qli = SLI_INFO(lun_info);

	if (qlinfo(ql_lun_vhdl) == NULL) {
		return 0;
	}

	mutex_lock(&OPEN_MUTEX(lun_info), PRIBIO);
	if (qli->qli_dev_flags & DFLG_EXCLUSIVE) {
		retval = EBUSY;
		goto _allocout;
	}

	if (opt & SCSIALLOC_EXCLUSIVE) {
		if (qli->qli_ref_count != 0) {
	    		retval = EBUSY;
	    		goto _allocout;
		} else 
	    		atomicSetInt(&qli->qli_dev_flags, DFLG_EXCLUSIVE);
	}
	atomicAddInt(&qli->qli_ref_count, 1); 
	if (cb) {
		if (qli->qli_sense_callback && qli->qli_sense_callback != cb) {
			retval = EINVAL;
			goto _allocout;
		} else
		    	qli->qli_sense_callback = cb;
	}
_allocout:
    	mutex_unlock(&OPEN_MUTEX(lun_info));
    	return retval;

}

static int 
ql_reset(vertex_hdl_t ql_ctlr_vhdl)
{
    	pHA_INFO ha = ql_ha_info_from_ctlr_get(ql_ctlr_vhdl);
    	u_short  mbox_sts[8];
	int rc;

    	if (ha == NULL) 
		return(1); /*fail*/

	QL_MUTEX_ENTER(ha->chip_info);
	rc = ql_set_bus_reset(ha->chip_info, mbox_sts, ha->channel_id);
	QL_MUTEX_EXIT(ha->chip_info);

    	return(rc ? 1 : 0);
}

#ifdef IP30
/* 
 * IP30 calls qlreset() on ac power fail interrupt 
 */
int
qlreset(vertex_hdl_t ql_ctlr_vhdl)
{
	return ql_reset(ql_ctlr_vhdl);
}
#endif

/*
 * qldump - called prior to doing a panic dump
 *
 * We do have to reset the locks however, in case the panic
 * occured while a qlogic device was active in this driver.
 *
 * The actual dumping will be done by calls to qlcommand() later.
 *      called at spl7()
 */
int 
qldump(vertex_hdl_t 	ql_ctlr_vhdl)
{
#define DUMP_SUCCESS 1
#define DUMP_FAIL    0

    	int	         rv;
    	pHA_INFO         ha;
	ql_local_info_t *qli;
	u_int            id, lun, req_cnt;
	pREQ_INFO        r_ptr;
	vertex_hdl_t     ql_lun_vhdl;

	/*
	 * Interrupts are disabled, so set DUMPING flag for all
	 * adapters. This becomes important when qlhalt is called with
	 * interrupts disabled.
	 */
	for (ha = ql_ha_list; ha; ha = ha->next_ha)
	{
		atomicSetInt(&ha->chip_info->chip_flags, AFLG_CHIP_DUMPING);
	}

	ha = ql_ha_info_from_ctlr_get(ql_ctlr_vhdl);
    	if (ha == NULL) {
		ql_cpmsg(ql_ctlr_vhdl, "Dump failed; no host adapter structure\n");
		return(DUMP_FAIL);
    	}

    	INITLOCK(&ha->chip_info->req_lock, "ql", ql_ctlr_vhdl);
    	INITLOCK(&ha->chip_info->res_lock, "ql", ql_ctlr_vhdl);
    	INITLOCK(&ha->chip_info->mbox_lock, "ql_mbox_lock", ql_ctlr_vhdl);
	INITLOCK(&ha->waitQ_lock, "ql_waitQ_lock", ql_ctlr_vhdl);
	INITLOCK(&ha->chip_info->probe_lock, "ql_probe_lock", ql_ctlr_vhdl);

        QL_MUTEX_ENTER(ha->chip_info);
        ql_reset_interface(ha->chip_info,1);
        QL_MUTEX_EXIT(ha->chip_info);

	/* Init the raw request queue to empty.  */
	ha->req_forw = NULL;
	ha->req_back = NULL;

	for (id = 0; id < QL_MAXTARG; id++)
	{
		/* 
		 * Cleanup the target structures 
		 */
		INITLOCK(&(ha->timeout_info[id].per_tgt_tlock), "ql_timeout", ql_ctlr_vhdl);
		IOLOCK(ha->timeout_info[id].per_tgt_tlock,s);
		ha->timeout_info[id].ql_dups = 0;
		ha->timeout_info[id].current_timeout = 0;
		ha->timeout_info[id].time_base = 0;
		ha->timeout_info[id].ql_tcmd = 0;
		ha->timeout_info[id].timeout_state = 0;
		r_ptr = &ha->reqi_block[id][0];
		ha->reqi_cntr[id] = 1;
		for (req_cnt=0; req_cnt <= (MAX_REQ_INFO); req_cnt++, r_ptr++)
		{
			r_ptr->req = NULL;
			r_ptr->ha_p = (void *)ha;
		}
		IOUNLOCK(ha->timeout_info[id].per_tgt_tlock,s);

		/* 
		 * Cleanup the LUN structures
		 */
		for (lun = 0; lun < QL_MAXLUN; ++lun) {
			if ((ql_lun_vhdl = scsi_lun_vhdl_get(ha->ctlr_vhdl, id, lun)) == GRAPH_VERTEX_NONE)
				continue;
			qli = SLI_INFO(scsi_lun_info_get(ql_lun_vhdl));
			init_mutex(&qli->qli_open_mutex, MUTEX_DEFAULT, "ql_open", ql_lun_vhdl);
			init_mutex(&qli->qli_lun_mutex, MUTEX_DEFAULT, "ql_lun", ql_lun_vhdl);
			IOLOCK(qli->qli_lun_mutex, s);
			qli->qli_dev_flags = DFLG_BEHAVE_QERR1;
			qli->qli_awaitf = qli->qli_awaitb = NULL;
			qli->qli_iwaitf = qli->qli_iwaitb = NULL;
			qli->qli_cmd_rcnt = qli->qli_cmd_awcnt = qli->qli_cmd_iwcnt = 0;
			IOUNLOCK(qli->qli_lun_mutex, s);
		} /* for (lun) */
	} /* for (id) */
	atomicClearInt(&(ha->ql_ncmd), 0xffffffff);
	rv = ql_reset(ql_ctlr_vhdl); 
	if (rv) {
		QL_MUTEX_ENTER(ha->chip_info);
		if (rv = ql_reset_interface(ha->chip_info,1))
			atomicSetInt(&ha->chip_info->chip_flags, AFLG_CHIP_SHUTDOWN);
		QL_MUTEX_EXIT(ha->chip_info);
	}		
	if (rv == 0) {
		for (id = 0; id < QL_MAXTARG; id++) {
			if (id == ha->ql_defaults.Initiator_SCSI_Id)
				continue;
			if ((ql_lun_vhdl = scsi_lun_vhdl_get(ql_ctlr_vhdl,id,0))
				!= GRAPH_VERTEX_NONE) {
				qlinfo(ql_lun_vhdl);
			}
		}
	}
        return((rv == 0) ? DUMP_SUCCESS : DUMP_FAIL);
}

/*
 * create a vertex for the ql controller in the
 * hardware graph and allocate space for the info
 * the vertex passed in is our pci connection point
 */
/*ARGSUSED3*/
static vertex_hdl_t
ql_ctlr_add(vertex_hdl_t vhdl, int channel_id)
{
	/*REFERENCED*/
	char			loc_str[80];
	/*REFERENCED*/
	graph_error_t		rv;
	vertex_hdl_t		ctlr_vhdl;
	scsi_ctlr_info_t	*ctlr_info;

	/*
	 * add the ql controller vertex to the hardware graph,
	 * if it has not been done already. the canonical path
	 * goes up through our connection point. convenience
	 * links are taken care of elsewhere.
	 *
	 * Right now a pci slot can have only one controller, so
	 * 0 has been hardcoded.  We expect that future PCI
	 * devices may support multiple controllers in a single
	 * PCI slot.
	 *
	 */
	sprintf(loc_str, "%s/%d", EDGE_LBL_SCSI_CTLR, channel_id);
	rv = hwgraph_path_add(	vhdl,
				loc_str,
				&ctlr_vhdl);
	ASSERT((rv == GRAPH_SUCCESS) && 
	       (ctlr_vhdl != GRAPH_VERTEX_NONE));

#if IP30
	/*
	 * At the moment, the IP30 boot sequence depends on
	 * using /scsi/ to find a disk.
	 */
	if (channel_id == 0)
	
		rv = hwgraph_edge_add(	vhdl, 
				ctlr_vhdl, 
				EDGE_LBL_SCSI);
	ASSERT(rv == GRAPH_SUCCESS);
#endif

	/* 
	 * allocate memory for the info structure if it has not
	 * been done already
	 */
	ctlr_info = (scsi_ctlr_info_t *)hwgraph_fastinfo_get(ctlr_vhdl);
	if (!ctlr_info) {
		ctlr_info 			= scsi_ctlr_info_init();
		SCI_CTLR_VHDL(ctlr_info)	= ctlr_vhdl;

		SCI_ADAP(ctlr_info)		= ql_pci_slot_number;

		SCI_ALLOC(ctlr_info)		= qlalloc;
		SCI_COMMAND(ctlr_info)		= qlcommand;
		SCI_FREE(ctlr_info)		= qlfree;
		SCI_DUMP(ctlr_info)		= qldump;
		SCI_INQ(ctlr_info)		= qlinfo;
		SCI_IOCTL(ctlr_info)		= qlioctl;
		SCI_ABORT(ctlr_info)		= qlabort;

		scsi_ctlr_info_put(ctlr_vhdl,ctlr_info);
		scsi_bus_create(ctlr_vhdl);
#if defined(DEBUG) && ATTACH_DEBUG
		cmn_err(CE_CONT, "%v: ql_ctlr_add calling pciio_error_register\n", vhdl);
#endif
		pciio_error_register(vhdl, ql_error_handler, ctlr_info);
	}
	hwgraph_vertex_unref(ctlr_vhdl);
	return(ctlr_vhdl);
}

/*
 *    qlinit: called once during system startup or
 *      when a loadable driver is loaded.  
 *      
 *      The driver_register function should normally
 *      be in _reg, not _init.  But the ql driver is 
 *      required by devinit before the _reg routines 
 *      are called, so this is an exception.
 */
void
qlinit(void)
{

	/* register ql1040 chip */
	pciio_driver_register(QLogic_VENDOR_ID, QLogic_1040_DEVICE_ID, "ql", 0);

	/* register ql1080 chip */
	pciio_driver_register(QLogic_VENDOR_ID, QLogic_1080_DEVICE_ID, "ql", 0);

	/* register ql1240 chip */
	pciio_driver_register(QLogic_VENDOR_ID, QLogic_1240_DEVICE_ID, "ql", 0);

	/* register ql1280 chip */
	pciio_driver_register(QLogic_VENDOR_ID, QLogic_1280_DEVICE_ID, "ql", 0);

	/* initialize mutex for qldriverlock */
	init_mutex(&ql_driver_lock, MUTEX_DEFAULT, "qldriverlock", 0);

	/* initialize default value */
	ql_pci_slot_number = 0;
	ql_ha_list = NULL;
	ql_watchdog_timer_inited = 0;
	
}

int
qlattach(vertex_hdl_t vhdl)
{
	pPCI_REG	config_addr;
	pISP_REGS	mem_addr;
	pCHIP_INFO	chip_info=NULL;
	int		count0 ;
	int		count1 ;
	pciio_piomap_t	cmap = 0;
	pciio_piomap_t	rmap = 0;
	vertex_hdl_t	ql_ctlr_vhdl;
	int             disks_found = 0;
	pHA_INFO	ha=NULL;
	pHA_INFO	ha_tmp=NULL;
	int	        chip_rev = 0;
	device_desc_t   ql_dev_desc;
	int		max_targets= QL_MAXTARG;
	int             rc;
	int             diffsens = 0;
	int		i;

	u_short		mbox_sts[8];
	u_short		old_retry_delay;
	u_short		old_retry_cnt;
	u_short		reset_retry_cnt = 0;
	pISP_REGS	isp;

	u_short		 device_id;
	u_short		 rev_id;


	config_addr = (pPCI_REG)pciio_pio_addr(vhdl, 0, PCIIO_SPACE_CFG,
					       0, sizeof(*config_addr), &cmap, 0);

#ifdef USE_IOSPACE
	mem_addr = (pISP_REGS)pciio_pio_addr(vhdl, 0, PCIIO_SPACE_WIN(0),
					     0, sizeof(*mem_addr), &rmap, 0);
#else
	mem_addr = (pISP_REGS)pciio_pio_addr(vhdl, 0, PCIIO_SPACE_WIN(1),
					     0, sizeof(*mem_addr), &rmap, 0);
#endif

	/* XXX- when detaching, we must piomap_free cmap and rmap if
	 * they are not NULL, or we will have a leak of piomap
	 * resources.  */

#ifdef USE_IOSPACE
	{
		ushort command = PCI_INH(&config_addr->Command);
		if (!(command & PCI_CMD_IO_SPACE)) {
			PCI_OUTH(&config_addr->Command, command | PCI_CMD_IO_SPACE);
		}
	}
#else
	{
		ushort command = PCI_INH(&config_addr->Command);
		if (!(command & PCI_CMD_MEM_SPACE)) {
			PCI_OUTH(&config_addr->Command, command | PCI_CMD_MEM_SPACE);
		}
	}
#endif


	ql_dev_desc = device_desc_dup(vhdl);
	device_desc_intr_name_set(ql_dev_desc, QLOGIC_DEV_DESC_NAME);
	device_desc_default_set(vhdl,ql_dev_desc);
	device_id = PCI_INH(&config_addr->Device_Id);
	rev_id = PCI_INB(&config_addr->Rev_Id);

	/* XXX- when detaching, we must piomap_free cmap and rmap if
	 * they are not NULL, or we will have a leak of piomap
	 * resources.  */
	if ( (device_id == QLogic_1040_DEVICE_ID) ||
		(device_id == QLogic_1080_DEVICE_ID)) {
		count0 = 2;
		count1 = 0;
		PCI_OUTB(&config_addr->Latency_Timer, 0x40);
		PCI_OUTB(&config_addr->Cache_Line_Size, 0x80);
	} else {
		count0 = 2;
		count1 = 2;
		PCI_OUTB(&config_addr->Latency_Timer, 0x40);
		PCI_OUTB(&config_addr->Cache_Line_Size, 0x80);
	}
#if SN || IP30
	if (pcibr_rrb_alloc(vhdl, &count0, &count1) < 0)
		cmn_err(CE_PANIC, "ql: Unable to get Bridge RRB's");
#endif
	switch(device_id) {
		case QLogic_1040_DEVICE_ID:
		   switch(rev_id) {
			case REV_ISP1040B:
				chip_rev = INV_QL_REV3;
			break;
			case REV_ISP1040A:
				chip_rev = INV_QL_REV2;
			break;
			case REV_ISP1040:
				chip_rev = INV_QL_REV1;
			break;
			case REV_ISP1040AV4:
				chip_rev = INV_QL_REV2_4;
			break;
			case REV_ISP1040BV2:
				chip_rev = INV_QL_REV4;
			break;
			default:	/* Needed for when we use a new part that
				 	* the driver does not yet know about
				 	* Otherwise it shows up as a WD95
				 	*/
				chip_rev = INV_QL;
	           }
		break;
		case  QLogic_1240_DEVICE_ID:
		   chip_rev = INV_QL_1240;
		break;
		case QLogic_1080_DEVICE_ID:
		   chip_rev = INV_QL_1080;
		break;
		case QLogic_1280_DEVICE_ID:
		   chip_rev = INV_QL_1280;
		break;
		default:
		   chip_rev = INV_QL;
	}

	/* add the first one always */
	ql_ctlr_vhdl = ql_ctlr_add(vhdl, 0);

	/* initialize the adapter */
	if (ql_init(ql_ctlr_vhdl, vhdl, config_addr, mem_addr)) {
		ql_pci_slot_number++; /* increment slot so the next one is ok */
		return(1);
	} 

 	ha = ql_ha_info_from_ctlr_get(ql_ctlr_vhdl);

	sprintf(ha->ha_name, "%v", ha->ctlr_vhdl); 	/* remember the name */

	chip_info =(pCHIP_INFO) ha->chip_info;

	isp = (pISP_REGS)chip_info->ha_base;
   
	for (i=0;i < chip_info->channel_count; i++){

		ha_tmp = (pHA_INFO)chip_info->ha_info[i];

		if((diffsens = ql_diffsense(ha_tmp)) == -1) {
			diffsens = 0;
		}

        	device_inventory_add(ha_tmp->ctlr_vhdl,
			     INV_DISK,
			     INV_SCSICONTROL,
			     -1, diffsens<<6, chip_rev);
		if (diffsens == 0)
			ha->bus_mode = SCSI_BUS_MODE_SINGLE_ENDED;
		else {
			/* (diffsens:1 -> SCSI_BUS_MODE_HVD, diffsens:2 SCSI_BUS_MODE_LVD) */
			ha->bus_mode = diffsens<<1; 
		}

		if (showconfig)
			ql_cpmsg(ha->ctlr_vhdl, "SCSI Adapter %x in bus mode : %d\n",
				ha->chip_info->device_id, ha->bus_mode);
	}

	if (chip_rev == INV_QL_REV2)
		ql_cpmsg(ql_ctlr_vhdl, "INVALID CUSTOMER CONFIGURATION: Downrev SCSI QL1040A\n");

	/*
	* Set the retry count to zero for probing.
	* this will make the probing faster.
	* No lock is needed because here.
	*/



	QL_MUTEX_ENTER(chip_info);
	if (ql_set_selection_timeout(chip_info, mbox_sts)) {
		QL_MUTEX_EXIT(chip_info);
		rc = 1;
		goto attach_failed;
	}
			

	if (ql_set_retry_count(chip_info, mbox_sts)) {
		QL_MUTEX_EXIT(chip_info);
		rc = 2;
		goto attach_failed;
	} else {
		old_retry_cnt = mbox_sts[1];
		old_retry_delay = mbox_sts[2];
		reset_retry_cnt = 1;
	}


	atomicSetUint(&chip_info->chip_flags, AFLG_CHIP_INITIALIZED);

	QL_MUTEX_EXIT(chip_info);
	


	for (i=0; i < chip_info->channel_count; i++) { 

		char             is_mscsi;
        	char             is_channel0;
        	arbitrary_info_t ainfo ;
        	vertex_hdl_t     vhdl1 ;
        	vertex_hdl_t     vhdl0 ;
        	u_short          gpio_val;

		ha_tmp = (pHA_INFO) chip_info->ha_info[i];

		max_targets = QL_MAXTARG;

#ifdef IP30
		if (ql_pci_slot_number == 0) max_targets = 4;

#if (SYNC_DATA_TRANSFERS==0) || (WIDE_DATA_TRANSFERS==0)
	/* this is for debugging in async mode */
	/* sync state is lost when the kernel reloads fw to the ql chip */
		ql_reset(ha_tmp->ctlr_vhdl);
#endif

#endif /* #ifdef IP30 */

#define MSCSI_A         1
#define MSCSI_B         2
#define MSCSI_BO        3               /* MSCSI/B for Octane */
#define MSCSIA_PNUM     "030-0872-"
#define MSCSIB_PNUM     "030-1243-"
#define MSCSIBO_PNUM    "030-1281-"     /* MSCSI/B for Octane  */




		is_mscsi = 0;
		is_channel0 = 0;
		ainfo = 0;
		vhdl1 = hwgraph_connectpt_get(vhdl);
		vhdl0 = hwgraph_connectpt_get(vhdl1);


		/* Unref corr. to reference got thru hwgraph_connectpt_get()
	 	 * above.
	 	 */
		hwgraph_vertex_unref(vhdl1);
		gpio_val = QL_PCI_INH(&isp->b4);

		/*
	 	 * Read the NIC on the MSCSI board
	 	 */
		if (hwgraph_info_get_LBL(vhdl0, INFO_LBL_NIC, &ainfo) == GRAPH_SUCCESS) {

			if (strstr((char *)ainfo, MSCSIA_PNUM))
				is_mscsi = MSCSI_A;
			else
			if (strstr((char *)ainfo, MSCSIB_PNUM))
				is_mscsi = MSCSI_B;
			else
			if (strstr((char *)ainfo, MSCSIBO_PNUM))
				is_mscsi = MSCSI_BO;
		}
#ifdef SN0

		/*
		 * Force internal Speedo channel 1 to be narrow
	 	 */
            	if ((private.p_sn00) && !is_mscsi && (gpio_val & 0x000c)) {
			pciio_info_t	pciio_info = pciio_info_get(vhdl);
			pciio_slot_t 	pciio_slot = pciio_info_slot_get(pciio_info);
			if (pciio_slot == SLOT_ONE) { 
				max_targets = 8;
				atomicSetUint(&ha_tmp->host_flags, AFLG_HA_FAST_NARROW);
				ql_set_defaults(ha_tmp);
			}
            	}
#endif /* SN0 */

		/* 
		* Unref corr. to reference got thru hwgraph_connectpt_get()
		* above.
		*/
		hwgraph_vertex_unref(vhdl0);

		/*
 		* If mscsi, make it a realtime device to use ALL
		* resources.
		*/
		if (is_mscsi) 
 			pciio_priority_set(vhdl, PCI_PRIO_HIGH);

		/* 
		* (1) Determine if this is channel 0 of an
		*     MSCSI. [This is kluge for now. We need a better way.]
		*     not done for IP30
		* (2) Determine if DIFFSENS is set.
		*/
		if (is_mscsi) {
#ifndef IP30 /* IP30 doesn't need this check */
			int base = (int)((long)chip_info->ha_base);
			is_channel0 = ((base & 0x00f00000) == (int)0x200000);
#endif

			if (is_mscsi == MSCSI_B || is_mscsi == MSCSI_BO) {
		  		if ((diffsens = ql_diffsense(ha_tmp)) == -1){
		    			return(1);
				}
			}
		} /* if (is_mscsi) */

		/*
		* If it's an MSCSI and it is in xtalk slot XIO1 than
		* first probe the external SE/DF bus.  If there're no drives
		* found on external bus than probe the internal bus.  
		*/
		if (is_mscsi && is_channel0 && (!(gpio_val & NOT_SLOT_ZERO))) {
			QL_PCI_OUTH(&isp->b6, ENABLE_WRITE);

			/*
		 	* (1) Try external bus
		 	*/
		       	if (is_mscsi == MSCSI_A) {
				QL_CPMSGS(ha_tmp->ctlr_vhdl, "Probing MSCSI/A slot 0 external SE/DF bus.\n");
				QL_PCI_OUTH(&isp->b4, GPIO1 | GPIO0);
			}
			else 	
			{
				if (diffsens) {
					QL_CPMSGS(ha_tmp->ctlr_vhdl, "Probing MSCSI/B slot 0 external DF bus.\n");
					QL_PCI_OUTH(&isp->b4, GPIO1 | GPIO0);  /* External DF, internally terminated */
				}
				else {
					QL_CPMSGS(ha_tmp->ctlr_vhdl, "Probing MSCSI/B slot 0 external SE bus.\n");
					QL_PCI_OUTH(&isp->b4, GPIO0);          /* External SE, internally terminated  */
				}
			}
			disks_found = ql_probe_bus(ha_tmp, max_targets, PROBE_ALL_LUNS);
			DBG(1, "= %d (disks)\n", disks_found);
			if (disks_found) {
				ql_pci_slot_number++;
				goto end;
			}

			/* 
		 	* (2) Next try internal SE bus
		 	*/
			if (is_mscsi == MSCSI_A) {
				QL_CPMSGS(ha_tmp->ctlr_vhdl, "Probing MSCSI/A [channel 0] internal SE bus.\n");
				QL_PCI_OUTH(&isp->b4, GPIO1);
			}
			else {
				QL_CPMSGS(ha_tmp->ctlr_vhdl, "Probing MSCSI/B [channel 0] internal SE bus.\n");
				QL_PCI_OUTH(&isp->b4, 0);
			}
			disks_found = ql_probe_bus(ha_tmp, max_targets, PROBE_ALL_LUNS);
			DBG(1, "= %d (disks)\n", disks_found);
			ql_pci_slot_number++;
			goto end;
		}

		/*
		 * If it's an MSCSI and it is in xtalk slot other
		 * than XIO1, enable SE/DF as appropriate. 
		 */
#ifdef SN
		if (is_mscsi && is_channel0) {
#else
               	if (is_mscsi) {
#endif

		       	QL_PCI_OUTH(&isp->b6, ENABLE_WRITE);
			if (is_mscsi == MSCSI_A) {
				QL_PCI_OUTH(&isp->b4, GPIO1 | GPIO0);          /* External, internally terminated */
			}
			else 
			{
				if (diffsens) {
					QL_PCI_OUTH(&isp->b4, GPIO1 | GPIO0);  /* External DF, internally terminated */
				}
				else 
				{
					QL_PCI_OUTH(&isp->b4, GPIO0);          /* External SE, internally terminated */
				}
			}
			
		}

		QL_CPMSGS(ha_tmp->ctlr_vhdl, "Probing SCSI bus\n");

		disks_found = ql_probe_bus(ha_tmp, max_targets, PROBE_ALL_LUNS);
		ql_pci_slot_number++;
		DBG(1, "= %d (disks)\n", disks_found);
end:
		;

	}/* for channel_count loop */

	QL_MUTEX_ENTER(chip_info);

	if (ql_set_selection_timeout(chip_info, mbox_sts)) {
		QL_MUTEX_EXIT(chip_info);
		rc = 3;
		goto attach_failed;

	} 


	/*
	* if retry count was set to zero than reset it to its old value.
	*/
	if (reset_retry_cnt) {
		chip_info->chip_parameters.Retry_Count = old_retry_cnt;
		chip_info->chip_parameters.Retry_Delay = old_retry_delay;
		if (ql_set_retry_count(chip_info, mbox_sts)) {
			QL_MUTEX_EXIT(chip_info);
			rc = 4;
			goto attach_failed;
		}

	}
	QL_MUTEX_EXIT(chip_info);

	return(0);

attach_failed:	
	ql_cpmsg(chip_info->pci_vhdl, "qlattach() failed (%d)\n", rc);
	return(rc);
}


int
qldetach(struct pci_record *pp, edt_t *e, __int32_t reason) 
{
    printf("ql_detach: does nothing - pci_record %x edt %x reason %x\n",
        pp, e, reason);

    /* remove driver and instance of boards in system */
    ql_pci_slot_number = 0;
    return(0);
}


int
qlerror(struct pci_record *pp, edt_t *e, __int32_t reason) 
{
    printf("ql_error: does nothing - pci_record %x edt %x reason %x\n",
	   pp, e, reason);
    return(0);
}

/*ARGSUSED*/
int
qlabort(scsi_request_t *scsi_request)
{
	u_short          mbox_sts[8];
	pHA_INFO         ha = ql_ha_info_from_lun_get(scsi_request->sr_lun_vhdl);
	ql_local_info_t *qli = SLI_INFO(scsi_lun_info_get(scsi_request->sr_lun_vhdl));
	int              retval = 0; /* assume failure */

	/* 
	 * Only issue the ABORT mailbox command if there are
	 * outstanding * I/Os, otherwise punt.  
	 */
	if (((qli->qli_dev_flags & DFLG_ABORT_IN_PROGRESS) == 0) &&
	    (qli->qli_cmd_rcnt > 0)) {
		atomicSetUint(&qli->qli_dev_flags, DFLG_ABORT_IN_PROGRESS);

		QL_MUTEX_ENTER(ha->chip_info);
		if (ql_mbox_cmd(ha->chip_info, mbox_sts, 2, 2,
				MBOX_CMD_ABORT_DEVICE,
				(u_short) (ha->channel_id<<15) |
				(u_short) (scsi_request->sr_target << 8) | 
				(u_short) (scsi_request->sr_lun),
				0, 0, 0, 0, 0, 0)) 
		{
			ql_lpmsg(ha->ctlr_vhdl, scsi_request->sr_target, scsi_request->sr_lun, 
				 "MBOX_CMD_ABORT_DEVICE Command failed\n");
		} 
		else {
			retval = 1; /* success */
		}
		QL_MUTEX_EXIT(ha->chip_info);
	}
	return(retval); 
}

void
qlhalt(void)
{
	pHA_INFO	ha;

	for (ha = ql_ha_list; ha; ha = ha->next_ha)
		ql_reset(ha->ctlr_vhdl);
}

/*ARGSUSED*/
static void
fill_sg_elem(pHA_INFO ha, data_seg *dsp, alenaddr_t address, size_t length, int allow_prefetch)
{

	dev_t dev = ha->chip_info->pci_vhdl;
	u_char	chan = ha->channel_id;
	int	vchan;

	if (chan == 0x01)
		vchan = PCIBR_VCHAN1;
	else	
		vchan = PCIBR_VCHAN0;

	address = (alenaddr_t)pciio_dmatrans_addr
		(dev, NULL, address, length, 
		vchan |
		PCIIO_DMA_DATA |
		PCIIO_BYTE_STREAM |
		PCIBR_PREFETCH |
		PCIIO_DMA_A64);

#ifdef BRIDGE_B_PREFETCH_DATACORR_WAR
	if (!allow_prefetch)
		address &= ~PCI64_ATTR_PREF;
#endif

	dsp->base = address;
	dsp->count = length;
}

static void	
munge(u_char *pointer, int count)
{
	/* munge 4 bytes at a time */
	int	i;
	u_char * cpointer, *copyptr;
	int	temp;

	cpointer = copyptr = pointer;
	cpointer = pointer;
	copyptr = pointer;
	for (i = 0; i < count / 4; i++) {
		temp = *((int *) copyptr);
		cpointer = (u_char * ) & temp;

		*(copyptr + 3) = *(cpointer + 0);
		*(copyptr + 2) = *(cpointer + 1);
		*(copyptr + 1) = *(cpointer + 2);
		*(copyptr + 0) = *(cpointer + 3);
		copyptr = copyptr + 4;
	}
	cpointer = pointer;
}

#ifdef QL1240_SWAP_WAR
static void
munge_iocb(u_char *pointer, int count)
{
        uint64_t *p;
        uint32_t p1, p2;
        int     i;

        p = (uint64_t *)pointer;
        for (i = 0; i < count; i += 8) {
                p1 = *((uint32_t *)p);
                p2 = *((uint32_t *)p + 1);
                *((uint32_t *)p)     = p2;
                *((uint32_t *)p + 1) = p1;
                ++p;
        }
}
#endif


/*
** Function:    ql_set_defaults
**
** Description: Sets the default parameters.
*/
static void	
ql_set_defaults(pHA_INFO ha)
{
    	int			index;
    	Default_Parameters	*n_ptr = &ha->ql_defaults;
	pCHIP_INFO		chip_info = ha->chip_info;
	Chip_Default_Parameters	*c_ptr = &chip_info->chip_parameters;	
    	int			disc_allow = 1;
	char			*dev_admin;
	int			tmpval;

	if  ( (chip_info->device_id == QLogic_1240_DEVICE_ID) ||
	      (chip_info->device_id == QLogic_1080_DEVICE_ID) ||
	      (chip_info->device_id == QLogic_1280_DEVICE_ID) ) 
	        chip_info->ql_request_queue_depth = REQUEST_QUEUE_1240_DEPTH;
	else
    	        chip_info->ql_request_queue_depth = REQUEST_QUEUE_1040_DEPTH;


	if  ( (chip_info->device_id == QLogic_1240_DEVICE_ID) ||
		(chip_info->device_id == QLogic_1080_DEVICE_ID) ||
		(chip_info->device_id == QLogic_1280_DEVICE_ID) ) 

		chip_info->ql_response_queue_depth = RESPONSE_QUEUE_1240_DEPTH;
	else
               	chip_info->ql_response_queue_depth = RESPONSE_QUEUE_1040_DEPTH;


	/* need to add code to detect which controller is this and assign accordingly  */
	if (chip_info->device_id == QLogic_1040_DEVICE_ID) 
    		c_ptr->Fifo_Threshold = BURST_128;
	else 
		c_ptr->Fifo_Threshold = BURST_128;


    	n_ptr->HA_Enable = HA_ENABLE;

	if (dev_admin = device_admin_info_get(ha->ctlr_vhdl, "ql_hostid")) 
	{
		n_ptr->Initiator_SCSI_Id = atoi(dev_admin);
		QL_CPMSGS(ha->ctlr_vhdl,
			  "DEVICE ADMIN: ql_hostid changed from %d to %d.\n",
			  INITIATOR_SCSI_ID, n_ptr->Initiator_SCSI_Id);	
	}
	else 
	{
		n_ptr->Initiator_SCSI_Id = INITIATOR_SCSI_ID;
		if (arg_qlhostid[0]) 
		{
			if ((arg_qlhostid[0] >= '0') && (arg_qlhostid[0] <= '9')) 
				n_ptr->Initiator_SCSI_Id = arg_qlhostid[0] - '0';
			else
				if ((arg_qlhostid[0] >= 'a') && (arg_qlhostid[0] <= 'f'))
					n_ptr->Initiator_SCSI_Id = arg_qlhostid[0] - 'a' + 10;
			else
				if ((arg_qlhostid[0] >= 'A') && (arg_qlhostid[0] <= 'F'))
					n_ptr->Initiator_SCSI_Id = arg_qlhostid[0] - 'A' + 10;
		}
		if (n_ptr->Initiator_SCSI_Id > 7)
			ql_cpmsg(ha->ctlr_vhdl, "Host SCSI ID set to %d - narrow devices will not be seen.\n",
				 n_ptr->Initiator_SCSI_Id); 
	}
	 

	c_ptr->Retry_Count = RETRY_COUNT;
	c_ptr->Retry_Delay = RETRY_DELAY;
	c_ptr->Firmware_Features = FIRMWARE_FEATURES;

	/* do not allow disconnects on early ql parts. only rev b */
	if ((chip_info->device_id == QLogic_1040_DEVICE_ID) &&(chip_info->revision <= REV_ISP1040A))
		disc_allow = 0;

	n_ptr->ASync_Data_Setup_Time = ASYNC_DATA_SETUP_TIME;
	n_ptr->REQ_ACK_Active_Negation = REQ_ACK_ACTIVE_NEGATION;
	n_ptr->DATA_Line_Active_Negation = DATA_ACTIVE_NEGATION;
	c_ptr->Data_DMA_Burst_Enable = DATA_DMA_BURST_ENABLE;
	c_ptr->Command_DMA_Burst_Enable = CMD_DMA_BURST_ENABLE;
	c_ptr->Tag_Age_Limit = TAG_AGE_LIMIT;
	
	c_ptr->Fast_Memory_Enable = 0x0001;

	if (dev_admin = device_admin_info_get(ha->ctlr_vhdl, "ql_selection_timeout")) 
	{ 
		n_ptr->Selection_Timeout = atoi(dev_admin);
		QL_CPMSGS(ha->ctlr_vhdl,
			  "DEVICE ADMIN: ql_selection_timeout changed from %d to %d.\n",
			  SELECTION_TIMEOUT, n_ptr->Selection_Timeout);
	}
	else
	{
		n_ptr->Selection_Timeout = SELECTION_TIMEOUT;
	}  

	n_ptr->Max_Queue_Depth = MAX_QUEUE_DEPTH;
    	n_ptr->Bus_Reset_Delay = ql_bus_reset_delay;

	for (index = 0; index < QL_MAXTARG; index++) 
	{
		n_ptr->Id[index].Capability.bits.Renegotiate_on_Error = RENEGOTIATE_ON_ERROR;
		n_ptr->Id[index].Capability.bits.Stop_Queue_on_Check  = STOP_QUEUE_ON_CHECK;
		n_ptr->Id[index].Capability.bits.Auto_Request_Sense   = AUTO_REQUEST_SENSE;
		if (disc_allow) 
		{
			if (dev_admin = device_admin_info_get(ha->ctlr_vhdl, "ql_disconnect_allowed")) 
			{
				n_ptr->Id[index].Capability.bits.Disconnect_Allowed = atoi(dev_admin) & 0x1;
				QL_TPMSGS(ha->ctlr_vhdl, index,
					  "DEVICE ADMIN: ql_disconnect_allowed changed from %d to %d.\n",
					  DISCONNECT_ALLOWED, n_ptr->Id[index].Capability.bits.Disconnect_Allowed);
			}
			else 
			{
				n_ptr->Id[index].Capability.bits.Disconnect_Allowed = DISCONNECT_ALLOWED;
			}

			if (dev_admin = device_admin_info_get(ha->ctlr_vhdl, "ql_ctq_enable")) 
			{
				n_ptr->Id[index].Capability.bits.Tagged_Queuing = atoi(dev_admin) & 0x1;
				QL_TPMSGS(ha->ctlr_vhdl, index,
					  "DEVICE ADMIN: ql_ctq_enable changed from %d to %d.\n",
					  TAGGED_QUEUING, n_ptr->Id[index].Capability.bits.Tagged_Queuing);
			}
			else 
			{
				n_ptr->Id[index].Capability.bits.Tagged_Queuing = TAGGED_QUEUING;
			}
		}

		if (dev_admin = device_admin_info_get(ha->ctlr_vhdl, "ql_sync_enable")) 
		{
			n_ptr->Id[index].Sync_Allowed = atoi(dev_admin) & 0x1;
			n_ptr->Id[index].Capability.bits.Sync_Data_Transfers = 0;
			QL_TPMSGS(ha->ctlr_vhdl, index,
				  "DEVICE ADMIN: ql_sync_enable from %d to %d.\n",
				  SYNC_DATA_TRANSFERS, n_ptr->Id[index].Sync_Allowed);
		}
		else
		{
			n_ptr->Id[index].Sync_Allowed = SYNC_DATA_TRANSFERS;
			n_ptr->Id[index].Capability.bits.Sync_Data_Transfers = 0;
		}

		if (ha->host_flags & AFLG_HA_FAST_NARROW) 
		{
			n_ptr->Id[index].Sync_Period = SYNC_PERIOD;
			n_ptr->Id[index].Wide_Allowed = 0;
			n_ptr->Id[index].Capability.bits.Wide_Data_Transfers = 0;
		} 
		else 
		{
			if (dev_admin = device_admin_info_get(ha->ctlr_vhdl, "ql_wide_enable")) 
			{
				n_ptr->Id[index].Wide_Allowed = atoi(dev_admin) & 0x1;
				n_ptr->Id[index].Capability.bits.Wide_Data_Transfers = 0;
				QL_TPMSGS(ha->ctlr_vhdl, index,
					  "DEVICE ADMIN: ql_wide_enable from %d to %d.\n",
					  WIDE_DATA_TRANSFERS, n_ptr->Id[index].Wide_Allowed);
			}
			else
			{
				n_ptr->Id[index].Wide_Allowed = WIDE_DATA_TRANSFERS;
				n_ptr->Id[index].Capability.bits.Wide_Data_Transfers = 0;
			}

			if (dev_admin = device_admin_info_get(ha->ctlr_vhdl, "ql_sync_period")) 
			{
				n_ptr->Id[index].Sync_Period = atoi(dev_admin); 

				if (ha->chip_info->clock_rate == FAST20_CLOCK_RATE) {
					/* don't let the user put in a crazy value */
					if ( (n_ptr->Id[index].Sync_Period != 12) &&
				    	     (n_ptr->Id[index].Sync_Period != 16) &&
				    	     (n_ptr->Id[index].Sync_Period != 20) &&
				    	     (n_ptr->Id[index].Sync_Period != 25) &&
				    	     (n_ptr->Id[index].Sync_Period != 29) &&
				    	     (n_ptr->Id[index].Sync_Period != 33) &&
				    	     (n_ptr->Id[index].Sync_Period != 37) &&
				    	     (n_ptr->Id[index].Sync_Period != 41) &&
				    	     (n_ptr->Id[index].Sync_Period != 45) &&
				    	     (n_ptr->Id[index].Sync_Period != 50) &&
				    	     (n_ptr->Id[index].Sync_Period != 58) &&
				    	     (n_ptr->Id[index].Sync_Period != 62) &&
				    	     (n_ptr->Id[index].Sync_Period != 66)) 
					{
					   n_ptr->Id[index].Sync_Period = SYNC_PERIOD_FAST20;
					}
					QL_TPMSGS(ha->ctlr_vhdl, index,
					  "DEVICE ADMIN: ql_sync_period from %d to %d.\n",
					  SYNC_PERIOD_FAST20, n_ptr->Id[index].Sync_Period);
				} else {
              				/* don't let the user put in a crazy value */
                                       	if (	(n_ptr->Id[index].Sync_Period != 10) &&
                                    		(n_ptr->Id[index].Sync_Period != 12) &&
                                    		(n_ptr->Id[index].Sync_Period != 25) &&
                                    		(n_ptr->Id[index].Sync_Period != 27) &&
                                    		(n_ptr->Id[index].Sync_Period != 30) &&
                                    		(n_ptr->Id[index].Sync_Period != 32) &&
                                    		(n_ptr->Id[index].Sync_Period != 35) &&
                                    		(n_ptr->Id[index].Sync_Period != 37) &&
                                    		(n_ptr->Id[index].Sync_Period != 40))
					{
                                       	    n_ptr->Id[index].Sync_Period = SYNC_PERIOD_FAST40;
					}
					QL_TPMSGS(ha->ctlr_vhdl, index,
					  "DEVICE ADMIN: ql_sync_period from %d to %d.\n",
					  SYNC_PERIOD_FAST40, n_ptr->Id[index].Sync_Period);
				}

			}
			else
			{
				if  (ha->chip_info->clock_rate == FAST20_CLOCK_RATE) 
					n_ptr->Id[index].Sync_Period = SYNC_PERIOD_FAST20;
				else
					n_ptr->Id[index].Sync_Period = SYNC_PERIOD_FAST40;
			}
		}
		n_ptr->Id[index].Capability.bits.Parity_Checking = PARITY_CHECKING;

		n_ptr->Id[index].Throttle = EXECUTION_THROTTLE;
		if (dev_admin = device_admin_info_get(ha->ctlr_vhdl, "ql_sync_offset")) 
		{
			n_ptr->Id[index].Sync_Offset = atoi(dev_admin);
			/* don't let the user put in a crazy value */
			if (n_ptr->Id[index].Sync_Offset > MAX_SYNC_OFFSET)
				n_ptr->Id[index].Sync_Offset = SYNC_OFFSET;
			QL_TPMSGS(ha->ctlr_vhdl, index,
				  "DEVICE ADMIN: ql_sync_offset from %d to %d.\n",
				  SYNC_OFFSET, n_ptr->Id[index].Sync_Offset,index);
		}
		else
		{
			n_ptr->Id[index].Sync_Offset = SYNC_OFFSET;
		}
	}
}

/*
** 
**
*/
#ifdef QL_STATS
static void
add_deltatime(struct timeval *current_time, 
	      struct timeval *old_time, 
	      struct timeval *add_to_this)
{

	struct timeval scratch_time;
	scratch_time.tv_sec = 0;
	scratch_time.tv_usec = 0;

	/* start with easy case */
	/* no wrap */
	if (current_time->tv_sec >= old_time->tv_sec) /* no wrapping going on */ {
		if (current_time->tv_usec > old_time->tv_usec) {
		    scratch_time.tv_sec = current_time->tv_sec - old_time->tv_sec;
		    scratch_time.tv_usec = current_time->tv_usec - 
			old_time->tv_usec;
		} else {
		    scratch_time.tv_sec = current_time->tv_sec - 
			old_time->tv_sec - 1;
		    scratch_time.tv_usec = old_time->tv_usec - 
			current_time->tv_usec;
		}
	} else /* timer wrapped */			 {
		scratch_time.tv_sec--;
		scratch_time.tv_usec--;
		scratch_time.tv_sec -= old_time->tv_sec;
		scratch_time.tv_usec -= old_time->tv_usec;
		scratch_time.tv_sec += current_time->tv_sec;
		scratch_time.tv_usec += current_time->tv_usec;
	}

	if (scratch_time.tv_sec > 10000 ) {
		return;
	}

	if (scratch_time.tv_usec >= 1000000 ) {
		scratch_time.tv_sec++;
		scratch_time.tv_usec -= 1000000;
	}
	add_to_this->tv_sec += scratch_time.tv_sec;
	add_to_this->tv_usec += scratch_time.tv_usec;
	if (add_to_this->tv_usec >= 1000000) {
		add_to_this->tv_sec++;
		add_to_this->tv_usec -= 1000000;
	}

	return;
}
#endif

static void 
scan_bus(vertex_hdl_t ql_ctlr_vhdl)
{
	int disks_found = 0;
	pHA_INFO ha = ql_ha_info_from_ctlr_get(ql_ctlr_vhdl);

	QL_CPMSGS(ql_ctlr_vhdl, "Re-probing SCSI bus\n");

	/* set defaults again in case some device got changed up */
	ql_set_defaults(ha);

	disks_found = ql_probe_bus(ha, QL_MAXTARG, 1);
	DBG(1, "= %d (disks)\n", disks_found);
}

static int
reqi_get_slot(pHA_INFO ha,int tgt)
{
	int		i;
	pREQ_INFO	r_ptr;
	int		slot;
	int		found = 0;

	i = ha->reqi_cntr[tgt];
	r_ptr = &ha->reqi_block[tgt][i];
	do {
		i++;
		if (i > MAX_REQ_INFO) {
			i = 1;
		} 
		r_ptr = &ha->reqi_block[tgt][i];
		if (r_ptr->req == NULL) {
			found = 1;
			break;
		} else {
			if (++r_ptr->cmd_age > TAG_AGE_LIMIT)
                                atomicSetUint(&ha->host_flags, AFLG_HA_DRAIN_IO);
		}
			
	} while (i != ha->reqi_cntr[tgt]);
	if (found) {
		ha->reqi_cntr[tgt] = i;
		slot = (tgt * (MAX_REQ_INFO +1)) | i;
	} else {
		slot = found;
	}

	return slot;
}

static void
ql_quiesce_bus(pHA_INFO ha, long quiesce_time)
{

	/* is this to extend the quiesce */
	if(ha->host_flags & AFLG_HA_QUIESCE_COMPLETE) {
		untimeout(ha->quiesce_id);
		ha->quiesce_id = 
		timeout(kill_quiesce, ha, quiesce_time, KILL_WITHOUT_TIMEOUT);
	}
	if (ha->ql_ncmd == 0) {
		atomicSetUint(&ha->host_flags, AFLG_HA_QUIESCE_COMPLETE);
       		atomicClearUint(&ha->host_flags, AFLG_HA_QUIESCE_IN_PROGRESS);
		ha->quiesce_id = timeout(kill_quiesce, ha, quiesce_time, KILL_WITHOUT_TIMEOUT);
	 } else {
	       	atomicSetUint(&ha->host_flags, AFLG_HA_QUIESCE_IN_PROGRESS);
	      	ha->quiesce_in_progress_id = 
		timeout(kill_quiesce,ha, quiesce_time, KILL_WITHOUT_TIMEOUT);
               	ha->quiesce_time = quiesce_time;
	}

}

static void 
kill_quiesce(pHA_INFO ha, int kill_timeout)
{
	atomicClearUint(&ha->host_flags, (AFLG_HA_QUIESCE_IN_PROGRESS | AFLG_HA_QUIESCE_COMPLETE));
	if(ha->quiesce_in_progress_id) {
		if (kill_timeout == KILL_WITH_TIMEOUT)
			untimeout(ha->quiesce_in_progress_id);
		ha->quiesce_in_progress_id = NULL;
	}
	if(ha->quiesce_id) {
		if (kill_timeout == KILL_WITH_TIMEOUT)
			untimeout(ha->quiesce_id);
		ha->quiesce_id = NULL;
		ha->quiesce_time = NULL;
	}
	/* try to startup and pending commands */
	if (!(ha->host_flags & AFLG_HA_DRAIN_IO))
		ql_start_scsi(ha->chip_info);
}

static int 
ql_download_fw(pCHIP_INFO chip_info)
{	
	u_short     mbox_sts[8];
	pISP_REGS   isp = (pISP_REGS)chip_info->ha_base;
	int         count;
#ifndef LOAD_VIA_DMA
	u_short     checksum;
	u_short*    w_ptr;
#else	
        paddr_t     p_ptr;
#endif
	uint        Config_Reg;
	int         rc = 0;

	QL_PCI_OUTH(&isp->bus_icr, ICR_ENABLE_RISC_INT | ICR_SOFT_RESET);
	flushbus();
	DELAY(2);
	QL_PCI_OUTH(&isp->hccr, HCCR_CMD_RESET);
	QL_PCI_OUTH(&isp->hccr, HCCR_CMD_RELEASE);


	QL_PCI_OUTH(&isp->hccr, HCCR_WRITE_BIOS_ENABLE);

	atomicSetUint(&chip_info->chip_flags, AFLG_CHIP_DOWNLOAD_FIRMWARE);

#ifdef QL_SRAM_PARITY

	/* turn on the sram parity support first and let the routine */
	/* to determine if the controller is capable to support sram parity */	


	atomicSetUint(&chip_info->chip_flags, AFLG_CHIP_SRAM_PARITY_ENABLE);
	if (ql_enable_sram_parity(chip_info, 1)) {
		rc = 7;
		goto download_failed;
	}


#endif

	ql_enable_intrs(chip_info);

#ifdef LOAD_FIRMWARE

	/* If we're going to load our own firmware, we need to */
	/* stop any currently running firmware (if any).  */

#ifdef MAILBOX_TEST
	if (ql_mbox_cmd(chip_info, mbox_sts, 8, 8,
			MBOX_CMD_MAILBOX_REGISTER_TEST,
		        (ushort)0x1111,
		        (ushort)0x2222,
		        (ushort)0x3333,
		        (ushort)0x4444,
		        (ushort)0x5555,
		        (ushort)0x6666,
		        (ushort)0x7777)) 
	{
		rc = 1;
		goto download_failed;
	}
#endif /* MAILBOX TEST */

#endif /* LOAD_FIRMWARE */


	/* At this point, the RISC is running, but not any loaded firmware.*/
	/* We can now either load new firmware or restart the previously loaded */
	/* firmware. */

	/* Initialize the Control Parameter configuration.  */

	/* Build the value for the ISP Configuration 1 Register.  */
	Config_Reg = chip_info->chip_parameters.Fifo_Threshold;


	/* always set burst enable */
	Config_Reg |= CONF_1_BURST_ENABLE;

	QL_PCI_OUTH(&isp->hccr, HCCR_CMD_PAUSE);   /* do I need to do this? */
	QL_PCI_OUTH(&isp->bus_config1, Config_Reg);
	QL_PCI_OUTH(&isp->bus_sema, 0);
	DELAY(100);
	QL_PCI_OUTH(&isp->hccr, HCCR_CMD_RELEASE); /* and this? */

#ifdef LOAD_FIRMWARE

	/* Okay, we're ready to load our own firmware. */
	/* We have two ways of doing this: DMA or word by word. */
	/* I've left both version in because it may help to bring */
	/* up a a new system. */

#ifdef LOAD_VIA_DMA
	    {
		ushort *firmware,*prom_firmware;
		ushort count;
		u_char *charp;
		ushort holder;

		prom_firmware=(ushort *)chip_info->risc_code;
		firmware = (ushort *)QL_KMEM_ZALLOC(4+sizeof(chip_info->risc_code),
			VM_CACHEALIGN | VM_NOSLEEP | VM_DIRECT | VM_PHYSCONTIG);

		charp = (u_char *)firmware;
		for(count=0;count<2+chip_info->risc_code_length;count++)
		{
			holder=*(prom_firmware++);
			*charp++=(holder & 0xff);
			*charp++=((holder>>8) & 0xff);
		}


#ifdef FLUSH
		MR_DCACHE_WB_INVAL(firmware,4+sizeof(chip_info->risc_code));
#endif
                p_ptr = pciio_dmatrans_addr(chip_info->pci_vhdl, NULL, 
					    kvtophys(firmware), 
					    sizeof(chip_info->risc_code),
					    PCIIO_DMA_DATA |
					    PCIIO_BYTE_STREAM |
					    PCIIO_DMA_A64);

		ASSERT(kvtophys(firmware) < 2L*1024L*1024L*1024L);
		/* Load my version of the executable firmware by DMA.  */

		if (ql_mbox_cmd(chip_info, mbox_sts, 5, 5,
				MBOX_CMD_LOAD_RAM,
				(short)chip_info->risc_code_addr,
				(short)((u_int)p_ptr >> 16),
				(short)((u_int)p_ptr & 0xFFFF),
				chip_info->risc_code_length,
				0,0,0)) 
		{
			rc = 2;
			kern_free(firmware);
			goto download_failed;
		}
		kern_free(firmware);
	    }

	/* go get the first 10 and print them out */
	for (count=0;count<10;count++) {
		if (ql_mbox_cmd(chip_info, mbox_sts, 2, 3,
				MBOX_CMD_READ_RAM_WORD,
				(u_short) (chip_info->risc_code_addr + count), /* risc addr */
				0,0,0,0,0,0)) 
		{
			rc = 3;
			goto download_failed;
		}
	}
#else /* ! LOAD_VIA_DMA */

	/* 
	 * Load executable firmware one word at a time, computing
	 * checksum as we go.
	 */
	w_ptr = &chip_info->risc_code[0];
	checksum = 0;
	for (count = 0; count < chip_info->risc_code_length; count++) {
		if (ql_mbox_cmd(chip_info, mbox_sts, 3, 3,
				MBOX_CMD_WRITE_RAM_WORD,
				(u_short)(chip_info->risc_code_addr + count),  /* risc addr */
				*w_ptr,				      /* risc data */
				0,0,0,0,0)) 
		{
			rc = 4;
			goto download_failed;
		}
		checksum += *w_ptr++;
	}
#endif /* LOAD_VIA_DMA */

	/* Start ISP firmware.  */
	if (ql_mbox_cmd(chip_info, mbox_sts, 2, 1,
			MBOX_CMD_EXECUTE_FIRMWARE,
			chip_info->risc_code_addr,
			0,0,0,0,0,0)) 
	{
		rc = 5;
		goto download_failed;
	}
#endif /* LOAD_FIRMWARE */
	if (showconfig) {
		if (ql_mbox_cmd(chip_info, mbox_sts, 1, 3,
				MBOX_CMD_ABOUT_FIRMWARE,
				0,0,0,0,0,0,0)) 
		{
			rc = 6;
			goto download_failed;
		}
		ql_cpmsg(chip_info->ctlr_vhdl[0], "Firmware version: %d.%d.%d\n",
			 mbox_sts[1], mbox_sts[2], mbox_sts[3]);
	}

	atomicClearUint(&chip_info->chip_flags, AFLG_CHIP_DOWNLOAD_FIRMWARE);


	return(0);
      download_failed:
	ql_cpmsg(chip_info->pci_vhdl, "ql_download_fw failed (%d)\n", rc);
	return(rc);
}

static int 
ql_reset_interface(pCHIP_INFO chip, int reset)
{
	u_short     	mbox_sts[8];
	pISP_REGS   	isp = (pISP_REGS)chip->ha_base;
	paddr_t     	p_ptr;
	int		i;
	targ_t          targ;
        lun_t           lun;
	ql_local_info_t *qli;
        vertex_hdl_t    lun_vhdl,targ_vhdl;
	u_short		mailbox0;
	int	        retry_cnt;
	int             rc = 0;
	pHA_INFO	ha=NULL;


        /*
	*  reset AFLG_INITIALIZED for adapter
	*  set the defaults for the devices (wide might change if hot plugged) 
        *  reset DFLG_INITIALIZED for all attached devices
	*  reset DFLG_CONTINGENT_ALLEGIANCE for all attached devices
        *  disable interrupt
        *  pause RISC
        *  reset ISP1040
        *  clear the CMD & DATA DMA chan
        *  wait for risc reset
        */

	atomicClearUint(&chip->chip_flags, AFLG_CHIP_INITIALIZED);

	for (i=0; i < chip->channel_count; i++) {
		ha = chip->ha_info[i];
		atomicSetUint(&ha->host_flags, AFLG_HA_SEND_MARKER);
        	for(targ=0 ; targ<QL_MAXTARG ; targ++) {
            		if(scsi_targ_vertex_exist(ha->ctlr_vhdl, targ,&targ_vhdl) == VERTEX_FOUND) {
                		for(lun=0 ; lun<QL_MAXLUN ; lun++) {
                    			if((lun_vhdl=scsi_lun_vhdl_get(ha->ctlr_vhdl,targ,lun))!=GRAPH_VERTEX_NONE) {
                        			qli = SLI_INFO(scsi_lun_info_get(lun_vhdl));
                        			atomicClearUint(&qli->qli_dev_flags, (DFLG_INITIALIZED | DFLG_CONTINGENT_ALLEGIANCE));
			
                    			}
                		}
            		}
        	}
	}

	chip->queue_space = 0;
	chip->request_in  = 0;
	chip->request_out = 0;
	chip->response_out = 0;
	chip->request_ptr = chip->request_base;
	chip->response_ptr = chip->response_base;

	/*
	 * Mark all responses as dead.
	 */
	{
		int          i;
		status_entry *p;

		for (p = (status_entry *)chip->response_base, i = 0; i < chip->ql_response_queue_depth; ++i, ++p)
			p->handle = 0xDEAD;
	}

	QL_PCI_OUTH(&isp->bus_icr, 0); /* disable interrupts */

	/*
	 * Pause RISC and wait till it's paused.
	 */
	QL_PCI_OUTH(&isp->hccr, HCCR_CMD_PAUSE);
	flushbus();
	for (retry_cnt = 10000; ; ) {
		DELAY(100);
		if (QL_PCI_INH(&isp->hccr) & HCCR_PAUSE)
			break;
		if (--retry_cnt <= 0) {
			rc = 1;
			goto reset_failed;
		}
	}
	
	/*
	 * Reset ISP and wait till it's reset.
	 */
	QL_PCI_OUTH(&isp->bus_icr,  ICR_SOFT_RESET);
	flushbus();
	for (retry_cnt = 10000; ; ) {
		DELAY(100);
		if (!(QL_PCI_INH(&isp->hccr) & HCCR_RESET))
			break;
		if (--retry_cnt <= 0) {
			rc = 2;
			goto reset_failed;
		}
	}

	if (chip->device_id == QLogic_1040_DEVICE_ID) {

		/* 1040 only */
		QL_PCI_OUTH(&isp->control_dma_control, DMA_CON_RESET_INT | DMA_CON_CLEAR_FIFO | DMA_CON_CLEAR_CHAN);
		QL_PCI_OUTH(&isp->data_dma_control,    DMA_CON_RESET_INT | DMA_CON_CLEAR_FIFO | DMA_CON_CLEAR_CHAN);

	} else {

		/* 1080, 1240, and 1280 only and we double the register name with other banks */
		u_short tmp_val, tmp_val1;
		
		/* switch to DMA bank registers file */
		tmp_val = QL_PCI_INH(&isp->bus_config1);
		QL_PCI_OUTH(&isp->bus_config1, tmp_val|CONF_1_1240_DMA_SEL);
		flushbus();

		/*  write the pci base + address 0x82 to reset DMA command  */
		QL_PCI_OUTH(&isp->r1, DMA_CON_RESET_INT | DMA_CON_CLEAR_FIFO | DMA_CON_CLEAR_CHAN);

		/* write the pci base + address 0xA2 to reset DMA Data 0 channel */
		QL_PCI_OUTH(&isp->ivr, DMA_CON_RESET_INT | DMA_CON_CLEAR_FIFO | DMA_CON_CLEAR_CHAN);

		/* write the pci base + address 0xC2 to reset DMA Data 1 channel */
		QL_PCI_OUTH(&isp->c2, DMA_CON_RESET_INT | DMA_CON_CLEAR_FIFO | DMA_CON_CLEAR_CHAN);
		flushbus();

		tmp_val1 = QL_PCI_INH(&isp->accumulator);
		QL_PCI_OUTH(&isp->accumulator, tmp_val1|DCR_DMA_BURST_ENABLE);
		flushbus();

		tmp_val1 = QL_PCI_INH(&isp->hccr);
		QL_PCI_OUTH(&isp->hccr, tmp_val1|DCR_DMA_BURST_ENABLE);
		flushbus();

		/* switch back the registers bank */
		QL_PCI_OUTH(&isp->bus_config1, tmp_val);
		flushbus();
	}


	for (i = 0;i < QL_RESET; i++) {
		mailbox0 = QL_PCI_INH(&isp->mailbox0);
		if (mailbox0 != MBOX_STS_BUSY)
			break;
		DELAY(1000);
	}

#ifdef QL_SRAM_PARITY
	ql_enable_sram_parity(chip,0);
#endif /* QL_SRAM_PARITY */

	QL_PCI_OUTH(&isp->bus_config1, chip->chip_parameters.Fifo_Threshold|CONF_1_BURST_ENABLE);
	QL_PCI_OUTH(&isp->bus_sema, 0);
	QL_PCI_OUTH(&isp->hccr, HCCR_CMD_RELEASE);
	QL_PCI_OUTH(&isp->hccr, HCCR_WRITE_BIOS_ENABLE);

	/* At this point, the firmware is up and running and we're ready to */
	/* initialze for normal startup.  If you are bringing up the ISP */
	/* device for the first time, leave normal_startup undefined until */
	/* you have worked out the bugs of getting the firmware loaded and */
	/* running.  */

	ql_enable_intrs(chip);


#if SN0
	/* enable differential */
	{
	    if (private.p_sn00) {
		    pISP_REGS  isp = (pISP_REGS) chip->ha_base;
		    QL_PCI_OUTH(&isp->b6,ENABLE_WRITE_SN00);
		    QL_PCI_OUTH(&isp->b4, 0);
		    DELAY(100000);
	    }
	}
#endif
	    /* Start ISP firmware.  */
	if (ql_mbox_cmd(chip, mbox_sts, 2, 1,
			MBOX_CMD_EXECUTE_FIRMWARE,
			chip->risc_code_addr,
			0,0,0,0,0,0))
	{
		rc = 3;
		goto reset_failed;
	}

	/* Set Bus Control Parameters.  */
	if (ql_set_pci_control_parameters(chip, mbox_sts)) {
		rc = 4;
		goto reset_failed;
	}


	/* Set the Clock Rate for the board */

	if (ql_set_clock_rate(chip, mbox_sts)) {
		rc = 5;
		goto reset_failed;
	}

#ifdef USE_MAPPED_CONTROL_STREAM
	if (chip->response_dmaptr == NULL) {
		chip->response_dmamap = pciio_dmamap_alloc(chip->pci_vhdl, NULL,
							 sizeof(queue_entry)*chip->ql_response_queue_depth,
							 PCIIO_DMA_CMD |
							 PCIIO_WORD_VALUES);
		chip->response_dmaptr = pciio_dmamap_addr(chip->response_dmamap, kvtophys(chip->response_base), 
							sizeof(queue_entry)*chip->ql_response_queue_depth);
	}
#else
	/* 
	 * PCIIO_DMA_CMD turns prefetcher off, barrier on.
	 */
	if (chip->response_dmaptr == NULL) {
		chip->response_dmaptr = pciio_dmatrans_addr(chip->pci_vhdl, NULL, 
							  kvtophys(chip->response_ptr),
							  sizeof(queue_entry)*chip->ql_response_queue_depth,
							  PCIIO_BYTE_STREAM | 
							  PCIIO_DMA_CMD |
							  PCIIO_DMA_A64);
	}
#endif /* USE_MAPPED_CONTROL_STREAM */
	p_ptr = chip->response_dmaptr;

	bzero(chip->response_base, chip->ql_response_queue_depth*sizeof(queue_entry));
#ifdef FLUSH
	MRW_DCACHE_WB_INVAL(chip->response_base,
		chip->ql_response_queue_depth*sizeof(queue_entry));
#endif	/* FLUSH */

	/* Initialize response queue.  */
#ifndef USE_MAPPED_CONTROL_STREAM
	if (ql_mbox_cmd(chip, mbox_sts, 8, 8,
			MBOX_CMD_INIT_RESPONSE_QUEUE_64,
			chip->ql_response_queue_depth,
			(u_short)((u_long)p_ptr >> 16),
			(u_short)((u_long)p_ptr & 0xFFFF),
			0,
			chip->response_out,
			(u_short) ((u_long)p_ptr >> 48),
			(u_short) ((u_long)p_ptr >> 32)))
#else
	if (ql_mbox_cmd(chip, mbox_sts, 6, 6,
			MBOX_CMD_INIT_RESPONSE_QUEUE,
			chip->ql_response_queue_depth,
			(u_short)((u_int)p_ptr >> 16),
			(u_short)((u_int)p_ptr & 0xFFFF),
			0,
			chip->response_out,
			0,
			0))
#endif /* !USE_MAPPED_CONTROL_STREAM */
	{
		rc = 6;
		goto reset_failed;
	}


	chip->request_ptr = chip->request_base;

#ifdef USE_MAPPED_CONTROL_STREAM
	if (chip->request_dmaptr == NULL) {
		chip->request_dmamap = pciio_dmamap_alloc(chip->pci_vhdl, NULL,
							sizeof(queue_entry)*chip->ql_request_queue_depth,
							PCIIO_DMA_CMD | 
							PCIIO_WORD_VALUES);
		chip->request_dmaptr = pciio_dmamap_addr(chip->request_dmamap, kvtophys(chip->request_base), 
						       sizeof(queue_entry)*chip->ql_request_queue_depth);
	}
#else
	if (chip->request_dmaptr == NULL) {
		chip->request_dmaptr = pciio_dmatrans_addr(chip->pci_vhdl, NULL,
							 kvtophys(chip->request_base),
							 sizeof(queue_entry)*chip->ql_request_queue_depth,
							 PCIIO_BYTE_STREAM |
							 PCIIO_DMA_CMD |
							 PCIIO_DMA_A64);
	}
#endif
	p_ptr = chip->request_dmaptr;
	bzero(chip->request_base,chip->ql_request_queue_depth*sizeof(queue_entry));

	/* Initialize request queue.  */

#ifndef USE_MAPPED_CONTROL_STREAM
	if (ql_mbox_cmd(chip, mbox_sts, 8, 8,
			MBOX_CMD_INIT_REQUEST_QUEUE_64,
			chip->ql_request_queue_depth,
			(u_short)((u_long)p_ptr >> 16),
			(u_short)((u_long)p_ptr & 0xFFFF),
			chip->request_in,
			0,
			(u_short) ((u_long)p_ptr >> 48),
			(u_short) ((u_long)p_ptr >> 32)))
#else
	if (ql_mbox_cmd(chip, mbox_sts, 5, 5,
			MBOX_CMD_INIT_REQUEST_QUEUE,
			chip->ql_request_queue_depth,
			(short)((u_int)p_ptr >> 16),
			(short)((u_int)p_ptr & 0xFFFF),
			chip->request_in,
			0,
			0,
			0))
#endif /* !USE_MAPPED_CONTROL_STREAM */

	{
		rc = 7;
		goto reset_failed;
	}

	if (reset) {
		for (i=0; i < chip->channel_count; ++i) {
			ha = chip->ha_info[i];	

			if (ql_set_bus_reset(chip, mbox_sts, ha->channel_id)) {
				rc = 8;
				goto reset_failed;
			}

		}
        }
	

	/* Set SCSI id */
	if (ql_set_scsi_id(chip, mbox_sts)) {
		rc = 9;
		goto reset_failed;
	}


	/* Set Selection Timeout */
	if (ql_set_selection_timeout(chip, mbox_sts)) {
		rc = 10;
		goto reset_failed;
	}


	/* Set RETRY limits.  */
	if (ql_set_retry_count(chip, mbox_sts)) {
		rc = 11;
		goto reset_failed;
	}

	/* Set Tag Age Limits.  */
	if (ql_set_tag_age_limit(chip, mbox_sts)) {
		rc = 12;
		goto reset_failed;
	}

	/* Set Active Negation.  */
	if (ql_set_negation_state(chip, mbox_sts)) {
		rc = 13;
		goto reset_failed;
	}

	/* Set Asynchronous Data Setup Time.  */
	if (ql_set_asynchronous_data_setup_time(chip, mbox_sts)) {
		rc = 14;
		goto reset_failed;
	}


	/* Set Data Overrun Recovery Mode  */
	if (ql_set_data_over_recovery_mode(chip, mbox_sts)) {
		rc = 15;
		goto reset_failed;
	}

	/* set scsi bus reset delay paramters */
	if (ql_set_reset_delay_parameters(chip, mbox_sts)) {
		rc = 16;
		goto reset_failed;
	}

	/* set system parameters such as memory timing */
	if (ql_set_system_parameters(chip, mbox_sts)) {
		rc = 17;
		goto reset_failed;
	}

	/* set firmware features such as */
	if (ql_set_firmware_features(chip, mbox_sts)) {
		rc = 18;
		goto reset_failed;
	}

#if 0
	/* set host data so that if there is another party driver can identify */
	if (ql_set_host_data(chip, mbox_sts)) {
		rc = 19;
		goto reset_failed;
	}
#endif

	/* Indicate we successfully initialized this ISP! */
 	atomicSetUint(&chip->chip_flags, AFLG_CHIP_INITIALIZED);
	return(0);
 reset_failed:
	ql_cpmsg(chip->pci_vhdl, "ql_reset_interface failed (%d)\n", rc);
	return(rc);
}

static void
ql_flush_queue(pHA_INFO ha, uint sr_status)
{
	int		      i, id, lun;
	pREQ_INFO	      r_ptr;
	scsi_request_t*       cmp_forw = NULL;
	scsi_request_t*       cmp_back = NULL;
	scsi_request_t*       request = NULL;
	vertex_hdl_t          lun_vhdl;
	ql_local_info_t      *qli;

        for (id = 0; id < QL_MAXTARG; id++) {
	        if (id == ha->ql_defaults.Initiator_SCSI_Id)
			continue;
		/*  
		 * Clean up queued target commands
		 */
		IOLOCK(ha->timeout_info[id].per_tgt_tlock,s);
		for (i = 0; i <= MAX_REQ_INFO ; i++) {
			r_ptr = &ha->reqi_block[id][i];
			if (r_ptr->req != NULL) { 
				if (cmp_forw) {
		    			cmp_back->sr_ha = r_ptr->req;
					cmp_back        = r_ptr->req;
				}
				else
				{
		    			cmp_forw = r_ptr->req;
		    			cmp_back = r_ptr->req;
				}
				r_ptr->req->sr_ha  = NULL;
				r_ptr->req = NULL;
			}
		}
		ha->timeout_info[id].ql_dups = 0;
        	ha->timeout_info[id].current_timeout = 0;
		ha->timeout_info[id].watchdog_timer = 0;
        	ha->timeout_info[id].time_base = 0;
		ha->timeout_info[id].ql_tcmd = 0;
		ha->timeout_info[id].timeout_state = 0;
		IOUNLOCK(ha->timeout_info[id].per_tgt_tlock,s);
		
		/* 
		 * Clean up queued LUN commands 
		 */
		for (lun = 0; lun < QL_MAXLUN; ++lun) {
			if ((lun_vhdl = scsi_lun_vhdl_get(ha->ctlr_vhdl, id, lun)) == GRAPH_VERTEX_NONE)
				continue;
			qli = SLI_INFO(scsi_lun_info_get(lun_vhdl));
			IOLOCK(qli->qli_lun_mutex, s);
			if (qli->qli_dev_flags & DFLG_ABORT_IN_PROGRESS && qli->qli_awaitf) {
				if (cmp_forw) {
					cmp_back->sr_ha = qli->qli_awaitf;
					cmp_back        = qli->qli_awaitb;
				}
				else {
					cmp_forw = qli->qli_awaitf;
					cmp_back = qli->qli_awaitb;
				}
				cmp_back->sr_ha  = NULL;		    
				qli->qli_awaitf = qli->qli_awaitb = NULL;
			}
			atomicClearUint(&qli->qli_dev_flags, DFLG_ABORT_IN_PROGRESS);

			if (qli->qli_dev_flags & DFLG_INIT_IN_PROGRESS && qli->qli_iwaitf) {
				if (cmp_forw) {
					cmp_back->sr_ha = qli->qli_iwaitf;
					cmp_back        = qli->qli_iwaitb;
				}
				else {
					cmp_forw = qli->qli_iwaitf;
					cmp_back = qli->qli_iwaitb;
				}
				cmp_back->sr_ha  = NULL;		    
				qli->qli_iwaitf = qli->qli_iwaitb = NULL;
			}
			atomicClearUint(&qli->qli_dev_flags, DFLG_INIT_IN_PROGRESS);

			qli->qli_cmd_awcnt = qli->qli_cmd_iwcnt = qli->qli_cmd_rcnt = 0;
			IOUNLOCK(qli->qli_lun_mutex, s);
		} /* for (lun) */
	} /* for (id) */
	ha->ql_ncmd = 0;
	QL_MUTEX_EXIT(ha->chip_info);
        while (request = cmp_forw) {
            cmp_forw = request->sr_ha;

	    if (request->sr_status == 0)
		request->sr_status = sr_status;
            /* callback now */
            (*request->sr_notify)(request);

        }
	QL_MUTEX_ENTER(ha->chip_info);
}

static void
set_new_timebase(pHA_INFO ha,int tgt)
{
        int time_base = 0;
        int i,dups=0;
        pREQ_INFO ri_ptr = &ha->reqi_block[tgt][0];


        for (i = 1; i <= MAX_REQ_INFO; i++, ri_ptr++) {
                if (ri_ptr->req) {
			if (time_base < ri_ptr->timeout) {
				dups = 0;
				time_base = ri_ptr->timeout;
			}
			if (time_base == ri_ptr->timeout) 
				dups++;
		}
        }
	ha->timeout_info[tgt].time_base = time_base;
	ha->timeout_info[tgt].ql_dups = dups;
}

#ifdef QL_INTR_DEBUG
static void
ql_interrupt_timer(void)
{
	pHA_INFO	ha;
	pISP_REGS   	isp;
	u_short         isr;
	u_short         response_in;
	
	for (ha = ql_ha_list; ha != NULL ; ha = ha->next_ha) 
	{
		LOCK(ha->chip_info->res_lock);
		isp = (pISP_REGS)ha->chip_info->ha_base;
		isr = QL_PCI_INH(&isp->bus_isr) & BUS_ISR_RISC_INT;
		response_in = QL_PCI_INH(&isp->mailbox5);
		if ((ha->chip_info->chip_flags & AFLG_CHIP_INITIALIZED) && (isr == 0) && (response_in != ha->chip_info->response_out))
		{
			if (ha->chip_info->ha_interrupt_pending == 0)
				ha->chip_info->ha_interrupt_pending = lbolt;
			if ((lbolt - ha->chip_info->ha_interrupt_pending) > 2) {
				cmn_err(CE_DEBUG, "Response available, but ISR = 0 - ha = 0x%x\n", ha);
			}
		}
		if ((ha->chip_info->chip_flags & AFLG_CHIP_INITIALIZED) && isr && (response_in != ha->chip_info->response_out))
		{
			if (ha->chip_info->ha_interrupt_pending == 0)
				ha->chip_info->ha_interrupt_pending = lbolt;
			if ((lbolt - ha->chip_info->ha_interrupt_pending) > 100) {
				printf("Interrupt pending > 1000mS - ha 0x%x\n", ha);
			}
		}
		UNLOCK(ha->chip_info->res_lock);
	}
	fast_itimeout(ql_interrupt_timer, NULL, 1, TIMEOUT_SPL);
}
#endif

static void 
ql_watchdog_timer(void)
{
	pHA_INFO 	ha;
	pHA_INFO 	ha_tmp;
	pCHIP_INFO 	chip_info;
	pTO_INFO 	timeout_info;
	/*REFERENCED*/
	int	s;
	int	tgt;
	int	current_time,dt;
	int	bus_reset_needed;
	int	chip_reset_needed;

	/* needed to fix watchdog timer problem with dead requests and polling requests */
	long	quiesce_time;
	int	i;
	pREQ_INFO r_ptr;
	


	for ( ha = ql_ha_list; ha != NULL ; ha = ha->next_ha) {
		chip_info = ha->chip_info;

#ifdef QL_WATCHDOG_TIMER
       
	  if(ha->ql_ncmd)
	    ql_cpmsg(ha->ctlr_vhdl, "ql_watchdog_timer and ql_ncmd = %d.\n",ha->ql_ncmd);
#endif

	    /* 
	     * Check for pending mailbox command
	     */
	    if ((chip_info->chip_flags & (AFLG_CHIP_MAIL_BOX_POLLING | AFLG_CHIP_MAIL_BOX_WAITING | AFLG_CHIP_MAIL_BOX_DONE)) == AFLG_CHIP_MAIL_BOX_WAITING) {

		    QL_MUTEX_ENTER(chip_info);

		    if ((chip_info->chip_flags & (AFLG_CHIP_MAIL_BOX_POLLING | AFLG_CHIP_MAIL_BOX_WAITING | AFLG_CHIP_MAIL_BOX_DONE)) == AFLG_CHIP_MAIL_BOX_WAITING) {
			    if (chip_info->mbox_timeout_info > 0)
				    ha->chip_info->mbox_timeout_info -= WATCHDOG_TIME;
			    else
				    vsema(&ha->chip_info->mbox_done_sema);
		    }
		    QL_MUTEX_EXIT(chip_info);
	    }

	    /* 
	     * Check for pending SCSI commands
	     */
	    bus_reset_needed = chip_reset_needed = 0;
	    if  (ha->ql_ncmd) {
		for (tgt = 0; tgt < QL_MAXTARG ; tgt++) {
		        if (tgt == ha->ql_defaults.Initiator_SCSI_Id) 
			        continue;
			timeout_info = &ha->timeout_info[tgt];
	    		IOLOCK(timeout_info->per_tgt_tlock,s);
			if (timeout_info->ql_tcmd) {
#ifdef QL_WATCHDOG_TIMER
				ql_cpmsg(ha->ctlr_vhdl, "target %d, has ql_tcmd %d current_timeout %d t_last_intr %d. \n",
					tgt, timeout_info->ql_tcmd, timeout_info->current_timeout, timeout_info->t_last_intr); 
				ql_cpmsg(ha->ctlr_vhdl, "ql_dups %d, time_base %d watchdog_timer %d .\n", 
					timeout_info->ql_dups, timeout_info->time_base, timeout_info->watchdog_timer);	
#endif
				
				timeout_info->current_timeout -= WATCHDOG_TIME;
				timeout_info->watchdog_timer += WATCHDOG_TIME;
				current_time = lbolt /HZ;

				if ((timeout_info->current_timeout < 0) 
					&& (!(ha->host_flags & AFLG_HA_DRAIN_IO))) {
					atomicSetUint(&ha->host_flags, AFLG_HA_DRAIN_IO);
				} else { 
					if (ha->host_flags & AFLG_HA_DRAIN_IO) {
						dt = current_time - timeout_info->t_last_intr;
						if (dt > timeout_info->time_base){
							ql_tpmsg(ha->ctlr_vhdl, tgt, "SCSI command timeout: %d commands: ",
						 		timeout_info->ql_tcmd);
							{

								for (i = 0; i <= MAX_REQ_INFO ; i++) {
									r_ptr = &ha->reqi_block[tgt][i];
									if (r_ptr->req != NULL)
										cmn_err(CE_CONT, "0x%x ", r_ptr->req->sr_command[0]);
								}
								cmn_err(CE_CONT, "\n");
							}
							chip_reset_needed++;
							timeout_info->watchdog_timer = 0;
						} 
					} 
				}
				/* need to scan to all the targets to make sure requests are not expired. */ 
				if (ha->timeout_info[tgt].watchdog_timer > ha->timeout_info[tgt].time_base) {
					for (i=0; i<=MAX_REQ_INFO; i++) {
					 	r_ptr = &ha->reqi_block[tgt][i]; 
						if (r_ptr->req != NULL) {
							/* if the last interrupt resets the time base 
							and current timeout then queisce the bus and wait for 
							for all the requests finished or timeout */
							if( (r_ptr->starttime + r_ptr->timeout) < current_time) {

								quiesce_time = (ha->timeout_info[tgt].time_base + WATCHDOG_TIME)*HZ;
#ifdef QL_WATCHDOG_TIMER
								ql_cpmsg(ha->ctlr_vhdl, "watchdog timer %d, starttime %d, \
									timeout %d current %d .\n",ha->timeout_info[tgt].watchdog_timer, r_ptr->starttime, r_ptr->timeout, current_time); 
								ql_cpmsg(ha->ctlr_vhdl," quiesce bus target %d quiesce time %d.\n", tgt, quiesce_time);
#endif
								ql_quiesce_bus(ha, quiesce_time);
								break;
							}
						}
					}
					ha->timeout_info[tgt].watchdog_timer = 0;
				}
			} else {
				ha->timeout_info[tgt].ql_dups = 0;
				ha->timeout_info[tgt].current_timeout = 0;
				ha->timeout_info[tgt].time_base = 0;
				ha->timeout_info[tgt].watchdog_timer = 0;
			}
			IOUNLOCK(ha->timeout_info[tgt].per_tgt_tlock,s);
		}

		if (chip_reset_needed) {	
			QL_MUTEX_ENTER(chip_info);
	               	ql_cpmsg(ha->ctlr_vhdl, "Resetting SCSI chip.\n");
#ifdef DEBUG
		        {
				pISP_REGS	isp = (pISP_REGS) chip_info->ha_base;
				ushort		isr = QL_PCI_INH(&isp->bus_isr);
				ushort		bus_sema = QL_PCI_INH(&isp->bus_sema);
				ushort		m4 = QL_PCI_INH(&isp->mailbox4);
				ushort		m5 = QL_PCI_INH(&isp->mailbox5);

				ql_cpmsg(ha->ctlr_vhdl, 
					 "(isr 0x%x, bus_sema 0x%x, m4 0x%x, m5 0x%x, "
					 "response_out 0x%x, request_in 0x%x)\n",
					 isr, bus_sema, m4, m5, ha->chip_info->response_out, ha->chip_info->request_in);
			}
#endif
			ql_reset_interface(chip_info,1);
			for (i=0; i < chip_info->channel_count; i++){
				ha_tmp = chip_info->ha_info[i];
				ql_flush_queue(ha_tmp, SC_CMDTIME);
				atomicClearUint(&ha_tmp->host_flags, AFLG_HA_DRAIN_IO);
			}
			QL_MUTEX_EXIT(chip_info);
		} 
		else if (bus_reset_needed) {
			ql_cpmsg(ha->ctlr_vhdl, "Resetting SCSI bus.\n");
			QL_MUTEX_ENTER(chip_info);
			atomicSetUint(&chip_info->chip_flags, AFLG_CHIP_WATCHDOG_TIMER);
			QL_MUTEX_EXIT(chip_info);
                        if (ql_reset(ha->ctlr_vhdl))
			{
				/* if bus reset failed that chip reset must take place */
				QL_MUTEX_ENTER(chip_info);
				atomicClearUint(&chip_info->chip_flags, AFLG_CHIP_WATCHDOG_TIMER);
				ql_reset_interface(chip_info,1);
				for (i=0; i < chip_info->channel_count; i++){
					ha_tmp = chip_info->ha_info[i];
					ql_flush_queue(ha_tmp, SC_CMDTIME);
					atomicClearUint(&ha_tmp->host_flags, AFLG_HA_DRAIN_IO);
				}
				QL_MUTEX_EXIT(chip_info);
			} 
			else 
			/*
			 * Reset Target parameters to their default
			 * values.  
			 */
			{
				int id;
				ushort mbox_sts[8];

				QL_MUTEX_ENTER(chip_info);
				for (id = 0; id < QL_MAXTARG; ++id) {
					if (id == ha->ql_defaults.Initiator_SCSI_Id)
						continue;
					if (scsi_lun_vhdl_get(ha->ctlr_vhdl, id, 0) == GRAPH_VERTEX_NONE)
						continue;
					if (ql_set_scsi_parameters(ha->chip_info, mbox_sts, 
					    ha->channel_id, id) ) {
						ql_tpmsg(ha->ctlr_vhdl, id, "MBOX_CMD_SET_TARGET_PARAMETERS failed\n");
					}
				}
				atomicClearUint(&chip_info->chip_flags, AFLG_CHIP_WATCHDOG_TIMER);
				ql_flush_queue(ha, SC_CMDTIME);
				atomicClearUint(&ha_tmp->host_flags, AFLG_HA_DRAIN_IO);

				QL_MUTEX_EXIT(chip_info);
			}
		}
	    }
	    if (!(ha->host_flags & AFLG_HA_DRAIN_IO) && ((ha->waitf) || (ha->req_forw)))
            	ql_start_scsi(ha->chip_info);
	}
	itimeout(ql_watchdog_timer, NULL, WATCHDOG_TIME * HZ, TIMEOUT_SPL);
}

/*
 * The older QL controller sometimes gives us PIO Timeouts.
 * Handle them here.
 */
/*ARGSUSED*/
static int
ql_error_handler(void *parg,
		 int error_code,
		 ioerror_mode_t error_mode,
		 ioerror_t *ioerror)
{
    scsi_ctlr_info_t *ctlr_info = (scsi_ctlr_info_t *)parg;
    pHA_INFO ha = SCI_HA_INFO(ctlr_info);

    if (ha && ha->chip_info->revision >= REV_ISP1040B)
	return IOERROR_UNHANDLED;

#ifdef DEBUG
    cmn_err(CE_ALERT, "%v: IGNORING QL BUS ERROR\n"
	    "Access details follow:",
	    SCI_CTLR_VHDL(ctlr_info));
    ioerror_dump("QL", error_code, error_mode, ioerror);
#endif

    return IOERROR_HANDLED;
}

static void	
ql_service_mbox_interrupt(pCHIP_INFO chip)
{
	pISP_REGS   	isp      = (pISP_REGS)chip->ha_base;
	pHA_INFO    	ha	     = NULL;
	u_short	    	channel_id;
	u_short 	mailbox0 = 0;
	int 		i;


	/* Get a new copy of mailbox 0.  */
	mailbox0 = QL_PCI_INH(&isp->mailbox0);

	/* check for mailbox for command completion and asynchronous
	   event */

	if ( (mailbox0 >= 0x4000) && (mailbox0 <= 0x7fff)) {
		chip->mailbox[7] = QL_PCI_INH(&isp->mailbox7);
		chip->mailbox[6] = QL_PCI_INH(&isp->mailbox6);
		chip->mailbox[5] = QL_PCI_INH(&isp->mailbox5);
		chip->mailbox[4] = QL_PCI_INH(&isp->mailbox4);
		chip->mailbox[3] = QL_PCI_INH(&isp->mailbox3);
		chip->mailbox[2] = QL_PCI_INH(&isp->mailbox2);
		chip->mailbox[1] = QL_PCI_INH(&isp->mailbox1);
		chip->mailbox[0] = QL_PCI_INH(&isp->mailbox0);
	
		if (mailbox0 == 0x4000) 
			atomicSetUint(&chip->chip_flags, AFLG_CHIP_MAIL_BOX_DONE);

		if (chip->chip_flags & AFLG_CHIP_MAIL_BOX_WAITING)
			vsema(&chip->mbox_done_sema);

		goto intr_done;
	}




	/*
	 * mailbox0 = 0x8000 - 0x8fff down  
 	 */
	
	if (mailbox0 & 0x8000) {

		/* get the channel number from mailbox 6 */
		channel_id = chip->mailbox[6]&0x1;

		/* get the ha  */
		ha = (pHA_INFO) chip->ha_info[channel_id];
	}
	
	/* Get the response to our command.  */
	switch (mailbox0) {

	case MBOX_ASTS_SYSTEM_ERROR: 
		ql_cpmsg(ha->ctlr_vhdl, "Unrecoverable SCSI bus state/condition (pc=0x%x)\n",
			 chip->mailbox[1]);
		IOUNLOCK(chip->res_lock,s);
		QL_MUTEX_ENTER(chip);
		ql_reset_interface(chip,1);
		for (i = 0; i<chip->channel_count; i++) {
			ha = chip->ha_info[i];
			ql_flush_queue(ha, SC_CMDTIME);	/* We want to retry these commands */
		}
		QL_MUTEX_EXIT(chip);
		IOUNLOCK(chip->req_lock,s);
		break;

	case MBOX_ASTS_SCSI_BUS_RESET:
		atomicSetUint(&ha->host_flags, AFLG_HA_SEND_MARKER);
		ql_cpmsg(ha->ctlr_vhdl, "SCSI bus RESET seen.\n");
		break;

	case MBOX_STS_INVALID_COMMAND:
		ql_cpmsg(ha->ctlr_vhdl, "Invalid mailbox command: 0x%x\n", mailbox0);
		break;

	case MBOX_ASTS_EXECUTION_TIMEOUT_RESET:
		ql_cpmsg(ha->ctlr_vhdl, "Execution timeout and SCSI bus reset is asserted.\n");
		break;

	case MBOX_ASTS_EXTENDED_MESSAGE_UNDERRUN:
		ql_cpmsg(ha->ctlr_vhdl, "Extended message underrun and Device Reset is asserted.\n");
		break;
	case MBOX_ASTS_SCAM_INTERRUPT:
		ql_cpmsg(ha->ctlr_vhdl, "SCAM interrupt with completion.\n");
		break;

	case MBOX_ASTS_SCSI_BUS_HANG:
		ql_cpmsg(ha->ctlr_vhdl, "SCSI bus hang dues to data overrun.  Reset SCSI bus is needed or set Data Overrun Recovery Mode to 2.\n");
		break;
	
	case MBOX_ASTS_SCSI_BUS_RESET_BY_ISP:
		ql_cpmsg(ha->ctlr_vhdl, "SCSI bus reset by isp is dues to data overrun.\n");
		
		break;

	case MBOX_ASTS_SCSI_BUS_MODE_TRANSITION:
		chip->mailbox[1] = QL_PCI_INH(&isp->mailbox1);
/*
		ql_cpmsg(ha->ctlr_vhdl, "SCSI bus mode switch from :%x to %x\n", ha->bus_mode, chip->mailbox[1]);
*/
		ha->bus_mode = (chip->mailbox[1]>>10) & SCSI_BUS_MODE_MASK;
		break;

	case MBOX_ASTS_REQUEST_QUEUE_WAKEUP:
		/* fall thru */

	case MBOX_ASTS_RESPONSE_TRANSFER_ERROR:
		/* fall thru */

	case MBOX_ASTS_REQUEST_TRANSFER_ERROR:
		/* fall thru */
	default:
		break;
	}
 intr_done:
	QL_PCI_OUTH(&isp->bus_sema, 0);
}


/* added new code to support Low Voltage Differential, High Voltage Differential and Single Ended */

static int 
ql_diffsense(pHA_INFO ha) 
{
	pCHIP_INFO	chip_info = (pCHIP_INFO)ha->chip_info;
	pISP_REGS	isp = (pISP_REGS)chip_info->ha_base;
	int		retry_cnt;
	ushort		tmp1;
	/*REFERENCED*/
	ushort		tmp2;
	ushort		tmp3;
	int		diff_sense;

	/*
	 * To determine DF/SE we read the value of the DIFFS bit in the
	 * SCSI Differential Control Pin. To do so we need to pause the RISC
	 * and perform a register bank switch first.  
	 */
	QL_PCI_OUTH(&isp->hccr, HCCR_CMD_PAUSE); /* Pause RISC */
	flushbus();
	for (retry_cnt = 10000; ; ) 
	{
		DELAY(100);
		if (QL_PCI_INH(&isp->hccr) & HCCR_PAUSE)
			break;
		if (--retry_cnt <= 0)
			return(-1);
	}

	tmp1 = QL_PCI_INH(&isp->bus_config1);
	QL_PCI_OUTH(&isp->bus_config1, tmp1 | 0x0008); /* Perform register bank switch */
	tmp2 = QL_PCI_INH(&isp->bus_config1);
	ASSERT(tmp2 & 0x0008);
		  
	tmp3 = QL_PCI_INH(&isp->f6);
	if (chip_info->device_id == QLogic_1040_DEVICE_ID ||
	    chip_info->device_id == QLogic_1240_DEVICE_ID) {
		diff_sense = ((tmp3 & 0x0200) == 0x0200);
	} else {
		/* LVD */
		if (tmp3 & 0x1000)
			diff_sense = 2;
		else 
		/* HVD */
		if (tmp3 & 0x0800)
			diff_sense = 1;
		else
		/* SE */
		 	diff_sense = 0;

	}

	QL_PCI_OUTH(&isp->bus_config1, tmp1); /* Perform register bank switch */
	tmp2 = QL_PCI_INH(&isp->bus_config1);
	ASSERT(tmp2 == tmp1);

	DELAY(100);

	QL_PCI_OUTH(&isp->hccr, HCCR_CMD_RELEASE);	/* Restart RISC */
	flushbus();
	for (retry_cnt = 10000; ; ) 
	{
		DELAY(100);
		if ((QL_PCI_INH(&isp->hccr) & HCCR_PAUSE) == 0)
			break;
		if (--retry_cnt <= 0)
			return(-1);
	}
	return(diff_sense);
}

static int 
ql_probe_bus(pHA_INFO ha, int max_targets, int probe_all_luns)
{
	pCHIP_INFO	chip_info = ha->chip_info;
	int		disks_found = 0;
	int		targ, lu;
	vertex_hdl_t	lun_vhdl;
	ql_local_info_t *qli;
	
	mutex_lock(&chip_info->probe_lock, PRIBIO);
	atomicSetUint(&ha->host_flags, AFLG_HA_BUS_SCAN_IN_PROGRESS);
	for (targ = 0; targ < max_targets; targ++)
	{
		if (targ == ha->ql_defaults.Initiator_SCSI_Id)
			continue;
		lu = 0;
		DBG(1, "%d", targ);
		if ((lun_vhdl = scsi_lun_vhdl_get(ha->ctlr_vhdl, targ, lu)) == GRAPH_VERTEX_NONE)
			lun_vhdl = ql_device_add(ha->ctlr_vhdl, targ, lu);
		if (!qlinfo(lun_vhdl)) 
		{
			/*
			 * If target no longer exists, remove all the LUN vertices
			 * also. Make sure no one has the LUN open before removing it --
			 * check ref count.
			 */
			for (lu = 0; lu < QL_MAXLUN; lu ++) 
			{
				if ((lun_vhdl = scsi_lun_vhdl_get(ha->ctlr_vhdl, targ, lu)) != GRAPH_VERTEX_NONE) 
				{
					qli = SLI_INFO(scsi_lun_info_get(lun_vhdl));
					if (qli->qli_ref_count == 0)
						ql_device_remove(ha->ctlr_vhdl, targ, lu);
				}
			}
			DBG(1, "- ");
		}
		else 
		{ 
			disks_found++;
			DBG(1, "+ ");
			if (probe_all_luns) 
			{
				lu++;
				for( ; lu < QL_MAXLUN; lu ++) 
				{
					if ((lun_vhdl = scsi_lun_vhdl_get(ha->ctlr_vhdl, targ, lu)) == GRAPH_VERTEX_NONE)
						lun_vhdl = ql_device_add(ha->ctlr_vhdl, targ, lu);
					if (!qlinfo(lun_vhdl)) 
					{
						qli = SLI_INFO(scsi_lun_info_get(lun_vhdl));
						if (qli->qli_ref_count == 0)
							ql_device_remove(ha->ctlr_vhdl, targ, lu);
					}
				}
			}
		}
	}
	atomicClearUint(&ha->host_flags, AFLG_HA_BUS_SCAN_IN_PROGRESS);
	mutex_unlock(&chip_info->probe_lock);
	return(disks_found);
}

static void
ql_cpmsg(dev_t ctlr_vhdl, char *fmt, ...)
{
	va_list		  ap;
	inventory_t      *pinv;
	pHA_INFO	  ha;

	if ((hwgraph_info_get_LBL(ctlr_vhdl, INFO_LBL_INVENT, (arbitrary_info_t *)&pinv) == GRAPH_SUCCESS) &&
	    (pinv->inv_controller != (major_t)-1))
		cmn_err(CE_CONT, "ql%d: ", pinv->inv_controller);
	else {
		ha = ql_ha_info_from_ctlr_get(ctlr_vhdl);
		cmn_err(CE_CONT, "%s: ", ha->ha_name);
	}
	va_start(ap, fmt);
	icmn_err(CE_CONT, fmt, ap);
	va_end(ap);
}

static void
ql_tpmsg(dev_t ctlr_vhdl, u_char target, char *fmt, ...)
{
	va_list		  ap;
	inventory_t      *pinv;
	pHA_INFO	  ha;

	if ((hwgraph_info_get_LBL(ctlr_vhdl, INFO_LBL_INVENT, (arbitrary_info_t *)&pinv) == GRAPH_SUCCESS) &&
	    (pinv->inv_controller != (major_t)-1))
		cmn_err(CE_CONT, "ql%dd%d: ", pinv->inv_controller, target);
	else {
		ha = ql_ha_info_from_ctlr_get(ctlr_vhdl);
		cmn_err(CE_CONT, "%s/%s/%d: ", ha->ha_name, EDGE_LBL_TARGET, target);
	}
	va_start(ap, fmt);
	icmn_err(CE_CONT, fmt, ap);
	va_end(ap);
}

static void
ql_lpmsg(dev_t ctlr_vhdl, u_char target, u_char lun, char *fmt, ...)
{
	va_list		  ap;
	inventory_t      *pinv;
	pHA_INFO	  ha;

	if ((hwgraph_info_get_LBL(ctlr_vhdl, INFO_LBL_INVENT, (arbitrary_info_t *)&pinv) == GRAPH_SUCCESS) &&
	    (pinv->inv_controller != (major_t)-1))
		cmn_err(CE_CONT, "ql%dd%dl%d: ", pinv->inv_controller, target, lun);
	else {
		ha = ql_ha_info_from_ctlr_get(ctlr_vhdl);
		cmn_err(CE_CONT, "%s/%s/%d/%d: ", ha->ha_name, EDGE_LBL_TARGET, target, lun);
	}
	va_start(ap, fmt);
	icmn_err(CE_CONT, fmt, ap);
	va_end(ap);
}

static void
DBG(int level, char *format, ...)
{
	va_list ap;
	char tempstr[200];

	if (ql_debug >= level)
	{
		va_start(ap, format);
		vsprintf(tempstr, format, ap);
		printf(tempstr);
		va_end(ap);
	}
}

static char *
ql_completion_status_msg(uint completion_status)
{
	struct {
		int  code;
		char *msg;
	} ql_comp_msg[] = {
	{ SCS_COMPLETE,			"SCS_COMPLETE" },
	{ SCS_INCOMPLETE,		"SCS_INCOMPLETE" },
	{ SCS_DMA_ERROR,		"SCS_DMA_ERROR" },
	{ SCS_TRANSPORT_ERROR,		"SCS_TRANSPORT_ERROR" },
	{ SCS_RESET_OCCURRED,		"SCS_RESET_OCCURRED" },
	{ SCS_ABORTED,			"SCS_ABORTED" },
	{ SCS_TIMEOUT,			"SCS_TIMEOUT" },
	{ SCS_DATA_OVERRUN,		"SCS_DATA_OVERRUN" },
	{ SCS_COMMAND_OVERRUN,		"SCS_COMMAND_OVERRUN" },
	{ SCS_STATUS_OVERRUN,		"SCS_STATUS_OVERRUN" },
	{ SCS_BAD_MESSAGE,		"SCS_BAD_MESSAGE" },
	{ SCS_NO_MESSAGE_OUT,		"SCS_NO_MESSAGE_OUT" },
	{ SCS_EXT_ID_FAILED,		"SCS_EXT_ID_FAILED" },
	{ SCS_IDE_MSG_FAILED,		"SCS_IDE_MSG_FAILED" },
	{ SCS_ABORT_MSG_FAILED,		"SCS_ABORT_MSG_FAILED" },
	{ SCS_REJECT_MSG_FAILED,	"SCS_REJECT_MSG_FAILED" },
	{ SCS_NOP_MSG_FAILED,		"SCS_NOP_MSG_FAILED" },
	{ SCS_PARITY_ERROR_MSG_FAILED,	"SCS_PARITY_ERROR_MSG_FAILED" },
	{ SCS_ID_MSG_FAILED,		"SCS_ID_MSG_FAILED" },
	{ SCS_UNEXPECTED_BUS_FREE,	"SCS_UNEXPECTED_BUS_FREE" },
	{ SCS_DATA_UNDERRUN,		"SCS_DATA_UNDERRUN" },
	{ -1,				"Unknown" }
	};
	int i;

	for (i = 0; ql_comp_msg[i].code != -1; ++i)
		if (completion_status == ql_comp_msg[i].code)
			return(ql_comp_msg[i].msg);
	return(ql_comp_msg[i].msg);
}

static void
ql_sensepr(vertex_hdl_t ctlr_vhdl, int unit, u_char *sptr, int length)
{
int key = *(sptr + 2);

        ql_cpmsg(ctlr_vhdl, "Sense: unit %d: ", unit);
	if (key & 0x80)
		cmn_err(CE_CONT, "FMK ");
	if (key & 0x40)
		cmn_err(CE_CONT, "EOM ");
	if (key & 0x20)
		cmn_err(CE_CONT, "ILI ");

	cmn_err(CE_CONT, "sense key 0x%x (%s) ",
		key & 0xF, scsi_key_msgtab[key & 0xF]);

	if (length > 12) {
		int asc = *(sptr + 12);

		cmn_err(CE_CONT, "asc 0x%x", asc);

		if (asc < SC_NUMADDSENSE && scsi_addit_msgtab[asc] != NULL)
			cmn_err(CE_CONT, " (%s)", scsi_addit_msgtab[asc]);
	}

	if (length > 13) cmn_err(CE_CONT, " asq 0x%x\n", *(sptr+13));

}

#ifdef DEBUG
/*
** Dump the registers for this ISP.
*/
/* ARGSUSED */
/* REFERENCED */
static void
dump_registers(pCHIP_INFO chip_info, pISP_REGS isp)
{
    u_short bus_id_low, bus_id_high, bus_config0, bus_config1;
    u_short bus_icr = 0;
    u_short bus_isr = 0;
    u_short bus_sema;
    u_short hccr = 0;
    u_short risc_pc;
    u_short mbox0, mbox1, mbox2, mbox3, mbox4, mbox5, mbox6, mbox7;

    u_short control_dma_control;
    u_short control_dma_config;
    u_short control_dma_fifo_status;
    u_short control_dma_status;
    u_short control_dma_counter;
    u_short control_dma_address_counter_1;
    u_short control_dma_address_counter_0;
    u_short control_dma_address_counter_3;
    u_short control_dma_address_counter_2;


    u_short data_dma_control;
    u_short data_dma_config;
    u_short data_dma_fifo_status;
    u_short data_dma_status;
    u_short data_dma_xfer_counter_high;
    u_short data_dma_xfer_counter_low;
    u_short data_dma_address_counter_1;
    u_short data_dma_address_counter_0;
    u_short data_dma_address_counter_3;
    u_short data_dma_address_counter_2;


    /* Get copies of the registers.  */
    mbox0 = QL_PCI_INH(&isp->mailbox0);
    mbox1 = QL_PCI_INH(&isp->mailbox1);
    mbox2 = QL_PCI_INH(&isp->mailbox2);
    mbox3 = QL_PCI_INH(&isp->mailbox3);
    mbox4 = QL_PCI_INH(&isp->mailbox4);
    mbox5 = QL_PCI_INH(&isp->mailbox5);
    mbox6 = QL_PCI_INH(&isp->mailbox6);
    mbox7 = QL_PCI_INH(&isp->mailbox7);

    bus_id_low = QL_PCI_INH(&isp->bus_id_low);
    bus_id_high = QL_PCI_INH(&isp->bus_id_high);
    bus_config0 = QL_PCI_INH(&isp->bus_config0);
    bus_config1 = QL_PCI_INH(&isp->bus_config1);
    bus_icr = QL_PCI_INH(&isp->bus_icr);
    bus_isr = QL_PCI_INH(&isp->bus_isr);
    bus_sema = QL_PCI_INH(&isp->bus_sema);
    hccr = QL_PCI_INH(&isp->hccr);
    QL_PCI_OUTH(&isp->hccr, HCCR_CMD_PAUSE);
    control_dma_control = QL_PCI_INH(&isp->control_dma_control);
    control_dma_config = QL_PCI_INH(&isp->control_dma_config);
    control_dma_fifo_status = QL_PCI_INH(&isp->control_dma_fifo_status);
    control_dma_status = QL_PCI_INH(&isp->control_dma_status);
    control_dma_counter = QL_PCI_INH(&isp->control_dma_counter);
    control_dma_address_counter_1 = QL_PCI_INH(&isp->control_dma_address_counter_1);
    control_dma_address_counter_0 = QL_PCI_INH(&isp->control_dma_address_counter_0);
    control_dma_address_counter_3 = QL_PCI_INH(&isp->control_dma_address_counter_3);
    control_dma_address_counter_2 = QL_PCI_INH(&isp->control_dma_address_counter_2);


    data_dma_config = QL_PCI_INH(&isp->data_dma_config);
    data_dma_fifo_status = QL_PCI_INH(&isp->data_dma_fifo_status);
    data_dma_status = QL_PCI_INH(&isp->data_dma_status);
    data_dma_control = QL_PCI_INH(&isp->data_dma_control);
    data_dma_xfer_counter_high = QL_PCI_INH(&isp->data_dma_xfer_counter_high);
    data_dma_xfer_counter_low = QL_PCI_INH(&isp->data_dma_xfer_counter_low);
    data_dma_address_counter_1 = QL_PCI_INH(&isp->data_dma_address_counter_1);
    data_dma_address_counter_0 = QL_PCI_INH(&isp->data_dma_address_counter_0);
    data_dma_address_counter_3 = QL_PCI_INH(&isp->data_dma_address_counter_3);
    data_dma_address_counter_2 = QL_PCI_INH(&isp->data_dma_address_counter_1);


    risc_pc = QL_PCI_INH(&isp->risc_pc);

    QL_PCI_OUTH(&isp->hccr, HCCR_CMD_RELEASE);


    printf("\ndump_regs: mbox0 = %x, mbox1 = %x\n", mbox0, mbox1);
    printf("dump_regs: mbox2 = %x, mbox3 = %x\n", mbox2, mbox3);
    printf("dump_regs: mbox4 = %x, mbox5 = %x\n", mbox4, mbox5);
    printf("dump_regs: mbox6 = %x, mbox7 = %x\n", mbox6, mbox7);
    printf("dump_regs: BUS_ID_LOW=%x, BUS_ID_HIGH=%x\n",
        bus_id_low, bus_id_high);
    printf("dump_regs: BUS_CONFIG0=%x, BUS_CONFIG1=%x\n",
        bus_config0, bus_config1);
    printf("dump_regs: BUS_ICR=%x, BUS_ISR=%x, BUS_SEMA=%x\n",
        bus_icr, bus_isr, bus_sema);
    printf("dump_regs: HCCR=%x: %x  \n", &isp->hccr, hccr);
    printf("risc PC : %x\n", risc_pc);
    printf("dump_regs: control_dma_control %x: %x\n",
        &isp->control_dma_control, control_dma_control);
    printf("dump_regs: control_dma_config %x: %x\n",
        &isp->control_dma_config, control_dma_config);
    printf("dump_regs: control_dma_fifo_status %x: %x\n",
        &isp->control_dma_fifo_status, control_dma_fifo_status);
    printf("dump_regs: control_dma_status %x: %x\n",
        &isp->control_dma_status, control_dma_status);
    printf("dump_regs: control_dma_counter %x: %x\n",
        &isp->control_dma_counter, control_dma_counter);
    printf("dump_regs: control_dma_address_counter_1 %x: %x\n",
        &isp->control_dma_address_counter_1, control_dma_address_counter_1);
    printf("dump_regs: control_dma_address_counter_0 %x: %x\n",
        &isp->control_dma_address_counter_0, control_dma_address_counter_0);
    printf("dump_regs: control_dma_address_counter_3 %x: %x\n",
        &isp->control_dma_address_counter_3, control_dma_address_counter_3);
    printf("dump_regs: control_dma_address_counter_0 %x: %x\n",
        &isp->control_dma_address_counter_2, control_dma_address_counter_2);
    printf("dump_regs: data_dma_config %x: %x\n",
        &isp->data_dma_config, data_dma_config);

    printf("dump_regs: data_dma_status %x: %x\n",
        &isp->data_dma_status, data_dma_status);
    printf("dump_regs: data_dma_fifo_status %x: %x\n",
        &isp->data_dma_fifo_status, data_dma_fifo_status);
    printf("dump_regs: data_dma_control %x: %x\n",
        &isp->data_dma_control, data_dma_control);
    printf("dump_regs: data_dma_xfer_counter_high %x: %x\n",
        &isp->data_dma_xfer_counter_high, data_dma_xfer_counter_high);
    printf("dump_regs: data_dma_xfer_counter_low %x: %x\n",
        &isp->data_dma_xfer_counter_low, data_dma_xfer_counter_low);
    printf("dump_regs: data_dma_address_counter_1 %x: %x\n",
        &isp->data_dma_address_counter_1,
        data_dma_address_counter_1);
    printf("dump_regs: data_dma_address_counter_0 %x: %x\n",
        &isp->data_dma_address_counter_0,
        data_dma_address_counter_0);
    printf("dump_regs: data_dma_address_counter_3 %x: %x\n",
        &isp->data_dma_address_counter_3,
        data_dma_address_counter_3);
    printf("dump_regs: data_dma_address_counter_2 %x: %x\n",
        &isp->data_dma_address_counter_2,
        data_dma_address_counter_2);
}

/*
** Dump FIRMWARE parameters.
*/
/* REFERENCED */
static void 
dump_parameters(vertex_hdl_t ql_ctlr_vhdl)
{
    pHA_INFO    ha  = ql_ha_info_from_ctlr_get(ql_ctlr_vhdl);
    u_short mbox_sts[8];


    if (ql_get_firmware_status(ha->chip_info, mbox_sts))
    {
	    cmn_err(CE_WARN,
		    "ql_dump_parameters - GET_FIRMWARE_STATUS Command Failed");
    } else
    {
	    printf("ql_dump_parameters - GET_FIRMWARE_STATUS\n");
	    printf("    %x, %x, %x, %x, %x, %x, %x, %x\n",
		   mbox_sts[0], mbox_sts[1], mbox_sts[2], mbox_sts[3],
		   mbox_sts[4], mbox_sts[5], mbox_sts[6], mbox_sts[7]);
    }

    if (ql_get_scsi_id(ha->chip_info, mbox_sts))
    {
	    cmn_err(CE_WARN,
		    "ql_dump_parameters - GET_INITIATOR_SCSI_ID Command Failed");
    } else
    {
	    printf("ql_dump_parameters - GET_INITIATOR_SCSI_ID\n");
	    printf("    %x, %x, %x, %x, %x, %x\n",
		   mbox_sts[0], mbox_sts[1], mbox_sts[2],
		   mbox_sts[3], mbox_sts[4], mbox_sts[5]);
    }

    if (ql_get_selection_timeout(ha->chip_info, mbox_sts))
    {
	    cmn_err(CE_WARN,
		    "ql_dump_parameters - GET_SELECTION_TIMEOUT Command Failed");
    } else
    {
	    printf("ql_dump_parameters - GET_SELECTION_TIMEOUT\n");
	    printf("    %x, %x, %x, %x, %x, %x\n",
		   mbox_sts[0], mbox_sts[1], mbox_sts[2],
		   mbox_sts[3], mbox_sts[4], mbox_sts[5]);
    }

    if (ql_get_retry_count(ha->chip_info, mbox_sts))
    {
	    cmn_err(CE_WARN,
		    "ql_dump_parameters - GET_RETRY_COUNT Command Failed");
    } else
    {
	    printf("ql_dump_parameters - GET_RETRY_COUNT\n");
	    printf("    %x, %x, %x, %x, %x, %x\n",
		   mbox_sts[0], mbox_sts[1], mbox_sts[2],
		   mbox_sts[3], mbox_sts[4], mbox_sts[5]);
    }

    if (ql_get_tag_age_limit(ha->chip_info, mbox_sts))
    {
	    cmn_err(CE_WARN,
		    "ql_dump_parameters - GET_TAG_AGE_LIMIT Command Failed");
    } else
    {
	    printf("ql_dump_parameters - GET_TAG_AGE_LIMIT\n");
	    printf("    %x, %x, %x, %x, %x, %x\n",
		   mbox_sts[0], mbox_sts[1], mbox_sts[2],
		   mbox_sts[3], mbox_sts[4], mbox_sts[5]);
    }
    
    if (ql_get_clock_rate(ha->chip_info, mbox_sts))
    {
	    cmn_err(CE_WARN,
		    "ql_dump_parameters - GET_CLOCK_RATE Command Failed");
    } else
    {
	    printf("ql_dump_parameters - GET_CLOCK_RATE\n");
	    printf("    %x, %x, %x, %x, %x, %x\n",
		   mbox_sts[0], mbox_sts[1], mbox_sts[2],
		   mbox_sts[3], mbox_sts[4], mbox_sts[5]);
    }

    if (ql_get_negation_state(ha->chip_info, mbox_sts))
    { 
	cmn_err(CE_WARN, 
		"ql_dump_parameters - GET_ACTIVE_NEGATION_STATE Command Failed"); 
    } else 
    { 
	printf("ql_dump_parameters - GET_ACTIVE_NEGATION_STATE\n"); 
	printf("    %x, %x, %x, %x, %x, %x\n", 
		mbox_sts[0], mbox_sts[1], mbox_sts[2], 
		mbox_sts[3], mbox_sts[4], mbox_sts[5]); 
    } 

    if (ql_get_asynchronous_data_setup_time(ha->chip_info, mbox_sts))
    {
	    cmn_err(CE_WARN,
		    "ql_dump_parameters - GET_ASYNC_DATA_SETUP_TIME Command Failed");
    } else
    {
	    printf("ql_dump_parameters - GET_ASYNC_DATA_SETUP_TIME\n");
	    printf("    %x, %x, %x, %x, %x, %x\n",
		   mbox_sts[0], mbox_sts[1], mbox_sts[2],
		   mbox_sts[3], mbox_sts[4], mbox_sts[5]);
    }

    if (ql_get_pci_control_parameters(ha->chip_info, mbox_sts))
    {
	    cmn_err(CE_WARN,
		    "ql_dump_parameters - GET_BUS_CONTROL_PARAMETERS Command Failed");
    } else
    {
	    printf("ql_dump_parameters - GET_BUS_CONTROL_PARAMETERS\n");
	    printf("    %x, %x, %x, %x, %x, %x\n",
		   mbox_sts[0], mbox_sts[1], mbox_sts[2],
		   mbox_sts[3], mbox_sts[4], mbox_sts[5]);
    }

   if (ql_get_scsi_parameters(ha->chip_info, mbox_sts, (u_short)0, (ushort)0))
    {
	    cmn_err(CE_WARN,
		    "ql_dump_parameters - GET_TARGET_PARAMETERS Command Failed");
    } else
    {
	    printf("ql_dump_parameters - GET_TARGET_PARAMETERS\n");
	    printf("    %x, %x, %x, %x, %x, %x\n",
		   mbox_sts[0], mbox_sts[1], mbox_sts[2],
		   mbox_sts[3], mbox_sts[4], mbox_sts[5]);
    }

   if (ql_get_device_queue_parameters(ha->chip_info, mbox_sts, (u_short)0, (u_short)0, (u_short)0))
    {
	    cmn_err(CE_WARN,
		    "ql_dump_parameters - GET_DEVICE_QUEUE_PARAMETERS Command Failed");
    } else
    {
	    printf("ql_dump_parameters - GET_DEVICE_QUEUE_PARAMETERS\n");
	    printf("    %x, %x, %x, %x, %x, %x\n",
		   mbox_sts[0], mbox_sts[1], mbox_sts[2],
		   mbox_sts[3], mbox_sts[4], mbox_sts[5]);
    }

   if (ql_get_reset_delay_parameters(ha->chip_info, mbox_sts))
    {
	    cmn_err(CE_WARN,
		    "ql_dump_parameters - GET_RESET_DELAY_PARAMETERS Command Failed");
    } else
    {
	    printf("ql_dump_parameters - GET_RESET_DELAY_PARAMETERS\n");
	    printf("    %x, %x, %x, %x, %x, %x\n",
		   mbox_sts[0], mbox_sts[1], mbox_sts[2],
		   mbox_sts[3], mbox_sts[4], mbox_sts[5]);
    }

    if (ql_get_system_parameters(ha->chip_info, mbox_sts))
    {
	    cmn_err(CE_WARN,
		    "ql_dump_parameters - GET_SYSTEM_PARAMETERS Command Failed");
    } else
    {
	    printf("ql_dump_parameters - GET_SYSTEM_PARAMETERS\n");
	    printf("    %x, %x, %x, %x, %x, %x\n",
		   mbox_sts[0], mbox_sts[1], mbox_sts[2],
		   mbox_sts[3], mbox_sts[4], mbox_sts[5]);
    }

    if (ql_get_firmware_features(ha->chip_info, mbox_sts))
    {
	    cmn_err(CE_WARN,
		    "ql_dump_parameters - GET_FIRMWARE_FEATURES Command Failed");
    } else
    {
	    printf("ql_dump_parameters - GET_FIRMWARE_FEATURES\n");
	    printf("    %x, %x, %x, %x, %x, %x\n",
		   mbox_sts[0], mbox_sts[1], mbox_sts[2],
		   mbox_sts[3], mbox_sts[4], mbox_sts[5]);
    }

    if (ql_get_data_over_recovery_mode(ha->chip_info, mbox_sts))
    {
	    cmn_err(CE_WARN,
		    "ql_dump_parameters - GET_DATA_RECOVERY_MODE Command Failed");
    } else
    {
	    printf("ql_dump_parameters - GET_DATA_RECOVERY_MODE\n");
	    printf("    %x, %x, %x, %x, %x, %x\n",
		   mbox_sts[0], mbox_sts[1], mbox_sts[2],
		   mbox_sts[3], mbox_sts[4], mbox_sts[5]);
    }

   if (ql_get_host_data(ha->chip_info, mbox_sts))
    {
	    cmn_err(CE_WARN,
		    "ql_dump_parameters - GET_HOST_DATA Command Failed");
    } else
    {
	    printf("ql_dump_parameters - GET_HOST_DATA\n");
	    printf("    %x, %x, %x, %x, %x, %x\n",
		   mbox_sts[0], mbox_sts[1], mbox_sts[2],
		   mbox_sts[3], mbox_sts[4], mbox_sts[5]);
    }

}

static void	
dump_iocb(command_entry *q_ptr)
{
	int	i;
	char	*ptr;
	
	ptr = (char *) q_ptr;
	printf("RAW data: ");
	for (i = 0; i <= 64; i++) 
		printf(" %x", *ptr++);
	printf("\n");
	
	
	printf("------------- IOCB -------------  %x\n", q_ptr);
	printf("\n");
	printf("Entry Count\t:\t%x\n", q_ptr->hdr.entry_cnt);
	printf("Entry Type\t:\t%x\n", q_ptr->hdr.entry_type);
	printf("Flags\t\t: \t%x\n", q_ptr->hdr.flags);
	printf("System Defined\t:\t%x\n", q_ptr->hdr.sys_def_1);
	printf("Handle\t\t:\t%x\n", q_ptr->handle);
	printf("Channel\t\t:\t%x\n", q_ptr->channel_id);
	printf("Target ID\t:\t%x\n", q_ptr->target_id);
	printf("LUN\t\t:\t%x\n", q_ptr->target_lun);
	printf("CDB Length\t:\t%x\n", q_ptr->cdb_length);
	printf("Control Flags\t:\t%x\n", q_ptr->control_flags);
	printf("Timeout\t\t:\t%x\n", q_ptr->time_out);
	printf("Data Seg Count\t:\t%x\n", q_ptr->segment_cnt);
	switch (q_ptr->cdb_length) {
	case SC_CLASS0_SZ:
		printf("CDB\t:\t%x %x %x %x %x %x\n", q_ptr->cdb0, q_ptr->cdb1, q_ptr->cdb2, q_ptr->cdb3,
		       q_ptr->cdb4, q_ptr->cdb5);
		break;
		
	case SC_CLASS2_SZ:
		printf("CDB\t:\t%x %x %x %x %x %x %x %x\n", q_ptr->cdb0, q_ptr->cdb1, q_ptr->cdb2, q_ptr->cdb3,
		       q_ptr->cdb4, q_ptr->cdb5, q_ptr->cdb6, q_ptr->cdb7);
		break;
	case SC_CLASS1_SZ:
		printf("CDB\t:\t%x %x %x %x %x %x %x %x %x %x %x %x\n", q_ptr->cdb0, q_ptr->cdb1, q_ptr->cdb2,
		       q_ptr->cdb3, q_ptr->cdb4, q_ptr->cdb5, q_ptr->cdb6, q_ptr->cdb7, q_ptr->cdb8, q_ptr->cdb9,
		       q_ptr->cdb10, q_ptr->cdb11);
		break;
	}
	
	for (i = 0; i < IOCB_SEGS; i++) {
#ifdef A64_BIT_OPERATION
		printf("Data Segment %d:  Address hi %08x low %08x\tCount %x\n", i+1,
		       q_ptr->dseg[i].base_high_32, q_ptr->dseg[i].base_low_32,
		       q_ptr->dseg[i].count);
#else
		printf("Data Segment %d:  Address %x\tCount %x\n", i+1,
		       q_ptr->dseg[i].base_low_32, q_ptr->dseg[i].count);
#endif
	}
}

static void	
dump_iorb(status_entry *q_ptr)
{
	int	i;
	char	*ptr;

	ptr = (char *) q_ptr;
	printf("RAW data: ");
	for (i = 0; i <= 64; i++) 
		printf(" %x", *ptr++);
	printf("\n");
	
	printf("------------- IORB -------------  %x\n", q_ptr);
	printf("Entry Count\t:\t%x\n", q_ptr->hdr.entry_cnt);
	printf("Entry Type\t:\t%x\n", q_ptr->hdr.entry_type);
	printf("Flags\t\t:\t%x\n", q_ptr->hdr.flags);
	printf("System Defined\t:\t%x\n", q_ptr->hdr.sys_def_1);
	printf("Handle\t\t:\t%x\n", q_ptr->handle);
	printf("Scsi Status\t:\t%x\n", q_ptr->scsi_status);
	printf("Comp. Status\t:\t%x\n", q_ptr->completion_status);
	printf("State Flags\t:\t%x\n", q_ptr->state_flags);
	printf("Status Flags\t:\t%x\n", q_ptr->status_flags);
	printf("Time\t\t:\t%x\n", q_ptr->time);
	printf("Sense Length\t:\t%x\n", q_ptr->req_sense_length);
	printf("Timeout\t\t:\t%x\n", q_ptr->time);
	printf("Residual\t:\t%x\n", q_ptr->residual);
	printf("Request Sense : %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x\n", q_ptr->req_sense_data[3],
	       q_ptr->req_sense_data[2], q_ptr->req_sense_data[1], q_ptr->req_sense_data[0], q_ptr->req_sense_data[7],
	       q_ptr->req_sense_data[6], q_ptr->req_sense_data[5], q_ptr->req_sense_data[4], q_ptr->req_sense_data[11],
	       q_ptr->req_sense_data[10], q_ptr->req_sense_data[9], q_ptr->req_sense_data[8], q_ptr->req_sense_data[15],
	       q_ptr->req_sense_data[14], q_ptr->req_sense_data[13], q_ptr->req_sense_data[12]);
	printf("Request Sense : %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x\n", q_ptr->req_sense_data[19],
	       q_ptr->req_sense_data[18], q_ptr->req_sense_data[17], q_ptr->req_sense_data[16], q_ptr->req_sense_data[23],
	       q_ptr->req_sense_data[22], q_ptr->req_sense_data[21], q_ptr->req_sense_data[20], q_ptr->req_sense_data[27],
	       q_ptr->req_sense_data[26], q_ptr->req_sense_data[25], q_ptr->req_sense_data[24], q_ptr->req_sense_data[31],
	       q_ptr->req_sense_data[30], q_ptr->req_sense_data[29], q_ptr->req_sense_data[28]);
}

static void	
dump_continuation(continuation_entry *q_ptr)
{
	int	i;
	char	*ptr;

	ptr = (char *) q_ptr;
	printf("RAW data: ");
	for (i = 0; i <= 64; i++) 
		printf(" %x", *ptr++);
	printf("\n");

	printf("------------- CONTINUATION -------------  %x\n", q_ptr);
	printf("Entry Count\t:\t%x\n", q_ptr->hdr.entry_cnt);
	printf("Entry Type\t:\t%x\n", q_ptr->hdr.entry_type);
	printf("Flags\t\t:\t%x\n", q_ptr->hdr.flags);
	printf("System Defined\t:\t%x\n", q_ptr->hdr.sys_def_1);

	for (i = 0; i < CONTINUATION_SEGS; i++) {
#ifdef A64_BIT_OPERATION
		printf("Data Segment %d:  Address hi %08x low %08x\tCount %x\n", i+1,
		       q_ptr->dseg[i].base_high_32, q_ptr->dseg[i].base_low_32,
		       q_ptr->dseg[i].count);
#else
		printf("Data Segment %d:  Address %x\tCount %x\n", i+1,
		       q_ptr->dseg[i].base_low_32, q_ptr->dseg[i].count);
#endif
	}
}

static void	
dump_marker(marker_entry *q_ptr)
{
	printf("\n------------- MARKER -------------  %x\n", q_ptr);
	printf("Entry Count\t:\t%x\n", q_ptr->hdr.entry_cnt);
	printf("Entry Type\t:\t%x\n", q_ptr->hdr.entry_type);
	printf("Flags\t\t:\t%x\n", q_ptr->hdr.flags);
	printf("System Defined\t:\t%x\n", q_ptr->hdr.sys_def_1);
	printf("reserved0\t:\t%x\n", q_ptr->reserved0);
	printf("Channel Id\t:\t%x\n", q_ptr->channel_id);
	printf("Target\t\t:\t%x\n", q_ptr->target_id);
	printf("Lun\t\t:\t%x\n", q_ptr->target_lun);
	printf("Modifier\t:\t%x\n", q_ptr->modifier);
}

struct cmd_dma {
	u_short	cdma_control;
	u_short cdma_config;
	u_short cdma_fifo_status;
	u_short cdma_status;
	u_short cdma_reserved;
	u_short cdma_counter;
	u_short cdma_addr_1;
	u_short cdma_addr_0;
	u_short cdma_addr_3;
	u_short cdma_addr_2;
};
typedef struct cmd_dma	cmd_dma_t;

/* REFERENCED */
static void
dump_cmd_dma(pHA_INFO ha)
{
	pISP_REGS	isp = (pISP_REGS)ha->chip_info->ha_base;
	cmd_dma_t	*cdp = (cmd_dma_t *)((__psint_t)isp + 0x20);

	QL_PCI_OUTH(&isp->hccr, HCCR_CMD_PAUSE);
	printf("cmd dma control %04x, config %04x, status %04x, counter %04x\n",
	       QL_PCI_INH(&cdp->cdma_control), QL_PCI_INH(&cdp->cdma_config),
	       QL_PCI_INH(&cdp->cdma_status), QL_PCI_INH(&cdp->cdma_counter));
	printf("        counter  3 %04x,  2 %04x,  1 %04x,  0 %04x\n",
	       QL_PCI_INH(&cdp->cdma_addr_3), QL_PCI_INH(&cdp->cdma_addr_2),
	       QL_PCI_INH(&cdp->cdma_addr_2), QL_PCI_INH(&cdp->cdma_addr_0));
	QL_PCI_OUTH(&isp->hccr, HCCR_CMD_RELEASE);
}
#endif /* DEBUG */

#ifdef BRIDGE_B_DATACORR_WAR
/* Stub for Bridge Rev B data corruption bub */
/* ARGSUSED */
int ql_bridge_rev_b_war(vertex_hdl_t pcibr_vhdl)
{
	return 0;
}
#endif



/*
 *  Qlogic mailbox command functions
 *  	ql_set_scsi_id 
 *  	ql_set_selection_timeout
 *  	ql_set_retry_count
 *  	ql_set_tag_age_limit
 *  	ql_set_clock_rate
 *  	ql_set_negation_state
 *	ql_set_asynchronous_data_setup_time
 *	ql_set_pci_control_parameters
 * 	ql_set_scsi_parameters
 *	ql_set_device_queue_parameters
 *	ql_set_reset_delay_parameters
 *	ql_set_system_parameters
 *	ql_set_firmware_features
 *	ql_set_host_data
 * 
 */


/* Set Bus Reset */
static int
ql_set_bus_reset(pCHIP_INFO chip, u_short *mbox_sts, u_short channel_id)
{

	pHA_INFO	ha=NULL;

	ha = chip->ha_info[channel_id];	

	if ( ql_mbox_cmd(chip, mbox_sts, 3, 3, 
			 MBOX_CMD_BUS_RESET,
			 ha->ql_defaults.Bus_Reset_Delay,
			 channel_id,
			 0, 0, 0, 0, 0)) {
		return (1);
	}
	return (0);
}


/* Set SCSI Id */
static int 
ql_set_scsi_id(pCHIP_INFO chip, u_short *mbox_sts)
{
	pHA_INFO	ha=NULL;
	int		i;

	/* Set Initiator SCSI ID.  */
	for (i = 0; i < chip->channel_count; ++i) {
        	ha = chip->ha_info[i];
                if (ql_mbox_cmd(chip, mbox_sts, 2, 2,
                         MBOX_CMD_SET_INITIATOR_SCSI_ID,
                         (ha->channel_id << 7) | ha->ql_defaults.Initiator_SCSI_Id,
                          0,0,0,0,0,0))
			return (1);
	}
	return(0);
}

/* Set Selection Timeout */
static int
ql_set_selection_timeout(pCHIP_INFO chip, u_short *mbox_sts)
{
	int 		i;
	pHA_INFO	ha;
	u_short		port[2];


 	for (i=0; i<chip->channel_count; i++) {
        	 ha = (pHA_INFO)chip->ha_info[i];
                 port[i] = ha->ql_defaults.Selection_Timeout;
        }

	if (ql_mbox_cmd(chip, mbox_sts, 3, 3,
                        MBOX_CMD_SET_SELECTION_TIMEOUT,
                        (u_short)port[0],
                        (u_short)port[1],
                        0,0,0,0,0))
		return(1);

	return(0);	

}

/* Set Retry Count */
static int 
ql_set_retry_count(pCHIP_INFO chip, u_short *mbox_sts)
{
	u_short mboxin, mboxout;

	if (chip->channel_count > 1)
		mboxin = mboxout = 8;
	else
		mboxin = mboxout = 3;

      	if (ql_mbox_cmd(chip, mbox_sts, mboxin, mboxout,
                        MBOX_CMD_SET_RETRY_COUNT,
                        chip->chip_parameters.Retry_Count,
                        chip->chip_parameters.Retry_Delay,
                        0,0,0,
                        chip->chip_parameters.Retry_Count,
                        chip->chip_parameters.Retry_Delay))
		return(1);

	return(0);

}
  
/* Set Tag Age Limit */
static int
ql_set_tag_age_limit(pCHIP_INFO chip, u_short *mbox_sts)
{

  	/* Set Tag Age Limits.  */
        if (ql_mbox_cmd(chip, mbox_sts, 3, 3,
                        MBOX_CMD_SET_TAG_AGE_LIMIT,
                        chip->chip_parameters.Tag_Age_Limit,
                        chip->chip_parameters.Tag_Age_Limit,
                        0,0,0,0,0))
		return (1);

	return (0);
}

/* Set Clock Rate */
static int
ql_set_clock_rate(pCHIP_INFO chip, u_short *mbox_sts)
{

     if (ql_mbox_cmd(chip, mbox_sts, 2, 2,
                                MBOX_CMD_SET_CLOCK_RATE,
                                chip->clock_rate,
                                0,0,0,0,0,0))
		return(1);

     return (0);
}

/* Set Negation State */
static int
ql_set_negation_state(pCHIP_INFO chip, u_short *mbox_sts)
{
	pHA_INFO	ha=NULL;
	u_short 	port[2]={0};
	int		i;

        for (i = 0; i < chip->channel_count; ++i) {
        	ha  = chip->ha_info[i];
                port[i] = (ha->ql_defaults.REQ_ACK_Active_Negation << 5) | (ha->ql_defaults.DATA_Line_Active_Negation << 4);
         }
         if (ql_mbox_cmd(chip, mbox_sts, 3, 3,
                 MBOX_CMD_SET_ACTIVE_NEGATION_STATE,
                 port[0],
                 port[1],
                 0,0,0,0,0))
		return(1);

	return(0);
}

/* Set Asynchronous Data Setup Time */
static int
ql_set_asynchronous_data_setup_time(pCHIP_INFO chip, u_short *mbox_sts)
{
	pHA_INFO	ha=NULL;
	u_short		port[2]={0};
	int		i;

        /* Set Asynchronous Data Setup Time.  */

        for (i = 0; i < chip->channel_count; ++i) {
             ha = chip->ha_info[i];
             port[i] = ha->ql_defaults.ASync_Data_Setup_Time;
         }

         if (ql_mbox_cmd(chip, mbox_sts, 3, 3,
                 MBOX_CMD_SET_ASYNC_DATA_SETUP_TIME,
                 port[0],
                 port[1],
                 0,0,0,0,0))
		return (1);

	 return (0);
}


/* Set Bus Control Parameters.  */
static int
ql_set_pci_control_parameters(pCHIP_INFO chip, u_short *mbox_sts)
{
	u_short		port[2]={0};

        if (chip->channel_count == 1) {
                port[0] = chip->chip_parameters.Data_DMA_Burst_Enable << 1;
                port[1] = chip->chip_parameters.Command_DMA_Burst_Enable << 1;
        }
        else {
                port[0] = port[1] = chip->chip_parameters.Command_DMA_Burst_Enable << 1;
        }
        if (ql_mbox_cmd(chip, mbox_sts, 3, 3,
                        MBOX_CMD_SET_BUS_CONTROL_PARAMETERS,
                        port[0],
                        port[1],
                        0,0,0,0,0))
		return (1);

	return (0);
}

/* Set SCSI Parameters */
static int
ql_set_scsi_parameters(pCHIP_INFO chip, u_short *mbox_sts, u_short channel_id, u_short target_id)
{
	pHA_INFO	ha=NULL;

	ha = chip->ha_info[channel_id];

#if 0
	ql_cpmsg(ha->ctlr_vhdl, "Sync_Offset 0x%x Sync_Period 0x%x\n",
		ha->ql_defaults.Id[target_id].Sync_Offset,
		ha->ql_defaults.Id[target_id].Sync_Period);
#endif
	if (ql_mbox_cmd(chip, mbox_sts, 4, 4,
               MBOX_CMD_SET_TARGET_PARAMETERS,
               (u_short)((channel_id << 15) | (target_id << 8)),
               (u_short)(ha->ql_defaults.Id[target_id].Capability.byte << 8),
               (u_short)((ha->ql_defaults.Id[target_id].Sync_Offset << 8) |
               (ha->ql_defaults.Id[target_id].Sync_Period)),
               0,0,0,0))
		return (1);

#if 0
	ql_cpmsg(ha->ctlr_vhdl, "mbox0 = 0x%x mbox1 = 0x%x mbox2 = 0x%x, mbox3 = 0x%x\n",
		mbox_sts[0], mbox_sts[1], mbox_sts[2], mbox_sts[3]);
#endif
	return (0);
}

/* Set Device Queue Parameters */
static int
ql_set_device_queue_parameters(pCHIP_INFO chip, u_short *mbox_sts, u_short channel_id, u_short target_id, u_short lun_id)
{
	pHA_INFO	ha=NULL;


	ha = chip->ha_info[channel_id];

	if (ql_mbox_cmd(chip, mbox_sts, 4, 4,
              MBOX_CMD_SET_DEVICE_QUEUE_PARAMETERS,
              (channel_id << 15) | (target_id << 8) | lun_id,
              ha->ql_defaults.Max_Queue_Depth,
              ha->ql_defaults.Id[target_id].Throttle,
             0,0,0,0))
		return (1);

	return (0);

}

/* Set Reset Delay Parameters */
static int
ql_set_reset_delay_parameters(pCHIP_INFO chip, u_short *mbox_sts)
{

	pHA_INFO	ha=NULL;
	u_short		port[2]={0};
	int		i;

        /* Set Reset Delay Time.  */

        for (i = 0; i < chip->channel_count; ++i) {
             ha = chip->ha_info[i];
             port[i] = ha->ql_defaults.Bus_Reset_Delay;
         }

         if (ql_mbox_cmd(chip, mbox_sts, 3, 3,
                 MBOX_CMD_SET_RESET_DELAY_PARAMETERS,
                 port[0],
                 port[1],
                 0,0,0,0,0))
		return (1);

	 return (0);



}

/* Set System Parameters */
static int
ql_set_system_parameters(pCHIP_INFO chip, u_short *mbox_sts)
{


         if (ql_mbox_cmd(chip, mbox_sts, 2, 2,
                 MBOX_CMD_SET_SYSTEM_PARAMETERS,
		 chip->chip_parameters.Fast_Memory_Enable,
                 0,0,0,0,0,0))
		return (1);

	 return (0);

}

/* Set Firmware Features */
static int
ql_set_firmware_features(pCHIP_INFO chip, u_short *mbox_sts)
{
	if (ql_mbox_cmd(chip, mbox_sts, 2, 2,
		MBOX_CMD_SET_FIRMWARE_FEATURES,
		chip->chip_parameters.Firmware_Features,
		0,0,0,0,0,0))
		return (1);
	
	return (0);
}


/* not needed right now */
#if  0

/* Set Host Data */
static int
ql_set_host_data(pCHIP_INFO chip, u_short *mbox_sts)
{

	if (ql_mbox_cmd(chip, mbox_sts, 4, 4,
		MBOX_CMD_SET_HOST_DATA,
		chip->host_data[0],
		chip->host_data[1],
		chip->host_data[2],
		0,0,0,0))
		return(1);

	return(0);

}
#endif

/* Set Data Overrun Recovery Mode */
static int
ql_set_data_over_recovery_mode(pCHIP_INFO chip, u_short *mbox_sts)
{

        if (ql_mbox_cmd(chip, mbox_sts, 2, 2,
                MBOX_CMD_SET_DATA_RECOVERY_MODE,
                DATA_OVERRUN_MODE_2,
                0,0,0,0,0,0)) 
                return (1);
        
        return (0);
}

static int
ql_set_abort_device(pCHIP_INFO chip, u_short *mbox_sts, u_short channel_id, u_short target_id, u_short lun_id)
{

        if (ql_mbox_cmd(chip, mbox_sts, 2, 2,
                MBOX_CMD_ABORT_DEVICE,
               (u_short)( (channel_id<<15) | (target_id << 8) | (u_short) (lun_id) ),
               0, 0, 0, 0, 0, 0))
		return (1);
	return (0);

}

#ifdef DEBUG
/* Get Firmware Status */
static int
ql_get_firmware_status(pCHIP_INFO chip, u_short *mbox_sts)
{
	if (ql_mbox_cmd(chip, mbox_sts, 1, 3,
		MBOX_CMD_GET_FIRMWARE_STATUS,
		0, 0, 0, 0, 0, 0, 0))
		return(1);
	return(0);
}


/* Get SCSI ID */
static int
ql_get_scsi_id(pCHIP_INFO chip, u_short *mbox_sts)
{

	if (ql_mbox_cmd(chip, mbox_sts, 1, 3,
                    MBOX_CMD_GET_INITIATOR_SCSI_ID,
	            0, 0, 0, 0, 0, 0, 0))
		return (1);

	return (0);

}

/* Get Selection Timeout */
static int			
ql_get_selection_timeout(pCHIP_INFO chip, u_short *mbox_sts)
{

    if (ql_mbox_cmd(chip, mbox_sts, 1, 3,
                    MBOX_CMD_GET_SELECTION_TIMEOUT,
                    0, 0, 0, 0, 0, 0, 0))
	return (1);

    return (0);

}

/* Get Retry Count */
static int
ql_get_retry_count(pCHIP_INFO chip, u_short *mbox_sts)
{

 	if (ql_mbox_cmd(chip, mbox_sts, 1, 3,
                    MBOX_CMD_GET_RETRY_COUNT,
                    0, 0, 0, 0, 0, 0, 0))
		return (1);

	return (0);
	
}

/* Get Tag Age Limit */
static int
ql_get_tag_age_limit(pCHIP_INFO chip, u_short *mbox_sts)
{
	if (ql_mbox_cmd(chip, mbox_sts, 1, 3,
                    MBOX_CMD_GET_TAG_AGE_LIMIT,
                    0, 0, 0, 0, 0, 0, 0))
		return (1);

	return (0);


}

/* Get Clock Rate */
static int			
ql_get_clock_rate(pCHIP_INFO chip, u_short *mbox_sts)
{
	if (ql_mbox_cmd(chip, mbox_sts, 1, 2,
                    MBOX_CMD_GET_CLOCK_RATE,
                    0, 0, 0, 0, 0, 0, 0))
		return (1);
	return (0);

}

/* Get Negation State */
static int			
ql_get_negation_state(pCHIP_INFO chip, u_short *mbox_sts)
{

	if (ql_mbox_cmd(chip, mbox_sts, 1, 3,
                    MBOX_CMD_GET_ACTIVE_NEGATION_STATE,
                    0, 0, 0, 0, 0, 0, 0))
		return (1);

	return (0);

}

/* Get Asynchronous Data Setup Time */
static int			
ql_get_asynchronous_data_setup_time(pCHIP_INFO chip, u_short *mbox_sts)
{
	   if (ql_mbox_cmd(chip, mbox_sts, 1, 3,
                    MBOX_CMD_GET_ASYNC_DATA_SETUP_TIME,
                    0, 0, 0, 0, 0, 0, 0))
		return (1);

	   return (0);
}

/* Get PCI Control Parameters */
static int
ql_get_pci_control_parameters(pCHIP_INFO chip, u_short *mbox_sts)
{
   	if (ql_mbox_cmd(chip, mbox_sts, 1, 3,
                    MBOX_CMD_GET_BUS_CONTROL_PARAMETERS,
                    0, 0, 0, 0, 0, 0, 0))
		return (1);

	return (0);


}
#endif

/* Get SCSI Parameters */
static int 			
ql_get_scsi_parameters(pCHIP_INFO chip, u_short *mbox_sts, u_short channel_id, u_short target_id)
{

 	if (ql_mbox_cmd(chip, mbox_sts, 2, 4,
                    MBOX_CMD_GET_TARGET_PARAMETERS,
                    channel_id<<15|target_id<<8,                  /* SCSI id */
                    0, 0, 0, 0, 0, 0))
		return (1);

	return (0);

}

#ifdef DEBUG

/* Get Device Queue Parameters */ 
static int			
ql_get_device_queue_parameters(pCHIP_INFO chip, u_short *mbox_sts, u_short channel_id, u_short target_id, u_short lun_id)
{

  	if (ql_mbox_cmd(chip, mbox_sts, 2, 6,
                    MBOX_CMD_GET_DEVICE_QUEUE_PARAMETERS,
                    channel_id<<15|target_id<<8|lun_id,        /* PORT id | SCSI id | LUN id*/
                    0, 0, 0, 0, 0, 0))
		return (1);

	return (0);
}

/* Get Reset Delay Parameters */
static int			
ql_get_reset_delay_parameters(pCHIP_INFO chip, u_short *mbox_sts)
{
   	if (ql_mbox_cmd(chip, mbox_sts, 1, 3,
                    MBOX_CMD_GET_RESET_DELAY_PARAMETERS,
                    0,
                    0, 0, 0, 0, 0, 0))
		return (1);

	return (0);
}

/* Get System Parameters */
static int			
ql_get_system_parameters(pCHIP_INFO chip, u_short *mbox_sts)
{
 	if (ql_mbox_cmd(chip, mbox_sts, 1, 2,
                    MBOX_CMD_GET_SYSTEM_PARAMETERS,
                    0,
                    0, 0, 0, 0, 0, 0))
		return (1);
	return (0);

}

/* Get Firmware Features */
static int			
ql_get_firmware_features(pCHIP_INFO chip, u_short *mbox_sts)
{
   	if (ql_mbox_cmd(chip, mbox_sts, 1, 2,
                    MBOX_CMD_GET_FIRMWARE_FEATURES,
                    0,
                    0, 0, 0, 0, 0, 0))
		return (1);
	
	return (0);
}

/* Get Host Data */
static int			
ql_get_host_data(pCHIP_INFO chip, u_short *mbox_sts)
{

 	if (ql_mbox_cmd(chip, mbox_sts, 1, 4,
                    MBOX_CMD_GET_HOST_DATA,
                    0,
                    0, 0, 0, 0, 0, 0))
		return (1);

	return (0);
}

/* Get Data Over Recovery Mode */
static int			
ql_get_data_over_recovery_mode(pCHIP_INFO chip, u_short *mbox_sts)
{

    	if (ql_mbox_cmd(chip, mbox_sts, 1, 2,
                    MBOX_CMD_GET_DATA_RECOVERY_MODE,
                    0,
                    0, 0, 0, 0, 0, 0))
		return (1);

	return (0);

}
#endif
