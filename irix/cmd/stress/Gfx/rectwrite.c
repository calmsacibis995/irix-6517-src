#include "gl.h"
#include "device.h"
#include "stdio.h"
#include "signal.h"

extern int getopt(), optind;
extern char *optarg;
int x = -1;
int y = -1;
int runtime = -1;
int quit = 0;

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
    register int i, c;
    register unsigned short *parray1, *parray2, *sp;
    short dummy;

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

    if ((parray1 = (unsigned short *)malloc(100*100*sizeof(unsigned short)))
	           == NULL) {
	printf("malloc failed\n");
	exit(1);
    }
    for (i = 0, sp = parray1; i < 100*100; i++, sp++) {
	*sp = CYAN;
    }

    if ((parray2 = (unsigned short *)malloc(100*100*sizeof(unsigned short)))
	           == NULL) {
	printf("malloc failed\n");
	exit(1);
    }
    for (i = 0, sp = parray2; i < 100*100; i++, sp++) {
	*sp = MAGENTA;
    }

    if (x > 0 && y > 0)
	prefposition(x, x+99, y, y+99);
    else
    	prefsize(100,100);
    foreground();
#if _MIPS_SIM == _MIPS_SIM_NABI32
    winopen("N32 rectwrite");
#else
    winopen("rectwrite");
#endif
    cmode();
    gconfig();

    if (runtime > 0) {
	sigset(SIGALRM, sigalarm);
	alarm(runtime);
    }

    while (!quit) {
	while (qtest())
	    if (qread(&dummy) == REDRAW)
		reshapeviewport();
	rectwrite(0, 0, 99, 99, parray1); /* cyan */
	sginap(50);
	rectwrite(0, 0, 99, 99, parray2); /* magenta */
	sginap(50);
    }

}
