#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <signal.h>
#include <syslog.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/schedctl.h>
#include <sys/ioctl.h>
#include <sys/capability.h>
#include <unistd.h>
#include <math.h>
#include <getopt.h>

#include "cmd.h"
#include "load.h"
#include "mem.h"
#include "oobmsg.h"
#include "error.h"
#include "double_time.h"

#define DEFAULT_DECAY	0.25		/* Decay factor per second        */

#define CONTACT_PERIOD	120		/* seconds                        */
#define RELEASE_WAIT	3		/* seconds; see comment below     */

#define MAX_CONS_PCT	25		/* Max % of console B/W to use    */
#define MAX_UPDATE_FRQ	3.0

char		       *opt_device;
int			opt_console = 1;
int			opt_nodemode = 0;
int			opt_memmode = 0;
int			opt_graceful;
int			opt_gfx;
int			opt_meter;
char		       *opt_debug;
int			opt_baud;
double			opt_period;
double			opt_decay	= DEFAULT_DECAY;
char		       *load_title	= "Origin2000 System Activity";
char		       *mem_title	= "Origin2000 Memory Utilization";
char		       *opt_title	= NULL;
char		       *opt_title_gfx	= "Onyx2 System Activity";
int			opt_ncpus;	/* Undoc. ncpus debug override    */
int			opt_nofork;

mmsc_t		       *m;		/* NULL if not in contact w/ MMSC */
int			graph_started;	/* Flag                           */
int			protocol;	/* MMSC_PROTOCOL_VERSION number   */
int			ncpus;
int			nnodes;
int			bardim;		/* Bytes of data per bar          */
int			want_exit;
double			period;

void usage(void)
{
    perr("Usage:\n");
    perr("    mmscd [-c cdev] [-f sdev] [-b baud] [-u freq]\n");
    perr("          [-t \"title\"] [-N cpus] [-D fctr] [-gdlMmnG]\n");
    perr("Options:\n");
    perr("    -c cdev   Use alternate MMSC console device\n");
    perr("              (default /dev/console)\n");
    perr("    -f sdev   Use alternate MMSC non-console device\n");
    perr("    -b baud   Use baud rate other than the device default\n");
    perr("              (applicable only in conjunction with -f)\n");
    perr("    -g        When shutting down system, do it gracefully\n");
    perr("    -G        Include graphics information in CPU meter\n");
    perr("    -d file   Generate debug output to file\n");
    perr("    -m        Display CPU meter\n");
    perr("    -u freq   CPU meter update frequency in Hz (default is\n");
    perr("              chosen to use at most %d%% of console bandwidth)\n",
	 MAX_CONS_PCT);
    perr("    -D fctr   Decay factor per second (default %f)\n",
	 DEFAULT_DECAY);
    perr("    -t title  Specify alternate CPU meter title\n");
    perr("    -n        Do not fork into the background\n");
    perr("    -N cpus   Simulate an different # of CPUs (testing only)\n");
    perr("    -l        Operate in node mode\n");
    perr("    -M        Display node memory instead of CPU meter (implies -l)\n");
    fatal("Illegal command-line arguments\n");
}

/*
 * contact
 *
 *   Attempts to open the MMSC serial device and exchange some packets
 *   with the MMSC.  If successful, m will be non-NULL.
 *
 *   If silent is non-zero, contact() will not print an error message
 *   if contact is not made with the MMSC.
 *
 *   The global variable m is set to non-NULL if and only if contact is
 *   made.
 */

static void contact(int silent)
{
    debug("contact\n");

    if (m) {
	warning("Closing MMSC connection\n");

	/*
	 * If MMSC is already open, an error must have occurred, so now
	 * we're trying to re-establish contact.
	 *
	 * Allow the MMSC to time out in case it's sending a command.
	 * This also serves to resynchronize if anything goes wrong, such
	 * as a length field was corrupted and is absurdly large, etc.
	 * The MMSC's timeout is 2 seconds by default.
	 */

	mmsc_close(m);
	m = 0;

	double_sleep((double) RELEASE_WAIT);
    }

    /*
     * Open MMSC device
     */

    debug("open device\n");

    if ((m = mmsc_open(opt_device, opt_baud, opt_console)) == 0)
	fatal("Could not open MMSC device %s\n", opt_device);

    debug("ping\n");

    if (cmd_ping(m) < 0) {
	if (! silent) {
	    warning("Could not establish connection to MMSC\n");
	    warning("Retrying in %d seconds\n", CONTACT_PERIOD);

	    mmsc_warn(m);	/* Write warning message to console */
	}

	mmsc_close(m);
	m = 0;
	return;
    }

    if ((protocol = cmd_init(m)) < 0) {
	warning("Could not obtain protocol version\n");

	mmsc_close(m);
	m = 0;
	return;
    }

    debug("Using protocol version %d\n", protocol);
}

/*
 * drop
 *
 *   Disconnects from the MMSC.  Should be used if communication fails
 *   for any reason so that the inner loop will periodically try to
 *   contact the MMSC again.
 */

static void drop(void)
{
    if (m)
	mmsc_close(m);

    graph_started = 0;
    m = 0;
}

/*
 * note_exit
 *
 *   Signal handler for SIGINT and SIGTERM to achieve clean exit handling
 *   in case of these signals.
 *
 *   If one of these signals is received a second time, it will not be
 *   caught and the program will die.
 */

void note_exit(void)
{
    debug("received INT or TERM signal\n");

    want_exit = 1;
}

/*
 * cleanup
 */

void cleanup(void)
{
    if (m && graph_started) {
	debug("about to end graph\n");
	sleep(1);
	(void) cmd_graph_end(m);
	debug("done ending graph\n");
    }

    debug("releasing\n");
    drop();
}

/*
 * init_graph_mode
 */

static int init_graph_mode(void)
{
    int			r = -1;

    /*
     * Initialize graph mode
     */

    debug("init_graph_mode: ncpus: %u nodemode=%u\n", ncpus, opt_nodemode);

    if (cmd_graph_start(m, ncpus, bardim) < 0) {
	debug("Graph start failed\n");
	goto done;
    }

    if (cmd_graph_label(m, GRAPH_LABEL_TITLE,
			opt_gfx ? opt_title_gfx : opt_title) < 0)
	goto done;

    if (opt_nodemode) {
	if (cmd_graph_label(m, GRAPH_LABEL_HORIZ, "Node") < 0)
	    goto done; }
    else {
	if (cmd_graph_label(m, GRAPH_LABEL_HORIZ, "Processor") < 0)
	    goto done; }


    if (opt_memmode) {
	char memstr[16];

	if (cmd_graph_label(m, GRAPH_LABEL_VERT, "Node Memory") < 0)
	    goto done;

	if (cmd_graph_label(m, GRAPH_LABEL_MIN, "0") < 0)
	    goto done;

	sprintf(memstr, "%d", max_nodemem);
	if (cmd_graph_label(m, GRAPH_LABEL_MAX, memstr) < 0)
	    goto done;

	if (cmd_graph_label(m, GRAPH_LEGEND_ADD, "Kernel") < 0)
	    goto done;

	if (cmd_graph_label(m, GRAPH_LEGEND_ADD, "User") < 0)
	    goto done;

	if (cmd_graph_label(m, GRAPH_LEGEND_ADD, "Free") < 0)
	    goto done;

    } else {

	if (cmd_graph_label(m, GRAPH_LABEL_VERT, "Activity") < 0)
	    goto done;

	if (cmd_graph_label(m, GRAPH_LABEL_MIN, "0%") < 0)
	    goto done;

	if (cmd_graph_label(m, GRAPH_LABEL_MAX, "100%") < 0)
	    goto done;

	if (cmd_graph_label(m, GRAPH_LEGEND_ADD, "User") < 0)
	    goto done;

	if (cmd_graph_label(m, GRAPH_LEGEND_ADD, "System") < 0)
	    goto done;

	if (cmd_graph_label(m, GRAPH_LEGEND_ADD, "Intr") < 0)
	    goto done;

	if (cmd_graph_label(m, GRAPH_LEGEND_ADD, "IOwait") < 0)
	    goto done;

	if (opt_gfx && cmd_graph_label(m, GRAPH_LEGEND_ADD, "GFXwait") < 0)
	    goto done;
    }

    r = 0;

    debug("Graph initialized\n");

done:

    if (r < 0) {
	debug("Could not initialize graph\n");
	drop();
    } else
	graph_started = 1;

    return r;
}

int
main(int argc, char **argv)
{
    double		next = 0.0, now;
    u_char	       *p;
    int			c;
    cap_t		ocap;
    cap_value_t		cap_sched_mgt = CAP_SCHED_MGT;
    int 		ccpus;

    /* Open our syslog connection */

    openlog("mmscd", LOG_PID, LOG_DAEMON);

    while ((c = getopt(argc, argv, "D:GMN:b:d:c:f:glmnt:u:")) >= 0)
	switch (c) {
	case 'l':
	    opt_nodemode = 1;
	    break;
	case 'M':
	    opt_memmode = 1;	/* Display node memory instead of cpu usage. */
	    opt_nodemode = 1;	/* Node memory implies node display. */
	    break;
	case 'D':
	    opt_decay = atof(optarg);
	    break;
	case 'G':
	    opt_gfx = 1;
	    break;
	case 'N':	/* Undocumented */
	    opt_ncpus = atoi(optarg);
	    break;
	case 'b':
	    opt_baud = atoi(optarg);
	    break;
	case 'd':
	    opt_debug = optarg;
	    break;
	case 'c':
	    opt_device = optarg;
	    break;
	case 'f':
	    opt_device = optarg;
	    opt_console = 0;
	    break;
	case 'g':
	    opt_graceful = 1;
	    break;
	case 'm':
	    opt_meter = 1;
	    break;
	case 'n':
	    opt_nofork = 1;
	    break;
	case 't':
	    opt_title     = optarg;
	    opt_title_gfx = optarg;
	    break;
	case 'u':
	    opt_period = 1.0 / atof(optarg);
	    break;
	default:
	    usage();
	}

    if (optind != argc)
	fatal("Extra arguments\n");

    ccpus=sysmp(MP_NPROCS);
    if (ccpus > 128) {
	if (ccpus > 256) {
	    fatal("The MMSC firmware can only display 128 bars. Exiting.\n");
	}
	opt_nodemode++;
    }

    if (opt_memmode)
	bardim = MEM_DATA_BARS;
    else
	bardim = opt_gfx ? 5 : 4;

    if (opt_title == NULL)
	opt_title = (opt_memmode ? mem_title : load_title);

    debug_enable(opt_debug);

    /*
     * Fork into the background
     */

    if (! opt_nofork) {
	int		pid;

	if ((pid = fork()) < 0)
	    fatal("Could not fork: %s\n", STRERR);

	if (pid)
	    exit(0);

	ioctl(0, TIOCNOTTY);
	ioctl(1, TIOCNOTTY);
	ioctl(2, TIOCNOTTY);
    }

    /*
     * Set a non-degrading priority higher than normal procs, but lower
     * than anyone else who asks for such a priority.
     */

    ocap = cap_acquire(1, &cap_sched_mgt);
    schedctl(NDPRI, NDPHIMIN);
    cap_surrender(ocap);

    /*
     * Attempt to contact the MMSC for the first time.  If contact is
     * not made, assume there is no MMSC and exit.
     */

    silent_enable(1);
    contact(1);
    silent_enable(0);

    if (m == 0)
	exit(0);

    /*
     * Set up CPU load module
     */

    if (opt_ncpus)
	load_cpu_set(opt_ncpus);

    ncpus = load_cpu_get();

    if (! opt_graceful)
	warning("Clean power-down is not enabled (see chkconfig).\n");

    if (opt_nodemode) {
	nnodes = sysmp(MP_NUMNODES);
	if (get_nodemem_map())
	    fatal("Can't get node memory map.\n");
    }

    /*
     * Catch TERM and INT signals for clean exit.
     */

    signal(SIGTERM, note_exit);
    signal(SIGINT, note_exit);

    /*
     * Ignore SIGHUP for when we're shutting down and send SIGHUP
     * to all processes.
     */

    signal(SIGHUP, SIG_IGN);

    /*
     * Main polling loop, loops once every "period" seconds.
     *
     * This needs to be rewritten.  There must be one basic polling
     * rate for menu and power-down polling, and one basic rate for
     * graph updating, and a third for re-contacting the MMSC.
     */

    while (! want_exit) {
	debug("Main loop\n");

	if (m == 0) {
	    contact(0);
	    next = 0.0;
	}

	/*
	 * Recalculate update period (needs a connection).
	 */

	if (opt_period)
	    period = opt_period;
	else if (m == 0)
	    period = 2.0;
	else {
	    int		bits_avail;
	    int		bytes_per_update;
	    int		bits_per_update;
	    double	updates_per_sec;

	    bits_avail = mmsc_baud_get(m);
	    bytes_per_update = (5 + ncpus * bardim);
	    bits_per_update = 10 * bytes_per_update;
	    updates_per_sec = (bits_avail * MAX_CONS_PCT / 100.0) /
		bits_per_update;
	    if (updates_per_sec > MAX_UPDATE_FRQ)
		updates_per_sec = MAX_UPDATE_FRQ;
	    period = 1.0 / updates_per_sec;

	    debug("%d %d %d %lf\n",
		  bits_avail, bytes_per_update, bits_per_update,
		  updates_per_sec);
	}

	debug("period: %lf\n", period);

	if (next == 0.0)
	    next = double_time() + period;

	if (m && opt_meter && ! graph_started)
	    init_graph_mode();

	if (m && graph_started) {
	    int nelts;

	    if (opt_memmode) {
		debug("Getting memory graph\n");
		p = mem_graph();
		nelts = nnodes;
	    } else {
		debug("Getting load graph\n");
		p = load_graph(opt_gfx, opt_decay, period);
		nelts = ncpus;
	    }

	    if (p == 0) {
		static int x_graph_warn = 0;

		if (x_graph_warn == 0) {
		    warning("Could not obtain %s graph\n", 
			opt_memmode ? "memory" : "load");
		    x_graph_warn = 1;
		}
	    } else {
		if (cmd_graph_data(m, p, nelts * bardim, nelts) < 0)
			drop();
	    }
	}

	if (m == 0)
	    double_sleep((double) CONTACT_PERIOD);
	else {
	    now = double_time();

	    if (next < now)
		next = now;
	    else if (next > now)
		double_sleep(next - now);

	    next += period;
	}

	/*
	 * want_exit is checked right after the sleep so we'll
	 * see it immediately if the sleep system call is interrupted.
	 */
    }

    cleanup();
    return(0);
}
