#include "gl.h"
#include "device.h"
#include "stdio.h"
#include "signal.h"

#define MAXWINS 32

extern int getopt(), optind;
extern char *optarg;
int x = -1;
int y = -1;
int runtime = -1;
int quit = 0;
int numwins = 4;

static void
sigalarm(sig)
int sig;
{
	quit = 1;
}

main(argc, argv)
int argc;
char **argv;
{
    short dummy;
    int curwid;
    int wids[MAXWINS];
    int c;

    while ((c = getopt(argc, argv, "n:t:x:y:")) != EOF) {
	switch (c) {
	  case 'n':
		numwins = atoi(optarg);
		if (numwins > MAXWINS) {
		    printf("%s: too many windows requested (%d), limit is %d\n",
			argv[0], numwins, MAXWINS);
		    exit(1);
		}
		break;
	  case 't':
		runtime = atoi(optarg);
		break;
	  case 'x':
		x = atoi(optarg);
		break;
	  case 'y':
		y = atoi(optarg);
		break;
	  default:
		printf("invalid option %c\n", c);
		exit(1);
	}
    }

    for (curwid = 0; curwid < numwins; curwid++) {
	foreground();
        if (x > 0 && y > 0) {
	    prefposition(x, x+99, y, y+99);
	    x -= 20;
	    y -= 20;
        } else
    	    prefsize(100,100);

#if _MIPS_SIM == _MIPS_SIM_NABI32
    	wids[curwid] = winopen("N32 winset");
#else
    	wids[curwid] = winopen("winset");
#endif
	if (wids[curwid] < 0) {
	    printf("winopen %d fails\n", curwid);
	    exit(1);
	}
        RGBmode();
        gconfig();
    }

    if (runtime > 0) {
	sigset(SIGALRM, sigalarm);
	alarm(runtime);
    }

    while (!quit) {
	for (curwid = 0; curwid < numwins; curwid++) {
	    winset(wids[curwid]);
	    RGBcolor(0,0,255); /* blue */
	    clear();
	    sginap(numwins > 1 ? 10 : 50);
	    while (qtest())
	        if (qread(&dummy) == REDRAW)
		    reshapeviewport();
	}
	for (curwid = 0; curwid < numwins; curwid++) {
	    winset(wids[curwid]);
	    RGBcolor(255,0,0); /* red */
	    clear();
	    sginap(numwins > 1 ? 10 : 50);
	    while (qtest())
	        if (qread(&dummy) == REDRAW)
		    reshapeviewport();
	}
    }

}
