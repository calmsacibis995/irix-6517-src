#ident "$Revision: 1.1 $"

/**************************************************************************
 *                                                                        *
 *            Copyright (C) 1993-1994, Silicon Graphics, Inc.             *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ifndef _XLV_STAT_H
#define _XLV_STAT_H

extern  int     xlv_stats_enabled;      /* Whether or not to collect stats */

typedef struct xlv_stat_s {
        __uint32_t  xs_ops_read;            /* total read operations */
        __uint32_t  xs_ops_write;           /* total write operations */
        __uint64_t  xs_io_rblocks;          /* total read blocks */
        __uint64_t  xs_io_wblocks;          /* total write blocks */

        /* Stripe statistics */
        __uint32_t  xs_stripe_io;           /* total number of stripe io */
        __uint32_t  xs_su_total;            /* total stripe units */
        /*
         * Aligned means start address is stripe unit aligned.
         */
        __uint32_t  xs_align_full_sw;       /* aligned, full stripe width */
        __uint32_t  xs_align_less_sw;       /* aligned, < stripe width */
        __uint32_t  xs_align_more_sw;       /* aligned, > stripe width */
        __uint32_t  xs_align_part_su;       /* aligned, partial last unit */

        __uint32_t  xs_unalign_full_sw;     /* unaligned, full stripe width */
        __uint32_t  xs_unalign_less_sw;     /* unaligned, < stripe width */
        __uint32_t  xs_unalign_more_sw;     /* unaligned, > stripe width */
        __uint32_t  xs_unalign_part_su;     /* unaligned, partial last unit */

        __uint32_t  xs_max_su_len;          /* highwater mark su's in an io */
        __uint32_t  xs_max_su_cnt;          /* count of io with max */
} xlv_stat_t;

extern  xlv_stat_t **xlv_stat_tab;      /* array of stat pointers */

#endif /* _XLV_STAT_H */
