/* scsi1Lib.h - SCSI library header file */

/* Copyright 1984-1994 Wind River Systems, Inc. */

/*
modification history
--------------------
02k,10oct94,jds  fixed for SCSI1 and SCSI2 compatability
02j,11jan93,ccc  added #ifndef _ASMLANGUAGE.
02i,30sep92,ccc  added two new errno's.
02h,24sep92,ccc  removed timeout parameter to scsiAutoConfig().
02g,22sep92,rrr  added support for c++
02f,27aug92,ccc  added timeout to SCSI_TRANSACTION structure.
02e,26jul92,rrr  Removed decl of scsiSyncTarget, it was made LOCAL.
02d,20jul92,eve  Remove conditional compilation.
02c,14jul92,eve  added a pScsiXaction info in SCSI_PHYS_DEV structure to
		 maintain the data direction information to support
		 cache coherency in drivers.
02b,06jul92,eve  added declaration for extended sync functionalities.
02a,04jul92,jcf  cleaned up.
01l,26may92,rrr  the tree shuffle
01k,07apr92,yao  changed BYTE_ORDER to _BYTE_ORDER, BIG_ENDIAN to _BIG_ENDIAN.
01j,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed READ, WRITE and UPDATE to O_RDONLY O_WRONLY O_RDWR
		  -changed VOID to void
		  -changed copyright notice
01i,07mar91,jcc  added SCSI_BLK_DEV_LIST and SCSI_BLK_DEV_NODE definitions;
		 added SCSI_SWAB macro. added a few error codes.
01h,05oct90,shl  added ANSI function prototypes.
                 added copyright notice.
01g,02oct90,jcc  changed SEM_ID's to SEMAPHORE's in SCSI_PHYS_DEV and SCSI_CTRL
		 structures; modified structures to return variable length
		 REQUEST SENSE data; miscellaneous.
01f,20aug90,jcc  added conditional defines and typedefs for 4.0.2 compatibility.
01e,10aug90,dnw  changed scsiBusReset and scsiSelTimeOutCvt to VOIDFUNCPTR.
01d,07aug90,shl  moved function declarations to end of file.
01c,12jul90,jcc  misc. enhancements and changes.
01b,08jun90,jcc  added scsiMsgInAck to SCSI_CTRL, which routine is invoked
		 to accept incoming messages.
01a,17apr89,jcc  written.
*/

#ifndef __INCscsi1Libh
#define __INCscsi1Libh

#ifndef	_ASMLANGUAGE

#ifdef __cplusplus
extern "C" {
#endif

#include "vwmodnum.h"
#include "blkio.h"
#include "lstlib.h"
#include "private/semlibp.h"

#ifndef WAIT_FOREVER    /* the following is for 4.0.2 backward compatibility */

#define WAIT_FOREVER    (-1)
typedef void            (*VOIDFUNCPTR) ();  /* ptr to function returning void */
typedef unsigned char   UINT8;
typedef unsigned short  UINT16;

#endif	/* WAIT_FOREVER */

/* SCSI direct-access device and general purpose command opcodes are enumerated.
 * NOTE: Some commands are optional and may not be supported by all devices.
 */

#define SCSI_OPCODE_TEST_UNIT_READY     ((UINT8) 0x00)  /* test unit ready */
#define SCSI_OPCODE_REZERO_UNIT         ((UINT8) 0x01)  /* rezero unit */
#define SCSI_OPCODE_REWIND		((UINT8) 0x01)  /* rewind */
#define SCSI_OPCODE_REQUEST_SENSE       ((UINT8) 0x03)  /* request sense */
#define SCSI_OPCODE_FORMAT_UNIT         ((UINT8) 0x04)  /* format unit */
#define SCSI_OPCODE_READ_BLOCK_LIMITS	((UINT8) 0x05)  /* read block limits */
#define SCSI_OPCODE_REASSIGN_BLOCKS     ((UINT8) 0x07)  /* reassign blocks */
#define SCSI_OPCODE_READ                ((UINT8) 0x08)  /* read */
#define SCSI_OPCODE_RECEIVE		((UINT8) 0x08)  /* receive */
#define SCSI_OPCODE_WRITE               ((UINT8) 0x0a)  /* write */
#define SCSI_OPCODE_PRINT               ((UINT8) 0x0a)  /* print */
#define SCSI_OPCODE_SEND		((UINT8) 0x0a)  /* send */
#define SCSI_OPCODE_SEEK                ((UINT8) 0x0b)  /* seek */
#define SCSI_OPCODE_TRACK_SELECT	((UINT8) 0x0b)  /* track select */
#define SCSI_OPCODE_SLEW_AND_PRINT	((UINT8) 0x0b)  /* slew and print */
#define SCSI_OPCODE_READ_REVERSE	((UINT8) 0x0f)  /* read reverse */
#define SCSI_OPCODE_WRITE_FILEMARKS	((UINT8) 0x10)  /* write filemarks */
#define SCSI_OPCODE_FLUSH_BUFFER	((UINT8) 0x10)  /* flush buffer */
#define SCSI_OPCODE_SPACE		((UINT8) 0x11)  /* space */
#define SCSI_OPCODE_INQUIRY             ((UINT8) 0x12)  /* inquiry */
#define SCSI_OPCODE_VERIFY_SEQ		((UINT8) 0x13)  /* seq. access verify */
#define SCSI_OPCODE_RECOVER_BUF_DATA	((UINT8) 0x14)  /* rec. buffered data */
#define SCSI_OPCODE_MODE_SELECT         ((UINT8) 0x15)  /* mode select */
#define SCSI_OPCODE_RESERVE             ((UINT8) 0x16)  /* reserve */
#define SCSI_OPCODE_RELEASE             ((UINT8) 0x17)  /* release */
#define SCSI_OPCODE_COPY		((UINT8) 0x18)  /* copy */
#define SCSI_OPCODE_ERASE		((UINT8) 0x19)  /* erase */
#define SCSI_OPCODE_MODE_SENSE          ((UINT8) 0x1a)  /* mode sense */
#define SCSI_OPCODE_START_STOP_UNIT     ((UINT8) 0x1b)  /* start/stop unit */
#define SCSI_OPCODE_LOAD_UNLOAD		((UINT8) 0x1b)  /* load/unload */
#define SCSI_OPCODE_STOP_PRINT		((UINT8) 0x1b)  /* stop print */
#define SCSI_OPCODE_RECV_DIAG_RESULTS	((UINT8) 0x1c)  /* recv diag results */
#define SCSI_OPCODE_SEND_DIAGNOSTIC     ((UINT8) 0x1d)  /* send diagnostic */
#define SCSI_OPCODE_CTRL_MEDIUM_REMOVAL	((UINT8) 0x1e)  /* ctrl med. removal */
#define SCSI_OPCODE_READ_CAPACITY       ((UINT8) 0x25)  /* read capacity */
#define SCSI_OPCODE_READ_EXT		((UINT8) 0x28)  /* read extended */
#define SCSI_OPCODE_WRITE_EXT      	((UINT8) 0x2a)  /* write extended */
#define SCSI_OPCODE_SEEK_EXT       	((UINT8) 0x2b)  /* seek extended */
#define SCSI_OPCODE_WRITE_AND_VERIFY    ((UINT8) 0x2e)  /* write and verify */
#define SCSI_OPCODE_VERIFY              ((UINT8) 0x2f)  /* verify */
#define SCSI_OPCODE_SEARCH_DATA_HIGH	((UINT8) 0x30)  /* search data high */
#define SCSI_OPCODE_SEARCH_DATA_EQUAL	((UINT8) 0x31)  /* search data equal */
#define SCSI_OPCODE_SEARCH_DATA_LOW	((UINT8) 0x32)  /* search data low */
#define SCSI_OPCODE_SET_LIMITS		((UINT8) 0x33)  /* set limits */
#define SCSI_OPCODE_READ_DEFECT_DATA    ((UINT8) 0x37)  /* read defect data */
#define SCSI_OPCODE_COMPARE    		((UINT8) 0x39)  /* compare */
#define SCSI_OPCODE_COPY_AND_VERIFY	((UINT8) 0x3a)  /* copy and verify */
#define SCSI_OPCODE_WRITE_BUFFER        ((UINT8) 0x3b)  /* write buffer */
#define SCSI_OPCODE_READ_BUFFER         ((UINT8) 0x3c)  /* read buffer */

/* SCSI general purpose sense keys are enumerated.
 * NOTE: Some sense keys are optional and may not be supported by all devices.
*/

#define SCSI_SENSE_KEY_NO_SENSE         0x00  /* no sense sense key */
#define SCSI_SENSE_KEY_RECOVERED_ERROR  0x01  /* recovered error sense key */
#define SCSI_SENSE_KEY_NOT_READY        0x02  /* not ready sense key */
#define SCSI_SENSE_KEY_MEDIUM_ERROR     0x03  /* medium error sense key */
#define SCSI_SENSE_KEY_HARDWARE_ERROR   0x04  /* hardware error sense key */
#define SCSI_SENSE_KEY_ILLEGAL_REQUEST  0x05  /* illegal request sense key */
#define SCSI_SENSE_KEY_UNIT_ATTENTION   0x06  /* unit attention sense key */
#define SCSI_SENSE_KEY_DATA_PROTECT     0x07  /* data protect sense key */
#define SCSI_SENSE_KEY_BLANK_CHECK      0x08  /* blank check sense key */
#define SCSI_SENSE_KEY_VENDOR_UNIQUE    0x09  /* vendor unique sense key */
#define SCSI_SENSE_KEY_COPY_ABORTED     0x0a  /* copy aborted sense key */
#define SCSI_SENSE_KEY_ABORTED_COMMAND  0x0b  /* aborted command sense key */
#define SCSI_SENSE_KEY_EQUAL            0x0c  /* key equal sense key */
#define SCSI_SENSE_KEY_VOLUME_OVERFLOW  0x0d  /* volume overflow sense key */
#define SCSI_SENSE_KEY_MISCOMPARE       0x0e  /* miscompare sense key */
#define SCSI_SENSE_KEY_RESERVED         0x0f  /* reserved sense key */

/* SCSI status byte codes are enumerated.
 * NOTE: Some status codes are optional and may not be supported by all devices.
*/

#define SCSI_STATUS_MASK            ((UINT8) 0x1e) /* mask vend uniq status */
#define SCSI_STATUS_GOOD            ((UINT8) 0x00) /* good status */
#define SCSI_STATUS_CHECK_CONDITION ((UINT8) 0x02) /* check condition status */
#define SCSI_STATUS_CONDITION_MET   ((UINT8) 0x04) /* condition met/good stat */
#define SCSI_STATUS_BUSY            ((UINT8) 0x08) /* busy status */
#define SCSI_STATUS_INTMED_GOOD     ((UINT8) 0x10) /* intermediate/good stat */
#define SCSI_STATUS_INTMED_COND_MET ((UINT8) 0x14) /* interm./cond. met stat */
#define SCSI_STATUS_RESERV_CONFLICT ((UINT8) 0x18) /* reservation conflict */
#define SCSI_STATUS_QUEUE_FULL      ((UINT8) 0x28) /* queue full status */

/* SCSI message codes are enumerated.
 * NOTE: Some message codes are optional and may not be supported by all
 *       devices.
*/

#define SCSI_MSG_COMMAND_COMPLETE   ((UINT8) 0x00) /* command complete msg. */
#define SCSI_MSG_EXTENDED_MESSAGE   ((UINT8) 0x01) /* extended message msg. */
#define SCSI_MSG_SAVE_DATA_POINTER  ((UINT8) 0x02) /* save data pointer msg. */
#define SCSI_MSG_RESTORE_POINTERS   ((UINT8) 0x03) /* restore pointers msg. */
#define SCSI_MSG_DISCONNECT         ((UINT8) 0x04) /* disconnect msg. */
#define SCSI_MSG_INITOR_DETECT_ERR  ((UINT8) 0x05) /* initor. detect err msg. */
#define SCSI_MSG_ABORT              ((UINT8) 0x06) /* abort msg. */
#define SCSI_MSG_MESSAGE_REJECT     ((UINT8) 0x07) /* message reject msg. */
#define SCSI_MSG_NO_OP              ((UINT8) 0x08) /* no operation msg. */
#define SCSI_MSG_MSG_PARITY_ERR     ((UINT8) 0x09) /* message parity err msg. */
#define SCSI_MSG_LINK_CMD_COMPLETE  ((UINT8) 0x0a) /* linked cmd. comp. msg. */
#define SCSI_MSG_LINK_CMD_FLAG_COMP ((UINT8) 0x0b) /* link cmd w/flag comp. */
#define SCSI_MSG_BUS_DEVICE_RESET   ((UINT8) 0x0c) /* bus device reset msg. */
#define SCSI_MSG_IDENT_DISCONNECT   ((UINT8) 0x40) /* identify disconnect bit */
#define SCSI_MSG_IDENTIFY 	    ((UINT8) 0x80) /* identify msg bit mask */

/* SCSI extended message codes */

#define SCSI_EXT_MSG_MODIFY_DATA_PTR ((UINT8) 0x00)  /* modify data pointer */
#define SCSI_EXT_MSG_SYNC_XFER_REQ   ((UINT8) 0x01)  /* sync. data xfer. req. */
#define SCSI_EXT_MSG_WIDE_XFER_REQ   ((UINT8) 0x03)  /* wide data xfer req. */

/* SCSI extended message lengths */

#define SCSI_MODIFY_DATA_PTR_MSG_LENGTH  5  /* modify data pointer */
#define SCSI_SYNC_XFER_REQ_MSG_LENGTH    3  /* sync. data xfer. req. */
#define SCSI_WIDE_XFER_REQ_MSG_LENGTH    2  /* wide data xfer req. */

/* the largest SCSI ID and LUN a target drive can have */

#define SCSI_MIN_BUS_ID			0	/* min. bus ID under SCSI */
#define SCSI_MAX_BUS_ID			7	/* max. bus ID under SCSI */
#define SCSI_MIN_LUN			0	/* min. logical unit number */
#define SCSI_MAX_LUN			7	/* max. logical unit number */

/* scsiLib errno's */

#define S_scsiLib_DEV_NOT_READY		(M_scsiLib | 1)
#define S_scsiLib_WRITE_PROTECTED	(M_scsiLib | 2)
#define S_scsiLib_MEDIUM_ERROR		(M_scsiLib | 3)
#define S_scsiLib_HARDWARE_ERROR	(M_scsiLib | 4)
#define S_scsiLib_ILLEGAL_REQUEST	(M_scsiLib | 5)
#define S_scsiLib_BLANK_CHECK		(M_scsiLib | 6)
#define S_scsiLib_ABORTED_COMMAND	(M_scsiLib | 7)
#define S_scsiLib_VOLUME_OVERFLOW	(M_scsiLib | 8)
#define S_scsiLib_UNIT_ATTENTION	(M_scsiLib | 9)
#define S_scsiLib_SELECT_TIMEOUT	(M_scsiLib | 10)
#define S_scsiLib_LUN_NOT_PRESENT	(M_scsiLib | 11)
#define S_scsiLib_ILLEGAL_BUS_ID	(M_scsiLib | 12)
#define S_scsiLib_NO_CONTROLLER		(M_scsiLib | 13)
#define S_scsiLib_REQ_SENSE_ERROR	(M_scsiLib | 14)
#define S_scsiLib_DEV_UNSUPPORTED	(M_scsiLib | 15)
#define S_scsiLib_ILLEGAL_PARAMETER	(M_scsiLib | 16)
#define S_scsiLib_EARLY_PHASE_CHANGE	(M_scsiLib | 17)
#define S_scsiLib_PHASE_CHANGE_TIMEOUT	(M_scsiLib | 18)
#define S_scsiLib_ILLEGAL_OPERATION	(M_scsiLib | 19)
#define S_scsiLib_DEVICE_EXIST          (M_scsiLib | 20)
#define S_scsiLib_SYNC_UNSUPPORTED      (M_scsiLib | 21)
#define S_scsiLib_SYNC_VAL_UNSUPPORTED  (M_scsiLib | 22)
#define	S_scsiLib_DATA_TRANSFER_TIMEOUT	(M_scsiLib | 23)
#define	S_scsiLib_UNKNOWN_PHASE		(M_scsiLib | 24)


					/* default select timeout (usec) */
#define SCSI_DEF_SELECT_TIMEOUT		((UINT) 250000)

					/* default auto-config timeout (usec) */
#define SCSI_DEF_CONFIG_TIMEOUT		((UINT) 1000)

#define	SCSI_TIMEOUT_5SEC		((UINT) 5000000)
#define	SCSI_TIMEOUT_1SEC		((UINT) 1000000)
#define	SCSI_TIMEOUT_FULL		((UINT) 0x7fffffff)

				/* default bus ID for a vxWorks SCSI port */

#define SCSI_DEF_CTRL_BUS_ID		7

/* values in status register for SCSI bus phases */

#define SCSI_DATA_OUT_PHASE		0x0
#define SCSI_DATA_IN_PHASE		0x1
#define SCSI_COMMAND_PHASE		0x2
#define SCSI_STATUS_PHASE		0x3
#define SCSI_MSG_OUT_PHASE		0x6
#define SCSI_MSG_IN_PHASE		0x7
#define SCSI_BUS_FREE_PHASE		0x8

/* device status enumeration */

typedef enum scsiPhysDevStatus
    {
    IDLE,
    SELECT_REQUESTED,
    SELECT_IN_PROGRESS,
    SELECT_SUCCESSFUL,
    SELECT_TIMEOUT,
    SELECT_LOST_ARBIT,
    DISCONNECTED,
    RECONNECTED
    } SCSI_DEV_STATUS;

/* device type enumeration */

#define SCSI_DEV_DIR_ACCESS	((UINT8) 0x00) /* direct-access dev (disk) */
#define SCSI_DEV_SEQ_ACCESS	((UINT8) 0x01) /* sequent'l-access dev (tape) */
#define SCSI_DEV_PRINTER	((UINT8) 0x02) /* printer dev */
#define SCSI_DEV_PROCESSOR	((UINT8) 0x03) /* processor dev */
#define SCSI_DEV_WORM		((UINT8) 0x04) /* write-once read-mult dev */
#define SCSI_DEV_RO_DIR_ACCESS	((UINT8) 0x05) /* read-only direct-access dev */
#define SCSI_LUN_NOT_PRESENT	((UINT8) 0x7f) /* logical unit not present */

#define MAX_SCSI_PHYS_DEVS		64
#define MAX_MSG_IN_BYTES		8
#define MAX_MSG_OUT_BYTES		8

#define VENDOR_ID_LENGTH		8
#define PRODUCT_ID_LENGTH		16
#define REV_LEVEL_LENGTH		4

typedef struct scsiBlkDevList
    {
    SEMAPHORE listMutexSem;
    LIST blkDevNodes;
    } SCSI_BLK_DEV_LIST;

/* structure to pass to ioctl call to execute an SCSI command*/

typedef struct  /* SCSI_TRANSACTION - information about a SCSI transaction */
    {
    UINT8 *cmdAddress;          /* address of SCSI command bytes */
    int cmdLength;              /* length of command in bytes */
    UINT8 *dataAddress;         /* address of data buffer */
    int dataDirection;          /* direction of data transfer */
    int dataLength;             /* length of data buffer in bytes (0=no data) */    int addLengthByte;          /* if != -1 , byte # of additional length */
    UINT8 statusByte;           /* status byte returned from target */
    int cmdTimeout;		/* number of usec for this command timeout */
    } SCSI_TRANSACTION;

/* NOTE: Not all of the fields in the following structure
 *       are currently supported or used.
 */

typedef struct 			/* SCSI_PHYS_DEV - SCSI physical device info */
    {
    SEMAPHORE devMutexSem;	/* semaphore for exclusive access */
    SEMAPHORE devSyncSem;	/* semaphore for waiting on I/O interrupt */
    int scsiDevBusId;		/* device's address on SCSI bus */
    int scsiDevLUN;		/* device's logical unit number */
    UINT selTimeOut;		/* device select time-out (units var.) */
    UINT8 scsiDevType;		/* SCSI device type */
    TBOOL disconnect;           /* whether device supports disconnect */
    TBOOL expectDisconnect;     /* TRUE if a disconnect is expected */
    TBOOL xmitsParity;     	/* TRUE if dev. xmits parity */
    TBOOL useIdentify;		/* whether to use identify message at select */
    TBOOL syncXfer;		/* TRUE for synchronous data transfer */
    int syncXferOffset;		/* synchronous xfer maximum offset */
    int syncXferPeriod;		/* synchronous xfer mimimum period */
    UINT8 *savedTransAddress;   /* saved data pointer address */
    int savedTransLength;       /* saved data pointer length */
    BOOL resetFlag;		/* set TRUE when dev reset sensed */
    FUNCPTR postResetRtn;	/* routine to call when dev has been reset */

    char devVendorID [VENDOR_ID_LENGTH + 1]; 	/* vendor ID in ASCII */
    char devProductID [PRODUCT_ID_LENGTH + 1];	/* product ID in ASCII */
    char devRevLevel [REV_LEVEL_LENGTH + 1];	/* revision level in ASCII */

    BOOL removable;		/* whether medium is removable */
    UINT8 lastSenseKey;		/* last sense key returned by dev */
    UINT8 lastAddSenseCode;	/* last additional sense code returned by dev */
    int controlByte;		/* vendor unique control byte for commands */
    int numBlocks;		/* number of blocks on the physical device */
    int blockSize;		/* size of an SCSI disk sector */
    struct scsiCtrl *pScsiCtrl;	/* ptr to dev's SCSI controller info */
    SCSI_DEV_STATUS devStatus;	/* status of most recent device operation */
    				/* list of outgoing messages */
    UINT8 msgOutArray [MAX_MSG_OUT_BYTES + 1];/* msg[0] reserved for identify */
    int msgLength;		/* number of bytes in outgoing message */
    BOOL extendedSense;		/* whether device returns extended sense */
    UINT8 *pReqSenseData;	/* ptr to last REQ SENSE data returned by dev */
    int reqSenseDataLength;	/* size of REQ SENSE data array */
    				/* ptr to first block dev created on device */
    struct scsiBlkDevList blkDevList;
    struct scsiBlkDev *pScsiBlkDev;
    int useMsgout;              /* whether to send a message out at select */
    int syncReserved1;          /* Reserved for hardware sync register value */
    int syncReserved2;          /* Reserved for hardware sync register value */
    SCSI_TRANSACTION  *pScsiXaction;	/* current scsi action requested */
    } SCSI_PHYS_DEV;


/* Step scsi period ( look at the scsi ANSI ) */

#define SCSI_SYNC_STEP_PERIOD     4     /* scsi step period in sync msg */

/* Used to negotiate a scsi sync agreement for the synchronous transfert
 * to return parameters computed both by the driver and target
 */
typedef struct
     {
     int syncPeriodCtrl;     /* Closest period available with the controller */
     int syncOffsetCtrl;     /* Closest offset available with the controller */
     int syncPeriodTarget;   /* Period value returned by the target */
     int syncOffsetTarget;   /* Offset value returned by the target */
     } SCSI_SYNC_AGREEMENT;

typedef SCSI_PHYS_DEV *SCSI_PHYS_DEV_ID;

typedef struct scsiBlkDev	/* SCSI_BLK_DEV -
				   SCSI logical block device info */
    {
    BLK_DEV blkDev;		/* generic logical block device info */
    SCSI_PHYS_DEV *pScsiPhysDev;/* ptr to SCSI physical device info */
    int numBlocks;		/* number of blocks on the logical device */
    int blockOffset;		/* address of first block on logical device */
    } SCSI_BLK_DEV;

typedef SCSI_BLK_DEV *SCSI_BLK_DEV_ID;

typedef struct scsiBlkDevNode
    {
    NODE blkDevNode;
    SCSI_BLK_DEV scsiBlkDev;
    } SCSI_BLK_DEV_NODE;

typedef struct scsiCtrl		/* SCSI_CTRL - generic SCSI controller info */
    {
    SEMAPHORE ctrlMutexSem;	/* semaphore for exclusive access */
    SEMAPHORE ctrlSyncSem;	/* semaphore for waiting on I/O interrupt */
    VOIDFUNCPTR scsiBusReset;	/* function for resetting the SCSI bus */
    FUNCPTR scsiTransact;	/* function for managing a SCSI transaction */
    FUNCPTR scsiDevSelect;	/* function for selecting a SCSI device */
    FUNCPTR scsiBytesIn;	/* function for SCSI input */
    FUNCPTR scsiBytesOut;	/* function for SCSI output */
    FUNCPTR scsiDmaBytesIn;	/* function for SCSI DMA input */
    FUNCPTR scsiDmaBytesOut;	/* function for SCSI DMA output */
    FUNCPTR scsiBusPhaseGet;	/* function returning the current bus phase */
    FUNCPTR scsiMsgInAck;	/* function for accepting an incoming message */
    VOIDFUNCPTR scsiSelTimeOutCvt; /* func. for converting a select time-out */
    UINT maxBytesPerXfer;	/* upper bound of ctrl. tansfer counter */
    UINT clkPeriod;		/* period of the controller clock (nsec) */
    int scsiCtrlBusId;		/* SCSI bus ID of this SCSI controller */
    int scsiPriority;		/* priority of task when doing SCSI I/O */
    int scsiBusPhase;           /* current phase of SCSI */
    TBOOL disconnect;		/* globally enable / disable disconnect */
				/* info on devs. attached to this controller */
    SCSI_PHYS_DEV *physDevArr [MAX_SCSI_PHYS_DEVS];
    FUNCPTR scsiSyncMsgConvert; /* function for converting extended sync
                                 * message in hardware register value */
    VOIDFUNCPTR scsiSetAtn;     /* function to set atn */
    } SCSI_CTRL;

/* definitions relating to SCSI commands */

#define MAX_SCSI_CMD_LENGTH 12  /* maximum length in bytes of a SCSI command */

typedef UINT8 SCSI_COMMAND [MAX_SCSI_CMD_LENGTH];

#define SCSI_GROUP_0_CMD_LENGTH  6
#define SCSI_GROUP_1_CMD_LENGTH 10
#define SCSI_GROUP_5_CMD_LENGTH 12

/* some useful defines and structures for CCS commands */

#define SCSI_FORMAT_DATA_BIT		((UINT8) 0x10)
#define SCSI_COMPLETE_LIST_BIT		((UINT8) 0x08)

#define DEFAULT_INQUIRY_DATA_LENGTH	36
#define REQ_SENSE_ADD_LENGTH_BYTE	7
#define INQUIRY_ADD_LENGTH_BYTE		4
#define MODE_SENSE_ADD_LENGTH_BYTE	0
#define INQUIRY_REMOVABLE_MED_BIT	((UINT8) 0x80)

typedef struct  /* RD_CAP_DATA - results from O_RDONLY CAPACITY */
    {
    int lastLogBlkAdrs;		/* address of last logical block on device */
    int blkLength;		/* block length of last logical block */
    } RD_CAP_DATA;

#define NON_EXT_SENSE_DATA_LENGTH	4

#define SCSI_SENSE_DATA_CLASS		0x70
#define SCSI_SENSE_DATA_CODE		0x0f

#define SCSI_SENSE_KEY_MASK		0x0f

#define SCSI_EXT_SENSE_CLASS		0x70
#define SCSI_EXT_SENSE_CODE		0x00

/* Min/Max sync value in 4ns step */

#define MAX_SCSI_SYNC_PERIOD  0xfa            /* ~1Mbytes/s value */
#define MIN_SCSI_SYNC_PERIOD  0x19            /* ~10Mbytes/s value */
#define DEFAULT_SCSI_OFFSET   0x4             /* scsi default offset */

/* Type of msgout to send */
#define SCSI_NO_MSGOUT          0x00000000    /* No msgout capability */
#define SCSI_SYNC_MSGOUT        0x00000001    /* Build and send sync message */
#define SCSI_WIDE_MSGOUT        0x00000002    /* Build and send wide message */
#define SCSI_DEV_RST_MSGOUT     0x00000004    /* Build and send wide message */
#define SCSI_OTHER_MSGOUT       0x00000008    /* Send user message */

/* external variable declarations */

IMPORT BOOL            scsiDebug; 	/* enable task level debug messages */
IMPORT BOOL            scsiIntsDebug;	/* enable int level debug messages */
IMPORT SCSI_CTRL *     pSysScsiCtrl;
IMPORT SCSI_FUNC_TBL * pScsiIfTbl;      /* scsiLib interface function table */
IMPORT UINT32          scsiSelectTimeout;
IMPORT int             blkDevListMutexOptions;
IMPORT int             scsiCtrlMutexOptions;
IMPORT int             scsiCtrlSemOptions;
IMPORT int             scsiPhysDevMutexOptions;
IMPORT int             scsiPhysDevSemOptions;



/* macros */

#define SCSI_MSG							\
	logMsg

#define SCSI_DEBUG_MSG							\
    if (scsiDebug)							\
	SCSI_MSG

#define SCSI_INT_DEBUG_MSG						\
    if (scsiIntsDebug)							\
	logMsg

/* macros to get msgout pointer Identify and msg array */

#define SCSI_GET_PTR_IDENTIFY(pScsiPhysDev)    (&pScsiPhysDev->msgOutArray[0])
#define SCSI_GET_PTR_MSGOUT(pScsiPhysDev)      (&pScsiPhysDev->msgOutArray[1])

/*******************************************************************************
*
* SCSI_SWAB - swap bytes on little-endian machines
*
* All fields which are defined as integers (short or long) by the SCSI spec
* and which are passed during the data phase should be pre- or post-processed
* by this macro. The macro does nothing on big-endian machines, but swaps
* bytes for little-endian machines. All SCSI quantities are big-endian.
*
* NOMANUAL
*/

#if (_BYTE_ORDER == _BIG_ENDIAN)
#define SCSI_SWAB(pBuffer, bufLength)
#else
#define SCSI_SWAB(pBuffer, bufLength)					\
    do									\
	{								\
	char temp;							\
	char *pHead = (char *) pBuffer;					\
	char *pTail = (char *) pBuffer + bufLength - 1;			\
									\
	while (pHead < pTail)						\
	    {								\
	    temp = *pTail;						\
	    *pTail-- = *pHead;						\
	    *pHead++ = temp;						\
	    }								\
	} while (FALSE)
#endif


/* function declarations */

extern void   scsi1IfInit ();

#if defined(__STDC__) || defined(__cplusplus)

extern BLK_DEV *       scsiBlkDevCreate (SCSI_PHYS_DEV *, int, int);
extern SCSI_PHYS_DEV * scsiPhysDevCreate (SCSI_CTRL *, int, int, int, int, 
					 BOOL, int, int);
extern SCSI_PHYS_DEV * scsiPhysDevIdGet (SCSI_CTRL *, int, int);
extern STATUS 	       scsiAutoConfig (SCSI_CTRL *);
extern STATUS 	       scsiBusReset (SCSI_CTRL *);
extern STATUS 	       scsiCmdBuild (SCSI_COMMAND, int *, UINT8, int, BOOL, 
                                     int, int , UINT8);
extern STATUS 	       scsiCtrlInit (SCSI_CTRL *);
extern STATUS 	       scsiFormatUnit (SCSI_PHYS_DEV *, BOOL, int, int, int, 
                                       char *, int);
extern STATUS 	       scsiInquiry (SCSI_PHYS_DEV *, char *, int);
extern STATUS 	       scsiIoctl (SCSI_PHYS_DEV *, int, int);
extern STATUS 	       scsiModeSelect (SCSI_PHYS_DEV *, int, int, char *, int);
extern STATUS 	       scsiModeSense (SCSI_PHYS_DEV *, int, int, char *, int);
extern STATUS 	       scsiPhysDevDelete (SCSI_PHYS_DEV *);
extern STATUS 	       scsiRdSecs (SCSI_BLK_DEV *, int, int, char *);
extern STATUS 	       scsiReadCapacity (SCSI_PHYS_DEV *, int *, int *);
extern STATUS 	       scsiReqSense (SCSI_PHYS_DEV *, char *, int);
extern STATUS 	       scsiShow (SCSI_CTRL *);
extern STATUS 	       scsiTestUnitRdy (SCSI_PHYS_DEV *);
extern STATUS 	       scsiTransact (SCSI_PHYS_DEV *, SCSI_TRANSACTION *);
extern STATUS 	       scsiWrtSecs (SCSI_BLK_DEV *, int, int, char *);
extern char *	       scsiPhaseNameGet (int);
extern void 	       scsiBlkDevInit (SCSI_BLK_DEV *, int, int);
extern void 	       scsiBlkDevShow (SCSI_PHYS_DEV *);
extern STATUS          scsiBuildByteMsgOut (UINT8 *,UINT8);
extern STATUS          scsiBuildExtMsgOut (UINT8 *,UINT8);

#else	/* __STDC__ */

extern BLK_DEV *       scsiBlkDevCreate ();
extern SCSI_PHYS_DEV * scsiPhysDevCreate ();
extern SCSI_PHYS_DEV * scsiPhysDevIdGet ();
extern STATUS 	       scsiAutoConfig ();
extern STATUS 	       scsiBusReset ();
extern STATUS 	       scsiCmdBuild ();
extern STATUS 	       scsiCtrlInit ();
extern STATUS 	       scsiFormatUnit ();
extern STATUS 	       scsiInquiry ();
extern STATUS 	       scsiIoctl ();
extern STATUS 	       scsiModeSelect ();
extern STATUS 	       scsiModeSense ();
extern STATUS 	       scsiPhysDevDelete ();
extern STATUS 	       scsiRdSecs ();
extern STATUS 	       scsiReadCapacity ();
extern STATUS 	       scsiReqSense ();
extern STATUS 	       scsiShow ();
extern STATUS 	       scsiTestUnitRdy ();
extern STATUS 	       scsiTransact ();
extern STATUS 	       scsiWrtSecs ();
extern char *	       scsiPhaseNameGet ();
extern void 	       scsiBlkDevInit ();
extern void 	       scsiBlkDevShow ();
extern STATUS          scsiBuildByteMsgOut ();
extern STATUS          scsiBuildExtMsgOut ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* _ASMLANGUAGE */
#endif /* __INCscsi1Libh */
