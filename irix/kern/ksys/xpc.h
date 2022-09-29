/*
 * File: 	xpc.h
 * Purpose:	Defines the xpc (Cross Partition Communication) interfaces 
 *		exported to the kernel.
 *
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#ifndef _KSYS_XPC_H
#define _KSYS_XPC_H 1

#include <sys/alenlist.h>
#include <sys/uio.h>
#include <sys/poll.h>

/*
 * The total number of rings is limited to 32. For performance reasons 
 * the internal cross partition structures require 4 bytes per channel, 
 * and 32 allows all of this interface shared info to fit on one cache line.
 *
 * Current channel assignment is as follows (PP is 2 hex digit partition id).
 *
 *  +---------+--------------------+----------------------------------------+
 *  | Channel |     hwgraph	   |	Purpose				    |
 *  +---------+--------------------+----------------------------------------+
 *  |   0     | kernel/control     | Control ring used by xpc only          |
 *  +---------+--------------------+----------------------------------------+
 *  |   1     | kernel/rpc/PP/01   | Cellular Irix			    |
 *  +---------+--------------------+----------------------------------------+
 *  |   2     | kernel/rpc/PP/02   | Cellular Irix			    |
 *  +---------+--------------------+----------------------------------------+
 *  |  3-10   |			   | Unused				    |
 *  +---------+--------------------+----------------------------------------+
 *  |  11     |	net/PP/00	   | Network interface (0)		    |
 *  +---------+--------------------+----------------------------------------+
 *  |  12     | net/PP/01	   | Netowrk interface (1)		    |
 *  +---------+--------------------+----------------------------------------+
 *  |  13     | admin/PP/partition | Partition management interface	    |
 *  +---------+--------------------+----------------------------------------+
 *  |  14     | admin/PP/clshm     | Shared memory daemon interface	    |
 *  +---------+--------------------+----------------------------------------+
 *  |15 - 30  | raw/PP/00-0f       | Raw device interface		    |
 *  +---------+--------------------+----------------------------------------+
 */
#define	XP_CTL_RINGS	1		/* # control rings */
#define	XP_RPC_RINGS	2		/* RPC/cellular irix rings */
#define	XP_NET_RINGS	2		/* # network interface rings */
#define	XP_RAW_RINGS	18		/* # raw device rings */
#define	XP_RINGS	31		/* Total rings */

#define	XP_CTL_BASE	0
#define	XP_RPC_BASE	1
#define	XP_NET_BASE	11
#define	XP_RAW_BASE	13

/*
 * Defines: XP_ALIGN_XXX
 * Purpose: Defines the alignment requirments for DMA operations. In general,
 *	    most things get aligned based on these defines.
 */
#if SN0
#define	XP_ALIGN_SIZE		(BTE_LEN_MINSIZE)
#define	XP_ALIGN_MASK		(~(BTE_LEN_ALIGN))
#define	XP_UNITS(_x)		((__psunsigned_t)(_x) >> BTE_LEN_SHIFT)
#else
/* If not SN0, then most likely an EVEREST MULTIKERNEL CELL prototype kernel,
 * so dummy up some defines.
 */
#define	XP_ALIGN_SIZE		(0x80)
#define	XP_ALIGN_MASK		(~(0x7f))
#define	XP_UNITS(_x)		((__psunsigned_t)(_x) >> 7)
#endif
#define	XP_ALIGN(_x)		((__psunsigned_t)(_x) & XP_ALIGN_MASK)
#define	XP_OFFSET(_x)		((__psunsigned_t)(_x) & (XP_ALIGN_SIZE - 1))
#define	XP_ALIGNED(_x)		(!XP_OFFSET(_x))


/*
 * Typedef: 	xpc_t
 * Purpose:	Defines the return values and values passed to 
 *		asynchronous functions. 
 */
typedef enum {
    xpcSuccess = 0,
    xpcDone,				/* 1: Operaion complete */
    xpcTimeout,				/* 2: timeout on connection */
    xpcError,				/* 3: link error on connection*/
    xpcPartid,				/* 4: invalid partition ID */
    xpcChannel,				/* 5: invalid channel ID */
    xpcAsync,				/* 6: asynchronous message */
    xpcAsyncUnknown,			/* 7: unknown/invalid message */
    xpcIntr,				/* 8: interrupted wait */
    xpcDisconnected,			/* 9: channel closed */
    xpcConnected,			/* 10: channel connected */
    xpcNoWait,				/* 11: operation would require wait */
    xpcBusy,				/* 12: ring is busy */
    xpcRetry,				/* 13: Retry operation */
    xpcVersion,				/* 14: incomatible remote version */
    xpcNoOvrd,				/* 15: no override on H/W revision */
    xpcConnectInProg			/* 16: Connection in progress */
}  xpc_t;

/*
 * Variable:	xpc_decode
 * Purpose:	Maps character names to the xpc_t return values.
 */
extern const char *xpc_decode[];

/*
 * Variable:	xpc_errno
 * Purpose:	Maps a rough xpc to errro, index with xpc_t to translate
 *		to errno.
 */
extern const int  xpc_errno[];

/*
 * Macros:	XPM_BUF(_XXX)
 * Purpose:	Macros to easily pull data and pointers out of an 
 *		inter-partition message.
 * Parameters:	_m - pointer to message (xpm_t)
 *		_x - buffer index in message
 *		_t - cast type
 *
 *	XPM_BUF - 	return pointer to indexed buffer
 *	XPM_PTR - 	return indexed buffer pointer
 *	XPM_BUF_CNT - 	return number of bytes in index buffer.
 *	XPM_BUF_FLAGS-	return flags for indexed buffer.
 *	XPM_BUF_PTR_TO_OFF- 
 *	XPM_BUF_OFF_TO_PTR-
 *	XPM_IDATA -	Returns pointer to immediate data given an index.
 *	XPM_BUF_IDATA - Returns pointer to immediate data. 
 */
#define	XPM_TCNT(_m)		((_m)->xpm_tcnt)
#define XPM_BUF(_m, _x)    	(&((xpmb_t *)((xpm_t *)(_m) + 1))[_x])
#define	XPM_BUF_PTR(_m, _x)	(XPM_BUF((_m), (_x))->xpmb_ptr)
#define	XPM_BUF_CNT(_m, _x)	(XPM_BUF((_m), (_x))->xpmb_cnt)
#define	XPM_BUF_FLAGS(_m, _x)	(XPM_BUF((_m), (_x))->xpmb_flags)
#define	XPM_BUF_PTR_TO_OFF(_m, _p)	((paddr_t)(_p) - (paddr_t)(_m))
#define	XPM_BUF_OFF_TO_PTR(_m, _o)	(void *)((paddr_t)(_m) + (paddr_t)(_o))
#define	XPM_IDATA(_m, _x, _t)	(_t)(XPM_BUF((_m), (_x)))
#define	XPM_BUF_IDATA(_m, _x, _t) \
    (_t)XPM_BUF_OFF_TO_PTR((_m), XPM_BUF_PTR((_m), (_x)))

/*
 * Macro: 	XPM_IDATA_LIMIT
 * Purpose:	Compute the # of immediate bytes usable in a message
 * Parameters:	_ms - message size in bytes.
 * 		_bc - buffer count used (number of buffer pointers 
 *		      being filled in).
 */
#define	XPM_IDATA_LIMIT(_ms, _bc)	\
    ((_ms) - sizeof(xpm_t) - ((_bc) * sizeof(xpmb_t)))

/*
 * Typedef: xpm_t	(Cross partition message)
 *	    xpmb_t	(Cross partition buffer)
 *
 * Purpose: Defines the format for an inter-partition message.
 *
 * General message format is as follows:
 *
 * 	+------------------------------+
 *      |   Message Header (xpm_t)    |
 *	+------------------------------+
 *      | Buffer Descriptor 0 (xpmb_t) |
 *	+------------------------------+
 *		................
 *      | Optional Buffer descriptors  |
 *		................
 *	+------------------------------+
 *      |       Immediate Data         |
 *	+------------------------------+
 *
 * Message header is a fixed size, and contains a count of the number of
 * buffer descriptors. The fixed message size is specified on the 
 * connect call that establishes a channel between two partitions. 
 * There is no requirement that both sides of a channel use the same 
 * message size, but then again, there is no reason not to.
 *
 * Buffer descriptors contain either:
 *		(1) physical address/byte count pairs if the buffer is 
 *		    marked as XPMB_F_PTR 
 *	    or  (2) byte offset from start of message header of the 
 *		    beginning of the data if the buffer is marked as
 *		    XPMB_F_IMD.
 *
 * The macros XPM_BUF_PTR_TO_OFF and XPM_BUF_OFF_TO_PTR can be used to 
 * convert between addresses and offsets.
 *
 */


typedef struct xpm_s {
    unsigned char	xpm_type;	/* X partition message type */
#   define	XPM_T_INVALID	0	/* Invalid type */
#   define	XPM_T_CONTROL	1	/* Control message */
#   define	XPM_T_DATA	2	/* Generic data message */
    unsigned char	xpm_flags;
#   define	XPM_F_CLEAR	0
#   define	XPM_F_INTERRUPT	0x01	/* Send interrupt when consumed */
#   define	XPM_F_NOTIFY	0x02	/* Completion notification required */
#   define	XPM_F_SYNC	0x04	/* Wait for remote notification */
#   define	XPM_F_DONE	0x80	/* Receive Only: xpc_free called */
#   define	XPM_F_READY	0x80	/* Send Only: xpc_send called */
    unsigned char	xpm_cnt;	/* # of address length pairs */
    unsigned char	xpm_filler;	/* unused */
    __uint32_t		xpm_tcnt;	/* Total # bytes this message */
    void		*xpm_ptr;	/* Private pointer */
} xpm_t;

/*
 * Typedef: xpmb_t
 * Purpose: Maps out buffer pointer structure for a message. 
 * Notes: If neither XPMB_F_IMD or XPMB_F_PTR is set, the xpmb is 
 *	  effectively ignored. 
 */
typedef struct xpmb_s {
    unsigned char 	xpmb_flags;	/* See below */
#   define	XPMB_F_IMD	0x01	/* Immediate data */
#   define	XPMB_F_PTR	0x02	/* Pointer to data */
#   define	XPMB_F_INMASK	0x03
    unsigned char	xpmb_reserved[3];
    __uint32_t		xpmb_cnt;	/* # bytes */
    paddr_t		xpmb_ptr;	/* Phys address or offset. */
} xpmb_t;


/*
 * Typedef: xpchan_t
 * Purpose: Abstraction for Cross partition channel. 
 */
typedef	struct xpr_s	*xpchan_t;

/*
 * Typedef: xpc_async_rtn_t
 * Purpose: Defines the asyncronous notification routine format. The 
 * 	    parameters used by the routine are as follows:
 *
 *		r - reason code (See table Below)
 *		p - partition ID associated with condition
 *		c - channel ID with condition 
 *		m - pointer to message. See Table below.
 * 
 *   Reason/Code	| Cause 			| key
 *   -------------------+-------------------------------+-------------------
 *   xpcTimeout		| XPC system detected a timeut	| NULL
 *			| condition on Control channel.	|
 *			| ALL channels shut down in this|
 *			| case.  xpcDisconnect MUST be 	|
 *			| called before a new connect-	|
 *			| ion can be established.	|
 *   -------------------+-------------------------------+-------------------
 *   xpcError		| Error occured on this channel,| NULL
 *			| and has been isolated to this |
 *			| channel. xpcDisconnect MUST be|
 *			| called before a new connect-	|
 *			| ion can be established.	|
 *   -------------------+-------------------------------+-------------------
 *   xpcDone		| Asyncronous send completed	| Value passed as
 *			|				| key on send.
 *   -------------------+-------------------------------+-------------------
 *   xpcAsync		| An unsolicited message arrived| Valid message, 
 *			|				| (xpm_t *)
 *			|				| xpc_done must be 
 *			|				| called when 
 *			|				| processed. 
 *   -------------------+-------------------------------+-------------------
 *   xpcDisconnected	| Channel has been disconnected	| NULL
 *			| by remote end of link.	|
 *   -------------------+-------------------------------+-------------------
 *   xpcConnected	| Chennel connection has been 	| NULL
 *			| established (ASYNC connect 	|
 *			| complete.			|
 *   -------------------+-------------------------------+-------------------
 */
typedef	void	(*xpc_async_rtn_t)(xpc_t, xpchan_t, partid_t p, 
				   int c, void *key);

/*
 * Function: 	xpc_allocate
 * Purpose:	Allocate a message to send to a given partition 
 *		channel pair.
 * Parameters:	cid-channel ID as returned from xpc_connect
 *		xpm - pointer to message pointer filled in on return.
 * Returns:	xpcSuccess,  with xpm filled in, 
 *		xpc_t, xpm invalid (NULL).
 * Notes:	Must be called with a context as it is possible for 
 *		this routine sleep waiting for a free message entry.
 *		This allocate routine will hold the "send lock" until
 *		ipcSend is called. Others calling xpcAllocate will
 *		queue up behind (THIS IS ONLY FOR THE FIRST IMPLEMENTATION. 
 *		It will be fixed later but it fits best with what I have 
 *		right now).
 *
 *
 */
extern	xpc_t	xpc_allocate(xpchan_t cid, xpm_t **xpm);

/*
 * Function: 	xpc_send
 * Purpose:	Send a cross partition message previously allocated using 
 *		xpc_allocate. The routine is asynchronous, it does NOT
 *		wait for a reply.
 * Parameters:	cid- channel ID as returned from xpcConnect
 *		m  - pointer to message allocated with xpcAllocate.
 * Returns:	xpc 
 * Notes:	This routine does NOT free any memory pointer to by the
 *		buffer pointers. 
 *
 *		If the message flag XPM_F_NOTIFY is set, this routine 
 *		sends the message synchronously with regards to me
 *		message system - this only means the remote end of the
 *		channel has receive the message and all associated data. 
 *		It allows the sender to free up or re-use any buffers 
 *		referenced by the message, but does NOT mean the message
 *		has been processed at the remote end by a receiver. If the
 * 		message is not marked with NOTIFY, non -immediate buffers 
 *		must not be altered until the sender knows (through some 
 *		higher level protocol) that the receiver has received the 
 *		data.
 */
extern	xpc_t	xpc_send(xpchan_t cid, xpm_t *m);

extern	xpc_t	xpc_send_notify(xpchan_t cid, xpm_t *m, 
				xpc_async_rtn_t r, void *k);

/*
 * Function: 	xpc_connect
 * Purpose:	Establish a connection to a remote partition. 
 * Parameters:	p  - partition ID of destination.
 *		c  - channel to connect.
 *		ms - message size in bytes, MUST be power of 2.
 *		mc - # of messages in ring. ms * mc must <= NBPP.
 *		     The message system MAY reserve some of the 
 *		     entires for internal use (1 or 2 messages).
 *		ar  - function to call when asynchronous notification
 *		     of an error is required. See definition of 
 *		     xpcAsyncRtn_t for parameter descriptions.
 *		flags - modes for connect/send/receive.
 *		t   - max number of threads allowed to be processing messages
 *		      at a given time.
 *		cid- pointer to location to store "ring pointer", used 
 *		     on all the other calls. 
 * Returns:	xpc_t
 * Notes:	Must be called with a context, and will sleep waiting
 *		for connection to be established or until a failure 
 *		is detected. The message system implements a very coarse
 *		grain timeout mechanism so this routine may be called before
 *		interpartition heart beats are active.
 */
extern	xpc_t	xpc_connect(partid_t p, int c, __uint32_t ms, __uint32_t mc, 
			    xpc_async_rtn_t ar, __uint32_t flags, __uint32_t t,
			    xpchan_t *cid);

#define	XPC_CONNECT_DEFAULT	0x00	/* nothing special here */
#define	XPC_CONNECT_AOPEN	0x01	/* asynchronous connect */
#define	XPC_CONNECT_NONBLOCK	0x02	/* non-blocking send/receive */
#define	XPC_CONNECT_ASEND	0x04	/* asyncronous send notification */
#define	XPC_CONNECT_BWAIT	0x08	/* breakable waits (signals) */

/*
 * Function:	xpc_disconnect
 * Purpose:	Disconnect/shutdown a channel.
 * Parameters:	partid	- remote partition id.
 * 		cid	- channel id as returtned from xpcConnect.
 * Returns:	Nothing
 */
extern	void	xpc_disconnect(xpchan_t cid);


/*
 * Function: 	xpc_receive_uio
 * Purpose:	Attempt to do a "fast" receive of a small message.
 * Parameters:	xpr - pointer to ring to receive data on
 *		uio - pointer to user uio struct
 * Returns:	xpc_t
 */
extern xpc_t	xpc_receive_uio(xpchan_t cid, uio_t *);

/*
 * Function: 	xpc_receive
 * Purpose:	Get the next incomming message.
 * Parameters:	cid - channel to pull message from
 *		xpm - pointer to fill in with message
 * Returns:	xpc_t
 * Notes: 	xpm only valid on return of xpcSuccess.
 */
extern xpc_t	xpc_receive(xpchan_t cid, xpm_t **xpm);

/*
 * Function: 	xpc_receive_xpmb
 * Purpose:	Pull data associated with an array of xpmb's
 * Parameters:	x - pointer to xpmb's to pull data from
 *		c - pointer to count of number of xpmb's valid, updated
 *		    on return.
 *		xo - pointer to xpmb in progress on return
 *		al - addr-len list to pull data into, cursor points to 
 *		     next data to receive if total length greater than
 *		     that of the message.
 *		ssize - if NULL, ignored, otherwise, total # bytes pulled 
 *		     returned.
 * Returns:	xpc_t
 */
extern xpc_t	xpc_receive_xpmb(xpmb_t *x, __uint32_t *c, xpmb_t **xo,
				 alenlist_t al, ssize_t *size);

/*
 * Function: 	xpc_done
 * Purpose: 	Mark a message as completely received, and free any
 *		xpc internal resources associated with the message. If a
 *		message is marked as "NOTIFY", this call notifies the 
 *		sender of completion.
 * Parameters:	cid- channel ID as retuned from xpc_connect.
 *		m  - pointer to the message that has been processed
 *		     (pointer passed to async handler).
 * Returns:	nothing.
 */
extern	void	xpc_done(xpchan_t cid, xpm_t *m);

/*
 * Function: 	xpc_poll
 * Purpose:	poll/select support.
 * Parameters:	xpr - pointer to ring to poll.
 *		As per poll entry point in device driver.
 * Returns:	xpc_t
 */
extern xpc_t	xpc_poll(xpchan_t cid, short events, int anyyet, 
			 short *reventsp, struct pollhead **phpp,
			 unsigned int *genp);

/*
 * Function: 	xpc_avail
 * Purpose:	Check if xpc is available to the specified partition.
 * Parameters:	p - target partition ID to check.
 * Returns:	xpc_t
 */
extern xpc_t	xpc_avail(partid_t p);

#if defined(DEBUG)
/*
 * Function: 	xpc_check
 * Purpose:	Verify a message is valid in terms of the message size 
 *		associated with the channel. 
 * Parameters:	cid- channel ID as retutned from xpcConnect
 *		m  - pointer to message.
 * Returns:	Nothing, routine will panic if message is invalid.
 *
 * Notes:	This routine is intended as a debugging aid. It 
 *		verifies that immediate data has not overflowed, 
 *		and various other things.
 * 	
 *		Use XPC_CHECK_XPM in-line to avoid #if DEBUG's
 *	
 */
extern	void	xpc_check(xpchan_t cid, xpm_t *m);

#define	XPC_CHECK_XPM(_x, _m)	xpc_check((_x), (_m))

#else

#define	XPC_CHECK_XPM(_x, _m)

#endif

#endif	/* _KSYS_XPC_H */

