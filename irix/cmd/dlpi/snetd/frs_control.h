
/*
 * Copyright (c) 1995 Spider Systems Limited
 *
 * This Source Code is furnished under Licence, and may not be
 * copied or distributed without express written agreement.
 *
 * All rights reserved.  Made in Scotland.
 *
 * Authors: Bill Byers, Fraser Moore, George Wilkie, Ian Wilson
 *
 * frs_control.h of snet module
 *
 * SpiderFRAME-RELAY
 * Release 1.0.3 95/06/15
 * 
 * 
 */




/* FRS STREAMS ID */
#define FRS_STID        302


/* IOCTL failures */
#define SETSNID_PPAEXISTS	-EEXIST
#define LINK_EXHAUSTED		-ENOSPC
#define TUNE_INVSTATE		-EBUSY
#define TUNE_INVSTANDARD	-EINVAL
#define TUNE_PPANOTFOUND	-ENODEV
#define TUNE_WANINIT_FAIL	-ENOSR
#define CTUNE_INVDLCI		-EINVAL
#define CTUNE_PPANOTFOUND	-ENODEV
#define CTUNE_DLCIEXISTS	-EEXIST
#define CTUNE_EXHAUSTED		-ENOSPC
#define CTUNE_DLCINOTFOUND	-ENODEV
#define STATUS_PPANOTFOUND	-ENODEV
#define STATS_PPANOTFOUND	-ENODEV
#define STATS_DLCINOTFOUND	-ENODEV
#define DEREGPVC_PPANOTFOUND	-ENODEV
#define DEREGPVC_DLCINOTFOUND	-ENODEV

/* IOCTL commands */
#define FR_SETSNID		(('F'<<8) | 1)
#define FR_GETSNID		(('F'<<8) | 2)
#define FR_SETTUNE		(('F'<<8) | 3)
#define FR_GETTUNE		(('F'<<8) | 4)
#define FR_SETCTUNE		(('F'<<8) | 5)
#define FR_GETCTUNE		(('F'<<8) | 6)
#define FR_GETSTATUS	(('F'<<8) | 7)
#define FR_GET_PPASTATS	(('F'<<8) | 8)
#define FR_ZERO_PPASTATS	(('F'<<8) | 9)
#define FR_GET_PVCSTATS	(('F'<<8) | 10)
#define FR_ZERO_PVCSTATS	(('F'<<8) | 11)
#define FR_TRACEON		(('F'<<8) | 12)
#define FR_TRACEOFF		(('F'<<8) | 13)
#define FR_DEREGPVC		(('F'<<8) | 14)

struct fr_snid_ioc {
	unsigned char fr_type;
	unsigned char fr_spare[3];
	unsigned long fr_snid_ppa;
	unsigned long fr_snid_index;
};

/* definitions for the defaults */
#define T391_DEFAULT   10
#define T392_DEFAULT   15
#define N391_DEFAULT   6
#define N392_DEFAULT   3
#define N393_DEFAULT   4
#define MAXFRAME_DEFAULT  1600
#define ACCESSRATE_DEFAULT 10000000

/* definitions for fr_ppa_standard */
#define LMISTYLE_BITMASK	0XC0
#define LMISTYLE_RESPONDER  0X80
#define LMISTYLE_DONTPOLL   0X40

#define ITU_INUSE		0
#define ANSI_INUSE		1
#define OLDANSI_INUSE		2
#define OGOF_INUSE		3

struct fr_ppa_ioc {
	unsigned char fr_type;
	unsigned char fr_spare[3];
	unsigned long fr_ppa_ppa;

	struct fr_ppa_tune {
		unsigned long dlcilength;
		unsigned long lmidlci;
		unsigned long T391;
		unsigned long T392;
		unsigned long N391;
		unsigned long N392;
		unsigned long N393;
		unsigned long maxframesize;
		unsigned long accessrate;
		unsigned long standard;
	} fr_ppa;
};


/* definitions for fr_pvc_flowstyle */
#define FLOW_USE_FECN   0x01
#define FLOW_USE_BECN   0x02
#define FLOW_USE_CLLM   0x04

struct fr_pvc_ioc {
	unsigned char fr_type;
	unsigned char fr_spare[3];
	unsigned long fr_pvc_ppa;
	unsigned long fr_pvc_dlci;
	struct fr_pvc_tune {
		unsigned long cir;
		unsigned long Bcommit;
		unsigned long Bexcess;
		unsigned long eetd;
		unsigned long stepcount;
		unsigned long flowstyle;
	} fr_pvc;
};

#define STATUS_BUFSIZE		1024

struct fr_status_ioc {
	unsigned char fr_type;
	unsigned char fr_spare[3];
	unsigned long fr_status_ppa;
	unsigned long fr_status_netstat;
	unsigned long fr_status_pvccount;
};


struct fr_ppastats_ioc {
	unsigned char fr_type;
	unsigned char fr_spare[3];
	unsigned long fr_ppastats_ppa;
	struct fr_ppa_stats {
		unsigned long timezeroed;
		unsigned long txframes;
		unsigned long rxframes;
		unsigned long txbytes;
		unsigned long rxbytes;
		unsigned long txlmipolls;
		unsigned long rxfullstat;
		unsigned long rxseqonly;
		unsigned long rxasynchs;
		unsigned long rxcllms;
		unsigned long lmierrors;
		unsigned long lmitimeouts;
		unsigned long rxtoobig;
		unsigned long rxinvDLCI;
		unsigned long rxunattDLCI;
		unsigned long rxdrops;
		unsigned long txinvrq;
		unsigned long rxinvrq;
		unsigned long wanflows;
		unsigned long LMIwanflows;
		unsigned long txstops;
		unsigned long txnobuffs;
		unsigned long rxasynch;
		unsigned long rxlmipolls;
		unsigned long rxpollerrs;
		unsigned long txfullstat;
		unsigned long txseqonly;
		unsigned long txasynchs;
		unsigned long txcllms;
	} fr_ppa;
};

/* additional bits set in fr_pvc_status */
#define PVCSTATUS_STOPPED  0x10000
#define PVCSTATUS_TXMSGS   0x20000
#define PVCSTATUS_RXMSGS   0x40000

struct fr_pvcstats_ioc {
	unsigned char fr_type;
	unsigned char fr_spare[3];
	unsigned long fr_pvcstats_ppa;
	unsigned long fr_pvcstats_dlci;
	struct fr_pvc_stats {
		unsigned long timezeroed;
		unsigned long txframes;
		unsigned long rxframes;
		unsigned long txbytes;
		unsigned long rxbytes;
		unsigned long txstops;
		unsigned long rxstops;
		unsigned long rxFECNs;
		unsigned long rxBECNs;
		unsigned long spare;
		unsigned long status;
		unsigned long statuschangetime;
		unsigned long cirlowat;
		unsigned long cirhiwat;
	}fr_pvc;
};

struct fr_trace_ioc {
	unsigned char fr_type;
	unsigned char fr_spare[3];
	unsigned long fr_trace_ppa;
	unsigned long fr_trace_dlci;
	unsigned long fr_trace_flags;
};

struct fr_deregpvc_ioc {
	unsigned char fr_type;
	unsigned char fr_spare[3];
	unsigned long fr_deregpvc_ppa;
	unsigned long fr_deregpvc_dlci;
};

