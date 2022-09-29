#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <signal.h>
#include <unistd.h>
#include <bstring.h>
#include <sys/time.h>
#include <sys/par.h>
#include <sys/utsname.h>
#include <sys/types.h>
#include <sys/syssgi.h>

#include "data.h"
#include "wait.h"
#include "term.h"
#include "freelist.h"
#include "top.h"

static const char *topwait_help_title[] = {
  "Topwait version 1.0",
  "",
  "A top synchronization objects display for Unix",
  NULL
};

static const char *topwait_help_text[] = {
  "t\t- show only objects of specified type (all, sv, mutex, mrlock, sema)",
  NULL
};

void *topwait_get_first(top_state *ts);
void *topwait_get_next(top_state *ts, void*);
static void topwait_zero_interval_counters(top_state *ts);
static int topwait_display_objs(top_state *ts, int lineno);
static int topwait_display_stats(top_state *ts, int lineno);
static int topwait_display_help_title(top_state *ts, int lineno);
static int topwait_display_help_text(top_state *ts, int lineno);
static int topwait_handle_event(top_state *ts, tstamp_event_entry_t*);
static int topwait_handle_keypress(top_state *ts, int ch);

static sort_order_proto(order_load);
static sort_order_proto(order_swtch);

static sort_order_t topwait_sort_orders[] = {
        "load",		order_load,
	"switch",	order_swtch,

	NULL, NULL
};

static top_impl_t topwait_impl = {
        NULL,	/* topwait_destroy */
        NULL,	/* topwait_usage */
        topwait_sort_orders,
        "task",
	NULL,	/* topwait_getopts */
        topwait_get_first,
        topwait_get_next,
        topwait_zero_interval_counters,
        topwait_display_objs,
        topwait_display_stats,
        topwait_display_help_title,
        topwait_display_help_text,
	topwait_handle_event,
	topwait_handle_keypress,
	NULL,	/* topwait_handle_option */
	NULL	/* topwait_init */
};

typedef struct wait_lock_type_s {
	const char *wt_name;
	int wt_type;
} wait_lock_type_t;

#define SHOW_ALL_TYPES 0

static wait_lock_type_t lock_types[] = {
	"all",          SHOW_ALL_TYPES,
	"sv", 		SVWAIT,
	"mutex",	MUTEXWAIT,
	"mrlock",	MRWAIT,
	"sema",		SEMAWAIT,

	NULL, NULL
};

#define UNPACK_WAIT_FLAGS(ev, f) { f = (int)((ev->qual[1])>>32); }

#define STAT_NLINES 3

static wait_state ws;
static freelist wp_freelist, wl_freelist;
static int show_type = SHOW_ALL_TYPES;
static char title[160];
static double cycle_rate;

static void pid_leaving_state(wait_pid_state *wp, tstamp_event_entry_t *ev);
static int set_show_type(const char *type);

FREELIST_IMPL_FUNCS(wait_pid_state, wp)
FREELIST_IMPL_FUNCS(wait_lock_state, wl)

static void
wl_up(wait_lock_state *wl, uint64_t tstamp)
{
	wl->wl_load++;
	wl->wl_nswtch++;

	assert(wl->wl_load > 0);

	if (wl->wl_load == 1)
		wl->wl_wait_when = tstamp;

	if (wl->wl_load > wl->wl_max_load)
		wl->wl_max_load = wl->wl_load;

	assert(wl->wl_max_load >= 1);
}

static void
wl_down(wait_lock_state *wl, uint64_t tstamp)
{
	assert(wl->wl_load > 0);

	wl->wl_load--;

	if (wl->wl_load == 0)
		wl->wl_wait_total += (tstamp - wl->wl_wait_when);
}

#define PID_ENTER_STATE(wpv, state, ev) \
	{ pid_leaving_state(wpv, ev); \
	  (wpv) -> wp_tstamp = (ev) -> tstamp; \
	  ws.ws_ ## state ++; wpv -> wp_reason = wp_ ## state; }

static void wait_block(tstamp_event_entry_t *ev)
{
	pid_t pid = (pid_t) ev->qual[0];
	int flags;
	hashable *hash;
	wait_pid_state *wp;

	ws.ws_nswtch.total++;

	if ((hash = htab_find(&ws.ws_pids, HASH_PID(pid), pid)) != NULL)
		wp = HASHABLE_TO_WP(hash);
	else {
		wp = wp_alloc(&wp_freelist);
		wp->wp_pid = pid;
		wp->wp_reason = wp_running;
		wp->wp_tstamp = ev->tstamp;
		wp->wp_locking = NULL;
		ws.ws_running++;
		htab_insert(&ws.ws_pids, HASH_PID(pid), pid,
			WP_TO_HASHABLE(wp));
	}

	UNPACK_WAIT_FLAGS(ev, flags);

	if (flags & RESCHED_LOCK) {
		wait_lock_state *wl;
		uint64_t lock = ev->qual[2];

		/* swtch event logging race condition.
		*/
		if (lock == 0)
			return;

		ws.ws_nswtch.wait++;

		if ((hash = htab_find(&ws.ws_locks, HASH_LOCK(lock),
			lock)) != NULL)
			wl = HASHABLE_TO_WL(hash);
		else {
			wl = wl_alloc(&wl_freelist);
			wl->wl_addr = lock;
			wl->wl_type = flags;
			wl->wl_load = 0;
			wl->wl_max_load = 0;
			wl->wl_nswtch = 0;
			wl->wl_wait_total = 0;
			htab_insert(&ws.ws_locks, HASH_LOCK(lock), lock,
				WL_TO_HASHABLE(wl));

			wl->wl_name[0] = '\0';
			if (ev->jumbocnt > 0) {
				const tstamp_event_resched_data_t* rdp =
				(const tstamp_event_resched_data_t*) ev->qual;
				if (rdp->wchanname[0])
					strncpy(wl->wl_name, rdp->wchanname,
						16);
			}
		}

#if 0
		if (wp->wp_tstamp > ev->tstamp) {
			/* If this event becomes visible
			   after the process has already switched
			   states, we should remain in the old state,
			   but properly credit the blocking time to
			   the lock.
			*/

			wl_up(wl, ev->tstamp);
			wl_down(wl, wp->wp_tstamp);
		}
		else {
#endif
			wl_up(wl, ev->tstamp);
			PID_ENTER_STATE(wp, waiting, ev);
			wp->wp_locking = wl;
#if 0
		}
#endif
	}
	else {
		switch (flags) {
		case MUSTRUNCPU:	/* switching cpus */
		case GANGWAIT:		/* waiting for gang to run */
		case RESCHED_P:		/* preempted in user mode */
		case RESCHED_KP:	/* preempted in kernel mode */
			ws.ws_nswtch.preempt++;
			PID_ENTER_STATE(wp, preempted, ev);
			break;
		case RESCHED_S:		/* stopped by signal */
			PID_ENTER_STATE(wp, stopped, ev);
			break;
		case RESCHED_Y:		/* voluntary yield */
			ws.ws_nswtch.yield++;
			PID_ENTER_STATE(wp, yielding, ev);
			break;
		case RESCHED_D:		/* died */
			PID_ENTER_STATE(wp, dead, ev);
			break;
		default:
			PID_ENTER_STATE(wp, unknown, ev);
			break;
		}
	}
}

static void
wp_unwait(wait_pid_state *wp, tstamp_event_entry_t *ev)
{
	wait_lock_state *wl;

	wl = wp->wp_locking;
	wl_down(wl, ev->tstamp);
}

static void
pid_leaving_state(wait_pid_state *wp, tstamp_event_entry_t *ev)
{
#define LEAVE_STATE(s) case wp_ ## s : ws.ws_ ## s --; break

	switch (wp->wp_reason) {
	case wp_waiting:
		wp_unwait(wp, ev);
		break;
	}

	switch (wp->wp_reason) {
	LEAVE_STATE(running);
	LEAVE_STATE(preempted);
	LEAVE_STATE(yielding);
	LEAVE_STATE(waiting);
	LEAVE_STATE(stopped);
	LEAVE_STATE(dead);
	LEAVE_STATE(unknown);

	default:
#ifndef NDEBUG
		abort();
#endif /* !NDEBUG */
		break;
	}

#undef LEAVE_STATE
}

static void wait_unblock(tstamp_event_entry_t *ev)
{
	pid_t pid = (pid_t) ev->qual[0];
	wait_pid_state *wp;
	hashable *hash;

	if ((hash = htab_find(&ws.ws_pids, HASH_PID(pid), pid)) == NULL)
		return; /* XXX assume running */
	wp = HASHABLE_TO_WP(hash);

	PID_ENTER_STATE(wp, preempted, ev);
}

static void wait_run(tstamp_event_entry_t *ev)
{
	pid_t pid = (pid_t) ev->qual[0];
	wait_pid_state *wp;
	hashable *hash;

	if ((hash = htab_find(&ws.ws_pids, HASH_PID(pid), pid)) == NULL)
		return; /* XXX assume running */
	wp = HASHABLE_TO_WP(hash);

	PID_ENTER_STATE(wp, running, ev);
}

static void
wait_exit(tstamp_event_entry_t *ev)
{
	pid_t pid = (pid_t) ev->qual[0];
	wait_pid_state *wp;
	hashable *hash;

	if ((hash = htab_find(&ws.ws_pids, HASH_PID(pid), pid)) == NULL)
		return;	/* XXX missed one */
	wp = HASHABLE_TO_WP(hash);

	PID_ENTER_STATE(wp, dead, ev);

	htab_remove(&ws.ws_pids, HASH_PID(pid), hash);
	wp_free(&wp_freelist, wp);
}

/*ARGSUSED*/
int topwait_handle_event(top_state *ts, tstamp_event_entry_t *ev)
{
	switch (ev->evt) {
	case TSTAMP_EV_EXIT:		/* process exit */
		wait_exit(ev);
		break;
	case EVENT_TASK_STATECHANGE:	/* task suspend */
		wait_block(ev);
		break;
	case TSTAMP_EV_EODISP:		/* dispatch to runq */
		wait_unblock(ev);
		break;
	case TSTAMP_EV_EOSWITCH:
	case TSTAMP_EV_EOSWITCH_RTPRI:	/* switch in (runq to processor) */
		wait_run(ev);
		break;
	default:
        	return 0;
	}

        return 1;
}

static void
wait_init(wait_state *ws)
{
	htab_init(&ws->ws_pids, N_PID_BUCKETS);
	htab_init(&ws->ws_locks, N_LOCK_BUCKETS);
	ws->ws_running = 0;
	ws->ws_preempted = 0;
	ws->ws_yielding = 0;
	ws->ws_waiting = 0;
	ws->ws_stopped = 0;
	ws->ws_dead = 0;
	ws->ws_unknown = 0;
}

static sort_order_proto(order_load)
{
	const wait_lock_state *wla, *wlb;

	wla = *(wait_lock_state**)a;
	wlb = *(wait_lock_state**)b;

	return (wlb->wl_max_load - wla->wl_max_load);
}

static sort_order_proto(order_swtch)
{
	const wait_lock_state *wla, *wlb;

	wla = *(wait_lock_state**)a;
	wlb = *(wait_lock_state**)b;

	return (wlb->wl_nswtch - wla->wl_nswtch);
}

static filter_proto(wait_filter)
{
	const wait_lock_state *wl;

	wl = *(wait_lock_state**)obj;

	if (wl->wl_max_load == 0)
		return 0;

	if (show_type == SHOW_ALL_TYPES)
		return 1;
	else
		return (wl->wl_type == show_type);
}

/*ARGSUSED*/
void*
topwait_get_first(top_state *ts)
{
	hashable *wl_hash;
	wait_lock_state *wl;

	wl_hash = htab_begin(&ws.ws_locks);
	if (wl_hash == NULL)
		return NULL;

	wl = HASHABLE_TO_WL(wl_hash);
	return wl;
}

/*ARGSUSED*/
void*
topwait_get_next(top_state *ts, void *last)
{
	hashable *wl_hash;
	wait_lock_state *wl;

	wl = (wait_lock_state*) last;
	wl_hash = WL_TO_HASHABLE(wl);

	wl_hash = htab_next(&ws.ws_locks, HASH_LOCK(wl->wl_addr), wl_hash);
	if (wl_hash == NULL)
		return NULL;

	wl = HASHABLE_TO_WL(wl_hash);
	return wl;
}

static int
set_show_type(const char *type)
{
	wait_lock_type_t *wt;

	for (wt = lock_types; wt->wt_name != NULL; wt++)
		if (!strcmp(wt->wt_name, type))
			break;

	if (wt->wt_name != NULL) {
		show_type = wt->wt_type;
		return 1;
	}
	else
		return 0;
}

static void
display_init(void)
{
	sprintf(title, "%16s %16s %7s %4s %6s %7s",
		"WCHAN",
		"NAME",
		"TYPE",
		"LOAD",
		"%SWTCH",
		"SWT/SEC");
}

int
topwait_display_help_title(top_state *ts, int line_start)
{
  return term_draw_lines(top_term(ts), line_start, topwait_help_title);
}

int
topwait_display_help_text(top_state *ts, int line_start)
{
  return term_draw_lines(top_term(ts), line_start, topwait_help_text);
}

int
topwait_display_stats(top_state *ts, int line_start)
{
  int line;
  char linebuf[160];
  int nprocs = ws.ws_preempted + ws.ws_yielding + ws.ws_waiting
    + ws.ws_stopped + ws.ws_unknown;

  line = line_start;

#define proc_percent(type) int_percent(ws.ws_ ## type, nprocs)

  sprintf(linebuf, "%d processes: %d%% (%d) preempt, %d%% (%d) yield, "
          "%d%% (%d) wait,",
          nprocs,
          proc_percent(preempted), ws.ws_preempted,
          proc_percent(yielding), ws.ws_yielding,
          proc_percent(waiting), ws.ws_waiting);
  term_draw_line(top_term(ts), line++, linebuf);

  sprintf(linebuf, "               %d%% (%d) stop, %d%% (%d) unk",
          proc_percent(stopped), ws.ws_stopped,
          proc_percent(unknown), ws.ws_unknown);
  term_draw_line(top_term(ts), line++, linebuf);

#undef proc_percent

#define swtch_percent(type) \
    int_percent(ws.ws_nswtch. ## type ,ws.ws_nswtch.total)

  sprintf(linebuf, "%llu switches: %d%% (%llu) preempt, "
          "%d%% (%llu) wait, "
          "%d%% (%llu) yield",
          ws.ws_nswtch.total,
          swtch_percent(preempt), ws.ws_nswtch.preempt,
          swtch_percent(wait), ws.ws_nswtch.wait,
          swtch_percent(yield), ws.ws_nswtch.yield);
  term_draw_line(top_term(ts), line++, linebuf);

#undef swtch_percent

  return line - line_start;
}

int
topwait_display_objs(top_state *ts, int line_start)
{
  char linebuf[160];
  int ii;
  wait_lock_state **wl;
  double period;
  int line;

  line = line_start;

  term_draw_bold_line(top_term(ts), line++, title);

  wl = (wait_lock_state**) ts->ts_top_objs;
  period = (LAST_TSTAMP - FIRST_TSTAMP) / cycle_rate;

  for (ii = 0; ii < ts->ts_ntop_objs; ii++, wl++) {
    const char *locktype;
    double swtch_percent;
    int swtch_rate;

    switch ((*wl)->wl_type) {
    case SEMAWAIT:      locktype = "sema"; break;
    case MUTEXWAIT:     locktype = "mutex"; break;
    case SVWAIT:        locktype = "sv"; break;
    case MRWAIT:        locktype = "mrlock"; break;
    default:            locktype = "???"; break;
    }

    if (ws.ws_nswtch.total == 0)
      swtch_percent = 0;
    else
      swtch_percent = ((double) (*wl)->wl_nswtch) * 100 / ws.ws_nswtch.total;

    if (period)
	swtch_rate = (*wl)->wl_nswtch / period;
    else
	swtch_rate = 0;

    sprintf(linebuf, "%016llx %16s %7s %4d %6.1f %7d",
	    (*wl)->wl_addr,
	    (*wl)->wl_name,
	    locktype,
	    (*wl)->wl_max_load,
	    swtch_percent,
	    swtch_rate);

    term_draw_line(top_term(ts), line++, linebuf);
  }

  return line - line_start;
}

/*ARGSUSED*/
void
topwait_zero_interval_counters(top_state *ts)
{
	wait_lock_state *wl;
	hashable *wl_hash;

	for (wl_hash = htab_begin(&ws.ws_locks);
		wl_hash != NULL;
		wl_hash = htab_next(&ws.ws_locks,
			HASH_LOCK(wl->wl_addr), WL_TO_HASHABLE(wl)))
	{
		wl = HASHABLE_TO_WL(wl_hash);

		wl->wl_max_load = wl->wl_load;
		wl->wl_nswtch = 0;
		wl->wl_wait_total = 0;
	}

	memset(&ws.ws_nswtch, 0, sizeof(ws.ws_nswtch));
}

int
topwait_handle_keypress(top_state *ts, int ch)
{
	switch (ch) {
	case 't':
		{
			char line[80];

			term_getstr(top_term(ts), 0, top_prompt_line(ts),
				"Type to display",
				line, sizeof(line));
			if (!*line)
				break;

			if (set_show_type(line)) {
                          top_update(ts);
			}
			else {
                          top_error("Invalid lock type: %s", line);
			}
		}
        	break;
	default:
        	return 0;
	}

        return 1;
}

int
top_impl_init(top_state *ts)
{
	unsigned int cycle_period;

	FREELIST_INIT(&wp_freelist);
	FREELIST_INIT(&wl_freelist);

	wait_init(&ws);
	display_init();

	top_set_impl(ts, &topwait_impl);
	top_set_sort_order(ts, order_swtch);
	top_set_filter_out(ts, wait_filter);

	syssgi(SGI_QUERY_CYCLECNTR, &cycle_period);
	cycle_rate = 1.0E12/cycle_period;

        return 1;
}

#ifndef NDEBUG
void
topwait_lock_htab_dump(void)
{
	htab_dump(&ws.ws_locks, stdout);
}
#endif /* !NDEBUG */
