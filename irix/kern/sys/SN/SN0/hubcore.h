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

#ifndef	__SYS_SN_SN0_CORE_H__
#define	__SYS_SN_SN0_CORE_H__

/*
 * Hub core registers
 */

#ident "$Revision: 1.7 $"

/* All register "addresses" are actually offsets. */

#define CORE_MD_ALLOC_BW	0x2000b0	/* Warning: see bug 402533 */
#define CORE_ND_ALLOC_BW	0x2000b8	/* Warning: see bug 402533 */
#define CORE_DBGSEL		0x2000c0
#define CORE_ID_ALLOC_BW	CORE_DBGSEL
#define CORE_ARB_CTRL		0x2000c8
#define	CORE_REQQ_DEPTH		0x2000d0
#define CORE_REPQ_DEPTH		0x2000d8
#define CORE_FIFO_DEPTH		0x2000e0	/* Warning: see bug 402537 */
#ifdef _LANGUAGE_C
typedef union hubcore_reqqd_u {
	__uint64_t	hc_reqqd_value;
	struct {
		__uint64_t	reqqd_rsvd:	 3,
		                reqqd_niq_rq:	 6,
		                reqqd_noq_rqa:	 6,
		                reqqd_noq_rqb:	 6,
		                reqqd_piq_rq:	 6,
		                reqqd_poq_rq:	 7,
		                reqqd_iiq_rq:	 6,
		                reqqd_ioq_rq:	 7,
		                reqqd_miq_rqh:	 5,
		                reqqd_miq_rqd:	 6,
		                reqqd_moq_rq:	 6;
	} hc_reqqd_fields;
} hubcore_reqqd_t;

typedef union hubcore_repqd_u {
	__uint64_t	hc_repqd_value;
	struct {
		__uint64_t	repqd_rsvd:	 3,
		                repqd_niq_rp:	 6,
		                repqd_noq_rpa:	 6,
		                repqd_noq_rpb:	 6,
		                repqd_piq_rp:	 6,
		                repqd_poq_rp:	 7,
		                repqd_iiq_rp:	 6,
		                repqd_ioq_rp:	 7,
		                repqd_miq_rph:	 5,
		                repqd_miq_rpd:	 6,
		                repqd_moq_rp:	 6;
	} hc_repqd_fields;
} hubcore_repqd_t;

typedef union hubcore_fifod_u {
	__uint64_t	hc_fifod_value;
	struct { 
		__uint64_t	fifod_rsvd1:	1,
				fifod_ioq_thr:	7,
				fifod_rsvd2:	3,
				fifod_miq_hdr:	5,
				fifod_rsvd3:	2,
				fifod_miq_dat:	6,
				fifod_rsvd4:	2,
				fifod_niq_rpl:	6,
				fifod_rsvd5:	3,
				fifod_niq_req:	5,
				fifod_rsvd6:	2,
				fifod_piq_rpl:	6,
				fifod_rsvd7:	3,
				fifod_piq_req:	5,
				fifod_rsvd8:	2,
				fifod_iiq_fif:	6;
		} hc_fifod_fields;
} hubcore_fifod_t;

#endif /* LANGUAGE_C */
#endif /* __SYS_SN_SN0_CORE_H__ */
