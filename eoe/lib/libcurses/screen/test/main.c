#include <curses.h>
#include <string.h>

static void	do_proc(void (*)(void));

static void	_a(void);
static void	_b(void);
static void	_c(void);
static void	_d(void);
static void	_e(void);
static void	_f(void);
static void	_g(void);
static void	_h(void);
static void	_i(void);
static void	_j(void);
static void	_k(void);
static void	_l(void);
static void	_m(void);
static void	_mv(void);
static void	_mvw(void);
static void	_n(void);
static void	_o(void);
static void	_p(void);
static void	_q(void);
static void	_r(void);
static void	_s(void);
static void	_slk(void);
static void	_t(void);
static void	_u(void);
static void	_v(void);
static void	_wa_wb(void);
static void	_wc_wg(void);
static void	_wh_wp(void);
static void	_wq_wz(void);
static void	_x(void);
static void	_y(void);
static void	_z(void);

static void	nothing(void){}

static	chtype buf[1024];
static	chtype *ch_str;
static	char str[1024];
static	int i;
static	int x, y;
static	chtype chtype_string[8] = {
		0xff35, 0x3543, 0x438d, 0x3463,
		0x3353, 0x3847, 0x8343, 0x0000
	};
static	WINDOW *win, *pad;

main()
{
	slk_init(0);

	use_env(FALSE);

	initscr();

	win = newwin(53, 62, 4, 2);

	pad = newpad(256, 256);

	do_proc(nothing);

	do_proc(_a);
	do_proc(_b);
	do_proc(_c);
	do_proc(_d);
	do_proc(_e);
	do_proc(_f);
	do_proc(_g);
	do_proc(_h);
	do_proc(_i);
	do_proc(_j);
	do_proc(_k);
	do_proc(_l);
	do_proc(_m);
	do_proc(_mv);
	do_proc(_mvw);
	do_proc(_n);
	do_proc(_o);
	do_proc(_p);
	do_proc(_q);
	do_proc(_r);
	do_proc(_s);
	do_proc(_slk);
	do_proc(_t);
	do_proc(_u);
	do_proc(_v);
	do_proc(_wa_wb);
	do_proc(_wc_wg);
	do_proc(_wh_wp);
	do_proc(_wq_wz);
	do_proc(_x);
	do_proc(_y);
	do_proc(_z);

	printw("\n");
	delwin(win);
	endwin();
}

static void
do_proc(void (*proc)(void))
{
	proc();

	printw("%ld", refresh());

	printw("%ld", wrefresh(win));

	printw("%ld", prefresh(win, 20, 20, 40, 40, 60, 60));
}


static void
_a(void)
{
	printw("%ld", addch((chtype) 'j'));

	printw("%ld", addchnstr(chtype_string, 4));

	printw("%ld", addchstr(chtype_string));

	printw("%ld", addnstr("Nimble too and marvel boo", 17));

	printw("%ld", addstr("Jack the lack of spades the mack"));

	printw("%ld", attroff(0x1234));

	printw("%ld", attron(0x2345));

	printw("%ld", attrset(0xf0a3));

}

static void
_b(void)
{
	printw("%ld", baudrate());

	printw("%ld", beep());

	printw("%ld", bkgd(0x3274));

	bkgdset(0x363f);

	printw("%ld", border(0x0034, 0x0034, 0x374f, 0x329a, 0x8ad3, 0x3458, 0x0035, 0x00f8));

	printw("%ld", box(win, 0x35d2, 0x0035));

}

static void
_c(void)
{
	printw("%ld", cbreak());

	printw("%ld", clear());

	printw("%ld", clearok(win, TRUE));

	printw("%ld", clrtobot());

	printw("%ld", clrtoeol());

	printw("%ld", copywin(stdscr, win, 0, 0, 35, 23, 45, 33, FALSE));

	printw("%ld", curs_set(2));

}

static void
_d(void)
{
	printw("%ld", def_prog_mode());

	printw("%ld", delay_output(5));

	printw("%ld", delch());

	printw("%ld", deleteln());

	printw("%ld", doupdate());

}

static void
_e(void)
{
	printw("%ld", echo());

	printw("%ld", echochar(0x039c));

	printw("%ld", erase());

	printw("%ld", erasechar());

}

static void
_f(void)
{
	printw("%ld", flash());

	printw("%ld", flushinp());

}

static void
_g(void)
{
	getbegyx(win, y, x);
	printw("%ld%ld", y, x);

	printw("%ld", getch());


	getmaxyx(win, y, x);
	printw("%ld%ld", y, x);

	getparyx(win, y, x);
	printw("%ld%ld", y, x);

	printw("%ld", getstr(str));

	printw("%ld", getsyx(y, x));
	printw("%ld%ld", y, x);

	getyx(win, y, x);
	printw("%ld%ld", y, x);

}

static void
_h(void)
{
	printw("%ld", halfdelay(1));

	printw("%ld", has_ic());

	printw("%ld", has_il());

}

static void
_i(void)
{
	idcok(win, TRUE);

	printw("%ld", idlok(win, FALSE));

	immedok(win, TRUE);

	printw("%ld", inch());

	ch_str = buf;
	printw("%ld", (i = inchnstr(ch_str, 5)));
	for (i = 0; i < 5; i++)
		printw("%ld", ch_str[i]);

	ch_str = buf;
	printw("%ld", inchstr(ch_str));
	while (*ch_str)
		printw("%ld", *(ch_str++));

	printw("%ld", innstr(str, 6));
	str[6] = 0;
	printw("%s", str);

	printw("%ld", insch(0x4f));

	printw("%ld", insdelln(-5));
	
	printw("%ld", insertln());

	printw("%ld", insnstr("jvn nvowernn asfdnlkas ioi nfdsnakds.",  14));

	printw("%ld", insstr("The angle of the dangle is inversely proportional to the heat of the beat."));

	printw("%ld", instr(str));
	printw("%s", str);

	printw("%ld", intrflush(stdscr, TRUE));

	printw("%ld", is_linetouched(win, 7));

	printw("%ld", is_wintouched(stdscr));

	printw("%ld", isendwin());

}

static void
_j(void)
{
}

static void
_k(void)
{
	printw("%s", keyname(0xc3d9));

	printw("%ld", keypad(stdscr, TRUE));

	printw("%ld", killchar());

}

static void
_l(void)
{
	printw("%ld", leaveok(win, FALSE));

	printw("%s", longname());

}

static void
_m(void)
{
	printw("%ld", meta(stdscr, TRUE));

	printw("%ld", move(34, 12));
}

static void
_mv(void)
{
	printw("%ld", mvaddch(12, 34, 0x35f9));

	printw("%ld", mvaddchnstr(23, 32, chtype_string, 5));

	printw("%ld", mvaddchstr(25, 3, chtype_string));

	printw("%ld", mvaddnstr(35, 23, "Munchless foo for me and you", 19));

	printw("%ld", mvaddstr(0, 3, "In a white room with black curtains"));

	printw("%ld", mvdelch(35, 24));

	printw("%ld", mvgetch(23, 0));

	printw("%ld", mvgetstr(35, 24, str));
	printw("%s", str);

	printw("%ld", mvinch(1, 10));

	ch_str = buf;
	printw("%ld", (i = mvinchnstr(4, 6, ch_str, 5)));
	for (i = 0; i < 5; i++)
		printw("%ld", ch_str[i]);

	ch_str = buf;
	printw("%ld", mvinchstr(35, 30, ch_str));
	while (*ch_str)
		printw("%ld", *(ch_str++));

	printw("%ld", mvinnstr(24, 35, str, 6));
	str[6] = 0;
	printw("%s", str);

	printw("%ld", mvinsch(20, 3, 0x69));

	printw("%ld", mvinsnstr(15, 19, "j.dj\002kd\tdjm dmo dkjf jd35",  14));

	printw("%ld", mvinsstr(13, 3, "Huh huh, huh huh, huh."));

	printw("%ld", mvinstr(13, 7, str));
	printw("%s", str);

	printw("%ld",
	       (mvprintw(24, 12, "The %s are %s only if there are %ld of them",
					"snakes", "scary", 3593)));

	printw("%ld", mvscanw(18, 12, "%s%d", str, &x));
	printw("%s%ld", str, x);
}

static void
_mvw(void)
{
	printw("%ld", mvwaddch(win, 12, 34, 0x35f9));

	printw("%ld", mvwaddchnstr(win, 23, 32, chtype_string, 5));

	printw("%ld", mvwaddchstr(win, 25, 3, chtype_string));

	printw("%ld", mvwaddnstr(win, 35, 23, "Munchless foo for me and you", 19));

	printw("%ld", mvwaddstr(win, 0, 3, "In a white room with black curtains"));

	printw("%ld", mvwdelch(win, 35, 24));

	printw("%ld", mvwgetch(win, 23, 0));

	printw("%ld", mvwgetstr(win, 35, 24, str));
	printw("%s", str);

	printw("%ld", mvwin(win, 5, 5));

	printw("%ld", mvwinch(win, 1, 10));

	ch_str = buf;
	printw("%ld", (i = mvwinchnstr(win, 4, 6, ch_str, 5)));
	for (i = 0; i < 5; i++)
		printw("%ld", ch_str[i]);

	ch_str = buf;
	printw("%ld", mvwinchstr(win, 35, 30, ch_str));
	while (*ch_str)
		printw("%ld", *(ch_str++));

	printw("%ld", mvwinnstr(win, 24, 35, str, 6));
	str[6] = 0;
	printw("%s", str);

	printw("%ld", mvwinsch(win, 20, 3, 0x69));

	printw("%ld", mvwinsnstr(win, 15, 19, ".dj\002kd\tdjm dmo dkjf jd35",  14));

	printw("%ld", mvwinsstr(win, 13, 3, "Huh huh, huh huh, huh."));

	printw("%ld", mvwinstr(win, 13, 7, str));
	printw("%s", str);

	printw("%ld",
	       (mvwprintw(win, 24, 12, "The %s are %s only if there be %ld of them",
					"snakes", "scary", 3593)));

	printw("%ld", mvwscanw(win, 18, 18, "%s%d", str, &x));
	printw("%s%ld", str, x);
}

static void
_n(void)
{
	printw("%ld", napms(23));

	printw("%ld", nl());

	printw("%ld", nocbreak());

	printw("%ld", nodelay(win, FALSE));

	printw("%ld", nonl());

	noqiflush();

	printw("%ld", noraw());

	printw("%ld", notimeout(stdscr, TRUE));

}

static void
_o(void)
{
}

static void
_p(void)
{
	printw("%ld", pechochar(pad, 0x0fff));

	printw("%ld", pnoutrefresh(pad, 44, 53, 56, 66, 100, 128));

	printw("%ld", printw("I aint no {%s|%s|%s} son",
				"senator's", "millionaire's", "military"));
}

static void
_q(void)
{
	qiflush();
}

static void
_r(void)
{
	printw("%ld", raw());

	printw("%ld", redrawwin(stdscr));

	printw("%ld", reset_prog_mode());
}

static void
_s(void)
{
	printw("%ld", scanw("%d %s %d", &x, str, &y));
	printw("\n%ld\t%s\t%ld\n", x, str, y);

	printw("%ld", scroll(stdscr));

	printw("%ld", scrollok(win, TRUE));

	printw("%ld", setscrreg(5, 33));

	printw("%ld", setsyx(33, 32));

	printw("%ld", scrl(-5));

	printw("%ld", standend());

	printw("%ld", standout());

	printw("%ld", syncok(win, TRUE));
}

static void
_slk(void)
{
	strcpy(str, "Lbl ");
	x = (int) strlen(str);
	str[x + 1] = '\000';
	for (i = 1; i <= 8; i++) {
		str[x] = (char) (i + '0');
		printw("%ld", slk_set(i, str, (i<=3 ? 0 : (i<=5 ? 1 : 2))));
	}

	printw("%ld", slk_refresh());

	printw("%ld", slk_clear());

	printw("%ld", slk_attrset(0xacfc));

	printw("%ld", slk_attrset(0xacfc));

	printw("%ld", slk_attron(0x0515));

	printw("%ld", slk_attroff(0x4301));

	for (i = 1; i <= 8; i++)
		printw("%s", slk_label(i));

	printw("%ld", slk_touch());

	printw("%ld", slk_restore());

	printw("%ld", slk_noutrefresh());
}

static void
_t(void)
{
	printw("%ld", termattrs());

	printw("%s", termname());

	timeout(5);

	printw("%ld", touchline(stdscr, 5, 3));

	printw("%ld", touchwin(win));

	printw("%ld", typeahead(-1));
}

static void
_u(void)
{
	printw("%s", unctrl('\173'));

	if (ungetch(0x8534) == ERR)
		printw("%ld", ERR);
	else
		printw("%ld", OK);

	printw("%ld", untouchwin(stdscr));
}

static void
_v(void)
{
	/* vwprintww called by other printw functions */

	/* vwscanw called by other scanw functions */
}

static void
_wa_wb(void)
{
	printw("%ld", waddch(stdscr, (chtype) 'j'));

	printw("%ld", waddchnstr(pad, chtype_string, 4));

	printw("%ld", waddchstr(win, chtype_string));

	printw("%ld", waddnstr(win, "Nimble too and marvel boo", 17));

	printw("%ld", waddstr(pad, "Jack the lack of spades the mack"));

	printw("%ld", wattroff(win, 0x1234));

	printw("%ld", wattron(stdscr, 0x2345));

	printw("%ld", wattrset(win, 0xf0a3));

	printw("%ld", wbkgd(win, (chtype) '_'));

	wbkgdset(stdscr, 0x8322);

	printw("%ld", wborder(pad, 0x9334, 0x3839, 0xfc34, 0x3939,
				   0x9334, 0x3839, 0xfc34, 0x3939));

}

static void
_wc_wg(void)
{
	printw("%ld", wclear(stdscr));

	printw("%ld", wclrtobot(pad));

	printw("%ld", wclrtoeol(win));

	printw("%ld", wdelch(pad));

	printw("%ld", wdeleteln(win));

	printw("%ld", wechochar(pad, 0x8939));

	printw("%ld", werase(win));

	printw("%ld", wgetch(stdscr));

	printw("%ld", wgetnstr(win, str, 5));
	printw("%s", str);

	printw("%ld", wgetstr(pad, str));
	printw("%s", str);
}

static void
_wh_wp(void)
{
	printw("%ld", whline(win, 0x3f9d, 8));

	printw("%ld", winch(win));

	ch_str = buf;
	printw("%ld", (i = winchnstr(win, ch_str, 5)));
	for (i = 0; i < 5; i++)
		printw("%ld", ch_str[i]);

	ch_str = buf;
	printw("%ld", winchstr(pad, ch_str));
	while (*ch_str)
		printw("%ld", *(ch_str++));

	printw("%ld", winnstr(win, str, 6));
	str[6] = 0;
	printw("%s", str);

	printw("%ld", winsch(win, 0x4f));

	printw("%ld", winsdelln(win, -5));
	
	printw("%ld", winsertln(stdscr));

	printw("%ld", winsnstr(pad, "jvn nvowernn asfdnlkas ioi nfdsnakds.",  14));

	printw("%ld", winsstr(stdscr, "The angle of the dangle is inversely proportional to the heat of the beat."));

	printw("%ld", winstr(win, str));
	printw("%s", str);
	
	printw("%ld", wmove(win, 18, 22));

	printw("%ld", wnoutrefresh(win));

	printw("%ld", wprintw(win, "faded as my %s", "jeans"));
}

static void
_wq_wz(void)
{
	printw("%ld", wredrawln(pad, 6, 20));

	printw("%ld", wscanw(win, "%d%s", &y, str));
	printw("%ld%s", y, str);

	printw("%ld", wscrl(win, 3));

	printw("%ld", wsetscrreg(pad, 5, 22));

	printw("%ld", wstandend(win));

	printw("%ld", wstandout(win));

	wtimeout(win, 35);

	printw("%ld", wtouchln(stdscr, 6, 7, 9));

	printw("%ld", wvline(win, 0xfccf, 12));
}

static void
_x(void)
{

}

static void
_y(void)
{

}

static void
_z(void)
{

}



