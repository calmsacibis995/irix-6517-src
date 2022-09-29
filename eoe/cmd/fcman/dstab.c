/*
 *  dstab.c -- devscsi tables
 *
 *	Copyright 1988, 1989, by
 *	Gene Dronek (Vulcan Laboratory) and
 *	Rich Morin  (Canta Forda Computer Laboratory).
 *	All Rights Reserved.
 *	Ported to SGI by Dave Olson 3/89
 *
 *	This file is linked with user programs using dslib
 *	to control SCSI Common Command Set Devices.
 *
 *		Tables
 *			devscsi request flags
 *			devscsi return codes
 *			status-byte
 *			message-byte
 *			command-name
 */
#ident "dstab.c: $Revision: 1.1 $"

#include "dslib.h"


/*
 *  DEVSCSI request flags
 */

struct vtab dsrqnametab[] = {
/*  request options	 */
  "DSRQ_ASYNC",    0x1,      "no (yes) sleep until request done",
  "DSRQ_SENSE",    0x2,	     "(no) auto get sense on status",
  "DSRQ_TARGET",   0x4,	     "target (initiator) role",
  "DSRQ_SELATN",   0x8,	     "select with (without) atn",
  "DSRQ_DISC",     0x10,     "identify disconnect (not) allowed",
  "DSRQ_SYNC",     0x20,     "(no) synchronous-xfer",
  "DSRQ_SELMSG",   0x40,     "send supplied (generate) message",
  "DSRQ_IOV",      0x80,     "scatter-gather (not) specified",
  "DSRQ_READ",     0x100,    "input from scsi bus",
  "DSRQ_WRITE",    0x200,    "output to  scsi bus",
  "DSRQ_BUF",      0x400,    "buffered (direct) data xfer",
  "DSRQ_CALL",     0x800,    "notify progress (completion-only)",
  "DSRQ_ACKH",     0x1000,   "hold (don't) ACK asserted",
  "DSRQ_ATNH",     0x2000,   "hold (don't) ATN asserted",
  "DSRQ_ABORT",    0x4000,   "abort msg until bus clear",
  "DSRQ_TRACE",    0x8000,   "trace (no) this request",
  "DSRQ_PRINT",    0x10000,  "print (no) this request",
  "DSRQ_CTRL1",    0x20000,  "host control bit 1",
  "DSRQ_CTRL2",    0x40000,  "host control bit 2",
  "DSRQ_INTERNAL", 0x80000,  "internal kernel request",
  "DSRQ_MIXRDWR",  0x100000, "request mixed read/write",
  0,0,0 ,
};

 
/*
 *  DEVSCSI return codes
 */

struct vtab dsrtnametab[] = {
  "DSRT_STAI",    0x21,	 "error during status phase",	
  "DSRT_CMDO",    0x22,  "error during command phase",	
  "DSRT_MEMORY",  0x23,  "host memory error",		
  "DSRT_PARITY",  0x24,  "parity error on SCSI bus",	
  "DSRT_REJECT",  0x25,	 "message reject",		
  "DSRT_EBSY",    0x26,	 "busy dropped unexpectedly",	
  "DSRT_PROTO",   0x27,	 "misc. protocol failure",	
  "DSRT_LONG",    0x30,	 "target overran data bounds",	
  "DSRT_TIMEOUT", 0x31,	 "request timed out",		
  "DSRT_NOSENSE", 0x32,	 "cmd w/status, error w/ sense",	
  "DSRT_SENSE",   0x33,	 "cmd w/ status, sense gotten",	
  "DSRT_OK",      0x34,	 "cmd complete, no errors",	
  "DSRT_SHORT",   0x35,	 "incomplete xfer",		
  "DSRT_NOSEL",   0x36,	 "no response to select",	
  "DSRT_HOST",    0x37,	 "misc. host failure",		
  "DSRT_AGAIN",   0x43,	 "Retry--transient error",	
  "DSRT_REVCODE", 0x44,	 "s/w version mismatch",		
  "DSRT_CANCEL",  0x45,	 "lower request cancelled",	
  "DSRT_MULT",    0x46,	 "request rejected by host",	
  "DSRT_DEVSCSI", 0x47,	 "misc. devscsi error",		
  0,0,0
};

/*
 *  SCSI command status byte
 */

struct vtab cmdstatustab[] = {
  "STA_GOOD",     0x00,  "GOOD",				
  "STA_CHECK",    0x02,  "CHECK CONDITION",		
  "STA_BUSY",     0x08,  "BUSY",				
  "STA_IGOOD",    0x10,  "INTERM/GOOD",			
  "STA_RESERV",   0x18,  "RESERV CONFL",			
  0,0,0
};


/*
 *  SCSI message bytes
 */

struct vtab  msgnametab[]  =  {
 "MSG_COMPL",     0x00,  "Command Complete",		
 "MSG_XMSG",      0x01,  "Extended Message",		
 "MSG_SAVEP",     0x02,  "Save Pointers",		
 "MSG_RESTP",     0x03,  "Restore Pointers",		
 "MSG_DISC",      0x04,  "Disconnect",			
 "MSG_IERR",      0x05,  "Initiator Err",		
 "MSG_ABORT",     0x06,  "Abort",			
 "MSG_REJECT",    0x07,  "Message Reject",		
 "MSG_NOOP",      0x08,  "No Operation",		
 "MSG_MPARITY",   0x09,  "Message Parity",		
 "MSG_LINK",      0x0A,  "Linked Cmd Complete",	
 "MSG_LINKF",     0x0B,  "Lnkd w/flg Complete",	
 "MSG_BRESET",    0x0C,  "Bus Device Reset",		
 "MSG_IDENT",     0x80,  "Targ Ident Lun 0",		
 "",   		  0xC0,  "Init Ident Lun 0",		
 0,0,0
};


/*
 *  SCSI sense bytes
 */

struct ctab sensekeytab[] = {
  0x00,	"No Sense Information",
  0x02,	"Not Ready",
  0x04,	"Hardware Error",
  0x05, "Illegal Request",
  0x06,	"Unit Attention",
  0x09,	"Other Error",
  0,0
};


/*
 *  SCSI command names
 */

struct vtab cmdnametab[] = {
  "G0_TEST",	   0x00,  "Test Unit",
  "G0_REWI",	   0x01,  "Rewind/Rezero Unit",
  "G0_REZE",	   0x01,  "Rezero",		
  "G0_DOWN",	   0x02,  "Download",		
  "G0_REQU",	   0x03,  "Request Sense",
  "G0_FORM",	   0x04,  "Format Unit",
  "G0_RBL",	   0x05,  "Read Block Limits",
  "G0_DRWB",	   0x05,  "Draw Bits",
  "G0_CLRB",	   0x06,  "Clear Bits",
  "G0_REAS",	   0x07,  "Reassign Blocks",
  "G0_READ",	   0x08,  "Read/Receive",
  "G0_RECE",	   0x08,  "Receive",		
  "G0_WRIT",	   0x0A,  "Write/Print",
  "G0_PRIN",	   0x0A,  "Print",		
  "G0_SEEK",	   0x0B,  "Seek",
  "G0_SLEW",	   0x0B,  "Slew & Print",	
  "G0_TSEL",	   0x0B,  "Track Select",
  "G0_RR",	   0x0F,  "Read Reverse",
  "G0_WF",	   0x10,  "Write Filemark",
  "G0_FLUS",	   0x10,  "Flush Buffer",	
  "G0_SPAC",	   0x11,  "Space",
  "G0_INQU",	   0x12,  "Inquiry",
  "G0_VERI",	   0x13,  "Verify",
  "G0_RBD",	   0x14,  "Recover Buf Data",
  "G0_MSEL",	   0x15,  "Mode Select",
  "G0_RESU",	   0x16,  "Reserve Unit",
  "G0_RELU",	   0x17,  "Release Unit",
  "G0_COPY",	   0x18,  "Copy",
  "G0_ERAS",	   0x19,  "Erase",
  "G0_MSEN",	   0x1A,  "Mode Sense",
  "G0_STOP",	   0x1B,  "Start/Stop Unit",
  "G0_LOAD",	   0x1B,  "Load/Unload",		
  "G0_STPR",	   0x1B,  "Stop",		
  "G0_SCAN",	   0x1B,  "Scan",		
  "G0_RDR",	   0x1C,  "Read Diag Response",
  "G0_SD",	   0x1D,  "Send Diagnostic",
  "G0_PREV",	   0x1E,  "Media Removal",
  "G0_RLOG",	   0x1F,  "Read Log",
  
  "G1_DWP",	   0x24,  "Define Window Parms",
  "G1_GWP",	   0x24,  "Get Window Parms",	
  "G1_RCAP",	   0x25,  "Read Capacity",
  "G1_REAS",	   0x27,  "Reassign Blocks",
  "G1_RX",	   0x28,  "Read Extended",
  "G1_WRIT",	   0x2A,  "Write/Send",
  "G1_WX",	   0x2A,  "Write Extended",	
  "G1_SX",	   0x2B,  "Seek Extended",	
  "G1_LOCA",	   0x2B,  "Locate",		
  "G1_SEEK",	   0x2B,  "Seek",		
  "G1_WVER",	   0x2E,  "Write & Verify",
  "G1_VERI",	   0x2F,  "Verify",
  "G1_SDH",	   0x30,  "Search Data High",
  "G1_SDE",	   0x31,  "Search Data Equal",
  "G1_SDL",	   0x32,  "Search Data Low",
  "G1_MPOS",	   0x32,  "Medium Position",	
  "G1_SLIM",	   0x33,  "Set Limits",
  "G1_PF",	   0x34,  "Pre-Fetch",
  "G1_RPOS",	   0x34,  "Read Position",	
  "G1_GDS",	   0x34,  "Get Data Status",	
  "G1_FCAC",	   0x35,  "Flush Cache",
  "G1_LCAC",	   0x36,  "Lock/Unlock Cache",
  "G1_RDD",	   0x37,  "Read Defect Data",
  "G1_COMP",	   0x39,  "Compare",
  "G1_CVER",	   0x3A,  "Copy & Verify",
  "G1_WBUF",	   0x3B,  "Write Buffer",
  "G1_RBUF",	   0x3C,  "Read Buffer",
  "G1_RLON",	   0x3E,  "Read Long",
  "G1_WLON",	   0x3F,  "Write Long",
  0,0,0,
};
