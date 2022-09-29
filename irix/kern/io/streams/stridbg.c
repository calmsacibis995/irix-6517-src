/**************************************************************************
 *	 	Copyright (C) 1986-1997 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded	instructions, statements, and computer programs	 contain  *
 *  unpublished	 proprietary  information of Silicon Graphics, Inc., and  *
 *  are	protected by Federal copyright law.  They  may	not be disclosed  *
 *  to	third  parties	or copied or duplicated	in any form, in	whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident	"$Revision: 1.4 $"

#include <bstring.h>
#include <ctype.h>
#include <limits.h>
#include <sys/types.h>
#include <ksys/as.h>
#include <sys/reg.h>
#include <sys/systm.h>
#include <sys/param.h>
#include <ksys/vpgrp.h>
#include <ksys/vproc.h>
#include <os/proc/vpgrp_private.h>
#include <os/proc/vproc_private.h>
#include <sys/sbd.h>
#include <ksys/xthread.h>
#include <sys/cpu.h>
#include <sys/loaddrs.h>
#include <sys/sysmacros.h>
#include <sys/pcb.h>
#include <sys/signal.h>
#include <sys/flock.h>
#include <sys/sema.h>
#include <sys/sema_private.h>
#include <sys/swap.h>
#include <sys/proc.h>
#include "os/proc/pproc_private.h"
#include <ksys/vsession.h>
#include <sys/arsess.h>
#include <ksys/exception.h>
#include <sys/sysinfo.h>
#include <sys/map.h>
#include <ksys/vfile.h>
#include <ksys/fdt_private.h>
#include <sys/vfs.h>
#include <sys/vnode_private.h>
#include <sys/poll.h>
#include <sys/quota.h>
#include <sys/cmn_err.h>
#include <ksys/cred.h>
#include <sys/debug.h>
#include <sys/pda.h>
#include <sys/splock.h>
#include <sys/conf.h>
#include <sys/lock.h>
#include <sys/callo.h>
#include <sys/calloinfo.h>
#include <sys/var.h>
#include <sys/edt.h>
#include <sys/runq.h>
#include <sys/rt.h>
#include <sys/q.h>
#include <sys/idbg.h>
#include <sys/idbgentry.h>
#include <sys/inst.h>
#include <sys/errno.h>
#include <sys/nodepda.h>
#include <sys/kmem.h>
#include <sys/sysmp.h>
#include <sys/syssgi.h>
#include <sys/cachectl.h>
#include <ksys/kern_heap.h>
#include <elf.h>

#undef	TRUE
#undef	FALSE
#include <sys/strsubr.h>
#include <sys/strstat.h>
#include <sys/strmp.h>

void	idbg_clfcnts(__psint_t);
void	idbg_clfvec(__psint_t);
void	idbg_cstrstats(__psint_t);
void	idbg_lfvec(__psint_t addr);
#ifdef DEBUG
void	idbg_gstrcalls(void);
#endif /* DEBUG */

void	idbg_gstrvars(void);
void	idbg_strdatab(__psint_t);
void	idbg_strhead(__psint_t);
void	idbg_strinfo(void);
void	idbg_strintr();
void	idbg_strintrb(__psint_t);
void	idbg_strintrlist(__psint_t);
void	idbg_strlocks();
void	idbg_strmodinfo(__psint_t);
void	idbg_strmon(__psint_t);
void	idbg_strmsgb(__psint_t);
void	idbg_strmsgbw(__psint_t);
void	idbg_strmsgbws(__psint_t);
void	idbg_strmsgbwsi(__psint_t);

void	idbg_strqband(__psint_t);
void    idbg_strqhead();
void	idbg_strqinfo(__psint_t);
void	idbg_strqueue(__psint_t);
void    idbg_strqueinfo(__psint_t);
void	idbg_strstats(__psint_t);

/* define DEBUG_MBLK_TIMING 1 */

#if defined(DEBUG) && defined(DEBUG_MBLK_TIMING) && defined(R10000)
void	idbg_msgbt1(__psint_t);
void	idbg_msgbt2(__psint_t);
void	idbg_msgbt3(__psint_t);
void	idbg_msgbt4(__psint_t);
void	idbg_msgbt5(__psint_t);
void	idbg_msgbt6(__psint_t);
void	idbg_msgbtlk(__psint_t);
#endif /* DEBUG_MBLK_TIMING */

/*
 * External Global Variables
 */
extern struct strinfo Strinfo[];

extern int str_page_got, str_page_rel;
extern int str_page_cur, str_page_max;
extern int str_num_pages, str_min_pages;
extern char strbcflag, strbcwait;
extern int page_keep_lbolt, page_obtained_lbolt, page_released_lbolt;

extern struct bclist strbcalls;
extern char strbcflag;
extern char strbcwait;

extern int nstrintr;
extern lock_t str_intr_lock;
extern struct strintr *strintrfree;
extern struct strintr *strintrrsrv;

extern int streams_intr_avail;
extern int streams_intr_lowat;
extern sv_t joinwait;
extern mon_t *privmon_list;
extern int privmon_cnt;
extern int strpmonmax;

extern int strgiveback_cpuid, strgiveback_token, strgiveback_last_cpuid;

#ifdef DEBUG
extern time_t strgiveback_lbolt;
extern int strgiveback_calls, strgiveback_lastrel, strgiveback_totrel;

extern int streams_intr_inuse, streams_intr_inuse_max;
extern int streams_intr_rsrvd, streams_intr_rsrvd_max;

extern struct str_procstats str_procstats;
extern struct str_mblkstats str_mblkstats;
#endif /* DEBUG */

/*
 * Forward Referenced Procedures
 */
int idbg_count_bytes(register queue_t *);
int idbg_count_msgs(register queue_t *);

#define VD	(void (*)())

/*
 * Values of the b_mopsflag in the struct msgb definition
 */
char *tab_b_mopsflag[] = {
	"ALLOCMSG,",	      /* 0x0001 */
	"ALLOCMD,",	      /* 0x0002 */
	"ALLOCMD_ESB,",	      /* 0x0004 */
	"FREEMSG,",	      /* 0x0008 */
	"FREEMD,",	      /* 0x0010 */
	"FREEMD_ESB,",	      /* 0x0020 */
	"MBLK_TO_MBUF,",      /* 0x0040 */
	"MBLK_TO_MBUF_DUP,",  /* 0x0080 */
	"MBLK_TO_MBUF_FREE,", /* 0x0100 */
	"MBUF_TO_MBLK,",      /* 0x0200 */
	"MBUF_TO_MBLK_FREE,", /* 0x0400 */
	0
};

/*
 * Values of the stream head (stdata) field 'sd_flag'
 */
char *strh_flag_name[] = {
	"iocwait",
	"rsleep",
	"wsleep",
	"strpri",
	"strhup",
	"strwopen",
	"stplex",
	"stristty",
	"rmsgdis",
	"rmsgnodis",
	"strderr",
	"strtime",
	"str2time",
	"str3time",
	"strclose",
	"sndmread",
	"oldndelay",
	"rdbufwait",
	"strsndzero",
	"strtostop",
	"rdprotdat",
	"rdprotdis",
	"strmount",
	"strsigpipe",
	"strdelim",
	"stwrerr",
	"strhold",
#ifndef _SVR4_IOCTL
	"stfionbio",
	"cttyflg",
#endif /* _SVR4_IOCTL */
};

/*
 * Values of the testable and non-testable struct pollfd events and revents
 * for the poll head stored in the stream head (stdata) as 'sd_pollist'
 */
char *strh_pollflags[] = {
	"POLLIN",	/* 0x0001 */
	"POLLPRI",	/* 0x0002 */
	"POLLOUT",      /* 0x0004 */
	"POLLERR",	/* 0x0080 */
	"POLLHUP",	/* 0x0010 */
	"POLLNVAL",	/* 0x0020 */
	"POLLNORM/POLLRDNORM", /* 0x0040 */
	"POLLRDBAND",	/* 0x0080 */
	"POLLWRBAND",	/* 0x0100 */
	"POLLCONN",	/* 0x0200 */
};

/*
 * Values of the stream head (stdata) field 'sd_sigflags'
 */
char *strh_sigflags[] =	{
	"input",
	"hipr",
	"output/rdnorm",
	"msg",
	"error",
	"hangup",
	"rdnorm",
	"rdband",
	"wrband",
	"bandurg",
};

/*
 * String names for the various Stream buffer types
 */
char *lf_bufnames[LFVEC_MAX_INDEX] = {
	"MSG",		/* 0x00000 */
	"MD",		/* 0x00001 */
	"BUF64",	/* 0x00002 */
	"BUF256",	/* 0x00003 */
	"BUF512",	/* 0x00004 */
	"BUF2K",	/* 0x00005 */
	"BUFPAGE"	/* 0x00006 */
};

struct stridbg_funcs_s {
	char	*name;
	void	(*func)(int);
	char	*help;
} stridbg_funcs[] = { /* names must be unique within first 11 characters */

	"clfcnts", VD idbg_clfcnts,
		"Print Stream lfvec list counts for each cpu",
	"clfvec", VD idbg_clfvec,
		"Print Stream lfvec struct address for each cpu",
	"cstrstats", VD idbg_cstrstats,
		"Print Stream strstat struct address for each cpu",
	"datab", VD idbg_strdatab, "Dump a streams data block",
#ifdef DEBUG
	"gstrcalls", VD idbg_gstrcalls,
		"Print Global Stream procedure call statistics",
#endif /* DEBUG */

	"gstrvars", VD idbg_gstrvars, "Print Global Stream variables",
	"lfvec", VD idbg_lfvec,
		"Print Stream lfvec structure given it's address",
	"modinfo", VD idbg_strmodinfo, "Print a streams module info struct",
	"msgb",	VD idbg_strmsgb, "Dump a streams message block",
	"msgbw", VD idbg_strmsgbw, "Streams msgb list walk and dump entries",
	"msgbws", VD idbg_strmsgbws,
		"Streams msgb list walk and dump summary info",
#ifdef DEBUG
	"msgbwsi", VD idbg_strmsgbwsi,
		"Strinfo msgb list walk and dump full entries",
#endif
#if defined(DEBUG) && defined(DEBUG_MBLK_TIMING) && defined(R10000)
	"msgbt1", VD idbg_msgbt1, "Execution times of allocb/freeb times",
	"msgbt2", VD idbg_msgbt2, "Execution times of allocb/freeb times",
	"msgbt3", VD idbg_msgbt3, "Execution times of allocb/freeb pair times",
	"msgbt4", VD idbg_msgbt4, "Execution times for few allocb/freeb calls",
	"msgbt5", VD idbg_msgbt5,"Execution times for many allocb/freeb calls",
	"msgbt6", VD idbg_msgbt6,
	   "Execution times for small consecutive allocb/dupb/freeb calls",
	"msgbtlk", VD idbg_msgbtlk, "Execution times for mutex_spinlock",
#endif /* DEBUG_MBLK_TIMING */

	"qband", VD idbg_strqband, "Dump a streams priority band",
	"qinfo", VD idbg_strqinfo, "Print a streams queue info struct",
	"strh",	 VD idbg_strhead, "Dump a stream head structure",
	"strinfo", VD idbg_strinfo, "Dump Strinfo struct of list head ptrs",
	"strintr", VD idbg_strintr, "Print streams interrupt block pool info",
	"strintrb",VD idbg_strintrb, "Print streams interrupt block",
	"strintrlist", VD idbg_strintrlist,"Print members of streams intr blk",
	"strlocks",    VD idbg_strlocks, "Print info about streams locks",
	"strmon",      VD idbg_strmon, "Print info about a streams monitor",
	"strq",        VD idbg_strqueue, "Dump a streams queue",
	"strqhead",    VD idbg_strqhead, "Print the enabled list",
	"strqueinfo",  VD idbg_strqueinfo, "Dump a streams struct queinfo",
	"strstats", VD idbg_strstats, "Print Stream stat struct given address",
	0,	0,	0,
};

/*
 * Initialization procedure
 */
void
stridbginit(void)
{
	struct stridbg_funcs_s *p;

	for (p = stridbg_funcs; p->func; p++) {
		idbg_addfunc(p->name, (void (*)())(p->func));
	}
}

/*
 * print flags
 */
void
str_printflags(uint64_t flags, char **strings, char *name)
{
	register uint64_t mask = 1;

	if (name)
		qprintf("%s 0x%llx <", name, flags);
	while (flags != 0 && *strings) {
		if (mask & flags) {
			qprintf("%s ", *strings);
			flags &= ~mask;
		}
		mask <<= 1;
		strings++;
	}
	if (name)
		qprintf("> ");
	return;
}

/*
 * Count the total number of messages queued on a streams queue
 */
int
idbg_count_msgs(register queue_t *q)
{
	register mblk_t *bp;
	register int i = 0;

	for (bp = q->q_first; bp; bp = bp->b_next)
		i++;

	return i;
}

/*
 * Count the total number of data bytes queued on a streams queue
 */
int
idbg_count_bytes(register queue_t *q)
{
	register mblk_t *bp;
	register mblk_t *mp;
	register int i = 0;

	for (bp = q->q_first; bp; bp = bp->b_next)
		for (mp = bp; mp; mp = mp->b_cont)
			i += mp->b_wptr - mp->b_rptr;

	return i;
}

#ifdef DEBUG
/*
 *  kp gstrcalls
 *  Prints contents of the global Streams procedure call statistics structure
 */
void
idbg_gstrcalls(void)
{
	qprintf("Stream procedure call counts\n");

	qprintf(" allocb_calls %d, ok %d, md_fail %d, buf_fail %d\n",
		str_procstats.allocb_calls, str_procstats.allocb_ok,
		str_procstats.allocb_md_fail, str_procstats.allocb_buf_fail);

	qprintf(" allocb_nobuf ok %d, fail %d, allocb_bigbuf ok %d, fail %d\n",
	 str_procstats.allocb_nobuf_ok, str_procstats.allocb_nobuf_fail,
	 str_procstats.allocb_bigbuf_ok, str_procstats.allocb_bigbuf_fail);

	qprintf(" allocb_nobuffer_calls %d, ok %d, fail %d\n",
		str_procstats.allocb_nobuffer_calls,
		str_procstats.allocb_nobuffer_ok,
		str_procstats.allocb_nobuffer_fail);

	qprintf(" allocb_freebigbuf_calls %d\n",
		str_procstats.allocb_freebigbuf_calls);

	qprintf(" esballoc_calls %d, ok %d, fail_null %d, fail_allocb %d\n",
		str_procstats.esballoc_calls,
		str_procstats.esballoc_ok,
		str_procstats.esballoc_fail_null,
		str_procstats.esballoc_fail_allocb);

	qprintf(" esbbcall_calls %d, ok %d, fail_sealloc %d\n",
		str_procstats.esbbcall_calls,
		str_procstats.esbbcall_ok,
		str_procstats.esbbcall_fail_sealloc);

	qprintf(" testb_calls %d, bufcall_calls %d, rmvb_calls %d\n",
		str_procstats.testb_calls,
		str_procstats.bufcall_calls,
		str_procstats.rmvb_calls);
	
	qprintf(" linkb_calls %d, unlinkb_calls %d\n",
		str_procstats.linkb_calls,
		str_procstats.unlinkb_calls);

	qprintf(" msgdsize_calls %d, str_msgsize_calls %d\n",
		str_procstats.msgdsize_calls,
		str_procstats.str_msgsize_calls);

	qprintf(" bufcall_ok %d, fail_sealloc %d\n",
		str_procstats.bufcall_ok, str_procstats.bufcall_fail_sealloc);

	qprintf(" adjmsg_calls %d, fail_xmsgsize %d, fromhead %d, fromtail %d\n",
		str_procstats.adjmsg_calls,
		str_procstats.adjmsg_fail_xmsgsize,
		str_procstats.adjmsg_fromhead,
		str_procstats.adjmsg_fromtail);

	qprintf(" copymsg_calls %d, ok %d, fail_null %d, fail_copyb %d\n",
		str_procstats.copymsg_calls,
		str_procstats.copymsg_ok,
		str_procstats.copymsg_fail_null,
		str_procstats.copymsg_fail_copyb);

	qprintf(" dupb_calls %d, ok %d, fail_nomsg %d\n",
		str_procstats.dupb_calls,
		str_procstats.dupb_ok,
		str_procstats.dupb_fail_nomsg);

qprintf(" dupmsg_calls %d, fail_null %d, fail_dupb %d, dupb calls %d, ok %d\n",
		str_procstats.dupmsg_calls,
		str_procstats.dupmsg_fail_null,
		str_procstats.dupmsg_fail_dupb,
		str_procstats.dupmsg_dupb,
		str_procstats.dupmsg_ok);

	qprintf(" freeb_calls %d\n", str_procstats.freeb_calls);
	qprintf("  freeb decrefcnt: ourmsg %d, notourmsg %d\n",
		str_procstats.freeb_decrefcnt_ourmsg,
		str_procstats.freeb_decrefcnt_notourmsg);

	qprintf("  freeb refcnt_zero %d, ourmsg %d, notourmsg %d\n",
		str_procstats.freeb_refcnt_zero,
		str_procstats.freeb_ourmsg,
		str_procstats.freeb_notourmsg);

	qprintf("  freeb frtnp_null %d, extfun %d, extfun_null %d\n",
		str_procstats.freeb_frtnp_null,
		str_procstats.freeb_extfun,
		str_procstats.freeb_extfun_null);

	qprintf(" freemsg_calls %d, null %d, freeb %d, ok %d\n",
		str_procstats.freemsg_calls,
		str_procstats.freemsg_null,
		str_procstats.freemsg_freeb,
		str_procstats.freemsg_ok);

	qprintf(" pullupmsg_calls %d, m1_aligned %d, notm1_aligned %d\n",
		str_procstats.pullupmsg_calls,
		str_procstats.pullupmsg_m1_aligned,
		str_procstats.pullupmsg_notm1_aligned);
	qprintf("  pullupmsg: xmsgsize_fail %d, refcnt %d, refcnt_zero %d\n",
		str_procstats.pullupmsg_xmsgsize_fail,
		str_procstats.pullupmsg_refcnt,
		str_procstats.pullupmsg_refcnt_zero);

qprintf(" str_getbuf_index: calls %d, [LT 0] %d, [EQ 0] %d, [1-64] %d\n",
		str_procstats.str_getbuf_index_calls,
		str_procstats.str_getbuf_index_ltzero,
		str_procstats.str_getbuf_index_zero,
		str_procstats.str_getbuf_index_64);

qprintf("  str_getbuf_index: [1-256] %d, [257-512] %d, [513-2048] %d, [2049-PAGE] %d\n",
		str_procstats.str_getbuf_index_256,
		str_procstats.str_getbuf_index_512,
		str_procstats.str_getbuf_index_2K,
		str_procstats.str_getbuf_index_page);

	qprintf(" mblk_to_mbuf %d, null_arg %d\n",
		str_mblkstats.mblk2mbuf_calls,
		str_mblkstats.mblk2mbuf_null);

	qprintf("  mblk_to_mbuf: mclx_ok %d, mclx_fail %d\n",
		str_mblkstats.mblk2mbuf_mclx_ok,
		str_mblkstats.mblk2mbuf_mclx_fail);

	qprintf(" mblk_to_mbuf_dup %d\n", str_mblkstats.mblk2mbuf_dupfun);

	qprintf(" mblk_to_mbuf_free [freeb %d, RefGTone %d, bogus %d]\n",
		str_mblkstats.mblk2mbuf_free_freeb,
		str_mblkstats.mblk2mbuf_free_RefGTone,
		str_mblkstats.mblk2mbuf_free_bogus);

	qprintf(" mbuf_to_mblk: %d, esballoc[ok %d, fail %d]\n",
		str_mblkstats.mbuf2mblk_calls,
		str_mblkstats.mbuf2mblk_esballoc_ok,
		str_mblkstats.mbuf2mblk_esballoc_fail);
	qprintf("  mbuf_to_mblk: mbufZeroLen: allocb[ok %d, fail %d]\n",
		str_mblkstats.mbuf2mblk_allocb_ok,
		str_mblkstats.mbuf2mblk_allocb_fail);

	qprintf(" mbuf_to_mblk_free: calls %d\n",
		str_mblkstats.mbuf2mblk_free);

	qprintf(" lf_addpage_calls %d, empty %d, occupied %d\n",
		str_procstats.lf_addpage_calls,
		str_procstats.lf_addpage_empty,
		str_procstats.lf_addpage_occupied);

	qprintf(" lf_getpage_tiled: calls %d, reclaim %d, newpage %d\n",
		str_procstats.lf_getpage_tiled_calls,
		str_procstats.lf_getpage_tiled_reclaim,
		str_procstats.lf_getpage_tiled_newpage);
	qprintf("  lf_getpage_tiled: failed %d, maxwait %d, maxnowait %d\n",
		str_procstats.lf_getpage_tiled_failed,
		str_procstats.lf_getpage_tiled_maxwait,
		str_procstats.lf_getpage_tiled_maxnowait);

	qprintf(" lf_get: calls %d, local %d, addpage %d, fail %d\n",
		str_procstats.lf_get_calls, str_procstats.lf_get_local,
		str_procstats.lf_get_addpage, str_procstats.lf_get_fail);

	qprintf(" lf_free: calls %d, localadd %d, reclaimpage_set %d\n",
		str_procstats.lf_free_calls,
		str_procstats.lf_free_localadd,
		str_procstats.lf_free_reclaimpage_set);
	qprintf("  lf_free: reclaimpage_clear %d, reclaimpage_incr %d\n",
		str_procstats.lf_free_reclaimpage_clear,
		str_procstats.lf_free_reclaimpage_incr);
	qprintf("  lf_free: pagesize_norelease %d, pagesize_release %d\n",
		str_procstats.lf_free_pagesize_norelease,
		str_procstats.lf_free_pagesize_release);

qprintf(" lf_freepage: calls %d, release %d, thresh_recycle %d, recycle %d\n",
		str_procstats.lf_freepage_calls,
		str_procstats.lf_freepage_release,
		str_procstats.lf_freepage_thresh_recycle,
		str_procstats.lf_freepage_recycle);

	return;
}
#endif /* DEBUG */

/*
 *  kp gstrvars
 *  Prints contents of the global Streams variables
 */
void
idbg_gstrvars(void)
{
	qprintf("Streams global variables\n");

	qprintf(" str_page_got %d, str_page_rel %d\n",
		str_page_got, str_page_rel);

	qprintf(" str_page_cur %d, str_page_max %d\n",
		str_page_cur, str_page_max);

	qprintf(" str_num_pages %d, str_min_pages %d\n",
		str_num_pages, str_min_pages);

	qprintf(" strbcflag %d, strbcwait %d\n", strbcflag, strbcwait);

	qprintf(" page: keep_lbolt %d, obtained_lbolt %d, released_lbolt %d\n",
		page_keep_lbolt, page_obtained_lbolt, page_released_lbolt);

	qprintf(" strgiveback: token %d, cpuid %d, last_cpuid %d\n",
		strgiveback_token, strgiveback_cpuid, strgiveback_last_cpuid);
#ifdef DEBUG
	qprintf(" strgiveback: calls %d, totrel %d\n",
		strgiveback_calls, strgiveback_totrel);
	qprintf(" strgiveback: lbolt %d, lastrel %d\n",
		strgiveback_lbolt, strgiveback_lastrel);
#endif /* DEBUG */
	return;
}

/*
 *  kp cstrstats cpuid
 *  Prints the strstat structure address for each cpu.
 */
void
idbg_cstrstats(__psint_t addr)
{
	register struct strstat *kstr_statsp;
	register int i, min, max;

	if (addr == -1) { /* no cpuid specified so dump them all */
		min = 0;
		max = (int)maxcpus;
	} else {
		min = addr;
		max = min + 1;

		if (min < 0) {
			min = 0;
			max = maxcpus;
		} else {
			if (max > (u_short)maxcpus) {
				min = 0;
				max = maxcpus;
			}
		}
	}
	for (i = min; i < max; i++) {
		kstr_statsp = (struct strstat *)pdaindr[i].pda->kstr_statsp;
		qprintf("pdaindr ix %d, cpuid %d, kstr_statsp 0x%x\n",
			i, pdaindr[i].CpuId, kstr_statsp);
	}
	return;
}

void
idbg_alcdat(char *s, alcdat *alcdatp)
{
	qprintf("%s: use %d, max %d, fail %d\n",
		s, alcdatp->use, alcdatp->max, alcdatp->fail);
	return;
}

/*
 *  kp strstats strstatp
 *  Print the cpu tcp/ip statistics given a strtstat structure address
 *  'strstatp'  associated with a cpu. The strstat structure holds the
 *  Streams statistics blocks for the various msg/dblk counts and buffers
 *  for a particular cpu.
 */
void
idbg_strstats(__psint_t addr)
{
	register struct strstat *kstr_statsp;

	if (addr == -1L) { /* no address specified so exit */
		qprintf(" no strstat structure address supplied\n");
		return;
	}
	kstr_statsp = (struct strstat *)addr;

	idbg_alcdat("Stream", &(kstr_statsp->stream));
	idbg_alcdat("Queue", &(kstr_statsp->queue));
	idbg_alcdat("Linkblk", &(kstr_statsp->linkblk));
	idbg_alcdat("Strevent", &(kstr_statsp->strevent));

	idbg_alcdat("Mblk", &(kstr_statsp->buffer[0]));
	idbg_alcdat("Msg/Dblk", &(kstr_statsp->buffer[1]));
	idbg_alcdat("Buf64", &(kstr_statsp->buffer[2]));
	idbg_alcdat("Buf256", &(kstr_statsp->buffer[3]));
	idbg_alcdat("Buf512", &(kstr_statsp->buffer[4]));
	idbg_alcdat("Buf2K", &(kstr_statsp->buffer[5]));
	idbg_alcdat("BufPage", &(kstr_statsp->buffer[6]));

	return;
}

/*
 * kp datab address
 * takes (struct datab *) and prints information about the
 * streams datab at <address>.
 */
void
idbg_strdatab(__psint_t n)
{
	dblk_t *db;
	unsigned char type;

	if (n == -1L) {
		qprintf("Usage:  kp datab <addr of a streams datab>\n");
		return;
	}

	db = (dblk_t *) n;
	qprintf("Streams datab 0x%x\n", db);

	qprintf("\tdb_base 0x%x, db_lim 0x%x\n", db->db_base, db->db_lim);
	type = db->db_type;

	qprintf("\tdb_type(%d) %s\n", db->db_type,
		type == M_DATA ? "M_DATA" :
		 type == M_PROTO ? "M_PROTO" :
		  type == M_BREAK ? "M_BREAK" :
		   type == M_PASSFP ? "M_PASSFP" :
		    type == M_EVENT ? "M_EVENT" :
		     type == M_SIG ? "M_SIG" :
		      type == M_DELAY ? "M_DELAY" :
		       type == M_CTL ? "M_CTL" :
		        type == M_IOCTL ? "M_IOCTL" :
		         type == M_SETOPTS ? "M_SETOPTS" :
		          type == M_RSE ? "M_RSE" :
		           type == M_IOCACK ? "M_IOCACK" :
		            type == M_IOCNAK ? "M_IOCNAK" :
		             type == M_PCPROTO ? "M_PCPROTO" :
		              type == M_PCSIG ? "M_PCSIG" :
		               type == M_READ ? "M_READ" :
		                type == M_FLUSH ? "M_FLUSH" :
		                 type == M_STOP ? "M_STOP" :
		                  type == M_START ? "M_START" :
		                   type == M_HANGUP ? "M_HANGUP" :
		                    type == M_ERROR ? "M_ERROR" :
		                     type == M_COPYIN ? "M_COPYIN" :
		                      type == M_COPYOUT ? "M_COPYOUT" :
		                       type == M_IOCDATA ? "M_IOCDATA" :
		                        type == M_PCRSE ? "M_PCRSE" :
		                         type == M_STOPI ? "M_STOPI" :
		                          type == M_STARTI ? "M_STARTI" :
		                           type == M_PCEVENT ? "M_PCEVENT" :
		"UNKNOWN");

	qprintf("\tdb_size %d, db_msgaddr 0x%x, db_ref %d\n",
		db->db_size, db->db_msgaddr, db->db_ref);

	qprintf("\tdb_cpuid %d, db_buf_cpuid %d, db_index %d\n",
		db->db_cpuid, db->db_buf_cpuid, db->db_index);

	qprintf("\tdb_f freep/frtnp 0x%x\n", db->db_f.freep);
	qprintf("\tdb_frtn: free_func 0x%x, free_arg 0x%x\n",
		db->db_frtn.free_func, db->db_frtn.free_arg);

#ifdef STREAMS_MEM_TRACE
	str_mem_trace_print(&(db->tracelog), "\t");
#endif /* STREAMS_MEM_TRACE */
}

/*
 * kp msgb address
 * takes (struct msgb *) and prints information about the
 * streams msgb at <address>.
 */
void
idbg_strmsgb(__psint_t n)
{
	mblk_t *mb;
	caddr_t page;
#ifdef DEBUG
	struct mbinfo *mbinfop;
#endif
	if (n == -1L) {
		qprintf("Usage:  kp msgb <addr of a streams msgb>\n");
		return;
	}
	mb = (mblk_t *) n;
	page = (caddr_t)((__psunsigned_t)mb & -NBPP);
	qprintf("Streams msgb 0x%x, page 0x%x\n", mb, page);
	qprintf("\tb_next 0x%x, b_prev 0x%x, b_cont 0x%x\n",
		mb->b_next, mb->b_prev, mb->b_cont);
	qprintf("\tb_rptr 0x%x, b_wptr 0x%x\n", mb->b_rptr, mb->b_wptr);
	qprintf("\tb_datap 0x%x\n", mb->b_datap);

	qprintf("\tb_cpuid %d, b_band %d, b_flag (0x%x)%s%s%s%s\n",
		mb->b_cpuid, mb->b_band, mb->b_flag,
		mb->b_flag & MSGMARK ? "MSGMARK " : "",
		mb->b_flag & MSGNOLOOP ? "MSGNOLOOP " : "",
		mb->b_flag & MSGDELIM ? "MSGDELIM " : "",
		mb->b_flag & MSGNOGET ? "MSGNOGET " : "");

	str_printflags((unsigned int)mb->b_mopsflag,
		    tab_b_mopsflag,
		    "\tb_mopsflag");
	qprintf("\n");
	qprintf("\tb_mbuf_refct %d\n", ((struct msgb *)mb)->b_mbuf_refct);

#ifdef STREAMS_MEM_TRACE
	str_mem_trace_print(&(mb->tracelog), "\t");
#endif /* STREAMS_MEM_TRACE */

#ifdef DEBUG
	mbinfop = (struct mbinfo *)mb;
	qprintf("\tmbinfo 0x%x\n", mbinfop);
	qprintf("\tm_next 0x%x, m_prev 0x%x\n",
		mbinfop->m_next, mbinfop->m_prev);
	qprintf("\talloc_func 0x%x, dupb_func 0x%x, freeb_func 0x%x\n",
		mbinfop->alloc_func, mbinfop->dupb_func, mbinfop->freeb_func);
	qprintf("\tputq_q 0x%x, putnext_q 0x%x\n",
		mbinfop->putq_q, mbinfop->putnext_q);
	qprintf("\tputnext_qi_putp 0x%x\n", mbinfop->putnext_qi_putp);
#endif /* DEBUG */
	return;
}

/*
 * kp msgbw address
 * takes (struct msgb *) list and prints each stream msgb at <address>
 */
void
idbg_strmsgbw(__psint_t n)
{
	mblk_t *mb;
	int i = 0;
	if (n == -1L) {
		qprintf("Usage:  kp msgb <addr of a streams msgb list>\n");
		return;
	}
	mb = (mblk_t *)n;
	while (mb) {
		idbg_strmsgb((__psint_t)mb);
		mb = mb->b_next;
		i++;
	}
	qprintf("msgbw: msgb count %d\n", i);
	return;
}

/*
 * kp msgbws address
 * takes (struct msgb *) list and prints summary info on streams
 * msgb at <address>
*/
void
idbg_strmsgbws(__psint_t n)
{
	mblk_t *mb;
	caddr_t page;
	int i = 0;
	if (n == -1L) {
		qprintf("Usage:  kp msgbws <addr of a streams msgb list>\n");
		return;
	}
	qprintf("msgbws summary\n");
	mb = (mblk_t *)n;
	while (mb) {
		/* compute base page address associated with node */
		page = (caddr_t)((__psunsigned_t)mb & -NBPP);
		qprintf(" msgb 0x%x, b_next 0x%x, page 0x%x\n",
			mb, mb->b_next, page);
		mb = mb->b_next;
		i++;
	}
	qprintf("msgbws: msgb count %d\n", i);
	return;
}

#ifdef DEBUG
/*
 * kp msgbwsi address
 * takes Strinfo msgb block list and prints summary info on each msg!
 * msgb list head at <address>
 * NOTE: This is DIFFERENT than the normal msgbw list procedure since
 * this is valid for DEBUG kernels only AND the mbinfo structure's
 * m_prev and m_next fields are used for the list walking!
 */
void
idbg_strmsgbwsi(__psint_t n)
{
	struct mbinfo *mb;
	int i = 0;

	if (n == -1L) {
	    qprintf("Usage:  kp msgbwsi <addr of head strinfo msgb list>\n");
		return;
	}

	mb = (struct mbinfo *)n;
	qprintf("msgbwsi summary\n");

	while (mb) {
		idbg_strmsgb((__psint_t)mb);
		idbg_strdatab((__psint_t)(mb->m_mblock.b_datap));

		mb = mb->m_next;
		i++;
	}
	qprintf("msgbwsi: msgb count %d\n", i);
	return;
}
#endif /* DEBUG */

#if defined(DEBUG) && defined(DEBUG_MBLK_TIMING) && defined(R10000)
/*
 * Commmon mbuf test code
 * Execute and print execution time of various combinations of
 * Stream allocb/freeb/dupb interfaces
 */
/* extern struct msgb *allocb(int, int); */
extern void freeb(struct msgb *);
extern void __cache_wb_inval(void *, int);
extern uint _get_count(void);

#define STR_BUFFER64    64
#define STR_BUFFER256   256
#define STR_BUFFER512   512
#define STR_BUFFER2K   2048
#define STR_BUFFERPAGE NBPP

#define IDBG_MBLK_MINCT 10
#define IDBG_MBLK_MAXCT 50

int msgb_cycle_int = 5555;		/* LEGO 180 MHz */
/* int msgb_cycle_int = 5130;*/		/* LEGO 195 MHz */
/* int msgb_cycle_int = 5555;*/		/* SPEEDO 180 MHz */

void
idbg_msg_tstc(int invalidate, int size, int mblk_loop_ct)
{
	struct msgb *m;
	int i, s;
	long getavg, getmin, getmed, getmax;
	long freeavg, freemin, freemed, freemax;
	long get, get_end, free, free_end;
	long get_times[IDBG_MBLK_MAXCT], free_times[IDBG_MBLK_MAXCT];

	if (invalidate == 0) { /* Warm cache case */
		/*
		 * allocb and freeb for designated buffer size in bytes
		 */
		freemin = getmin = 999999;
		freemax = getmax = getavg = freeavg = 0;

		for (i = 0; i < mblk_loop_ct; i++) {

			s = splhi();
			get = (long)(_get_count());
			m = allocb(size, BPRI_HI);
			get_end = (long)(_get_count());
			splx(s);

			get_times[i] = get = (get_end - get);
			if (get < getmin) {
				getmin = get;
			}
			if (get > getmax) {
				getmax = get;
			}
			getavg += get;

			if (m) { /* free message */
				s = splhi();
				free = (long)(_get_count());
				freeb(m);
				free_end = (long)(_get_count());
				splx(s);
				free = (free_end - free);
			} else {
				free = 0;
			}
			free_times[i] = free;
			if (free < freemin) {
				freemin = free;
			}
			if (free > freemax) {
				freemax = free;
			}
			freeavg += free;
		}
		getmed = (getavg - (getmin + getmax))/(mblk_loop_ct-2);
		getavg /= mblk_loop_ct;
		freemed = (freeavg - (freemin + freemax))/(mblk_loop_ct-2);
		freeavg /= mblk_loop_ct;

		qprintf("Warm Cache: Loop count %d\n", mblk_loop_ct);
		qprintf(" allocb %d bytes; min %d, med %d, max %d, avg %d\n",
			size,
			(getmin * msgb_cycle_int) / 1000,
			(getmed * msgb_cycle_int) / 1000,
			(getmax * msgb_cycle_int) / 1000,
			(getavg * msgb_cycle_int) / 1000);

		for (i = 0; i < mblk_loop_ct; i++) {
			get_times[i] = (get_times[i] * msgb_cycle_int)/1000;
		}
		for (i = 0; i < mblk_loop_ct; i+=10) {
		  qprintf("  allocb times: %d %d %d %d %d %d %d %d %d %d\n",
			get_times[i], get_times[i+1], get_times[i+2],
			get_times[i+3], get_times[i+4], get_times[i+5],
			get_times[i+6], get_times[i+7], get_times[i+8],
			get_times[i+9]);
		}

		qprintf(" freeb %d bytes; min %d, med %d, max %d, avg %d\n",
			size,
			(freemin * msgb_cycle_int) / 1000,
			(freemed * msgb_cycle_int) / 1000,
			(freemax * msgb_cycle_int) / 1000,
			(freeavg * msgb_cycle_int) / 1000);

		for (i = 0; i < mblk_loop_ct; i++) {
			free_times[i] = (free_times[i] * msgb_cycle_int)/1000;
		}
		for (i = 0; i < mblk_loop_ct; i+=10) {
		  qprintf("  freeb times: %d %d %d %d %d %d %d %d %d %d\n",
			free_times[i], free_times[i+1], free_times[i+2],
			free_times[i+3], free_times[i+4], free_times[i+5],
			free_times[i+6], free_times[i+7], free_times[i+8],
			free_times[i+9]);
		}
		qprintf("\n");

	} else {
		/*
		 * allocb and freeb for 'size' bytes
		 */
		freemin = getmin = 999999;
		freemax = getmax = getavg = freeavg = 0;

		for (i = 0; i < mblk_loop_ct; i++) {

			__cache_wb_inval((void *)K0BASE, 16*1024*1024);
			s = splhi();
			get = (long)(_get_count());
			m = allocb(size, BPRI_HI);
			get_end = (long)(_get_count());
			splx(s);
			get_times[i] = get = (get_end - get);

			if (get < getmin) {
				getmin = get;
			}
			if (get > getmax) {
				getmax = get;
			}
			getavg += get;

			if (m) { /* free message */
				__cache_wb_inval((void *)K0BASE, 16*1024*1024);
				s = splhi();
				free = (long)(_get_count());
				freeb(m);
				free_end = (long)(_get_count());
				splx(s);
				free = (free_end - free);
			} else {
				free = 0;
			}
			free_times[i] = free;

			if (free < freemin) {
				freemin = free;
			}
			if (free > freemax) {
				freemax = free;
			}
			freeavg += free;
		}
		getmed = (getavg - (getmin + getmax))/(mblk_loop_ct-2);
		getavg /= mblk_loop_ct;
		freemed = (freeavg - (freemin + freemax))/(mblk_loop_ct-2);
		freeavg /= mblk_loop_ct;

		qprintf("Cold Cache: Loop count %d\n", mblk_loop_ct);
		qprintf(" allocb %d bytes; min %d, med %d, max %d, avg %d\n",
			size,
			(getmin * msgb_cycle_int) / 1000,
			(getmed * msgb_cycle_int) / 1000,
			(getmax * msgb_cycle_int) / 1000,
			(getavg * msgb_cycle_int) / 1000);

		for (i = 0; i < mblk_loop_ct; i++) {
			get_times[i] = (long)
			 ((get_times[i] * (long)msgb_cycle_int)/(long)1000);
		}
		for (i = 0; i < mblk_loop_ct; i+=10) {
		  qprintf("  allocb times: %d %d %d %d %d %d %d %d %d %d\n",
			get_times[i], get_times[i+1], get_times[i+2],
			get_times[i+3], get_times[i+4], get_times[i+5],
			get_times[i+6], get_times[i+7], get_times[i+8],
			get_times[i+9]);
		}
		qprintf(" freeb %d bytes; min %d, med %d, max %d, avg %d\n",
			size,
			(freemin * msgb_cycle_int) / 1000,
			(freemed * msgb_cycle_int) / 1000,
			(freemax * msgb_cycle_int) / 1000,
			(freeavg * msgb_cycle_int) / 1000);
		for (i = 0; i < mblk_loop_ct; i++) {
			free_times[i] = (free_times[i] * msgb_cycle_int)/1000;
		}
		for (i = 0; i < mblk_loop_ct; i+=10) {
		  qprintf("  freeb times: %d %d %d %d %d %d %d %d %d %d\n",
			free_times[i], free_times[i+1], free_times[i+2],
			free_times[i+3], free_times[i+4], free_times[i+5],
			free_times[i+6], free_times[i+7], free_times[i+8],
			free_times[i+9]);
		}
		qprintf("\n");
	}
	return;
}

/*
 *  kp msgbt1 invalidateCache
 *  Execute and print execution time of mbuf interfaces
 */
void
idbg_msgbt1(__psint_t x)
{
	int invalidate = (x == 0) ? 0 : 1;

	idbg_msg_tstc(invalidate, STR_BUFFER64, IDBG_MBLK_MINCT);
	idbg_msg_tstc(invalidate, STR_BUFFER256, IDBG_MBLK_MINCT);
	idbg_msg_tstc(invalidate, STR_BUFFER512, IDBG_MBLK_MINCT);
	idbg_msg_tstc(invalidate, STR_BUFFER2K, IDBG_MBLK_MINCT);
	idbg_msg_tstc(invalidate, STR_BUFFERPAGE, IDBG_MBLK_MINCT);
	idbg_msg_tstc(invalidate, STR_BUFFERPAGE*2, IDBG_MBLK_MINCT);
	return;
}

/*
 *  kp msgbt2 invalidateCache
 *  Execute and print execution time of mbuf interfaces
 */
void
idbg_msgbt2(__psint_t x)
{
	int invalidate = (x == 0) ? 0 : 1;

	idbg_msg_tstc(invalidate, STR_BUFFER64, IDBG_MBLK_MAXCT);
	idbg_msg_tstc(invalidate, STR_BUFFER256, IDBG_MBLK_MAXCT);
	idbg_msg_tstc(invalidate, STR_BUFFER512, IDBG_MBLK_MAXCT);
	idbg_msg_tstc(invalidate, STR_BUFFER2K, IDBG_MBLK_MAXCT);
	idbg_msg_tstc(invalidate, STR_BUFFERPAGE, IDBG_MBLK_MAXCT);
	idbg_msg_tstc(invalidate, STR_BUFFERPAGE*2, IDBG_MBLK_MAXCT);
	return;
}

/*
 *  kp msgbt3
 *  Execute and print execution times of paired freeb(allocb()) calls
 *  NOTE: Only warm cache execution timed.
 */
/* ARGSUSED */
void
idbg_msgbt3(__psint_t x)
{
	struct msgb *m;
	int i, s;
	long getavg, getmin, getmax, get, get_end;
	long get_times[IDBG_MBLK_MINCT];
	long mblk_loop_ct = IDBG_MBLK_MINCT;

	getmin = 999999; getavg = getmax = 0;
	for (i = 0; i < mblk_loop_ct; i++) {

		s = splhi();
		get = (long)(_get_count());
		m = allocb(STR_BUFFER64, BPRI_HI);
		if (m) freeb(m);
		get_end = (long)(_get_count());
		splx(s);
		get_times[i] = get = (get_end - get);

		if (get < getmin) {
			getmin = get;
		}
		if (get > getmax) {
			getmax = get;
		}
		getavg += get;
	}
	getavg /= mblk_loop_ct;

	qprintf("Warm Cache: Loop count %d\n", mblk_loop_ct);

	qprintf(" freeb(allocb) Pair 64 bytes; min %d, max %d, avg %d\n",
		(getmin * msgb_cycle_int) / 1000,
		(getmax * msgb_cycle_int) / 1000,
		(getavg * msgb_cycle_int) / 1000);

	for (i = 0; i < mblk_loop_ct; i++) {
	 get_times[i] = (long)
	   ((get_times[i] * (long)msgb_cycle_int)/(long)1000);
	}
	for (i = 0; i < mblk_loop_ct; i+=10) {
	qprintf("  freeb(allocb) Pair times: %d %d %d %d %d %d %d %d %d %d\n",
		get_times[i], get_times[i+1], get_times[i+2],
		get_times[i+3], get_times[i+4], get_times[i+5],
		get_times[i+6], get_times[i+7], get_times[i+8],
		get_times[i+9]);
	}
	qprintf("\n");

	getmin = 999999; getavg = getmax = 0;
	for (i = 0; i < mblk_loop_ct; i++) {

		s = splhi();
		get = (long)(_get_count());
		m = allocb(STR_BUFFER512, BPRI_HI);
		if (m) freeb(m);
		get_end = (long)(_get_count());
		splx(s);
		get_times[i] = get = (get_end - get);

		if (get < getmin) {
			getmin = get;
		}
		if (get > getmax) {
			getmax = get;
		}
		getavg += get;
	}
	getavg /= mblk_loop_ct;

	qprintf(" freeb(allocb) Pair 512 bytes; min %d, max %d, avg %d\n",
		(getmin * msgb_cycle_int) / 1000,
		(getmax * msgb_cycle_int) / 1000,
		(getavg * msgb_cycle_int) / 1000);

	for (i = 0; i < mblk_loop_ct; i++) {
	 get_times[i] = (long)
	   ((get_times[i] * (long)msgb_cycle_int)/(long)1000);
	}
	for (i = 0; i < mblk_loop_ct; i+=10) {
	qprintf("  freeb(allocb) Pair times: %d %d %d %d %d %d %d %d %d %d\n",
		get_times[i], get_times[i+1], get_times[i+2],
		get_times[i+3], get_times[i+4], get_times[i+5],
		get_times[i+6], get_times[i+7], get_times[i+8],
		get_times[i+9]);
	}
	qprintf("\n");

	getmin = 999999; getavg = getmax = 0;
	for (i = 0; i < mblk_loop_ct; i++) {

		s = splhi();
		get = (long)(_get_count());
		m = allocb(STR_BUFFER2K, BPRI_HI);
		if (m) freeb(m);
		get_end = (long)(_get_count());
		splx(s);
		get_times[i] = get = (get_end - get);

		if (get < getmin) {
			getmin = get;
		}
		if (get > getmax) {
			getmax = get;
		}
		getavg += get;
	}
	getavg /= mblk_loop_ct;

	qprintf(" freeb(allocb) Pair 2048 bytes; min %d, max %d, avg %d\n",
		(getmin * msgb_cycle_int) / 1000,
		(getmax * msgb_cycle_int) / 1000,
		(getavg * msgb_cycle_int) / 1000);
	for (i = 0; i < mblk_loop_ct; i++) {
		get_times[i] = (long)
			((get_times[i] * (long)msgb_cycle_int)/(long)1000);
	}
	for (i = 0; i < mblk_loop_ct; i+=10) {
   qprintf("  freeb(allocb) Pair times: %d %d %d %d %d %d %d %d %d %d\n",
		get_times[i], get_times[i+1], get_times[i+2],
		get_times[i+3], get_times[i+4], get_times[i+5],
		get_times[i+6], get_times[i+7], get_times[i+8],
		get_times[i+9]);
	}
	qprintf("\n");

	getmin = 999999; getavg = getmax = 0;
	for (i = 0; i < mblk_loop_ct; i++) {

		s = splhi();
		get = (long)(_get_count());
		m = allocb(NBPP, BPRI_HI);
		if (m) freeb(m);
		get_end = (long)(_get_count());
		splx(s);
		get_times[i] = get = (get_end - get);

		if (get < getmin) {
			getmin = get;
		}
		if (get > getmax) {
			getmax = get;
		}
		getavg += get;
	}
	getavg /= mblk_loop_ct;

	qprintf(" freeb(allocb) Pair NBPP bytes; min %d, max %d, avg %d\n",
		(getmin * msgb_cycle_int) / 1000,
		(getmax * msgb_cycle_int) / 1000,
		(getavg * msgb_cycle_int) / 1000);
	for (i = 0; i < mblk_loop_ct; i++) {
		get_times[i] =
		  (long)((get_times[i] * (long)msgb_cycle_int)/(long)1000);
	}
	for (i = 0; i < mblk_loop_ct; i+=10) {
	qprintf("  freeb(allocb) Pair times: %d %d %d %d %d %d %d %d %d %d\n",
		get_times[i], get_times[i+1], get_times[i+2],
		get_times[i+3], get_times[i+4], get_times[i+5],
		get_times[i+6], get_times[i+7], get_times[i+8],
		get_times[i+9]);
	}
	return;
}

/*
 * Execute and print execution times of 10 consecutive allocb calls
 * followed by an equal number of freeb calls.
 * Both warm and cold cache execution options supported.
 */
void
idbg_msgbt4_cmn(int invalidate, int size, int mblk_loop_ct)
{
	struct msgb *msg_ptrs[IDBG_MBLK_MAXCT], *m;
	int i, s;
	long getavg, getmin, getmed, getmax, get, get_end;
	long get_times[IDBG_MBLK_MAXCT];
	long freeavg, freemin, freemed, freemax, free, free_end;
	long free_times[IDBG_MBLK_MAXCT], cmb_times[IDBG_MBLK_MAXCT];

	/*
	 * allocb and freeb for 'size' bytes
	 */
	getmin = 999999; getavg = getmax = 0;
	for (i = 0; i < mblk_loop_ct; i++) {

		if (invalidate) {
			__cache_wb_inval((void *)K0BASE, 16*1024*1024);
		}
		s = splhi();
		get = (long)(_get_count());
		m = allocb(size, BPRI_HI);
		get_end = (long)(_get_count());
		splx(s);
		msg_ptrs[i] = m;
		get_times[i] = get = (get_end - get);

		if (get < getmin) {
			getmin = get;
		}
		if (get > getmax) {
			getmax = get;
		}
		getavg += get;
	}
	getmed = (getavg - (getmin + getmax))/(mblk_loop_ct-2);
	getavg /= mblk_loop_ct;

	freemin = 999999; freeavg = freemax = 0;
	for (i = 0; i < mblk_loop_ct; i++) {

		if ((m = msg_ptrs[i]) == 0) {
			free_times[i] = 0;
			continue;
		}
		if (invalidate) {
			__cache_wb_inval((void *)K0BASE, 16*1024*1024);
		}
		s = splhi();
		free = (long)(_get_count());
		freeb(m);
		free_end = (long)(_get_count());
		splx(s);
		free_times[i] = free = (free_end - free);

		if (free < freemin) {
			freemin = free;
		}
		if (free > freemax) {
			freemax = free;
		}
		freeavg += free;
	}
	freemed = (freeavg - (freemin + freemax))/(mblk_loop_ct-2);
	freeavg /= mblk_loop_ct;

	if (invalidate) {
		qprintf("Cold Cache: Loop Ct %d\n", mblk_loop_ct);
	} else {
		qprintf("Warm Cache: Loop Ct %d\n", mblk_loop_ct);
	}

	qprintf(" allocb %d bytes; min %d, med %d, max %d, avg %d\n",
		size,
		(getmin * msgb_cycle_int) / 1000,
		(getmed * msgb_cycle_int) / 1000,
		(getmax * msgb_cycle_int) / 1000,
		(getavg * msgb_cycle_int) / 1000);
	for (i = 0; i < mblk_loop_ct; i++) {
		get_times[i] = (long)
			((get_times[i] * (long)msgb_cycle_int)/(long)1000);
	}
	for (i = 0; i < mblk_loop_ct; i+=10) {
	qprintf("  allocb times: %d %d %d %d %d %d %d %d %d %d\n",
		get_times[i], get_times[i+1], get_times[i+2],
		get_times[i+3], get_times[i+4], get_times[i+5],
		get_times[i+6], get_times[i+7], get_times[i+8],
		get_times[i+9]);
	}
	qprintf(" freeb %d bytes; min %d, med %d, max %d, avg %d\n",
		size,
		(freemin * msgb_cycle_int) / 1000,
		(freemed * msgb_cycle_int) / 1000,
		(freemax * msgb_cycle_int) / 1000,
		(freeavg * msgb_cycle_int) / 1000);
	for (i = 0; i < mblk_loop_ct; i++) {
		free_times[i] = (long)
			((free_times[i] * (long)msgb_cycle_int)/(long)1000);
	}
	for (i = 0; i < mblk_loop_ct; i+=10) {
	qprintf("  freeb times: %d %d %d %d %d %d %d %d %d %d\n",
		free_times[i], free_times[i+1], free_times[i+2],
		free_times[i+3], free_times[i+4], free_times[i+5],
		free_times[i+6], free_times[i+7], free_times[i+8],
		free_times[i+9]);
	}
	for (i = 0; i < mblk_loop_ct; i++) {
		cmb_times[i] = (get_times[i]+free_times[i]);
	}
	for (i = 0; i < mblk_loop_ct; i+=10) {
	qprintf(" allocb+freeb times: %d %d %d %d %d %d %d %d %d %d\n",
		cmb_times[i], cmb_times[i+1], cmb_times[i+2],
		cmb_times[i+3], cmb_times[i+4], cmb_times[i+5],
		cmb_times[i+6], cmb_times[i+7], cmb_times[i+8],
		cmb_times[i+9]);
	}
	qprintf("\n");
	return;
}

/*
 * kp msgbt4 cache
 * Execute and print execution times of 'IDBG_MBLK_MINCT' consecutive allocb
 * calls followed by an equal number of freeb calls for each buffer size.
 * NOTE: Which cache execution timing is dependent on the parameter, 'x';
 * zero => Warm cache execution timed; 1 => Cold cache execution timed.
 */
void
idbg_msgbt4(__psint_t x)
{
	int invalidate = (x == 0) ? 0 : 1;

	idbg_msgbt4_cmn(invalidate, STR_BUFFER64, IDBG_MBLK_MINCT);
	idbg_msgbt4_cmn(invalidate, STR_BUFFER256, IDBG_MBLK_MINCT);
	idbg_msgbt4_cmn(invalidate, STR_BUFFER512, IDBG_MBLK_MINCT);
	idbg_msgbt4_cmn(invalidate, STR_BUFFER2K, IDBG_MBLK_MINCT);
	idbg_msgbt4_cmn(invalidate, STR_BUFFERPAGE, IDBG_MBLK_MINCT);
	return;
}

/*
 * kp msgbt5 cache
 * Execute and print execution times of 'IDBG_MBLK_MAXCT' consecutive allocb
 * calls followed by an equal number of freeb calls for each buffer size.
 * NOTE: Which cache execution timing is dependent on the parameter, 'x';
 * zero => Warm cache execution timed; 1 => Cold cache execution timed.
 */
void
idbg_msgbt5(__psint_t x)
{
	int invalidate = (x == 0) ? 0 : 1;

	idbg_msgbt4_cmn(invalidate, STR_BUFFER64, IDBG_MBLK_MAXCT);
	idbg_msgbt4_cmn(invalidate, STR_BUFFER256, IDBG_MBLK_MAXCT);
	idbg_msgbt4_cmn(invalidate, STR_BUFFER512, IDBG_MBLK_MAXCT);
	idbg_msgbt4_cmn(invalidate, STR_BUFFER2K, IDBG_MBLK_MAXCT);
	idbg_msgbt4_cmn(invalidate, STR_BUFFERPAGE, IDBG_MBLK_MAXCT);
	return;
}

/*
 * Execute and print execution times of allocb(), and dupb() calls
 * followed by an twice as many freeb() calls to return all the storage.
 * Both warm and cold cache execution options supported.
 */
void
idbg_msgbt6_cmn(int invalidate, int size, int mblk_loop_ct)
{
	struct msgb *msg_ptrs[IDBG_MBLK_MINCT], *msg_dupptrs[IDBG_MBLK_MINCT];
	struct msgb *m, *m_dup;
	int i, s;
	long getavg, getmin, getmed, getmax, get, get_end;
	long get_times[IDBG_MBLK_MINCT];

	long freeavg, freemin, freemed, freemax, free, free_end;
	long free_times[IDBG_MBLK_MINCT], cmb_times[IDBG_MBLK_MINCT];

	long dupb_times[IDBG_MBLK_MINCT];
	long dupbavg, dupbmin, dupbmed, dupbmax, dup, dup_end;

	/*
	 * allocb for 'size' bytes
	 */
	if (mblk_loop_ct > IDBG_MBLK_MINCT) mblk_loop_ct = IDBG_MBLK_MINCT;

	getmin = 999999; getavg = getmax = 0;
	for (i = 0; i < mblk_loop_ct; i++) {

		if (invalidate) {
			__cache_wb_inval((void *)K0BASE, 16*1024*1024);
		}
		s = splhi();
		get = (long)(_get_count());
		m = allocb(size, BPRI_HI);
		get_end = (long)(_get_count());
		splx(s);
		msg_ptrs[i] = m;
		get_times[i] = get = (get_end - get);

		if (get < getmin) {
			getmin = get;
		}
		if (get > getmax) {
			getmax = get;
		}
		getavg += get;
	}
	getmed = (getavg - (getmin + getmax))/(mblk_loop_ct-2);
	getavg /= mblk_loop_ct;

	/*
	 * dupb for 'size' bytes
	 */
	dupbmin = 999999; dupbavg = dupbmax = 0;
	for (i = 0; i < mblk_loop_ct; i++) {

		if (invalidate) {
			__cache_wb_inval((void *)K0BASE, 16*1024*1024);
		}
		if ((m = msg_ptrs[i])) {
			s = splhi();
			dup = (long)(_get_count());
			m_dup = dupb(m);
			dup_end = (long)(_get_count());
			splx(s);
			msg_dupptrs[i] = m_dup;
			dup = (dup_end - dup);
		} else {
			msg_dupptrs[i] = 0;
			dup = 0;
		}
		dupb_times[i] = dup;
		if (dup < dupbmin) {
			dupbmin = dup;
		}
		if (dup > dupbmax) {
			dupbmax = dup;
		}
		dupbavg += dup;
	}
	dupbmed = (dupbavg - (dupbmin + dupbmax))/(mblk_loop_ct-2);
	dupbavg /= mblk_loop_ct;

	/*
	 * freeb() the dupb() msg pointers
	 */
	for (i = 0; i < mblk_loop_ct; i++) {
		if ((m_dup = msg_dupptrs[i])) {
			freeb(m_dup);
		}
	}

	/* Time the last freeb() which will release the storage */
	freemin = 999999; freeavg = freemax = 0;
	for (i = 0; i < mblk_loop_ct; i++) {

		if ((m = msg_ptrs[i]) == 0) {
			free_times[i] = 0;
			continue;
		}
		if (invalidate) {
			__cache_wb_inval((void *)K0BASE, 16*1024*1024);
		}
		s = splhi();
		free = (long)(_get_count());
		freeb(m);
		free_end = (long)(_get_count());
		splx(s);
		free_times[i] = free = (free_end - free);

		if (free < freemin) {
			freemin = free;
		}
		if (free > freemax) {
			freemax = free;
		}
		freeavg += free;
	}
	freemed = (freeavg - (freemin + freemax))/(mblk_loop_ct-2);
	freeavg /= mblk_loop_ct;

	if (invalidate) {
		qprintf("Cold Cache: Loop Ct %d\n", mblk_loop_ct);
	} else {
		qprintf("Warm Cache: Loop Ct %d\n", mblk_loop_ct);
	}

	qprintf(" allocb %d bytes; min %d, med %d, max %d, avg %d\n",
		size,
		(getmin * msgb_cycle_int) / 1000,
		(getmed * msgb_cycle_int) / 1000,
		(getmax * msgb_cycle_int) / 1000,
		(getavg * msgb_cycle_int) / 1000);
	for (i = 0; i < mblk_loop_ct; i++) {
		get_times[i] = (long)
			((get_times[i] * (long)msgb_cycle_int)/(long)1000);
	}
	for (i = 0; i < mblk_loop_ct; i+=10) {
	  qprintf("  allocb times: %d %d %d %d %d %d %d %d %d %d\n",
		get_times[i], get_times[i+1], get_times[i+2],
		get_times[i+3], get_times[i+4], get_times[i+5],
		get_times[i+6], get_times[i+7], get_times[i+8],
		get_times[i+9]);
	}

	qprintf(" dupb %d bytes; min %d, med %d, max %d, avg %d\n",
		size,
		(dupbmin * msgb_cycle_int) / 1000,
		(dupbmed * msgb_cycle_int) / 1000,
		(dupbmax * msgb_cycle_int) / 1000,
		(dupbavg * msgb_cycle_int) / 1000);

	for (i = 0; i < mblk_loop_ct; i++) {
		dupb_times[i] = (long)
			((dupb_times[i] * (long)msgb_cycle_int)/(long)1000);
	}
	for (i = 0; i < mblk_loop_ct; i+=10) {
	  qprintf("  dupb times: %d %d %d %d %d %d %d %d %d %d\n",
		dupb_times[i], dupb_times[i+1], dupb_times[i+2],
		dupb_times[i+3], dupb_times[i+4], dupb_times[i+5],
		dupb_times[i+6], dupb_times[i+7], dupb_times[i+8],
		dupb_times[i+9]);
	}

	qprintf(" freeb %d bytes; min %d, med %d, max %d, avg %d\n",
		size,
		(freemin * msgb_cycle_int) / 1000,
		(freemed * msgb_cycle_int) / 1000,
		(freemax * msgb_cycle_int) / 1000,
		(freeavg * msgb_cycle_int) / 1000);
	for (i = 0; i < mblk_loop_ct; i++) {
		free_times[i] = (long)
			((free_times[i] * (long)msgb_cycle_int)/(long)1000);
	}
	for (i = 0; i < mblk_loop_ct; i+=10) {
	  qprintf("  freeb times: %d %d %d %d %d %d %d %d %d %d\n",
		free_times[i], free_times[i+1], free_times[i+2],
		free_times[i+3], free_times[i+4], free_times[i+5],
		free_times[i+6], free_times[i+7], free_times[i+8],
		free_times[i+9]);
	}
	for (i = 0; i < mblk_loop_ct; i++) {
		cmb_times[i] = (get_times[i]+free_times[i]);
	}
	for (i = 0; i < mblk_loop_ct; i+=10) {
	  qprintf(" allocb+freeb times: %d %d %d %d %d %d %d %d %d %d\n",
		cmb_times[i], cmb_times[i+1], cmb_times[i+2],
		cmb_times[i+3], cmb_times[i+4], cmb_times[i+5],
		cmb_times[i+6], cmb_times[i+7], cmb_times[i+8],
		cmb_times[i+9]);
	}
	qprintf("\n");
	return;
}

/*
 * kp msgbt6 cache
 * Execute and print execution times of dupb() calls
 * followed by an equal number of freeb calls.
 * NOTE: Which cache execution timing is dependent on the parameter, 'x';
 * zero => Warm cache execution timed; 1 => Cold cache execution timed.
 */
void
idbg_msgbt6(__psint_t x)
{
	int invalidate = (x == 0) ? 0 : 1;

	idbg_msgbt6_cmn(invalidate, STR_BUFFER64, IDBG_MBLK_MINCT);
	idbg_msgbt6_cmn(invalidate, STR_BUFFER256, IDBG_MBLK_MINCT);
	idbg_msgbt6_cmn(invalidate, STR_BUFFER512, IDBG_MBLK_MINCT);
	idbg_msgbt6_cmn(invalidate, STR_BUFFER2K, IDBG_MBLK_MINCT);
	idbg_msgbt6_cmn(invalidate, STR_BUFFERPAGE, IDBG_MBLK_MINCT);
	return;
}

/*
 * kp mbuftlk
 * Execute and print execution times of 10 pairs of
 * mutex_spinlock/mutex_spinunlock calls.
 * NOTE: Both warm(x=0) and cold(x=1) cache execution times supported.
 */
void
idbg_msgbtlk(__psint_t x)
{
	int i, s, s_lfvec;
	lock_t lock;
	long getavg, getmin, getmax, get, get_end;
	long get_times[IDBG_MBLK_MINCT];
	long freeavg, freemin, freemax, free, free_end;
	long free_times[IDBG_MBLK_MINCT];
	int invalidate = (x == 0) ? 0 : 1;
	long mblk_loop_ct = IDBG_MBLK_MINCT;
	spinlock_init(&lock, 0);

	getmin = 999999; getavg = getmax = 0;
	freemin = 999999; freeavg = freemax = 0;

	for (i = 0; i < mblk_loop_ct; i++) {

		if (invalidate) {
			__cache_wb_inval((void *)K0BASE, 16*1024*1024);
		}
		s = splhi();
		get = (long)(_get_count());
		s_lfvec = mutex_spinlock(&lock);
		get_end = (long)(_get_count());
		splx(s);
		get_times[i] = get = (get_end - get);
		if (get < getmin) {
			getmin = get;
		}
		if (get > getmax) {
			getmax = get;
		}
		getavg += get;

		if (invalidate) {
			__cache_wb_inval((void *)K0BASE, 16*1024*1024);
		}
		s = splhi();
		free = (long)(_get_count());
		mutex_spinunlock(&lock, s_lfvec);
		free_end = (long)(_get_count());
		splx(s);
		free_times[i] = free = (free_end - free);
		if (free < freemin) {
			freemin = free;
		}
		if (free > freemax) {
			freemax = free;
		}
		freeavg += free;
	}
	getavg /= mblk_loop_ct;
	freeavg /= mblk_loop_ct;

	if (invalidate) {
		qprintf(" mutex_spinlock Cold cache; min %d, max %d, avg %d\n",
			(getmin * msgb_cycle_int) / 1000,
			(getmax * msgb_cycle_int) / 1000,
			(getavg * msgb_cycle_int) / 1000);
	} else {
		qprintf(" mutex_spinlock Warm cache; min %d, max %d, avg %d\n",
			(getmin * msgb_cycle_int) / 1000,
			(getmax * msgb_cycle_int) / 1000,
			(getavg * msgb_cycle_int) / 1000);
	}
	for (i = 0; i < mblk_loop_ct; i++) {
		get_times[i] = (long)
			((get_times[i] * (long)msgb_cycle_int)/(long)1000);
	}
	for (i = 0; i < mblk_loop_ct; i+=10) {
	qprintf("  mutex_spinlock: %d %d %d %d %d %d %d %d %d %d\n",
		get_times[i], get_times[i+1], get_times[i+2],
		get_times[i+3], get_times[i+4], get_times[i+5],
		get_times[i+6], get_times[i+7], get_times[i+8],
		get_times[i+9]);
	}

	qprintf(" mutex_spinunlock: min %d, max %d, avg %d\n",
		(freemin * msgb_cycle_int) / 1000,
		(freemax * msgb_cycle_int) / 1000,
		(freeavg * msgb_cycle_int) / 1000);
	for (i = 0; i < mblk_loop_ct; i++) {
		free_times[i] = (long)
			((free_times[i] * (long)msgb_cycle_int)/(long)1000);
	}
	for (i = 0; i < mblk_loop_ct; i+=10) {
	qprintf("  mutex_spinunlock times: %d %d %d %d %d %d %d %d %d %d\n",
		free_times[i], free_times[i+1], free_times[i+2],
		free_times[i+3], free_times[i+4], free_times[i+5],
		free_times[i+6], free_times[i+7], free_times[i+8],
		free_times[i+9]);
	}
}
#endif /* DEBUG_MBLK_TIMING */

/*
 * kp qband address
 * takes (struct qband *) and prints information about the
 * streams qband at <address>.
 */
void
idbg_strqband(__psint_t n)
{
	qband_t *qb;

	if (n == -1L) {
		qprintf("Usage:  kp msgb <addr of a streams msgb>\n");
		return;
	}

	qb = (qband_t *) n;

	qprintf("Streams qband 0x%x\n", qb);
	qprintf("\tnext 0x%x count %d first 0x%x last 0x%x\n",
		qb->qb_next, qb->qb_count, qb->qb_first, qb->qb_last);
	qprintf("\thiwat %d lowat %d flag %s%s%s\n",
		qb->qb_hiwat, qb->qb_lowat,
		qb->qb_flag & QB_FULL ? "QB_FULL " : "",
		qb->qb_flag & QB_WANTW ? "QB_WANTW " : "",
		qb->qb_flag & QB_BACK ? "QB_BACK " : "");
	return;
}

void
idbg_strqinfo(__psint_t n)
{
	struct qinit *qip;

	qip = (struct qinit *)n;
	qprintf("Queue info (qinit) 0x%x:\n", qip);
	qprintf("\tputp 0x%x srvp 0x%x open 0x%x close 0x%x\n",
		qip->qi_putp,
		qip->qi_srvp,
		qip->qi_qopen,
		qip->qi_qclose);
	qprintf("\tmodinfo 0x%x mstat 0x%x\n",
		qip->qi_minfo,
		qip->qi_mstat);
	return;
}

void
idbg_strmodinfo(__psint_t n)
{
	struct module_info *mip;

	mip = (struct module_info *)n;

	qprintf("Module info 0x%x (%s):\n", mip, mip->mi_idname);
	qprintf("\tidnum 0x%x minpsz 0x%x maxpsz 0x%x\n",
		mip->mi_idnum,
		mip->mi_minpsz,
		mip->mi_maxpsz);
	qprintf("\thiwat 0x%x lowat 0x%x locking 0x%x\n",
		mip->mi_hiwat,
		mip->mi_lowat,
		mip->mi_locking);
	return;
}

/*
 * kp strq address
 * takes (struct queue *) and prints information about the
 * streams queue at <address>.
 */
void
idbg_strqueue(__psint_t n)
{
	queue_t *q;

	if (n == -1L) {
		qprintf("Usage:  kp strq <addr of a streams queue>\n");
		return;
	}

	q = (queue_t *) n;

	qprintf("Streams queue 0x%x (type %s, otherq 0x%x):\n",
		q, (q->q_flag & QREADR) ? "READ" : "WRITE", OTHERQ(q));

	qprintf("\tqinfo 0x%x (modinfo 0x%x name %s) monpp 0x%x\n",
		q->q_qinfo, q->q_qinfo->qi_minfo,
		q->q_qinfo->qi_minfo->mi_idname,
		q->q_monpp);
	qprintf("\tfirst 0x%x last 0x%x\n",
		q->q_first, q->q_last);
	qprintf("\tnext 0x%x otherq next 0x%x link 0x%x ptr 0x%x\n",
		q->q_next, OTHERQ(q)->q_next, q->q_link, q->q_ptr);
	qprintf("\tcount %d # msgs queued %d total data bytes %d\n",
		q->q_count, idbg_count_msgs(q), idbg_count_bytes(q));
	qprintf("\tflag: 0x%x %s%s%s%s%s%s%s%s%s\n",
		q->q_flag,
		q->q_flag & QENAB ? "QENAB " : "",
		q->q_flag & QWANTR ? "QWANTR " : "",
		q->q_flag & QWANTW ? "QWANTW " : "",
		q->q_flag & QFULL ? "QFULL " : "",
		q->q_flag & QREADR ? "QREADR " : "",
		q->q_flag & QUSE ? "QUSE " : "",
		q->q_flag & QNOENB ? "QNOENB " : "",
		q->q_flag & QBACK ? "QBACK " : "",
		q->q_flag & QHLIST ? "QHLIST " : "");
	qprintf("\tpflag: 0x%x %s%s%s%s%s%s%s%s%s\n",
		q->q_pflag,
		q->q_pflag & QENAB ? "QENAB " : "",
		q->q_pflag & QWANTR ? "QWANTR " : "",
		q->q_pflag & QWANTW ? "QWANTW " : "",
		q->q_pflag & QFULL ? "QFULL " : "",
		q->q_pflag & QREADR ? "QREADR " : "",
		q->q_pflag & QUSE ? "QUSE " : "",
		q->q_pflag & QNOENB ? "QNOENB " : "",
		q->q_pflag & QBACK ? "QBACK " : "",
		q->q_pflag & QHLIST ? "QHLIST " : "");
	qprintf("\tminpsz 0x%x maxpsz 0x%x hiwat %d lowat %d\n",
		q->q_minpsz, q->q_maxpsz, q->q_hiwat, q->q_lowat);
	qprintf("\tbandp 0x%x nband %d blocked %d\n",
		q->q_bandp, q->q_nband, q->q_blocked);
	return;
}

/*
 * kp strqueinfo address
 * takes (struct queinfo *) and prints information about the
 * both the streams queues and links at <address>
 */
void
idbg_strqueinfo(__psint_t n)
{
	struct queinfo *que;

	if (n == -1L) {
	qprintf("Usage:  kp strqinfo <addr of Streams struct queinfo>\n");
		return;
	}
	que = (struct queinfo *)n;

	qprintf("Streams queinfo 0x%x\n", que);
	qprintf("\tqu_rqueue 0x%x, qu_wqueue 0x%x\n",
		&(que->qu_rqueue), &(que->qu_wqueue));
	qprintf("\tqu_next 0x%x, qu_prev 0x%x\n",
		que->qu_next, que->qu_prev);
	return;
}

/*
 * kp strh address
 * takes (struct stdata *) and prints information about the
 * stream head at <address>.
 */
void
idbg_strhead(__psint_t n)
{
	struct stdata *sp;
	struct shinfo *shinfop;
	int i, bit, first;

	if (n == -1L) {
		qprintf("Usage:  kp strh <addr of a stream head>\n");
		return;
	}

	sp = (struct stdata *)n;
	shinfop = (struct shinfo *)n;

	qprintf("stream head 0x%x, WRQ 0x%x, RDQ 0x%x\n",
		sp, sp->sd_wrq, OTHERQ(sp->sd_wrq));
	qprintf("\tshinfo 0x%x, sh_next 0x%x, sh_prev 0x%x\n",
		shinfop, shinfop->sh_next, shinfop->sh_prev);
#ifdef DEBUG
	qprintf("\tsh_alloc_func 0x%x, sh_freeb_func 0x%x\n",
		shinfop->sh_alloc_func, shinfop->sh_freeb_func);
#endif /* DEBUG */
	qprintf("\tioctl msgb 0x%x, vnode 0x%x, streamtab 0x%x\n",
		sp->sd_iocblk, sp->sd_vnode, sp->sd_strtab);
	qprintf("\tmonp 0x%x, sd_assoc_prev 0x%x, sd_assoc_next 0x%x\n",
		sp->sd_monp, sp->sd_assoc_prev, sp->sd_assoc_next);
	qprintf("\tflag(0x%x): ", sp->sd_flag);
	bit = 1;
	first = 1;
	for (i = 0; i < 8 * sizeof sp->sd_flag; i++, bit <<= 1) {
		if (bit & sp->sd_flag) {
			if (first)
				first = 0;
			else
				qprintf("|");

			if (i>= sizeof strh_flag_name/sizeof strh_flag_name[0])
				qprintf("unknown");
			else
				qprintf("%s", strh_flag_name[i]);
		}
	}
	qprintf("\n");
	qprintf("\tpflag(0x%x): ", sp->sd_pflag);
	bit = 1;
	first = 1;
	for (i = 0; i < 8 * sizeof sp->sd_pflag; i++, bit <<= 1) {
		if (bit & sp->sd_pflag) {
			if (first)
				first = 0;
			else
				qprintf("|");

			if(i >= sizeof strh_flag_name/sizeof strh_flag_name[0])
				qprintf("unknown");
			else
				qprintf("%s", strh_flag_name[i]);
		}
	}
	qprintf("\n");

#ifdef _SVR4_SESSIONS
	qprintf("\tpgrp %d sid %d",
		sp->sd_pgidp->pid_id, sp->sd_sidp->pid_id);
#else
	qprintf("\tpgrp %d sid %d",
		sp->sd_pgid, sp->sd_vsession ? sp->sd_vsession->vs_sid : 0);
#endif /* _SVR4_SESSIONS */

	qprintf("\tiocid %d iocwait %d wroff %d rerror %d werror %d\n",
		sp->sd_iocid, sp->sd_iocwait, sp->sd_wroff,
		sp->sd_rerror, sp->sd_werror);
	qprintf("\tpushcnt %d\n",
		sp->sd_pushcnt);

	qprintf("\tsigflags(0x%x):  ", sp->sd_sigflags);
	bit = 1;
	first = 1;
	for (i = 0; i < 8 * sizeof sp->sd_sigflags; i++, bit <<= 1) {
		if (bit & sp->sd_sigflags) {
			if (first)
				first = 0;
			else
				qprintf("|");
			if (i >= sizeof strh_sigflags/sizeof strh_sigflags[0])
				qprintf("unknown");
			else
				qprintf("%s", strh_sigflags[i]);
		}
	}
	qprintf("\n");

	qprintf(
	      "\tsiglist 0x%x pollist 0x%x mark 0x%x closetime %d, rtime %d\n",
		sp->sd_siglist, &sp->sd_pollist, sp->sd_mark,
		sp->sd_closetime, sp->sd_rtime);
	return;
}

/*
 *
 * kp strinfo
 * Prints the contents of the Strinfo structure which holds pointers to
 * the heads of lists for various types of stream data structures.
*/
void
idbg_strinfo(void)
{
	qprintf("Strinfo[DYN_STREAM] head 0x%x, sd_cnt %d\n",
		Strinfo[DYN_STREAM].sd_head, Strinfo[DYN_STREAM].sd_cnt);

	qprintf("Strinfo[DYN_QUEUE] head 0x%x, sd_cnt %d\n",
		Strinfo[DYN_QUEUE].sd_head, Strinfo[DYN_QUEUE].sd_cnt);

	qprintf("Strinfo[DYN_MSGBLOCK] head 0x%x, sd_cnt %d\n",
		Strinfo[DYN_MSGBLOCK].sd_head, Strinfo[DYN_MSGBLOCK].sd_cnt);

	qprintf("Strinfo[DYN_LINKBLK] head 0x%x, sd_cnt %d\n",
		Strinfo[DYN_LINKBLK].sd_head, Strinfo[DYN_LINKBLK].sd_cnt);

	qprintf("Strinfo[DYN_STREVENT] head 0x%x, sd_cnt %d\n",
		Strinfo[DYN_STREVENT].sd_head, Strinfo[DYN_STREVENT].sd_cnt);

	qprintf("Strinfo[DYN_QBAND] head 0x%x, sd_cnt %d\n",
		Strinfo[DYN_QBAND].sd_head, Strinfo[DYN_QBAND].sd_cnt);
	return;
}

void
printqlist(struct queue *q)
{
	int i;
	struct queue *qp;

	i = 0;
	qp = q;
	while (qp) {
		qprintf("[%d] 0x%x\n", i, qp);
		qp = qp->q_link;
		i++;
	}
	return;
}

/*
 * strqhead - prints the qhead list
 */
void
idbg_strqhead()
{
	if (qhead) 
		printqlist(qhead);
	else
		qprintf("Enabled list empty\n");
	return;
}

/*
 * strbcall - prints the bufcall list
 */
void
idbg_strbcall()
{
	int i;
	struct strevent *sep;

	qprintf("strbcflag 0x%x strbcwait 0x%x\n", strbcflag, strbcwait);
	if (!(sep = strbcalls.bc_head)) {
		qprintf("Bufcall list empty\n");
		return;
	}
	qprintf("bc_head 0x%x bc_tail 0x%x\n",
		strbcalls.bc_head, strbcalls.bc_tail);
	i = 1;
	do {
		qprintf(
                   "[%d] se_monpp 0x%x se_func 0x%x se_arg 0x%x\n",
			i++, sep->se_monpp, sep->se_func, sep->se_arg);
	} while (sep = sep->se_next);

	return;
}

void
printintrb(struct strintr *sp)
{
	qprintf("\tnext 0x%x prev 0x%x gen %d id %d\n",
		sp->next, sp->prev, sp->gen, sp->id);
	qprintf("\tfunc 0x%x arg1 0x%x arg2 0x%x arg3 0x%x\n",
	        sp->func, sp->arg1, sp->arg2, sp->arg3);
	return;
}

void
printintrlist(struct strintr *sp)
{
	int i;

	for (i = 0; sp != NULL; sp = sp->next, i++) {
		qprintf("block %03d 0x%x:\n", i, sp);
		printintrb(sp);
		sp = sp->next;
	}
	return;
}

/*
 * strintrb address
 * prints information about a streams interrupt block.
*/
void
idbg_strintrb(__psint_t n)
{
	struct strintr *sp;
	
	if (n == -1L) {
		qprintf("Usage:  kp strintrb <addr of a struct strintr>\n");
		return;
	}

	sp = (struct strintr *) n;

	qprintf("streams interrupt block 0x%x:\n", sp);
	printintrb(sp);
	return;
}

/*
 * strintrlist address
 * prints the members of a list of streams interrupt blocks.
 */
void
idbg_strintrlist(__psint_t n)
{
	struct strintr *sp;
	
	if (n == -1L) {
		qprintf("Usage:  kp strintrlist <addr of a struct strintr>\n");
		return;
	}

	sp = (struct strintr *) n;
	qprintf("streams interrupt block list starting at 0x%x:\n", sp);
	printintrlist(sp);
}

/*
 * strintr
 * prints information about the streams interrupt block pool.
 */
void
idbg_strintr()
{
	qprintf("streams interrupt block pool:\n");
	qprintf("\tlock 0x%x\n", str_intr_lock);
	qprintf("\tstrintrfree 0x%x strintrrsrv 0x%x\n",
		strintrfree, strintrrsrv);
	qprintf("\tnstrintr %d avail %d lowat %d\n",
		nstrintr, streams_intr_avail, streams_intr_lowat);
#ifdef DEBUG
	qprintf("\tinuse %d inuse_max %d rsrvd %d rsrvd_max %d\n",
		streams_intr_inuse, streams_intr_inuse_max,
		streams_intr_rsrvd, streams_intr_rsrvd_max);
#endif /* DEBUG */
	return;
}

void
idbg_strmon(__psint_t n)
{
	mon_t *mp;
	struct streams_mon_data *smd;

	mp = (mon_t *)n;

	qprintf(
	  "\tlock 0x%x lock_flags 0x%x wait 0x%x monpp 0x%x monp_lock 0x%x\n",
		&mp->mon_lock, mp->mon_lock_flags, &mp->mon_wait,
		mp->mon_monpp, mp->mon_monp_lock);
	qprintf("\tthreadid %lld trips %d p_arg 0x%x\n",
		mp->mon_id, mp->mon_trips, mp->mon_p_arg);
	qprintf("\tprev 0x%x next 0x%x\n",
		mp->mon_prev, mp->mon_next);
	qprintf("\tqueue 0x%x:\n", mp->mon_queue);
	printintrlist((struct strintr *)mp->mon_queue);
	qprintf("\tfuncp 0x%x\n", mp->mon_funcp);
	if (mp->mon_funcp) {
		qprintf("\t\tinit 0x%x serv 0x%x p_mon 0x%x v_mon 0x%x\n",
			mp->mon_funcp->mf_init,
			mp->mon_funcp->mf_service,
			mp->mon_funcp->mf_p_mon,
			mp->mon_funcp->mf_v_mon);
		qprintf("\t\tq_mon_sav 0x%x q_mon_rst 0x%x\n",
			mp->mon_funcp->mf_q_mon_sav,
			mp->mon_funcp->mf_q_mon_rst);
		qprintf("\t\tr_mon 0x%x a_mon 0x%x\n",
			mp->mon_funcp->mf_r_mon,
			mp->mon_funcp->mf_a_mon);
	}
	qprintf("\tprivate 0x%x\n", mp->mon_private);
	if (smd = mp->mon_private) {
		qprintf("\t\tflags 0x%x joining 0x%x assoc_cnt 0x%x\n",
			smd->smd_flags,
			smd->smd_joining,
			smd->smd_assoc_cnt);
		qprintf("\t\theadp 0x%x tailp 0x%x\n",
			smd->smd_headp,
			smd->smd_tailp);
	}
}

/*
 * strlocks - prints information about various streams-related locks.
*/
void
idbg_strlocks()
{
	/* REFERENCED */
	register int i;
	mon_t *mp = &streams_mon;
	qprintf("global streams monitor: 0x%x\n", mp);
	qprintf("private streams monitor list head: 0x%x\n", privmon_list);
	qprintf("private streams monitor count: %d\n", privmon_cnt);
	qprintf("max private streams monitors: %d\n", strpmonmax);
	qprintf("\nstr_intr_lock: 0x%x\n", &str_intr_lock);
	qprintf("str_resource_lock: 0x%x\n", &str_resource_lock);
	qprintf("strhead_monp_lock: 0x%x\n", &strhead_monp_lock);
	qprintf("\n");
	qprintf("joinwait 0x%x\n", &joinwait);
}

#ifdef STREAMS_MEM_TRACE
extern void str_mem_trace_print(struct str_mem_trace_log *, char *);
#endif /* STREAMS_MEM_TRACE */

/*
 *  kp lfvec addr
 *
 *  Print the address of the listvec structure  and the contents of the
 *  lfvec for this particular list head.
 */
void
idbg_lfvec(__psint_t addr)
{
	register struct lf_listvec *lf_listvecp;
	register int i;

	if (addr == -1L) { /* no address specified so exit */
		qprintf(" no lf_listvec structure address supplied\n");
		return;
	}
	lf_listvecp = (struct lf_listvec *)addr;

	qprintf("  lf_listvec 0x%x, lock(0x%x) 0x%x\n",
		lf_listvecp, &(lf_listvecp->lock), lf_listvecp->lock);

	for (i = 0; i < LFVEC_MAX_INDEX; i++) {

		qprintf("  Index %d [%s]\n", i, lf_bufnames[i]);
		qprintf("   head 0x%x, tail 0x%x, avg_nodect %d, nodecnt %d\n",
			lf_listvecp->buffer[i].head,
			lf_listvecp->buffer[i].tail,
			lf_listvecp->buffer[i].avg_nodecnt,
			lf_listvecp->buffer[i].nodecnt);
		qprintf("   reclaim_page 0x%x, node_mask 0x%x\n",
			lf_listvecp->buffer[i].reclaim_page,
			lf_listvecp->buffer[i].node_mask);
		qprintf("   nelts %d, e_per_page %d, size %d, threshold %d\n",
			lf_listvecp->buffer[i].nelts,
			lf_listvecp->buffer[i].e_per_page,
			lf_listvecp->buffer[i].size,
			lf_listvecp->buffer[i].threshold);
#ifdef DEBUG
		qprintf("   reclaim_page_cur 0x%x\n",
			lf_listvecp->buffer[i].reclaim_page_cur);

		qprintf("   lf_get calls %d, local %d, addpage %d, fail %d\n",
			lf_listvecp->buffer[i].get_calls,
			lf_listvecp->buffer[i].get_local,
			lf_listvecp->buffer[i].get_addpage,
			lf_listvecp->buffer[i].get_fail);

qprintf("   lf_free calls %d, reclaimpage: set %d, clear %d, incr %d\n",
			lf_listvecp->buffer[i].free_calls,
			lf_listvecp->buffer[i].free_reclaimpage_set,
			lf_listvecp->buffer[i].free_reclaimpage_clear,
			lf_listvecp->buffer[i].free_reclaimpage_incr);

qprintf("   lf_free pagesize_release %d, pagesize_norelease %d, localadd %d\n",
			lf_listvecp->buffer[i].free_pagesize_release,
			lf_listvecp->buffer[i].free_pagesize_norelease,
			lf_listvecp->buffer[i].free_localadd);
#endif /* DEBUG */
	}
	return;
}

/*
 *  kp clfcnts cpuid
 *  For each cpu print the address of the knetvec structure which holds the
 *  local vector structures and the various list counts and node sizes.
 *  Alternatively 'cpuid' can be the index into the pdaindr array of
 *  structures for the particular cpu desired.
 */
void
idbg_clfcnts(__psint_t addr)
{
	register struct lf_listvec *kstr_lfvecp;
	register int i, min, max;

	if (addr == -1L) { /* no cpuid specified so dump them all */
		min = 0;
		max = (int)maxcpus;
	} else {
		min = addr;
		max = min + 1;

		if (min < 0) {
			min = 0;
			max = maxcpus;
		} else {
			if (max > (u_short)maxcpus) {
				min = 0;
				max = maxcpus;
			}
		}
	}
	for (i = min; i < max; i++) {
		kstr_lfvecp = (struct lf_listvec *)pdaindr[i].pda->kstr_lfvecp;
		if (kstr_lfvecp == (struct lf_listvec *)0) {
			continue;
		}
		qprintf(" pdaindr ix %d, cpuid %d, kstr_lfvecp 0x%x\n",
				i, pdaindr[i].CpuId, kstr_lfvecp);
		idbg_lfvec((__psint_t)kstr_lfvecp);
	}
	return;
}

/*
 *  kp clfvec cpuid
 *  Prints contents of all lfvec structures for a cpu where 'cpuid'
 *  is the index into the pdaindr array of structures holding the per
 *  cpu information.  Inside the structure pointed to by kstr_lfvecp
 *  are the addresses of the lf_listvec structures containing the various
 *  sized Stream msg/dblk/buffer lists.
 */
void
idbg_clfvec(__psint_t addr)
{
	register struct lf_listvec *kstr_lfvecp;
	register int i, min, max;

	if (addr == -1) { /* no cpuid specified so dump them all */
		min = 0;
		max = (int)maxcpus;
	} else {
		min = addr;
		max = min + 1;

		if (min < 0) {
			min = 0;
			max = maxcpus;
		} else {
			if (max > (u_short)maxcpus) {
				min = 0;
				max = maxcpus;
			}
		}
	}
	for (i = min; i < max; i++) {
		kstr_lfvecp = (struct lf_listvec *)pdaindr[i].pda->kstr_lfvecp;
		qprintf("pdaindr ix %d, cpuid %d, kstr_lfvecp 0x%x\n",
			i, pdaindr[i].CpuId, kstr_lfvecp);
	}
	return;
}
