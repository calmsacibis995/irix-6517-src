#define SCIP_MAXTARG	16
#define SCIP_MAXLUN	8
#define SCIP_MAXQUEUE	128
#define SCIP_FLOATQUEUE	(SCIP_MAXQUEUE - (SCIP_MAXTARG - 1))
#define SCIP_SCBSIZE	128
#define SCIP_SCBNUM	SCIP_MAXQUEUE
#define SCIP_MCBNUM	4
#define SCIP_MEMSIZE	(128 * 1024)
#define SCIPSRAM_INITBLOCK 0x2C000
#define SCIP_SRAMSIZE	0x20000


struct sciprequest
{
	toid_t			 timeout_id;
	struct dmamap		*dmamap;
	struct scsi_request	*req;
	struct sciprequest	*next;
};


struct scip_scb { /* base 0x0 align 0x80 */
	/* Fields filled out by CPU */
	char			 opcode;
	char			 target;
	char			 lun;
	char			 tag_type;
	char			 xferfactor;	/* min xfer factor to use */
	char			 offset;	/* max sync offset to use */
	short			 flags;		/* SCIP sets this also */
	char			 scsi_cmd[16];
	char			 cmd_length;
	char			 dma_xlatehi;
	short			 dma_offset;
	int			 dma_xlatelo;
	int			 dma_length;
	char			 sense_length;
	char			 sense_xlatehi;
	short			 sense_offset;
	int			 sense_xlatelo;

	/* reserved for CPU use */
	char			 reserve[20 - sizeof(void *)];
	struct sciprequest	*scipreq;

	/* fields filled out by SCIP */
	char			 scsi_status;
	char			 scip_status;
	char			 sense_gotten;
	char			 odd_byte;
	int			 dma_resid;
	char			 error_data[3];
	char			 odd_sense_byte;
	int			 reserve2[2];

	/* reserved for SCIP use */
	int			 scip_reserved[10];
	uint			 op_id;
};

/*
 * Constants for SCB.flags / struct scip_scb.flags
 *
 * High bits are set by scip, low bits are set by driver
 */
#define SCB_DIR			0x0001
#define SCB_DO_SYNC             0x0002
#define SCB_DO_WIDE             0x0004
#define SCB_NO_DISC_ALLOW       0x0008
#define SCB_SENSE_ACK		0x0010

#define SCB_ODD_SENSE_BYTE	0x0400
#define SCB_ODD_BYTE_FLAG	0x4000
#define SCB_COMPLETE		0x8000

/*
 * Constants for struct scip_scb.scip_status
 *
 * ILLEGAL_STAT is when the status byte that was returned does not match any
 * defined in the SCSI-2 spec.
 *
 * CHK_COND will have sense data returned with the length of the data in
 * the "sense_gotten" field in the SCB.
 */
#define SCIP_STAT_OK		0x0
#define SCIP_STAT_TIMEOUT	0x4	/* selection timeout */
#define SCIP_STAT_CCABORT	0x5	/* aborted due to contingent allegiance */

/* causes error SCB to be generated */
#define FIRM_UNEXP_RESET	0x10	/* Someone reset the SCSI bus            */
#define FIRM_ERR_IDLE		0x11	/* firmware should have had something to do */
#define FIRM_ERR_SRCID		0x12	/* scb not valid -- invalid tag (device err),
					   or fw error if command is untagged    */
#define FIRM_ERR_RECON_Q	0x13	/* invalid reselection -- no command active
					   for the target			 */

/* causes SCB to be sent back with bad status */
#define WD_ERR_UEI		0x20	/* got a UEI from the WD                 */
#define WD_ERR_UPHAS		0x21	/* unexpected phase on the SCSI bus      */
#define WD_ERR_PARE		0x22	/* parity error on the SCSI bus          */
#define WD_ERR_STOPU		0x23	/* got STOPU (not UPHASE or PARE)        */
#define WD_ERR_VBSYI		0x24	/* got VBUSY status, should never get this  */
#define WD_ERR_MISC_MSG		0x25	/* unrecognizable message in on SCSI bus */
#define WD_ERR_UNEXP_REJECT	0x26	/* Unexpected reject message */
#define WD_ERR_DISC_Q		0x27	/* SCB to remove from DISCONNECT Q not there*/
#define WD_ERR_BAD_INT		0x28	/* stop or int from WD is invalid        */
#define WD_ERR_RS_Q		0x29	/* SCB to rm from Req sense Q not there */
#define WD_ERR_LAST_FLAG	0x2a	/* WD error, last flag not set */


struct scip_resetcb { /* base 0x2c000 align 0x800 */
	char	 opcode;
	char	 target;
	short	 flags;
	int	 lo_iq_remove_base_hi;
	int	 lo_iq_remove_base_lo;
	int	 lo_iq_add_base_hi;
	int	 lo_iq_add_base_lo;
	int	 lo_cq_add_base_hi;
	int	 lo_cq_add_base_lo;
	int	 lo_iq_mask;
	int	 med_iq_remove_base_hi;
	int	 med_iq_remove_base_lo;
	int	 med_iq_add_base_hi;
	int	 med_iq_add_base_lo;
	int	 med_cq_add_base_hi;
	int	 med_cq_add_base_lo;
	int	 med_iq_mask;
	int	 hi_req_add_base_hi;
	int	 hi_req_add_base_lo;
	int	 error_scb_addr_hi;
	int	 error_scb_addr_lo;
	int	 scip_host_id;
	int	 complete_intr;
	int	 finish_ptr_hi;
	int	 finish_ptr_lo;
	int	 reserved[8];
	int	 op_id;
};


struct scipfinishinfo
{
	uint			 high;
	uint			 med;
	uint			 low;
	uint			 error;
};


struct scipluninfo
{
	u_char			 number;
	u_char			 l_ca_req:1,	/* contingent allegiance req */
				 sense_ack:1,	/* ack to FW needed after CA */
				 exclusive:1;	/* exclusive access */
	u_short			 refcount;
	struct scsi_request	*waithead;	/* q-head of waiting commands */
	struct scsi_request	*waittail;	/* q-tail of waiting commands */
	struct sciprequest	*active;	/* pointer to active list */
	struct scipluninfo	*nextwait;	/* next lun waiting to issue cmd */
	struct dmamap		*sensemap;	/* for issuing request sense */
	u_char			 sense_xlatehi;
	u_short			 sense_offset;
	u_int			 sense_xlatelo;
	void			(*sense_callback)();
	struct scsi_target_info	 tinfo;		/* info about LUN */
};

struct scipunitinfo
{
	u_char			 number;
	u_char			 renegotiate:1,	/* renegotiate xfer params */
				 scipreqbusy:1,	/* unit's scipreq busy */
				 wide_supported:1, /* unit supports wide SCSI */
				 present:1;	/* unit responds to inquiry */
	u_char			 min_syncperiod;
	u_char			 max_syncoffset;
	struct scipluninfo	*lun[SCIP_MAXLUN];
	struct sciprequest	 unitreq;	/* unit's scipreq */
	mutex_t			 opensema;	/* single thread open and alloc */
};

struct scipctlrinfo
{
	ushort			 number;	/* internal ctlr number */
	ushort			 quiesce:1,	/* don't issue commands */
				 dead:1,	/* SCIP not responding */
				 reset:1,	/* reset in progress */
				 intr:1,	/* current in intr routine */
				 restartpend:1,	/* restart timeout pending */
				 diff:1,	/* set if differential */
				 external:1,	/* set if bus goes external to chassis */
				 chan:2;	/* channel number on SCIP */
	ushort			 cmdcount;	/* # outstanding commands */
	ushort			 reset_num;	/* number of resets done */
	uint			 scb_iqadd;	/* IQ add pointer */
	uint			 scb_hicnt;	/* count of completed hi-pri */
	uint			 scb_errcnt;	/* count of errors seen */
	toid_t			 reset_timeout;	/* timeout id for reset timeout */
	struct scip_scb		*scb_cqremove;	/* CQ remove pointer */
	struct scip_scb		*scb;		/* low pri request queue */
	struct scip_scb		*mcb;		/* medium pri request queue */
	struct scip_scb		*hcb;
	struct scip_scb		*ecb;
	struct scipfinishinfo	*finish;
	struct scipluninfo	*waithead;	/* luns waiting to issue cmds */
	struct scipluninfo	*waittail;	/* last lun in line */
	struct sciprequest	*request;	/* queue of avail requests */
	struct scsi_request	*intrhead;	/* queue used during intr */
	struct scsi_request	*intrtail;	/* queue used during intr */
	volatile u_char		*chip_addr;	/* KV addr of scip registers */
	char			*page;		/* extra page for DMA */
	mutex_t			 lock;
	mutex_t			 intrlock;
	mutex_t			 scanlock;
	struct scipunitinfo	*unit[SCIP_MAXTARG];
};

int scipfw_download(volatile u_char *);
int scip_earlyinit(int);
int scip_startup(volatile u_char *);
void scipintr(eframe_t *, __psunsigned_t);
