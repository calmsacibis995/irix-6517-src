#include <stdio.h>
#include <stdlib.h>
#include <audio.h>
#include <math.h>
#include <sys/types.h>
#include <time.h>
#include "dmedia/md.h"
#include "sys/select.h"
#include "sigfpe.h"

#define SAMPLERATE 44100.0
#define PI   M_PI
#define KEYLATENCY 441
#define AUDIOQUEUESIZE 50000
#define MIN(x, y)   ((x) < (y) ? (x) : (y))
#define MAX(x, y)   ((x) > (y) ? (x) : (y))

typedef struct note_s {

    /* Physical characteristics of a note */
    float freq;		/* fundamental frequency of the note */
    float duration;	/* duration of note in seconds */
    float decay;	/* decay in dB */

    /* Paramaterization used by the synthesis algorithm */
    float G, Gnorm;

    /* Processing progress of a note */

    int active;		/* 1 if note is on active list, 0 otherwise */
    float *delaybuf,	/* ring buffer implementing a delay line */
          *bptr,	/* current location in ring buffer */
          *bend;	/* last (wrap) location in ring buffer */
    float s1,s;		/* 2 coef recursive filter coeficients */
    float lastsample;   /* previous sample */
    float ap_coeff;
    float lastres1, lastres2;
    float rho;
    struct note_s *next; /* active note list */
    long long StartSampleNumber;
    int SamplesRemaining;
} note_t;

double freq_tab[256];
ALport AudioPort;
note_t Notes[256];
note_t *ActiveNoteList=0;
long long CurrentSampleNumber;

void randfill(float *buf, int n, float amp);
void InitializeNote(note_t *note,
		float Freq, float Duration, float Decay);
void StartNote(note_t *note, float Amplitude, long long samplenumber);
void ProduceSamplesFromNote(note_t *note, int nsamps, int* buf);
void make_note_to_freq_table(void);
void EventLoop(void);
int BeginPlayingNote(int notenum, int velocity, long long samplenumber);
void AddNoteToActiveList(note_t *note);
void GetMoreSamples(int *buf, int nsamps);

main(int argc, char **argv)
{

    Initialize();
    EventLoop();
}

Initialize()
{
    int buf[48000];		/* sample buffer */
    ALconfig config;
    int i;

    /* Make the floating point system not interrupt on underflows */

    sigfpe_[_UNDERFL].repls = _FLUSH_ZERO;
    handle_sigfpes(_ON,_EN_UNDERFL,0,_ABORT_ON_ERROR,0);


    config=ALnewconfig();
    ALsetqueuesize(config,AUDIOQUEUESIZE);
    ALsetchannels(config,AL_MONO);
    ALsetwidth(config,AL_SAMPLE_24);
    AudioPort = ALopenport("pluck", "w", config);
    CurrentSampleNumber=0;
    
    /* start with a new seed for random() */
    srandom(time(0));
    make_note_to_freq_table();

    for(i=0;i<256;i++) {
        InitializeNote(
            &Notes[i],
    	    freq_tab[i], /* Frequency */
    	    2.0,	 /* Duration */
    	    40.0	 /* Decay */
        );
    }

    for(i=0;i<256;i++) {
        StartNote(&Notes[i], 16000*256.0, 0);
    }
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

/* Scale buf to +/- fsp */

    scale = 1.0/(float)0x7fffffff;
    for (i = 0; i < n; i++) {
	buf[i] = 2.0*(scale*random()-0.5);
    }

/* Estimate the mean */

    sum=0;
    for (i = 0; i < n; i++) {
	sum += buf[i];
    }
    avg = sum/n;

/* Subtract off the mean */

    for (i = 0; i < n; i++) {
	buf[i] -= avg;
    }

/* Find the max */

    max=0;
    for (i = 0; i < n; i++) {
	tmp = FABS(buf[i]);
	if (tmp > max) max = tmp;
    }

/* Scale to +/- amp */

    scale = amp/max;
    for (i = 0; i < n; i++) {
	buf[i] *= scale;	
    }
}


void
InitializeNote(note_t *note,
		float Freq, float Duration, float Decay)
{
    note->freq=Freq;
    note->duration=Duration;
    note->decay=Decay;
    note->next=0;
    note->active=0;
}

void
ProduceSamplesFromNote(note_t *note, int nsamps, int* buf)
{
    int i;
    float sample, res1, res2;

    for (i = 0; i < nsamps; i++) {
	sample = *note->bptr;
	buf[i] = sample;
#undef DRUM
#ifdef DRUM
if((random()>>4) & 1)
res1 = (note->s1*sample + note->s*note->lastsample);
else
res1 = -(note->s1*sample + note->s*note->lastsample);
#else
	res1 = note->rho * (note->s1*sample + note->s*note->lastsample);
#endif
	note->lastsample = sample;
	res2 = note->ap_coeff*res1 +
               note->lastres1 -
               note->ap_coeff*note->lastres2;
	*note->bptr = res2;
	note->lastres1 = res1;
	note->lastres2 = res2;
	note->bptr++;
	if (note->bptr > note->bend) note->bptr = note->delaybuf;
    }
    note->StartSampleNumber += nsamps;
    note->SamplesRemaining -= nsamps;
}

void
StartNote(note_t *note, float Amplitude, long long samplenumber)
{
    float freq;
    int period;		/* Period (in samples) of fundamental */
    float delayloop;
    float allpassdelay;

    
    note->StartSampleNumber = samplenumber + 2*KEYLATENCY;
    note->SamplesRemaining = note->duration*SAMPLERATE;
    note->rho = 1.0;
    note->s = 0.5;
    
    freq = note->freq;
    
    note->G =	    pow(10.0, -note->decay/(20.0*freq*note->duration));
    note->Gnorm = cos(PI*freq/SAMPLERATE);
    
    if (note->Gnorm > note->G) {	/* need decay shortening */
	note->rho = note->G/note->Gnorm;
    }
    else {		/* need decay lengthening */
	/*
	 * solve quadratic equation
	 */
	float a, b, c;
	float omega, tmp;
	float s1, s2;	    /* solutions to the quadratic */
	
	omega = 2.0*PI*freq/SAMPLERATE;
	tmp = 2.0*cos(omega);
	a = 2.0 - tmp;
	b = tmp - 2.0;
	c = 1.0 - note->G*note->G;
	tmp = sqrt(b*b - 4.0*a*c);
	s1 = (-b + tmp)/(2.0*a);
	s2 = (-b - tmp)/(2.0*a);
	
	note->s = MIN(s1, s2);
    }
    period = SAMPLERATE/freq;
    delayloop = SAMPLERATE/freq;
    if ((float)period + note->s > delayloop) {
	period--;
    } else {
}
    allpassdelay = delayloop - (period + note->s);
    note->ap_coeff = (1.0 - allpassdelay)/(1.0 + allpassdelay);
    
    /* create delay line */
    note->delaybuf = (float*)malloc(period*sizeof(float));
    if (note->delaybuf == NULL) {
	printf("pluck: malloc failure; period = %d\n", period);
    }
    randfill(note->delaybuf, period, Amplitude);
    
    note->bptr = note->delaybuf;
    note->bend = note->delaybuf+period-1;
    note->lastsample = note->lastres2 = note->lastres1 = 0.0;
    note->s1 = 1.0 - note->s;
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

/*--------------------------------------------------------------------*/

void
EventLoop(void)
{
    int	nports, x, i;
    int audiofd, midifd;
    MDport inport;
    MDevent mdv;
    fd_set inports, outports;
    int	nfds, highfd;
    int status, channum, notenum, velocity;
    long long samplenumber;
    int buf[KEYLATENCY];

    nports = mdInit();
    printf("%d devices available\n", nports);

    inport = mdOpenInPort(0);
    if (inport == NULL)
	printf("open failed\n");

    mdSetStampMode(inport, MD_NOSTAMP);

    midifd=mdGetFd(inport);
    audiofd=ALgetfd(AudioPort);

    FD_ZERO(&inports);
    FD_ZERO(&outports);
    highfd = 1+MAX(midifd, audiofd);

    while (1) {
        FD_SET(midifd, &inports);
        FD_SET(audiofd, &outports);
	ALsetfillpoint(AudioPort,AUDIOQUEUESIZE-KEYLATENCY);
	nfds = select(highfd, &inports, &outports, 0, 0);
	if (FD_ISSET(midifd, &inports)) {
	    if (mdReceive(inport, &mdv, 1) < 0) {
		    printf("failure receiving message\n");
		    abort();
		    exit(-1);
	    }
	    status=mdv.msg[0] & 0xf0;
	    if(status== MD_NOTEON) {
	        channum=mdv.msg[0]&0x0f;
	        notenum=mdv.msg[1];
	        velocity=mdv.msg[2];
	        samplenumber=CurrentSampleNumber-ALgetfilled(AudioPort);
	        if(velocity==0) {
/*
		    printf("Turning off note %d\n",notenum);
*/
	        } else {
/*
		    printf("Turning on note %d, freq %f, velocity %d\n",
		           notenum, freq_tab[notenum], velocity);
*/
		    BeginPlayingNote(notenum, velocity, samplenumber);
	        }
	    }
	}
        if(FD_ISSET(audiofd, &outports)) {
	    GetMoreSamples(buf,KEYLATENCY);
	    ALwritesamps(AudioPort,buf,KEYLATENCY);
	    CurrentSampleNumber += KEYLATENCY;
        }
    }
}

int
BeginPlayingNote(int notenum, int velocity, long long samplenumber)
{
    note_t *note;
    note=&Notes[notenum];
    StartNote(note, velocity/128.0*64000*256, samplenumber);
    if(!note->active){
	AddNoteToActiveList(note);
    }
}

void
AddNoteToActiveList(note_t *note)
{
    note->next=ActiveNoteList;
    note->active=1;
    ActiveNoteList=note;
}

void
GetMoreSamples(int *buf, int nsamps)
{
    note_t *note, **notep;
    int tempbuf[10000];
    int i;
    int fsp, fsn;

    for(i=0;i<nsamps;i++)buf[i]=0;
    note=ActiveNoteList;
    while(note) {
	if(CurrentSampleNumber == note->StartSampleNumber) {
	    int samps_to_produce;
	    samps_to_produce=MIN(nsamps, note->SamplesRemaining);
            ProduceSamplesFromNote(note, samps_to_produce, tempbuf);
            for(i=0;i<samps_to_produce;i++)buf[i]+=tempbuf[i];

        } else if(CurrentSampleNumber < note->StartSampleNumber) {
	    int samps_to_produce, nzeros;
/*
printf("padding\n");
*/
	    nzeros=note->StartSampleNumber-CurrentSampleNumber;
	    samps_to_produce=MIN(nsamps-nzeros, note->SamplesRemaining);
	    if(samps_to_produce > 0) {
                ProduceSamplesFromNote(note, samps_to_produce, tempbuf);
                for(i=0;i<samps_to_produce;i++)buf[i+nzeros]+=tempbuf[i];
	    }

	} else /* (CurrentSampleNumber > note->StartSampleNumber) */{
	    int samps_to_skip, samps_to_produce;
/*
printf("skipping\n");
*/
	    samps_to_skip = CurrentSampleNumber-note->StartSampleNumber;
	    while(samps_to_skip) {
	        samps_to_produce =
		    MIN(samps_to_skip, sizeof(tempbuf)/sizeof(int));
                ProduceSamplesFromNote(note, samps_to_produce, tempbuf);
		samps_to_skip -= samps_to_produce;
	    }
	    samps_to_produce=MIN(nsamps, note->SamplesRemaining);
            ProduceSamplesFromNote(note, samps_to_produce, tempbuf);
            for(i=0;i<samps_to_produce;i++)buf[i]+=tempbuf[i];
	}

	note=note->next;
    }

    notep= &ActiveNoteList;
    note=*notep;
    while(note) {
	if(note->SamplesRemaining <= 0) {
	    *notep=note->next;
	    note->active=0;
    	    note=*notep;
        } else {
	    notep= &note->next;
	    note=*notep;
        }
    }
    fsp=0x007fff00;
    fsn=0xff800000;
    for(i=0;i<nsamps;i++) {
	if(buf[i] > fsp)
	    buf[i] = fsp;
	else if(buf[i] < fsn)
	    buf[i] = fsn;
    }
}
