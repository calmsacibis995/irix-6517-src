/*
 * Header file for Adaptec 7800 family of SCSI controllers
 *
 * $Id: adp78.h,v 1.14 1999/08/19 03:28:47 gwh Exp $
 */

#define ADP78_MAXTARG	16
#define ADP78_MAXLUN	8

#define INQUIRY_CMD 0x12

/****************************************************************
%%% Scatter Gather Element Definition %%%
*****************************************************************/
/*
 * The scatter gather element list must be in contiguous memory.
 * There is no limit on how many elements there are.
 * The data_len field of the last element must have SG_LAST_ELEMENT.
 */

#define MIN_LRGSG_NUMSG		1024	/* When alloc'ing a large sg, get one
					 * with at least this many entries.
					 * 1024 will allow us to issue a 4MB cmd
					 */

#define SG_LAST_ELEMENT		0x80000000

typedef struct sg_element {
	void		*data_ptr;	 /* 32 bit data pointer    */
	unsigned int	data_len;	 /* 32 bit data length	   */
} SG_ELEMENT;

#define SG_ELEMENT_SIZE		(sizeof(SG_ELEMENT))


/*
 *****************************************************************
 *
 * Statistics and performance monitoring stuff.
 *
 *****************************************************************
 */
#define SOP_ADP_GETSTATS	_SCIO(80)
#define SOP_ADP_RESETBUS	_SCIO(81)
#define SOP_ADP_PERFTEST	_SCIO(90)
#define SOP_ADP_CMDLOG_START	_SCIO(91)
#define SOP_ADP_CMDLOG_STOP	_SCIO(92)
#define SOP_ADP_7895TEST	_SCIO(93)

typedef struct adp_stats {
	uint		intrs;		/* intrs received */
	uint		intrs_spur;	/* spurious interrupts received */
	uint		cmds;		/* cmds completed */
	uint		blocks;		/* blocks either read or write */
} adp_stats_t;

typedef struct cmdlog_entry{
	char		cmd;
	char		targ;
	unsigned short	blocks;
	int		addr;
	int		sec;
	int		usec;
}cmdlog_entry_t;



/****************************************************************
 *
 * The adp78_ha_t is a "handle" of SCSI tasks.  It can either be busy
 * or free.  When it is free, it is sitting on the adp78_dev_t's freeha
 * list.  When it is busy, it is sitting on the adp78_dev_t's busyha
 * list.  It is always pointing to a s/g list and a SCB (sp_struct).
 * When it is busy, it will also point to the scsi_request_t that it
 * handling.
 *
 ****************************************************************/
typedef struct adp78_ha {
	u_char		ha_id; /* host adapter id to which this ha belongs */
	u_char		ha_qflag;	/* queue related flags;
					 * used by queuing functions only */
	u_char		ha_pad[2];

	u_int		ha_flags;	/* flags for this struct */
	scsi_request_t  *ha_req;	/* pointer to active scsi request */
	struct sp 	*ha_scb;	/* pointer to scb */
	char		*ha_ebuf;	/* buffer for swizzled cdb */
	SG_ELEMENT 	*ha_sg;		/* pointer to s/g list */
	int		ha_numsg;	/* number of s/g elements */
	SG_ELEMENT	*ha_lrgsg;
	int		ha_lrgsg_numsg;
	time_t		ha_start;	/* cmd start time */
	struct adp78_ha	*ha_forw;
	struct adp78_ha	*ha_back;
} adp78_ha_t;
 
#define HAQ_FREE		0x10	/* ha is on free list */
#define HAQ_BUSY		0x20	/* ha is on busy list */
#define HAQ_DONE		0x40	/* ha is on done list */

#define HAF_TIMED_OUT		0x01	/* ha has timed out   */
#define HAF_STUCK		0x02	/* ha may be "stuck"  */
#define HAF_ABORT		0x04	/* ha is abort msgo   */


/****************************************************************
 *
 * Adaptec 7880 Host adapter interface record.
 * Holds lists of active scsi requests/scb's, waiting scsi requests, and
 * free scb's as well as state information.
 *
 * A thread can hold only one of the following locks at a time:
 * - ad_him_lk
 * - ad_haq_lk
 * - ad_req_lk
 ****************************************************************/
typedef struct adp78_adap {
	u_char		ad_ctlrid;	/* adapter/controller number */
	u_char		ad_scsiid;	/* scsi id of this controller */
	u_char		ad_rev;		/* PCI revision number		*/
	u_char		ad_pcibusnum;	/* 0 for main bus	     */
	u_char		ad_pcidevnum;	/* PCI device number 	     */
	vertex_hdl_t	ad_vertex;	/* vertex for this controller */

        mutex_t		ad_him_lk;	/* lock for entry into HIM	*/
	struct cfp	*ad_himcfg;	/* HIM configuration structure	*/
	u_int		ad_strayintr;

	lock_t		*ad_req_lk;	/* lock for the scsi request q	*/
	int		ad_req_ospl;
	scsi_request_t  *ad_waithead;	/* FIFO list of scsi requests */
	scsi_request_t  *ad_waittail;

	lock_t		*ad_haq_lk;	/* lock for the 3 ha queues	*/
	int		ad_haq_ospl;
	adp78_ha_t	*ad_freeha;
	adp78_ha_t	*ad_busyha;
	adp78_ha_t	*ad_doneha;

	lock_t		*ad_adap_lk;	/* lock for the rest of the adap fields	*/
	int		ad_adap_ospl;
        mutex_t         ad_probe_lk;    /* lock for probing SCSI bus */   
        mutex_t         ad_abort_lk;    /* lock for sending abort */
	u_int 		ad_flags;
	int		ad_busymap;	/* bit map of targs executing non-tagged cmds */
	int		ad_memwaitmap;	/* bit map of targs stuck waiting for mem */
	int		ad_tagholdmap;	/* bit map of targs to hold off sending tagged
					 * cmds to because a non-tagged cmd is waiting */
	u_char		ad_tagcmds[ADP78_MAXTARG];
					/* # of tagged cmds executing on targ. */
	uint		ad_resettime;	/* lbolt of last reset */
	char		*ad_baseaddr;	/* Memory mapped address */
	char		*ad_cfgaddr;	/* PCI config address */
	int		ad_numha;	/* num of ha/scbs		*/
	int		ad_normsg_numsg;/* num entries in normal sg	*/
	SG_ELEMENT	*ad_lrgsg;
	int		ad_lrgsg_numsg;	/* how many SG entries		*/
	int		ad_lrgsg_age;	/* how long since last use	*/
	u_char		ad_alloc[ADP78_MAXTARG][ADP78_MAXLUN];
	void            (*adp_sense_callback[ADP78_MAXTARG][ADP78_MAXLUN])();/* callback */
     union {
       u_char     all_params;
       u_char     per_off_wid[3];
       struct {
	 u_char   allow_sync:1;
	 u_char   allow_wide:1;
	 u_char   force_sync:1;
	 u_char   force_wide:1;
	 u_char   is_cdrom:1;
	 u_char   do_abort:1;
	 u_char   first_inq_done:1;
	 u_char	  period;
	 u_char	  offset;
	 u_char	  width;
       } params;
     } ad_negotiate[ADP78_MAXTARG];
        mutex_t         ad_targ_open_lk[ADP78_MAXTARG];
	scsi_target_info_t ad_info[ADP78_MAXTARG][ADP78_MAXLUN];
	adp_stats_t	*ad_stats;
	cmdlog_entry_t	*ad_cmdbuf;
	uint		ad_cmdbufcur;
	uint		ad_cmdbuflen;
} adp78_adap_t;


/*
 * Flags for ad_flags
 */
#define ADF_OPTIONAL_PCI	0x01	/* controller on extern PCI card */
#define ADF_RESET_IN_PROG	0x02	/* scsi reset in progress	*/
#define ADF_RESET		0x04	/* reset occured, look for AEN_ACK*/
#define ADF_RESET_PART_1	0x08
#define ADF_RESET_PART_2	0x10

/*
 * Flags for ad_syncflags
 */
#define AD_CANTSYNC	1
#define AD_SYNC_ON	2


/*
 * Miscellaneous stuff.
 */
#define ADP78_NUMADAPTERS	7	/* adapter 0 - 6 allowed */

#define ADP_SCB_DEV_WEIGHT	 1	/* non-hard disk device gets 1 SCB */
#define ADP_SCB_HDISK_WEIGHT	 4	/* hard disk device gets 4 SCB	*/
#define ADP_MINSCBS		 1	/* min num of scbs per scsi bus	*/


/*
 * Bit fields for dealing with adp_device_options from
 * master.d/adp78
 */

#define ADP_ALLOW_DISCONNECT	0x02
#define ADP_ALLOW_SYNC  	0x01
#define ADP_ALLOW_WIDE          0x80
#define ADP_FORCE_SYNC          0x04
#define ADP_FORCE_WIDE          0x08
#define ADP_SYNC_RATE           0x70


/*
 * Internal timer for timing out cmds, and maybe other stuff.
 * 2 seconds seems frequent enough.
 * T1 is how long to assert RST on the SCSI bus for a reset.
 * T2 is how long to wait after RST deassertion before sending cmds.
 */
#define ADP_TIMER_VAL		2 * HZ
#define ADP_RESET_T1		7


/*
 * Defines for standalone.
 * Number of us to wait between polls of the intstat register
 */
#define POLL_WAIT		1000
