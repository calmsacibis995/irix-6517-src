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
	register long i = 0;
	int c;

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
		prefposition(x, x+99, y, y+49);
	else
		prefsize(100,50);
	foreground();
#if _MIPS_SIM == _MIPS_SIM_NABI32
	winopen("N32 modes");
#else
	winopen("modes");
#endif

	if (runtime > 0) {
		sigset(SIGALRM, sigalarm);
		alarm(runtime);
	}

	mapcolor(100,255,0,0);

	while (!quit) {
		if (i = 1 - i) {
			cmode();
			gconfig();
			color(100);
			clear();
		} else {
			RGBmode();
			gconfig();
			RGBcolor(0,0,255);
			clear();
		}
	}

}
