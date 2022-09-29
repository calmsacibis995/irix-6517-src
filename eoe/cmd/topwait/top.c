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

#include "data.h"
#include "wait.h"
#include "term.h"
#include "freelist.h"
#include "top.h"
#include "util.h"

#define CONTROL(c)  ((c) & 0x1f)

top_state g_top;
char descr[128];
uint64_t FIRST_TSTAMP;
uint64_t LAST_TSTAMP;
const char *top_hostname = "localhost";
const char *top_cpulist = "0-";
const char *top_progname;
int top_ndisplays = 0;
int top_batchmode = 0;

static const char *help_text[] = {
  "These single-character commands are available:",
  "",
  "^L\t- redraw screen",
  "q\t- quit",
  "h or ?\t- help; show this text",
  "s\t- change number of seconds to delay between updates",
  NULL
};

static const char *help_prompt = "Hit any key to continue: ";

#define TOP_OBJS (g_top.ts_top_objs)
#define NTOP_OBJS (g_top.ts_ntop_objs)
#define NTOP_OBJS_MAX (g_top.ts_ntop_objs_max)
#define MSG_COUNT (g_top.ts_msg_count)
#define DATA (g_top.ts_data)
#define TERM (g_top.ts_term)
#define PROMPT_LINE (g_top.ts_prompt_line)
#define DONE (g_top.ts_done)
#define FORCE_UPDATE (g_top.ts_force_update)
#define SORT_ORDER (g_top.ts_sort_order)
#define FILTER_OUT (g_top.ts_filter_out)
#define DELAY_SECS (g_top.ts_delay_secs)
#define IMPL (*g_top.ts_impl)

static int
set_sort_order(const char *order)
{
	const sort_order_t *so;

	for (so = IMPL.ti_sort_orders; so->so_name != NULL; so++)
		if (!strcmp(so->so_name, order))
			break;

	if (so->so_name != NULL) {
		SORT_ORDER = so->so_func;
		return 1;
	}
	else
		return 0;
}

static void
zero_interval_counters(void)
{
  IMPL.ti_zero_interval_counters(&g_top);

  LAST_TSTAMP = FIRST_TSTAMP = 0;
  memset(&MSG_COUNT, 0, sizeof(MSG_COUNT));
}

static void
handle_event(tstamp_event_entry_t *ev)
{
	if (!FIRST_TSTAMP)
		FIRST_TSTAMP = ev->tstamp;

	MSG_COUNT.mc_recv++;

        switch (ev->evt) {
	case TSTAMP_EV_LOST_TSTAMP:	/* lost events */
        	MSG_COUNT.mc_lost += ev->qual[0];
		break;
        case EVENT_WIND_EXIT_IDLE:      /* idle runq */
        case TSTAMP_EV_SORECORD:        /* rtmon internal */
        case TSTAMP_EV_CONFIG:          /* cpu config */
        case TSTAMP_EV_TASKNAME:        /* process name */
        case TSTAMP_EV_FORK:            /* process fork */
        case TSTAMP_EV_EXEC:            /* process exec */
		/* If the implementation doesn't handle these, then
		   we'll consider them ignored. */
		if (!IMPL.ti_handle_event(&g_top, ev)) {
			MSG_COUNT.mc_ignored++;
		}
                break;
        default:
          if (!IMPL.ti_handle_event(&g_top, ev)) {
#ifndef NDEBUG
            log_event(ev);
#endif
            MSG_COUNT.mc_unhdl++;
          }
        }

	LAST_TSTAMP = ev->tstamp;
}

static void
show_help_screen(void)
{
  int line;
  const sort_order_t *orders;
  const char *prefix;
  char linebuf[128];
  char *linetmp;

  line = 0;
  term_clear(&TERM);

  line += IMPL.ti_display_help_title(&g_top, line);
  line++;

  line += term_draw_lines(&TERM, line, help_text);

  {
    linetmp = linebuf;
    linetmp += sprintf(linetmp, "o\t- specify sort order (");

    prefix = "";
    for (orders = IMPL.ti_sort_orders; orders->so_name != NULL; orders++)
      {
        linetmp += sprintf(linetmp, "%s%s", prefix, orders->so_name);
        prefix = ", ";
      }

    linetmp += sprintf(linetmp, ")");
    term_draw_line(&TERM, line++, linebuf);
  }

  if (IMPL.ti_display_help_text != NULL) {
	  line += IMPL.ti_display_help_text(&g_top, line);
  }
  line++;

  term_draw_bold_line(&TERM, line, help_prompt);
  term_goto(&TERM, strlen(help_prompt), line);

  term_update(&TERM);
  term_getch_wait(&TERM);

  term_goto(&TERM, 0, PROMPT_LINE);
  FORCE_UPDATE = 1;
}

static void
handle_keypress(int ch)
{
	switch (ch) {
	case 'h':
	case '?':
        	show_help_screen();
		break;
	case CONTROL('J'):
	case CONTROL('L'):
	case CONTROL('M'):
	case ' ':
		FORCE_UPDATE = 1;
		break;
	case 'q':
		DONE = 1;
		break;
	case 's':
		{
			uint64_t secs;
			char line[80];
			char *next;

			FORCE_UPDATE = 1;

			term_getstr(&TERM, 0, PROMPT_LINE,
				"Seconds to delay",
				line, sizeof(line));
			if (!*line)
				break;

			secs = strtol(line, &next, 10);
			if ((next == line && secs == 0) ||
				*next != '\0')
				break;

			DELAY_SECS = secs;
		}
		break;
	case 'o':
		{
			char line[80];

			term_getstr(&TERM, 0, PROMPT_LINE,
				"Order to sort",
				line, sizeof(line));
			if (!*line)
				break;

			if (set_sort_order(line)) {
				FORCE_UPDATE = 1;
			}
			else {
                          top_error("Invalid sort order: %s", line);
			}
		}
		break;
	default:
          if (IMPL.ti_handle_keypress == NULL ||
		!IMPL.ti_handle_keypress(&g_top, ch)) {
            top_error("Unknown command: '%c'", ch);
          }
          break;
	}
}

static void
display_init(void)
{
  struct utsname name;

  if (uname(&name) >= 0) {
    sprintf(descr, "%s %s %s %s %s",
            name.sysname,
            name.nodename,
            name.release,
            name.version,
            name.machine);
  }
  else {
    strcpy(descr, "NO SYSTEM INFORMATION AVAILABLE");
  }
}

static void
find_top(int nobjs, order_f cmp, filter_f filter)
{
	void *current;

	/* XXX This should probably use a heap to speed things up.
	*/

#define greater_than(a,b) ((*cmp)(&(a),&(b)) < 0)

	if (nobjs > NTOP_OBJS_MAX) {
		NTOP_OBJS_MAX = nobjs;
		TOP_OBJS = calloc(NTOP_OBJS_MAX, sizeof(void*));
	}

	NTOP_OBJS = 0;

	for (current = IMPL.ti_get_first(&g_top);
		current != NULL;
		current = IMPL.ti_get_next(&g_top, current))
	{
		if (!(*filter)(&current))
			continue;

		if (NTOP_OBJS < nobjs) {
			TOP_OBJS[NTOP_OBJS++] = current;
		}
		else {
			int ii;

			for (ii = 0; ii < NTOP_OBJS; ii++) {
				if (greater_than(current, TOP_OBJS[ii])) {
                                  TOP_OBJS[ii] = current;
				  break;
				}
			}
		}
	}

#undef greater_than

	if (NTOP_OBJS > 0)
		qsort(TOP_OBJS, NTOP_OBJS, sizeof(TOP_OBJS[0]), cmp);

#ifndef NDEBUG
	{
	int ii;

	for (ii = 1; ii < NTOP_OBJS; ii++)
	{
		if (TOP_OBJS[ii-1] == TOP_OBJS[ii]) {
			abort();
		}
	}
	}
#endif
}

static void
display_update(void)
{
  int nlines;
  int line;
  char linebuf[128];

  nlines = term_get_lines(&TERM);

  term_clear(&TERM);
  line = 0;

  term_draw_line(&TERM, line++, descr);

  if (IMPL.ti_display_stats != NULL)
	  line += IMPL.ti_display_stats(&g_top, line);

#define msg_percent(type) \
  int_percent(MSG_COUNT.mc_ ## type , MSG_COUNT.mc_recv )

  sprintf(linebuf, "rtmon: %llu recv, %llu lost, "
          "%d%% (%llu) unhdl, %d%% (%llu) ignored",
          MSG_COUNT.mc_recv,
          MSG_COUNT.mc_lost,
          msg_percent(unhdl), MSG_COUNT.mc_unhdl,
          msg_percent(ignored), MSG_COUNT.mc_ignored);
  term_draw_line(&TERM, line++, linebuf);

#undef msg_percent

  term_goto(&TERM, 0, line);
  PROMPT_LINE = line;

  term_draw_line(&TERM, line++, "");

  find_top(nlines - line - 1, SORT_ORDER, FILTER_OUT);
  line += IMPL.ti_display_objs(&g_top, line);

  term_update(&TERM);
}

/*ARGSUSED*/
static void handle_alarm(int sign)
{
	FORCE_UPDATE = 1;
}

static void alarm_init(void)
{
	struct sigaction sa;

	sa.sa_flags = SA_RESTART;
	sa.sa_handler = handle_alarm;

	if (sigaction(SIGALRM, &sa, NULL) < 0) {
		fprintf(stderr, "Unable to set alarm handler\n");
		exit(EXIT_FAILURE);
	}
}

static void alarm_set(void)
{
	alarm(DELAY_SECS);
}

void
top_error(const char *fmt, ...)
{
	char str[80];
	va_list ap;

	va_start(ap, fmt);
	vsprintf(str, fmt, ap);
	va_end(ap);

	term_draw_bold_line(&TERM, PROMPT_LINE, str);
	term_update(&TERM);
}

/*ARGSUSED*/
static void
sighnd_done(int sign)
{
	DONE = 1;
}

static void
catch_signals(void)
{
	struct sigaction sa;

	sa.sa_flags = 0;
	sa.sa_handler = sighnd_done;

	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGHUP, &sa, NULL);
}

static void
top_init(void)
{
  alarm_init();
  data_init(&DATA, top_hostname, top_cpulist, IMPL.ti_data_mask);

  catch_signals();
  term_init(&TERM, top_batchmode);

  term_goto(&TERM, 0, top_prompt_line(&g_top));
  alarm_set();

  display_init();
  zero_interval_counters();
}

static void
usage(void)
{
	fprintf(stderr, "\nusage: %s [options]\n\n", top_progname);
	fprintf(stderr, "Where [options] is one of:\n\n");
	fprintf(stderr,
"\t-h <hostname>         connect to rtmond running on machine <hostname>\n"
"\t-p <cpu-list>         observe cpus in <cpu-list>\n"
"\t-s <delay>            <delay> seconds between updates\n"
"\t-d <count>            show <count> updates and then exit\n"
"\t-n or -b              enter non-interactive (batch) mode\n"
	);

	if (IMPL.ti_usage != NULL)
		IMPL.ti_usage();

	fprintf(stderr, "\n");

	exit(EXIT_FAILURE);
}

static void
handle_options(int argc, char *argv[])
{
	int ch;
	char opts[128];

	DELAY_SECS = 2;
	PROMPT_LINE = 0;

	top_progname = strrchr(argv[0], '/');
	if (top_progname == NULL)
		top_progname = argv[0];
	else
		top_progname++;

	strcpy(opts, "h:p:s:d:bn");
	if (IMPL.ti_getopts != NULL)
		strcat(opts, IMPL.ti_getopts);

	while ((ch = getopt(argc, argv, opts)) != -1) {
		switch (ch) {
		case 'b':
		case 'n':
			top_batchmode = 1;
			break;
		case 'd':
			top_ndisplays = strtol(optarg, NULL, 0);
			if (top_ndisplays <= 0) {
				fprintf(stderr, "Illegal count: %s\n",
					optarg);
				usage();
			}
			break;
		case 'h':
			top_hostname = optarg;
			break;
		case 'p':
			top_cpulist = optarg;
			break;
		case 's':
			DELAY_SECS = strtol(optarg, NULL, 0);
			if (DELAY_SECS <= 0) {
				fprintf(stderr, "Illegal delay: %s\n",
					optarg);
				usage();
			}
			break;
		case '?':
		default:
			if (IMPL.ti_handle_option == NULL ||
				!IMPL.ti_handle_option(&g_top, ch, optarg)) {
				usage();
				/*NOTREACHED*/
			}
		}
	}
}

int
main(int argc, char *argv[])
{
	int nfds;
	fd_set readfds_orig;

	TOP_OBJS = NULL;
	NTOP_OBJS = 0;
	NTOP_OBJS_MAX = 0;
	FILTER_OUT = NULL;
	SORT_ORDER = NULL;

	if (!top_impl_init(&g_top))
		exit(EXIT_FAILURE);

	handle_options(argc, argv);
	if (IMPL.ti_init)
		IMPL.ti_init(&g_top);
        top_init();

	FD_ZERO(&readfds_orig);
	FD_SET(data_fd(&DATA), &readfds_orig);
	if (!top_batchmode)
		FD_SET(STDIN_FILENO, &readfds_orig);
	nfds = data_fd(&DATA) + 1;

	/* Filtering and sorting functions must be defined by
	   implementation. */
	assert(FILTER_OUT && SORT_ORDER);

	for (;;) {
		fd_set readfds = readfds_orig;

		select(nfds, &readfds, NULL, NULL, NULL);

		if (FD_ISSET(data_fd(&DATA), &readfds)) {
			tstamp_event_entry_t *evt;

			if (!data_read(&DATA))
				break;
			while (data_next_event(&DATA, &evt))
				handle_event(evt);
		}

		if (FD_ISSET(STDIN_FILENO, &readfds)) {
			int ch;

			if ((ch = term_getch(&TERM)) != 0)
				handle_keypress(ch);
		}

		if (DONE)
			break;

		if (FORCE_UPDATE) {
			display_update();
			zero_interval_counters();

			if (top_ndisplays != 0 && --top_ndisplays == 0)
				break;

			FORCE_UPDATE = 0;
			alarm_set();
		}
	}

	if (IMPL.ti_destroy != NULL)
		IMPL.ti_destroy(&g_top);

	term_destroy(&TERM);

	return EXIT_SUCCESS;
}
