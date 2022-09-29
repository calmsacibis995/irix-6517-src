/***************************************************************************
*                                                                          *
* Copyright 1993 Adaptec, Inc.,  All Rights Reserved.                      *
*                                                                          *
* This software contains the valuable trade secrets of Adaptec.  The       *
* software is protected under copyright laws as an unpublished work of     *
* Adaptec.  Notice is for informational purposes only and does not imply   *
* publication.  The user of this software may make copies of the software  *
* for use with parts manufactured by Adaptec or under license from Adaptec *
* and for no other use.                                                    *
*                                                                          *
***************************************************************************/

/*
 * $Id: him_scb.h,v 1.14 1999/08/19 03:07:45 gwh Exp $
 */

#define  REL_LEVEL      0x00        /* Current Release Level               */
                                    /* 0 = Prototype                       */
#define  REV_LEVEL      0x01        /* Current Revision Level              */


#include "osm.h"

#define  _OPTIMA

/****************************************************************************

 LANCE SCSI REGISTERS DEFINED AS AN ARRAY IN A STRUCTURE

****************************************************************************/

typedef struct aic7870_reg
{
   UBYTE aic7870[192];
}aic_7870;

#define  AIC7870 (OS_TYPE) &base->aic7870
#define  AIC_7870 aic_7870



/****************************************************************************
                    %%% Sequencer Control Block (SCB) %%%
****************************************************************************/
#define  MAX_CDB_LEN    12          /* Maximum length of SCSI CDB          */
#define  MAX_SCBS       255         /* Maximum number of possible SCBs     */
#define  INVALID_SCB_INDEX 255      /* was invalid_scbptr */
#define  MIN_SCB_LENGTH 20          /* Min. # of SCB bytes required by HW  */
#define  LOW_SCB_INDEX  1           /* Lowest legal SCB index              */
#define  HIGH_SCB_INDEX 254         /* Highest legal SCB index             */
                                    /*                                     */
#define  THRESHOLD      30          /* SCB(1-30) = 30 Non-Tag SCB's/Lance  */
                                    /*             MAXIMUM                 */
#define  MAX_INTRLOOP   15	    /* Max svc loops in ph_inthander_abn   */

      
typedef struct sp                   /* This is the SCB                     */
{                                   /*                                     */
   UBYTE Sp_scb_num;		    /* preassigned scb number              */
   UBYTE Sp_ctrl;		    /* controller to which scb belongs	   */
   union                            /*                                     */
   {                                /*                                     */
      struct cfp *ConfigPtr;        /* Pointer to config structure         */
      DWORD _void;                  /* Dword entry to align rest of struct */
   } Sp_config;                    /*                                     */
   struct                           /*                                     */
   {                                /*                                     */
      DWORD Cmd:8;                  /* SCB command type                    */
      DWORD Stat:8;                 /* SCB command status                  */
      UDWORD Rsvd1:1;               /* Reserved = 0                        */
      UDWORD NegoInProg:1;          /* Negotiation in progress             */
      UDWORD Rsvd2:2;               /* Reserved = 0                        */
      UDWORD OverlaySns:1;          /* 1 = Overlay SCB with Sense Data     */
      UDWORD NoNegotiation:1;       /* No negotiation for the scb          */
      UDWORD NoUnderrun:1;          /* 1 = Suppress underrun errors        */
      UDWORD AutoSense:1;           /* 1 = Automatic Request Sense enabled */
      DWORD MgrStat:8;              /* Intermediate status of SCB          */
   } Sp_control;                   /*                                     */
   union                            /* Sequencer SCSI Cntl Block (32 bytes)*/
   {                                /*                                     */
      DWORD seq_scb_array[8];       /* Double-word array for SCB download  */
#ifdef MIPS_BE
      struct                        /* Bit-wise definitions for HIM use    */
      {                             /*                                     */
	 /* this word field was switched */
         DWORD SegCnt:8;            /* Number of Scatter/Gather segments   */
         DWORD CDBLen:8;            /* Length of Command Descriptor Block  */
         UDWORD RejectMDP:1;        /* Non-contiguous data                 */
         UDWORD DisEnable:1;        /* Disconnection enabled               */
         UDWORD TagEnable:1;        /* Tagged Queuing enabled              */
         UDWORD Rsvd3:1;            /* Reserved = 0                        */
         UDWORD SpecFunc:1;         /* Defined for "special" opcode        */
         UDWORD Discon:1;           /* Target currently disconnected       */
         DWORD TagType:2;           /* Tagged Queuing type                 */
         DWORD Tarlun:8;            /* Target/Channel/Lun                  */

         DWORD SegPtr;              /* Pointer to Scatter/Gather list      */

         DWORD CDBPtr;              /* Pointer to Command Descriptor Block */

	 /* this word field was switched */
         DWORD Offshoot:8;          /* Pointer to offshoot SCB from chain  */
         DWORD NextPtr:8;           /* Pointer to next SCB to be executed  */
         DWORD Progress:4;          /* Progress count (2's complement)     */
         UDWORD Aborted:1;          /* Aborted SCB flag                    */
         UDWORD Abort:1;            /* Abort SCB flag                      */
         UDWORD Concurrent:1;       /* Cuncurrent SCB flag                 */
         UDWORD Holdoff:1;          /* Holdoff SCB flag                    */
         DWORD TargStat:8;          /* Target status (overlays 'sreentry') */

         DWORD ResCnt;              /* Residual byte count                 */

         DWORD Rsvd4;               /* Reserved = 0                        */

         DWORD Address;             /* Current host buffer address         */

	 /* this word field was switched */
         DWORD HaStat:8;            /* Host Adapter status                 */
         DWORD Length:24;           /* Current transfer length             */
      } seq_scb_struct;             /*                                     */
#else /* MIPS_BE */
      struct                        /* Bit-wise definitions for HIM use    */
      {                             /*                                     */
         DWORD Tarlun:8;            /* Target/Channel/Lun                  */
         DWORD TagType:2;           /* Tagged Queuing type                 */
         DWORD Discon:1;            /* Target currently disconnected       */
         DWORD SpecFunc:1;          /* Defined for "special" opcode        */
         DWORD Rsvd3:1;             /* Reserved = 0                        */
         DWORD TagEnable:1;         /* Tagged Queuing enabled              */
         DWORD DisEnable:1;         /* Disconnection enabled               */
         DWORD RejectMDP:1;         /* Non-contiguous data                 */
         DWORD CDBLen:8;            /* Length of Command Descriptor Block  */
         DWORD SegCnt:8;            /* Number of Scatter/Gather segments   */
         DWORD SegPtr;              /* Pointer to Scatter/Gather list      */
         DWORD CDBPtr;              /* Pointer to Command Descriptor Block */
         DWORD TargStat:8;          /* Target status (overlays 'sreentry') */
         DWORD Holdoff:1;           /* Holdoff SCB flag                    */
         DWORD Concurrent:1;        /* Cuncurrent SCB flag                 */
         DWORD Abort:1;             /* Abort SCB flag                      */
         DWORD Aborted:1;           /* Aborted SCB flag                    */
         DWORD Progress:4;          /* Progress count (2's complement)     */
         DWORD NextPtr:8;           /* Pointer to next SCB to be executed  */
         DWORD Offshoot:8;          /* Pointer to offshoot SCB from chain  */
         DWORD ResCnt;              /* Residual byte count                 */
         DWORD Rsvd4;               /* Reserved = 0                        */
         DWORD Address;             /* Current host buffer address         */
         DWORD Length:24;           /* Current transfer length             */
         DWORD HaStat:8;            /* Host Adapter status                 */
      } seq_scb_struct;             /*                                     */
#endif /* MIPS_BE */
   } Sp_seq_scb;                       /*                                     */
   DWORD Sp_SensePtr;               /* Mini-sg list: buf addr (phys)       */
   DWORD Sp_SenseLen;               /* Mini-sg list: buf len               */
   DWORD Sp_Sensesgp;		    /* pointer to sg list (phys)	   */
   OS_TYPE Sp_Sensebuf;		    /* pointer to sense buf (virtual)	   */
   UBYTE Sp_CDB[MAX_CDB_LEN];       /* SCSI Command Descriptor Block       */
   UBYTE Sp_ExtMsg[8];              /* Work area for extended messages     */
   UBYTE Sp_WideInProg;		    /* wide nego in prog		   */
   UBYTE Sp_dowide;		    /* target has agreed to do wide	   */
   UBYTE Sp_SyncInProg;		    /* sync nego in prog		   */
   UBYTE Sp_Reserved[2];            /* Reserved for future expansion       */
   OS_TYPE Sp_OSspecific;           /* Pointer to custom data structures   */
                                    /* required by O/S specific interface. */
} sp_struct;

#define  SP_ConfigPtr   Sp_config.ConfigPtr
#define  SP_Cmd         Sp_control.Cmd
#define  SP_Stat        Sp_control.Stat
#define  SP_NegoInProg  Sp_control.NegoInProg
#define  SP_Head        Sp_control.Head
#define  SP_Queued      Sp_control.Queued
#define  SP_OverlaySns  Sp_control.OverlaySns
#define  SP_NoNegotiation Sp_control.NoNegotiation
#define  SP_NoUnderrun  Sp_control.NoUnderrun
#define  SP_AutoSense   Sp_control.AutoSense
#define  SP_MgrStat     Sp_control.MgrStat

#define  SP_Tarlun      Sp_seq_scb.seq_scb_struct.Tarlun
#define  SP_TagType     Sp_seq_scb.seq_scb_struct.TagType
#define  SP_Discon      Sp_seq_scb.seq_scb_struct.Discon
#define  SP_Wait        Sp_seq_scb.seq_scb_struct.Wait
#define  SP_TagEnable   Sp_seq_scb.seq_scb_struct.TagEnable
#define  SP_SpecFunc    Sp_seq_scb.seq_scb_struct.SpecFunc
#define  SP_DisEnable   Sp_seq_scb.seq_scb_struct.DisEnable
#define  SP_RejectMDP   Sp_seq_scb.seq_scb_struct.RejectMDP
#define  SP_CDBLen      Sp_seq_scb.seq_scb_struct.CDBLen
#define  SP_SegCnt      Sp_seq_scb.seq_scb_struct.SegCnt
#define  SP_SegPtr      Sp_seq_scb.seq_scb_struct.SegPtr
#define  SP_CDBPtr      Sp_seq_scb.seq_scb_struct.CDBPtr
#define  SP_TargStat    Sp_seq_scb.seq_scb_struct.TargStat
#define  SP_Holdoff     Sp_seq_scb.seq_scb_struct.Holdoff 
#define  SP_Concurrent  Sp_seq_scb.seq_scb_struct.Concurrent
#define  SP_Abort       Sp_seq_scb.seq_scb_struct.Abort
#define  SP_Aborted     Sp_seq_scb.seq_scb_struct.Aborted
#define  SP_Progress    Sp_seq_scb.seq_scb_struct.Progress
#define  SP_NextPtr     Sp_seq_scb.seq_scb_struct.NextPtr
#define  SP_Offshoot    Sp_seq_scb.seq_scb_struct.Offshoot
#define  SP_ResCnt      Sp_seq_scb.seq_scb_struct.ResCnt
#define  SP_HaStat      Sp_seq_scb.seq_scb_struct.HaStat

/****************************************************************************
                        %%% Sp_Cmd values %%%
****************************************************************************/

#define  EXEC_SCB       0x02        /* Standard SCSI command               */
#define  READ_SENSE     0x01        /*                                     */
#define  SOFT_RST_DEV   0x03        /*                                     */
#define  HARD_RST_DEV   0x04        /*                                     */
#define  NO_OPERATION   0x00        /*                                     */

#define  BOOKMARK       0xFF        /* Used for hard host adapter reset */

#define  ABORT_SCB      0x00        /*                                     */
#define  SOFT_HA_RESET  0x01        /*                                     */
#define  HARD_HA_RESET  0x02        /*                                     */
#define  FORCE_RENEGOTIATE 0x03     /*                                     */

#define  REALIGN_DATA   0x04        /* Re-link config structures with him struct */
#define  BIOS_ON        0x05        /*                */
#define  BIOS_OFF       0x06        /*                */
#define  H_BIOS_OFF     0x10        /*                */

/****************************************************************************
                       %%% Sp_Stat values %%%
****************************************************************************/
#define  SCB_PENDING    0x00        /* SCSI request in progress            */
#define  SCB_COMP       0x01        /* SCSI request completed no error     */
#define  SCB_ABORTED    0x02        /* SCSI request aborted                */
#define  SCB_ERR        0x04        /* SCSI request completed with error   */

/****************************************************************************
                       %%% Sp_MgrStat values %%%
****************************************************************************/
#define  SCB_DONE       SCB_COMP    /* SCB finished without error          */
#define  SCB_DONE_ABT   SCB_ABORTED /* SCB finished due to abort from host */
#define  SCB_DONE_ERR   SCB_ERR     /* SCB finished with error             */
#define  SCB_READY      0x10        /* SCB ready to be loaded into Lance   */
#define  SCB_ACTIVE     0x40        /* SCB loaded into Lance               */
#define  SCB_AUTOSENSE  0x08        /* SCB w/autosense in progress */


/****************************************************************************
                   %%% SCB_Tarlun (SCB00) values %%%
****************************************************************************/
#define  TARGET_ID      0xf0        /* SCSI target ID (4 bits)             */
#define  CHANNEL        0x08        /* SCSI Bus selector: 0=chan A,1=chan B*/
#define  LUN            0x07        /* SCSI target's logical unit number   */

/****************************************************************************
                   %%% SCB_Cntrl (SCB01) values %%%
****************************************************************************/
#define  REJECT_MDP     0x80        /* Non-contiguous data                 */
#define  DIS_ENABLE     0x40        /* Disconnection enabled               */
#define  TAG_ENABLE     0x20        /* Tagged Queuing enabled              */
#ifdef   _STANDARD
#define  SWAIT          0x08        /* Sequencer trying to select target   */
#endif   /* _STANDARD */
#define  SDISCON        0x04        /* Traget currently disconnected       */
#define  TAG_TYPE       0x03        /* Tagged Queuing type                 */

/****************************************************************************
                     %%% Sp_TargStat values %%%
****************************************************************************/
#define  UNIT_GOOD      0x00        /* Good status or none available       */
#define  UNIT_CHECK     0x02        /* Check Condition                     */
#define  UNIT_MET       0x04        /* Condition met                       */
#define  UNIT_BUSY      0x08        /* Target busy                         */
#define  UNIT_INTERMED  0x10        /* Intermediate command good           */
#define  UNIT_INTMED_GD 0x14        /* Intermediate condition met          */
#define  UNIT_RESERV    0x18        /* Reservation conflict                */

/***************************************************************************
                      %%% Sp_HaStat values %%%
****************************************************************************/
#define  HOST_NO_STATUS 0x00        /* No adapter status available         */
#define  HOST_ABT_HOST  0x04        /* Command aborted by host             */
#define  HOST_ABT_HA    0x05        /* Command aborted by host adapter     */
#define  HOST_SEL_TO    0x11        /* Selection timeout                   */
#define  HOST_DU_DO     0x12        /* Data overrun/underrun error         */
#define  HOST_BUS_FREE  0x13        /* Unexpected bus free                 */
#define  HOST_PHASE_ERR 0x14        /* Target bus phase sequence error     */
#define  HOST_INV_LINK  0x17        /* Invalid SCSI linking operation      */
#define  HOST_SNS_FAIL  0x1b        /* Auto-request sense failed           */
#define  HOST_TAG_REJ   0x1c        /* Tagged Queuing rejected by target   */
#define  HOST_HW_ERROR  0x20        /* Host adpater hardware error         */
#define  HOST_ABT_FAIL  0x21        /* Target did'nt respond to ATN (RESET)*/
#define  HOST_RST_HA    0x22        /* SCSI bus reset by host adapter      */
#define  HOST_RST_OTHER 0x23        /* SCSI bus reset by other device      */
#define  HOST_NOAVL_INDEX 0x30      /* SCSI bus reset by other device      */

/****************************************************************************
                        %%% SP_SegCnt values %%%
****************************************************************************/

#define  SPEC_OPCODE_00 0x00        /* "SPECIAL" Sequencer Opcode          */

#define  SPEC_BIT3SET   1           /* "SPECIAL" -- Set Bit 3 of the SCB   */
                                    /*              Control Byte           */



/****************************************************************************
               %%% Host Adapter Configuration Structure %%%
****************************************************************************/

typedef struct cfp
{
   UWORD Cf_AdapterId;              /* Host Adapter ID (ie. 0x7870)        */
   UBYTE Cf_BusNumber;              /* PCI bus number                      */
   UBYTE Cf_DeviceNumber;           /* PCI device number                   */

   struct aic7870_reg *Cf_base_addr;   /* Base address (I/O or memory)     */

   void * Cf_Adapter;		    /* adp78_adap_t pointer		   */
   vertex_hdl_t Cf_ConnVhdl;	    /* vertex for pci services		   */
   UBYTE Cf_scb16;		    /* next 4 used by PH_scsi_bus_reset    */
   UBYTE Cf_scb17;		  
   UBYTE Cf_simode0;
   UBYTE Cf_simode1;

   union
   {
      DWORD ConfigFlags;            /* Configuration Flags                 */
      struct
      {
         UDWORD PrChId:1;           /* AIC-7770 B Channel is primary       */
         UDWORD BiosActive:1;       /* SCSI BIOS presently active          */
         UDWORD EdgeInt:1;          /* AIC-7770 High Edge Interrupt        */
         DWORD ScsiTimeOut:2;       /* Selection timeout (2 bits)          */
         UDWORD ScsiParity:1;       /* Parity checking enabled if equal 1  */
         UDWORD ResetBus:1;         /* Reset SCSI bus at 1st initialization*/
         UDWORD InitNeeded:1;       /* Initialization required             */

         UDWORD MultiTaskLun:1;     /* 1 = Multi-task on target/lun basis  */
                                    /* 0 = Multi-task on target basis only */
         UDWORD BiosCached:1;       /* BIOS activated, Driver suspended (AIC-7770)  */
         UDWORD BiosAvailable:1;    /* BIOS available for activation (AIC-7770)     */
         UDWORD RsevBit11:1;        /* Reserved = 0                        */
         UDWORD TerminationLow:1;   /* Low byte termination flag           */
         UDWORD TerminationHigh:1;  /* High byte termination flag          */
         UDWORD TwoChnl:1;          /* AIC-7770 configured for two channel */
         UDWORD DriverIdle:1;       /* HIM Driver and sequencer idle       */

	 UDWORD SuppressNego:1;	    /* Suppress sync/wide nego	*/
	 UDWORD EnableFast20:1;	    /* Enable Double Speed negotiation */
         DWORD ResvByte2:6;         /* Reserved for future use             */

         DWORD ResvByte3:8;         /* Reserved for future use             */
      } flags_struct;
   } Cf_flags;
   UWORD Cf_AccessMode;             /* SCB handling mode to use:           */
                                    /* 0 = Use default                     */
                                    /* 1 = Internal method (4 SCBs max.)   */
                                    /* 2 = Optima method (255 SCBs max.)   */
                                    /*          HOST CONFIGURATION         */
   UBYTE Cf_BusRelease;             /* PCI Latency time                    */
   UBYTE Cf_ScsiId;                 /* Host Adapter's SCSI ID              */
   UBYTE Cf_MaxTargets;             /* Maximum number of targets on bus    */
   union {
     UBYTE   byte;
     struct {
       UBYTE WideMode:1;
       UBYTE SyncRate:3;
       UBYTE AsyncFrc:1;
       UBYTE NarrowFrc:1;
       UBYTE AbortMsg:1;
       UBYTE SyncMode:1;
     } u;
   } Cf_ScsiOption[16];             /* SCSI config options (1 byte/targ)*/
   UBYTE Cf_DoNegotiation[16];      /* force a negotiation on target       */
   union
   {
      UWORD AllowDscnt;             /* Bit map to allow disconnection      */
      struct
      {
#ifdef MIPS_BE
         UBYTE AllowDscntH;         /* Disconnect bit map high             */
         UBYTE AllowDscntL;         /* Disconnect bit map low              */
#else  /* MIPS_BE */
         UBYTE AllowDscntL;         /* Disconnect bit map low              */
         UBYTE AllowDscntH;         /* Disconnect bit map high             */
#endif /* MIPS_BE */
      } dscnt_struct;
   } Cf_dscnt;
   UWORD Cf_AllowSyncMask;	    /* Mask set of devices allowed to do sync */

   union
   {
      struct hsp *HaDataPtr;        /* Pointer to HIM data stucture        */
      DWORD HaDataPtrField;         /* HIM use only for pointer arithmetic */
   } Cf_hsp_ptr;
   DWORD Cf_HaDataPhy;              /* Physical pointer to HIM data str    */
   UWORD Cf_MaxNonTagScbs;          /* Max nontagged SCBs per target/LUN   */
                                    /* Valid settings are 1 or 2           */
   UWORD Cf_MaxTagScbs;             /* Max tagged SCBs per target/LUN      */
                                    /* Valid settings are 1 to 32          */
   UWORD Cf_NumberScbs;             /* Number of SCBs                      */
                                    /* Valid setting are 1 to 255          */
   UBYTE Cf_DReserved[32];          /* Reserved for future expansion       */
   OS_TYPE Cf_OSspecific;           /* Pointer to custom data structures   */
                                    /* required by O/S specific interface. */

} cfp_struct;

#define  CFP_AdapterId        Cf_AdapterId
#define  CFP_BusNumber        Cf_BusNumber
#define  CFP_DeviceNumber     Cf_DeviceNumber
#define  CFP_Base             Cf_base_addr
#define  CFP_base_addr        Cf_base_addr
#define  CFP_Adapter          Cf_Adapter
#define  CFP_ConfigFlags      Cf_flags.ConfigFlags
#define  CFP_BiosActive       Cf_flags.flags_struct.BiosActive
#define  CFP_ScsiTimeOut      Cf_flags.flags_struct.ScsiTimeOut
#define  CFP_ScsiParity       Cf_flags.flags_struct.ScsiParity
#define  CFP_ResetBus         Cf_flags.flags_struct.ResetBus
#define  CFP_InitNeeded       Cf_flags.flags_struct.InitNeeded
#define  CFP_MultiTaskLun     Cf_flags.flags_struct.MultiTaskLun
#define  CFP_Scb2Bios         Cf_flags.flags_struct.Scb2Bios
#define  CFP_OptimaMode       Cf_flags.flags_struct.OptimaMode
#define  CFP_DriverIdle       Cf_flags.flags_struct.DriverIdle
#define  CFP_SuppressNego     Cf_flags.flags_struct.SuppressNego
#define  CFP_EnableFast20     Cf_flags.flags_struct.EnableFast20
#define  CFP_TerminationLow   Cf_flags.flags_struct.TerminationLow
#define  CFP_TerminationHigh  Cf_flags.flags_struct.TerminationHigh

#define  CFP_AccessMode       Cf_AccessMode
#define  CFP_BusRelease       Cf_BusRelease
#define  CFP_ScsiId           Cf_ScsiId
#define  CFP_MaxTargets       Cf_MaxTargets
#define  CFP_ScsiOption       Cf_ScsiOption
#define  CFP_DoNegotiation    Cf_DoNegotiation
#define  CFP_HaDataPtr        Cf_hsp_ptr.HaDataPtr
#define  CFP_HaDataPhy        Cf_HaDataPhy
#define  CFP_MaxNonTagScbs    Cf_MaxNonTagScbs
#define  CFP_MaxTagScbs       Cf_MaxTagScbs
#define  CFP_NumberScbs       Cf_NumberScbs
#define  CFP_AllowDscnt       Cf_dscnt.AllowDscnt
#define  CFP_AllowDscntL      Cf_dscnt.dscnt_struct.AllowDscntL
#define  CFP_AllowDscntH      Cf_dscnt.dscnt_struct.AllowDscntH
#define  CFP_AllowSyncMask    Cf_AllowSyncMask

/****************************************************************************
                     %%% HA_config_flags values %%%
****************************************************************************/
#define  DRIVER_IDLE    0x8000      /* HIM Driver and sequencer idle       */
#define  INIT_NEEDED    0x0080      /* Initialization required             */
#define  RESET_BUS      0x0040      /* Reset SCSI bus at 1st initialization*/
#define  SCSI_PARITY    ENSPCHK     /* Parity checking enabled if equal 1  */
                                    /* ENSPCHK defined in SXFRCTL1         */
#define  STIMESEL       STIMESEL0 + STIMESEL1 /* Selection timeout (2 bits)*/
                                    /* STIMESEL(0-1) defined in SXFRCTL1   */
#define  BIOS_ACTIVE    0x0002      /* SCSI BIOS presently active          */
#define  OPTIMA_MODE    0x4000      /* Driver configured for OPTIMA mode   */

/****************************************************************************
                      %%% scsi_options values %%%
****************************************************************************/
#define  WIDE_MODE      0x80        /* Allow wide negotiation if equal 1   */
#define  SYNC_RATE      0x70        /* 3-bit mask for maximum transfer rate*/
#define  ASYNC_FRC      0x08        /* Force a async negotiation           */
#define  NARROW_FRC     0x04        /* Force a narrow negotiation          */
#define  ABORT_MSG      0x02        /* send abort message                  */
#define  SYNC_MODE      0x01        /* Allow synchronous negotiation if = 1*/
#define  SOME_NEGO      (WIDE_MODE | SYNC_MODE | NARROW_FRC | ASYNC_FRC)

/*****************************************/
/*    Generic union of DWORD/UWORD/UBYTE */
/*****************************************/
union gen_union
{
   UBYTE ubyte[4];
   UWORD uword[2];
   DWORD dword;
};

/***************************************/
/*    DWORD alligned sp_struct pointer */
/***************************************/
union sp_dword
{
   DWORD dword;
   sp_struct *sp;
};

/****************************************************************************
                  %%% Host Adapter Data Structure %%%
****************************************************************************/


typedef struct hsp
{
   DWORD	*scb_ptr_array;	    /* array of 32bit phys addr of Q'd SCBs */
   UBYTE	*qout_ptr_array;    /* Pointer to external queue out array  */
   int		*qout_ptr_all;	    /* pointer to all of queue out array    */

   UWORD sel_cmp_brkpt;             /* Sequencer address for breakpoint on
                                       target selection complete           */
   UBYTE free_lo_scb;               /* Low free SCB allocation index       */
   UBYTE qout_index;                /* Queue out index                     */

   UBYTE Hsp_MaxNonTagScbs;         /* Max nontagged SCBs per target/LUN   */
   UBYTE Hsp_MaxTagScbs;            /* Max tagged SCBs per target/LUN      */
   UBYTE done_cmd;                  /* Index to done SCB array             */
   UBYTE free_hi_scb;               /* High free SCB allocation index      */

   sp_struct    *hsp_active_ptr[ADP78_NUMSCBS];
   UBYTE	hsp_free_stack[ADP78_NUMSCBS];
   UBYTE	hsp_done_stack[ADP78_NUMSCBS];


   UBYTE Hsp_SaveScr[64];           /* SCRATCH RAM Save Area within        */
                                    /*  him_struct                         */


   UWORD (*calcmodesize)(UWORD);    /* calculate mode dependent size       */
   UBYTE (*morefreescb)(struct hsp *, sp_struct *);
				/* check if more free scb available    */
   void (*cmdcomplete)(cfp_struct *);
				/* command complete handler            */
   UBYTE (*getfreescb)(struct hsp *, sp_struct *);
				/* get free scb                        */
   void  (*returnfreescb)(cfp_struct *, UBYTE);
				/* return free scb                     */
   int   (*abortactive)(sp_struct *);
				/* abort (active) scb                  */
   void  (*indexclearbusy)(cfp_struct *, UBYTE);
				    /* clear busy indexed entry            */
   void  (*cleartargetbusy)(cfp_struct *, UBYTE);
				    /* clear target busy                   */
   void  (*requestsense)(sp_struct *, UBYTE);
				    /* request sense                       */
   void  (*enque)(UBYTE, sp_struct *);
			  	/* enqueue for sequencer               */
   void  (*enquehead)(UBYTE, sp_struct *, AIC_7870 *);
				/* enqueue scb on Head for sequencer   */
} hsp_struct;


#define  ACTTARG        ha_ptr->acttarg
#define  FREE_LO_SCB    ha_ptr->free_lo_scb
#define  FREE_HI_SCB    ha_ptr->free_hi_scb
#define  FREE_STACK     ha_ptr->hsp_free_stack
#define  DONE_CMD       ha_ptr->done_cmd
#define  DONE_STACK     ha_ptr->hsp_done_stack
#define  ACTPTR         ha_ptr->hsp_active_ptr

#define  SCB_PTR_ARRAY  ha_ptr->scb_ptr_array
#define  QOUT_PTR_ARRAY ha_ptr->qout_ptr_array

#define  Head_Of_Queue  Head_Of_Q.sp
#define  End_Of_Queue   End_Of_Q.sp

/****************************************************************************
                        %%% Miscellaneous %%%
****************************************************************************/
#define  SCB_LENGTH     0x1c        /* # of SCB bytes to download to Lance */
#define  ERR            0xff        /* Error return value                  */
#define  NONEGO         0x00        /* Responding to target's negotiation  */ 
#define  NEEDNEGO       0x8f        /* Need negotiation response from targ */



/****************************************************************************
                      %%% targ_option values %%%
****************************************************************************/
#define  ALLOW_DISCON   0x04        /* Allow targets to disconnect         */
#define  SYNCNEG        0x08        /* Init for synchronous negotiation    */
#define  PARITY_ENAB    0x20        /* Enable SCSI parity checking         */
#define  WIDENEG        0x80        /* Init for Wide negotiation           */
#define  MAX_SYNC_RATE  0x07        /* Maximum synchronous transfer rate   */



/* \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\ */
/*                                                                         */
/*              CONSTANTS FOR FAULT TOLERANT CODE                          */
/*                                                                         */
/* \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\ */

/* Return Codes for scb_deque */

#define  DQ_GOOD        0x00        /* SCB successfully dequeued */
#define  DQ_FAIL        0xFF        /* SCB is ACTIVE, cannot dequeue */

/* Values for actstat array in ha_struct */

#define  AS_SCBFREE        0x00
#define  AS_SCB_ACTIVE     0x01
#define  AS_SCB_ABORT      0x02
#define  AS_SCB_ABORT_CMP  0x03
#define  AS_SCB_BIOS       0x40
#define  AS_SCB_RST        0x10
                        
#define  ABORT_DONE     0x00
#define  ABORT_INPROG   0x01
#define  ABORT_FAILED   0xFF

/* Values for scsi_state in send_trm_msg */

#define  NOT_CONNECTED     0x0C
#define  CONNECTED         0x00
#define  WAIT_4_SEL        0x88
#define  OTHER_CONNECTED   0x40



/* Return code mask when findha detects that the chip is disabled */

#define  NOT_ENABLED       0x80

/****************************************************************************
               %%% BIOS Information Structure %%%
****************************************************************************/

#define  BI_BIOS_ACTIVE    0x01
#define  BI_GIGABYTE       0x02        /* Gigabyte support enabled */
#define  BI_DOS5           0x04        /* DOS 5 support enabled */

#define  BI_MAX_DRIVES     0x08        /* DOS 5 maximum, 2 for DOS 3,4 */

#define SCRATCH_BANK       0           /* additional scratch bank    */

#define  BIOS_BASE         0
#define  BIOS_DRIVES       BIOS_BASE + 4
#define  BIOS_GLOBAL       BIOS_BASE + 12
#define  BIOS_FIRST_LAST   BIOS_BASE + 14
#define  BIOS_INTVECTR     BIOS_BASE + 16 /* storage for BIOS int vector  */

#define  BIOS_GLOBAL_DUAL  0x08
#define  BIOS_GLOBAL_WIDE  0x04
#define  BIOS_GLOBAL_GIG   0x01

#define  BIOS_GLOBAL_DOS5  0x40

typedef struct bios_info_block {

   UBYTE bi_global;
   UBYTE bi_first_drive;
   UBYTE bi_last_drive;
   UBYTE bi_drive_info[BI_MAX_DRIVES];
   UWORD bi_bios_segment;
   UBYTE bi_reserved[3];

} bios_info;

/*
 * Codes for the "why" argument to Ph_BadSeq.
 * Now I can tell who called BadSeq and why, since this usually causes a
 * SCSI bus reset.
 */
#define BADSEQ_TARGABORT_NULLSCB	0xe0010000
#define BADSEQ_TARGABORT		0xe0020000
#define BADSEQ_EMSGI			0xe0030000
#define BADSEQ_MSGI			0xe0040000
#define BADSEQ_CDBXFER			0xe0050000
#define BADSEQ_PHASE			0xe0060000
#define BADSEQ_SCBNUM			0xe0070000
#define BADSEQ_NULLSCB			0xe0080000



/*********************************************************************
*  HIM.C
*********************************************************************/
UWORD PH_init_him (cfp_struct *);			/* entry point */
int Ph_LoadSequencer (cfp_struct *);
void Ph_ext_scsi_bus_reset(cfp_struct *);
void Ph_ResetChannel (cfp_struct *);
void Ph_CheckLength (sp_struct *,register AIC_7870 *);
void PH_reset_him(cfp_struct *cfp);
void PH_reset_scsi_bus(cfp_struct *cfp, int assertrst);
void PH_abort_seq_cmds(cfp_struct *cfp);
int Ph_BadSeq (cfp_struct *, AIC_7870 *, int why);
void Ph_CheckCondition (sp_struct *,register AIC_7870 *, UBYTE);
void Ph_RdByteX (AIC_7870 *base);
UBYTE Ph_RdByte (AIC_7870 *base);
int Ph_TargetAbort (cfp_struct *, sp_struct *, AIC_7870 *);
void Ph_HandleMsgi (sp_struct *,register AIC_7870 *);
UBYTE Ph_SendTrmMsg ( cfp_struct *,UBYTE);
void Ph_SendMsgo (sp_struct *,register AIC_7870 *);
void PH_SetNeedNego (AIC_7870 *, UBYTE, UBYTE);		/* entry point */
void Ph_Negotiate (sp_struct *,register AIC_7870 *);
UBYTE Ph_SyncSet (sp_struct *);
void Ph_SyncNego (sp_struct *,register AIC_7870 *);
void Ph_WideNego (sp_struct *, AIC_7870 *);
void Ph_ExtMsgi (sp_struct *,register AIC_7870 *);
UBYTE Ph_ExtMsgo (sp_struct *,register AIC_7870 *);
void Ph_IntSelto (cfp_struct *,sp_struct *);
void PH_Set_Selto (cfp_struct *, int);			/* entry point */
void Ph_IntFree (cfp_struct *,sp_struct *);
void Ph_ParityError (sp_struct *, AIC_7870 *);
void Ph_pci_error(cfp_struct *, AIC_7870 *);
UBYTE Ph_Wt4Req (sp_struct *, register AIC_7870 *);
void PH_Pause (register AIC_7870 *);			/* entry point */
int PH_Pause_rtn (register AIC_7870 *);			/* entry point */
void PH_UnPause_hard (AIC_7870 *);			/* entry point */
void PH_UnPause (register AIC_7870 *);			/* entry point */
void PH_WriteHcntrl (register AIC_7870 *,UBYTE);	/* entry point */
UBYTE PH_ReadIntstat (register AIC_7870 *);		/* entry point */
void Ph_EnableNextScbArray(sp_struct *);
void Ph_ClearFast20Reg (cfp_struct *,sp_struct *);
void Ph_LogFast20Map (cfp_struct *,sp_struct *);
                      
/*********************************************************************
*  HIMD.C
*********************************************************************/
void PH_ScbSend (sp_struct *);				/* entry point */
UBYTE PH_IntHandler (cfp_struct *);			/* entry point */
UBYTE PH_IntHandler_abn (cfp_struct *);			/* entry point */
UBYTE Ph_SendCommand (hsp_struct *, sp_struct *);
void Ph_TerminateCommand (sp_struct *,UBYTE);
void Ph_PostCommand (cfp_struct *);
void Ph_init_hsp_struct(cfp_struct *);

/*********************************************************************
*  HIMDOPT.C
*********************************************************************/
void Ph_OptimaEnque(UBYTE,sp_struct *);
void Ph_OptimaEnqueHead(UBYTE,sp_struct *, AIC_7870 *);
void Ph_OptimaQHead (UBYTE,sp_struct *, AIC_7870 *);
void Ph_OptimaCmdComplete(cfp_struct *, UBYTE);
void Ph_OptimaRequestSense(sp_struct *, UBYTE);
void Ph_OptimaIndexClearBusy(cfp_struct *, UBYTE);
void Ph_OptimaClearTargetBusy (cfp_struct *, UBYTE);
void Ph_OptimaCheckClearBusy(cfp_struct *, UBYTE);
void Ph_set_scratchmem(cfp_struct *);
UBYTE Ph_OptimaGetFreeScb(hsp_struct *, sp_struct *);
void Ph_MovPtrToScratch(UWORD, DWORD, cfp_struct *);
int PH_OptimaAbortActive(sp_struct *);			/* entry point */

