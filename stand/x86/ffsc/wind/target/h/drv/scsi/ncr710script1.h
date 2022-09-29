/* ncr710Script1.h - NCR 710 Script definition header */

/* Copyright 1984-1994 Wind River Systems, Inc. */

/*
modification history
--------------------
01j,08nov94,jds  renamed for SCSI1 compatability
01i,22sep92,rrr  added support for c++
01h,01sep92,ccc	 fixed warnings.
01g,03jul92,eve  change NCRBUG to NCR_COMPILER_BUG
01f,03jul92,eve  Merge new header
01e,26jun92,ccc  change ASMLANGUAGE to _ASMLANGUAGE.
40b,26may92,rrr  the tree shuffle
40a,12nov91,ccc  SPECIAL VERSION FOR 5.0.2 68040 RELEASE.
02a,26oct91,eve  Add byte definition to turnoff/on the timeout in the script.
01a,23oct91,eve	 Created script and driver header
*/

#ifndef __INCncr710Script1h
#define __INCncr710Script1h

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _ASMLANGUAGE


/* Just used to compile the primary file .n that contain
 * source code for the ncr compiler.
 */

#ifdef NCR_COMPILER_BUG
typedef unsigned char   UINT8;
typedef unsigned int    ULONG;
typedef unsigned int    UINT;
#endif /* NCR_COMPILER_BUG */

/* Because this file will be include in a script source
 * do not include vxWorks header.
 */

/* Structure for the script.  Script takes information from
 * this structure, don't change count/pointer group order
 * All of these fields must be aligned on a 32bit boundry
 */

typedef struct ncrCtl
    {
    UINT device;	    /* target description. */
    UINT msgOutCount;	    /* count for message out. */
    UINT8 *pMsgOut;	    /* message out area. */
    UINT msgInCount;	    /* count message in (always 1).*/
    UINT8 *pMsgIn;	    /* message in area. */
    UINT  extMsgInCount;    /* count ext message in (always 2).*/
    UINT8 *pExtMsgIn;	    /* extended message in area. */
    UINT cmdCount;	    /* 24 bit count for scsi command. */
    UINT8 *pCmd;	    /* pointer to scsi command. */
    UINT dataCount;	    /* 24 bit user data count. */
    UINT8 *pData;	    /* pointer to user data buffer. */
    UINT  statusCount;	    /* length of status scsi (always 1). */
    UINT8 *pScsiStatus;	    /* pointer for scsi status. */
    /* information for reconnect and save pointer. */
    UINT identCount;        /* msg in reselect message count. */
    UINT8 *pIdentMsg;       /* pointer to local IDent reconnect Message. */
    UINT scriptAddr;        /* script start address. */
    UINT *pScsiPhysDev;     /* pointer to phys dev info for this nexus. */
    UINT phaseRequested;    /* save phase requested by the script at int lvl. */
    /* informations supply by ncr preprocessor. */
    UINT *pReloc;	    /* pointer to reloc table. */
    UINT *pFirstRtn;	    /* pointer to the first script routine. */
			    /* (Entry point). */
    UINT *scriptPtr;	    /* pointer to table of addresses */
			    /* of script routines. */
    } NCR_CTL;

/* offset used literally in the script, if a change occurs
 * in ncrCtrl structure above you have to change the offset value here
 * Because it seems that the ncr compiler does not like a long and
 * nested in PASS declaration does not use a macro
 */
#define DEVICEID	0x00
#define MSGOUTCOUNT	0x04
#define PMSGOUTCOUNT	0x08
#define MSGINCOUNT	0x0C
#define PMSGIN		0x10
#define EXTMSGINCOUNT	0x14
#define PEXTMSGIN	0x18
#define CMDCOUNT	0x1C
#define PCMD		0x20
#define DATACOUNT	0x24
#define PDATA		0x28
#define STATUSCOUNT     0x2C
#define PSTATUS		0x30
#define IDENTCOUNT      0x34
#define PIDENTMSG       0x38

/* Field values for relocate script */
/* definitions for the relocation table entry_id fields */
#define SCRIPT_BASE     0x98080000      /* IDentifies a script ptr entry */
#define MEM_SIZE        0x0F000001      /* IDentifies a script byte size entry*/
#define DATA_OFFSET     0x88080000      /* IDentifies a data offset entry */
#define CODE_OFFSET     0x80080000      /* IDentifies a code offset entry */
#define TABLE_END       0x60000040      /* IDentifies a the end of reloc tbl */

/* relocation entry */
typedef struct relocInfo
    {
    UINT entryType;	/* type of Entry */
    UINT entryValue;	/* entry offset */
    } RELOC_INFO;

/* use both by C and script via scratch register to keep track of the phase
 * requested.
 */
#define PHASE_DATAOUT      0x00            /* data out (to target) */
#define PHASE_DATAIN       0x01            /* data in (from target) */
#define PHASE_COMMAND      0x02            /* command (to target) */
#define PHASE_STATUS       0x03            /* status (from target) */
#define PHASE_UNDEF1       0x04            /* reserved (to target) */
#define PHASE_UNDEF2       0x05            /* reserved (from target) */
#define PHASE_MSGOUT       0x06            /* message out (to target) */
#define PHASE_MSGIN        0x07            /* message in (from target) */
#define PHASE_DISCON       0x08            /* disconnect */
#define PHASE_RESEL        0x09            /* ident msg */
#define PHASE_NOPHASE      0x0a            /* phase has yet to be determined */

/* Scsi initiator message type Cut/Paste from scsiLib (used in script) */
#define M_CMD_COMP              (0x00) /* command complete msg. */
#define M_EXT_MSG               (0x01) /* extended message msg. */
#define M_SAV_DAT_P             (0x02) /* save data pointer msg. */
#define M_REST_P                (0x03) /* restore pointers msg. */
#define M_DISC                  (0x04) /* disconnect msg. */
#define M_INITOR_DETECT_ERR     (0x05) /* initor. detect err msg. */
#define M_ABORT                 (0x06) /* abort msg. */
#define M_MSG_REJECT            (0x07) /* message reject msg. */
#define M_NO_OP                 (0x08) /* no operation msg. */
#define M_PARITY_ERR            (0x09) /* message parity err msg. */
#define M_LINK_CMD_COMPLETE     (0x0a) /* linked cmd. comp. msg. */
#define M_LINK_CMD_FLAG_COMP    (0x0b) /* link cmd w/flag comp. */
#define M_BUS_DEVICE_RESET      (0x0c) /* bus device reset msg. */
#define M_IDENT_DISCONNECT      (0x40) /* identify disconnect bit */
#define M_IDENTIFY              (0x80) /* identify msg bit mask */

#ifndef NCRCOMPILE

/* due to a script compiler bug */
#ifdef NCR_COMPILER_BUG
#define EQ =
#endif /* NCR_COMPILER_BUG */

/* Return value of intr script instruction for C code. 	*/
/* As these definitions are share between script and C.	*/

#define MSGOUT_EXPECT		0x0ff01	/* No message out after a selection. */
#define BAD_PH_BEFORE_CMD 	0x0ff02	/* unexecpected phase before command */
					/* phase. */
#define BAD_PH_AFTER_CMD 	0x0ff03	/* unexpected phase after command */
					/* phase. */
#define MSGIN_EXPECT_AFTER_ST 	0x0ff04	/* no message in phase after status */
					/* phase. */
#define GOOD_END 		0x0ff00	/* transaction process properly */
					/* finished. */
#define ST_CHECK_CONDITION 	0x0fffe	/* scsi status :check cond. */
#define ST_BUSY 		0x0fffd	/* scsi status :busy. */
#define ST_RESERVED 		0x0fffc	/* scsi status :reservation conflict. */
#define ST_UNKNOWN 		0x0fffb	/* status returned is unknown. */
#define BAD_PH_AFTER_DATA 	0x0ff05	/* unexepected phase after a data */
					/* transfert. */
#define BAD_MSGIN_BEFORE_CMD 	0x0ff06	/* unexpected message in before a */
					/* command phase. */
#define EXTMSG_BEFORE_CMD 	0x0ff07	/* extended message present before */
					/* a command phase. */
#define SAVDATP_BEFORE_CMD 	0x0ff08	/* save data pointer before the */
					/* command phase. */
#define DISC_BEFORE_CMD 	0x0ff09	/* disconnect before the command */
					/* phase. */
#define SAVDATP_AFTER_CMD 	0x0ff10	/* save data pointer after the */
                                        /* command phase. */
#define BAD_MSG_AFTER_CMD 	0x0ff11	/* unexpected message after the */
					/* command phase. */
#define EXTMSG_AFTER_CMD 	0x0ff12	/* extended message present after the */
					/* command phase. */
#define DISC_AFTER_CMD 		0x0ff13	/* disconnect after a command phase. */
#define SAVDATP_AFTER_DATA 	0x0ff14	/* save data pointer after a data */
					/* transfer. */
#define BAD_MSG_AFTER_DATA 	0x0ff15	/* unexpected message after a data */
					/* transfer. */
#define EXTMSG_AFTER_DATA 	0x0ff16	/* extended message after a data */
					/* transfer. */
#define DISC_AFTER_DATA 	0x0ff17	/* disconnect after a data transfer. */
#define MSGIN_NOT_AFTER_RESEL 	0x0ff18	/* not a message in phase after a */
					/* reselection. */
#define DATIN_AFTER_RESEL_IDRECV   0x0ff19 /* data in after reselect/message */
					   /* identify received. */
#define DATOUT_AFTER_RESEL_IDRECV  0x0ff20 /* data out after reselection */
					   /* message ident received */
#define MSGIN_AFTER_RESEL_IDRECV   0x0ff21 /* msg in after reselect and */
					   /* identify message received.*/
#define ST_AFTER_RESEL_IDRECV 	   0x0ff22 /* status after reselect and */
					   /* identify message received. */
#define MSGOUT_AFTER_RESEL_IDRECV  0x0ff23 /* message out out after reselect */
					   /* and identify message received. */
#define PH_UNKNOWN_IDRECV 	   0x0ff24 /* unknown phase after reselect */
                                           /* and identify message received. */
#define SELECT_AS_TARGET 	   0x0ff25 /* select as a target. */
#define BAD_MSG_INSTEAD_CMDCOMP    0x0ff26 /* unexpected message instead of */
					   /* command complete message. */
#define REJECT_MSG1                0x0ff27 /* received a reject message. */
#define ABORT_CLEAR_END            0x0ff28 /* finish script because bus free */
					   /* phase condition received. */
#define PH_UNKNOWN                 0x0ff29 /* unknown phase after sortPhase. */
#define RES_IN_DETECTED            0x0ff2a /* Reserved phase detected. */
#define RES_OUT_DETECTED           0x0ff2b /* Reserved phase detected. */
#define RECONNECT_PROCESS          0x0ff2c /* interrupt cpu for reconnected. */
#define NEW_COMMAND_PROCESS        0x0ff2d /* interrupt cpu to run a new */
					   /* command. */
#define RESTORE_POINTER            0x0ff2e /* interrupt cpu to restore ptr. */
#define BAD_NEW_CMD                0x0ff2f /* Bogus new command. */
#define RECONNECT_IN_SELECT        0x0ff30 /* Reconnection during a select. */

#else /* NCRCOMPILE */

ABSOLUTE B_TIMEO=0x10		;/* Timout timer bit */
ABSOLUTE B_CON=0x10		;/* Connected connection status bit */
ABSOLUTE B_SIGP=0x20		;/* SIGP bit position */
#endif  /* NCRCOMPILE */


#endif  /* _ASMLANGUAGE */
#ifdef __cplusplus
}
#endif

#endif /* __INCncr710Script1h */
