/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * wd95a_struct.h -- Western Digital 95a driver data structures.
 */

#ident "$Revision: 1.4 $"

#ifndef _SYS_WD95A_STRUCT
#define _SYS_WD95A_STRUCT

#if defined(EVEREST) && !defined(SCSI_3)
#define SCSI_3
#endif

/* definitions for numbers of things */
#if !defined(EVEREST)
#define WD95_CTLRS	8		/* max # of controllers */
#endif
#define	WD95_QDEPTH	1		/* number of outstanding que'd */

#ifdef SCSI_3
#define WD95_LUPC	16		/* logical units per controller */
#else
#define WD95_LUPC	8		/* logical units per controller */
#endif

/* defines for scsirequest.sr_ha_flags */
#define WFLAG_ODDBYTEFIX	1

/*
 * WD95a driver equivalent of scsirequest -- once a slot is
 * free, a wd95request gets matched with a scsirequest.
 */
struct wd95request {
	u_char			active : 1,
				starting : 1;
	u_char			ctlr;
	u_char			unit;
	u_char			lun;
	u_char			cmd_tag;	/* command tag */
	u_char			reset_num;	/* reset number when timeout started */
	int			timeout_id;
	int			resid;
	int			tlen;
	struct scsi_request	*sreq;
	struct wd95request	*next;
};
typedef struct wd95request wd95request_t;

/* per logical unit (lun) information structure */
struct wd95luninfo {
	u_char	lun_num;	/* lun number for this device */
	u_char	sense:1,	/* request sense in progress */
		excl_act:1,	/* exclusive access active */
		cmdt_en:1;	/* cmd tag queueing available */
	uint	refcnt;
	scsi_request_t	*u_sense_req;	/* scsi request for sense info */
	wd95request_t *wqueued; /* queue of active for this device */
	struct scsi_target_info	tinfo;
};
typedef struct wd95luninfo wd95luninfo_t;

/* per unit (device) information structure */
struct wd95unitinfo {
	u_char	number;		/* unit number of this unit (0-13) */
	u_char	wreq_cnt;	/* count of total wreqs for this target */
	ushort	sense:1,	/* request sense in progress */
		active:1,	/* unit is currently active */
		u_ca:1,		/* contingent allegiance occurred */
		abortqueue:1,	/* work queues aborted */
		aenreq:1,	/* need AEN acknowledge */
		tostart:1,	/* start timeout running */
		disc:1,		/* disconnect enabled */
		sync_en:1,	/* sync is OR should be enabled */
		sync_req:1,	/* sync negotiation needed */
		wide_en:1,	/* wide is OR should be enabled */
		wide_req:1,	/* wide negotiation needed */
		ext_msg:1,	/* extended message in progress */
		cmdt_en:1;	/* enable cmd tag queueing */
	u_char	sync_off;	/* negotiated sync offset */
	u_char	sync_per;	/* required sync period */
	u_char	sel_tar;	/* location for selection */
	u_char	lun_round;	/* round robin queue lun checking */
	u_char	cur_lun;	/* current lun under service */

	scsi_request_t *reqhead; /* queue commands here normally */
	scsi_request_t *reqtail;

	scsi_request_t *auxhead; /* queue commands here when u_ca set */
	scsi_request_t *auxtail;

	wd95request_t *wrfree; /* pointer to free wd95request structs */
	wd95request_t wreq[WD95_QDEPTH];

	wd95luninfo_t	*lun_info[SCSI_MAXLUN];
	wd95luninfo_t	lun_0;

	void	(*sense_callback)(char *);

	struct wd95ctlrinfo *ctrl_info;

	sema_t	opensem;

	sema_t	start;		/* queue for no-notify semaphore */
	sema_t	done;		/* semaphore for no-notify commands */

#if WD95_CMDHIST
	char prevcmd[16][12];
	int prevcmd_rotor;
#endif
};
typedef struct wd95unitinfo wd95unitinfo_t;

struct wd95ctlrinfo {
	lock_t	lock;
	lock_t	qlock;		/* Lock for structure queues */
	lock_t	auxlock;

	u_char	number;		/* number of this controller */
	u_char	ci_idmsgin;	/* resel & looks like id msg in */
	u_char	host_id;	/* host id of this controller */
	u_char	round_robin;	/* last unit number that was checked */
	u_char	max_round;	/* unit to wrap at */
	u_char	cur_target;	/* current target id */
	u_char	reset_num;	/* current reset number */

	ushort	present:1,	/* flag indicating if ctlr is installed */
		reset:1,	/* set if resetting scsi busses */
		pagebusy:1,	/* odd-byte fixup buffer use status */
		active:1,	/* controller is currently active */
		intr:1,		/* currently processing intr on this ctlr */
		narrow:1,	/* this is a narrow bus (limited by hw) */
		diff:1;		/* clear if single ended, set if differential */
	u_char	sync_period;	/* synchronous period for this controller */
	u_char	sync_offset;	/* and offset for this controller */

	u_char	*page;		/* odd-byte fixup buffer */
	ushort	*wdi_log;	/* log all accesses to wd95 */

	sema_t	d_sleepsem;	/* Synchronous request waiting sem */
	sema_t	d_onlysem;	/* 'only' request waiting sem */
	short	d_sleepcount;	/* Synchronous request waiter count */
	short	d_onlycount;	/* Synchronous request waiter count */

	scsi_request_t *acthead; /* queue commands here while active */
	scsi_request_t *acttail;

	caddr_t	ha_addr;	/* address of the associated wd95a chip */
	
	dmamap_t *d_map;
	struct wd95unitinfo *unit[WD95_LUPC]; /* unit info */
};
typedef struct wd95ctlrinfo wd95ctlrinfo_t;

/* flags for each bus */
#define	WD95BUS_SYNC	0x01		/* Synchronous enable */
#define	WD95BUS_DISC	0x02		/* Disconnect/reconnect enable */
#define	WD95BUS_WIDE	0x04		/* Wide enable */
#define	WD95BUS_LUN_CK	0x08		/* check all the luns on the bus */
#define	WD95BUS_CMDT	0x10		/* cmd tag queueing enable */

extern u_char wd95_bus_flags[];

extern u_char wd95_syncperiod[];

#endif /* _SYS_WD95A_ */
