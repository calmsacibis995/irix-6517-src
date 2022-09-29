/* dump PPP stuff
 */

#ident "$Revision: 1.21 $"

#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <stdio.h>
#include <bstring.h>
#include <curses.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <termios.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#undef TRUE				/* uucp.h redefines TRUE & FALSE */
#undef FALSE
#include "pppinfo.h"

extern char pppinfo_err[];


static WINDOW *win;
static int y, x;
#define TOP	2
#define COL_WIDTH 26
#define COL1	0
#define COL2	26
#define COL3	52
#define BOS	(LINES-1)		/* bottom of screen */

#define LABEL_WIDTH	14
#define MAX_FIELD_WIDTH	11
#define NOMINAL_FIELD_WIDTH 3
#define NUM_VALUES	8		/* maximum values/field */


static struct termio tb;
#define INTRCHAR tb.c_cc[VINTR]
#define SUSPCHAR tb.c_cc[VSUSP]

static time_t mode_date;

char *pgmname;

static int interval = 1;
static int unit;
static int unit_delta;
static int fail_cnt = 0;
#define FATAL_FAIL 45
static int first = 2;
static int needfoot;			/* need to repaint footing */
static char scrn_no = '5';
static enum {
	D_MODE=0,
	Z_MODE=1,
	R_MODE=2
} mode;

static struct ppp_status base, cur;
#define C_BUNDLE cur.ps_pi.pi_bundle
#define C_KDEVS cur.ps_pi.pi_devs
#define C_DDEVS cur.ps_devs
#define B_BUNDLE base.ps_pi.pi_bundle
#define B_KDEVS base.ps_pi.pi_devs
#define B_DDEVS base.ps_devs

static int indeces[MAX_PPP_LINKS], old_indeces[MAX_PPP_LINKS];

static char lochost_name[4*3+3+1];
static char remhost_name[4*3+3+1];

static void quit(int);
static void sick(char *, ...);
#if 0
static void fail(char *, ...);
#endif
static void sig_winch(int);
static void usage(void);
static char* hms(int);
static void pval(int, char *, char *);
static char* vtos(char *, ...);
static void set_base(void);
static void clear_base(void);
static void dis_link(char *, int);
static void dis_io(char *);
static void dis_acct(char *);
static void dis_counts(char *);
static int input_wait(void);
static void paint(void);
static void banner(char *, time_t);
static void prnt(char *, ...);

void
main(int argc,
     char **argv)
{
	int i, c;
	char *p;


	pgmname = argv[0];

	while ((i = getopt(argc, argv, "nu:i:m:")) != EOF)
		switch (i) {
		case 'u':
			i = (int)strtoul(optarg,&p,0);
			if (*p != '\0' || i < 0 || i > MAX_PPP_DEVS)
				usage();
			if (0 > pppinfo_next_unit(i,0,&cur)) {
				fprintf(stderr, "%s: no such PPP unit %s\n",
					pgmname, optarg);
				exit(1);
			}
			break;
		case 'i':
			interval = (int)strtoul(optarg,&p,0);
			if (*p != '\0' || interval == 0)
				usage();
			break;
		case 'm':
			if (strlen(optarg) != 1)
				usage();
			switch (*optarg) {
			case 'd': mode = D_MODE; break;
			case 'r': mode = R_MODE; break;
			case 'z': mode = Z_MODE; break;
			default: usage(); break;
			}
			break;
		default:
			usage();
			break;
	}
	if (argc != optind)
		usage();

	(void) ioctl(0, TCGETA, &tb);

	win = initscr();
	signal(SIGINT, quit);
	signal(SIGTERM, quit);
	signal(SIGHUP, quit);
	(void)signal(SIGWINCH, sig_winch);

	cbreak();
	noecho();
	keypad(stdscr, TRUE);
	leaveok(stdscr, FALSE);
	nonl();
	intrflush(stdscr,FALSE);
	move(0, 0);

	for (;;) {
		if (first != 0) {
			clear();
			needfoot = 1;
			fail_cnt = 0;
		}


		if (unit < 0
		    && 0 > (unit = pppinfo_next_unit(0, unit_delta, &cur))) {
			sick("no active PPP connnections");
		} else if (0 > pppinfo_get(unit, &cur)) {
			sick(pppinfo_err);
		} else {
			fail_cnt = 0;
			paint();
		}

		if (!input_wait())
			continue;

		c = getch();
		switch (c) {
		case 'q':
		case 'Q':
			quit(0);
			break;
		case 'd':
		case 'D':
			first = 2;
			mode = D_MODE;
			break;
		case 'z':
		case 'Z':
			first = 2;
			mode = Z_MODE;
			break;
		case 'r':
		case 'R':
			first = 2;
			mode = R_MODE;
			break;
		case '\f':
			if (!first)
				first = 1;
			break;
		case '+':
		case '=':		/* in case of typos */
			unit = pppinfo_next_unit(unit, unit_delta = 1, &cur);
			first = 2;
			break;
		case '-':
		case '_':		/* in case of typos */
			unit = pppinfo_next_unit(unit, unit_delta = -1, &cur);
			first = 2;
			break;
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
			scrn_no = c;
			if (!first)
				first = 1;
			break;
		default:
			if (c == INTRCHAR)
				quit(0);
			if (c == SUSPCHAR) {
				reset_shell_mode();
				kill(getpid(), SIGTSTP);
				reset_prog_mode();
			}
			/* repaint screen on unrecognized commands */
			if (!first)
				first = 1;
			break;
		}
	}
}


/* wait for a second or until something is typed
 */
static int
input_wait(void)
{
	int i;
	struct timeval wait;
	fd_set rmask;


	/* Try to delay for exactly the right interval,
	 * adjusted for time spent displaying things.
	 * Display at the middle of the correct second.
	 */
	(void)gettimeofday(&wait, 0);
	wait.tv_sec = interval;
	wait.tv_usec += 250000;
	if (wait.tv_usec > 1000000)
		wait.tv_usec -= 1000000;
	if (wait.tv_usec != 0) {
		wait.tv_usec = 1000000 - wait.tv_usec;
		if (wait.tv_usec >= 1000000/5)
			wait.tv_sec--;
	}
	FD_ZERO(&rmask);
	FD_SET(0, &rmask);
	i = select(1, &rmask, 0, 0, &wait);
	if (i < 0) {
		if (errno != EAGAIN && errno != EINTR)
			sick("select: %s", strerror(errno));
		return 0;
	}
	return i;
}


/* paint the screen
 */
static void
paint(void)
{
	char cmds[80];
	int i, j, same_dev, active_dev;


	/* If the interface is new, reset everything */
	if (cur.ps_pid != base.ps_pid)
		first = 2;

	/* recompute map of link # to STREAMS mux device */
	for (j = 0; j < MAX_PPP_LINKS; j++)
		indeces[j] = -1;
	same_dev = 0;
	active_dev = 0;
	for (i = 0; i < MAX_PPP_LINKS; i++) {
		if (C_KDEVS[i].dev_link.pl_max_frag <= 0 )
			continue;
		j = C_KDEVS[i].dev_link.pl_link_num;
		if (j >= 0 && j < MAX_PPP_LINKS)
			indeces[j] = i;
		active_dev++;

		/* notice in passing links that have come or gone */
		if ((B_KDEVS[i].dev_link.pl_index
			 != C_KDEVS[i].dev_link.pl_index)
		    || old_indeces[j] != i) {
			bcopy(&C_KDEVS[i], &B_KDEVS[i], sizeof(B_KDEVS[i]));
			bcopy(&C_DDEVS[j], &B_DDEVS[j], sizeof(B_DDEVS[j]));
			if (first == 0)
				first = 1;
			needfoot = 1;
		} else {
			same_dev++;
		}
	}
	if (active_dev > 0 && same_dev == 0)
		bcopy(&cur.ps_pi.pi_vj_tx, &base.ps_pi.pi_vj_tx,
		      sizeof(base.ps_pi.pi_vj_tx));

	/* if the map has changed, recompute the footing */
	if (bcmp(&indeces[0], &old_indeces[0], sizeof(old_indeces))) {
		bcopy(&indeces[0], &old_indeces[0], sizeof(old_indeces));
		if (!first)
			first = 1;
		needfoot = 1;
	}

	if (needfoot || first != 0) {
		for (i = 0, j = 0; j < MAX_PPP_LINKS; j++) {
			if (indeces[j] < 0)
				continue;
			i += sprintf(&cmds[i], "%s%d", (i ? ",": ""), j+1);
		}
		strcpy(&cmds[i], i ? ":link" : "");

		move(BOS, 0);
		clrtoeol();
		standout();
		printw("%-8.8s %-50.50s %25.25s",
		       cmds, "  5:I/O  6:acct  7:counts",
		       "+/-:Interface   DZRN:Mode");
		standend();
		needfoot = 0;
	}

	if (first > 1) {
		strcpy(lochost_name, inet_ntoa(cur.ps_lochost));
		strcpy(remhost_name, inet_ntoa(cur.ps_remhost));

		switch (mode) {
		case D_MODE:
			set_base();
			mode_date = cur.ps_initial_date;
			break;
		case Z_MODE:
			set_base();
			mode_date = time(0);
			break;
		case R_MODE:
			clear_base();
			mode_date = cur.ps_initial_date;
			break;
		}
	}

	switch (scrn_no) {
	case '1':
		dis_link("1:link 1", 0);
		break;
	case '2':
		dis_link("2:link 2", 1);
		break;
	case '3':
		dis_link("3:link 3", 2);
		break;
	case '4':
		dis_link("4:link 4", 3);
		break;
	case '5':
		dis_io("5:I/O");
		break;
	case '6':
		dis_acct("6:acct");
		break;
	case '7':
		dis_counts("7:counts");
		break;
	}

	/* paint the screen, leaving the cursor at the top */
	move(0, 0);
	refresh();

	first = 0;
	if (mode == D_MODE)
		set_base();
}


static void
banner(char *scrn_name,
       time_t date)
{
	char m;
	char init_date_str[sizeof("mm/dd hh:mm:ss to ")+1];
	char cur_time_str[sizeof("HH:MM:SS")+1];


	switch (mode) {
	case D_MODE:
		m = 'D';
		break;

	case Z_MODE:
		m = 'Z';
		date = mode_date;
		break;

	case R_MODE:
		m = 'R';
		date = mode_date;
		break;
	}

	if (date == 0) {
		init_date_str[0] = '\0';
	} else {
		(void)cftime(init_date_str,"%m/%d %T to ",&date);
	}
	(void)cftime(cur_time_str,"%T",&cur.ps_cur_date);

	x = 0;
	y = 0;
	prnt("%-8s  ppp%-2d %15.15s %-18s %*s%c: %s%s",
	     scrn_name, unit, cur.ps_rem_sysname, remhost_name,
	     date ? 0 : sizeof(init_date_str)-2, "",
	     m, init_date_str, cur_time_str);

	x = COL1;
	y = TOP;
}


#define DELTA(nm)		(cur.nm - base.nm)
#define PB_METER_DELTA(nm)	DELTA(ps_pi.pi_bundle.pb_meters.nm)
#define PF_METER_DELTA(n,nm)	DELTA(ps_pi.pi_devs[n].dev_meters.nm)
#define PL_DELTA(n,nm)		DELTA(ps_pi.pi_devs[n].dev_link.nm)
#define PS_DEV_DELTA(nm)	(C_DDEVS[link].nm - B_DDEVS[link].nm)

#define SNUM(v)			vtos("%3d", v)
#define SSEC(v)			(v <= (5*60) ? vtos("%3d sec", v) : hms(v))
#define SBYTE(v)		vtos("%3d", v)
#define SRATIO(n,d)		((d)!=0 ? vtos("%3.1f:1", (n)*1.0/(d)) : "")
#define SKBPS(v)		((v)!=0 ? vtos("%3.1f kbps", (v)/1000.0) : "")


static void
pr_vj_tx(int sw)
{
	if (cur.ps_pi.pi_vj_tx.tx_states == 0)
		sw = -1;
	pval(sw, "VJ TX total",	    SNUM(DELTA(ps_pi.pi_vj_tx.cnt_packets)));
	pval(sw, "   compressed",  SNUM(DELTA(ps_pi.pi_vj_tx.cnt_compressed)));
	pval(sw, "   searches",	    SNUM(DELTA(ps_pi.pi_vj_tx.cnt_searches)));
	pval(sw, "   misses",	    SNUM(DELTA(ps_pi.pi_vj_tx.cnt_misses)));
}



static void
pr_vj_rx(int sw,
	 int link)
{
#define V(n) ps_pi.pi_devs[n].dev_vj_rx
	int i = indeces[link];

	if (i < 0 || cur.V(i).rx_states == 0)
		sw = -1;
	pval(sw," RX compressed",SNUM(DELTA(V(i).cnt_compressed)));
	pval(sw,"   incomp",	SNUM(DELTA(V(i).cnt_uncompressed)));
	pval(sw,"   tossed",	SNUM(DELTA(V(i).cnt_tossed)));
#undef V
}



static void
pr_bsd_db(int sw,
	  int tx,			/* 1=TX, 0=RX */
	  int link,
	  int indx)
{
#define BSD_DELTA(nm) (cur_db->nm - base_db->nm)
	struct bsd_db *cur_db, *base_db;
	int max_bits, comp, raw;


	if (tx) {
		cur_db = &C_KDEVS[indx].dev_tx_db.bsd;
		base_db = &B_KDEVS[indx].dev_tx_db.bsd;
	} else {
		cur_db = &C_KDEVS[indx].dev_rx_db.bsd;
		base_db = &B_KDEVS[indx].dev_rx_db.bsd;
	}

	/* figure out the size of the code word */
	for (max_bits = MIN_BSD_BITS;
	     (1<<max_bits) <= cur_db->maxmaxcode && max_bits < MAX_BSD_BITS;
	     max_bits++)
		continue;

	pval(sw, tx ? "TX BSD bits" : "RX BSD bits", SNUM(max_bits));
	pval(sw,    " cur bits",	SNUM(cur_db->n_bits));
	pval(sw,    " max code",	SNUM(cur_db->max_ent));
	comp = BSD_DELTA(bytes_out)+BSD_DELTA(prev_bytes_out);
	raw = BSD_DELTA(in_count)+BSD_DELTA(prev_in_count);
	pval(sw,    " ratio",		SRATIO(raw,comp));
	pval(sw,    " ckpoint",		SNUM(cur_db->checkpoint));
	pval(sw,    " seq #",		SNUM(cur_db->seqno));
	pval(sw,    " cleared",		SNUM(cur_db->clear_count));
	pval(sw,    " incomp pkts",	SNUM(BSD_DELTA(incomp_count)));
	if (tx) {
		pval(sw," reset peer",	SNUM(PS_DEV_DELTA(ccp_rreq_rcvd)));
	} else {
		pval(sw," reset peer",	SNUM(PS_DEV_DELTA(ccp_rack_rcvd)));
		pval(sw," buf over",	SNUM(BSD_DELTA(overshoot)));
		pval(sw,"   undershoot",SNUM(BSD_DELTA(undershoot)));
	}

#undef BSD_DELTA
}


static void
pr_pred_db(int sw,
	   int tx,			/* 1=TX, 0=RX */
	   int link,
	   int indx)
{
#define PRED_DELTA(nm) (cur_db->nm - base_db->nm)
	struct pred_db_info *cur_db, *base_db;
	int comp, raw;


	if (tx) {
		cur_db = &C_KDEVS[indx].dev_tx_db.pred,
		base_db = &B_KDEVS[indx].dev_tx_db.pred;
	} else {
		cur_db = &C_KDEVS[indx].dev_rx_db.pred,
		base_db = &B_KDEVS[indx].dev_rx_db.pred;
	}

	comp = PRED_DELTA(comp);
	raw = PRED_DELTA(uncomp);
	pval(sw, tx ? "TX Pred ratio" : "RX Pred ratio", SRATIO(raw,comp));
	pval(sw,	"  incomp pkts",SNUM(PRED_DELTA(incomp)));
	if (tx) {
		pval(sw,"  reset peer",	SNUM(PS_DEV_DELTA(ccp_rreq_rcvd)));
	} else {
		pval(sw,"  reset peer",	SNUM(PS_DEV_DELTA(ccp_rreq_rcvd)));
		pval(sw,"  padding",	SNUM(PRED_DELTA(padding)));
	}

#undef PRED_DELTA
}


static void
pr_comp(int sw,
	int link)			/* -1 for bundle */
{
	int indx;
	int save_x, save_y, ret_y;

	if (link < 0) {
		link = 0;
		if (!(C_BUNDLE.pb_state & PB_ST_MP)
		    && cur.ps_maxdevs > 1) {
			indx = -1;
		} else {
			indx = indeces[0];
		}
	} else {
		if ((C_BUNDLE.pb_state & PB_ST_MP)
		    || cur.ps_maxdevs == 1)
			indx = 0;
		else
			indx = indeces[link];
	}
	if (indx < 0) {
		indx = 0;
		sw = -1;
	}

	if (sw < 0)
		return;

	save_x = x;
	save_y = y;

	if ((C_BUNDLE.pb_state & PB_ST_TX_BSD)
	    || (C_KDEVS[indx].dev_link.pl_state & PL_ST_TX_BSD)) {
		pr_bsd_db(sw,1,link,indx);
	} else if ((C_BUNDLE.pb_state & PB_ST_TX_PRED)
		   || (C_KDEVS[indx].dev_link.pl_state&PL_ST_TX_PRED)) {
		pr_pred_db(sw,1,link,indx);
	}
	ret_y = y;

	x += COL_WIDTH;
	y = save_y;
	if ((C_BUNDLE.pb_state & PB_ST_RX_BSD)
	    || (C_KDEVS[indx].dev_pf_state & PF_ST_RX_BSD)) {
		pr_bsd_db(sw,0,link,indx);
	} else if ((C_BUNDLE.pb_state & PB_ST_RX_PRED)
		   || (C_KDEVS[indx].dev_pf_state & PF_ST_RX_PRED)) {
		pr_pred_db(sw,0,link,indx);
	}
	if (ret_y < y)
		ret_y = y;

	x += COL_WIDTH;
	y = save_y;
	pr_vj_tx(sw);
	pr_vj_rx(sw, link);

	x = save_x;
	y = ret_y;
}


static void
dis_link(char *name, int link)
{
#   define ID_PAT "%-72.72s"
#   define ID_LEN 72
	int indx, sw;

	banner(name,0);

	indx = indeces[link];
	if (indx < 0) {
		y++;
		prnt(ID_PAT, "");
		standout();
		prnt("link %d inactive", link+1);
		standend();
		return;
	}

	/* the ID string is directly indexed */
	prnt("peer: "ID_PAT, &C_DDEVS[link].ident.msg[0]); y++;
	prnt("      "ID_PAT, (C_DDEVS[link].ident_len <= ID_LEN ? ""
			      : (char*)&C_DDEVS[link].ident.msg[ID_LEN]));

	y = TOP+2;
	pval(1,"link type", ((C_KDEVS[indx].dev_link.pl_state & PL_ST_SYNC)
			     ? "sync" : "async"));
	pval(1,"",	    C_DDEVS[link].callee ? "originated" : "answered");
	pval(1,"raw input",	SBYTE(PF_METER_DELTA(indx,raw_ibytes)));
	pval(1,"   output",	SBYTE(PL_DELTA(indx,pl_raw_obytes)));
	pval(1,"link rate",	SKBPS(C_DDEVS[link].bps));
	pval((C_DDEVS[link].lcp_echo_rtt != 0.0) ? 1 : 0, "RTT",
	     vtos("%3.0f ms", C_DDEVS[link].lcp_echo_rtt*1000.0));

	y += 2;
	pr_comp(1,link);		/* 3 columns of compression */

	y = TOP+2;
	x = COL2;
	pval(1,"active for",	SSEC(C_DDEVS[link].ps_active));
	pval(1,"MRU",		SNUM(C_DDEVS[link].lcp_neg_mru));
	pval(1,"MTU",		SNUM(C_DDEVS[link].lcp_neg_mtu));
	sw = (C_BUNDLE.pb_state & PB_ST_MP) ? 1 : -1;
	pval(sw,"MRRU",		SNUM(C_DDEVS[link].lcp_neg_mrru));
	pval(sw,"MTRU",		SNUM(C_DDEVS[link].lcp_neg_mtru));

	y = TOP+2;
	x = COL3;
	pval(1,"bad FCS",	SNUM(PF_METER_DELTA(indx,bad_fcs)));
	pval(1,"no buf",	SNUM(PF_METER_DELTA(indx,nobuf)));
	pval(1,"null frames",	SNUM(PF_METER_DELTA(indx,null)));
	pval(1,"~~~~~~~tiny",	SNUM(PF_METER_DELTA(indx,tiny)));
	pval(1,"~~~~aborted",	SNUM(PF_METER_DELTA(indx,abort)));
	pval(1,"~~~~~~~long",	SNUM(PF_METER_DELTA(indx,long_frame)));
	pval(1,"~~~~~babble",	SNUM(PF_METER_DELTA(indx,babble)));
}



static void
dis_io(char *name)
{
#	define IO_PAT "%8s %11s %11s %11s %11s %6s"
	u_int n, m;


	banner(name,0);

	prnt(IO_PAT, "", " raw", "  IP", "ratio", "recent", ""); y++;
	n = PB_METER_DELTA(raw_ibytes);
	m= PB_METER_DELTA(ibytes);
	prnt(IO_PAT, "   input", SBYTE(n), SBYTE(m), SRATIO(m,n),
	     SKBPS(cur.ps_cur_ibps),
	     ((cur.ps_beep_st & BEEP_ST_RX_BUSY) ? "hi"
	      : (cur.ps_beep_st & BEEP_ST_RX_ACT) ? "medium"
	      : ""));
	y++;
	n = PB_METER_DELTA(raw_obytes);
	m= PB_METER_DELTA(obytes);
	prnt(IO_PAT, "  output", SBYTE(n), SBYTE(m), SRATIO(m,n),
	     SKBPS(cur.ps_cur_obps),
	     ((cur.ps_beep_st & BEEP_ST_TX_BUSY) ? "hi"
	      : (cur.ps_beep_st & BEEP_ST_TX_ACT) ? "medium"
	      : ""));

	y += 2;

	/* top of first column */
	y = TOP+4;
	pval(1,"current devs",	SNUM(cur.ps_numdevs));

	/* Compression--leave it for the per-link screens if more than
	 * one compressor.  If present here, it uses the bottom of
	 * all three columns.
	 */
	y = TOP+10;
	pr_comp(1,-1);

	/* top of second column */
	y = TOP+4;
	x = COL2;

	/* top of third column */
	y = TOP+4;
	x = COL3;
	pval(1, "avail",	    SKBPS(cur.ps_avail_bps));
	pval(1, "previous",	    SKBPS(cur.ps_prev_avail_bps));
}


static void
dis_acct(char *name)
{
	u_int link, m;
	int t1, sw;


	banner(name, cur.ps_acct.last_date);

	/* The accounting base is reset periodically by the daemon.
	 * Nothing happens in only second, to take D_MODE as R_MODE.
	 */
#	define DA(nm) (cur.ps_acct.nm - ((mode == D_MODE		    \
					  || mode_date<=cur.ps_acct.last_date)\
					 ? 0 : base.ps_acct.nm))

	pval(1, "calls answered~~", SNUM(DA(answered)));

	y++;
	pval(1, "links originated", SNUM(DA(calls)));
	pval(1, "~~~~~~~~~~dialed", SNUM(DA(attempts)));
	pval(1, "~~~~gave up link", SNUM(DA(failures)));

	y++;
	sw = (cur.ps_acct.calls != 0);
	pval(sw,"min call setup",   SSEC(cur.ps_acct.min_setup));
	pval(sw,"~~~~~~~~~~~avg",   SSEC(cur.ps_acct.setup
					 / (sw ? cur.ps_acct.calls : 1)));
	pval(sw,"~~~~~~~~~~~max",   SSEC(cur.ps_acct.max_setup));

	y++;
	pval(cur.ps_acct.calls,	    "calling~~~~~~",hms(DA(orig_conn)));
	pval(cur.ps_acct.calls,	    "~~~~~~~~~idle",hms(DA(orig_idle)));
	pval(cur.ps_acct.calls,	    "~~~~~~~active",hms(DA(orig_conn)
							- DA(orig_idle)));

	y++;
	pval(cur.ps_acct.answered,  "answering~~~~",hms(DA(ans_conn)));
	pval(cur.ps_acct.answered,  "~~~~~~~~~idle",hms(DA(ans_idle)));
	pval(cur.ps_acct.answered,  "~~~~~~~active",hms(DA(ans_conn)
							- DA(ans_idle)));
	y++;
	pval(1,			    "not connected",
	     hms(cur.ps_acct.sconn
		 - ((mode == Z_MODE) ? MAX(cur.ps_acct.last_secs,
					   base.ps_acct.sconn)
		    : cur.ps_acct.last_secs)
		 - DA(orig_conn) - DA(ans_conn)));

	/* second column */
	y = TOP;
	x = COL2;

	pval(1,"maxdevs", SNUM(cur.ps_maxdevs));
	pval(1,"mindevs", SNUM(cur.ps_mindevs));
	pval(1,"outdevs", SNUM(cur.ps_outdevs));

	y++;
	y++;

#	define NEVER (60*60)
	pval(cur.ps_numdevs != 0 && cur.ps_atime < NEVER,
	     "activity timer",	SSEC(cur.ps_atime));
	pval(cur.ps_idle_time < NEVER && cur.ps_idle_time != 0,
	     "MP idle timer~",   SSEC(cur.ps_idle_time));
	pval(cur.ps_numdevs != 0 && cur.ps_busy_time < NEVER,
	     "MP busy timer~",	SSEC(cur.ps_busy_time));
	t1 = cur.ps_add_time;
	if (cur.ps_numdevs == 0 && cur.ps_atime < NEVER)
		t1 = MAX(cur.ps_atime, t1);
	pval(t1 != 0,
	     "no addition~~~",   SSEC(cur.ps_add_time));
	pval(cur.ps_add_backoff > 5,
	     "~~~~~backoff~~",   SSEC(cur.ps_add_backoff));

	/* third column */
	y = TOP;
	x = COL3;

	pval(1,"current devs",	SNUM(cur.ps_numdevs));
	for (link = 0, m = 0; link < MAX_PPP_LINKS; link++) {
		if (indeces[link] >= 0 && C_DDEVS[link].callee)
			m++;
	}
	pval(1,"  originated",	SNUM(cur.ps_numdevs-m));
	pval(1,"  answered",	SNUM(m));
	pval(1,"active TCP",	SNUM(cur.ps_pi.pi_vj_tx.actconn));

	y++;
	pval(1,			"main PID",	SNUM(cur.ps_pid));
	pval(cur.ps_add_pid > 0,"  new PID",	SNUM(cur.ps_add_pid));

#	undef DA
}


static void
dis_counts(char *name)
{
	int sw;


	banner(name,0);

	y++;
	pval(1,"pullups",	SNUM(PB_METER_DELTA(rput_pullup)));

	sw = (C_BUNDLE.pb_state & PB_ST_MP) ? 1 : -1;
	pval(sw,"MP pullups",	SNUM(PB_METER_DELTA(mp_pullup)));
	pval(sw,"  frags",	SNUM(PB_METER_DELTA(mp_frags)));
	pval(sw,"  nulls",	SNUM(PB_METER_DELTA(mp_null)));

	pval(sw,"  stale",	SNUM(PB_METER_DELTA(mp_stale)));
	pval(sw,"  no E bit",	SNUM(PB_METER_DELTA(mp_no_end)));
	pval(sw,"  orphan",	SNUM(PB_METER_DELTA(mp_no_bits)));
	pval(sw,"  stray",	SNUM(PB_METER_DELTA(mp_stray)));
	pval(sw,"  no mem",	SNUM(PB_METER_DELTA(mp_alloc_fails)));
	pval(sw,"  nested",	SNUM(PB_METER_DELTA(mp_nested)));

	pval(sw,"  frag queue",	SNUM(C_BUNDLE.pb_mp.num_frags));
	pval(sw,"  next sn",	SNUM(C_BUNDLE.pb_mp.sn_in));


	y = TOP;
	x = COL2;

	pval(1,"TOS queuing",	SNUM(PB_METER_DELTA(move2f)));
	pval(1,"TOS skipped",	SNUM(PB_METER_DELTA(move2f_leak)));

	y++;
	pval(1,"debugging",	SNUM(cur.ps_debug));
}


static void
prnt(char *fmt, ...)
{
	va_list args;

	move(y,x);
	va_start(args, fmt);
	if (win->_cury < BOS)
		vwprintw(win, fmt, args);
	va_end(args);
}


static void
clear_base(void)
{
	bzero(&base, sizeof(base));
}


static void
set_base(void)
{
	bcopy(&cur, &base, sizeof(cur));
}


static char*
hms(int secs)
{
	static int n;
	static struct {
		char c[16];		/* hhhhhhhhh:mm:ss */
	} bufs[NUM_VALUES];
	char *p;

	p = bufs[n].c;
	n = (n+1) % NUM_VALUES;

	if (secs < 0)
		secs = 0;
	sprintf(p, "%2d:%02d:%02d",
		(secs/(60*60)) % 10000, (secs/60) % 60, secs % 60);
	return p;
}


static char*
vtos(char *fmt, ...)
{
	static int n;
	static struct {
		char c[COL_WIDTH+1];
	} bufs[NUM_VALUES];
	char *p;
	va_list args;


	va_start(args, fmt);
	p = bufs[n].c;
	n = (n+1) % NUM_VALUES;

	(void)vsprintf(p, fmt, args);
	va_end(args);

	return p;
}


static void
pval(int sw, char *lab, char *v)
{
	int lab_len, v_len, pad;
	char buf[COL_WIDTH+1];


	if (sw < 0) {
		lab = "";
		v = "";
	} else if (sw == 0) {
		v = "";
	}

	for (lab_len = 0; *lab != '\0'; lab++, lab_len++)
		buf[lab_len] = (*lab == '~') ? ' ' : *lab;
	buf[lab_len] = '\0';

	move(y,x);
	v_len = strlen(v);
	pad = LABEL_WIDTH - lab_len;
	if (v_len > NOMINAL_FIELD_WIDTH)
		pad -= v_len - NOMINAL_FIELD_WIDTH;
	if (pad < 0)
		pad = 0;
	printw("%s %*s%-*s ", buf,
	       pad, "",
	       (LABEL_WIDTH-lab_len-pad)+MAX_FIELD_WIDTH, v);
	y++;
}


static void
usage(void)
{
	fflush(stdout);
	fflush(stderr);
	(void)fprintf(stderr, "%s: usage: [-u unit] [-i interval]"
		      " [-m d|r|z]\n",
		      pgmname);
	exit(1);
}


static void
sick(char *fmt, ...)
{
	va_list args;

	move(BOS, 0);
	clrtoeol();

	va_start(args, fmt);
	if (++fail_cnt >= FATAL_FAIL) {
		refresh();
		endwin();
		(void)vprintf(fmt,args);
		putc('\n',stdout);
		exit(1);
	}

	(void)vwprintw(win, fmt, args);
	refresh();
	needfoot = 1;
	va_end(args);
}


#if 0
static void
fail(char *fmt, ...)
{
	va_list args;

	move(BOS, 0);
	clrtoeol();
	va_start(args, fmt);

	refresh();
	endwin();
	(void)vprintf(fmt,args);
	putc('\n',stdout);
	exit(1);

	va_end(args);
}
#endif


static void
quit(int s)
{
	move(BOS, 0);
	clrtoeol();
	refresh();
	endwin();
	exit(s);
}


static void
sig_winch(int s)
{
	(void)endwin();
	win = initscr();
	if (first < 1)
		first = 1;

	(void)signal(s, sig_winch);
}
