#include "gl.h"
#include "device.h"
#include "stdio.h"
#include "signal.h"
#include "sys/types.h"
#include "sys/prctl.h"
#include "sys/signal.h"

#define NUMPROCS 4

extern int getopt(), optind;
extern char *optarg;
int x = -1;
int y = -1;
int runtime = -1;
int quit = 0;

volatile int turn = 0;
void slave(int);

static void
sigalarm(sig)
int sig;
{
	quit = 1;
}

int isIgloo( void );

long windowId = 0;

main(argc, argv)
int argc;
char **argv;
{
    register int i, c;

    while ((c = getopt(argc, argv, "t:x:y:")) != EOF) {
	switch (c) {
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
    if (x > 0 && y > 0)
	prefposition(x, x+299, y, y+299);
    else
    	prefsize(300,300);
    foreground();

    windowId = winopen(
#if _MIPS_SIM == _MIPS_SIM_NABI32
		"N32 parallel");
#else
		"parallel");
#endif

    cmode();
    doublebuffer();
    gconfig();

    if (runtime > 0) {
	sigset(SIGALRM, sigalarm);
	alarm(runtime);
    }

    prctl(PR_SETEXITSIG, SIGTERM);

    for (i = 1; i < NUMPROCS; i++) {
	if (sproc((void(*)(void *))slave, PR_SALL, i) < 0) {
		perror("sproc");
		exit(1);
	}
    }
    slave(0);
}

void
slave(int id)
{
    int igloo;
    short dummy;
    register int next;

    igloo = isIgloo();

    while (!quit) {
	/*
	 * Wait until it's my turn.
	 */
	while (turn != id)
	    sginap(1);

        if (igloo)
            winset( windowId );

	while (qtest())
	    if (qread(&dummy) == REDRAW)
		reshapeviewport();
	color(GREEN);
	clear();
        swapbuffers();
	color(YELLOW);
	clear();
	swapbuffers();

        /*
         * In igloo, release the current context
         * so that another thread may use it.
         */
        if (igloo)
	    winset(0);

	/*
	 * Pass the torch. Using "next" is a probably over-conservative.
	 */
	next = (turn+1) % NUMPROCS;
	turn = next;

	sginap(50);
    }
}

int isIgloo( void )
{
    char version[64];
    static int result = -1;

    /*
     * Assume an unknown machine uses igloo.
     */
    if ( -1 == result )
    {
        gversion(version);

        if (	0 == strncmp("GL4DGT", version, 6) ||
		0 == strncmp("GL4DVG", version, 6) ||
		0 == strncmp("GL4DRE", version, 6) ||
		0 == strncmp("GL4DPI", version, 6) ||
		0 == strncmp("GL4DLG", version, 6) ||
		0 == strncmp("GL4DXG", version, 6) ||
		0 == strncmp("GL4DNP", version, 6))
        {
            result = 0;
        }
        else
        {
            result = 1;
        }
    }
    return result;
}

