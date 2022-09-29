#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/time.h>
#include <sys/par.h>
#include <sys/param.h>
#include <sys/prf.h>
#include <assert.h>

#include "data.h"
#include "term.h"
#include "freelist.h"
#include "top.h"
#include "prf.h"

typedef struct stacktrace_s {
	int st_nsamples;
	int st_state;
	uint64_t st_stack[PRF_MAXSTACK];
} stacktrace;

typedef struct func_sym_s {
	const char *fs_name;
	uint64_t fs_lowpc;
	uint64_t fs_child_hits;
	uint64_t fs_self_hits;
	uint64_t fs_sum_depth;
} func_sym_t;

#define sym_avg_depth(symp) ((symp)->fs_sum_depth / \
	((symp)->fs_child_hits + (symp)->fs_self_hits))

typedef struct addr_range_s {
	uint64_t ar_begin;
	uint64_t ar_end;
} addr_range_t;

typedef struct stack_stats_s {
	uint64_t ss_nstacks;
	uint64_t ss_sum_depth;
	uint64_t ss_max_depth;
} stack_stats_t;

static uint64_t total_hits = 0;
static stacktrace traces[MAXCPU];
static stack_stats_t stack_stats;

func_sym_t *syms = NULL;
uint64_t nsyms = 0;
uint64_t nsyms_max = 0;
#define NSYMS_BLOCK 1024

#define ST_KEEP			0x01
#define ST_DISCARD		0x02

#define MAX_FILTER_ADDRS 25
addr_range_t filter_out_addrs[MAX_FILTER_ADDRS];
uint64_t nfilter_out_addrs = 0;
addr_range_t filter_in_addrs[MAX_FILTER_ADDRS];
uint64_t nfilter_in_addrs = 0;

static sort_order_proto(order_self_hits);
static sort_order_proto(order_incl_hits);
static sort_order_proto(order_child_hits);
static sort_order_proto(order_depth);
static filter_proto(filter_func);

static sort_order_t topfunc_sort_orders[] = {
        "self",		order_self_hits,
	"child",	order_child_hits,
	"incl",		order_incl_hits,
	"depth",	order_depth,

	NULL, NULL
};

static void topfunc_usage(void);
static void *topfunc_get_first(top_state *ts);
static void *topfunc_get_next(top_state *ts, void*);
static void topfunc_zero_interval_counters(top_state *ts);
static int topfunc_display_objs(top_state *ts, int lineno);
static int topfunc_display_stats(top_state *ts, int lineno);
static int topfunc_display_help_title(top_state *ts, int lineno);
static int topfunc_display_help_text(top_state *ts, int lineno);
static int topfunc_handle_event(top_state *ts, tstamp_event_entry_t*);
static int topfunc_handle_keypress(top_state *ts, int ch);
static int topfunc_handle_option(top_state *ts, int opt, const char *optarg);
static void topfunc_init(top_state *ts);

static top_impl_t topfunc_impl = {
	NULL,	/* topfunc_destroy */
	topfunc_usage,
	topfunc_sort_orders,
	"profile",
	"u:",
        topfunc_get_first,
        topfunc_get_next,
        topfunc_zero_interval_counters,
        topfunc_display_objs,
        topfunc_display_stats,
        topfunc_display_help_title,
        topfunc_display_help_text,
        topfunc_handle_event,
        topfunc_handle_keypress,
        topfunc_handle_option,
	topfunc_init
};

static const char *topfunc_help_title[] = {
  "Topfunc version 1.0",
  "",
  "A top function usage display for Unix",
  NULL
};

static const char *topfunc_help_text[] = {
  "f\t- display only call-stacks containing function (\"none\" disables)",
  "F\t- filter out call-stacks containing function (\"none\" disables)",
  NULL
};

#define SIZEOF_QUAL (sizeof(((tstamp_event_entry_t*)NULL)->qual[0]))
#define NUM_STACK32_PER_QUAL (SIZEOF_QUAL / sizeof(uint32_t))
#define NUM_STACK32_PER_EVENT (NUM_STACK32_PER_QUAL * TSTAMP_NUM_QUALS)
#define NUM_STACK64_PER_QUAL (SIZEOF_QUAL / sizeof(uint64_t))
#define NUM_STACK64_PER_EVENT (NUM_STACK64_PER_QUAL * TSTAMP_NUM_QUALS)

static const char *namelist = "/unix";

static int
sym_lookup(uint64_t pc)
{
	int low, high, current;

	low = 0;
	high = nsyms;

	while (low < high) {
		current = ((high - low) / 2) + low;

		if (syms[current].fs_lowpc <= pc) {
			if (low == current)
				break;
			low = current;
		}
		else {
			high = current;
		}
	}

#ifndef NDEBUG
	{
		if (syms[current].fs_lowpc > pc)
			abort();

		if ((current + 1) >= nsyms)
			abort();

		if (syms[current+1].fs_lowpc < pc)
			abort();
	}
#endif

	return current;
}

static void
stack_commit(stacktrace *trace)
{
	int ii;
	uint64_t *sp;
	int bucket;

	stack_stats.ss_nstacks++;

	if (trace->st_nsamples > stack_stats.ss_max_depth)
		stack_stats.ss_max_depth = trace->st_nsamples;

	stack_stats.ss_sum_depth += trace->st_nsamples;

	sp = trace->st_stack;

	bucket = sym_lookup(*sp);
	syms[bucket].fs_self_hits++;
	syms[bucket].fs_sum_depth += (trace->st_nsamples - 1);
	sp++;

	for (ii = 1; ii < trace->st_nsamples; ii++, sp++)
	{
		bucket = sym_lookup(*sp);
		syms[bucket].fs_child_hits++;
		syms[bucket].fs_sum_depth += (trace->st_nsamples - ii - 1);
	}

	total_hits++;
}

/*ARGSUSED*/
static int
func_hit(tstamp_event_entry_t *ev, uint64_t addr_raw)
{
	addr_range_t *ar;
	stacktrace *trace = &traces[ev->cpu];
	int ii;
	uint64_t addr = (addr_raw & ~0x3ULL);

	if (addr_raw & PRF_STACKSTART) {
		trace->st_nsamples = 0;
		trace->st_state = 0;
	}

	/* Note that syms[nsyms-1] should be _etext! */
	assert(addr >= syms[0].fs_lowpc && addr < syms[nsyms-1].fs_lowpc);

	if (trace->st_state & ST_DISCARD)
		return 0;

	/* Only allow call-stacks containing functions in
	   filter_in_addrs.
	*/
	for (ii = 0, ar = filter_in_addrs; ii < nfilter_in_addrs; ii++, ar++)
		if (addr >= ar->ar_begin && addr <= ar->ar_end)
			trace->st_state |= ST_KEEP;

	/* Only allow call-stacks not containing functions in
	   filter_out_addrs.
	*/
	for (ii = 0, ar = filter_out_addrs; ii < nfilter_out_addrs; ii++, ar++)
	{
		if (addr >= ar->ar_begin && addr <= ar->ar_end)
		{
			trace->st_state |= ST_DISCARD;
			return 0;
		}
	}

	trace->st_stack[trace->st_nsamples] = addr;
	trace->st_nsamples++;

	if (addr_raw & PRF_STACKEND) {
		if (nfilter_in_addrs && !(trace->st_state & ST_KEEP))
			return 0;

		stack_commit(trace);

		return 0;
	}

	return 1;
}

static void
handle_stack32_event(tstamp_event_entry_t *ev)
{
	int npc;
	uint32_t *sptr;

	for (sptr = (uint32_t*) ev->qual, npc = 0;
		npc < NUM_STACK32_PER_EVENT; npc++, sptr++)
	{
		if (!func_hit(ev, (uint64_t) *sptr))
			return;
	}
}

static void
handle_stack64_event(tstamp_event_entry_t *ev)
{
	int npc;
	uint64_t *sptr;

	for (sptr = (uint64_t*) ev->qual, npc = 0;
		npc < NUM_STACK64_PER_EVENT; npc++, sptr++)
	{
		if (!func_hit(ev, (uint64_t) *sptr))
			return;
	}
}

/*ARGSUSED*/
int
topfunc_handle_event(top_state *ts, tstamp_event_entry_t *ev)
{
	switch (ev->evt) {
	case TSTAMP_EV_PROF_STACK64:
		handle_stack64_event(ev);
		break;
	case TSTAMP_EV_PROF_STACK32:
		handle_stack32_event(ev);
		break;
        default:
        	return 0;
	}

        return 1;
}

static sort_order_proto(order_incl_hits)
{
	func_sym_t *fsa = *(func_sym_t**)a;
	func_sym_t *fsb = *(func_sym_t**)b;
	return ((int64_t) (fsb->fs_self_hits + fsb->fs_child_hits)
		- (fsa->fs_self_hits + fsa->fs_child_hits));
}

static sort_order_proto(order_self_hits)
{
	func_sym_t *fsa = *(func_sym_t**)a;
	func_sym_t *fsb = *(func_sym_t**)b;
	return ((int64_t) fsb->fs_self_hits - fsa->fs_self_hits);
}

static sort_order_proto(order_child_hits)
{
	func_sym_t *fsa = *(func_sym_t**)a;
	func_sym_t *fsb = *(func_sym_t**)b;
	return ((int64_t) fsb->fs_child_hits - fsa->fs_child_hits);
}

static sort_order_proto(order_depth)
{
	func_sym_t *fsa = *(func_sym_t**)a;
	func_sym_t *fsb = *(func_sym_t**)b;
	return ((int64_t) sym_avg_depth(fsb) - sym_avg_depth(fsa));
}

/*ARGSUSED*/
static filter_proto(filter_func)
{
	func_sym_t *fs = *(func_sym_t**)obj;
	return !(fs->fs_child_hits == 0 && fs->fs_self_hits == 0);
}

static char title[128];
static const char *title_fmt = "%40s %6s %6s %6s";
static const char *line_fmt =  "%40s %5.1f%% %5.1f%% %6llu";

static void
display_init(void)
{
  sprintf(title, title_fmt,
	  "FUNC",
          "SELF",
          "CHILD",
	  "DEPTH");
}

static int top_iter_index = 0;

/*ARGSUSED*/
static void*
topfunc_get_first(top_state *ts)
{
	top_iter_index = 0;
	return &syms[top_iter_index];
}

/*ARGSUSED*/
static void*
topfunc_get_next(top_state *ts, void *last)
{
	if (++top_iter_index == nsyms)
		return NULL;

	return &syms[top_iter_index];
}

int
topfunc_display_help_title(top_state *ts, int line_start)
{
  return term_draw_lines(top_term(ts), line_start, topfunc_help_title);
}

/*ARGSUSED*/
int
topfunc_display_help_text(top_state *ts, int line_start)
{
  return term_draw_lines(top_term(ts), line_start, topfunc_help_text);
}

/*ARGSUSED*/
int
topfunc_display_stats(top_state *ts, int line_start)
{
	int line = line_start;
	char linebuf[128];
	double avg_depth;

	if (stack_stats.ss_nstacks == 0)
		avg_depth = 0;
	else
		avg_depth = ((double) stack_stats.ss_sum_depth)
			/ stack_stats.ss_nstacks;

	sprintf(linebuf, "%llu stacks: %.1f avg depth, %llu max depth",
		stack_stats.ss_nstacks,
		avg_depth,
		stack_stats.ss_max_depth);
	term_draw_line(top_term(ts), line++, linebuf);

	return line - line_start;
}

int
topfunc_display_objs(top_state *ts, int line_start)
{
	int line;
	char linebuf[160];
	int ii;
	func_sym_t **fs;

        line = line_start;

        term_draw_bold_line(top_term(ts), line++, title);

	fs = (func_sym_t**) ts->ts_top_objs;

	for (ii = 0; ii < ts->ts_ntop_objs; ii++, fs++) {
		assert((*fs)->fs_name != NULL);

		sprintf(linebuf, line_fmt,
			(*fs)->fs_name,
			float_percent((*fs)->fs_self_hits, total_hits),
			float_percent((*fs)->fs_child_hits, total_hits),
			sym_avg_depth(*fs));

		term_draw_line(top_term(ts), line++, linebuf);
	}

        return line - line_start;
}

/*ARGSUSED*/
void
topfunc_zero_interval_counters(top_state *ts)
{
	int ii;
	func_sym_t *fs;

	for (fs = syms, ii = 0; ii < nsyms; ii++, fs++) {
		fs->fs_child_hits = 0;
		fs->fs_self_hits = 0;
		fs->fs_sum_depth = 0;
	}

	stack_stats.ss_max_depth = 0;
	stack_stats.ss_nstacks = 0;
	stack_stats.ss_sum_depth = 0;

	total_hits = 0;
}

static int
add_function_filter(const char *fn, addr_range_t *filter, uint64_t *nfilter)
{
	func_sym_t *fs;
	int ii;
	uint64_t end;

	if (!strcmp("none", fn)) {
		*nfilter = 0;
		return 1;
	}

	for (fs = syms, ii = 0; ii < nsyms; ii++, fs++) {
		if (!strcmp(fs->fs_name, fn))
			break;
	}
	if (ii == nsyms) {
		top_error("Function not found: %s", fn);
		return 0;
	}

	if ((ii + 1) == nsyms)
		end = (uint64_t) -1;
	else
		end = (fs+1)->fs_lowpc - 1;

	if (((*nfilter) + 1) == MAX_FILTER_ADDRS) {
		top_error("Maximum number of functions reached.");
		return 0;
	}

	filter[*nfilter].ar_begin = fs->fs_lowpc;
	filter[*nfilter].ar_end = end;
	(*nfilter)++;

	return 1;
}

/*ARGSUSED*/
int
topfunc_handle_keypress(top_state *ts, int ch)
{
	switch (ch) {
	case 'F':
	case 'f':
		{
                        char line[80];
			const char *prompt;
			uint64_t *nfilter;
			addr_range_t *filter;

			if (ch == 'F') {
				prompt = "Function to filter out";
				filter = filter_out_addrs;
				nfilter = &nfilter_out_addrs;
			}
			else {
				prompt = "Function to display";
				filter = filter_in_addrs;
				nfilter = &nfilter_in_addrs;
			}

                        term_getstr(top_term(ts), 0, top_prompt_line(ts),
                                prompt,
                                line, sizeof(line));
                        if (!*line)
                                break;

			if (add_function_filter(line, filter, nfilter)) {
				top_update(ts);
			}
		}
		break;
	default:
		return 0;
	}

	return 1;
}

static void
topfunc_usage(void)
{
	fprintf(stderr,
"\t-u <unix>             use <unix> as kernel binary for symbols\n"
	);
}

/*ARGSUSED*/
static int
topfunc_handle_option(top_state *ts, int opt, const char *optarg)
{
	switch (opt) {
	case 'u':
		namelist = optarg;
		break;

	case '?':
	default:
		return 0;
	}
	return 1;
}

static void
sym_add(char *name, symaddr_t lowpc)
{
	assert(name != NULL);

	if (nsyms == nsyms_max) {
		nsyms_max += NSYMS_BLOCK;
		syms = (func_sym_t*) realloc(syms,
			sizeof(func_sym_t) * nsyms_max);
		if (syms == NULL) {
			perror("realloc");
			exit(EXIT_FAILURE);
		}
	}

	syms[nsyms].fs_name = name;
	syms[nsyms].fs_lowpc = lowpc;
	nsyms++;
}

static int
sym_cmp(const void *a, const void *b)
{
	const func_sym_t *fsa = (func_sym_t*)a;
	const func_sym_t *fsb = (func_sym_t*)b;
	return (fsa->fs_lowpc - fsb->fs_lowpc);
}

static void
sym_init(const char *objfile)
{
	int fd;
	int dwarf_failed;

	fd = open(objfile, O_RDONLY);
	if (fd < 0) {
		perror("unable to open namelist file");
		exit(EXIT_FAILURE);
	}

	dwarf_failed = rdsymtab(fd, sym_add);

	if (rdelfsymtab(fd, sym_add) && dwarf_failed) {
		fprintf(stderr, "unable to read elf or dwarf symtab of "
			"%s\n", objfile);
		exit(EXIT_FAILURE);
	}

	if (nsyms != 0)
		qsort(syms, nsyms, sizeof(func_sym_t), sym_cmp);
}

int
list_repeated(symaddr_t addr)
{
	func_sym_t *fs;
	int ii;

	for (fs = syms, ii = 0; ii < nsyms; ii++, fs++)
		if (fs->fs_lowpc == addr)
			return 1;

	return 0;
}

int
top_impl_init(top_state *ts)
{
	display_init();

	top_set_impl(ts, &topfunc_impl);
	top_set_sort_order(ts, order_incl_hits);
	top_set_filter_out(ts, filter_func);

        return 1;
}

/*ARGSUSED*/
static void
topfunc_init(top_state *ts)
{
	sym_init(namelist);
}
