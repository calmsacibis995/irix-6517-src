/**************************************************************************
 *									  *
 * 		 Copyright (C) 1993 Silicon Graphics, Inc.	          *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#define	TCSSOCKET	(TIOC|34)	/* associate fd with socket */

struct tcssocket_info {
    int		tsi_fd;			/* socket FD */
#   define	TSI_NAME_SIZE	64
    char	tsi_name[TSI_NAME_SIZE]; /* terminal name */
#   define	TSI_OPTION_CNT	256
    char	tsi_options[TSI_OPTION_CNT];
    char	tsi_doDontResp[TSI_OPTION_CNT];
    char	tsi_willWontResp[TSI_OPTION_CNT];
};

#ifdef	_KERNEL
/*
 * Typedef: tn_t
 * Purpose: Defines a single telnet connection.
 * Notes: All fields in this structure are protected by the lock found in 
 *	  the struct itself, EXCEPT for th tn_state and tn_next fields.
 *	  Allocate and freeing of a device structure is protected by the 
 *	 the XXX lock.
 */

#include <sys/termio.h>

typedef struct tn {
    struct tn	*tn_next;		/* next on free list */
    uint_t	tn_flags;		/* current state */
#   define	TN_CLEAR	0x00	/* fresh */
#   define	TN_TXSTOP	0x01	/* stopped by XOFF */
#   define	TN_LIT		0x02	/* literal char seen */
#   define	TN_ICP		0x04	/* Instert character pending */
#   define	TN_OPTIONS	0x08	/* send telnet options as tty chngs */

#   define	TN_SIGIO	0x10	/* Asynch I/O enabled */
#   define	TN_EXCLUSIVE	0x20	/* exclusive open */
#   define	TN_FLOW_CNTL	0x40	/* XON/XOFF active */

    uchar_t	tn_open;		/* Protected by global tn_lock\ */
    int		tn_group;		/* group ID */
    lock_t 	tn_lock;		/* Protects ??? */
    queue_t	*tn_rq,		/* streams read  queue */
                *tn_wq;			/* streams write queue */
    toid_t	tn_rqBufCallId,		/* read CALL id 0 --> none pending */
                tn_wqBufCallId;		/* write call id 0--> none pending */
    int		tn_state;		/* telnet state */
    struct vsocket *tn_vsocket;		/* vsocket */
    struct termio tn_termio;		/* terminal information */
    struct vfile *tn_file;		/* kernel file pointer */
    struct winsize tn_ws;		/* window size */
    char	tn_options[TSI_OPTION_CNT];
    char	tn_doDontResp[TSI_OPTION_CNT];
    char	tn_willWontResp[TSI_OPTION_CNT];
    char	tn_soBuffer[100];	/* Sub-option accumulation buffer */
    char	*tn_soc;		/* Sub-option current pointer */
    char	*tn_soe;		/* Sub-option end of buffer */
    /*
     * The following 2 fields support the sending of data with embedded
     * IAC. tn_data is a list of mblk that are in the process of being
     * send out on the socket. They are already translated. All data
     * blocks linked off here MUST be sent before any blocks from 
     * tn_control can be sent.
     *
     * If tn_control is non-null, all blocks queued should be sent before
     * any more blocks are put on the tn_data queue.
     *
     * Both fields are linked lists through the b_next field.
     */
    mblk_t	*tn_data;		/* current data being sent */
    mblk_t	*tn_control;		/* current control being sent */
    char	tn_name[TSI_NAME_SIZE];	/* terminal name */
} tn_t;

extern	int	tn_maxunit;

#endif


