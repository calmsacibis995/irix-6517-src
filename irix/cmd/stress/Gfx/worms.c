/*

	 @@@        @@@    @@@@@@@@@@     @@@@@@@@@@@    @@@@@@@@@@@@
	 @@@        @@@   @@@@@@@@@@@@    @@@@@@@@@@@@   @@@@@@@@@@@@@
	 @@@        @@@  @@@@      @@@@   @@@@           @@@@ @@@  @@@@
	 @@@   @@   @@@  @@@        @@@   @@@            @@@  @@@   @@@
	 @@@  @@@@  @@@  @@@        @@@   @@@            @@@  @@@   @@@
	 @@@@ @@@@ @@@@  @@@        @@@   @@@            @@@  @@@   @@@
	  @@@@@@@@@@@@   @@@@      @@@@   @@@            @@@  @@@   @@@
	   @@@@  @@@@     @@@@@@@@@@@@    @@@            @@@  @@@   @@@
	    @@    @@       @@@@@@@@@@     @@@            @@@  @@@   @@@

				 Eric P. Scott
			  Caltech High Energy Physics
				 October, 1980

		    Ported to the iris by Paul Haeberli 1984

*/
#include "stdio.h"
#include "gl.h"
#include "device.h"
#include "signal.h"

#define GREY(x)	greyi((x)*255/24)

extern int getopt(), optind;
extern char *optarg;
int winx = -1;
int winy = -1;
int runtime = -1;
int quit = 0;

static void
sigalarm(sig)
int sig;
{
	quit = 1;
}

#define INCREMENT	1.0

#define MAXCOLS	100
#define MAXROWS	75

#define SEG0		20
#define SEG1		21
#define TRAIL_OBJ 	22

int Wrap;
short *ref[MAXROWS];

static int flavor[]={
    1, 2, 3, 4, 5, 6 
};

static int segobj[]={
    SEG1, SEG0, SEG0, SEG0, SEG0, SEG0 
};

static short 

xinc[]= {
     1,  1,  1,  0, -1, -1, -1,  0
},

yinc[]= {
    -1,  0,  1,  1,  1,  0, -1, -1
};

static struct worm {
    int orientation, head;
    short *xpos, *ypos;
} worm[40];

static char *field;
static int length=16, number=3, trail=' ';

static struct options {
    int nopts;
    int opts[3];
} normopts[8]={
    { 3, { 7, 0, 1 } },
    { 3, { 0, 1, 2 } },
    { 3, { 1, 2, 3 } },
    { 3, { 2, 3, 4 } },
    { 3, { 3, 4, 5 } },
    { 3, { 4, 5, 6 } },
    { 3, { 5, 6, 7 } },
    { 3, { 6, 7, 0 } }
}, upper[8]={
    { 1, { 1, 0, 0 } },
    { 2, { 1, 2, 0 } },
    { 0, { 0, 0, 0 } },
    { 0, { 0, 0, 0 } },
    { 0, { 0, 0, 0 } },
    { 2, { 4, 5, 0 } },
    { 1, { 5, 0, 0 } },
    { 2, { 1, 5, 0 } }
}, left[8]={
    { 0, { 0, 0, 0 } },
    { 0, { 0, 0, 0 } },
    { 0, { 0, 0, 0 } },
    { 2, { 2, 3, 0 } },
    { 1, { 3, 0, 0 } },
    { 2, { 3, 7, 0 } },
    { 1, { 7, 0, 0 } },
    { 2, { 7, 0, 0 } }
}, right[8]={
    { 1, { 7, 0, 0 } },
    { 2, { 3, 7, 0 } },
    { 1, { 3, 0, 0 } },
    { 2, { 3, 4, 0 } },
    { 0, { 0, 0, 0 } },
    { 0, { 0, 0, 0 } },
    { 0, { 0, 0, 0 } },
    { 2, { 6, 7, 0 } }
}, lower[8]={
    { 0, { 0, 0, 0 } },
    { 2, { 0, 1, 0 } },
    { 1, { 1, 0, 0 } },
    { 2, { 1, 5, 0 } },
    { 1, { 5, 0, 0 } },
    { 2, { 5, 6, 0 } },
    { 0, { 0, 0, 0 } },
    { 0, { 0, 0, 0 } }
}, upleft[8]={
    { 0, { 0, 0, 0 } },
    { 0, { 0, 0, 0 } },
    { 0, { 0, 0, 0 } },
    { 0, { 0, 0, 0 } },
    { 0, { 0, 0, 0 } },
    { 1, { 3, 0, 0 } },
    { 2, { 1, 3, 0 } },
    { 1, { 1, 0, 0 } }
}, upright[8]={
    { 2, { 3, 5, 0 } },
    { 1, { 3, 0, 0 } },
    { 0, { 0, 0, 0 } },
    { 0, { 0, 0, 0 } },
    { 0, { 0, 0, 0 } },
    { 0, { 0, 0, 0 } },
    { 0, { 0, 0, 0 } },
    { 1, { 5, 0, 0 } }
}, lowleft[8]={
    { 3, { 7, 0, 1 } },
    { 0, { 0, 0, 0 } },
    { 0, { 0, 0, 0 } },
    { 1, { 1, 0, 0 } },
    { 2, { 1, 7, 0 } },
    { 1, { 7, 0, 0 } },
    { 0, { 0, 0, 0 } },
    { 0, { 0, 0, 0 } }
}, lowright[8]={
    { 0, { 0, 0, 0 } },
    { 1, { 7, 0, 0 } },
    { 2, { 5, 7, 0 } },
    { 1, { 5, 0, 0 } },
    { 0, { 0, 0, 0 } },
    { 0, { 0, 0, 0 } },
    { 0, { 0, 0, 0 } },
    { 0, { 0, 0, 0 } }
};

int m1, m2, m3;
int coffset;
int slowmode;
int bigblox;
int CO, LI;
int napcount = 2;

main(argc,argv)
int argc;
char *argv[];
{
    float ranf();
    register int x, y;
    register int n;
    register struct worm *w;
    register struct options *op;
    register int h;
    register short *ip;
    int last, bottom;
    char *tcp;
    register char *term;
    char tcb[100];
    int c;

    while ((c = getopt(argc, argv, "t:x:y:s:")) != EOF) {
	switch (c) {
	  case 't':
		runtime = atoi(optarg);
		break;
	  case 'x':
		winx = atoi(optarg);
		break;
	  case 'y':
		winy = atoi(optarg);
		break;
	  case 's':
		napcount = atoi(optarg);
		break;
	  default:
		printf("invalid option %c\n", c);
		exit(1);
	}
    }

    srand(getpid());
    CO = MAXCOLS;
    LI = MAXROWS;
    CO = 60;
    LI = 45;
    bottom = LI-1;
    last = CO-1;

/* make a work area */

    if (winx > 0 && winy > 0) {
	prefposition(winx, winx+200, winy, winy+150);
    }
    keepaspect(400,300);
    foreground();
#if _MIPS_SIM == _MIPS_SIM_NABI32
    winopen("N32 worms");
    wintitle("N32 worms");
#else
    winopen("worms");
    wintitle("worms");
#endif
    makeframe();

    makeobjects();
    qdevice(RIGHTMOUSE);
    qdevice(MIDDLEMOUSE);
    qdevice(LEFTMOUSE);

#ifdef notdef
    for (x=1;x<argc;x++) {
	register char *p;
	p=argv[x];
	if (*p=='-') p++;
	switch (*p) {
	case 'f':
	    field="WORM";
	    break;
	case 'l':
	    if (++x==argc) goto usage;
	    if ((length=atoi(argv[x]))<2||length>1024) {
		fprintf(stderr,"%s: Invalid length\n",*argv);
		exit(1);
	    }
	    break;
	case 's':
	    if (++x==argc) goto usage;
	    napcount = atoi(argv[x]);
	    break;
	case 'n':
	    if (++x==argc) goto usage;
	    if ((number=atoi(argv[x]))<1||number>40) {
		fprintf(stderr,"%s: Invalid number of worms\n",*argv);
		exit(1);
	    }
	    break;
	case 't':
	    trail='.';
	    break;
	default:
	usage:
	    fprintf(stderr,
		"usage: %s [-field] [-length #] [-number #] [-trail]\n",*argv);
	    exit(1);
	    break;
	}
    }
#endif /* notdef */
    ip=(short *)malloc(LI*CO*sizeof (short));
    for (n=0;n<LI;) {
	ref[n++]=ip; ip+=CO;
    }

    for (ip=ref[0],n=LI*CO;--n>=0;) 
	*ip++=0;

    if (Wrap) ref[bottom][last]=1;

    for (n=number, w= &worm[0];--n>=0;w++) {
	w->orientation=w->head=0;
	if (!(ip=(short *)malloc(length*sizeof (short)))) {
	    fprintf(stderr,"%s: out of memory\n",*argv);
	    exit(1);
	}
	w->xpos=ip;
	for (x=length;--x>=0;) *ip++ = -1;
	if (!(ip=(short *)malloc(length*sizeof (short)))) {
	    fprintf(stderr,"%s: out of memory\n",*argv);
	    exit(1);
	}
	w->ypos=ip;
	for (y=length;--y>=0;) *ip++ = -1;
    }

    if (field) {
	register char *p;
	pushmatrix();
	    p=field;
	    for (y=bottom;--y>=0;) {
	        pushmatrix();
	            for (x=CO;--x>=0;)  {
		        putfield();
		        translate(INCREMENT,0.0,0.0);
		    }
	        popmatrix();
	        translate(0.0,INCREMENT,0.0);
	    }
	popmatrix();
    }
    gflush();

    if (runtime > 0) {
	sigset(SIGALRM, sigalarm);
	alarm(runtime);
    }

    while (!quit) {
	checkmouse();
	for (n=0,w= &worm[0];n<number;n++,w++) {
    	    sginap(napcount);
	    if ((x=w->xpos[h=w->head])<0) {
		x=w->xpos[h]=0;
		y=w->ypos[h]=bottom;
		pushmatrix();
		    translate((float)x,(float)y,0.0);
		    if(bigblox)
			scale(2.0,2.0,1.0);
		    putsegment(flavor[n%6],segobj[n%6]);
		popmatrix();
		ref[y][x]++;
	    }
	    else y=w->ypos[h];
	    if (++h==length) h=0;
	    if (w->xpos[w->head=h]>=0) {
		register int x1, y1;
		x1=w->xpos[h]; y1=w->ypos[h];
		if (--ref[y1][x1]==0) {
		    pushmatrix();
			translate((float)x1,(float)y1,0.0);
			puttrail();
		    popmatrix();
		}
	    }
            op= &(x==0 ? (y==0 ? upleft : (y==bottom ? lowleft : left)) :
                (x==last ? (y==0 ? upright : (y==bottom ? lowright : right)) :
		(y==0 ? upper : (y==bottom ? lower : normopts))))[w->orientation];
	    switch (op->nopts) {
	    case 0:
		fflush(stdout);
		abort();
		return 1;
	    case 1:
		w->orientation=op->opts[0];
		break;
	    default:
		w->orientation=op->opts[(int)(ranf()*(float)op->nopts)];
	    }
	    x+=xinc[w->orientation];
	    y+=yinc[w->orientation];
	    if (!Wrap||x!=last||y!=bottom) {
	        pushmatrix();
		    translate((float)x,(float)y,0.0);
		    if(bigblox)
			scale(2.0,2.0,1.0);
		    putsegment(flavor[n%6],segobj[n%6]);
	        popmatrix();
	    }
	    ref[w->ypos[h]=y][w->xpos[h]=x]++;
	}
    }
}

checkmouse()
{
    short dev, val;
    static int upcount;

    if(upcount++ != 20)
	return 0;
    gflush();
    if(slowmode)
	sleep(2);
    upcount = 0;
    gsync();
    while(qtest()) {
	dev = qread(&val);
	switch(dev) {
	    case RIGHTMOUSE: m1 = val;
			 coffset++;
			 break;
	    case MIDDLEMOUSE: m2 = val;
			 if(val)
			     slowmode = 1-slowmode;
			 break;
	    case LEFTMOUSE: m3 = val;
			 if(val)
			     bigblox = 1-bigblox;
			 break;
	    case REDRAW:
			 reshapeviewport();
			 makeframe();
			 break;
        }
	if(m1 && m3) {
	    color(GREY(2));
	    clear();
	}
    }
    return 0;
}

float ranf() {
    return ((rand()>>1) % 10000)/10000.0;

}

putfield()
{
    color(3);
    callobj(SEG0);
    return 0;
}

putsegment(col,obj)
int col;
int obj;
{
    color((col+coffset)%8);
    callobj(obj);
    return 0;
}

puttrail()
{
    callobj(TRAIL_OBJ);
    return 0;
}

makeobjects()
{
    makeobj(SEG0);
        rectf(-INCREMENT/3.0,-INCREMENT/3.0,INCREMENT/3.0,INCREMENT/3.0);
    closeobj();

    makeobj(SEG1);
        rectf(-INCREMENT/3.0,-INCREMENT/3.0,INCREMENT/3.0,INCREMENT/3.0);
    closeobj();

    makeobj(TRAIL_OBJ);
	color(GREY(8));
        rectf(-INCREMENT/3.0,-INCREMENT/3.0,INCREMENT/3.0,INCREMENT/3.0);
    closeobj();
    return 0;
}

makeframe()
{
    color(GREY(2));
    clear();
    ortho2(-1.5,CO+0.5,-1.5,LI+0.5);
    color(GREY(15));
    recti(-1,-1,CO,LI);
    return 0;
}
