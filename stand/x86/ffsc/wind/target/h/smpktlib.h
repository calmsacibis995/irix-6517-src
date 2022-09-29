/* smPktLib.h - include file for VxWorks shared packets protocol library */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
02e,22sep92,rrr  added support for c++
02d,11sep92,ajm  moved redundant define of DEFAULT_BEATS_TO_WAIT to smLib.h
02c,29jul92,pme  changed DEFAULT_CPUS_MAX to 10.
02b,24jul92,elh  Moved heartbeat to header from anchor.
02a,04jul92,jcf  cleaned up.
01h,02jun92,elh  the tree shuffle
01g,27may92,elh	 split from shMemLib, general cleanup.
01f,13may92,pme  Added smObjHeaderAdrs fiels to anchor.
01e,03may92,elh  Added smNetReserved fields to anchor.
01d,01apr92,elh	 Removed references to shMemHwTasFunc and
		 shMemIntGenFunc (now in smUtilLib).
		 Removed interrupt types.
01c,04feb92,elh	 ansified
01b,17dec91,elh  externed hooks, added ifdef around file, changed VOID
		 to void.  Added masterCpu, user1 and user2, to SM_ANCHOR.
		 Added S_shMemLib_MEMORY_ERROR. Changed copyright.
01a,15aug90,kdl	 written.
*/

#ifndef __INCsmPktLibh
#define __INCsmPktLibh

#ifdef __cplusplus
extern "C" {
#endif

/* includes */

#include "vwmodnum.h"
#include "smlib.h"

/* defines */

/* Error codes */

#define S_smPktLib_SHARED_MEM_TOO_SMALL		(M_smPktLib | 1)
#define S_smPktLib_MEMORY_ERROR			(M_smPktLib | 2)
#define S_smPktLib_DOWN				(M_smPktLib | 3)
#define S_smPktLib_NOT_ATTACHED			(M_smPktLib | 4)
#define S_smPktLib_INVALID_PACKET		(M_smPktLib | 5)
#define S_smPktLib_PACKET_TOO_BIG		(M_smPktLib | 6)
#define S_smPktLib_INVALID_CPU_NUMBER		(M_smPktLib | 7)
#define S_smPktLib_DEST_NOT_ATTACHED		(M_smPktLib | 8)
#define S_smPktLib_INCOMPLETE_BROADCAST		(M_smPktLib | 9)
#define S_smPktLib_LIST_FULL			(M_smPktLib | 10)
#define S_smPktLib_LOCK_TIMEOUT			(M_smPktLib | 11)

/* Miscellaneous Constants */

#define SM_BROADCAST	0xbbbbbbbb	/* dest cpu number to send to all cpus*/

#define SM_FLUSH	0		/* dont flush queued packets */
#define SM_NO_FLUSH	1

/* default values */

#define DEFAULT_MEM_SIZE	0x10000	/* default memory size */
#define DEFAULT_PKT_SIZE	2176 	/* default packet size */
#define DEFAULT_PKTS_MAX	200	/* max input packets */
#define DEFAULT_CPUS_MAX	10	/* max number of cpus */

#if CPU_FAMILY==I960
#pragma align 1				/* don't to optimize alignments */
#endif /* CPU_FAMILY==I960 */

/* Packet List Node */

typedef struct sm_sll_node      /* SM_SLL_NODE */
    {
    int  	next;  		/* ptr to next node in list */
    } SM_SLL_NODE;


/* Packet Singly-Linked List */

typedef struct sm_sll_list	/* SM_SLL_LIST */
    {
    int         lock;           /* mutual exclusion lock (for TAS) */
    int  	head;        	/* head (first node) of list (offset) */
    int 	tail;        	/* tail (last node) of list (offset) */
    int         count;          /* number of packets currently in list */
    int         limit;          /* max number of packets allowed in list */
    } SM_SLL_LIST;

/* Packet Header */

typedef struct sm_pkt_hdr	/* SM_PKT_HDR */
    {
    SM_SLL_NODE	node;		/* node header for linked lists */
    int		type;		/* packet type */
    int		nBytes;		/* number of bytes of data in packet */
    int		srcCpu;		/* source CPU number */
    int		ownerCpu;	/* owner CPU number (future use) */
    int		reserved1;	/* (future use) */
    int		reserved2;	/* (future use) */
    } SM_PKT_HDR;

/* Packet */

typedef struct sm_pkt 		/* SM_PKT */
    {
    SM_PKT_HDR	header;		/* packet header */
    char	data [1];	/* data buffer (actual size = maxPktBytes) */
    } SM_PKT;


/* per CPU Packet Descriptor */

typedef struct sm_pkt_cpu_desc  /* SM_PKT_CPU_DESC */
    {
    int         status;         /* CPU status - attached/unattached */
    SM_SLL_LIST inputList;      /* input list of packets */
    SM_SLL_LIST freeList;       /* free list of packets (future use) */
    } SM_PKT_CPU_DESC;


/* Shared Memory Packet Memory Header */

typedef struct sm_pkt_mem_hdr	/* SM_PKT_MEM_HDR */
    {
    UINT	  heartBeat; 	/* incremented via smPktBeat() */
    SM_SLL_LIST   freeList;	/* global list of free packets */
    int 	  pktCpuTbl;	/* packet descriptor table (offset) */
    int           maxPktBytes;	/* max size of packet data (in bytes) */
    int		  reserved1;	/* (future use) */
    int		  reserved2;	/* (future use) */
    } SM_PKT_MEM_HDR;

#if CPU_FAMILY==I960
#pragma align 0				/* turn off alignment requirement */
#endif /* CPU_FAMILY==I960 */

typedef struct sm_pkt_desc	/* SM_PKT_DESC */
    {
    int			status;
    SM_DESC		smDesc;		/* shared memory descriptor */
    int         	maxPktBytes;   	/* max size of packet buffer */
    int         	maxInputPkts;	/* max packets allowed in queue */
    SM_PKT_MEM_HDR *	hdrLocalAdrs;	/* pkt memory header local adrs */
    SM_PKT_CPU_DESC *	cpuLocalAdrs;	/* pkt cpu local adrs */
    } SM_PKT_DESC;

/* Shared Memory Packet Information Structure */

typedef struct sm_pkt_info 	/* SM_INFO */
    {
    SM_INFO	smInfo;
    int		attachedCpus;	/* number of cpu's currently attached */
    int		maxPktBytes;	/* max number of data bytes in packet */
    int		totalPkts;	/* total number of sh mem packets */
    int		freePkts;	/* number of packets currently free */
    } SM_PKT_INFO;

/* CPU Information Structure */

typedef struct sm_pkt_cpu_info	/* SM_CPU_INFO */
    {
    SM_CPU_INFO	smCpuInfo;
    int		status;		/* cpu status - attached/unattached */
    int		maxInputPkts;	/* max packets allowed in input queue */
    int		inputPkts;	/* current count of input pkts queued */
    int		totalPkts;	/* (future use) */
    int		freePkts;	/* (future use) */
    } SM_PKT_CPU_INFO;


/* Function Declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern STATUS 	smPktFreeGet (SM_PKT_DESC *pSmPktDesc, SM_PKT **ppPkt);
extern STATUS 	smPktFreePut (SM_PKT_DESC *pSmPktDesc, SM_PKT *pPkt);
extern STATUS 	smPktRecv (SM_PKT_DESC *pSmPktDesc, SM_PKT **ppPkt);
extern STATUS 	smPktSend (SM_PKT_DESC *pSmPktDesc, SM_PKT *pPkt, int destCpu);
extern STATUS 	smPktSetup (SM_ANCHOR * anchorLocalAdrs, char * smLocalAdrs,
			    int smSize,	int tasType, int maxCpus,
			    int maxPktBytes);
extern void 	smPktInit (SM_PKT_DESC *pSmPktDesc, SM_ANCHOR *anchorLocalAdrs,
			   int maxInputPkts, int ticksPerBeat, int intType,
			   int intArg1, int intArg2, int intArg3);
extern STATUS 	smPktAttach (SM_PKT_DESC *pSmPktDesc);
extern STATUS 	smPktDetach (SM_PKT_DESC *pSmPktDesc, BOOL noFlush);
extern STATUS 	smPktInfoGet (SM_PKT_DESC *pSmPktDesc, SM_PKT_INFO *pInfo);
extern STATUS 	smPktCpuInfoGet (SM_PKT_DESC *pSmPktDesc, int cpuNum,
			         SM_PKT_CPU_INFO *pCpuInfo);
extern void 	smPktBeat (SM_PKT_MEM_HDR *pSmPktHdr);


#else	/* __STDC__ */

extern STATUS 	smPktFreeGet ();
extern STATUS 	smPktFreePut ();
extern STATUS 	smPktInfoGet ();
extern STATUS 	smPktRecv ();
extern STATUS 	smPktSend ();
extern STATUS 	smPktSetup ();
extern void 	smPktInit ();
extern STATUS 	smPktDetach ();
extern STATUS 	smPktInfoGet ();
extern STATUS 	smPktCpuInfoGet ();
extern void 	smPktBeat ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCsmPktLibh */
