/* $Header: /proj/irix6.5.7m/isms/stand/arcs/lib/libsk/io/adphim/RCS/him_scb.h,v 1.2 1997/07/24 00:33:20 philw Exp $ */
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

/***************************************************************************
*
*  Module Name:   HIM_SCB.H
*
*  Description:
*					This file is comprised of three sections:
*
*             	1. O/S specific definitions that can be customized to allow
*                 for proper compiling and linking of the Hardware Interface
*                 Module.
*
*              2. Structure definitions:
*                   SP_STRUCT  - Sequencer Control Block Structure
*                   HIM_CONFIG - Host Adapter Configuration Structure
*                                (may be customized)
*                   HIM_STRUCT - Hardware Interface Module Data Structure
*                                (cannot be customized)
*
*              3. Function prototypes for the Hardware Interface Module.
*
*              Refer to the Hardware Interface Module specification for
*              more information.
*
*  Programmers:   Paul von Stamwitz
*                 Chuck Fannin
*                 Jeff Mendiola
*                 Harry Yang
*    
*  Notes:         NONE
*
*  Revisions -
*
***************************************************************************/
/*                                                                         */
/*                CUSTOMIZED OPERATING SPECIFIC DEFINITIONS                */
/*                                                                         */
/***************************************************************************/
#include "osm.h"

#if !defined( _EX_SEQ00_ )
#define  _STANDARD
#endif   /* !defined( _EX_SEQ00_ ) */

#if !defined( _EX_SEQ01_ )
#define  _OPTIMA
#endif   /* !defined( _EX_SEQ01_ ) */

#define	HCNTRL_PAUSE_DELAY	50
#define HCNTRL_PAUSE_LOOPS	40000

/****************************************************************************

 LANCE SCSI REGISTERS DEFINED AS AN ARRAY IN A STRUCTURE

****************************************************************************/

typedef struct aic7870_reg
{
   UBYTE aic7870[192];
}aic_7870;

#define  AIC7870 (OS_TYPE) &base->aic7870
#define  AIC_7870 aic_7870

/***************************************************************************/
/*                                                                         */
/*        STRUCTURE DEFINITIONS: SP_STRUCT, HIM_CONFIG, HIM_STRUCT         */
/*                                                                         */
/***************************************************************************/
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
typedef struct sp                   /*                                     */
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
      DWORD Rsvd1:1;                /* Reserved = 0                        */
      DWORD NegoInProg:1;           /* Negotiation in progress             */
      DWORD Rsvd2:2;                /* Reserved = 0                        */
      DWORD OverlaySns:1;           /* 1 = Overlay SCB with Sense Data     */
      DWORD NoNegotiation:1;        /* No negotiation for the scb          */
      DWORD NoUnderrun:1;           /* 1 = Suppress underrun errors        */
      DWORD AutoSense:1;            /* 1 = Automatic Request Sense enabled */
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
         DWORD RejectMDP:1;         /* Non-contiguous data                 */
         DWORD DisEnable:1;         /* Disconnection enabled               */
         DWORD TagEnable:1;         /* Tagged Queuing enabled              */
         DWORD Rsvd3:1;             /* Reserved = 0                        */
         DWORD SpecFunc:1;          /* Defined for "special" opcode        */
         DWORD Discon:1;            /* Target currently disconnected       */
         DWORD TagType:2;           /* Tagged Queuing type                 */
         DWORD Tarlun:8;            /* Target/Channel/Lun                  */

         DWORD SegPtr;              /* Pointer to Scatter/Gather list      */

         DWORD CDBPtr;              /* Pointer to Command Descriptor Block */

	 /* this word field was switched */
         DWORD Offshoot:8;          /* Pointer to offshoot SCB from chain  */
         DWORD NextPtr:8;           /* Pointer to next SCB to be executed  */
         DWORD Progress:4;          /* Progress count (2's complement)     */
         DWORD Aborted:1;           /* Aborted SCB flag                    */
         DWORD Abort:1;             /* Abort SCB flag                      */
         DWORD Concurrent:1;        /* Cuncurrent SCB flag                 */
         DWORD Holdoff:1;           /* Holdoff SCB flag                    */
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
#ifdef _DRIVER
   UBYTE Sp_Reserved[4];            /* Reserved for future expansion       */
   OS_TYPE Sp_OSspecific;           /* Pointer to custom data structures   */
                                    /* required by O/S specific interface. */
#endif   /* _DRIVER */
} sp_struct;

#ifdef _DRIVER
#define  SP_Next        Sp_queue.Next
#endif   /* _DRIVER */
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
#define  INV_SCB_CMD    0x80        /* Invalid SCSI request                */

/****************************************************************************
                       %%% Sp_MgrStat values %%%
****************************************************************************/
#define  SCB_PROCESS    0x00        /* SCB needs to be processed           */
#define  SCB_DONE       SCB_COMP    /* SCB finished without error          */
#define  SCB_DONE_ABT   SCB_ABORTED /* SCB finished due to abort from host */
#define  SCB_DONE_ERR   SCB_ERR     /* SCB finished with error             */
#define  SCB_READY      0x10        /* SCB ready to be loaded into Lance   */
#define  SCB_WAITING    0x20        /* SCB waiting for another to finish   */
#define  SCB_ACTIVE     0x40        /* SCB loaded into Lance               */

#define  SCB_ABORTINPROG 0x44       /* Abort special function in progress  */
#define  SCB_BDR        0x41        /* Bus Device Reset special function   */

#define  SCB_DONE_ILL   INV_SCB_CMD /* SCB finished due to illegal command */

#define  SCB_AUTOSENSE  0x08        /* SCB w/autosense in progress */

#define  SCB_DONE_MASK  SCB_DONE+SCB_DONE_ABT+SCB_DONE_ERR+SCB_DONE_ILL

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

#define  ABTACT_CMP     0           /* Abort completed, post SCB */
#define  ABTACT_PRG     1           /* Abort in progress, don't post SCB */
#define  ABTACT_NOT     2           /* Abort unsuccessful */
#define  ABTACT_ERR     -1          /* Abort error */


/****************************************************************************
               %%% Host Adapter Configuration Structure %%%
****************************************************************************/

typedef struct cfp
{       
   UWORD Cf_AdapterId;              /* Host Adapter ID (ie. 0x7870)        */
   UBYTE Cf_BusNumber;              /* PCI bus number                      */
   UBYTE Cf_DeviceNumber;           /* PCI bus number                      */

   union
   {
      DWORD BaseAddress;            /* XXX done use this! not 64 bit safe */
      struct aic7870_reg *Base;	    /* Base address (I/O or memory)       */
   }Cf_base_addr;

   void * Cf_Adapter;		    /* adp78_adap_t pointer		   */
   char * Cf_Busaddr;		    /* cookie for pci_put_cfg		   */

   union
   {
      DWORD ConfigFlags;            /* Configuration Flags                 */
      struct
      {
         DWORD PrChId:1;            /* AIC-7770 B Channel is primary       */
         DWORD BiosActive:1;        /* SCSI BIOS presently active          */
         DWORD EdgeInt:1;           /* AIC-7770 High Edge Interrupt        */
         DWORD ScsiTimeOut:2;       /* Selection timeout (2 bits)          */
         DWORD ScsiParity:1;        /* Parity checking enabled if equal 1  */
         DWORD ResetBus:1;          /* Reset SCSI bus at 1st initialization*/
         DWORD InitNeeded:1;        /* Initialization required             */
         DWORD MultiTaskLun:1;      /* 1 = Multi-task on target/lun basis  */
                                    /* 0 = Multi-task on target basis only */
         DWORD BiosCached:1;        /* BIOS activated, Driver suspended (AIC-7770)  */
         DWORD BiosAvailable:1;     /* BIOS available for activation (AIC-7770)     */
         DWORD RsevBit11:1;         /* Reserved = 0                        */
         DWORD TerminationLow:1;    /* Low byte termination flag           */
         DWORD TerminationHigh:1;   /* High byte termination flag          */
         DWORD TwoChnl:1;           /* AIC-7770 configured for two channel */
         DWORD DriverIdle:1;        /* HIM Driver and sequencer idle       */
	 DWORD SuppressNego:1;	    /* Suppress sync/wide nego	*/
	 DWORD EnableFast20:1;	    /* Enable Double Speed negotiation */
         DWORD ResvByte2:6;         /* Reserved for future use             */
         DWORD ResvByte3:7;         /* Reserved for future use             */
         DWORD WWQioCnt:1;          /* WW for QINCNT/QOUTCNT (Dagger A WW) */
      } flags_struct;
   } Cf_flags;
   UWORD Cf_AccessMode;             /* SCB handling mode to use:           */
                                    /* 0 = Use default                     */
                                    /* 1 = Internal method (4 SCBs max.)   */
                                    /* 2 = Optima method (255 SCBs max.)   */
                                    /*          HOST CONFIGURATION         */
   UBYTE Cf_ScsiChannel;            /* SCSI channel designator             */
   UBYTE Cf_IrqChannel;             /* Interrupt channel #                 */
   UBYTE Cf_DmaChannel;             /* DMA channel # (ISA only)            */
   UBYTE Cf_BusRelease;             /* PCI Latency time                    */
   UBYTE Cf_Threshold;              /* Data FIFO threshold                 */
   UBYTE Cf_Reserved;               /* Reserved for future expansion       */
   UBYTE Cf_BusOn;                  /* DMA bus-on timing (ISA only)        */
   UBYTE Cf_BusOff;                 /* DMA bus-off timing (ISA only)       */
   UWORD Cf_StrobeOn;               /* DMA bus timing (ISA only)           */
   UWORD Cf_StrobeOff;              /* DMA bus timing (ISA only)           */
                                    /*          SCSI CONFIGURATION         */
   UBYTE Cf_ScsiId;                 /* Host Adapter's SCSI ID              */
   UBYTE Cf_MaxTargets;             /* Maximum number of targets on bus    */
   UBYTE Cf_ScsiOption[16];         /* SCSI config options (1 byte/target) */
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
#ifdef   _DRIVER
   union
   {
      struct hsp *HaDataPtr;        /* Pointer to HIM data stucture        */
      DWORD HaDataPtrField;         /* HIM use only for pointer arithmetic */
   } Cf_hsp_ptr;
   DWORD Cf_HaDataPhy;              /* Physical pointer to HIM data str    */
   UWORD Cf_HimDataSize;            /* Size of HIM data structure in bytes */
                                    /* Valid after call to get_config      */
   UWORD Cf_MaxNonTagScbs;          /* Max nontagged SCBs per target/LUN   */
                                    /* Valid settings are 1 or 2           */
   UWORD Cf_MaxTagScbs;             /* Max tagged SCBs per target/LUN      */
                                    /* Valid settings are 1 to 32          */
   UWORD Cf_NumberScbs;             /* Number of SCBs                      */
                                    /* Valid setting are 1 to 255          */
   UBYTE Cf_DReserved[32];          /* Reserved for future expansion       */
   OS_TYPE Cf_OSspecific;           /* Pointer to custom data structures   */
                                    /* required by O/S specific interface. */
#endif   /* _DRIVER */
} cfp_struct;

#define  CFP_AdapterId        Cf_id.AdapterId
#define  CFP_AdapterIdL       Cf_id.id_struct.AdapterIdL
#define  CFP_AdapterIdH       Cf_id.id_struct.AdapterIdH
#define  CFP_Base             Cf_base_addr.Base
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
#define  CFP_WWQioCnt         Cf_flags.flags_struct.WWQioCnt
#ifdef   _DRIVER
#define  CFP_HaDataPtr        Cf_hsp_ptr.HaDataPtr
#define  CFP_HaDataPtrField   Cf_hsp_ptr.HaDataPtrField
#endif   /* _DRIVER */
#define  CFP_AllowDscnt       Cf_dscnt.AllowDscnt
#define  CFP_AllowDscntL      Cf_dscnt.dscnt_struct.AllowDscntL
#define  CFP_AllowDscntH      Cf_dscnt.dscnt_struct.AllowDscntH

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
#define  SYNC_MODE      0x01        /* Allow synchronous negotiation if = 1*/

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


   union sp_dword Head_Of_Q;        /* First SCB ptr on Queue              */
   union sp_dword End_Of_Q;         /* Last SCB ptr on Queue               */

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
#define  NULL_SCB       0xFF

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

/****************************************************************************
                      %%% SemState values %%%
****************************************************************************/
#define  SEM_RELS       0           /* semaphore released                  */
#define  SEM_LOCK       1           /* semaphore locked                    */


/***************************************************************************/
/*                                                                         */
/*              CONSTANTS FOR FAULT TOLERANT CODE                          */
/*                                                                         */
/***************************************************************************/

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

/* "NULL ptr-like" value (Don't want to include libraries ) */

#define  NOT_DEFINED    (sp_struct *) - 1
#define  BIOS_SCB       (sp_struct *) - 2

/* Values for scsi_state in send_trm_msg */

#define  NOT_CONNECTED     0x0C
#define  CONNECTED         0x00
#define  WAIT_4_SEL        0x88
#define  OTHER_CONNECTED   0x40

/* Return code mask when int_handler detects BIOS interrupt */

#define  BIOS_INT          0x80

/*
 * Return codes from PH_IntHandler.  The top 4 bits hold values from here.
 * The lower 4 bits are values from INTSTAT.
 */
#define  NOT_OUR_INT       0x80
#define  OUR_CC_INT        0x40		/* cc means command complete? */
#define  OUR_OTHER_INT     0x20
#define  OUR_INT           OUR_CC_INT|OUR_OTHER_INT


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


/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%  Function Prototypes
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

void PH_GetConfig (cfp_struct *);
UWORD PH_init_him (cfp_struct *);
int Ph_LoadSequencer (cfp_struct *);
#ifdef   _FAULT_TOLERANCE
int PH_Special (UBYTE,cfp_struct *,sp_struct *);
#endif   /* _FAULT_TOLERANCE */
void PH_EnableInt (cfp_struct *);
void PH_DisableInt (cfp_struct *);
#ifdef   _FAULT_TOLERANCE
UBYTE Ph_NonInit (sp_struct *);
void Ph_Abort (sp_struct *);
#endif   /* _FAULT_TOLERANCE */
void Ph_ext_scsi_bus_reset(cfp_struct *);
void Ph_ResetChannel (cfp_struct *);
void Ph_CheckLength (sp_struct *,register AIC_7870 *);
#ifndef _LESS_CODE
void Ph_CdbAbort (sp_struct *,register AIC_7870 *);
UBYTE Ph_TargetAbort (cfp_struct *, sp_struct *, register AIC_7870 *);
#endif   /* _LESS_CODE */
void Ph_ResetSCSI (AIC_7870 *);
void Ph_BadSeq (cfp_struct *,register AIC_7870 *);
void Ph_CheckCondition (sp_struct *,register AIC_7870 *);
void Ph_HandleMsgi (sp_struct *,register AIC_7870 *);
UBYTE Ph_SendTrmMsg ( cfp_struct *,UBYTE);
#ifdef   _FAULT_TOLERANCE
UBYTE Ph_Wt4BFree (cfp_struct *);
UBYTE Ph_TrmCmplt ( cfp_struct *,UBYTE,UBYTE);
UBYTE Ph_ProcBkpt (cfp_struct *);
#endif   /* _FAULT_TOLERANCE */
void  Ph_BusReset (cfp_struct *);
void Ph_SendMsgo (sp_struct *,register AIC_7870 *);
void Ph_SetNeedNego (UBYTE, register AIC_7870 *);
void Ph_Negotiate (sp_struct *,register AIC_7870 *);
UBYTE Ph_SyncSet (sp_struct *);
void Ph_SyncNego (sp_struct *,register AIC_7870 *);
void Ph_WideNego (sp_struct *, AIC_7870 *);
void Ph_ExtMsgi (sp_struct *,register AIC_7870 *);
UBYTE Ph_ExtMsgo (sp_struct *,register AIC_7870 *);
void Ph_IntSelto (cfp_struct *,sp_struct *);
void PH_Set_Selto (cfp_struct *, int);
void Ph_IntFree (cfp_struct *,sp_struct *);
void Ph_ParityError (register AIC_7870 *);
UBYTE Ph_Wt4Req (register AIC_7870 *);
void Ph_MultOutByte (register AIC_7870 *,UBYTE,int);
void Ph_MemorySet(UBYTE *,UBYTE,int);
void Ph_Pause (register AIC_7870 *);
void Ph_UnPause (register AIC_7870 *);
void Ph_WriteHcntrl (register AIC_7870 *,UBYTE);
UBYTE Ph_ReadIntstat (register AIC_7870 *);
void Ph_ScbRenego (cfp_struct *,UBYTE);
void Ph_ClearFast20Reg (cfp_struct *,sp_struct *);
void Ph_LogFast20Map (cfp_struct *,sp_struct *);

#ifdef _DRIVER                   
                      
/*********************************************************************
*  HIMD.C
*********************************************************************/
void PH_ScbSend (sp_struct *);
UBYTE PH_IntHandler (cfp_struct *);
UBYTE PH_IntHandler_abn(cfp_struct *, UBYTE);
void Ph_ScbPrepare (cfp_struct *,sp_struct *);
UBYTE Ph_SendCommand (hsp_struct *, sp_struct *);
void Ph_TerminateCommand (sp_struct *,UBYTE);
void Ph_PostCommand (cfp_struct *);
void Ph_PostNonActiveScb (cfp_struct *,sp_struct *);
void Ph_TermPostNonActiveScb (sp_struct *);
void Ph_AbortChannel (cfp_struct *,UBYTE);
UBYTE Ph_GetCurrentScb (cfp_struct *);
void Ph_HardReset (cfp_struct *, int);
void Ph_init_hsp_struct(cfp_struct *);
void Ph_SetScbMark (cfp_struct *);
UBYTE Ph_GetScbStatus (hsp_struct *,sp_struct *);
void Ph_SetMgrStat (sp_struct *);
#ifdef   _FAULT_TOLERANCE
void Ph_BusDeviceReset (sp_struct *);
void Ph_ScbAbort (sp_struct *);
void Ph_ScbRenego (cfp_struct *,UBYTE);
void Ph_ScbAbortActive (cfp_struct *,sp_struct *);
#endif   /* _FAULT_TOLERANCE */

/*********************************************************************
*  HIMDOPT.C
*********************************************************************/
void Ph_OptimaEnque(UBYTE,sp_struct *);
void Ph_OptimaEnqueHead(UBYTE,sp_struct *, AIC_7870 *);
void Ph_OptimaQHead (UBYTE,sp_struct *, AIC_7870 *);
void Ph_OptimaCmdComplete(cfp_struct *, UBYTE);
void Ph_OptimaRequestSense(sp_struct *, UBYTE);
void Ph_OptimaClearDevQue(sp_struct *, AIC_7870 *);
void Ph_OptimaIndexClearBusy(cfp_struct *, UBYTE);
void Ph_OptimaCheckClearBusy(cfp_struct *, UBYTE);
int Ph_OptimaAbortActive(sp_struct *);
void Ph_OptimaClearChannelBusy(cfp_struct *);
void Ph_set_scratchmem(cfp_struct *);
UBYTE Ph_OptimaGetFreeScb(hsp_struct *, sp_struct *);
void Ph_OptimaAbortScb(sp_struct *, UBYTE);
void Ph_OptimaClearQinFifo(cfp_struct *);
void Ph_MovPtrToScratch(UWORD,DWORD, cfp_struct *);
UBYTE * Ph_ScbPageJustifyQIN(cfp_struct *);
void Ph_OptimaClearTargetBusy (cfp_struct *, UBYTE);
#endif  /* DRIVER */

