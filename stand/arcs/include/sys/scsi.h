#ifndef __SYS_SCSI_H__
#define __SYS_SCSI_H__

/**************************************************************************
 *									  *
 * 		 Copyright (C) 1990, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 3.40 $"

#include "sys/sema.h" /* mainly so app programs that use this file
	will still compile */

/*
 * Common SCSI definitions needed by formatter drivers
 * and the SCSI interface driver.
 */

#if EVEREST || SN0
#define SC_MAXADAP      150 /* Max number of adapters allowed by the system */
#define SC_MAXTARG      16  /* Max number of targets allowed by the system */
#define SC_MAXLUN       8   /* Max number of lu's allowed by the system */

#elif IP30
#define SC_MAXADAP      16
#define SC_MAXTARG      16
#define SCSI_MAXTARG    16
#define SC_MAXLUN       8

#elif defined(IP32) || defined (MHSIM)
#define SC_MAXADAP	10  /* Moosehead/Roadrunner systems */
#define SC_MAXTARG	16
#define SC_MAXLUN	8

#else
#define SC_MAXADAP      8
#define SC_MAXTARG      8
#define SCSI_MAXTARG    16
#define SC_MAXLUN       8
#endif

#define SCTMSK 7	/* mask to get target id */
#define SCLMSK 7	/* mask to get lun  */

/* Possible status bytes returned after a scsi command */
#define ST_GOOD		0
#define ST_CHECK	2
#define ST_COND_MET	4
#define ST_BUSY		8
#define ST_INT_GOOD	16
#define ST_INT_COND_MET	20
#define ST_RES_CONF	24

/* Possible error codes returned by the scsi driver */
#define SC_GOOD		0	/* No error */
#define SC_TIMEOUT	1	/* Timeout on scsi bus */
#define SC_HARDERR	2	/* Hardware or scsi device error */
#define SC_PARITY	3	/* Parity error on the SCSI bus during xfer */
#define	SC_MEMERR	4	/* Parity/ECC error on host memory */
#define SC_CMDTIME	5	/* the command timed out */
#define SC_ALIGN	6	/* i/o address wasn't aligned on 4 byte boundary */
#define SC_BUS_RESET	7	/* scsi bus reset occured.  The command was */
				/* not issued				    */

/* Size of different scsi command classes */
#define SC_CLASS0_SZ	6
#define SC_CLASS1_SZ	10
#define SC_CLASS2_SZ	12

typedef struct scsisubchan	scsisubchan_t;
/*
 * Subchannel control record.
 *
 * This structure encapsulates all the information for a request.
 */
struct scsisubchan {
	/* PUBLIC INPUT DATA */
	u_char		*s_cmd;		/* SCSI command */
	int		s_len;		/* Length of command */
	struct buf	*s_buf;		/* transfer data, if any */
	void		(*s_notify)(scsisubchan_t *);	/* Completion routine for async IO */
	u_int		s_timeoutval;	/* Timeout value for transaction */
	caddr_t s_caller;	/* for callers use; typically identifies
		a structure of interest to the caller */

	/* PUBLIC OUTPUT DATA */
	int		s_error;	/* SCSI operation error code (below) */
	u_char		s_status;	/* SCSI device status message (below) */

	/* PRIVATE DATA */
	u_char		s_adap;		/* Host adapter id */
	u_char		s_target;	/* Target id */
	u_char		s_lu;		/* Logical unit id */
	u_char s_syncreg;	/* sync reg value if target supports sync */
	u_char		s_flags;	/* Flags, see below */
	u_char		s_retry;	/* Number of error retrys done */
	u_char		s_syncs;	/* this many cmds must succeed before
		we can re-enable sync.  A hack because it seems to confuse
		the drive when we re-enable sync with a check condition
		outstanding. */
	u_char	s_dstid;	/* target ORed with direction.  Used
		with 93A, to set direction flag in chip */
	int		s_timeid;	/* Timeout key */
	sema_t		s_sem;		/* Semaphore for synchronous IO */
	caddr_t s_xferaddr;	/* Current xfer addr (reselection) */
	unsigned	s_xferlen;	/* Current xfer len (reselection) */
	unsigned	s_tlen;		/* Current working xfer len */
	unsigned	s_tentative;	/* amount transferred during previous
					   connection -- nonzero when we get a
					   disconnect without savedatap */
	scsisubchan_t	*s_link;	/* Link for wait or disconnected list */
#ifdef DMA_SCSI	/* things like the kernel that actually need it */
	dmamap_t *s_map;	/* per channel dma map */
#else	/* keep struct size the same */
	void *s_map;
#endif
};

#define	S_BUSY		0x01		/* SCSI operation in progress */
#define	S_DISCONNECT	0x02		/* Subchannel is disconnected */
#define	S_SAVEDP	0x04		/* Subchannel's data pointers saved */
#define	S_SYNC_ON	0x08		/* Channel using synchronous i/o */
#define	S_RESYNC	0x10		/* We have (temporarily) negotiated for
	async mode on a device because a transfer is too large to be mapped.
	Only needed with the 93, not the 93A */
#define	S_CANTSYNC	0x20		/* drive can't sync, don't try on
	it any more (rejected sync message) */
#define	S_RESTART	0x40		/* cmd is being restarted due to
	a 'simultaneous' re-select */
#define	S_TARGSYNC	0x80		/* target initiated sync mode */

#define SCDMA_IN	0x40	/* bit expected by 93A if data direction
	is to host.  OR'ed into s_dstid */

#define	SC_NUMSENSE	0x10	/* # of messages in scsi_key_msgtab */
#if 0
#define	SC_NUMADDSENSE	0x65	/* # of messages in scsi_addit_msgtab */
#endif
#define	SC_NUMADDSENSE	0x4a	/* A number of drivers use this, but many
				 * only define 0x4A messages.  This is defined
				 * in multiple header filesi -- want to make
				 * it the same in all. */

/* base sense error codes */
#define SC_NOSENSE	0x0
#define SC_ERR_RECOVERED	0x1
#define SC_NOT_READY	0x2
#define SC_MEDIA_ERR	0x3
#define SC_HARDW_ERR	0x4
#define SC_ILLEGALREQ	0x5
#define SC_UNIT_ATTN	0x6
#define SC_DATA_PROT	0x7
#define SC_BLANKCHK	0x8
#define SC_VENDUNIQ	0x9
#define SC_COPY_ABORT	0xA
#define SC_CMD_ABORT	0xB
#define SC_EQUAL	0xC
#define SC_VOL_OVERFL	0xD
#define SC_MISCMP	0xE

/* some  common extended sense error codes */
#define SC_NO_ADD	0x0	/* no extended sense code */
#define SC_NOINDEX	0x1
#define SC_NOSEEKCOMP	0x2
#define SC_WRITEFAULT	0x3
#define SC_NOTREADY	0x4
#define SC_NOTSEL	0x5
#define SC_ECC		0x10
#define SC_READERR	0x11
#define SC_NOADDRID	0x12
#define SC_NOADDRDATA	0x13
#define SC_DEFECT_ERR	0x19
#define SC_WRITEPROT	0x27
#define SC_RESET	0x29

/* these are the only routines in the low level scsi driver that
	can be called by the upper level scsi drivers. */
scsisubchan_t *allocsubchannel(int, int, int);
void freesubchannel(scsisubchan_t *);
void doscsi(scsisubchan_t *);
void scsi_setsyncmode(scsisubchan_t *, int);
u_char *scsi_inq(u_char, u_char, u_char);
int scsidump(int);

extern char *scsi_key_msgtab[], *scsi_addit_msgtab[];	/* in scsi.c */

extern int scsicnt;	/* # of SCSI host adapter configured */

void scsi_reset(int adap);

#ifdef IP22_WD95
scsisubchan_t *wd95_allocsubchannel(int, int, int);
void wd95_freesubchannel(scsisubchan_t *);
void wd95_doscsi(scsisubchan_t *);
void wd95_scsi_setsyncmode(scsisubchan_t *, int);
u_char *wd95_scsi_inq(u_char, u_char, u_char);
#endif

#endif /* !__SYS_SCSI_H__ */
