/**************************************************************************
 *									  *
 *	 	Copyright (C) 1994-1995 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded	instructions, statements, and computer programs	 contain  *
 *  unpublished	 proprietary  information of Silicon Graphics, Inc., and  *
 *  are	protected by Federal copyright law.  They  may	not be disclosed  *
 *  to	third  parties	or copied or duplicated	in any form, in	whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident	"$Revision: 1.18 $"

#include <sys/types.h>
#include <sys/uuid.h>
#include <sys/buf.h>
#include <sys/idbg.h>
#include <sys/idbgentry.h>
#include <sys/ktrace.h>
#include <sys/major.h>
#include <sys/scsi.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/hwgraph.h>
#include <sys/xlv_base.h>
#include "xlv_mem.h"
#include <sys/xlv_lock.h>
#include <sys/xlv_tab.h>
#include <sys/xlv_stat.h>
#include <sys/debug.h>
#include "xlv_ioctx.h"

/*
 * External functions & data not in header files.
 */
extern ktrace_entry_t	*ktrace_first(ktrace_t *, ktrace_snap_t *);
extern int		ktrace_nentries(ktrace_t *);
extern ktrace_entry_t	*ktrace_next(ktrace_t *, ktrace_snap_t *);
extern ktrace_entry_t	*ktrace_skip(ktrace_t *, int, ktrace_snap_t *);
extern char		*tab_bflags[];

extern void idbg_semdump(sema_t *);

extern int xlv_maxunits;

static void	idbg_xlv_help(void);
static void 	idbg_xlv_vol_print(xlv_tab_vol_entry_t *);
static void 	idbg_xlv_subvol_print(xlv_tab_subvol_t *);
static void	idbg_xlv_plex_print(xlv_tab_plex_t *);
static void	idbg_xlv_ve_print(xlv_tab_vol_elmnt_t *);
static void 	idbg_xlv_lock_print(unsigned);
static void 	idbg_xlv_resmem_print(xlv_res_mem_t *);
static void 	idbg_xlv_io_context_print(xlv_io_context_t *);
static void 	idbg_xlvmem_print(xlvmem_t *);
static void 	idbg_xlvstat_print(xlv_stat_t *);
#ifdef DEBUG
static void	idbg_xlv_trace_print_last(int);
static void	idbg_xlv_trace_print(void *);
static void	idbg_xlv_lock_trace_print(void *);
#endif
#ifdef XLV_LOCK_DEBUG
static void 	idbg_xlv_lock_trace(int);
#endif
#ifdef XLV_RESMEM_DEBUG
static void	idbg_xlv_resmem_trace(int);
#endif


#define VD	(void (*)())

static struct xif {
	char	*name;
	void	(*func)();
	char	*help;
} xlvidbg_funcs[] = {
    "xlvhelp",	VD idbg_xlv_help,		"Print help for idbg-xlv",
    "xlvvol",	VD idbg_xlv_vol_print,		"Dump XLV xlv_tab vol entry",
    "xlvsv",	VD idbg_xlv_subvol_print,	"Dump XLV xlv_tab subvol entry",
    "xlvplex",	VD idbg_xlv_plex_print,		"Dump XLV xlv_tab plex entry",
    "xlvve",	VD idbg_xlv_ve_print,		"Dump XLV xlv_tab volume element entry",
    "xlvlock",	VD idbg_xlv_lock_print,		"Dump XLV lock structure",
    "xlvrm",	VD idbg_xlv_resmem_print,	"Dump XLV reserved memory structures",
    "xlvioc",	VD idbg_xlv_io_context_print,	"Dump XLV io context structure",
    "xlvmem",	VD idbg_xlvmem_print,		"Dump XLV buffer memory structure",
    "xlvstat",	VD idbg_xlvstat_print,		"Dump XLV io statistics",
#ifdef DEBUG
    "xlvtrc",	VD idbg_xlv_trace_print_last,	"Dump [count] XLV trace entries",
    "xlvmtrc",	VD idbg_xlv_trace_print,	"Dump [match] XLV global trace buffer",
    "xlvltrc",	VD idbg_xlv_lock_trace_print,	"Dump XLV lock trace buffer",
#endif
#ifdef XLV_LOCK_DEBUG
    "xlvldt",	VD idbg_xlv_lock_trace,		"Dump last count XLV lock debug entries",
#endif
#ifdef XLV_RESMEM_DEBUG
    "xlvrmt",	VD idbg_xlv_resmem_trace,	"Dump XLV reserved memory trace buffer",
#endif
    0,		0,	0
};

/*
 * Initialization routine.
 */
void
xlvidbginit(void)
{
	struct xif	*p;

	for (p = xlvidbg_funcs; p->name; p++)
		idbg_addfunc(p->name, p->func);
}

int
xlvidbgunload(void)
{
	struct xif	*p;

	for (p = xlvidbg_funcs; p->name; p++)
		idbg_delfunc(p->func);
	return 0;
}

static char *xlv_io_context_state_str[] = {
	"none",
	"INIT",
	"READ",
	"REREAD",
	"REWRITE",
	"WAIT_FOR_OFFLINE",
	"WRITE",
	"DISK_OFFLINE_DONE",
	"XLV_SM_READ_WRITEBACK",
	"XLV_SM_WRITEBACK",
	"XLV_SM_REREAD_WRITEBACK",
	"XLV_SM_NOTIFY_VE_ACTIVE",
	"XLV_SM_MMAP_WRITE",		/* unused MMAP ops */
	"XLV_SM_MMAP_WRITE_CONT",
	"XLV_SM_MMAP_WRITE_CONT_ERROR",
	"XLV_SM_UNBLOCK_DRIVER",
	"XLV_SM_NOTIFY_VES_OFFLINE",
	"END",
	"BEYOND_END"
};

/*
 * Prototypes for static functions.
 */
static char *xlv_fmtuuid(uuid_t *);
static void xlv_tab_block_map_print(xlv_block_map_t *);
static void xlv_tab_subvol_print(xlv_tab_subvol_t *);
static void xlv_tab_vol_print(xlv_tab_vol_entry_t *);
static void xlv_stat_print(xlv_stat_t *);
#ifdef DEBUG
static void xlv_trace_entry (ktrace_entry_t *);
static void xlv_trace_lock_entry (ktrace_entry_t *);
#endif

/*
 * Static functions.
 */


/*
 * Format a uuid into a static buffer & return it.  This doesn't
 * use the formatted value, it probably should (see C library).
 */
static char *
xlv_fmtuuid(uuid_t *uu)
{
	static char rval[40];
	uint *ip = (uint *)uu;

	ASSERT(sizeof(*uu) == 16);
	sprintf(rval, "%w32x:%w32x:%w32x:%w32x", ip[0], ip[1], ip[2], ip[3]);
	return rval;
}

/*
 * Print out an xlv subvolume block map.
 */
static void
xlv_tab_block_map_print(xlv_block_map_t *block_map)
{
        int     i;

        if (block_map == NULL) {
		qprintf ("NULL block_map\n");
                return;
	}

	qprintf (
"  block map:  blocks    read_plex_vector   write_plex_vector\n");
        for (i=0; i<block_map->entries; i++) {
                qprintf ("  %18d          0x%x                 0x%x\n",
                        block_map->map[i].end_blkno,
                        block_map->map[i].read_plex_vector,
                        block_map->map[i].write_plex_vector);
        }

}

static char *svtype_str[] = {
	"",	/* 0-based. */
	"LOG", "DATA", "RT", "rsvd", "unknown",
};

static void 
xlv_tab_subvol_print(xlv_tab_subvol_t *subvol_entry)
{
	char            *uuid_strp;
	int             p;

	uuid_strp = xlv_fmtuuid (&(subvol_entry->uuid));

	if (subvol_entry->vol_p) {
		qprintf ( "%s [%s]\tuuid=%s\n", 
			svtype_str[subvol_entry->subvol_type], 
			subvol_entry->vol_p->name, uuid_strp);
	}
	else {
		qprintf ( "%s [no vol name]\t%s\n",
                        svtype_str[subvol_entry->subvol_type],
                        uuid_strp);
	}
	qprintf ( "  flags=0x%x, size=%d, plexes=%d, device=(%d [%d], %d)\n",
		subvol_entry->flags, subvol_entry->subvol_size,
		subvol_entry->num_plexes, emajor(subvol_entry->dev),
		major(subvol_entry->dev), minor(subvol_entry->dev));

	qprintf ( "  initial_read_slice=%d, vol_p: 0x%x, plexes=%d\n",
		subvol_entry->initial_read_slice, subvol_entry->vol_p,
		subvol_entry->num_plexes);

	xlv_tab_block_map_print (subvol_entry->block_map);

	qprintf (
"  cregion start=%d, end=%d, pending_requests: 0x%x, excl: 0x\n",
		 subvol_entry->critical_region.start_blkno,
		 subvol_entry->critical_region.end_blkno,
		 subvol_entry->critical_region.pending_requests,
		 subvol_entry->critical_region.excluded_rqst);
	qprintf (
"  rd_wr_back_start=%d, rd_wr_back_end=%d, err_pending_rqst: 0x%x\n",
		subvol_entry->rd_wr_back_start, 
		subvol_entry->rd_wr_back_end,
		subvol_entry->err_pending_rqst);

	for (p = 0; p < XLV_MAX_PLEXES; p++) {
		idbg_xlv_plex_print (subvol_entry->plex[p]);
	}
}


static void 
xlv_tab_vol_print(xlv_tab_vol_entry_t *vol_entry)
{
	char            *uuid_strp;
	static char	*vol_type_txt[] = {
		"none",	/* 0-based. */
		"complete",
		"missing common piece",
		"all pieces but holey",
		"missing unique piece",
		"missing required subvol",
		"unknown",
	};

	uuid_strp = xlv_fmtuuid (&(vol_entry->uuid));

	qprintf ( "VOL [%s] at 0x%x    uuid=%s\n",
		vol_entry->name, vol_entry, uuid_strp);

	qprintf ( "  versions=%d, flags=0x%x, [%s], sector_size=%d\n",
		vol_entry->version, vol_entry->flags,
		vol_type_txt[vol_entry->state], vol_entry->sector_size);

	qprintf ( "  log: 0x%x, data: 0x%x, rt: 0x%x\n",
		vol_entry->log_subvol, vol_entry->data_subvol,
		vol_entry->rt_subvol);

	qprintf ( "  nodename: %s\n",
		(vol_entry->nodename) ? vol_entry->nodename : "<NULL>");
}


#ifdef DEBUG
static void
xlv_trace_entry (ktrace_entry_t *ktep)
{
	ulong 	state_index;

	/*
	 * Don't print anything if bp is NULL.
	 */
	if (ktep->val[1] == NULL)
		return;

        qprintf("%s bp 0x%x flags ",
                (char *)(ktep->val[0]), ktep->val[1]);
        if (ktep->val[2] != 0) {
		qprintf ("[");
                printflags((ulong)(ktep->val[2]), tab_bflags,"bflags");
                qprintf("]\n");
        } else {
                qprintf("[no flags]\n");
        }
        qprintf(" bcount 0x%x bufsize 0x%x blkno 0x%x dmaaddr 0x%x\n",
                ktep->val[3], ktep->val[4], ktep->val[5], ktep->val[6]);
	state_index = (ulong) ktep->val[9];

	if (state_index > 16)
		state_index = 0;
	qprintf (" parent_bp 0x%x io_context 0x%x state %s\n",
ktep->val[7], ktep->val[8], xlv_io_context_state_str[state_index]);

	qprintf (
" orig_bp 0x%x retry_bp 0x%x mmap_bp 0x%x {0x%x 0x%x} lck_cnt: %d\n",
                ktep->val[10], ktep->val[11], ktep->val[12],
		ktep->val[13], ktep->val[14], ktep->val[15]);

}

static void
xlv_trace_lock_entry (ktrace_entry_t *ktep)
{
        ulong 	state_index, subvol_index;

	/*
	 * If this is an ADMIN entry, the arg 1 is a subvol index
	 * instead of a bp.
	 */
	subvol_index = (ulong) ktep->val[1];

	/*
	 * If this is a null entry, just return.
	 */
	if (ktep->val[0] == NULL)
		return;

	if (subvol_index < xlv_maxunits) {
		qprintf("%s  [%d] ", (char *)(ktep->val[0]), subvol_index);
	}
	else {
		state_index = (ulong) ktep->val[4];
		if (state_index > 16)
			state_index = 0;
		qprintf("%s bp 0x%x parent 0x%x io_context 0x%x %s\n",
			(char *)(ktep->val[0]), 
			(__psint_t)ktep->val[1], 
			(__psint_t)ktep->val[2],
			(__psint_t)ktep->val[3], 
			xlv_io_context_state_str[state_index]);
		qprintf(" orig_bp 0x%x retry_bp 0x%x mmap_bp 0x%x\n",
			(__psint_t)ktep->val[5], 
			(__psint_t)ktep->val[6], 
			(__psint_t)ktep->val[7]);
	}
	qprintf(" lock_cnt %d acc_cnt %d wait_cnt %d\n",
		(__psint_t)ktep->val[8], 
		(__psint_t)ktep->val[9], 
		(__psint_t)ktep->val[10]);

}
#endif	/* DEBUG */

/*
 * Command-level xlv-idbg functions.
 */

/*
 * Print out the help messages for these functions.
 */
static void
idbg_xlv_help(void)
{
	struct xif	*p;

	for (p = xlvidbg_funcs; p->name; p++)
		qprintf("%s %s\n", padstr(p->name, 16), p->help);
}

/*
 * Prints out the xlv io_context.
 */
static void
idbg_xlv_io_context_print(xlv_io_context_t *io_context)
{
	int 	i, max_failures;

	qprintf (
"orig_buf: 0x%x, retry_buf: 0x%x, mmap_wr_buf: 0x%x, xlvmem: 0x%x\n",
		 io_context->original_buf, &io_context->retry_buf,
		 NULL,
		 io_context->buffers);
	qprintf (
"state: %s, failures: %d, bytes_xferred: 0x%x, flags: 0x%x\n",
		 xlv_io_context_state_str[io_context->state], 
		 io_context->num_failures,
		 0,
		 io_context->flags);

	qprintf ("current_slices: blk_map_index: %d, entries: %d [ ",
		io_context->current_slices.first_block_map_index,
		io_context->current_slices.num_entries);
	for (i=0; i<3; i++)
		qprintf ("0x%x ", io_context->current_slices.slices_used[i]);
	qprintf ("]\n");

	max_failures = io_context->num_failures;
	if (max_failures > XLV_MAX_FAILURES)
		max_failures = XLV_MAX_FAILURES;
	for (i=0; i<io_context->num_failures; i++) {
		qprintf ("\tfailure[%d] error: 0%x : %s\n", 
			i, io_context->failures[i].error,
			devtopath (io_context->failures[i].dev));
	}
			
}


static void
xlv_print_queue(char *s, buf_queue_t *bq)
{
    buf_t	 *bp;
    int		  count;

    qprintf("  %s: count: %d, queued: %d, max: %d;", s, bq->count,
	    bq->n_queued, bq->max_queued);

    bp = bq->head;
    if (bp) {
	qprintf("\n    waiters:");
	for (bp = bq->head, count = 0; bp; bp = BUF_QUEUE_NEXT(bp), count++) {
	    if ((count & 3) == 1) qprintf(" 0x%x\n   ", bp);
	    else qprintf(" 0x%x", bp);
	}
	if ((count & 3) != 2) qprintf("\n");
    }
    else qprintf(" waiters: none.\n");
}


static void
idbg_xlv_lock_print (unsigned subvol_index)
{
	xlv_lock_t	 *svl;

	/*
         * See if the user has entered the major/minor device of
         * the subvolume. If so, use that to index to the lock
         * entry.
         */
        if ((emajor(subvol_index) == XLV_LOWER_MAJOR) ||
            (emajor(subvol_index) == XLV_MAJOR)) {
                subvol_index = minor(subvol_index);
        }

	svl = &xlv_io_lock[subvol_index];

	qprintf("XLV io lock[%d] at 0x%x: lock: 0x%x at 0x%x;\n"
		"  io count: %d; error count: %d; wait count: %d; statp: 0x%x;\n",
		subvol_index, svl, svl->lock, &svl->lock,
		svl->io_count, svl->error_count, svl->wait_count,
		svl->statp);
	idbg_semdump(&svl->wait_sem);
	xlv_print_queue("error queue  ", &svl->error_queue);
	xlv_print_queue("reissue queue", &svl->reissue_queue);
}


#ifdef XLV_LOCK_DEBUG
/* Dump out the last [count] entries in the xlv lock debug trace buffer */
static void
idbg_xlv_lock_trace(int count)
{
        ktrace_entry_t  *ktep;
        ktrace_snap_t   kts;
        int             nentries;
	extern ktrace_t	*xlv_lock_dbg_trace_buf;

        if (xlv_lock_dbg_trace_buf == NULL) {
                qprintf("The xlv lock debug trace buffer is not initialized.\n");
                return;
        }
        nentries = ktrace_nentries(xlv_lock_dbg_trace_buf);
        if (count == -1) {
                count = nentries;
        }
        if ((count <= 0) || (count > nentries)) {
                qprintf("Invalid count.  There are %d entries.\n", nentries);
                return;
        }

        ktep = ktrace_first(xlv_lock_dbg_trace_buf, &kts);
        if (count != nentries) {
                /*
                 * Skip the total minus the number to look at minus one
                 * for the entry returned by ktrace_first().
                 */
                ktep = ktrace_skip(xlv_lock_dbg_trace_buf, nentries-count-1, &kts);
                if (ktep == NULL) {
                        qprintf("Skipped them all\n");
                        return;
                }
        }

	for (count = 0;
	     ktep != NULL;
	     count++, ktep = ktrace_next(xlv_lock_dbg_trace_buf, &kts)) {
		if (count == 10) count = 0;
		if (count == 0) {
			qprintf(
"Caller     sv         bp           count  error queue head   error queue tail\n"
"           error     #io    wait   count reissue queue head reissue queue tail\n");
		}
			qprintf("%s %5d 0x%016x %7d 0x%016x 0x%016x\n"
				"         %7d %7d %7d %7d 0x%016x 0x%016x\n",
				ktep->val[0], ktep->val[1], ktep->val[2],
				ktep->val[6], ktep->val[7], ktep->val[8],
				ktep->val[3], ktep->val[4], ktep->val[5],
				ktep->val[9], ktep->val[10], ktep->val[11]);
        }
}
#endif /* XLV_LOCK_DEBUG */


#ifdef XLV_RESMEM_DEBUG
/* Dump out the last [count] entries in the xlv reserved memory trace buffer */
static void
idbg_xlv_resmem_trace(int count)
{
        ktrace_entry_t  *ktep;
        ktrace_snap_t   kts;
        int             nentries;
        extern ktrace_t *xlv_rm_trace_buf;

        if (xlv_rm_trace_buf == NULL) {
                qprintf("The xlv reserved memory trace buffer is not initialized.\n");
                return;
        }
        nentries = ktrace_nentries(xlv_rm_trace_buf);
        if (count == -1) {
                count = nentries;
        }
        if ((count <= 0) || (count > nentries)) {
                qprintf("Invalid count.  There are %d entries.\n", nentries);
                return;
        }

        ktep = ktrace_first(xlv_rm_trace_buf, &kts);
        if (count != nentries) {
                /*
                 * Skip the total minus the number to look at minus one
                 * for the entry returned by ktrace_first().
                 */
                ktep = ktrace_skip(xlv_rm_trace_buf, nentries-count-1, &kts);
                if (ktep == NULL) {
                        qprintf("Skipped them all\n");
                        return;
                }
        }

	for (count = 0;
	     ktep != NULL;
	     count++, ktep = ktrace_next(xlv_rm_trace_buf, &kts)) {
		if (count == 10) count = 0;
		if (count == 0) {
			qprintf(
"Caller  rm name       hits     misses    waits  slots  queue                 bp\n"
"          current slot        return slot memory wait q head memory wait q tail\n");
		}
			qprintf("%s %s %10d %10d %8d %6d %6d 0x%016x\n"
				"    0x%016x 0x%016x 0x%016x 0x%016x\n",
				ktep->val[0], ktep->val[1], ktep->val[2],
				ktep->val[3], ktep->val[4], ktep->val[5],
				ktep->val[6], ktep->val[7], ktep->val[8],
				ktep->val[9], ktep->val[10], ktep->val[11]);
        }
}
#endif /* XLV_RESMEM_DEBUG */


static void
xlv_print_resmem(xlv_res_mem_t *rm)
{
    void	 *mem;
    int		  count;

    qprintf("XLV reserved memory for %s: slots: %d; size: %d active: %d.\n",
	    rm->name, rm->slots, rm->size, rm->active);

    qprintf("  quiesce %d waiters %d.\n  ", rm->quiesce, rm->waiters);

    qprintf(
"  lock: 0x%x; total hits: %d, total misses: %d, waits: %d.\n  ",
	    rm->lock, rm->totalhits + rm->hits, rm->totalmisses + rm->misses, 
	    rm->waits);

    qprintf(
"  scale: %d%%, threshold: %d%%, mem resized %d, failed %d. in progress %d\n  ",
	    rm->scale, rm->threshold, rm->resized, rm-> resizefailed, 
	    rm->memresize);
	
    idbg_semdump(&rm->io_wait);
    idbg_semdump(&rm->quiesce_wait);
    qprintf("  res_mem at 0x%x; last slot at 0x%x;\n    current list:",
	    rm->res_mem_start, rm->res_mem_last);
    if (rm->res_mem) {
	for (mem = rm->res_mem, count = 0; mem; mem = *(void **) mem, count++) {
	    qprintf("%s0x%x", !(count & 3) ? "\n    " : " ", mem);
	}
	qprintf("\n");
    }
    else {
	qprintf(" empty.\n");
    }
}


/*
 * Prints out the xlv reserved memory structure
 */
static void
idbg_xlv_resmem_print(xlv_res_mem_t *rm)
{
    if ((int64_t) rm == -1LL) {
	qprintf("XLV reserved memory structures:\n"
		"  xlv_ioc at 0x%x; xlv_top at 0x%x;\n  xlv_low at 0x%x.\n",
		&xlv_ioctx_mem, &xlv_top_mem, &xlv_low_mem);
    }
    else if (rm == 0) {
	xlv_print_resmem(&xlv_ioctx_mem);
	xlv_print_resmem(&xlv_top_mem);
	xlv_print_resmem(&xlv_low_mem);
    }
    else {
	xlv_print_resmem(rm);
    }
}


#ifdef DEBUG
/*
 * Print out all the entries in the xlv lock trace buf with any entry
 * corresponding to the given buffer.
 */
/* ARGSUSED */
static void
idbg_xlv_lock_trace_print (void *match_val)
{
        ktrace_entry_t  *ktep;
        ktrace_snap_t   kts;
        extern ktrace_t *xlv_lock_trace_buf;

        if (xlv_lock_trace_buf == NULL) {
                qprintf("The xlv lock trace buffer is not initialized\n");
                return;
        }

        ktep = ktrace_first(xlv_lock_trace_buf, &kts);
        while (ktep != NULL) {
		xlv_trace_lock_entry(ktep);
                ktep = ktrace_next(xlv_lock_trace_buf, &kts);
        }
}
#endif

static void
idbg_xlv_plex_print (xlv_tab_plex_t *plex_entry)
{
	int			ve;
	char                *uuid_strp;

	if (plex_entry == NULL) {
		qprintf("PLEX <null>\n");
		return;
	}

	uuid_strp = xlv_fmtuuid (&(plex_entry->uuid));

	qprintf("PLEX at 0x%x uuid=%s,\n", plex_entry, uuid_strp);
	qprintf("  flags=0x%x, num=%d, max=%d\n",
		plex_entry->flags, plex_entry->num_vol_elmnts,
		plex_entry->max_vol_elmnts);

	for (ve = 0; ve < plex_entry->num_vol_elmnts; ve++)
		idbg_xlv_ve_print (&(plex_entry->vol_elmnts[ve]));
}

static void
idbg_xlv_subvol_print (xlv_tab_subvol_t *subvol_entry)
{
	long		subvol_index;
	dev_t		dev;

	/*
	 * See if the user has entered the major/minor device of
	 * the subvolume. If so, use that to index to the subvol
	 * entry.
	 */
	dev = (dev_t)((unsigned long)subvol_entry);
	if ((emajor(dev) == XLV_LOWER_MAJOR) ||
	    (emajor(dev) == XLV_MAJOR)) {
		/* minor(dev) < xlv_tab->max_subvols */
		subvol_entry = &xlv_tab->subvolume[minor(dev)];
		xlv_tab_subvol_print (subvol_entry);
		return;
	}

	/*
	 * If the user specified -1, then display all the non-null
	 * entries.
	 */
	subvol_index = (long) subvol_entry;
	if (subvol_index == -1L) {
		/*
		 * Print all the non-null entries.
		 */
		qprintf ("xlv_tab addr: 0x%x, num=%d, max=%d\n",
			xlv_tab, xlv_tab->num_subvols, xlv_tab->max_subvols);
		for (subvol_index=0; subvol_index < xlv_tab->max_subvols;
		     subvol_index++) {
			subvol_entry = &xlv_tab->subvolume[subvol_index];
			if (subvol_entry->vol_p) {
				xlv_tab_subvol_print (subvol_entry);
				qprintf ("\n");
			}
		}
		return;
	}

	/*
	 * See if the user has specified an index (< xlv_maxunits).
	 * If so, use that to index into the subvol entry.
	 */
	if ((0 <= subvol_index) && (subvol_index < xlv_tab->max_subvols)) {
		subvol_entry = &xlv_tab->subvolume[(ulong)subvol_entry];
		xlv_tab_subvol_print (subvol_entry);
		return;
	}

	/*
	 * Just treat it as an address.
	 */
	xlv_tab_subvol_print (subvol_entry);
}


static void
idbg_xlv_vol_print (xlv_tab_vol_entry_t *vol_entry)
{
	long		idx;

	/*
	 * If the user specified -1, then display all the non-null
	 * entries.
	 */
	idx = (long) vol_entry;
	if (idx == -1L) {
		/*
		 * Print all the non-null entries.
		 */
		qprintf ("xlv_tab_vol addr: 0x%x, num=%d, max=%d\n",
			xlv_tab_vol, xlv_tab_vol->num_vols,
			xlv_tab_vol->max_vols);
		for (idx=0; idx < xlv_tab_vol->num_vols; idx++) {
			vol_entry = &xlv_tab_vol->vol[idx];
			xlv_tab_vol_print (vol_entry);
			qprintf ("\n");
		}
		return;
	}

	/*
	 * See if the user has specified an index (< xlv_tab_vol->num_vols).
	 * If so, use that to index into the subvol entry.
	 */
	if ((0 <= idx) && (idx < xlv_tab_vol->num_vols)) {
		vol_entry = &xlv_tab_vol->vol[(ulong)vol_entry];
		xlv_tab_vol_print (vol_entry);
		return;
	}

	/*
	 * Just treat it as an address.
	 */
	xlv_tab_vol_print (vol_entry);
}

#ifdef DEBUG
/*
 * Print out all the entries in the xlv trace buf with any entry
 * corresponding to the given buffer. 
 */
static void
idbg_xlv_trace_print (void *match_val)
{
        ktrace_entry_t  *ktep;
        ktrace_snap_t   kts;
        extern ktrace_t *xlv_trace_buf;
	unsigned	i;

        if (xlv_trace_buf == NULL) {
                qprintf("The xlv trace buffer is not initialized\n");
                return;
        }

        qprintf("xlv_[search]trace 0x%x\n", match_val);
        ktep = ktrace_first(xlv_trace_buf, &kts);
        while (ktep != NULL) {
		for (i=1; i < 16; i++) {
			if (((ktep->val[i])) == match_val) {
				xlv_trace_entry(ktep);
				break;
			}
                }

                ktep = ktrace_next(xlv_trace_buf, &kts);
        }
}

/*
 * Dump out the last [count] entries in the xlv trace buffer.
 */
static void
idbg_xlv_trace_print_last(int count)
{
        ktrace_entry_t  *ktep;
        ktrace_snap_t   kts;
        int             nentries;
        int             skip_entries;
        extern ktrace_t *xlv_trace_buf;

        if (xlv_trace_buf == NULL) {
                qprintf("The xlv trace buffer is not initialized\n");
                return;
        }
        nentries = ktrace_nentries(xlv_trace_buf);
        if (count == -1) {
                count = nentries;
        }
        if ((count <= 0) || (count > nentries)) {
                qprintf("Invalid count.  There are %d entries.\n", nentries);
                return;
        }

        ktep = ktrace_first(xlv_trace_buf, &kts);
        if (count != nentries) {
                /*
                 * Skip the total minus the number to look at minus one
                 * for the entry returned by ktrace_first().
                 */
                skip_entries = nentries - count - 1;
                ktep = ktrace_skip(xlv_trace_buf, skip_entries, &kts);
                if (ktep == NULL) {
                        qprintf("Skipped them all\n");
                        return;
                }
        }
        while (ktep != NULL) {
                xlv_trace_entry(ktep);
                ktep = ktrace_next(xlv_trace_buf, &kts);
        }
}
#endif

static void
idbg_xlv_ve_print (xlv_tab_vol_elmnt_t *ve)
{
	int		d;
	char            *uuid_strp;
	static char	*xlv_ve_state_txt[] = {
		"empty",
		"clean",
		"active",
		"stale",
		"offline",
		"incomplete"
	};
	static char	*xlv_ve_type[] = {
		"none",
		"striped",
		"concat"
	};

	if (ve == NULL) {
		qprintf ( "VE <null>");
		return;
	}

	uuid_strp = xlv_fmtuuid(&(ve->uuid));

	qprintf ( "VE  uuid=%s, [%s], flags=0x%x\n",
		uuid_strp, xlv_ve_state_txt[ve->state], ve->flags);

	qprintf (
"  start=%llu, end=%llu, grp_size=%d, stripe_unit_size=%d\t",
		ve->start_block_no, ve->end_block_no, 
		ve->grp_size, ve->stripe_unit_size);

	if (ve->type <= XLV_VE_TYPE_CONCAT)
		qprintf ("%s\n", xlv_ve_type[ve->type]);
	else 
		qprintf ("<illegal flag %d>\n", ve->type);

	for (d = 0; d < ve->grp_size; d++) {
		int		p_inx;

		qprintf("  class: %d; # paths: %d (active: %d, retry: %d)\n",
			ve->disk_parts[d].class, ve->disk_parts[d].n_paths,
			ve->disk_parts[d].active_path,
			ve->disk_parts[d].retry_path);
		for (p_inx = 0; p_inx < ve->disk_parts[d].n_paths; p_inx++) {
			qprintf("    %s\t%s\n",
				devtopath(ve->disk_parts[d].dev[p_inx]),
				(p_inx == ve->disk_parts[d].active_path)
				    ? xlv_fmtuuid(&(ve->disk_parts[d].uuid))
				    : "standby path");
		}
	}
}

/*
 * Prints out the xlv private buffer structure.
 */
static void
idbg_xlvmem_print(xlvmem_t *xlvmem)
{
	int	i;

	qprintf ("next: 0x%x, mapped: 0x%x, bufs: %d, privmem: %d\n",
		xlvmem->next, xlvmem->mapped, xlvmem->bufs, xlvmem->privmem);

	for (i=0; i < xlvmem->bufs; i++) {
		qprintf ("0x%x  ", &(xlvmem->buf[i]));
		if (((i+1) % 8) == 0)
			qprintf ("\n");
	}
	qprintf ("\n");
}


static void
xlv_stat_print(xlv_stat_t *statp)
{
    qprintf(
	"address: 0x%x\n"
	"  read ops: %d, read blocks: %d\n",
		statp, statp->xs_ops_read, statp->xs_io_rblocks);
    qprintf(
	"  write ops: %d, write blocks: %d\n",
		statp->xs_ops_write, statp->xs_io_wblocks);
    qprintf(
	"    stripe ops: %d, total units: %d\n",
		statp->xs_stripe_io, statp->xs_su_total);
    qprintf(
	"        largest single i/o: %d stripe units,  frequency: %d\n",
		statp->xs_max_su_len, statp->xs_max_su_cnt);
    qprintf(
	"        aligned     <    stripe width; ends on stripe unit: %d\n"
	"        aligned     >    stripe width; ends on stripe unit: %d\n"
	"        aligned     =    stripe width; ends on stripe unit: %d\n"
	"        aligned   > or < stripe width; doesn't end on stripe unit: %d\n",
		statp->xs_align_less_sw, statp->xs_align_more_sw,
		statp->xs_align_full_sw, statp->xs_align_part_su);
    qprintf(
	"        unaligned   <    stripe width; ends on stripe unit: %d\n"
	"        unaligned   >    stripe width; ends on stripe unit: %d\n"
	"        unaligned   =    stripe width; doesn't end on stripe unit: %d\n"
	"        unaligned > or < stripe width; doesn't end on stripe unit: %d\n",
		statp->xs_unalign_less_sw, statp->xs_unalign_more_sw,
		statp->xs_unalign_full_sw, statp->xs_unalign_part_su);
}


/*
 * Prints out the xlv statistic structure.
 */
static void
idbg_xlvstat_print(xlv_stat_t *statp)
{
	long	idx, s;

	/*
	 * If the user specified -1, then display all statistics.
	 */
	idx = (long)statp;
	if (idx == -1L) {
		for (s = 0; s < xlv_tab->max_subvols; s++) {
			if (xlv_tab->subvolume[s].vol_p == NULL)
				continue;
			qprintf("stat[%d]  %s.%s  ",
				s,
				xlv_tab->subvolume[s].vol_p->name,
				svtype_str[xlv_tab->subvolume[s].subvol_type]);
			xlv_stat_print(xlv_io_lock[s].statp);
		}
		return;
	}

	/*
	 * See if the user has specified an index (< xlv_tab->max_subvols).
	 * If so, use that to index into the subvolume stat entry.
	 */
	if ((0 <= idx) && (idx < xlv_tab->max_subvols)) {
		qprintf("stat[%d]  ", idx);
		if (xlv_tab->subvolume[idx].vol_p != NULL)
			qprintf("%s.%s  ",
			    xlv_tab->subvolume[idx].vol_p->name,
			    svtype_str[xlv_tab->subvolume[idx].subvol_type]);
		xlv_stat_print(xlv_io_lock[idx].statp);
		return;
	}

	/*
	 * Just treat it as an address.
	 */
	xlv_stat_print(statp);

}
