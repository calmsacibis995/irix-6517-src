#include	<sys/types.h>
#include	<sys/sysmp.h>
#include	<sys/sysinfo.h>
#include	<time.h>
#include	<stdio.h>
#include	<unistd.h>
#include	<curses.h>

#define DECAY_PER_SECOND	0.25
#define UPDATE_PERIOD		0.25

/*
 * Test program for load module
 */

#include "load.h"

#define COL_WIDTH	1
#define COLCPU0		7
#define COL(cpu)	(COLCPU0 + (cpu) * COL_WIDTH)

int opt_nodemode=1;

main(int argc, char **argv)
{
    int		i, j;
    int		ncpus;
    int		clk_tck, period_tck;
    u_char     *p;

    clk_tck = CLK_TCK;
    period_tck = (int) (CLK_TCK * UPDATE_PERIOD + 0.5);

    initscr();
    clear();

    ncpus = load_cpu_get();

    mvaddstr(LINES - 5, 2, "0%");
    mvaddstr(0, 0, "100%");
    mvaddstr(LINES - 3, 3, "C");
    mvaddstr(LINES - 2, 3, "P");
    mvaddstr(LINES - 1, 3, "U");
    mvaddstr(LINES - 12, 0, "u)ser");
    mvaddstr(LINES - 11, 0, "s)ys");
    mvaddstr(LINES - 10, 0, "i)ntr");
    mvaddstr(LINES - 9, 0, "d)isk");
    mvaddstr(LINES - 8, 0, "g)fx");

    for (i = 0; i < ncpus; i++) {
	mvprintw(LINES - 1, COL(i), "%d", i % 10);
	mvprintw(LINES - 2, COL(i), "%d", i / 10 % 10);
	mvprintw(LINES - 3, COL(i), "%d", i / 100);
    }

    while (1) {
	p = load_graph(1, DECAY_PER_SECOND, UPDATE_PERIOD);

	for (i = 0; i < ncpus; i++) {
	    u_char	h_user, h_sys, h_intr, h_io, h_gfx;

	    h_user = *p++ * (LINES - 4) / 255;
	    h_sys  = *p++ * (LINES - 4) / 255;
	    h_intr = *p++ * (LINES - 4) / 255;
	    h_io   = *p++ * (LINES - 4) / 255;
	    h_gfx  = *p++ * (LINES - 4) / 255;

	    for (j = 0; j < LINES - 4; j++)
		mvaddch(LINES - 5 - j, COL(i),
			j < h_user ? 'u' :
			j < h_sys  ? 's' :
			j < h_intr ? 'i' :
			j < h_io   ? 'd' :
			j < h_gfx  ? 'g' :
			' ');
	}

	move(LINES - 1, 0);

	refresh();

	sginap(period_tck);
    }
}

void
cleanup(void) {
	/* needed by error.c */
	return;
}
