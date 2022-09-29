/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 *  new 5/96 PDR							  *
 **************************************************************************/

#ident "Revision"

/*
 *  Event definitions for Data Pipes.
 *
 */

#ifndef _DATAPIPE_EVENTS_H
#define _DATAPIPE_EVENTS_H

 /* This file contains the base (class) set of Data Pipe Events.
  *									  
  * These events represent the base line protocol that all pipe-ends
  * support. Additional events can be added to build new extensions
  * (sub-classes) to the base line protocol. Care should be taken when
  * building these new protocols or you quickly find yourself with a
  * tower of babble.
  */

 /*	Data Pipe event status values				  */
#define DPIPE_NO_ERROR	        0  	/* Event processed without error  */
#define DPIPE_IGNORED	        1  	/* Unknown Event: Ignored	  */
#define DPIPE_PARAMIGNOORED     2	/* Unknown Paramter: ignored	  */
#define DPIPE_PIPEBROKEN	3  	/* Pipe-end declaired borken	  */
#define DPIPE_NO_QUANTUM	4  	/* Quantum error -> no commom quantum */
#define DPIPE_NO_PROTOCOL	5  	/* Protocol error -> no common protocol */
#define DPIPE_NO_WINDOW	        6  	/* Transfer Window error */
#define DPIPE_NORMALEND	        7  	/* Normal complete found (eof) */


 /*
  * Common Events range from 1-100
  *   
  * Each sub-class should define (reserve) a unique range define here
  *
  * Each event represents a responsibility that a pipe-end service must
  * support. Most of the events come in matched event pairs of
  * request-reply. There are five actors (Source, Sink, Bus Master, Bus
  * Slave, Pipe) within a data pipe. Most events are processed by all cast
  * members, while a few are only processed by a single actor.
  */

#define DPIPE_REPLYOFFSET 50 /* all replies have this offset	  */

/*	          Event		   Actor(s)	Function	  */
#define DPIPE_INITPIPE     1 /* PipeEnds	create a new pipe	  */
#define DPIPE_DONE  	2 /* Bus Master	Quantum transfer Done	  */
#define DPIPE_READY	3 /* Bus Slave	Ready for next Quantum	  */
#define DPIPE_SET		4 /* All 	Set internal variables	  */
#define DPIPE_GET		5 /* All 	Get internal variables	  */
#define DPIPE_PROTOCOL     6 /* PipeEnds	List supported protocols  */
#define DPIPE_XFERWINDOW   7 /* PipeEnds	Bus Slave target addresses*/
#define DPIPE_QUANTUM      8 /* PipeEnds	Data per done/ready	  */
#define DPIPE_TRANSFER     9 /* PipeEnds	Transfer this much data	  */
#define DPIPE_STOP	        10/* PipeEnds	Stop current transfer	  */
#define DPIPE_RESET        11/* PipeEnds	Reset pipe back to init	  */
#define DPIPE_ERROR	12/* All	Pipe Broken		  */
#define DPIPE_NEGOTIATE	13/* All	start negotiation, Qnt,xfer,etc */
#define DPIPE_KILLPIPE     14/* PipeEnds	destroy pipe		  */

#define DPIPE_INITPIPEREPLY 	(DPIPE_REPLYOFFSET + DPIPE_INITPIPE)
#define DPIPE_SETREPLY		(DPIPE_REPLYOFFSET + DPIPE_SET)
#define DPIPE_GETREPLY		(DPIPE_REPLYOFFSET + DPIPE_GET)
#define DPIPE_PROTOCOLREPLY	(DPIPE_REPLYOFFSET + DPIPE_PROTOCOL)
#define DPIPE_XFERWINDOWREPLY 	(DPIPE_REPLYOFFSET + DPIPE_XFERWINDOW)
#define DPIPE_QUANTUMREPLY	        (DPIPE_REPLYOFFSET + DPIPE_QUANTUM)
#define DPIPE_TRANSFERREPLY	(DPIPE_REPLYOFFSET + DPIPE_TRANSFER)

 /*
  * All Data Pipe Events have a standard event header that is used to
  * simplify most event processing. The tag sent in a request is always
  * returned in the reply event and can be used as an ID for the request.
  */

typedef struct dpipeEventHeader {
    unsigned short	event;	/* defines which data pipe event */
    unsigned short	status;	/* valid only in reply events	 */
    __uint32_t		pipeID; /* tag to define which pipe      */
    __uint32_t          target_q;
    __uint64_t		tag;	/* requester cookie to track events */
    __uint64_t		replyQ;	/* where to send replies */
} dpipeEventHeader_t;

/*
 * Each bus master or slave has a number of common attributes that are used
 * by the pipe driver to decide whether a normal or buffer pipe needs to be
 * created.
 */

#define DPIPE_VARIABLEXFER	0x8000	/* supports variable transfers (byte count) */
#define DPIPE_BLOCKXFER	0x4000	/* supports only block size transfers */
#define DPIPE_FIXEDXFER	0x2000	/* supports only fixed (quantum) size transfers
*/

#define DPIPE_RANDOM 	0x0800	/* supports random addressing */
#define DPIPE_SEQUENTIAL	0x0400  /* only supports sequential addressing */
#define DPIPE_SEQBLOCK	0x0200	/* supports random block, sequential with block
*/
#define DPIPE_FIXEDADR	0x0100	/* one address only => register */

#define DPIPE_READ		0x0008	/* supports read accesses */
#define DPIPE_WRITE	0x0004	/* supports write accesses */
#define DPIPE_READONLY	0x0002	/* supports read only accesses */
#define DPIPE_WRITEONLY	0x0001	/* supports write only accesses */

 typedef struct dpipeBus {	 /* bus attributes of masters and slaves */
   __uint64_t	attributes;      /* bit attributes */
   __uint32_t	minimumTransfer; /* smallest transfer */
   __uint32_t	blockSize;	 /* block size for transfers */
   __uint32_t	alignment;	 /* alignment boundary */
 } dpipeBus_t;

 typedef struct dpipeEventQ {
   __uint32_t	QID;	        /* the pipe-end event queue ID */
   __uint64_t	Q;		/* the pipe-end event queue */
 } dpipeEventQ_t;

 typedef struct dpipePipeEnd {	/* attributes of a pipe-end */
   dpipeEventQ_t	eventQ;		/* normal event queue */
   dpipeEventQ_t  event2Q;	/* out of bands event queue */
   dpipeBus_t 	master;		/* pipe-end bus master attributes */
   dpipeBus_t 	slave;		/* pipe-end bus slave attributes */
   __uint64_t 	maxRate;	/* maximum data rate (bytes/sec) supported */
   __uint64_t 	minRate;	/* minimun data rate (bytes/sec) required */
   char		asynchronous;	/* pipe-end not tied to the time domain */
 } dpipePipeEnd_t;

typedef struct dpipeSimpleEvent {	/* just the header */
    dpipeEventHeader_t	head;
} dpipeSimpleEvent_t;

 /*
  * 	initPipe Event :
  *	First event sent to pipe-end to create a new data pipe.
  *	All required state is returned to the data pipe driver.
  *
  * non-zero status: pipe-create fails
  */

 typedef struct dpipeInitPipeEvent {
   dpipeEventHeader_t	head;		/* note pipeID in header */
   __uint32_t		source;         /* true -> this pipe-end is source */
   __uint32_t		pipeEventQID;	/* ID for the Pipe Driver event Q*/
 } dpipeInitPipeEvent_t;

 typedef struct dpipeInitPipeReplyEvent {
   dpipeEventHeader_t	head;		/* status of init request */
   dpipePipeEnd_t		pipeEnd;	/* starting state of a pipe-end */
 } dpipeInitPipeReplyEvent_t;

 /*
  * Set Event : The set event is a general purpose parameter setting function
  * within the data pipe drivers. Each event supports the assignment of a
  * number of paramters at a time.
  *
  * non-zero status: DP_IGNORED  	Unknown Event -> pipe broken
  *                : DP_PARAMIGNOORED 	Unknown Parameter tag -> ignored
  *			unknown tag will be returned as ~tag
  */

/*
 *  Common Paramter tags - also readable by people
 *
 */

#define MUNGE_TAG(a,b,c,d) (a << 24 | b << 16 | c << 8 | d) 

#define DPIPE_OTHER_Q_PARM_TAG      (MUNGE_TAG('O','t','h','Q'))
#define DPIPE_OTHER_2Q_PARM_TAG     (MUNGE_TAG('O','t','2','Q'))
#define DPIPE_SOURCE_PARM_TAG       (MUNGE_TAG('S','O','U','R'))
#define DPIPE_SINK_PARM_TAG         (MUNGE_TAG('S','I','N','K'))
#define DPIPE_MASTER_PARM_TAG       (MUNGE_TAG('M','A','S','T'))
#define DPIPE_SLAVE_PARM_TAG        (MUNGE_TAG('S','L','V','E'))
#define DPIPE_PROTOCOL_PARM_TAG     (MUNGE_TAG('P','R','O','T'))
#define DPIPE_QUANTUM_PARM_TAG      (MUNGE_TAG('Q','N','T','M'))
#define DPIPE_PIPEID_PARM_TAG       (MUNGE_TAG('P','I','P','E'))

 /*
  *  Set limit on number of paramters read/written per event.
  *
  *  Set Event Reply, Get Event, Get Event Reply : all have the same
  *  event structure.
  */

#define DPIPE_GETSET_COUNT 10

 typedef struct dpipeParam {
   __uint32_t		paramtag; /* param tag can be ASCII, debug aid */
   __int64_t		value;	  /* get or set value */
 } dpipeParam_t;

 typedef struct dpipeSetEvent {
   dpipeEventHeader_t	head;
   int                  paramCount;
   dpipeParam_t		params[DPIPE_GETSET_COUNT];
 } dpipeSetEvent_t;

 typedef struct dpipeSetReplyEvent {
   dpipeEventHeader_t	head;
   int                  paramCount;
   dpipeParam_t		params[DPIPE_GETSET_COUNT];
 } dpipeSetReplyEvent_t;

 typedef struct dpipeGetEvent {
   dpipeEventHeader_t	head;
   int                  paramCount;
   dpipeParam_t		params[DPIPE_GETSET_COUNT];
 } dpipeGetEvent_t;

 typedef struct dpipeGetReplyEvent {
   dpipeEventHeader_t	head;
   int                  paramCount;
   dpipeParam_t		params[DPIPE_GETSET_COUNT];
 } dpipeGetReplyEvent_t;


 /*
  * The Negotiate tells the pipe end to start the negotiation process for
  * Protocol, Quantum, and Transfer Window.
  */

typedef struct dpipeNegotiateEvent {
     dpipeEventHeader_t    head;
} dpipeNegotiateEvent_t;

 /*
  * Protocol Event : Allows the expansions of the data pipe model by
  * support new protocols beyond the base protocol.
  */

#define DPIPE_PROTOCOL_COUNT 40
#define DPIPE_SIMPLE_PROTOCOL 1

typedef struct dpipeProtocolEvent {
    dpipeEventHeader_t	head;
    unsigned short	count;		/* number of protocols supported */
    unsigned short	preferred;	/* preferred protocol (bid) */
    unsigned short	protocols[DPIPE_PROTOCOL_COUNT];
} dpipeProtocolEvent_t;

typedef dpipeProtocolEvent_t dpipeProtocolEventReply_t;

 /*
  * The Quantum event is used to negotiate the size and attributes of the
  * transfer between the ready and done events. It is the responsibility 
  * of the bus master to start the negotiation with the bus slave.
  *
  * The bus master fills in the bus master field while the bus slave responds
  * with its dp_bus_t and quantum bid. The event may be sent more than once
  * between the master and slave until either a quantum is agreed on or an
  * error is returned.
  *
  *  non-zero status: forward failed quantum event to pipe queue. Data can't
  *  be transferred without a valid quantum.
  */

 typedef struct dpipeQuantumEvent {
   dpipeEventHeader_t	head;
   dpipeBus_t 		master;		/* pipe-end bus master attributes */
   dpipeBus_t 		slave;		/* pipe-end bus slave attributes */

   	/* next part of event is the "bid" part of the negotiation */
   __uint32_t		preferred;	/* preferred quantum (bid) */
   __uint32_t		minimum;	/* minimum transferred supported */
   __uint32_t		maximum;	/* maximume transferred supported */
 } dpipeQuantumEvent_t;

 typedef struct  dpipeQuantumEvent_t dpipeQuantumEventReply_t;

 /*
  * The Transfer Window event is used to define what bus slave addresses
  * are used to transfer data. The transfer window event is created by the
  * bus slave and sent to the bus master. The event may be sent as often as the
  * bus slave requires a new transfer window.
  *
  * The simplest way to set this up is to define one or more extents for each
  * quantum. Then start a new extent for the next quantum.
  */

#define DPIPE_XFERNOCNT	0x8000	/* use window in loop, no counts used */
#define DPIPE_XFERLOOPCNT	0x4000	/* use window in loop, byte count */
#define DPIPE_XFERQUTMCNT	0x2000	/* use window in loop, quantum count */

#define DPIPE_XFERPACK	0x0080	/* no alignment, pack transfers (fifo) */
#define DPIPE_XFERNEXTEXT  0x0040  /* quantum always starts on next extent
				  boundary */
#define DPIPE_XFERNEXTQTM  0x0020	/* variable sized xfers start on quantum
				   boundaries */

#define DPIPE_XFEREXTCNT	16	/* support up to 16 phyiscal memory extents */

typedef struct dpipeXferExtent {
    __uint64_t		phyAdr; 	/* extent target address */
    __uint32_t		byteCount;	/* size of extent */
} dpipeXferExtent_t;

 typedef struct dpipeXferWindow {
    __uint32_t		windowFlags;	/* transfer flags */
   __uint64_t		byteCount;	/* byte count for window */
   __uint32_t		quantumCount;	/* quantum count */
   dpipeXferExtent_t	extents[DPIPE_XFEREXTCNT];
 } dpipeXferWindow_t;

 typedef struct dpipeXferWindowEvent {	/* NO REPLY */
   dpipeEventHeader_t	head;
   dpipeXferWindow_t      window;
 } dpipeXferWindowEvent_t;

 /*
  * The Ready event is sent by the bus slave to declare it ready for 
  * the next quantum transfer. The Ready event has no reply.
  */
typedef struct dpipeReadyEvent {
    dpipeEventHeader_t	head;
    __uint32_t          byteCount;   /* this many bytes ready */
    __uint32_t          lastQuantum; /* last quantum for transfer event */
} dpipeReadyEvent_t;


 /*
  * The Done event is sent by the bus master to declare the transfer
  * is complete. The Done event has no reply.
  */

 typedef struct dpipeDoneEvent {	       /* NO REPLY */
   dpipeEventHeader_t	head;
   __uint32_t		byteCount;     /* actual bytes transfered */
   __uint32_t           lastQuantum;   /* last quantum for transfer event */
 } dpipeDoneEvent_t;


/*
 * The Transfer Event is the main event used by the application to cause a
 * transfer of bytes or quantums.
 *
 * non-zero status: normal end (eof) or error state. The error state only
 * happens when one or more drivers find a problem. The pipe is frozen (broken)
 * until a Reset Event is issued. The pipe state freezes so the state may be
 * debugged.
 */

#define DPIPE_XFERBLOCKCOUNT 16	/* total size to scatter/gather list */

 typedef struct dpipeXferBlock {
   __uint64_t		offset;		/* move this far into data */
   __uint32_t		byteCount;	/* transfer this many bytes */
 } dpipeXferBlock_t;

typedef struct dpipeTransferEvent {
    dpipeEventHeader_t	head;
    __uint64_t		byteCount;	/* transfer this many bytes */
    __uint32_t          sink_offset;    /* start transfer at this offset on 
					   sink */
    __uint32_t          source_offset;    /* start transfer at this offset on 
					     source */
    __uint64_t		quantumCount;	/* transfer this many quantums */
    dpipeXferBlock_t	list[DPIPE_XFERBLOCKCOUNT];
} dpipeTransferEvent_t;

 typedef struct dpipeTransferEvent_t dpipeTransferReplyEvent_t;

 /*
  * The Stop Event halts the current transfer.
  */
typedef struct dpipeStopEvent {	/* just the header */
    dpipeEventHeader_t	head;
} dpipeStopEvent_t;

 typedef struct dpipeStopEvent_t   dpipeStopReplyEvent_t;

 /*
  * The Reset Event aborts the current transfer and resets quantum,
  * protocol, and transfer window.
  */
typedef struct dpipeResetEvent {
    dpipeEventHeader_t	head;
} dpipeResetEvent_t;

 typedef struct dpipeResetEvent_t dpipeResetReplyEvent_t;

 /*
  * The Error Event is used by the pipe-ends to report a broken pipe.
  */
typedef struct dpipeErrorEvent {
    dpipeEventHeader_t	head;
} dpipeErrorEvent_t;


 /*
  * The Kill Pipe event is used to destroy the pipe.
  */
typedef struct dpipeKillEvent {
    dpipeEventHeader_t	head;
} dpipeKillEvent_t;


#endif /* _DATAPIPE_EVENTS_H */
