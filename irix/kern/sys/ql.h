#ifdef _KERNEL
#include <sys/ktypes.h>
#include <sys/scsi.h>

/**********************************************************
**
**      Debug definitions
*/

#ifdef PIO_TRACE
#define	 QL_LOG_CNT	512
#endif

/**********************************************************
**
**      Macro Definitions
*/

#ifdef _STANDALONE
#define CRITICAL_BEGIN      {int  s = splhi();
#define CRITICAL_EXIT       splx(s)
#define CRITICAL_END        splx(s);}
#endif

/**********************************************************
**
**      General Information
*/

#ifdef A64_BIT_OPERATION /* this switch is defined in ql.c */
#define IOCB_SEGS			2
#define CONTINUATION_SEGS		5
#else
#define IOCB_SEGS			4
#define CONTINUATION_SEGS		7
#endif /* A64_BIT_OPERATION */

#define MAX_CONTINUATION_ENTRIES	254

#define MAX_ADAPTERS            128

#define QL_MAXTARG		16
#define QL_MAXLUN		8

#ifdef _STANDALONE

#define REQUEST_QUEUE_DEPTH     8
#define RESPONSE_QUEUE_DEPTH    8
#define MAX_REQ_INFO            7

#else

#define REQUEST_QUEUE_1040_DEPTH     256	
#define RESPONSE_QUEUE_1040_DEPTH    256 

#define REQUEST_QUEUE_1240_DEPTH     256	
#define RESPONSE_QUEUE_1240_DEPTH    256 

#define MAX_REQ_INFO            255

#endif /* STANDALONE */

#define DEF_EXECUTION_THROTTLE      255


/**********************************************************
**
**      Internal Command Information
*/

#define READ_CAP_DATA_LENGTH        8
#ifdef _STANDALONE
#define REQ_SENSE_DATA_LENGTH       18
#else
#define REQ_SENSE_DATA_LENGTH       32
#endif

/*********************************************************
**
**      Host Adapter Information
*/

/***
**** Queue Entry.
**** Used only to allocate space for request and response structures.
***/
typedef struct
{
    u_char      data[64];
} queue_entry;


/***
**** DATA SEGMENT structures.
***/
typedef struct
{
    __psint_t     base;
    u_int         count;
} data_seg;

typedef struct
{
    u_int     base_low_32;
#ifdef A64_BIT_OPERATION
    u_int     base_high_32;
#endif

    u_int     count;
} ql_data_seg;

typedef struct scatter_g
{
        /* array of scatter gather elements*/
    uint        num_pages;
    data_seg    dseg[REQUEST_QUEUE_1040_DEPTH*CONTINUATION_SEGS];

} scatter,*p_scatter;


typedef struct req_information
{
    alenlist_t		alen_l;
    int			timeout;
    void *		ha_p;
    int			cmd_age;
    int			starttime;
    scsi_request_t*	req;
} REQ_INFO, *pREQ_INFO;

typedef	struct	timeout_info 
{
    int		current_timeout;
    int		watchdog_timer;
    int		ql_dups;
    int		ql_tcmd;
    int		time_base;
    int         t_last_intr;
    int		timeout_state;
    mutex_t	per_tgt_tlock;
} TO_INFO, *pTO_INFO;


/* chip default information */
typedef struct  
{
	char 		Id[4];
	u_char		Version;
	u_char		Fifo_Threshold;
	u_char		Retry_Count;
	u_char		Retry_Delay;
	u_char		Command_DMA_Burst_Enable:1;
	u_char		Data_DMA_Burst_Enable:1;
	u_char		Unused0:6;
	u_char		Tag_Age_Limit;
	u_short		Fast_Memory_Enable;
	u_short		Firmware_Features;


} Chip_Default_Parameters;
 
typedef struct
{

      /* bits */

    	u_char  Not_Used0           :1; /* 2 */
    	u_char  HA_Enable           :1; /* 3 */
    	u_char  Initiator_SCSI_Id   :4; /* 4,5,6,7 */

    	u_char  Bus_Reset_Delay;
	
	u_char	Not_Used1			:2; /* 7, 6 */
    	u_char  DATA_Line_Active_Negation   	:1; /* 5 */
        u_char  REQ_ACK_Active_Negation     	:1; /* 4 */
        u_char  ASync_Data_Setup_Time       	:4; /* 0,1,2,3 */

    	u_char  Tag_Age_Limit;

    	u_short Selection_Timeout;
    	u_short Max_Queue_Depth;
    	u_short Delay_after_reset;
	u_char	Physical_interfer_mode;
	u_char  Force_interface_mode;


    	struct
    	{
    		union {
        		u_char  byte;   /* access by entire byte */

        		struct {                    /*bit*/
            			u_char  Disconnect_Allowed  :1; /* 7 */
            			u_char  Parity_Checking     :1; /* 6 */
            			u_char  Wide_Data_Transfers :1; /* 5 */
            			u_char  Sync_Data_Transfers :1; /* 4 */
            			u_char  Tagged_Queuing      :1; /* 3 */
            			u_char  Auto_Request_Sense  :1; /* 2 */
            			u_char  Stop_Queue_on_Check :1; /* 1 */
            			u_char  Renegotiate_on_Error    :1; /* 0 */
       		 	} bits;

    		} Capability;

    		u_char      Throttle;
    		u_char      Sync_Period;
                            /* bits */
    		u_char      Sync_Offset     :4; /* 0,1,2,3 */
    		u_char      Wide_Allowed    :1; /* 4 */
    		u_char      Sync_Allowed    :1; /* 5 */
    		u_char      Force_Sync      :1; /* 6 */
    		u_char      Is_CDROM        :1; /* 7 */


    	} Id[QL_MAXTARG];

} Default_Parameters;


typedef struct chip_information 
{
    	/*
    	** Base I/O Address for this card.
    	** Make sure to convert this to a usable virtual address.
    	*/
    	caddr_t		ha_base;
    	caddr_t 	ha_config_base;
    	vertex_hdl_t	pci_vhdl;	
	struct ha_information 	*ha_info[2];	/* point back the ha information */
	
	/* vertex info */
    	vertex_hdl_t	ctlr_vhdl[2];

	u_int		chip_flags;	/* general flags for the chip */	
	u_short		device_id;
	u_short		revision;	/* PCI revision ID number */
	u_short		bridge_revnum;	/* bridge revision number */
	u_short		clock_rate;

	u_short		*risc_code;	/* risc code firmware data */

	u_short		risc_code_length;	/* risc code data length */
	u_short		risc_code_addr;	/* risc code starting address */

	u_short		channel_count;

  	/*
         ** Request and Response pointers
         */
        u_short         request_in;             /* request in pointer */
        u_short         request_out;            /* request out pointer */
        u_short         response_out;           /* response out pointer (copy of mailbox 5 OUT) */
        u_short         response_in;            /* response in pointer (copy of mailbox
5 IN) */

    	int         queue_space;

    	queue_entry*    request_ptr;
    	queue_entry*    request_base;
    	queue_entry*    response_ptr;
    	queue_entry*    response_base;

    	int             ql_request_queue_depth;
    	int             ql_response_queue_depth;

    	pciio_dmamap_t  request_dmamap;
    	paddr_t         request_dmaptr;
    	pciio_dmamap_t  response_dmamap;
    	paddr_t         response_dmaptr;

#if !defined(_STANDALONE) && !defined(OLD_PCI)
    	pciio_intr_t        intr_info;
#endif

    	/* scatter gather holding place */
#if defined(_STANDALONE)
    	scatter     scatter_base;
#else
    	alenlist_t  alen_p;
#endif


	/* locking resources */
    	mutex_t     res_lock;
    	mutex_t     req_lock;
    	mutex_t     mbox_lock;
    	mutex_t     probe_lock;
    	sema_t      mbox_done_sema;

    	int         ha_last_intr;
    	int         mbox_timeout_info;
    	u_short     mailbox[8];

#ifdef PIO_TRACE
    	uint        *ql_log_vals;
#endif

#if defined(QL_INTR_DEBUG)
    	int         ha_interrupt_pending;
#endif

	/* chip default parameters */
	Chip_Default_Parameters	chip_parameters;

	/* device driver specific for passing info back and forward */
	u_short		host_data[3];
	u_short		req_rotor;
	char		chip_name[MAXDEVNAME];

} CHIP_INFO, *pCHIP_INFO;

typedef struct ha_information
{

    	struct chip_information 	*chip_info;
	struct ha_information		*next_ha;

    	u_int  	     	host_flags;		/* general flags for this adapter */

#ifndef _STANDALONE
	
	/* vertex info */
    	vertex_hdl_t	ctlr_vhdl;
    	/* bus quiesce timeout id's */
    	int		quiesce_in_progress_id; /* how long to wait for quiesce to   */
					/* complete			     */
    	toid_t		quiesce_id;             /* id for quiesce_time	 	     */
    	int		quiesce_time;           /* how long to keep the bus quiesced */
#endif

    	/*
    	** General flags for each device.
    	** This is used to determine if the device driver
    	** has performed a SCSI_INIT on the device AND whether the
    	** device has been enabled in the NVRAM.
    	*/
#if defined(_STANDALONE)
    	u_short     	controller_number;	        /* only for standalone */	
    	u_char		dev_init[QL_MAXTARG];	/* set with 1<<lun */
#ifdef IP30
    	void		*bus_base;
#endif
#endif


    	/*
    	** Queued requests.
    	*/
    	scsi_request_t*     req_forw;
    	scsi_request_t*     req_back;

    	/* locations to store the outstanding requests and the associated alen_l */

	TO_INFO		    timeout_info[QL_MAXTARG];
    	int		    reqi_cntr[QL_MAXTARG];
    	REQ_INFO	    reqi_block[QL_MAXTARG][MAX_REQ_INFO + 1];


    	Default_Parameters    ql_defaults;


    	/* mutual exclusion for mp work */
#ifndef _STANDALONE
    	mutex_t			waitQ_lock;
    	int			ql_ncmd;
    	scsi_request_t*     	waitf;
    	scsi_request_t*     	waitb;
#else /* _STANDALONE */
    	mutex_t     	lock;
#endif /* _STANDALONE */

#ifdef QL_STATS
        struct adapter_stats    *adp_stats;
#endif
	u_short			channel_id;	/* 1240 and 1280 will have 0 and 1 */
	u_short			bus_mode;	/* SE, LVD, HVD */
	u_short			bus_speed;	/* fast10, fast20/ultra, fast40/ultra2 */

	char			ha_name[MAXDEVNAME];

} HA_INFO, *pHA_INFO;

/*
** command register bit definitions
*/

#define MEMORY_SPACE_ENABLE 0x2
#define BUS_MASTER_ENABLE   0x4

/*
** DMA Configurations Register definitions 
** command channel pci address 0x80
** data0 channel pci address 0xa0
** data1 channel pci address 0xc0
*/

#define DCR_DMA_DATA_XFER_DIRECTION      0x0001
#define DCR_DMA_BURST_ENABLE             0x0002
#define DCR_DMA_INTERRUPT_ENABLE         0x0004
#define DCR_DMA_REQUEST_FROM_SCSI	 0x0008

/*
 ** dma control register definitions
*/

#define DMA_CON_CLEAR_CHAN      0x8
#define DMA_CON_CLEAR_FIFO	0x4
#define DMA_CON_RESET_INT       0x2


/*
** Host adapter flags.
*/
#ifdef _STANDALONE
#define AFLG_SENSE_HELD		    0x0001
#define AFLG_POLL_ISR               0x0002
#define AFLG_MAIL_BOX_EXPECTED      0x0004
#define AFLG_SEND_MARKER            0x0008
#define AFLG_FAST_NARROW    	    0x0010
#else
/*
 * chip flags 
 * 
 */
#define AFLG_CHIP_INITIALIZED            0x0001		/* isp initialized */
#define AFLG_CHIP_INTERRUPTS_ENABLED     0x0002		/* interrupts enabled */
#define AFLG_CHIP_MAIL_BOX_DONE	    	 0x0004		/* mailbox is done */
#define AFLG_CHIP_MAIL_BOX_WAITING       0x0008		/* waiting mailbox to finish thru interrupt */
#define AFLG_CHIP_MAIL_BOX_POLLING       0x0010		/* polling for mailbox to finish */
#define AFLG_CHIP_SHUTDOWN	    	 0x0020		/* shutdown in progress */
#define AFLG_CHIP_IN_INTR                0x0040		/* in interrupt mode */
#define AFLG_CHIP_DUMPING                0x0080		/* in dumping mode */
#define AFLG_CHIP_ASYNC_RESPONSE_IN      0x0100		/* async response coming in */
#define AFLG_CHIP_DOWNLOAD_FIRMWARE      0x0200		/* downloading firmware */
#define AFLG_CHIP_INITIAL_SRAM_PARITY    0x0400		/* initializing sram parity */
#define AFLG_CHIP_SRAM_PARITY_ENABLE	 0x0800		/* enable sram parity memory checking */
#define AFLG_CHIP_WATCHDOG_TIMER	 0x1000		/* in watchdog timeout routine */

#define AFLG_CHIP_MAIL_BOX_FLAGS	 (AFLG_CHIP_MAIL_BOX_DONE | AFLG_CHIP_MAIL_BOX_WAITING | AFLG_CHIP_MAIL_BOX_POLLING)

/*
 * host adapter flags 
 *
 */
#define AFLG_HA_FAST_NARROW            0x00000001		/* marker needs to be send */
#define AFLG_HA_QUIESCE_IN_PROGRESS    0x00000002		/* quiesce in progress */
#define AFLG_HA_QUIESCE_COMPLETE       0x00000004		/* quiesce completed */
#define AFLG_HA_DRAIN_IO    	       0x00000008		/* drain I/O */
#define AFLG_HA_SEND_MARKER    	       0x00000010		/* fast and narrow setup */
#define AFLG_HA_BUS_SCAN_IN_PROGRESS   0x00000020		/* bus scan in progress */

#define AFLG_HA_QUEUE_PROGRESS_FLAGS		(AFLG_HA_QUIESCE_IN_PROGRESS | AFLG_HA_QUIESCE_COMPLETE | AFLG_HA_DRAIN_IO)

#endif

/*
** Device flags.
*/
#define DFLG_INITIALIZED            0x0001
#define DFLG_INIT_IN_PROGRESS       0x0002 /* LUN Init in progress -- pause LUN queue */
#define DFLG_EXCLUSIVE              0x0004
#define DFLG_CONTINGENT_ALLEGIANCE  0x0008
#define DFLG_ABORT_IN_PROGRESS      0x0010 /* LUN Abort in progress -- pause LUN queue */
#define DFLG_SEND_MARKER            0x0020 /* Send LUN marker after LUN abort */
#define DFLG_BEHAVE_QERR1           0x0100 /* QERR=1 for this device */

/*
** Device capability bits.
*/
#define CAP_RENEGOTIATE_ON_ERROR	0x01
#define CAP_STOP_QUEUE_ON_CHECK         0x02
#define CAP_AUTO_REQUEST_SENSE          0x04
#define CAP_TAGGED_QUEUING		0x08
#define CAP_SYNC_DATA_TRANSFERS         0x10
#define CAP_WIDE_DATA_TRANSFERS         0x20
#define CAP_PARITY_CHECKING		0x40
#define CAP_DISCONNECT_PRIV		0x80


/*
** Request packet host status errors.
*/
#define HSTS_NO_ERR			0x0000
#define HSTS_NO_DEV			0x1001
#define HSTS_NO_INIT			0x1002
#define HSTS_BAD_PARAMETER		0x1003

#define	QLOGIC_DEV_DESC_NAME	"Qlogic SCSI"

/****************************************************************
**
**  ISP Firmware Definitions
*/

/***
**** ENTRY HEADER structure.
***/
typedef struct
{
    u_char      flags;
    u_char      sys_def_1;
    u_char      entry_cnt;
    u_char      entry_type;

} entry_header;


/*
** Entry Header Type Defintions.
*/
#ifndef A64_BIT_OPERATION
#define ET_COMMAND              1
#define ET_CONTINUATION         2
#else
#define ET_COMMAND              0x9
#define ET_CONTINUATION         0xa
#endif
#define ET_STATUS               3
#define ET_MARKER               4
#define ET_EXTENDED_COMMAND     5

/*
** Entry Header Flag Definitions.
*/
#define EF_CONTINUATION             0x01
#define EF_BUSY                     0x02
#define EF_BAD_HEADER               0x04
#define EF_BAD_PAYLOAD              0x08
#define EF_ERROR_MASK   (EF_BUSY | EF_BAD_HEADER | EF_BAD_PAYLOAD)


/***
**** COMMAND ENTRY structure.
***/
typedef struct
{
    entry_header    hdr;

    u_int       handle;

    u_short     cdb_length;
    u_char	channel_id:1,
         	target_id:7;
    u_char      target_lun;

    u_short     reserved;
    u_short     control_flags;

    u_short     segment_cnt;
    u_short     time_out;

    u_char      cdb3;
    u_char      cdb2;
    u_char      cdb1;
    u_char      cdb0;

    u_char      cdb7;
    u_char      cdb6;
    u_char      cdb5;
    u_char      cdb4;

    u_char      cdb11;
    u_char      cdb10;
    u_char      cdb9;
    u_char      cdb8;
#ifdef A64_BIT_OPERATION
    u_int       reserved2;
    u_int       reserved3;
#endif /* A64_BIT_OPERATION */

    ql_data_seg dseg[IOCB_SEGS];
} command_entry;

/*
** Command Entry Control Flag Definitions.
*/
#define CF_NO_DISCONNECTS           0x0001
#define CF_HEAD_TAG                 0x0002
#define CF_ORDERED_TAG              0x0004
#define CF_SIMPLE_TAG               0x0008
#define CF_TARGET_ROUTINE           0x0010
#define CF_READ                     0x0020
#define CF_WRITE                    0x0040
#define CF_RESERVED       	    0x0080
#define CF_AUTO_SENSE_DISABLE       0x0100
#define CF_FORCE_ASYNCHONOUS        0x0200
#define CF_FORCE_SYNCHONOUS         0x0400
#define CF_INITIATE_WIDE       	    0x0800
#define CF_DISABLE_DATA_PARITY      0x1000
#define CF_STOP_QUEUE       	    0x2000
#define CF_SUPPORT_EXTENDED_SENSE   0x4000
#define CF_PRIORITY_FLAG       	    0x8000




/***
**** EXTENDED COMMAND ENTRY structure.
**** note: the extended command entry in not currently used.
****       need to define swapped and not swapped versions of this
****       structure.  (this one is not swapped)
***/
typedef struct
{
    entry_header    hdr;

    u_int       handle;
    u_char      target_lun;
    u_char      target_id;
    u_short     cdb_length;
    u_short     control_flags;
    u_short     reserved;
    u_short     time_out;
    u_short     segment_cnt;
    u_char      cdb[44];
} ext_command_entry;


/***
**** CONTINUATION ENTRY structures.
***/
typedef struct
{
    entry_header    hdr;
#ifndef A64_BIT_OPERATION
    u_int           reserved;
#endif
    ql_data_seg     dseg[CONTINUATION_SEGS];
} continuation_entry;


/***
**** MARKER ENTRY structure.
***/

typedef struct
{
    entry_header    hdr;

    u_int       reserved0;

    u_char      reserved1;
    u_char      modifier;
    u_char	channel_id:1,
    	      	target_id:7;
    u_char      target_lun;

    u_char      reserved2[52];
} marker_entry;


/*
** Marker Entry Modifier Definitions.
*/
#define MM_SYNC_DEVICE          0
#define MM_SYNC_TARGET          1
#define MM_SYNC_ALL             2


/***
**** STATUS ENTRY structure.
***/

typedef struct
{
    entry_header  hdr;

    u_int       handle;

    u_short     completion_status;
    u_short     scsi_status;

    u_short     status_flags;
    u_short     state_flags;

    u_short     req_sense_length;
    u_short     time;

    u_int       residual;

    u_char      reserved[8];

    u_char      req_sense_data[32];
} status_entry;


/*
** Status Entry Completion Status Defintions.
*/
#define SCS_COMPLETE                	0x0000
#define SCS_INCOMPLETE              	0x0001
#define SCS_DMA_ERROR               	0x0002
#define SCS_TRANSPORT_ERROR         	0x0003
#define SCS_RESET_OCCURRED          	0x0004
#define SCS_ABORTED                 	0x0005
#define SCS_TIMEOUT                 	0x0006
#define SCS_DATA_OVERRUN            	0x0007
#define SCS_COMMAND_OVERRUN         	0x0008
#define SCS_STATUS_OVERRUN          	0x0009
#define SCS_BAD_MESSAGE             	0x000A
#define SCS_NO_MESSAGE_OUT          	0x000B
#define SCS_EXT_ID_FAILED           	0x000C
#define SCS_IDE_MSG_FAILED          	0x000D
#define SCS_ABORT_MSG_FAILED            0x000E
#define SCS_REJECT_MSG_FAILED           0x000F
#define SCS_NOP_MSG_FAILED          	0x0010
#define SCS_PARITY_ERROR_MSG_FAILED     0x0011
#define SCS_DEVICE_RESET_MSG_FAILED     0x0012
#define SCS_ID_MSG_FAILED           	0x0013
#define SCS_UNEXPECTED_BUS_FREE         0x0014
#define SCS_DATA_UNDERRUN           	0x0015

#define SCS_INVALID_TYPE_ENTRY		0x001B
#define SCS_DEVICE_QUEUE_FULL           0x001C
#define SCS_SCSI_PHASE_SKIPPED          0x001D
#define SCS_AUTO_SENSE_FAILED           0x001E
#define SCS_WIDE_DATA_XFER_FAILED	0x001F
#define SCS_SYNC_DATA_XFER_FAILED	0x0020
#define SCS_LVD_BUS_ERROR		0x0021

/*
** Status Entry State Flag Definitions.
*/
#define SS_GOT_BUS              	0x0100
#define SS_GOT_TARGET            	0x0200
#define SS_SENT_CDB             	0x0400
#define SS_TRANSFERRED_DATA         	0x0800
#define SS_GOT_STATUS               	0x1000
#define SS_GOT_SENSE                	0x2000
#define SS_TRANSFER_COMPLETE            0x4000

/*
** Status Entry Status Flag Definitions.
*/
#define SST_DISCONNECT                  0x0001
#define SST_SYNCHRONOUS                 0x0002
#define SST_PARITY_ERROR                0x0004
#define SST_BUS_RESET                   0x0008
#define SST_DEVICE_RESET                0x0010
#define SST_ABORTED            	        0x0020
#define SST_TIMEOUT                     0x0040
#define SST_NEGOTIATION                 0x0080

/*
** Mailbox Command Definitions.
*/
#define MBOX_CMD_NOP         		0x0000
#define MBOX_CMD_LOAD_RAM       	0x0001
#define MBOX_CMD_EXECUTE_FIRMWARE       0x0002
#define MBOX_CMD_DUMP_RAM           	0x0003
#define MBOX_CMD_WRITE_RAM_WORD         0x0004
#define MBOX_CMD_READ_RAM_WORD          0x0005
#define MBOX_CMD_MAILBOX_REGISTER_TEST  0x0006
#define MBOX_CMD_VERIFY_CHECKSUM        0x0007
#define MBOX_CMD_ABOUT_FIRMWARE         0x0008

#define MBOX_CMD_INIT_REQUEST_QUEUE     0x0010
#define MBOX_CMD_INIT_RESPONSE_QUEUE    0x0011
#define MBOX_CMD_WAKE_UP    		0x0013
#define MBOX_CMD_STOP_FIRMWARE          0x0014
#define MBOX_CMD_ABORT 	          	0x0015
#define MBOX_CMD_ABORT_DEVICE           0x0016
#define MBOX_CMD_ABORT_TARGET           0x0017


#define MBOX_CMD_BUS_RESET          	0x0018

#define MBOX_CMD_GET_FIRMWARE_STATUS    	0x001F
#define MBOX_CMD_GET_INITIATOR_SCSI_ID  	0x0020
#define MBOX_CMD_GET_SELECTION_TIMEOUT  	0x0021
#define MBOX_CMD_GET_RETRY_COUNT        	0x0022
#define MBOX_CMD_GET_TAG_AGE_LIMIT      	0x0023
#define MBOX_CMD_GET_CLOCK_RATE         	0x0024
#define MBOX_CMD_GET_ACTIVE_NEGATION_STATE  	0x0025
#define MBOX_CMD_GET_ASYNC_DATA_SETUP_TIME  	0x0026
#define MBOX_CMD_GET_BUS_CONTROL_PARAMETERS 	0x0027
#define MBOX_CMD_GET_TARGET_PARAMETERS      	0x0028
#define MBOX_CMD_GET_DEVICE_QUEUE_PARAMETERS    0x0029
#define MBOX_CMD_GET_RESET_DELAY_PARAMETERS	0x002A

#define MBOX_CMD_SET_INITIATOR_SCSI_ID      	0x0030
#define MBOX_CMD_SET_SELECTION_TIMEOUT      	0x0031
#define MBOX_CMD_SET_RETRY_COUNT        	0x0032
#define MBOX_CMD_SET_TAG_AGE_LIMIT      	0x0033
#define MBOX_CMD_SET_CLOCK_RATE         	0x0034
#define MBOX_CMD_SET_ACTIVE_NEGATION_STATE  	0x0035
#define MBOX_CMD_SET_ASYNC_DATA_SETUP_TIME  	0x0036
#define MBOX_CMD_SET_BUS_CONTROL_PARAMETERS 	0x0037
#define MBOX_CMD_SET_TARGET_PARAMETERS      	0x0038
#define MBOX_CMD_SET_DEVICE_QUEUE_PARAMETERS    0x0039
#define MBOX_CMD_SET_RESET_DELAY_PARAMETERS	0x003A

#define MBOX_CMD_SET_SYSTEM_PARAMETERS		0x0045
#define MBOX_CMD_GET_SYSTEM_PARAMETERS		0x0046
#define MBOX_CMD_SET_FIRMWARE_FEATURES		0x004A
#define MBOX_CMD_GET_FIRMWARE_FEATURES		0x004B


#define MBOX_CMD_SET_DATA_RECOVERY_MODE    	0x005A
#define MBOX_CMD_GET_DATA_RECOVERY_MODE         0x005B
#define MBOX_CMD_SET_HOST_DATA         		0x005C
#define MBOX_CMD_GET_HOST_DATA                  0x005D





/* 64 bit operation commands */

#define MBOX_CMD_LOAD_RAM_64		    0x0050
#define MBOX_CMD_DUMP_RAM_64                0x0051
#define MBOX_CMD_INIT_REQUEST_QUEUE_64      0x0052
#define MBOX_CMD_INIT_RESPONSE_QUEUE_64     0x0053
#define MBOX_CMD_EXECUTE_IOCB_64            0x0054


/*
** Mailbox Status Definitions.
*/
#define MBOX_STS_FIRMWARE_ALIVE         	0x0000
#define MBOX_STS_CHECKSUM_ERROR         	0x0001
#define MBOX_STS_SHADOW_LOAD_ERROR      	0x0002
#define MBOX_STS_BUSY               		0x0004

#define MBOX_STS_COMMAND_COMPLETE       	0x4000
#define MBOX_STS_INVALID_COMMAND        	0x4001
#define MBOX_STS_HOST_INTERFACE_ERROR   	0x4002
#define MBOX_STS_TEST_FAILED            	0x4003
#define MBOX_STS_COMMAND_ERROR          	0x4005
#define MBOX_STS_COMMAND_PARAMETER_ERROR    	0x4006
#define MBOX_STS_BUS_CONFIG_ERROR		0x4007

#define MBOX_ASTS_SCSI_BUS_RESET        	0x8001
#define MBOX_ASTS_SYSTEM_ERROR          	0x8002
#define MBOX_ASTS_REQUEST_TRANSFER_ERROR    	0x8003
#define MBOX_ASTS_RESPONSE_TRANSFER_ERROR   	0x8004
#define MBOX_ASTS_REQUEST_QUEUE_WAKEUP      	0x8005
#define MBOX_ASTS_EXECUTION_TIMEOUT_RESET		0x8006
#define MBOX_ASTS_EXTENDED_MESSAGE_UNDERRUN	0x800A
#define MBOX_ASTS_SCAM_INTERRUPT		0x800B
#define MBOX_ASTS_SCSI_BUS_HANG			0x800C
#define MBOX_ASTS_SCSI_BUS_RESET_BY_ISP		    	0x800D
#define MBOX_ASTS_SCSI_BUS_MODE_TRANSITION	0x800E

#define MBOX_ASTS_FAST_POSTING_EVENT_CODE	0x8020





/****************************************************************
**
**  ISP Register Definitions
*/


typedef volatile struct
{
    /*
    ** Bus interface registers.
    */                      /* Offsets  */


    u_short bus_id_high;        /* 0002 */
    u_short bus_id_low;         /* 0000 */

    u_short bus_config1;        /* 0006 */
    u_short bus_config0;        /* 0004 */

    u_short bus_isr;            /* 000A */
    u_short bus_icr;            /* 0008 */

    u_short NvRam_reg;		/* 000E */
    u_short bus_sema;           /* 000C */


    /*
    ** Skip over DMA Controller registers:
    **   0020-002F - Command DMA Channel
    **   0040-004F - Data DMA Channel ----- 
    **			except for the dma config and control reg
    **   0060-0063 - DMA FIFO Access Ports
    */

    u_char  not_used[0x0010];

    u_short control_dma_control;           /* 0022 */
    u_short control_dma_config;	           /* 0020 */
    u_short control_dma_fifo_status;       /* 0026 */
    u_short control_dma_status;	           /* 0024 */
    u_short control_dma_reserved;          /* 002a */
    u_short control_dma_counter;           /* 0028 */
    u_short control_dma_address_counter_1; /* 002e */
    u_short control_dma_address_counter_0; /* 002c */
    u_short control_dma_address_counter_3; /* 0032 */
    u_short control_dma_address_counter_2; /* 0030 */


    u_char  not_used0[0x000c];


    u_short data_dma_control;	           /* 0042 */
    u_short data_dma_config;	           /* 0040 */
    u_short data_dma_fifo_status;          /* 0046 */
    u_short data_dma_status;	           /* 0044 */
    u_short data_dma_xfer_counter_high;	   /* 004a */
    u_short data_dma_xfer_counter_low;     /* 0048 */
    u_short data_dma_address_counter_1;	   /* 004e */
    u_short data_dma_address_counter_0;	   /* 004c */
    u_short data_dma_address_counter_3;	   /* 0052 */
    u_short data_dma_address_counter_2;	   /* 0050 */


    u_char  not_used1[0x001c];

    /*
    ** Mailbox registers.
    */
    u_short mailbox1;           /* 0070     */
    u_short mailbox0;           /* 0072     */
    u_short mailbox3;           /* 0074     */
    u_short mailbox2;           /* 0076     */
    u_short mailbox5;           /* 0078     */
    u_short mailbox4;           /* 007A     */
    u_short mailbox7;           /* 007C     */
    u_short mailbox6;           /* 007E     */


    /*
    ** Note: in V3 part, RISC and SXP registers are paged.
    ** Since we don't use SXP registers, we won't define them. (only 1040 fw)
    ** - 1080/1240/1280 fw will be using this bank of register for DMA, SXP, and RISC.
    */

    /*
    ** 
    **  0080-00BF - RISC Registers.
    */

    u_short r1;			/* 82   */
    u_short accumulator;	/* 80	*/
    
    u_short r3;			/* 86   */
    u_short r2;			/* 84   */

    u_short r5;			/* 8a   */
    u_short r4;			/* 88   */

    u_short r7;			/* 8e   */
    u_short r6;			/* 8c   */

    u_short r9;			/* 92   */
    u_short r8;			/* 90   */

    u_short r11;		/* 96   */
    u_short r10;		/* 94   */

    u_short r13;		/* 9a   */
    u_short r12;		/* 98   */

    u_short r15;                /* 9e   */
    u_short r14;		/* 9c   */


    u_short ivr;		/* a2   */
    u_short psr;		/* a0   */

    u_short rar0;		/* a6   */
    u_short pcr;		/* a4   */

    u_short lcr;		/* aa   */
    u_short rar1;		/* a8   */

    u_short mtr;		/* ae   */
    u_short risc_pc;		/* ac	*/

    u_short sp;			/* b2   */
    u_short emb;		/* b0   */

    u_short b6;			/* b6   */
    u_short b4;			/* b4   */

    u_short ba;			/* ba   */
    u_short b8;			/* b8   */

    u_short hardware_revision;	/* be   */
    u_short bc;			/* bc	*/

    /*
    ** Host Configuration and Control register.
    */
    u_short c2;			/* c2   */
    u_short hccr;               /* c0     */

    u_short c6;			/* c6   */
    u_short c4;			/* c4   */

    u_short ca;			/* ca   */
    u_short c8;			/* c8   */

    u_short ce;			/* ce   */
    u_short cc;			/* cc	*/

    u_short d2;			/* d2   */
    u_short d0;			/* d0   */

    u_short d6;			/* d6   */
    u_short d4;			/* d4   */

    u_short da;			/* da   */
    u_short d8;			/* d8   */

    u_short de;			/* de   */
    u_short dc;			/* dc	*/

    u_short e2;			/* e2   */
    u_short e0;			/* e0   */

    u_short e6;			/* e6   */
    u_short e4;			/* e4   */

    u_short ea;			/* ea   */
    u_short e8;			/* e8   */

    u_short ee;			/* ee   */
    u_short ec;			/* ec	*/

    u_short f2;			/* f2   */
    u_short f0;			/* f0   */

    u_short f6;			/* f6   */
    u_short f4;			/* f4   */

    u_short fa;			/* fa   */
    u_short f8;			/* f8   */

    u_short fe;			/* fe   */
    u_short fc;			/* fc	*/


} ISP_REGS, *pISP_REGS;




/*
** Bus Configuration Register 1 Definitions.
*/
#define CONF_1_BURST_ENABLE     0x0004
#define CONF_1_1040_SXP_SEL     0x0008  /* Select SXP bank */
#define CONF_1_1240_SXP0_SEL    0x0200  /* Select SXP0 bank */
#define CONF_1_1240_SXP1_SEL    0x0100  /* Select SXP1 bank */
#define CONF_1_1240_DMA_SEL     0x0300  /* Select DMA bank */
#define CONF_1_1280_FMRM	0x0800  /* Force memory read multiple (1280 only) */
#define CONF_1_1280_DMA_CAM	0x2000	/* DMA channel arbitration mode (1280 only) */


/* this set the high water mark to 32 bytes (16) observed */


/*
** Bus Control Register Definitions.
*/
#define ICR_SOFT_RESET		0x0001
#define ICR_ENABLE_ALL_INTS	0x0002
#define ICR_ENABLE_RISC_INT	0x0004
#define ICR_ENABLE_SXP_INT	0x0008
#define ICR_ENABLE_SXP1_INT	0x0008		/* 1240 and 1280 only */
#define ICR_ENABLE_SXP0_INT	0x0040		/* 1240 and 1280 only */
#define ICR_ENABLE_DATA0_INT	0x0020		/* 1240 and 1280 only */
#define ICR_ENABLE_DATA1_INT	0x0010		/* 1240 and 1280 only */
#define ICR_1280_FLASH_BIOS_RW	0x0100		/* 1280 only */
#define ICR_1280_FLASH_BIOS_SEL 0x0200		/* 1280 only */


/*
** Bus Interrupt Status Register Defintions.
*/
#define BUS_ISR_PCI_64_SLOT	0x4000
#define BUS_ISR_SXP0_INT	0x0080
#define BUS_ISR_DATA0_INT	0x0040
#define BUS_ISR_DATA1_INT	0x0020
#define BUS_ISR_COMMAND_INT	0x0010
#define BUS_ISR_SXP1_INT	0x0008
#define BUS_ISR_RISC_INT	0x0004
#define BUS_SXP_ERR_INT		0x0008
#define BUS_ISR_PCI_INT		0x0002


/* 
 * Processor Status Register Definitions.
 */
#define PSR_LOOP_COUNT_DONE	0x4000
#define PSR_RISC_INT		0x2000
#define PSR_TIMER_ROLLOVER_FLAG	0x1000
#define PSR_ALU_OVERFLOW	0x0800
#define PSR_ALU_MSB		0x0400
#define PSR_ALU_CARRY		0x0200
#define PSR_ALU_ZERO		0x0100
#define PSR_60_MHZ		0x0080
#define	PSR_LVD_INT		0x0040 		/* 1080 and 1280 only */
#define PSR_SXP0_INT		0x0020		/* 1240 and 1280 only */
#define PSR_DMA_INT		0x0010
#define PSR_SXP1_INT		0x0008		/* 1240 and 1280 only , sxp0 for 1040 and 1080 */
#define PSR_HOST_INT		0x0004	
#define PSR_INT_PENDING		0x0002


/*
** Bus Semaphore Register Definitions.
*/
#define BUS_SEMA_LOCK           0x0001
#define BUS_SEMA_STATUS         0x0002


/*
** Processor Control Register Definitions.
*/

	/* Command Definitions */

#define PCR_CMD_RESTORE_PCR			0x1000 		/* restore PCR */
#define PCR_CMD_UPDATE_BANK_SELECT		0x2000		/* update bank select */
#define PCR_CMD_UPDATE_INTERRUPT_ENABLES	0x3000		/* update interrupt enables */
#define PCR_CMD_UPDATE_RISC_INTERRUPT		0x4000		/* update risc interrupt */
#define PCR_CMD_RESET_HOST_INTERRUPT		0x5000		/* reset host interrupt */
#define PCR_CMD_RESET_TIMER_FLAG		0x6000		/* reset timer flag */
#define PCR_CMD_SHADOW_MODE			0x7000		/* shadow mode */
#define PCR_CMD_UPDATE_MASTER_INTERRUPT_ENABLE	0x8000		/* update master interrupt enable */
#define PCR_CMD_UPDATE_PARITY_ENABLES		0x9000		/* update parity enables */
#define PCR_CMD_TEST_INTERRUPTS			0xf000		/* test interrupts */


	/*  Control Definitions */
#define PCR_REGISTER_FILE_BANK_0		0x0000		/* register file bank 0 */ 
#define PCR_REGISTER_FILE_BANK_1		0x0100		/* register file bank 1 */
#define PCR_REGISTER_FILE_BANK_2		0x0200		/* register file bank 2 */
#define PCR_REGISTER_FILE_BANK_3		0x0300		/* register file bank 3 */
#define PCR_REGISTER_FILE_BANK_4		0x0400		/* register file bank 4 */
#define PCR_REGISTER_FILE_BANK_5		0x0500		/* register file bank 5 */
#define PCR_REGISTER_FILE_BANK_6		0x0600		/* register file bank 6 */
#define PCR_REGISTER_FILE_BANK_7		0x0700		/* register file bank 7 */

#define PCR_CONTROL_BIT				0x0080		/* control bit */
#define PCR_LVD_INT_ENABLE			0x0020		/* 1080 and 1280 only */

#define PCR_INTERRUPT_CONTROL_SCSI_0		0x0010		/* SXP_0 */
#define PCR_INTERRUPT_CONTROL_DMA		0x0008		/* DMA */
#define PCR_INTERRUPT_CONTROL_SCSI_1		0x0004		/* SXP_1 */
#define PCR_INTERRUPT_CONTROL_HOST		0x0002		/* HOST */
#define PCR_INTERRUPT_CONTROL_MASTER		0x0001		/* Master interrupt enable */


/*
** Host Command and Control Register Definitions.
*/

        /* Command Definitions */

#define HCCR_CMD_RESET              0x1000  /* reset RISC */
#define HCCR_CMD_PAUSE              0x2000  /* pause RISC */
#define HCCR_CMD_RELEASE            0x3000  /* release paused RISC */
#define HCCR_CMD_SET_HOST_INT       0x5000  /* set host interrupt */
#define HCCR_CMD_CLEAR_RISC_INT     0x7000  /* clear RISC interrupt */
#define HCCR_WRITE_BIOS_ENABLE      0x9000  /* write BIOS enable */
#define HCCR_SRAM_PARITY_ENABLE     0xA000  /* enable sram parity */
#define HCCR_FORCE_PARITY_ERROR     0xE000  /* force sram parity error*/



        /* Status Defintions */
#define HCCR_SRAM_PARITY_ERROR_DETECTED_1240  0x0800  /* R: parity error detected */
#define HCCR_SRAM_PARITY_ERROR_DETECTED_1040  0x0400  /* R: parity error detected */
#define HCCR_SRAM_PARITY_BANK2  0x0400  /* R: bank two enable */
#define HCCR_SRAM_PARITY_BANK1  0x0200  /* R: bank one enable */
#define HCCR_SRAM_PARITY_BANK0  0x0100  /* R: bank zero enable */
#define HCCR_HOST_INT       	0x0080  /* R: host interrupt set */
#define HCCR_RESET          	0x0040  /* R: RISC reset in progress */
#define HCCR_PAUSE          	0x0020  /* R: RISC paused */
#define HCCR_EXTERN_BP_ENABLE   0x0010  /* R: enable external breakpoint */
#define HCCR_EXTERN_BP1_ENABLE  0x0008  /* R: enable external breakpoint 1 */
#define HCCR_EXTERN_BP0_ENABLE  0x0004  /* R: enable external breakpoint 2 */
#define HCCR_ENEABLE_INT_BP     0x0002  /* R: enable interrupt on breakpoint */
#define HCCR_FORCE_PARITY       0x0001  /* R: force an sram parity errror  */




/* 
 * DMA Interrupt Status Register  (RISC register 0x1c)
 */
#define DMA_DATA0_INT		0x0004
#define DMA_DATA1_INT		0x0002
#define DMA_COMMAND_INT		0x0001


/****************************************************************
** Temporary definitions (DEBUG)
*/


#define READ_CMD_28 	0x28
#define WRITE_CMD_2A 	0x2a
#define SCSI_DISK_INFO 	0x25 /*read capacity */
#define SCSI_TAPE_INFO 	0xff /* not implemented */
#define SCSI_SET_ASYNC 	0xfe /* set sync via mailbox command */
#define SCSI_INFO 	0xfd /* do an info to see what is set */
#define INQUIRY_CMD	0x12


/********************************************************************
**
**                           Constants
*/


/* clock rates for ISP1020 and ISP1040 */

#define FAST_CLOCK_RATE 40
#define FAST20_CLOCK_RATE 60
#define FAST40_CLOCK_RATE 100		/* 1080 and 1280 only */

#define FAST20_CHECK_BIT 0x0020		
#define FAST40_CHECK_BIT 0x0040		/* 1080 and 1280 only */


/*
 * SCSI BUS SPEED 
 */
#define SCSI_BUS_SPEED_FAST10		0x0100		/* 1020 only */
#define SCSI_BUS_SPEED_FAST20		0x0200		/* 1040 and 1240 */
#define SCSI_BUS_SPEED_FAST40		0x0400		/* 1080 and 1280 */

/*
 * SCSI Bus operation modes are determined by the DIFFM and DIFFS pins.
 */
#define SCSI_BUS_MODE_SINGLE_ENDED	0x0001
#define SCSI_BUS_MODE_HVD		0x0002
#define SCSI_BUS_MODE_LVD		0x0004

/* This is for bit mask when 0x800E Asynchronous event code return and the data is mailbox1 */
#define SCSI_BUS_MODE_MASK 		0x0007



/* 
 * SCSI Differential Control Pins Register - risc address 0x13b pci offset address 0xf6
 */
#define SCSI_DCPR_LVD_MODE		0x1000
#define SCSI_DCPR_HVD_MODE		0x0800
#define SCSI_DCPR_SE_MODE		0x0400
#define SCSI_DCPR_DIFF_MODE		0x0100
#define SCSI_DCPR_INTERNAL_DO_ENABLE	0x0080
#define SCSI_DCPR_EXTERNAL_RESET_ENABLE	0x0040
#define SCSI_DCPR_EXTERNAL_BUSY_ENABLE	0x0020
#define SCSI_DCPR_EXTERNAL_SEL_ENABLE	0x0010
#define SCSI_DCPR_EXTERNAL_DATA_ENABLE	0x0008
#define SCSI_DCPR_EXTERNAL_ARB_ENABLE	0x0004
#define SCSI_DCPR_TARGET_ENABLE		0x0002
#define SCSI_DCPR_INITIATOR_ENABLE	0x0001



/********************************************************************
**
**                           Structures
*/

/*
** Host Adapter defaults.
*/

/* new threshold control in Config Register 1   */
/* bits		name				*/
/* 15->8	reserved                        */
/* 7->4         Extended FIFO control           */
/* 3            SXP Register Select             */
/* 2            Burst enable                    */
/* 1->0         FIFO control                    */
/*                                              */
/*                                              */
/* the original 1020 only used bits 0 and 1     */
/* but this could not occomodate 128 the btye option*/
/* Now only use the Extended FIFO control       */

/*  bits 7->4	bits 1->0	burst size	*/
/*						*/ 
/* 	6		x	512		*/
/*	5		x  	256		*/
/*   	4		x	128		*/
/*   	3		x	64		*/
/*   	2		x	32		*/
/*   	1		x	16		*/
/*   	0		3	64		*/
/*   	0		2	32		*/
/*   	0		1	16		*/
/*   	0		0	8		*/

#define BURST_512 0x60			/* for 1240 1080, and 1280 chips only */
#define BURST_256 0x50			/* for 1240 1080, and 1280 chips only */
#define BURST_128 0x40
#define BURST_64  0x30
#define BURST_32  0x20
#define BURST_16  0x10



#define HA_ENABLE             		1   /* enabled */
#if defined(EVEREST) 
#define INITIATOR_SCSI_ID		7
#else
#define INITIATOR_SCSI_ID     		0
#endif
#define BUS_RESET_DELAY       		3   /* 3 seconds */
#define RETRY_COUNT           		0   /* no retries */
#define RETRY_DELAY           		0   /* no time between tries*/
#define ASYNC_DATA_SETUP_TIME     	9   /* 9 clock periods */
#define REQ_ACK_ACTIVE_NEGATION   	1   /* enabled */
#define DATA_ACTIVE_NEGATION      	1   /* enabled */
#define DATA_DMA_BURST_ENABLE     	0x01   /* enabled */
#define CMD_DMA_BURST_ENABLE      	0x01   /* enabled */

#define TAG_AGE_LIMIT         		8

#define SELECTION_TIMEOUT       	250 /* 250 ms */
#define SELECTION_TIMEOUT_SHORT    	75  /* 75  us */

#define MAX_QUEUE_DEPTH       		256 /* 256 commands */

#define DELAY_AFTER_RESET      		1   /*second*/

/*
** Drive defaults.
*/
#ifdef _STANDALONE
#define RENEGOTIATE_ON_ERROR      0
#define STOP_QUEUE_ON_CHECK       0
#define AUTO_REQUEST_SENSE        1
#define TAGGED_QUEUING            0
#define WIDE_DATA_TRANSFERS       0
#define PARITY_CHECKING           0
#if SN0 || IP30
#define DISCONNECT_ALLOWED        0
#define SYNC_DATA_TRANSFERS       0
#else
#define DISCONNECT_ALLOWED        1
#define SYNC_DATA_TRANSFERS       1
#endif
#define EXECUTION_THROTTLE        16  
#define SYNC_PERIOD          	  25  
#define SYNC_PERIOD_FAST20     	  12
#define SYNC_PERIOD_FAST40	  10		/* 1080 and 1280 */
#define SYNC_OFFSET               8
#define DEVICE_ENABLE             1  
#define DEFAULT_TIMEOUT           10 
#define FIRMWARE_FEATURES	0x0002

#else /* !_STANDALONE */

#define RENEGOTIATE_ON_ERROR      1   /* ON */
#define STOP_QUEUE_ON_CHECK       0   /* OFF */
#define AUTO_REQUEST_SENSE        1   /* ON  */
#define TAGGED_QUEUING       	  1   /* ON */
#define SYNC_DATA_TRANSFERS       1   /* ON */

#ifndef WIDE_DATA_TRANSFERS
#if defined(SN0) || defined(EVEREST) || defined(IP30)
#define WIDE_DATA_TRANSFERS       1   /* ON */
#else
#define WIDE_DATA_TRANSFERS       0   /* OFF */
#endif
#endif /* WIDE_DATA_TRANSFERS */

#define PARITY_CHECKING           1   
#define DISCONNECT_ALLOWED        1   
#define EXECUTION_THROTTLE        255
#define SYNC_PERIOD           	  25  /* 100 ns */
#define SYNC_PERIOD_FAST20        12  /*  50 ns */
#define SYNC_PERIOD_FAST40	  10  /* 25 ns */
#define SYNC_OFFSET               8
#define MAX_SYNC_OFFSET		  12

#define DEFAULT_TIMEOUT		  10 /* default scsi timeout is 10 sec */
#define FIRMWARE_FEATURES	0x0002

#endif  /* _STANDALONE  */



/*
 *  part revision numbers 
 *  starting number for 1020/1040 is 0x00 
 *  starting number for 1240 is 0x10 
 *  starting number for 1080 is 0x20 
 *  starting number for 1280 is 0x30 
 */
#define ISP1040_STARTING_REV_NUM	0x00
#define ISP1240_STARTING_REV_NUM	0x10
#define ISP1080_STARTING_REV_NUM	0x20
#define ISP1280_STARTING_REV_NUM	0x30

/*
 * ISP1020/ISP1040 
 * note that this is the same as 1020 
 * early part were speed graded only  
 */
#define REV_ISP1020	(ISP1040_STARTING_REV_NUM + 1)
/* note that this is the same as 1020 */
/* early part were speed graded only  */
#define REV_ISP1040     (ISP1040_STARTING_REV_NUM + 1) 
#define REV_ISP1040A    (ISP1040_STARTING_REV_NUM + 2)
#define REV_ISP1040AV4  (ISP1040_STARTING_REV_NUM + 3) 
#define REV_ISP1040B    (ISP1040_STARTING_REV_NUM + 4)
#define REV_ISP1040BV2	(ISP1040_STARTING_REV_NUM + 5) /* qlogic 1040B rev 2 */


/*
 * ISP1240
 */
#define REV_ISP1240	(ISP1240_STARTING_REV_NUM + 1)


/*
 * ISP1080
 */
#define REV_ISP1080	(ISP1080_STARTING_REV_NUM + 1)


/* 
 * ISP1280
 */
#define REV_ISP1280	(ISP1280_STARTING_REV_NUM + 1)






/* the following are defines for MBOX_CMD_SET_DATA_RECOVERY_MODE */
/* we are supporting mode 2 right now 				 */
#define DATA_OVERRUN_MODE_0  0 	/* pad data until the target switches out */
				/* of data phase 		          */		
#define DATA_OVERRUN_MODE_1  1  /* hang the bus interrupt host with       */
				/* 0x800c in outgoing mailbox 0 register  */
				/* the host must reset the bus with mail  */
				/* box command				  */

#define DATA_OVERRUN_MODE_2  2  /* ISP resets the SCSI bus places 0x800D  */
				/* in outgoing mailbox 0 and returns all  */
				/* outstanding io's			  */

/**********************************************************
**
**          PCI Service Information
*/

#define PCI_CONFIG          0xCF8
#define PCI_ENABLE          0x80

#define PCI_CONFIG_START        0xC000
#define PCI_CONFIG_LAST         0xCFFF

#define PCI_FUNCTION_ID         0xB100

/*
** PCI Function List.
*/
#define PCI_BIOS_PRESENT        0xB101
#define FIND_PCI_DEVICE         0xB102
#define FIND_PCI_CLASS_CODE     0xB103
#define GENERATE_SPECIAL_CYCLE  0xB106
#define READ_CONFIG_BYTE        0xB108
#define READ_CONFIG_WORD        0xB109
#define READ_CONFIG_DWORD       0xB10A
#define WRITE_CONFIG_BYTE       0xB10B
#define WRITE_CONFIG_WORD       0xB10C
#define WRITE_CONFIG_DWORD      0xB10D

/*
** PCI Return Code List.
*/
#define SUCCESSFUL         	0x00
#define FUNC_NOT_SUPPORTED      0x81
#define BAD_VENDOR_ID           0x83
#define DEVICE_NOT_FOUND        0x86
#define BAD_REGISTER_NUMBER     0x87



/**********************************************************
**
**          our PCI Information
*/

/*
** PCI Bus configuration space registers.
*/
typedef struct PCI_Config_Registers
{
    u_short Device_Id;
    u_short Vendor_Id;      /* 00 */

    u_short Status;
    u_short Command;        /* 04 */

    u_char  Class_Code[3];
    u_char  Rev_Id;         /* 08 */


    u_char  Bits;
    u_char  Header_Type;
    u_char  Latency_Timer;
    u_char  Cache_Line_Size;    /* 0C */


    u_int   IO_Base_Address;    /* 10 */

    u_int   Memory_Base_Address;    /* 14 */

    u_int   Not_Used1[6];       /* 18 - 2F */

    u_int   ROM_Base_Address;   /* 30 */

    u_int   Not_Used2[2];       /* 34 - 3B */

    u_char  Interrupt_Line;     /* 3C */
    u_char  Interrupt_Pin;
    u_char  Minimum_Grant;
    u_char  Maximum_Latency;

    u_short ISP_Config_1;       /* 40 */
    u_short Not_Used3;

} PCI_REG, *pPCI_REG;


/*
** Configuration space offsets:
*/

#define CONF_VENDOR_ID_OFFSET           0x00
#define     QLogic_VENDOR_ID            0x1077

#define CONF_DEVICE_ID_OFFSET           0x02
#define     QLogic_1040_DEVICE_ID       0x1020
#define	    QLogic_1240_DEVICE_ID	0x1240
#define	    QLogic_1080_DEVICE_ID	0x1080
#define     QLogic_1280_DEVICE_ID	0x1280

#define CONF_COMMAND_OFFSET         	0x04
#define     CONF_COMMAND_VALUE          0x0156

/*
**  0x0156 =    IO Space Disabled
**          Memory Space Enabled
**          PCI Master Capable
**          Memory Write & Invalidate Enabled
**          Parity Error Response Enabled
**          SERR# Enabled
**          Back to Back Transfers Disabled
*/

#define CONF_IO_BASE_OFFSET         0x10
#define CONF_MEM_BASE_OFFSET        0x14
#define MEMORY_SPACE_INDICATOR_MASK 0x00000001
#define IO_BASE_MASK                0xF800
#define MEMORY_BASE_MASK            0xFFFFF800


#define CONF_INTERRUPT_LINE_OFFSET  0x3C


typedef struct
{
	int commands_issued;
	int responses_handled;	
	int errors;
} dev_stats;


/*
** Structure to return for IOCSTATS.
*/
typedef struct stats {
int commands;            /* # of commands processed */
int responses;           /* # of responses processed */
int interrupts;          /* # of interrupts processed */
int intrs_processed;     /* # of interrupts ACTUALLY processed */
int responses_handled;   /* # IORB processed */
int markers;         	 /* # of markers issued */
dev_stats dev[QL_MAXTARG];

} stats, *ha_stats;

struct irix5_stats {
app32_int_t  commands;            /* # of commands processed */
app32_int_t  responses;           /* # of responses processed */
app32_int_t  interrupts;          /* # of interrupts processed */
app32_int_t  intrs_processed;     /* # of interrupts ACTUALLY processed */
app32_int_t  responses_handled;    /* # IORB processed */
app32_int_t  markers;         /* # of markers issued */
};

/*
** Structure to peek and poke risc memory.
*/
struct addr_data {
    unsigned short address;     /* word address in risc memory   */
    unsigned short data;        /* word data to write (if write) */
};


/*
 * info specific to the ql lun vertex
 */

typedef struct ql_local_info {
	mutex_t			qli_open_mutex;	/* open lock */
	mutex_t			qli_lun_mutex;	/* LUN lock */
	u_int			qli_dev_flags;	/* lun device flags */
	int			qli_ref_count;	/* no. of references */
	int                     qli_cmd_rcnt;   /* issued command count to this LUN */
	int                     qli_cmd_awcnt;  /* abort waiting command count for this LUN */
	int                     qli_cmd_iwcnt;  /* init waiting command count for this LUN */
	scsi_request_t*         qli_awaitf;     /* forward pointer for commands awaiting abort completion */
	scsi_request_t*         qli_awaitb;     /* backward pointer for commands awaiting abort completion */
	scsi_request_t*         qli_iwaitf;     /* forward pointer for commands awaiting init. completion */
	scsi_request_t*         qli_iwaitb;     /* backward pointer for commands awaiting init. completion */
	scsi_target_info_t	*qli_tinfo;	/* scsi target info */
	void                   (*qli_sense_callback)();	/* Sense callback routine */
} ql_local_info_t;
	
/*
 * macros associated with this structure	
 */
#define QLI_DEVFLAGS(p)		(p->qli_dev_flags)
#define QLI_OPENMUTEX(p)	(p->qli_open_mutex)
#define QLI_LUNMUTEX(p)	        (p->qli_lun_mutex)
#define QLI_REFCOUNT(p)		(p->qli_ref_count)
#define QLI_TINFO(p)		(p->qli_tinfo)

/*
 * macros associated with the scsi lun info structure specific
 * to ql
 */

#define QL_DEVFLAGS(p)		QLI_DEVFLAGS(((ql_local_info_t *)p))
#define QL_OPENMUTEX(p)		QLI_OPENMUTEX(((ql_local_info_t *)p))
#define QL_LUNMUTEX(p)		QLI_LUNMUTEX(((ql_local_info_t *)p))
#define QL_REFCOUNT(p)		QLI_REFCOUNT(((ql_local_info_t *)p))
#define QL_TINFO(p)		QLI_TINFO(((ql_local_info_t *)p))

#define DEV_FLAGS(p)		QL_DEVFLAGS(SLI_INFO(p))
#define OPEN_MUTEX(p)		QL_OPENMUTEX(SLI_INFO(p))
#define LUN_MUTEX(p)		QL_LUNMUTEX(SLI_INFO(p))
#define REF_COUNT(p)		QL_REFCOUNT(SLI_INFO(p))
#define TINFO(p)		QL_TINFO(SLI_INFO(p))

#define SCI_HA_INFO(p)		((pHA_INFO)(SCI_INFO(p)))
#define SLI_HA_INFO(p)		((pHA_INFO)(SCI_INFO(SLI_CTLR_INFO(p))))

#endif /* KERNEL */

/*
** Some miscellaneous configuration parameters.
*/

#define QL_RESET 	1000
#define QL_COMPUTE_PARITY_MEMORY	4000
#define WATCHDOG_TIME      10  /* for bringup set to 10 */
#define SCSI_INFO_BYTE          7
#define SCSI_CTQ_BIT            2

/*
 * Encoding of GPIO bits on channel 0 - MSCSI/A
 * [3](r)    [2](r)    [1](w)    [0](w)
 * 0 = in slot XIO1      
 * 1 = not in slot XIO1              
 *           diffsense 
 *                     0 = no internal terminator
 *                     1 = internal terminator
 *                               0 = internal bus selected
 *                               1 = external bus selected
 * 
 * Encoding of GPIO bits on channel 0 - MSCSI/B
 * [3](r)    [2](r)    [1](w)    [0](w)
 * 0 = in slot XIO1      
 * 1 = not in slot XIO1              
 *           not connected
 *                     0         0 = Internal SE selected
 *                     0         1 = External SE selected
 *                     1         0 = External DF (no termination)
 *                     1         1 = External DF (termination)
 *
 */
#define GPIO0             0x01
#define GPIO1             0x02
#define GPIO2             0x04
#define GPIO3             0x08
#define ENABLE_WRITE      (GPIO0 | GPIO1)
#define ENABLE_WRITE_SN00 0x01
#define NOT_SLOT_ZERO     GPIO3

/*
 * The IOCB handle is comprised of the following
 *        15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
 *        -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
 *                 PP UU UU UU UU TT TT TT TT TT TT TT TT
 *  where PP = port number; 0-1
 *        UU = unit number; 0-15
 *        TT = command index; 0-254
 */
#define MAX_HANDLE_NO (0x1000 | ((QL_MAXTARG - 1) * (MAX_REQ_INFO + 1)) | MAX_REQ_INFO)

/*
#define MAX_HANDLE_NO ((QL_MAXTARG * (MAX_REQ_INFO +1)) | MAX_REQ_INFO)
*/

#define QLPRI 0         /* Doesn't matter, since pri isn't used anymore */

#define INITLOCK(l,n,s) init_mutex(l, MUTEX_DEFAULT, n, s)
#define LOCK(l)         mutex_lock(&(l), QLPRI)
#define UNLOCK(l)       mutex_unlock(&(l))
#define IOLOCK(l,s)     mutex_lock(&(l), QLPRI)
#define IOUNLOCK(l,s)   mutex_unlock(&(l))
#define SPL_DECL(s)

#define QL_MUTEX_ENTER(chip) IOLOCK(chip->res_lock,s), \
                           IOLOCK(chip->req_lock,s)

#define QL_MUTEX_EXIT(chip)  IOUNLOCK(chip->res_lock,s), \
                           IOUNLOCK(chip->req_lock,s)


