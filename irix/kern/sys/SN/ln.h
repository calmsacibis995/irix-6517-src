/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992-1997, Silicon Graphics, Inc.          *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
/*
 * File: lndev.h
 * Purpose: sn0net device

#ifndef __SYS_SN_LN_H__
#define __SYS_SN_LN_H__

/*
 * SN0 Interpartition Structures
 *
 * Each partition contains a Cross partition Map (xpMap_t). The address of 
 * this map is places in the KLDIR entires for every node in the partition.
 *
 * 
 *	+----------------------------+
 *	|        Magic Number 	     |
 *	+----------------------------+
 *	|xpEntry_t [For partition 0] | ------+ xpe_control is physical system
 *	+----------------------------+       |  address of control ring used
 *                   ....      		     |	to commuicate between "this"
 *					     | partition and partition [n].
 *      +----------------------------+ 	     |
 *	|xpEntry_t [For partition 63]|       |
 *	+----------------------------+       |
 *                                           |
 *      +------------------------------------+
 *      |
 *      |     +--------------------------+
 *      +---->|  Control Ring Address    | -------> (lnr_t *)
 *            +--------------------------+
 *            |    Link Pending Mask     |
 *	      +--------------------------+
 *	      |     Total link count     |
 *            +--------------------------+
 */

typedef struct xpEntry_s {
    __psunsigned_t	xpe_control;	/* control ring address (lnrXP_t) */
    __psunsigned_t	xpe_lpending;	/* local pending mask Pointer */
    __psunsigned_t	xpe_rpending;	/* remote pending mask */
    __uint32_t		xpe_cnt;	/* # links */
#   define		XPE_PAD	(CACHE_SLINE_SIZE - 			\
				 (sizeof(__psunsigned_t) * 3		\
				  + sizeof(__uint32_t)))
    char		xpe_pad[XPE_PAD];
} xpEntry_t;

typedef	struct xpMap_s {
    __uint64_t		xpm_magic;
#   define		XPM_MAGIC	0x1122334455667788
    __uint64_t		xpm_version;
#   define		XPM_VERSION	1
#   define		XPM_PAD	(CACHE_SLINE_SIZE - (sizeof(__uint64_t) * 2))
    char		xpm_pad[XPM_PAD]; /* Just in case */
    xpEntry_t		xpm_xpe[MAX_PARTITIONS];
} xpMap_t;    


/*
 * SN0net Interpartition messages
 *
 * Format:
 *
 *	+----------------------------+
 *	|       Message Header       |    (8-bytes, lnHdr_t)
 *  	+----------------------------+
 *	|       Message Entry 	     |    (248-bytes, lnEntry_t)
 *	|   Addr/Len Pointer OR	     |
 *	|      Immediate Data	     |
 *	+----------------------------+
 *
 * 
 * Messages are grouped into 3 types:
 *
 * 	1) Control - used for driver to driver hand shaking. 
 *		     They are a form of immediate message. The
 *		     difference is they are interpreted by the
 *		     driver.
 *	
 *			+----------------------------+
 *			|       Message Header	     | (8-Bytes, lnHdr_t)
 *			+----------------------------+
 *		 	|      Control Message	     | (8-bytes, lnCntl_t)
 *			+----------------------------+
 *	
 *	2) Immediate-Messages that contain data in the tail of the 
 *		     message. It may be part of a larger message, 
 *		     or stand along. 
 *
 *	3) Indirect- Messages that contain a series of address/length
 *		     pairs in the tail of the message. 
 *
 * SN0net Ring:
 *
 * 			+----------------------------+
 *			| Remotely visable pointers  |
 *			+----------------------------+
 *			|      	   Message	     |
 *			|	    Ring	     |
 *				............
 *			+----------------------------+
 *
 * 	Message Ring - (lnMsg_t[]) used for messages described above
 *	Remotely Visible 
 *	    pointers - (lnrXP_t) ring pointers etc that indicate what
 *		       stage data transfers are at.
 *
 */

/*
 * Defines: LN_ALIGN_XXX
 * Purpose: Defines the alignment requirments for DMA operations. All
 * 	    pointers passed across partitions are aligned on at least
 * 	    LN_ALIGN_SIZE boundary.
 */
#define	LN_ALIGN_SIZE		(128)
#define	LN_ALIGN_MASK		(~(LN_ALIGN_SIZE-1))
#define	LN_ALIGN(_x)		((__psunsigned_t)(_x) & LN_ALIGN_MASK)
#define	LN_OFFSET(_x)		((__psunsigned_t)(_x) & (LN_ALIGN_SIZE - 1))
#define	LN_ALIGNED(_x)		(!LN_OFFSET(_x))
#define	LN_UNITS(_x)		((__psunsigned_t)(_x) >> 7)

#define	LN_PIPE_UNITS		4

/*
 * Defines: LN_MESSAGE_SIZE/IDATA_CNT/VEC_CNT
 * Purpose: Defines the size of an actual message in bytes/number of bytes
 *	    that fit in the tail for immediate messages, and the number of
 *	    address/length pairs that fit for indirect messages.
 * Note: LN_IDATA_CNT *SHOULD* be >= the alignment size above to allow
 *	 alignment to required boundaries to be accomplished in 1
 *	 immediate message.
 *
 * 	 Message size should be a multiple of secondary cache line sizes.
 */
#define	LN_MESSAGE_SIZE		(256)
#define	LN_IDATA_CNT		(LN_MESSAGE_SIZE-sizeof(lnHdr_t))
#define	LN_VEC_CNT		(LN_IDATA_CNT/sizeof(lnVec_t))

/*
 * Define: LN_CONTROL_SZ
 * Purpose: Size of control ring used for internal communication.
 */
#define	LN_CONTROL_SZ		0x4000

/*
 * Typedef: lnMsg_t
 * Purpose: This starts all messages that are passed across the sn0net
 *	    interface. 
 */
typedef	struct lnHdr_s {
    unsigned char 	lnh_type;
#   define		LNH_T_INVALID	0 /* Invalid type */
#   define		LNH_T_CONTROL	1 /* Control message */
#   define		LNH_T_IMMEDIATE	2 /* Immediate data */
#   define		LNH_T_INDIRECT	3 /* Buffer pointers */
    unsigned char	lnh_flags;
#   define		LNH_F_CLEAR	0x00
#   define		LNH_F_NOTIFY	0x01 /* Notify on completion */
    unsigned short	lnh_count;	/*
                                         * Control: Total # entires that
					 *          follow header.
					 * Immediate : Total # bytes that 
					 *          follow header in message.
					 * Indirect  : Total # address/length
					 *          pairs that follow header.
					 */
    __uint32_t		lnh_seq;	/* Sequence # */
} lnHdr_t;

typedef struct lnVec_s {
    paddr_t		lnv_base;
    __uint32_t		lnv_len;
    __uint32_t		lnv_pad;
} lnVec_t;

typedef	struct lnMsg_s {
    lnHdr_t	lnm_hdr;
    union {
	lnVec_t		u_vec[LN_VEC_CNT];
	unsigned char	u_imd[LN_IDATA_CNT]; /* Immediate data*/
    } 			lnm_tail;	/* tail of message */
#   define	lnm_vec	lnm_tail.u_vec
#   define	lnm_imd lnm_tail.u_imd
} lnMsg_t;



/*
 * Typedef: Control Message
 * Purpose: Defines the format of a control message sent accross the
 *	    sn0net interface. 
 */ 
typedef struct lnCntl_s {
    unsigned char	lnc_cmd;	/* command - see below */
#   define	LN_CNTL_INVALID	 0
#   define	LN_CNTL_OPEN	 1	/* Open connection request */
#   define	LN_CNTL_NOP	 2	/* Do nothing */
#   define	LN_CNTL_SEND_NOP 3	/* Send NOP command */
#   define	LN_CNTL_RESET	 4	/* Reset the link */
#   define	LN_CNTL_TARGET	 5	/* Set target nasid */
#   define	LN_CNTL_SHUTDOWN 6	/* Terminate the connection */
    __uint32_t		lnc_link;	/* link # in question */
    int			lnc_errno;	/* remote errno */
    __uint64_t		lnc_p[32];	/* Command parameter */
} lnCntl_t;


/*
 * Define: LNR_GET
 * Purpose: Return a pointer to the "next" message in the incomming ring.
 * Parameters: lnr - pointer to the ring
 * Notes: Uses cached local get pointer.
 */
#define	LNR_GET(lnr)	&(lnr)->lnr_rr[(lnr)->lnr_clxp.xp_get]

/*
 * Macro: LNR_LXP_FLUSH(lnr)
 * Purpose: Update the X-partition ring pointers (Sync global copies)
 * Parameters: lnr - pointer to ring to update
 * Notes: It is very important that the sequence number be updated last 
 *	  as that will indicate to the remote partition that the message
 *	  ring has changed, and should be looked at %%% FORCE %%%
 */
#define	LNR_LXP_FLUSH(lnr) 	*(lnr)->lnr_lxp = (lnr)->lnr_clxp

/*
 * Define: LNR_RXP_CACHE(lnr)
 * Purpose: Pull over remote X-partition variables to local cache.
 */
#define	LNR_RXP_CACHE(lnr) 	lnPullData((lnr), (paddr_t)(lnr)->lnr_rxp, \
					   (paddr_t)&(lnr)->lnr_crxp,\
					   sizeof((lnr)->lnr_crxp))

/*
 * Macro: LNR_SEND_INT
 * Purpose: Issue cross call interrupt to notify remote end of 
 *	    ring event.
 * Parameters: lnr - pointer to ring structure to isse interrupt from
 */
#define	LNR_SEND_INT(lnr) \
{\
     atomicSetUlong(&ln_xpMap->xpm_xpe[lnr->lnr_partid].xpe_rpending, 	\
		    1 << (lnr)->lnr_link);				\
     HUB_S((lnr)->lnr_target, 0);					\
}

#define	LNR_LINK_PEND(p, m)	\
    (m) = *(__psunsigned_t *)ln_xpMap->xpm_xpe[p].xpe_lpending

#define	LNR_LINK_CLEAR(p, m)    \
    atomicClearUlong((unsigned long *)ln_xpMap->xpm_xpe[p].xpe_lpending, (m))

/*
 * Macro: LNR_AVAIL
 * Purpose: Determine the number of entires available for "put" on local
 * 	    ring.
 * Parameters: lnr - pointer to local ring
 * Notes: Uses CACHED copies of local/remote variables.
 */
#define	LNR_AVAIL(l)		(LNR_ADD((l)->lnr_clxp.xp_put,	\
				          l->lnr_clxp.xp_limit, 1) \
                                 != (l)->lnr_crxp.xp_get)

#define	LNR_PEND(l)		((l)->lnr_clxp.xp_get != 	\
				 (l)->lnr_crxp.xp_put)

#define	LNR_NEXT(l)		(l)->lnr_clxp.xp_put

/*
 * Define: LNR_ADD/LNR_SUB
 * Purpose: Add/Sub using the ring modulo arithmetic.
 * Parameters:	x - base value
 *		l - ring limit (modulo "l")
 *		y - value to add to x
 * Notes:	Result stored in "x"
 */
#define	LNR_ADD(x, l, y)	(((x) + (y)) % (l))
#define	LNR_SUB(x, l, y)	LNR_ADD((x), (l), (y) + (l))

#define	LNR_INC_GET(l)		(l)->lnr_clxp.xp_get = 			\
                                        LNR_ADD((l)->lnr_clxp.xp_get, 	\
					(l)->lnr_crxp.xp_limit,		\
					1)

#define	LNR_INC_PUT(l)		(l)->lnr_clxp.xp_put = 			\
                                        LNR_ADD((l)->lnr_clxp.xp_put,	\
					(l)->lnr_clxp.xp_limit,		\
					1)

#define	LNR_DEC_GET(l)		(l)->lnr_clxp.xp_get = 			\
                                        LNR_SUB((l)->lnr_clxp.xp_get, 	\
					(l)->lnr_crxp.xp_limit,	\
					1)

#define	LNR_DEC_PUT(l)		(l)->lnr_clxp.xp_put =			\
                                        LNR_SUB((l)->lnr_clxp.xp_put,	\
					(l)->lnr_clxp.xp_limit,		\
					1)



/*
 * Typedef: lnrXP_r
 * Purpose: Maps the variables visible accross partition boundaries.
 * Notes:   This structure should be kept < cache line size to avoid
 *	    moving more tham the minimum amount of data. Anything
 *	    up to a single cache line size is OK.
 */
typedef	struct lnrXP_s {
    __uint32_t	xp_put;			/* Next put index in local ring  */
    __uint32_t	xp_limit;		/* Max index + 1 in entry array  */
    __uint32_t	xp_get;			/* Next get index in remote ring */
    __uint32_t	xp_sequence;		/* Sequence # last NOTIFY */
    paddr_t 	xp_ring;		/* Physical address of ring */
} lnrXP_t;

/*
 * Typedef: lnrWrWait_t
 * Purpose: Describes a waiting I/O operation that requires notification
 * 	    of completion. 
 */
typedef struct lnrWrWait_s {
    __uint64_t	lw_sequence;		/* Sequnce number desired */
    sema_t	lw_sema;		/* semaphore to sleep on */
    struct lnrWrWait_s *lw_next;		/* link */
} lnrWrWait_t;

typedef	struct lnr_s {
    char	lnr_magic[4];		/* "lnr\0" */
    struct lnr_s *lnr_control;		/* pointer to control ring */
    __uint32_t	lnr_flags;		/* General flags */
#   define	LNR_F_AVAIL	0x0001	/* Link/Device available/OK */
#   define	LNR_F_RDPOLL	0x0002	/* Read poll/select is active */
#   define	LNR_F_WRPOLL	0x0004	/* Write poll/select is active */
#   define	LNR_F_XPOLL	0x0008	/* X condition poll/select active */
#   define	LNR_F_RD_WAIT	0x0010	/* Reader waiting */
#   define	LNR_F_WR_WAIT	0x0020	/* Write waiting */
#   define	LNR_F_SHUTDOWN	0x0040	/* link being shut down */
#   define	LNR_F_RSHUTDOWN 0x0080	/* remote end shut down */
#   define	LNR_F_OPEN 	0x0100	/* local open pending */
#   define	LNR_F_ROPEN	0x0200 	/* Remote open pending */
#   define	LNR_F_ERROR	0x0400	/* fatal error has occured */
#   define	LNR_F_CWAIT	0x0800	/* completion wait */
    volatile hubreg_t  *lnr_target;	/* target for interrupts */
    __uint32_t	lnr_link;		/* logical link # (/hw/xp/X/link */
    sema_t	lnr_wr_sema;		/* write semaphore */
    sema_t      lnr_rd_sema;		/* read semaphore */
    sv_t	lnr_rdwait_sv;		/* read data pending possibly */
    sv_t	lnr_wrwait_sv;		/* write waiting for notification */
    sema_t	lnr_wrResource_sema;	/* # writers allowed  */
    lock_t	lnr_lock;		/* lock for updating this struct */
#   define	LNR_WR_CNT	10	/* # active writes  */
    lnrWrWait_t	lnr_wrResources[LNR_WR_CNT];
    lnrWrWait_t *lnr_wrActive;		/* waiting active writes */
    lnrWrWait_t *lnr_wrActiveEnd;	/* wend of waiting active writes */
    lnrWrWait_t *lnr_wrFree;		/* free write resources */
    int		lnr_il;			/* temp IL value */
    partid_t	lnr_partid;		/* remote partition id */
    __uint32_t	lnr_sequence;		/* write sequence # */

    pfd_t	*lnr_pfd;		/* ring page descriptor */
    size_t	lnr_psz;		/* ring page size */
    
    paddr_t	lnr_copy[2];		/* copy buffers */

    lnMsg_t	*lnr_lr;		/* local ring */
    lnMsg_t	*lnr_rr;		/* remote ring */

    /* Pointers to allow "stream" appearance across Read calls */

    size_t	lnr_rcvLen;		/* Current remaining length */
    paddr_t	lnr_rcvCp;		/* Current pointer - receiver */
    lnVec_t	*lnr_rcvLnv;		/* Current lnVec receiver */
    lnVec_t	*lnr_rcvLnvEnd;		/* Current ending lnVec pointer */

    xpEntry_t	*lnr_lxpEntry;		/* local XP map array entry  */
    xpEntry_t	*lnr_rxpEntry;		/* remote XP map array entry */

    lnrXP_t	lnr_clxp;		/* Cache local variables (working) */
    lnrXP_t	lnr_crxp;		/* Cached remote XP variables */
    lnrXP_t	*lnr_lxp;		/* local X-partition variables */
    lnrXP_t	*lnr_rxp;		/* remote X-partition variables */

    struct pollhead *lnr_ph;		/* For polling stuff */
} lnr_t;

#define	LNR_CHECK(l)	ASSERT((((l)->lnr_magic[0] == 'l') &&		\
				((l)->lnr_magic[1] == 'n') &&		\
				((l)->lnr_magic[2] == 'r') &&		\
				((l)->lnr_magic[3] == '\0')))
/*
 * Define: LNRXP_SIZE 
 * Purpose: Number of bytes to allocate for the cross partition area. It
 * 	    is a mulitple of message sizes - and assumes the message
 *	    size of a power of 2, and is choses to reduce cache conflicts.
 */
#define	LNRXP_SIZE ((sizeof(lnrXP_t)+LN_MESSAGE_SIZE-1) & ~(LN_MESSAGE_SIZE-1))


#define	LN_LOCK(l)		(l)->lnr_il = \
                                    mp_mutex_spinlock(&(l)->lnr_lock)
#define	LN_UNLOCK(l)		mp_mutex_spinunlock(&(l)->lnr_lock, 	\
						    (l)->lnr_il)
#define	LN_IS_LOCKED(l)		spinlock_islocked(&(l)->lnr_lock)

#define	LN_READ_WAIT(l, r)	ASSERT(LN_IS_LOCKED(l));		\
                                (l)->lnr_flags |= LNR_F_RD_WAIT;	\
                                (r) = sv_wait_sig(&(l)->lnr_rdwait_sv, 	\
						  PWAIT|PCATCH, 	\
					    &(l)->lnr_lock, (l)->lnr_il)

#define	LN_WRITE_WAIT(l, r)	ASSERT(LN_IS_LOCKED(l));		\
                                (l)->lnr_flags |= LNR_F_WR_WAIT;	\
                                (r) = sv_wait_sig(&(l)->lnr_wrwait_sv, 	\
						  PWAIT|PCATCH, 	\
					    &(l)->lnr_lock, (l)->lnr_il)

#define	LN_READ_GO(l)		ASSERT(LN_IS_LOCKED(l)); 		\
                                if((l)->lnr_flags & LNR_F_RD_WAIT) {	\
				    (l)->lnr_flags &= ~LNR_F_RD_WAIT;	\
                                    sv_broadcast(&(l)->lnr_rdwait_sv);	\
				}

#define	LN_WRITE_GO(l)		ASSERT(LN_IS_LOCKED(l)); 		\
                                if((l)->lnr_flags & LNR_F_WR_WAIT) {	\
				    (l)->lnr_flags &= ~LNR_F_WR_WAIT;	\
                                    sv_broadcast(&(l)->lnr_wrwait_sv);	\
				}

#define	LN_WRITE_LOCK(l, r)	(r) = psema(&(l)->lnr_wr_sema, PWAIT|PCATCH)
#define	LN_READ_LOCK(l, r)	(r) = psema(&(l)->lnr_rd_sema, PWAIT|PCATCH)
#define	LN_WRITE_UNLOCK(l)	vsema(&(l)->lnr_wr_sema)
#define	LN_READ_UNLOCK(l)	vsema(&(l)->lnr_rd_sema)

#define	LN_WRITE(l,r)		(r) = psema(&l->lnr_wrResource_sema, 	\
					    PWAIT|PCATCH)
#define	LN_WRITE_DONE(l)	vsema(&(l)->lnr_wrResource_sema)

#define	LN_CONTROL_LINK		0
 
#endif /* __SYS_SN_LN_H__ */
