/* @(#)sysmeter.c	1.1 88/02/26 3.2 NFSSRC */

#include <stdio.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <string.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>
#include <rpcsvc/rstat.h>
#include <arpa/inet.h>

#include <gl/gl.h>
#include <gl/device.h>


#ifndef FSCALE
#define FSCALE (1<<8)
#endif
#define	MAXSTEP		5	/* five step function in slide */
#define CHAR_SIZE	9	/* width per char. */
#define	SLIDER_SIZE	8	/* half of slider height */
#define H_GAP1		2	/* gap between lower horizontal line and lower chars. */
#define H_GAP2		4	/* gap between top h. line and top chars. */
#define V_GAP		2	/* gap between right vertical line and next char. */
#define PER_INT		1	/* pixls per interval */
#define	MIN_PIC		1	/* min. appearance on each view */
#define	MIN_INT_WID	(40/PER_INT)

#define S1X1		(CHAR_SIZE)
#define S1Y1		(2*SLIDER_SIZE)
#define S1X2		(S1X1+2*SLIDER_SIZE)
#define S1Y2		(S1Y1+2*SLIDER_SIZE)
#define S2X1		(S1X2+CHAR_SIZE*5)
#define S2Y1		(S1Y1+SLIDER_SIZE-CHAR_SIZE/2)
#define S2X2		(S2X1+CHAR_SIZE)
#define S2Y2		(S1Y2-SLIDER_SIZE+CHAR_SIZE/2)
#define S3X1		(S1X1)
#define S3Y1		(0)
#define S3X2		(CHAR_SIZE*14)
#define S3Y2		(2*SLIDER_SIZE-CHAR_SIZE-2)

#define	SLDXL		S3X1
#define	SLDXH		S3X2


#define M_ALLOC		100	/* num. of structure once allocated */

#define ADJMAX(val, max, indx)\
	while (max <= val) max *= 2; \
	if ( val > items[indx].cmax ) { \
		val = items[indx].cmax; \
	}

char	hostnm[MAXHOSTNAMELEN];


int	g_size = 40;
int	tot_elapse;
Colorindex backgnd_color = 15,
	chartback_color = 12,
	avg_color = 1,
	text_color = 4,
	changeback_color = 10;

#define	P_CPU		0
#define	P_PAGE		1
#define	P_SWAP		2
#define	P_INTR		3
#define	P_DISK		4
#define	P_CNTXT		5
#define	P_LOAD		6
#define	P_PKTS		7
#define	P_IPKTS		8
#define	P_OPKTS		9
#define	P_COLLS		10
#define	P_ERRS		11
#define	MAXITEMS	12

#define P_DISP_AVG	50
#define P_PARAM		51
#define P_STYLE		52
#define P_EXIT		53

/* the latter 4 items must be number n+1 */
const char menu[] =
    "Sysmeter %t|cpu|page|swap|intr|disk|contxt|load|pkts|inpkts|outpkts|colls|errs %l|Display average %x51|Change params %x52|Change style %x53|Exit %x54";
struct disp_fac {
	short	onoff;
	short	cmax;
	short	minmax;
	short	maxmax;
	char	*name;
	int		adj;
	int		x1;
	int		y1;
	int		x2;
	int		y2;
} items[MAXITEMS] = {
	{0, 100, 100, 100,  "cpu", 0},
	{0, 16,  4,   8192, "page", 0},
	{0, 4,   4,   8192, "swap", 0},
	{0, 64,  4,   8192, "intr", 0},
	{0, 32,  4,   8192, "disk", 0},
	{0, 64,  4,   8192, "cntxt", 0},
	{0, 4,   4,   8192, "load", 0},
	{0, 32,  4,   8192, "pkts", 0},
	{0, 32,  4,   8192, "inpkts", 0},
	{0, 32,  4,   8192, "outpkts", 0},
	{0, 4,   4,   8192, "colls", 0},
	{0, 4,   4,   8192, "errs", 0}, 
};

enum { SAMPLE, AVERAGE, WINDOW } chg_state = SAMPLE;

struct entry *avail_h, *used_h, *used_t;
struct entry {
	struct entry	*f_link;
	struct entry	*b_link;
	int				data[MAXITEMS];
	int				avg[MAXITEMS];
	int				intv;
};

int	total_item;
int	smp_intv = 2, avg_intv = 20;
int	meter = 0, f_row, row, x_coord = -1, y_coord = -1;
int	f_duration = 120, duration;
char	v_chg = 0, vflag;
int	vers, Debug = 0;
int	wid;
int	item_len, item_high;
int	in_interrupt, need_avg, fancy_title =1;
int	x_size, y_size;
int	rx_size, ry_size;
int	menu1, accum;
int	acc_s, acc_a, acc_w;
int	step_val[MAXSTEP] = { 1, 5, 30, 60, 300 };
int	step_rng[MAXSTEP] = { 20, 8, 8, 10, 21};
int		sld_max, sld_maxp;
char	buf[80];
char	buf2[80];
int	sig_timer;
void	get_perfdata();
int	set_unit();


CLIENT *Client;
struct hostent *hp;
struct timeval retry_timeout, total_timeout;
struct sockaddr_in server_addr;
int	sock = RPC_ANYSOCK;
struct statstime stats_info;

main(argc, argv)
	char **argv;
{
	short	val;
	int	tval, dev, x1, y1;

	(void) gethostname(hostnm, sizeof(hostnm));

	while (argc > 1) {
		if (argv[1][0] != '-') {
			if ( vflag ) {
				seton(argv[1]);
			}
			else
				usage();
		}
		else {
			vflag = 0;
			switch(argv[1][1]) {
			case 's':
				if ( argc < 3 || argv[2][0] == '-' )
					usage();
				smp_intv = atoi(argv[2]);
				if ( smp_intv < 1)
					smp_intv = 2; 
				argc--;
				argv++;
				break;
			case 'a':
				if ( argc < 3 || argv[2][0] == '-' )
					usage();
				avg_intv = atoi(argv[2]);
				need_avg = 1;
				argc--;
				argv++;
				break;
			case 'r':
				if ( argc < 3 || argv[2][0] == '-' )
					usage();
				f_row = atoi(argv[2]);
				argc--;
				argv++;
				break;
			case 'g':
				if ( argc < 3 || argv[2][0] == '-' )
					usage();
				g_size = atoi(argv[2]);
				argc--;
				argv++;
				break;
			case 'h':
				if ( argc < 3 || argv[2][0] == '-' )
					usage();
				strcpy(hostnm, argv[2]);
				argc--;
				argv++;
				break;
			case 'v':
				vflag++;
				break;
			case 'x':
				if ( argc < 3 || argv[2][0] == '-' )
					usage();
				x_coord = atoi(argv[2]);
				argc--;
				argv++;
				break;
			case 'y':
				if ( argc < 3 || argv[2][0] == '-' )
					usage();
				y_coord = atoi(argv[2]);
				argc--;
				argv++;
				break;
			case 'd':
				if ( argc < 3 || argv[2][0] == '-' )
					usage();
				f_duration = atoi(argv[2]);
				argc--;
				argv++;
				break;
			case 'm':
				meter = 1;
				break;
			case 'D':
				Debug = 1;
				break;
			case 'i':
				if ( argc < 3 || argv[2][0] == '-' )
					usage();
				(void) sscanf(argv[2], "%hu,%hu,%hu,%hu,%hu",
					&backgnd_color,
					&chartback_color,
					&avg_color,
					&text_color,
					&changeback_color);
				argc--;
				argv++;
				break;
			default:
				usage();
			}
		}
		argv++;
		argc--;
	}

	if ( !total_item ) {
		items[0].onoff = 1;
		total_item++;
	}
	row = f_row;
	if ( row == 0 )
		row = (total_item + 1)/2;

	rndup_dur();

	perf_setup();

	qdevice(RIGHTMOUSE);
	qdevice(LEFTMOUSE);
	qdevice(REDRAW);
	menu1 = defpup((char *)menu);
	for (tval = 0; tval < MAXITEMS; tval++) {
		setpup(menu1, tval+1, items[tval].onoff ? PUP_CHECK: PUP_BOX);
	}

	for ( ; ; ) {
		errno = 0;
		dev = qread(&val);
		if ( dev != REDRAW &&  val != 1 )
			continue;
		in_interrupt = 1;
		switch(dev) {
		case RIGHTMOUSE:
			tval = dopup(menu1);
			menu_op(tval);
			break;

		case LEFTMOUSE:
			if ( v_chg ) {
				x1 = getvaluator(MOUSEX) - x_coord;
				y1 = getvaluator(MOUSEY) - y_coord;
				left_func(x1, y1);
			}
			break;

		case REDRAW:
			re_start();
			break;

		default:
			break;
		}
		in_interrupt = 0;
	}
}


left_func(x, y)
int	x, y;
{
	int	i;
	int	t_x, t_y;

	getsize((long *)&t_x, (long *)&t_y);
	x = x*x_size/t_x;
	y = y*y_size/t_y;

	if ( x >= S1X1 && x <= S1X2 && y <= S1Y2 && y >= S1Y1 ) {
		switch (chg_state) {
		    case SAMPLE:
			if (accum >= 1)
				acc_s = accum;
			chg_state = AVERAGE;
			accum = acc_a;
			break;
		    case AVERAGE:
			if (accum > smp_intv)
				acc_a = accum;
			chg_state = WINDOW;
			accum = acc_w;
			break;
		    case WINDOW:
			if (accum > smp_intv)
				acc_w = accum;
			chg_state = SAMPLE;
			accum = acc_s;
			break;
		}
		draw_chg();
		draw_chg();
		return;
	}
	else if ( x >= S2X1 && x <= S2X2 && y <= S2Y2 && y >= S2Y1 ) {
		if (acc_s < 1) {
#ifdef DEBUG
			abort();
#endif
			acc_s = 1;
		}
		smp_intv = acc_s;
		avg_intv = acc_a;
		f_duration = acc_w;
		acc_s = acc_a = acc_w = 0;
		switch (chg_state) {
		    case SAMPLE:
			if (accum >= 1)
				smp_intv = accum;
			break;
		    case AVERAGE:
			if (accum > smp_intv)
				avg_intv = accum;
			break;
		    case WINDOW:
			if (accum > smp_intv)
				f_duration = accum;
			else
				return;
			break;
		}
		v_chg = 0;
		accum = 0;
		re_start();
	} else {
		for ( ; getbutton(LEFTMOUSE); ) {
			x = getvaluator(MOUSEX) - x_coord;
			y = getvaluator(MOUSEY) - y_coord;
			x = x*x_size/t_x;
			y = y*y_size/t_y;
			if (x >= S3X1 && x <= S3X2 && y <= S3Y2 && y >= S3Y1 ) {
				x = (x-SLDXL)*sld_maxp/(SLDXH - SLDXL);
				accum = 0;
				for ( i = 0; i < MAXSTEP; i++ ) {
					if ( x > step_rng[i] ) {
						accum += 
						    step_val[i]*step_rng[i];
						x -= step_rng[i];
						continue;
					}
					accum += step_val[i]*x;
					break;
				}
				draw_slide(accum);
			}
		}
	}
}


menu_op(val)
int	val;
{
	val--;
	if (v_chg && val != P_EXIT && val != P_PARAM)
		return;

	switch(val) {
	case P_CPU:
	case P_PAGE:
	case P_SWAP:
	case P_INTR:
	case P_DISK:
	case P_CNTXT:
	case P_LOAD:
	case P_PKTS:
	case P_IPKTS:
	case P_OPKTS:
	case P_COLLS:
	case P_ERRS:
		if ( items[val].onoff ) {
			items[val].onoff = 0;
			total_item--;
			setpup(menu1, val+1, PUP_BOX);
		} else {
			items[val].onoff = 1;
			total_item++;
			setpup(menu1, val+1, PUP_CHECK);
		}
		if ( total_item == 0 ) {
			items[P_CPU].onoff = 1;
			total_item = 1;
			setpup(menu1, P_CPU+1, PUP_CHECK);
		}
		re_start();
		break;

	case P_DISP_AVG:
		need_avg = !need_avg;
		re_start();
		break;

	case P_PARAM:
		if (v_chg) {
			v_chg = 0;
			re_start();
		} else {
			v_chg = 1;
			acc_s = smp_intv;
			acc_a = avg_intv;
			acc_w = duration;
			switch (chg_state) {
			    case SAMPLE:
				accum = smp_intv;
				break;
			    case AVERAGE:
				accum = avg_intv;
				break;
			    case WINDOW:
				accum = duration;
				break;
			    default:
#ifdef DEBUG
			    	abort();
#endif
				break;
			}
			if (acc_s < 1) {
#ifdef DEBUG
			    	abort();
#endif
			    	accum = 1;
			}
			re_draw(1);
			re_draw(1);
			draw_chg();
			draw_chg();
		}
		break;

	case P_STYLE:
		meter = !meter;
		re_start();
		break;

	case P_EXIT:
		winclose(wid);
		exit(1);

	default:
		break;
	}
}

static void
retitle(void)
{
	char title[MAXHOSTNAMELEN + 50];

	if (fancy_title) {
		if ( meter ) {
			if ( need_avg )
				sprintf(title, "%s (samp=%ds,avg=%ds)",
					hostnm, smp_intv, avg_intv);
			else
				sprintf(title,
					"%s (samp=%ds)", hostnm, smp_intv);
		} else if ( need_avg )
			sprintf(title, "%s (samp=%ds,avg=%ds,win=%ds)",
				hostnm, smp_intv, avg_intv, duration);
		else
			sprintf(title, "%s (samp=%ds,win=%ds)",
				hostnm, smp_intv, duration);
	} else
		sprintf(title, "%s", hostnm);

	wintitle(title);
}

re_start()
{
	rndup_dur();

	item_high = g_size + CHAR_SIZE + H_GAP1 + H_GAP2 + MIN_PIC;
	if ( meter ) {
		item_len = g_size + CHAR_SIZE*4+V_GAP;
	} else {
		item_len = (duration/smp_intv) * 
			PER_INT+PER_INT-1+4*CHAR_SIZE+V_GAP;
	}

	row = howmanyrow();
	x_size = item_len*((total_item+row-1)/row)+CHAR_SIZE;
	y_size = row*item_high + CHAR_SIZE;
	getorigin((long *)&x_coord, (long *)&y_coord);

	resize();
	getsize((long *)&rx_size,(long *)&ry_size);

	retitle ();

	draw_backg();
	draw_backg();
	re_draw(1);
	re_draw(1);

	if ( v_chg ) {
		draw_chg();
		draw_chg();
	}
	(void) signal(SIGALRM, get_perfdata);
	alarm(smp_intv);
}



seton(item)
char	*item;
{
	int	i;

	if (!strcmp(item, "all")) {
		for ( i = 0; i < MAXITEMS; i++ ) {
			items[i].onoff = 1;
			total_item++;
		}
		return;
	} else
		for ( i = 0; i < MAXITEMS; i++ )
			if ( !strcmp(item, items[i].name) ) {
				items[i].onoff = 1;
				total_item++;
				return;
			}
	usage();
}

usage()
{
	fprintf(stderr, "Usage: sysmeter [-s sample-interval]\n");
	fprintf(stderr, "                [-a averaging-interval]\n");
	fprintf(stderr, "                [-h hostname]\n");
	fprintf(stderr, "                [-r row]\n");
	fprintf(stderr, "                [-d duration-in-seconds]\n");
	fprintf(stderr, "                [-D debugging-version]\n");
	fprintf(stderr, "                [-x xcoord] [-y ycoord]\n");
	fprintf(stderr, "                [-g height]\n");
	fprintf(stderr, "                [-m]\n");
	fprintf(stderr, "                [-i 1stof5colorindex]\n");
	fprintf(stderr, "                [-v value value ...]\n");
	fprintf(stderr, "value can be:   cpu page swap intr disk cntxt load pkts inpkts outpkts colls errs\n");
	exit(1);
}

perf_setup()
{
	int	i;

	vers = RSTATVERS_TIME;
	if ((hp = gethostbyname(hostnm)) == NULL) {
		fprintf(stderr, "sysmeter: unknown host %s\n", hostnm);
		exit(1);
	}
	bcopy(hp->h_addr, (caddr_t)&server_addr.sin_addr, hp->h_length);
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = 0;	/* use pmapper */
	if (Debug) printf("Got the host named %s (%s)\n",
			hostnm, inet_ntoa(server_addr.sin_addr));
	retry_timeout.tv_sec = 5;
	retry_timeout.tv_usec = 0;

	if ((Client = clntudp_create(&server_addr, RSTATPROG, vers, retry_timeout,
		&sock)) == NULL) {
		fprintf(stderr, "sysmeter: cannot contact %s%s\n",
			hostnm, clnt_spcreateerror(""));
		exit(1);
	}
	if (Debug) printf("UDP RPC connection created\n");
	Client->cl_auth = authunix_create_default();

	total_timeout.tv_sec = 20;
	total_timeout.tv_usec = 0;

	for ( i = 0; i < MAXSTEP; i++ ) {
		sld_maxp += step_rng[i];
		sld_max += step_val[i]*step_rng[i];
	}

	/* open window, get window size and allocate memory */
	create_window();

	get_perfdata();
}


/* all coordination supposed to aligned with pixels, otherwise, you
   can't determine the window size */
create_window()
{
	item_high = g_size+CHAR_SIZE+H_GAP1+H_GAP2 + MIN_PIC;
	if ( meter ) {
		item_len = g_size + CHAR_SIZE*4+V_GAP;
	}
	else {
		item_len = (duration/smp_intv)*PER_INT+PER_INT-1+4*CHAR_SIZE+V_GAP;
	}
#ifdef DEBUG
	foreground();
#endif
	if ( x_coord >= 0 && y_coord >= 0 ) {
		if ( row > total_item )
			row = total_item;
		x_size = item_len*((total_item+row-1)/row) + CHAR_SIZE;
		y_size = row*item_high + CHAR_SIZE;

		if ( x_coord + x_size > XMAXSCREEN )
			x_coord = XMAXSCREEN - x_size;
		if ( y_coord + y_size + 2*CHAR_SIZE > YMAXSCREEN )
			y_coord = YMAXSCREEN - y_size - 2*CHAR_SIZE;
		prefposition(x_coord, x_coord + x_size, y_coord, y_coord + y_size);
		wid = winopen("sysmeter");
		winconstraints();;
	}
	else {
		wid = winopen("sysmeter");
		getorigin((long *)&x_coord, (long *)&y_coord);
		f_row = row = howmanyrow();
		x_size = item_len*((total_item+row-1)/row) + CHAR_SIZE;
		y_size = row*item_high + CHAR_SIZE;
		resize();
	}
	doublebuffer();
	gconfig();
	getsize((long *)&rx_size, (long *)&ry_size);

	retitle ();
	draw_backg();
	draw_backg();
}


resize()
{
	register float	t_x, t_y;

	if ( x_size >= rx_size ) {
		t_x = x_size/rx_size;
	}
	else {
		t_x = rx_size/x_size;
	}
	if ( y_size >= ry_size ) {
		t_y = y_size/ry_size;
	}
	else {
		t_y = ry_size/y_size;
	}
	if ( t_x >= t_y ) {
		if ( ry_size < y_size )
			ry_size = y_size;
		if ( ry_size*x_size/y_size - 1 > XMAXSCREEN )
			return;
		if ( rx_size != ry_size*x_size/y_size )
			winposition(x_coord, x_coord+ry_size*x_size/y_size-1,
				y_coord, y_coord+ry_size-1);
	}
	else {
		if ( rx_size < x_size )
			rx_size = x_size;
		if ( rx_size*y_size/x_size -1 > YMAXSCREEN )
			return;
		if ( ry_size != rx_size*y_size/x_size )
			winposition(x_coord, x_coord+rx_size-1, y_coord,
				y_coord+rx_size*y_size/x_size-1);
	}
}

/* given an arbitrary shape of window, figure out best arrangement */
int
howmanyrow()
{
	register int	i, j, col;
	float	prev_ratio, r_ratio, tmp_ratio;
	float	f1, f2;

	getsize((long *)&rx_size, (long *)&ry_size);
	prev_ratio = -1;
	f1 = rx_size;
	f2 = ry_size;
	r_ratio = f2/f1;

	for ( i = 1; i <= total_item; i++ ) {
		col = (total_item + i - 1)/i;
		if ( (i*col - total_item) > (col-1) )
			continue;
		x_size = item_len*col + CHAR_SIZE;
		y_size = i*item_high + CHAR_SIZE;
		f1 = x_size;
		f2 = y_size;
		tmp_ratio = f2/f1;
		if ( tmp_ratio >= r_ratio ) {
			if ( prev_ratio < 0 )
				return(i);
			if ( tmp_ratio - r_ratio > r_ratio - prev_ratio )
				return (j);
			else
				return (i);
		}
		prev_ratio = tmp_ratio;
		j = i;
	}
	return (total_item);
}


draw_backg()
{
	int		x1, x2, y1, y2;
	int		i, j, c_row, row_item;
	float	xx, yy;

	reshapeviewport();
	xx = x_size;
	yy = y_size;
	ortho2(0.0, xx, 0.0, yy);
	color(backgnd_color);
	clear();
	getsize((long *)&rx_size, (long *)&ry_size);

	row_item = 0;
	c_row = 0;
	for ( j = i = 0; i < total_item; i++) {
		for ( ; !(items[j].onoff); j++);

		x1 = item_len*row_item + 4*CHAR_SIZE + V_GAP;
		y1 = (row-c_row-1)*item_high + CHAR_SIZE + H_GAP1 + H_GAP2;
		y2 = y1 + g_size + MIN_PIC;

		if ( meter ) {
			x2 = x1 + g_size;

			color(text_color);
			move2i(x1, y2);
			draw2i(x1, y1);
			draw2i(x2, y1);
			move2i(x1, y2);
			draw2i(x1 + g_size/8, y2);
			move2i(x1, y1 + g_size*3/4 + MIN_PIC);
			draw2i(x1 + g_size/8, y1 + g_size*3/4 + MIN_PIC);
			move2i(x1, y1 + g_size/2 + MIN_PIC);
			draw2i(x1 + g_size/8, y1 + g_size/2 + MIN_PIC);
			move2i(x1, y1 + g_size/4 + MIN_PIC);
			draw2i(x1 + g_size/8, y1 + g_size/4 + MIN_PIC);
		}
		else {
			x2 = (row_item + 1)*item_len;

			color(chartback_color);
			rectfi(x1, y1, x2, y2);
		}

		items[j].x1 = x1;
		items[j].x2 = x2;
		items[j].y1 = y1;
		items[j].y2 = y2;

		strcpy(buf, items[j].name);

		color(text_color);
		cmov2i(x1 + V_GAP, y1 - H_GAP1 - CHAR_SIZE*x_size/rx_size);
		charstr(buf);

		cmov2i(x1 - V_GAP - 4*CHAR_SIZE*x_size/rx_size,
			y2 - CHAR_SIZE*x_size/rx_size);
		sprintf(buf, "%4d", items[j].cmax);
		charstr(buf);

		cmov2i(x1 - V_GAP - CHAR_SIZE*x_size/rx_size, y1);
		charstr("0");

		row_item++;
		if ( row_item >= (total_item+row-1)/row ) {
			row_item = 0;
			c_row++;
		}
		j++;
	}
	swapbuffers();
	getorigin((long *)&x_coord, (long *)&y_coord);
}



void
get_perfdata()
{
	enum clnt_stat err;

	sig_timer++;
	err = clnt_call(Client, RSTATPROC_STATS, xdr_void, 0, xdr_statstime,
		&stats_info, total_timeout);
	if ( err != RPC_SUCCESS ) {
		fprintf(stderr, "sysmeter: can't get stats: %s\n",
			clnt_sperrno(err));
		winclose(wid);
		exit(1);
	}

	insert_new();

	(void) signal(SIGALRM, get_perfdata);
	alarm(smp_intv);
}

insert_new()
{
	static struct statstime prev_v;
	static int first = 1;
	register int		i;
	register struct entry	*mem;
	int	temp, totcpu;

	if ( in_interrupt )
		return;

	if ( avail_h == 0 )
		release_used();

	if ( avail_h == 0 ) {
		mem = (struct entry *)malloc(M_ALLOC*sizeof(struct entry));
		if ( mem == NULL ) {
			fprintf(stderr, "Out of memory\n");
			winclose(wid);
			exit(1);
		}
		for ( i = 0; i < M_ALLOC; i++, mem++)
			insert_avail(mem);
	}

	if (first) {
		prev_v = stats_info;
		first = 0;
		return;
	}

	mem = avail_h;
	if ( avail_h->f_link )
		avail_h->f_link->b_link = 0;
	avail_h = avail_h->f_link;

	mem->intv = smp_intv;
	temp = stats_info.cp_time[CP_USER] - prev_v.cp_time[CP_USER] +
		stats_info.cp_time[CP_NICE] - prev_v.cp_time[CP_NICE] +
		stats_info.cp_time[CP_SYS] - prev_v.cp_time[CP_SYS];
	totcpu = temp + stats_info.cp_time[CP_IDLE] - prev_v.cp_time[CP_IDLE];
	if (totcpu)
		mem->data[P_CPU] = temp*100/totcpu;
	else
		mem->data[P_CPU] = 0;
	mem->data[P_PAGE] = (stats_info.v_pgpgin - prev_v.v_pgpgin +
		stats_info.v_pgpgout - prev_v.v_pgpgout + smp_intv/2) /
		smp_intv;
	mem->data[P_SWAP] = (stats_info.v_pswpin - prev_v.v_pswpin +
		stats_info.v_pswpout - prev_v.v_pswpout + smp_intv/2) /
		smp_intv;
	mem->data[P_INTR] = (stats_info.v_intr - prev_v.v_intr + smp_intv/2) /
		smp_intv;
	mem->data[P_DISK] =
	       (stats_info.dk_xfer[0] - prev_v.dk_xfer[0] +
		stats_info.dk_xfer[1] - prev_v.dk_xfer[1] +
		stats_info.dk_xfer[2] - prev_v.dk_xfer[2] +
		stats_info.dk_xfer[3] - prev_v.dk_xfer[3] + smp_intv/2) /
		smp_intv;
	mem->data[P_CNTXT] = (stats_info.v_swtch - prev_v.v_swtch + smp_intv/2) /
		smp_intv;
	mem->data[P_LOAD] = stats_info.avenrun[0]/FSCALE;
	mem->data[P_PKTS] = (stats_info.if_ipackets - prev_v.if_ipackets +
		stats_info.if_opackets - prev_v.if_opackets + smp_intv/2) /
		smp_intv;
	mem->data[P_IPKTS] = (stats_info.if_ipackets - prev_v.if_ipackets +
		smp_intv/2) / smp_intv;
	mem->data[P_OPKTS] = (stats_info.if_opackets - prev_v.if_opackets +
		smp_intv/2) / smp_intv;
	mem->data[P_COLLS] = (stats_info.if_collisions - prev_v.if_collisions +
		smp_intv/2) / smp_intv;
	mem->data[P_ERRS] = (stats_info.if_ierrors - prev_v.if_ierrors +
		stats_info.if_oerrors - prev_v.if_oerrors + smp_intv/2) /
		smp_intv;
	for ( i = 0; i < MAXITEMS; i++ )
		if ( mem->data[i] < 0 )	
			mem->data[i] = 0;

	if ( used_h ) {
		used_h->b_link = mem;
		mem->f_link = used_h;
		mem->b_link = 0;
		used_h = mem;
	}
	else {
		used_t = used_h = mem;
		mem->f_link = mem->b_link = 0;
	}

	set_avgval();

	tot_elapse += smp_intv;
	prev_v = stats_info;

	re_draw(0);
}


re_draw(force)
int	force;
{
	register struct entry *ent;
	int	new_max, c_col, i, j;
	int	c_val, ini_draw;
	int	x_pos, y_pos;
	long	draw[4][2];

	if ( used_h == 0 )
		return;
	if ( !force && v_chg )
		return;
	c_col = 0;
	for ( i = 0; i < MAXITEMS; i++) {
		if ( !items[i].onoff ) continue;
		ini_draw = c_val = 0;
		new_max = items[i].cmax/2;

		x_pos = items[i].x1;
		y_pos = items[i].y1;
		if ( meter ) {
			color(backgnd_color);
			rectfi(x_pos+g_size/3, y_pos, x_pos+g_size-g_size/3+g_size/8,
				y_pos+g_size+MIN_PIC);
		}
		else {
			color(chartback_color);
			rectfi(x_pos, y_pos, items[i].x2, items[i].y2);
		}
		if ( meter ) {
			if ( need_avg ) {
				c_val = used_h->avg[i];
				ADJMAX(c_val, new_max, i);
				color(avg_color);
				rectfi(x_pos+g_size/3+g_size/8, y_pos,
					x_pos+g_size-g_size/3+g_size/8,
					y_pos+MIN_PIC+g_size*c_val/items[i].cmax);
			}
			c_val = used_h->data[i];
			ADJMAX(c_val, new_max, i);
			color(chartback_color);
			rectfi(x_pos+g_size/3, y_pos, x_pos+g_size-g_size/3,
				y_pos+MIN_PIC+g_size*c_val/items[i].cmax);
			move2i(x_pos+g_size/3, y_pos);
			color(text_color);
			draw2i(x_pos+g_size-g_size/3+g_size/8, y_pos);

			goto adjust_max;
		}

		if ( need_avg && used_h->f_link) {
			color(avg_color);
			j = duration/smp_intv;
			c_val = used_h->avg[i];
			ADJMAX(c_val, new_max, i);
			draw[0][0] = x_pos + j*PER_INT;
			draw[0][1] = y_pos + MIN_PIC + g_size*c_val/items[i].cmax;
			draw[3][0] = draw[0][0];
			draw[2][1] = draw[3][1] = y_pos;
			ent = used_h->f_link;
			j--;
			for ( ; j >= 0; j-- ) {
				c_val = ent->avg[i];
				ADJMAX(c_val, new_max, i);

				draw[2][0] = draw[1][0] = x_pos + j*PER_INT;
				draw[1][1] = y_pos+MIN_PIC+g_size*c_val/items[i].cmax;
				polf2i(4, draw);
				draw[0][0] = draw[1][0];
				draw[0][1] = draw[1][1];
				draw[3][0] = draw[2][0];
				if ( !(ent = ent->f_link ) )
					break;
			}
		}

		color(BLACK);
		ent = used_h;
		ini_draw = c_val = 0;
		for ( j = duration/smp_intv; j; j-- ) {
			if ( !ent )
				break;
			c_val = ent->data[i];
			ent = ent->f_link;

			/* now draw one sample */
			ADJMAX(c_val, new_max, i);
			if ( ini_draw )
				draw2i(x_pos + j*PER_INT,
					y_pos + MIN_PIC + g_size*c_val/items[i].cmax);
			else {
				ini_draw = 1;
				move2i(x_pos + j*PER_INT,
					y_pos + MIN_PIC + g_size*c_val/items[i].cmax);
			}
		}

adjust_max:
		if ( new_max > items[i].maxmax )
			new_max = items[i].maxmax;
		if ( new_max < items[i].minmax )
			new_max = items[i].minmax;
		if ( i != P_CPU && ((new_max != items[i].cmax) ||
			items[i].adj) ) {
			if ( items[i].cmax == new_max )
				items[i].adj = 0;
			else
				items[i].adj = 1;
			items[i].cmax = new_max;
			x_pos = items[i].x1 - V_GAP - 4*CHAR_SIZE*x_size/rx_size;
			y_pos = items[i].y2 - CHAR_SIZE*x_size/rx_size;
			color(backgnd_color);
			rectfi(x_pos, y_pos, x_pos + 4*CHAR_SIZE*x_size/rx_size,
				y_pos + CHAR_SIZE*x_size/rx_size + H_GAP2);
			color(text_color);
			sprintf(buf, "%4d", items[i].cmax);
			cmov2i(x_pos, y_pos);
			charstr(buf);
		}

		if ( ++c_col >= (total_item+row-1)/row ) {
			c_col = 0;
		}
	}
	swapbuffers();
}


draw_chg()
{
	long	i, arrow[3][2];
	float	xx, yy;

	color(changeback_color);
	rectfi(0, 0, 15*CHAR_SIZE, 4*SLIDER_SIZE);
	color(text_color);
	arcfi(CHAR_SIZE+SLIDER_SIZE, 3*SLIDER_SIZE, SLIDER_SIZE-1, 450, 1350);
	arcfi(CHAR_SIZE+SLIDER_SIZE, 3*SLIDER_SIZE, SLIDER_SIZE-1, 1650, 2550);
	arcfi(CHAR_SIZE+SLIDER_SIZE, 3*SLIDER_SIZE, SLIDER_SIZE-1, 2850, 3750);
	color(changeback_color);
	circfi(CHAR_SIZE+SLIDER_SIZE, 3*SLIDER_SIZE, SLIDER_SIZE-3);
	color(text_color);
	for ( i = 0; i < 3; i++ ) {
		pushmatrix();
		xx = CHAR_SIZE+SLIDER_SIZE;
		yy = 3*SLIDER_SIZE;
		translate(xx, yy, 0.0);
		arrow[0][0] = 0;
		arrow[0][1] = -2;
		arrow[1][0] = 2;
		arrow[1][1] = 0;
		arrow[2][0] = -2;
		arrow[2][1] = 0;
		rotate(i*1200+450, 'Z');
		xx = SLIDER_SIZE-2;
		translate(xx, 0.0, 0.0);
		polf2i(3, arrow);
		popmatrix();
	}
	cmov2i(S1X2+CHAR_SIZE, 3*SLIDER_SIZE-CHAR_SIZE/2);
	switch (chg_state) {
	    case SAMPLE:
		strcpy(buf, "Sample");
		break;
	    case AVERAGE:
		strcpy(buf, "Average");
		break;
	    case WINDOW:
		strcpy(buf, "Window");
		break;
	}
	charstr(buf);
	recti(S2X1, S2Y1, S2X2, S2Y2);
	cmov2i(S2X1+2, S2Y1+1);
	charstr("ok");
	
	cmov2i(CHAR_SIZE, 2*SLIDER_SIZE-CHAR_SIZE+1);
	charstr("0");
	sprintf(buf, "%d", sld_max);
	cmov2i(14*CHAR_SIZE-strlen(buf)*CHAR_SIZE*x_size/rx_size,
		2*SLIDER_SIZE-CHAR_SIZE+1);
	charstr(buf);

	rectfi(SLDXL, 2*SLIDER_SIZE-CHAR_SIZE-2, SLDXH,
		2*SLIDER_SIZE-CHAR_SIZE);
	draw_slide(accum);
}


draw_slide(val)
int	val;
{
	register int	i, cur_val, tot_p;
	long	trian[3][2];
	float	x_lo, y_lo;

	color(changeback_color);
	rectfi(0, 0, 15*CHAR_SIZE, 2*SLIDER_SIZE-CHAR_SIZE-2);
	color(chartback_color);
	rectfi(3*CHAR_SIZE, 2*SLIDER_SIZE-CHAR_SIZE+1, CHAR_SIZE*10, 2*SLIDER_SIZE);
	color(text_color);
	cur_val = val/60;
	sprintf(buf, "%3d", cur_val);
	strcat(buf, "m");
	cur_val = val - cur_val*60;
	sprintf(buf2, "%2d", cur_val);
	strcat(buf, buf2);
	strcat(buf, "s");
	cmov2i(3*CHAR_SIZE, 2*SLIDER_SIZE-CHAR_SIZE+1);
	charstr(buf);

	cur_val = tot_p = 0;
	for ( i = 0; i < MAXSTEP; i++ ) {
		if ( val > cur_val + step_val[i]*step_rng[i] ) {
			cur_val += step_val[i]*step_rng[i];
			tot_p += step_rng[i];
			continue;
		}
		tot_p += ((val - cur_val + step_val[i] - 1)/step_val[i]);
		break;
	}
	x_lo = (SLDXH-SLDXL)*tot_p;
	x_lo = x_lo/sld_maxp + SLDXL;
	y_lo = 2*SLIDER_SIZE-CHAR_SIZE-2;
	pushmatrix();
	translate(x_lo, y_lo, 0.0);
	trian[0][0] = 0;
	trian[0][1] = 0;
	trian[1][0] = -2;
	trian[1][1] = -5;
	trian[2][0] = 2;
	trian[2][1] = -5;
	polf2i(3, trian);
	popmatrix();
	swapbuffers();
}


/*
 *  Compute averag value from '*used_h', averaging over 'avg_intv'.
 */
int
set_avgval()
{
	int			i;
	register tot_s;
	register float c_val;
	register struct entry	*ent;

	for ( i = 0; i < MAXITEMS; i++ ) {
		ent = used_h;
		c_val = 0;
		tot_s = 0;
		for ( ; ; ) {
			if ( tot_s + ent->intv > avg_intv )
				c_val = (c_val*tot_s+ent->data[i]*(avg_intv-tot_s))/avg_intv;
			else
				c_val = (c_val*tot_s+ent->data[i]*ent->intv)/(tot_s+ent->intv);
			tot_s += ent->intv;
			if ( tot_s >= avg_intv || !(ent = ent->f_link) ) {
				used_h->avg[i] = c_val;
				break;
			}
		}
	}
}


insert_avail(m)
struct entry *m;
{
	if ( avail_h ) {
		m->f_link = avail_h;
		avail_h->b_link = m;
		m->b_link = 0;
		avail_h = m;
	}
	else {
		avail_h = m;
		avail_h->f_link = avail_h->b_link = 0;
	}
}

release_used()
{
	register int	tm;
	register struct entry	*i, *next;

	tm = duration + avg_intv;
	if ( used_h == 0 || tot_elapse < tm )
		return;

	for ( i = used_t; tot_elapse > tm; i = next ) {
		next = i->b_link;
		tot_elapse -= i->intv;
		i->b_link->f_link = 0;
		used_t = i->b_link;
		insert_avail(i);
	}
}

rndup_dur()
{

	duration = (f_duration + smp_intv - 1) / smp_intv * smp_intv;

	if (duration/smp_intv < MIN_INT_WID)
		duration =  MIN_INT_WID * smp_intv;
}
