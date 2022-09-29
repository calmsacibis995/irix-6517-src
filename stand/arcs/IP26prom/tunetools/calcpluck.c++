/*
 * program to precalculate values needed by the plucked string
 * (Karplus-Strong) algorithm
 */
#include <audio.h>
#include <math.h>
#include <sys/types.h>
#include <time.h>
#include <dmedia/md.h>
#include <midifile.h>
#include <miditrack.h>
#include <libc.h>
#include <string.h>

#define RATE 44100.0
#define PI   M_PI

#define NOISE 0
typedef struct note_s {
    float freq;		/* fundamental frequency of the note */
    float duration;	/* duration of note in seconds */
    float amplitude;	/* amplitude of the note */
    float decay;	/* decay in dB */
} note_t;

typedef struct event_s {
    note_t *note;	/* ptr to the note to be played */
    float   start;	/* start of note in seconds */
} event_t;

typedef struct pluckparams_s {
    int period;		/* number of samples in delay buf (in frames) */
    int duration;	/* duration of the sound (in frames) */
    int start; 		/* starting location of sound (in frames) */
    float amplitude;	/* amplitude of the note */
    float rho;		/* decay coefficient */
    float s;		/* filter coefficients */
    float s1;		/* filter coefficients */
    float apcoeff;	/* all-pass filter coefficient */
} pluckparams_t;

void usage(char *str);
int  calcbuflen(event_t *eventlist, int listlen, float rate);
void randfill(float *buf, int n, float amp);
void pluck(pluckparams_t *params, short *soundbuf);
void calcparams(event_t *event, pluckparams_t *params);
void printparams(char *prefix, pluckparams_t *plist, int len);
void make_note_to_freq_table(void);

#define NEVENTS 100
pluckparams_t params[NEVENTS];

double freq_tab[256];
char optstr[] = "nd:t:p:f:s";

void
main(int argc, char **argv)
{
    int n, tempo, division, buflen, i, maxperiod = 0, nevents, noisy;
    short *buf;		/* sample buffer */
    ALport port;
    char *prefix, *filename;
    MFfile file;
    MDevent t[NEVENTS];
    double nanopertick;
    event_t list[NEVENTS];
    note_t notes[NEVENTS];   /* a note for each event */
    float startoffset;
    int offset = 1;

    extern char *optarg;
    extern int optind, opterr, optopt;
    int ch;
    
    noisy = 0;
    prefix = "foo";
    tempo = 0;
    division = 0;
    filename = "";
    
    while ((ch = getopt(argc, argv, optstr)) != EOF) {
	switch (ch) {
	    case 'p':	/* prefix */
		prefix = optarg;
		break;
	    case 't':	/* tempo */
		tempo = atoi(optarg);
		break;
	    case 'd':	/* division */
		division = atoi(optarg);
		break;
	    case 'f':	/* filename */
		filename = optarg;
		break;
	    case 'n':
		noisy = 1;
		break;
	    case 's':
	    	offset = 0;
	    	break;
	    default:
		usage(argv[0]);
		exit(-1);
		break;
	}
    }
   
    if (!strcmp(filename, "")) {
	usage(argv[0]);
	exit(-1);
    }
    make_note_to_freq_table();
#if DEBUG
    for (i=0;i<88;i++) printf("freq_tab[%d]=%f\n", i, freq_tab[i]);
#endif
    file.open(filename, "r");
    file.read();
    file.rewind();

    if (!division) division = file.division();
    if (!tempo) tempo = file.gettempo();
    nanopertick = 1000.0 * tempo / division;
#if DEBUG
    printf("prefix: %s tempo = %d division = %d file: %s\n", 
	prefix, tempo, division, filename);
#endif    
    nevents = 0;
    
    /* give each event its own note struct */
    for (i = 0; i < NEVENTS; i++) {
	list[i].note = &notes[i];
    }
    
    while (n = file.nextevent(t,NEVENTS)) {
	char stat, chan, note, vel;
	unsigned long long tstamp;
	int i;
	float secs;
    
	for (i = 0; i < n; i++) {       /* more than one event may occur */
	    stat = t[i].msg[0]>>4;      /* status nibble */
	    chan = t[i].msg[0]&0xf;     /* channel nibble */
	    tstamp = t[i].stamp;
	    secs = nanopertick * tstamp * 1.0e-9;
	    if (stat == 0x9) {          /* note on */
		note = t[i].msg[1];
		vel = t[i].msg[2];
		if (vel != 0) {
		    /*notes[nevents].amplitude = 16767.0;*/
		    notes[nevents].amplitude = vel/128.0*32767;
		    notes[nevents].freq = freq_tab[note];	/* hertz */
		    notes[nevents].duration = 2.0;	/* seconds */
		    notes[nevents].decay = 40.0;	/* dB */
		    list[nevents].start = secs;	/* seconds */
		    nevents++;
		}
#if DEBUG
    printf("note on: note=%d vel=%d\ttick=%lld time=%f ",
	    note, vel, tstamp, secs);
		printf("amp=%f freq=%f\n", notes[nevents-1].amplitude, 
		    notes[nevents-1].freq);
#endif
	    }
#if NOTYET
	    else if (stat == 0x8) {     /* note off */
		note = t[i].msg[1];
		vel = t[i].msg[2];
		printf("note off: chan=%d note=%d vel=%d\ttick=%lld\ttime=%f\n",                    chan, note, vel, tstamp, nanotime);
	    }
#endif
	}
    }

    if (!offset) {
    	startoffset = list[0].start;
    }
    else {
    	startoffset = 0.0f;
    }
    printf("first note start = %f\n", list[0].start);	
    
    for (i = 0; i < nevents; i++) {
	list[i].start -= startoffset;
    }
    
    buflen = calcbuflen(&list[0], nevents, RATE);
    /*
     * calculate plucked string parameters for each event
     * in the event list
     */
    for (i = 0; i < nevents; i++) {
	calcparams(&list[i], &params[i]);
    }
    /*
     * find the max period (delay line length) for all the
     * parameters. this will dictate how much memory is 
     * allocated for the delay lines in the prom.
     */
    for (i = 0; i < nevents; i++) {
	if (params[i].period > maxperiod) maxperiod = params[i].period;
    }
    if (noisy) {
	/* start with a new seed for random() */
	srandom(time(0));
	buf = (short*)malloc(2*buflen*sizeof(short));
	bzero(buf, 2*buflen*sizeof(short));
	for (i = 0; i < nevents; i++) {
	    pluck(&params[i], buf);
	}
    }

    printf("/******* %s parameter list *******/\n\n", prefix);
    printf("#define %s_buflen      %d\n", prefix, buflen);
    printf("#define %s_maxperiod   %d\n\n", prefix,
	maxperiod);
    printparams(prefix, params, nevents);
    if (noisy) {
	port = ALopenport("pluck", "w", NULL);
	ALwritesamps(port, buf, buflen*2);
	while (ALgetfilled(port)) sleep(1);
	ALcloseport(port);
    }
}

char options[] = "[-n] [-p prefix] [-t tempo] [-d division] -f filename\n\
    -n		'noisy' (i.e. play the synthesized tune)\n\
    -p prefix	set the prefix of the parameter array\n\
    -t tempo	override the tempo of the MIDI file (integer)\n\
    -d division	override the division value in the MIDI file (integer)\n\
    -s		subtract offset of first MIDI note (i.e. start tune at zero)\n\
    -f filename name of the MIDI file\n";
    
void
usage(char *str)
{
    printf("Usage: %s %s", str, options);
}

double twelve_ratio= 1.05946309435929526455;
void
make_note_to_freq_table(void)
{
    int i;

    freq_tab[0]=55.0/4.0*sqrt(sqrt(2.0));
    for(i=1;i<=12;i++){
        freq_tab[i]=freq_tab[i-1]*twelve_ratio;
    }
    for(i=12;i<256;i++)freq_tab[i]=2*freq_tab[i-12];
}

#define FABS(x)	(x < 0.0 ? -(x):(x))

void
randfill(float *buf, int n, float amp)
/*
 * fill buf with n random numbers in the range (-amp,amp)
 */
{
    int i;
    float scale;
    float avg, sum;
    float tmp, max;

    scale = 1.0/(float)0x7fffffff;
    for (i = 0; i < n; i++) {
	buf[i] = 2.0*(scale*random()-0.5);
    }
    sum=0;
    for (i = 0; i < n; i++) {
	sum += buf[i];
    }
    avg = sum/n;
    for (i = 0; i < n; i++) {
	buf[i] -= avg;
    }
    max=0;
    for (i = 0; i < n; i++) {
	tmp = FABS(buf[i]);
	if (tmp > max) max = tmp;
    }
    scale = amp/max;
    for (i = 0; i < n; i++) {
	buf[i] *= scale;	
    }
#ifdef NOWAY
    for (i = 0; i < n; i++) {
	printf("%d %f\n", i, buf[i]);	
    }
#endif
}

int
calcbuflen(event_t *eventlist, int listlen, float rate)
/*
 * calculate the length of the buffer needed to hold all the 
 * events in the event list. returns the length of the buffer
 * in frames...not samples. thus, in order to put out stereo,
 * we need to allocate a buffer of 2x the length returned by
 * this function.
 */
{
    int i, frames;
    float endtime, maxendtime=0.0;

    for (i = 0; i < listlen; i++) {
	endtime = eventlist[i].start + eventlist[i].note->duration;	
	if (endtime > maxendtime) maxendtime = endtime;
    }
    /*printf("maxendtime = %f rate = %f\n", maxendtime, rate);*/
    frames = maxendtime * rate; 
    return(frames);
}

#define MIN(x, y)   (x < y ? x : y)

void
pluck(pluckparams_t *params, short *buf)
{
    int	 offset, notelen;
    int i;
    float sample, lastsample, res1, res2, lastres1, lastres2;
    float *delaybuf, *bptr, *bend;
    
    /* create delay line */
    printf("delay line length = %d\n", params->period);

    delaybuf = (float*)malloc(params->period*sizeof(float));
    if (delaybuf == NULL) {
	printf("pluck: malloc failure; period = %d\n", params->period);
    }

    randfill(delaybuf, params->period, params->amplitude);
    
    offset = params->start;
    notelen = params->duration;
    
    bptr = delaybuf;
    bend = delaybuf+params->period-1;
    printf("bptr = 0x%x; bend = 0x%x (%d)\n", bptr, bend, bend-bptr);
    lastsample = lastres2 = lastres1 = 0.0;
    for (i = 0; i < notelen; i++) {
	sample = *bptr;
	buf[2*offset+2*i] += sample;	 /* left */
	buf[2*offset+2*i+1] += sample;    /* right */
	res1 = params->rho * (params->s1*sample + params->s*lastsample);
	lastsample = sample;
	res2 = params->apcoeff*res1+lastres1-params->apcoeff*lastres2;
	*bptr = res2;
	lastres1 = res1;
	lastres2 = res2;
	bptr++;
	if (bptr > bend) bptr = delaybuf;
    }
    free(delaybuf);
}

void
calcparams(event_t *event, pluckparams_t *params)
{
    float G, Gnorm, freq, decay, time;
    float delayloop, allpassdelay;
    float s1, s2;
    
    params->rho = 1.0;
    params->s = 0.5;
    
    freq = event->note->freq;
    decay = event->note->decay;
    time = event->note->duration;
    
    G =	    pow(10.0, -decay/(20.0*freq*time));
    Gnorm = cos(PI*freq/RATE);
    
    if (Gnorm > G) {	/* need decay shortening */
	params->rho = G/Gnorm;
    }
    else {		/* need decay lengthening */
	/*
	 * solve quadratic equation
	 */
	float a, b, c;
	float omega, tmp;

	omega = 2.0*PI*freq/RATE;
	tmp = 2.0*cos(omega);
	a = 2.0 - tmp;
	b = tmp - 2.0;
	c = 1.0 - G*G;
	tmp = sqrt(b*b - 4.0*a*c);
	s1 = (-b + tmp)/(2.0*a);
	s2 = (-b - tmp)/(2.0*a);
	
	params->s = MIN(s1, s2);
    }
    
    params->period = RATE/freq;
    delayloop = RATE/freq;
    if ((float)params->period + params->s > delayloop) {
	params->period--;
    }
    allpassdelay = delayloop - (params->period + params->s);
    params->apcoeff = (1.0 - allpassdelay)/(1.0 + allpassdelay);
    
    params->start = RATE*event->start;
    params->duration = RATE*time;
    params->amplitude = event->note->amplitude;
    params->s1 = 1.0 - params->s;
}

void
printparams(char *prefix, pluckparams_t *plist, int len)
{
    int i;
    printf("pluckparams_t %s_plist[] = {\n", prefix);
    for (i = 0; i < len; i++) {
	printf("  {%d, %d, %d, %f, %f, %f, %f, %f},\n", 
	    plist[i].period,
	    plist[i].duration,
	    plist[i].start,
	    plist[i].amplitude,
	    plist[i].rho,
	    plist[i].s,
	    plist[i].s1,
	    plist[i].apcoeff
	);    
    }
    printf("};\n\n");
}
