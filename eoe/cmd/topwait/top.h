#ifndef __TOP_H__
#define __TOP_H__

#include "data.h"
#include "term.h"

struct top_impl_s;

#define int_percent(n,total) (int) (((n) * 100) / ((total) != 0 ? (total) : 1))
#define float_percent(n,total) \
	(((double) (n) * 100) / ((total) != 0 ? (total) : 1))

#define sort_order_proto(name) int name (const void *a, const void *b)
typedef int (*order_f)(const void *a, const void *b);
#define filter_proto(name) int name (const void *obj)
typedef int (*filter_f)(const void *obj);

typedef struct sort_order_s {
	const char *so_name;
	order_f so_func;
} sort_order_t;

typedef struct msg_count_s {
	uint64_t mc_lost;
	uint64_t mc_recv;
	uint64_t mc_unhdl;
	uint64_t mc_ignored;
} msg_count_t;

typedef struct top_state_s {
	term_state ts_term;
	int ts_prompt_line;

	data_state ts_data;
	msg_count_t ts_msg_count;

	int ts_force_update;
	int ts_delay_secs;
	int ts_done;

	order_f ts_sort_order;
	filter_f ts_filter_out;

	void **ts_top_objs;
	int ts_ntop_objs;
	int ts_ntop_objs_max;

	struct top_impl_s *ts_impl;
} top_state;

/* Implementations *must* define this. */
int top_impl_init(top_state *ts);

typedef struct top_impl_s {
	void (*ti_destroy)(top_state *ts);
	void (*ti_usage)(void);

	/* NULL-terminated array of sorting functions. */
	const sort_order_t *ti_sort_orders;

	/* Rtmon event-mask - see util.c. */
	const char *ti_data_mask;

	/* Getopt-style options string - see getopt(3). */
	/* (may be NULL) */
	const char *ti_getopts;

	/* Iterate over valid objects for display. */
	void *(*ti_get_first)(top_state *ts);
	void *(*ti_get_next)(top_state *ts, void*);

	/* Zero any counters/state variables for next display. */
	void (*ti_zero_interval_counters)(top_state *ts);

	/* Returns number of lines used. */
	int (*ti_display_objs)(top_state *ts, int lineno);
	int (*ti_display_stats)(top_state *ts, int lineno);
	int (*ti_display_help_title)(top_state *ts, int lineno);
	int (*ti_display_help_text)(top_state *ts, int lineno);

	/* Returns TRUE if handled. */
	int (*ti_handle_event)(top_state *ts, tstamp_event_entry_t*);
	int (*ti_handle_keypress)(top_state *ts, int ch);
	int (*ti_handle_option)(top_state *ts, int opt, const char *optarg);

	/* called after option parsing is done */
	void (*ti_init)(top_state *ts);
} top_impl_t;

#define top_data(ts) (&(ts)->ts_data)
#define top_term(ts) (&(ts)->ts_term)
#define top_update(ts) { (ts)->ts_force_update = 1; }
#define top_set_delay(ts,d) { (ts)->ts_delay_secs = (d); }
#define top_prompt_line(ts) ((ts)->ts_prompt_line)
#define top_set_prompt_line(ts,pl) { (ts)->ts_prompt_line = (pl); }
#define top_set_sort_order(ts,so) { (ts)->ts_sort_order = (so); }
#define top_sort_order(ts) ((ts)->ts_sort_order)
#define top_set_filter_out(ts,fo) { (ts)->ts_filter_out = (fo); }
#define top_filter_out(ts) ((ts)->ts_filter_out)
#define top_done(ts) { (ts)->ts_done = 1; }
#define top_top_objs(ts) ((ts)->ts_top_objs)
#define top_ntop_objs(ts) ((ts)->ts_ntop_objs)
#define top_set_impl(ts,ti) { (ts)->ts_impl = (ti); }

void top_error(const char *fmt, ...);

extern uint64_t FIRST_TSTAMP;
extern uint64_t LAST_TSTAMP;

#endif /* __TOP_H__ */
