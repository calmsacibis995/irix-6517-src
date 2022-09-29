/* 
 *	For those shared between xbmon module and its applications.
 *
 *	$Revision: 1.9 $
 *
 */

#ifndef	_ML_XB_H
#define	_ML_XB_H

#include  <sys/time.h>

#define       XB_DRIVER_VERSION	1	/* version # for drvr API */

#define       XB_MON_MODE_OFF 	0
#define       XB_MON_MODE_SRC	1
#define       XB_MON_MODE_DST	2
#define       XB_MON_MODE_LEVEL	3

#define       XB_MON_LINK_MASK  7       /* max value for link (same as mask) */
#define       XB_MON_MODE_MASK  3       /* max value for mode (same as mask) */
#define       XB_MON_LEVEL_MASK 7       /* max value for IBUF level  "       */

/* queue support */
typedef struct {
		__uint64_t event_id;
		__uint64_t data;
		__uint64_t sec;
		__uint64_t nsec;
} xb_event;
/* queue support: end */

typedef struct {
		__uint32_t link;
		__uint32_t mode;
		__uint32_t level;
} xb_modeselect_t;

#define XB_VCNTR_LINK_ALIVE 0x01

typedef struct {
	__uint64_t	sec;
	__uint64_t	nsec;
} xb_time_t;

typedef struct {
	__uint64_t	flags;
	__uint64_t	vsrc;  /* virtual counter for src */
	xb_time_t	tsrc;  /* aggregate time for src */
	__uint64_t	vdst;  /* virtual counter for dst */
	xb_time_t	tdst;  /* aggregate time for dst */
	__uint64_t	crcv;  /* count for llp receive retries */
	__uint64_t	cxmt;  /* count for llp transmit retries */
} xb_vcounter_t;

typedef struct {
	xb_vcounter_t	vcounters[XB_MON_LINK_MASK+1];
} xb_vcntr_block_t;

typedef struct {
	int	unit;
} xb_ioctl_t;

#define	IOCTL_REQ_SIZE		(sizeof(xb_ioctl_t) << 16)
#define	XB_SET_TIMEOUT_VALUE	( (1 | ('P' << 8) | IOCTL_REQ_SIZE ) )
#define	XB_GET_TIMEOUT_VALUE	( (2 | ('P' << 8) | IOCTL_REQ_SIZE ) )
#define	XB_START_TIMER		( (3 | ('P' << 8) | IOCTL_REQ_SIZE ) )
#define	XB_STOP_TIMER		( (4 | ('P' << 8) | IOCTL_REQ_SIZE ) )
#define	XB_GET_OVERFLOW_COUNT	( (5 | ('P' << 8) | IOCTL_REQ_SIZE ) )
#define	XB_SET_BUFFER_SIZE	( (6 | ('P' << 8) | IOCTL_REQ_SIZE ) )
#define	XB_GET_BUFFER_SIZE	( (7 | ('P' << 8) | IOCTL_REQ_SIZE ) )
#define	XB_SET_BITSWITCH	( (8 | ('P' << 8) | IOCTL_REQ_SIZE ) )
#define	XB_GET_BITSWITCH	( (9 | ('P' << 8) | IOCTL_REQ_SIZE ) )
#define	XB_SELMODE_CNTR_A	( (10 | ('P' << 8) | IOCTL_REQ_SIZE ) )
#define	XB_SELMODE_CNTR_B	( (11 | ('P' << 8) | IOCTL_REQ_SIZE ) )
#define	XB_ENABLE_MPX_MODE	( (12 | ('P' << 8) | IOCTL_REQ_SIZE ) )
#define	XB_GET_VERSION    	( (13 | ('P' << 8) | IOCTL_REQ_SIZE ) )

/* ioctl to take in port# of xbow and return the slot# of that port */
#define	XB_GET_SLOTNUM    	( (14 | ('P' << 8) | IOCTL_REQ_SIZE ) )

#ifdef _KERNEL
extern int xbtrace(caddr_t context, __uint64_t event, __uint64_t data);

extern __uint64_t xb_eventmask;

#define XBTRACE(context, tp_id, event, data) 					\
    if (xb_eventmask & ( (__uint64_t) 1 << (tp_id)) ) {			\
	xbtrace(context,  (((__uint64_t) (tp_id)) << 56) | 			\
		((__uint64_t) (event) &  0x00FFFFFFFFFFFFFFLL),		\
		(__uint64_t)(data));					\
	}
#endif /* _KERNEL */

/* Unique IDs to differentiate between various drivers: */

#define ETHER_DRVR_ID	0x1 /* id for all ethernet driver tracepoints */ 
#define OTHER_DRVR_ID	0x2 /* Reserved */
#define SCSI_DRVR_ID	0x3 /* id for all scsi driver tracepoints */
#define GFX_DRVR_ID	0x4 /* id for all graphics driver tracepoints */
#define VIDEO_DRVR_ID	0x5 /* id for all video driver tracepoints */
#define XBOW_DRVR_ID	0x80 /* base id of all xbows */

#endif	/* _ML_XB_H */
