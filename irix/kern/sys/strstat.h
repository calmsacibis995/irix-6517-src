/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SYS_STRSTAT_H	/* wrapper symbol for kernel use */
#define _SYS_STRSTAT_H	/* subject to change without notice */

#ident	"@(#)uts-3b2:io/strstat.h	1.2"

/*
 * Streams Statistics header file.  This file
 * defines the counters which are maintained for statistics gathering
 * under Streams. 
 */
typedef struct {
	int use;	/* current item usage count */
	int max;	/* maximum item usage count */
	int fail;	/* count of allocation failures */
} alcdat;

#define STRSTAT_MAX_BUF 	7 /* NOTE: Also in file stream.c */

struct  strstat {
	alcdat stream;		/* stream allocation data */
	alcdat queue;		/* queue allocation data */
	alcdat buffer[STRSTAT_MAX_BUF];	/* msg/msgdblk & bufs alloc data */
	alcdat linkblk;		/* linkblk allocation data */
	alcdat strevent;	/* strevent allocation data */
	alcdat qbinfo;		/* Queue band info allocation data */
};

/*
 * per-module statistics structure
 */
struct module_stat {
	long ms_pcnt;		/* count of calls to put proc */
	long ms_scnt;		/* count of calls to service proc */
	long ms_ocnt;		/* count of calls to open proc */
	long ms_ccnt;		/* count of calls to close proc */
	long ms_acnt;		/* count of calls to admin proc */
	char *ms_xptr;		/* pointer to private statistics */
	short ms_xsize;		/* length of private statistics buffer */
};

/*
 * Debug Stream procedure call statistics
 * Maintained for DEBUG kernels only.
 */
struct str_procstats {
	u_int allocb_calls;		/* Number calls */
	u_int allocb_ok;		/* # times success */
	u_int allocb_md_fail;		/* # times fail msg/dblk allocated */
	u_int allocb_buf_fail;		/* # times fail buffer allocation */
	u_int allocb_nobuf_ok;		/* # times no buf request succeeded */
	u_int allocb_nobuf_fail;	/* # times no buf request but failed */
	u_int allocb_bigbuf_ok;		/* # times BIG buf request succeeded */
	u_int allocb_bigbuf_fail;	/* # times BIG buf request failed */

	u_int allocb_nobuffer_calls;	/* Number calls */
	u_int allocb_nobuffer_ok;	/* # times success */
	u_int allocb_nobuffer_fail;	/* # times failed */
	
	u_int allocb_freebigbuf_calls;	/* Number calls */

	u_int esballoc_calls;		/* Number calls */
	u_int esballoc_ok;		/* # times success */
	u_int esballoc_fail_null;	/* # times null pointers passed */
	u_int esballoc_fail_allocb;	/* # times allocb() failed */

	u_int esbbcall_calls;		/* Number calls */
	u_int esbbcall_ok;		/* # times success */
	u_int esbbcall_fail_sealloc;	/* # times sealloc() failed */

	u_int testb_calls;		/* Number calls */
	u_int bufcall_calls;		/* Number calls */
	u_int linkb_calls;		/* Number calls */
	u_int unlinkb_calls;		/* Number calls */
	u_int rmvb_calls;		/* Number calls */
	u_int msgdsize_calls;		/* Number calls */
	u_int str_msgsize_calls;	/* Number calls */

	u_int bufcall_ok;		/* # times success */
	u_int bufcall_fail_sealloc;	/* # times sealloc() failed */

	u_int adjmsg_calls;		/* Number calls */
	u_int adjmsg_fail_xmsgsize;	/* # times failed due to xmsgsize */
	u_int adjmsg_fromhead;		/* # times adjusted head of list */
	u_int adjmsg_fromtail;		/* # times adjusted tail of list */

	u_int copymsg_calls;		/* Number calls */
	u_int copymsg_ok;		/* # times success */
	u_int copymsg_fail_null;	/* # times null parameter pointers */
	u_int copymsg_fail_copyb;	/* # times copyb() failed */

	u_int dupb_calls;		/* Number calls */
	u_int dupb_ok;			/* # times success */
	u_int dupb_fail_nomsg;		/* # times failed to get msg allocate*/

	u_int dupmsg_calls;		/* Number calls */
	u_int dupmsg_fail_null;		/* # times null paramter passed */
	u_int dupmsg_fail_dupb;		/* # times failed to get msg allocate*/
	u_int dupmsg_dupb;		/* # dupb calls inside dupmsg */
	u_int dupmsg_ok;		/* # times success */

	u_int freeb_calls;		/* # calls */
	u_int freeb_decrefcnt_ourmsg; /* # times decr refcnt and it's our msg*/
	u_int freeb_decrefcnt_notourmsg; /* # times decr refcnt & not our msg*/
	u_int freeb_strbcflag;		/* # times refcnt > 0 with strbcwait */
	u_int freeb_refcnt_zero;	/* # times decr refcnt resulted zero */
	u_int freeb_notourmsg;		/* # times not our msg header */
	u_int freeb_ourmsg;		/* # times it was our msg header */
	u_int freeb_extfun;		/* # times with extended function */
	u_int freeb_extfun_null;	/* # times db_frtnp set but func null*/
	u_int freeb_frtnp_null;		/* # times db_frtnp null */

	u_int freemsg_calls;		/* # calls */
	u_int freemsg_null;		/* # calls with null ptr */
	u_int freemsg_freeb;		/* # freeb calls inside freemsg */
	u_int freemsg_ok;		/* # times simple exit */

	u_int lf_addpage_calls;		/* # calls */
	u_int lf_addpage_empty;		/* # times get list head null */
	u_int lf_addpage_occupied;	/* # times get list head non-zero */

	u_int lf_get_calls;		/* # calls */
	u_int lf_get_local;		/* # times lf_get got via local list */
	u_int lf_get_addpage;		/* # times added page of nodes */
	u_int lf_get_fail;		/* # times returned null */

	u_int lf_getpage_tiled_calls;	/* # calls */
	u_int lf_getpage_tiled_reclaim; /* # times got reclaimed page */
	u_int lf_getpage_tiled_newpage; /* # times got new page */
	u_int lf_getpage_tiled_failed;  /* # times totally failed */
	u_int lf_getpage_tiled_maxwait; /* # times over limit and waited */
	u_int lf_getpage_tiled_maxnowait; /* # times over limit and no wait */

	u_int lf_free_calls;		/* # calls */
	u_int lf_free_localadd;		/* # times node added to local list */
	u_int lf_free_reclaimpage_set;	/* # times reclaim_page set */
	u_int lf_free_reclaimpage_clear; /* # times reclaim_page zeroed */
	u_int lf_free_reclaimpage_incr;	 /* # times reclaim_page nelts incr */
	u_int lf_free_pagesize_norelease; /* # times page sized NOT releaased*/
	u_int lf_free_pagesize_release; /* # times page sized releaased */

	u_int lf_freepage_calls;	/* # calls */
	u_int lf_freepage_release;	/* # times page released to kernel */
	u_int lf_freepage_thresh_recycle;/* # times page recycled > threshold*/
	u_int lf_freepage_recycle;	/* # times page recycled for use */

	u_int pullupmsg_calls;		/* # calls */
	u_int pullupmsg_m1_aligned;	/* # times len == -1 and aligned */
	u_int pullupmsg_notm1_aligned;	/* # times len != -1 and aligned */
	u_int pullupmsg_xmsgsize_fail;	/* # times xmsgsize failed */
	u_int pullupmsg_refcnt;		/* # times refcnt > 0 after decr */
	u_int pullupmsg_refcnt_zero;	/* # times refcnt == 0 after decr */

	u_int str_getbuf_index_calls;	/* # calls */
	u_int str_getbuf_index_ltzero;	/* # times called lt zero */
	u_int str_getbuf_index_zero;	/* # times called eq zero */
	u_int str_getbuf_index_64;	/* # times returned 64 byte size */
	u_int str_getbuf_index_256;	/* # times returned 256 byte size */
	u_int str_getbuf_index_512;	/* # times returned 512 byte size */
	u_int str_getbuf_index_2K;	/* # times returned 2K byte size */
	u_int str_getbuf_index_page;	/* # times returned page size */
};

/*
 * Debug Stream mbuf_mblk conversion procedure call statistics
 * Maintained for DEBUG kernels only.
 */
struct str_mblkstats {
	u_int mblk2mbuf_calls;		/* # times mblk_to_mbuf called */
	u_int mblk2mbuf_null;		/* # times called with null ptr */

	u_int mblk2mbuf_dupfun;		/* # times dup function called */
	u_int mblk2mbuf_mclx_ok;	/* # times mbuf mclgetx sucess */
	u_int mblk2mbuf_mclx_fail;	/* # times mbuf mclgetx failed */

	u_int mblk2mbuf_free_freeb;	/* # times freeb() called (last ref) */
	u_int mblk2mbuf_free_RefGTone;  /* # times ref ct > 1 */
	u_int mblk2mbuf_free_bogus;     /* # times MT_FREE mbuf passed */

	u_int mbuf2mblk_calls;		/* # times mbuf_to_mblk called */
	u_int mbuf2mblk_esballoc_ok;	/* # calls to esballoc sucess */
	u_int mbuf2mblk_esballoc_fail;	/* # calls to esballoc failed */
	u_int mbuf2mblk_allocb_ok;	/* # times allocb() success */
	u_int mbuf2mblk_allocb_fail;	/* # times allocb() failed */

	u_int mbuf2mblk_free;		/*# calls to mbuf_to_mblk_free */
};

#endif	/* _SYS_STRSTAT_H */
