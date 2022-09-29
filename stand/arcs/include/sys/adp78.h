/*
 * Header file for Adaptec 7800 family of SCSI controllers
 *
 * $Id: adp78.h,v 1.3 1997/07/24 00:39:26 philw Exp $
 */


/****************************************************************
%%% Scatter Gather Element Definition %%%
*****************************************************************/
/*
 * The scatter gather element list must be in contiguous memory.
 * There is no limit on how many elements there are.
 * The data_len field of the last element must have SG_LAST_ELEMENT.
 */
#define NUMSG_SMALL		16	/* typical number of s/g needed */
#define NUMSG_BIG		512
#define SG_LAST_ELEMENT		0x80000000

typedef struct sg_element {
	void		*data_ptr;	 /* 32 bit data pointer    */
	unsigned int	data_len;	 /* 32 bit data length	   */
} SG_ELEMENT;

/****************************************************************
 *
 * Adaptec 7880 Host adapter interface record.
 * Holds lists of active scsi requests/scb's, waiting scsi requests, and
 * free scb's as well as state information.
 *
 ****************************************************************/

typedef struct adp78_adap {
	u_char		ad_ctlrid;	/* adapter/controller number */
	u_char		ad_scsiid;	/* scsi id of this controller */
	u_char		ad_rev;		/* rev of this controller    */
	u_char		ad_pcibusnum;	/* 0 for main bus	     */
	u_char		ad_pcidevnum;	/* PCI device number 	     */
	u_int 		ad_flags;
	SG_ELEMENT	*ad_sg;		/* s/g list, only 1 per adapter */
	char		*ad_ebuf;	/* for swizzled cdb */
	struct cfp	*ad_himcfg;	/* HIM configuration structure */
	char		*ad_baseaddr;	/* Memory mapped address */
	volatile char	*ad_cfgaddr;	/* config. address/token */
	int		ad_timeoutval;	/* timeout value for current command */
	scsisubchan_t	*ad_currcmd;	/* ptr to current cmd		*/
	scsisubchan_t	*ad_subchan[SC_MAXTARG][SC_MAXLUN];
	sp_struct	*ad_scb[SC_MAXTARG][SC_MAXLUN];
	char		*ad_inq[SC_MAXTARG][SC_MAXLUN];
	u_char		ad_syncflags[SC_MAXTARG];
} adp78_adap_t;

#define ADP78_NUMADAPTERS	SC_MAXADAP	/* only adaptecs recognized in standalone */

/*
 * Flags for ad_flags
 */
#define ADF_OPTIONAL_PCI	1	/* controller is on external PCI card */


/*
 * Flags for ad_syncflags
 */
#define AD_CANTSYNC	1
#define AD_SYNC_ON	2


/*
 * Bit fields for dealing with adp_scsi_opt.
 * Bits 7-4 and bit 0 from adp_device_options go directly into Cf_ScsiOptions
 * in the config structure of for the HIM.  I use bits 1 and 2 to make
 * configuration in master.d/adp78 a bit easier.
 */

#define HIM_OPTIONS		0xf1
#define ADP_ALLOW_DISCONNECT	0x02

/*
 * Defines for standalone.
 * Number of us to wait between polls of the intstat register
 */
#define POLL_WAIT		1000
#define US_IN_SEC		1000000
#define ADP_MIN_TIMEOUT		1
