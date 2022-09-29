/*
 *	audiosa.c -- standalone diag mips side control.
 *		Download diags and run them wait for output to print.
 *
 * Usage: audio [-en] [-fn] [-gn] [-pn] [-rn] [-sn] [-tn] [-vn] [-w]
 *	-en	set error loop flag to n (stop after n errors,
 *		       0=loop on error forever, -1=ignore errors)
 *	-fn	set sin wave frequency index to n (0-4)
 *	-gn	set input gain to n (0-50)
 *	-h	if present, use high temperature settings
 *	-jn	set transmit sample rate to n (0=48Khz, 2=44KHz, etc.)
 *	-kn	set receiver sample rate to n (0=48Khz, 2=44KHz, etc.)
 *	-pn	set loopflag (program will loop n times -- forever if n=0)
 *	-rn	under unix only, set report level to n (0=none, 4=all, etc.)
 *	-sn	set sin divide flag to n (0=don't divide, 1=divide if need be)
 *	-tn	do only test n (0-15)
 *		0	DSP to AES3 test
 *		1	DSP to DAC/ADC test
 *		2	Actel functionality test
 *		3	digital direct test
 *		4	analog line level test
 *		5	microphone line level test
 *		6	digital sample rate direct test
 *		7	analog sample rate line level test
 *		8	analog input gain line level test
 *		9	microphone input gain line level test
 *		10	microphone output volume level test
 *		11	digital level test
 *		12	digital direct play through
 *		13	analog line level play through
 *		14	microphone line level play through
 *		15	sine wave generator on
 *	-vn	set volume to n (0-255)
 *	-w	under unix only, write out files of data
 *		if -w present, 6 files are written
 *		    digtst.swf, anltst.swf, and mictst.swf
 *		    digtst.srf and anltst.srf
 *		    anltst.ing, and mictst.ing and mictst.vol
 *		    which are each
 *		    512 word buffer dumps (256 left and 256 right)
 *		    for each frequency, gain, or volume tested.
 *			i.e. 5 freqs = 2560 words (20480 bytes)
 *			50 gains = 25600 words (102400 bytes)
 *	-xn	set left channel to a fixed value n and don't check
 *		it during testing (n=0 channel off)
 *	-yn	set right channel to a fixed value n and don't check
 *		it during testing (n=0 channel off)
 *
 *
 *	Numbers: the amplitude used for comparison in this program is
 * the average square amplitude. i.e., each sound value (0-0xffff) is
 * squared and added to the companion value squared (the result is then
 * shifted right by 8 to keep the sum in 32 bits). The average for
 * all samples is then computed. The result is 0-0x400000 (0-4194304).
 *
 *	So for comparison, the following is done:
 * To convert from dB to the above (larry units or lus):
 *	a**2 = 10**(dB/10)*K	where K=4194304
 *	(Note: see program tosqa.c for calculation)
 * So, for example:
 *	0 dB, a**2 = 4194304
 *	1 dB, a**2 = 5280320
 *
 * To compare line out to line in loopback:
 * (using input gain of 11 to avoid clipping)
 *	13.04 dB			from ted's calculation
 *	-16.5 dB			from gain 11 (spec sheet AD7118)
 *	---------
 *	-3.46 dB, a**2 = 1890860	low end for test
 *
 *	15.63 dB			from ted's calculation
 *	-16.5 dB			from gain 11 (spec sheet AD7118)
 *	---------
 *	-0.87 dB, a**2 = 3432890	hi end for test
 *
 *
 * To compare headphone out to mic in loopback:
 * (using input gain of 14 to avoid clipping)
 *	12.28 dB			from ted's calculation
 *	-21.0 dB			from gain 14 (spec sheet AD7118)
 *	---------
 *	-8.72 dB, a**2 = 563196		low end for test
 *
 *	19.39 dB			from ted's calculation
 *	-21.0 dB			from gain 14 (spec sheet AD7118)
 *	---------
 *	-1.61 dB, a**2 = 2895080	hi end for test
 *
 */

#ifdef _STANDALONE
#define NULL	0
#define FILE int
#ifdef IDE
#include <ide_msg.h>
#endif
int audio(int argc, char *argv[]);
#else
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
int main(int argc, char *argv[]);
#endif

#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/sbd.h>
#include "audiotst.h"

#if IDE
#include <libsc.h>
#include "uif.h"
#else
#include <sys/mman.h>
#endif

void printitle(void);
int print_error(void);
void comp_swf(void);
void csend(int lps);
static int block(void);
int get_dsp16(int addr);
void micgain(void);
void compute(void);
int test_digital(void);
int test_first(void);
int test_mnsv(void);
int test_amp(void);
int test_srf(void);
int test_vol(void);
int test_nonaud(void);
int mapthedsp(void);
void download(void);
void write_buf(void);
void write_open(char *name);
void write_close(void);

volatile int *dsp,*miscsr;
int report_level, errcount, errorflag;
int mqal, mqar, msvl, msvr, meanl, meanr;
int mqal_save, mqar_save;
int testnum, swfind, swfreq, gain, volume, sindiv, txsrf, rxsrf;
int loval_left, hival_left, loval_right, hival_right, mean, noise;
int lovolume, hivolume, txsrfval, rxsrfval;
int right_channel, left_channel;
FILE *bufout;
char mbuf[200];

/* tests by number */
#define	DIGTXRX		0	/* test transmitter/receiver digital */
#define	ANLTXRX		1	/* test transmitter/receiver analog */
#define ACTLTST		2	/* Actel functional test */
#define	DIGTST		3	/* digital loopback test w/direct compare*/
#define	ANLTST		4	/* analog loopback test w/level compare */
#define	MICTST		5	/* microphone loopback test w/level compare */
#define	DIGSRTST	6	/* digital sample rate test w/direct compare*/
#define	ANLSRTST	7	/* analog sample rate test w/level compare */
#define	ANLIGTST	8	/* analog input gain test w/level compare */
#define	MICIGTST	9	/* microphone input gain test */
#define	MICVOLTST	10	/* microphone output gain test */
#define	DIGTSTP		11	/* digital loopback test w/level compare*/
#define	DIGPLAY		12	/* digital play forever */
#define	ANLPLAY		13	/* analog play forever */
#define	MICPLAY		14	/* microphone play forever */
#define	SINPLAY		15	/* sin play forever */

#define LASTEST		16	/* one more than last test number */
#define ENDTEST		11	/* last test+1 to run in normal loop */

char *fname[LASTEST] = {NULL,NULL,NULL,"digtst.swf","anltst.swf",
			 "mictst.swf","digtst.srf","anltst.srf",
			 "anltst.ing","mictst.ing","mictst.vol",
			 NULL,NULL,NULL,NULL,NULL};

char *tname[LASTEST] = { "DSP-AES3   ",		/* Test0 */
			 "DSP-DAC/ADC",		/* Test1 */
			 "Actel test ",		/* Test2 */
			 "Digital chk",		/* Test3 */
			 "Analog line",		/* Test4 */
			 "Hd/Mic line",		/* Test5 */
			 "Digital sr ",		/* Test6 */
			 "Analog sr  ",		/* Test7 */
			 "Analog gain",		/* Test8 */
			 "Hd/Mic gain",		/* Test9 */
			 "Hd/Mic vol ",		/* Test10 */
			 "Digital lev",		/* Test11 */
			 "Digital i/o",		/* Test12 */
			 "Analog i/o ",		/* Test13 */
			 "Hd/Mic i/o ",		/* Test14 */
			 "Sin wave on" };	/* Test15 */

/* dsp reporting error messages */
/* error message numbers */
#define	DTXERR		0	/* digital transmit error */
#define	DRXERR		1	/* digital receiver error */
#define	ATXERR		2	/* analog transmit error */
#define	ARXERR		3	/* analog receiver error */
#define SFIERR		4	/* soft interrupt error */
#define SFIBERR		5	/* soft bit interrupt error */
#define RBIERR		6	/* receive block interrupt error */
#define RBIBERR		7	/* receive block bit interrupt error */
#define TBIERR		8	/* transmit block interrupt error */
#define TBIBERR		9	/* transmit block bit interrupt error */
#define RNAIERR		10	/* receive block interrupt error */
#define RNAIBERR	11	/* receive block bit interrupt error */
#define TNAIERR		12	/* transmit block interrupt error */
#define TNAIBERR	13	/* transmit block bit interrupt error */
#define IRQERR		14	/* irq error */
#define	LASTMSG		15

/* error messages */
char *errmsg[LASTMSG] = {"Digital transmitter error",
			  "Digital receiver   error",
			  "Analog transmitter error",
			  "Analog receiver   error",
			  "No soft interrupt error",
			  "Soft interrupt bit error",
			  "Receive block interrupt error",
			  "Receive block interrupt bit error",
			  "Transmit block interrupt error",
			  "Transmit block interrupt bit error",
			  "Receive nonaudio data interrupt error",
			  "Receive nonaudio data interrupt bit error",
			  "Transmit nonaudio data interrupt error",
			  "Transmit nonaudio data interrupt bit error",
			  "Unexpected IRQB error"};

#define SAMPSIZE	256	/* number of samples to test */
#define	BUFSTART	0x5000	/* buffer in dsp, actual = y:$d000 */
#define BUFSIZE		0x2000	/* size of buffer in dsp */
#define SINTAB		0x4200	/* sin table in dsp ram acutal = y:$c200 */
#define	SAMPST		0x1c00	/* sample position in buffer */
#define DSP_REV_LEVEL	0x10	/* revision level (y:$8010) */
#define DSP_TXCLK_VAL	0x11	/* end tx clk buf pointer (y:$8011) */
#define DSP_RXCLK_VAL	0x12	/* end rx clk buf pointer (y:$8012) */
#define DSP_CP10MS	0x13	/* clock value location (y:$8013) */
#define DSP_ERRNUM	0x14	/* error number location (y:$8014) */
#define DSP_HS		0x20	/* handshake location (y:$8020) */
#define DSP_TXCLK	0x4400	/* transmit sample rate buffer (y:$c400) */
#define DSP_RXCLK	0x4600	/* receiver sample rate buffer (y:$c600) */

#define SMALLGAIN	24	/* gain > this use full product in compute */
#define ENDGAIN		45	/* low gain */
#define NOMLGAIN	11	/* nominal line gain */
#define NOMMGAIN	14	/* nominal mic gain */
#define HIMEAN		250	/* high mean for checking */
#define ENDVOL		256	/* high volume */
#define NOMVOL		128	/* nominal volume */
#define HINOISE		10	/* high noise value */
#define HITEMPNOISE	25	/* high temperature noise value */
#define HUGENOISE	1000	/* huge noise value */
#define	NOMSRFREQ	0	/* nominal sample rate frequency */
#define	AESSRFREQ	6	/* aes sample rate frequency */
#define ENDSRFREQ	7	/* final sample rate loop frequency */
#define BIGSRFREQ	64	/* final sample rate select frequency */
#define ENDSWI		5	/* final sine wave frequency index */
#define NOMSWI		3	/* nominal sine wave frequency index */

/* amplitude square averages for each input gain for digital, line and mic */
#define LODIG	4194000
#define HIDIG	4195000

/* sine wave frequency index tables 
 * sine table = 256 words, so at 48KHz that would be 48000/256 = 187.5Hz
 */
int swctab[ENDSWI] = {1, 2, 4, 8, 16};	/* 187.5,375,750,1500,3000 */

/* sample rate frequency test table
 * bit 0	0=no divide, 1=multiply by 2/3
 * bit 1-2	00=48KHz, 01=44.1KHz, 10=video, 11=AES Rx
 * bit 3-5	scaling (000=none, 001=1/2, 010=1/3 ...)
 *
 * for tests:
 *	0 = 48KHZ, 1 = 48*2/3=32KHz, 2 = 44.1KHz, 3 = 44.1*2/3 = 29.4KHz
 *	8 = 48*1/2 = 24KHz, 0x11 = 48*2/3*1/3=10.6KHz, 0x22=44.1*1/5=8.8KHz
 */
int srftst[ENDSRFREQ] = {0, 1, 2, 3, 8, 0x11, 0x22};

/* non-audio data test table */
int condata[16] = {0x004300, 0x004300, 0x004323, 0x004345,
		   0x004301, 0x004323, 0x004345, 0x004367,
	           0x004300, 0x004300, 0x004300, 0x004300,
		   0x004300, 0x123456, 0x0faced, 0x000004};

int brodata[16] = {0x004301, 0x004300, 0x004323, 0x004345,
		   0x00436f, 0x004302, 0x004300, 0x004300,
	           0x004320, 0x004360, 0x0043a4, 0x0043e8,
		   0x00430c, 0x123456, 0x0faced, 0x000004};

/* the following were computed by running ~/swat/misc/compsqa.c 
 * they are the computed gain values at various attentuation numbers
 * based on the constant 4194307 which is 32768**2/256.
 */
int loline[ENDGAIN] = {84461716, 59794316, 42331134, 29968147, 
		21215824, 15019653, 10633100, 7527658, 
		5329174, 3772766, 2670914, 1890862, 
		1338627, 947676, 670903, 474963, 
		336248, 238045, 168523, 119305, 
		84461, 59794, 42331, 29968, 
		21215, 3845031, 2722073, 1927080, 
		1364268, 965828, 683753, 484060, 
		342688, 242605, 171751, 121590, 
		86079, 60939, 43141, 30542, 
		21622, 15307, 10836, 7671, 
		5431};

int hiline[ENDGAIN] = {153341569, 108557517, 76852837, 54407642, 
		38517660, 27268415, 19304559, 13666581, 
		9675198, 6849516, 4849086, 3432890, 
		2430300, 1720520, 1218035, 862303, 
		610463, 432175, 305956, 216600, 
		153341, 108557, 76852, 54407, 
		38517, 6980714, 4941967, 3498644, 
		2476850, 1753476, 1241366, 878819, 
		622156, 440453, 311817, 220749, 
		156278, 110636, 78324, 55449, 
		39255, 27790, 19674, 13928, 
		9860};

int lomic[ENDGAIN] = {70902231, 50194935, 35535293, 25157061, 
		17809835, 12608397, 8926062, 6319168, 
		4473628, 3167086, 2242125, 1587303, 
		1123724, 795536, 563196, 398712, 
		282266, 199829, 141468, 100152, 
		70902, 50194, 35535, 25157, 
		17809, 3227749, 2285071, 1617707, 
		1145248, 810774, 573984, 406349, 
		287673, 203657, 144178, 102070, 
		72260, 51156, 36215, 25638, 
		18150, 12849, 9097, 6440, 
		4559};

int himic[ENDGAIN] = {364468420, 258023881, 182666919, 129318275, 
		91550328, 64812668, 45883855, 32483282, 
		22996402, 16280206, 11525503, 8159431, 
		5776435, 4089402, 2895075, 2049556, 
		1450974, 1027211, 727210, 514825, 
		364468, 258023, 182666, 129318, 
		91550, 16592043, 11746267, 8315720, 
		5887079, 4167732, 2950528, 2088814, 
		1478767, 1046887, 741139, 524686, 
		371449, 262966, 186165, 131795, 
		93303, 66054, 46762, 33105, 
		23436};

#ifndef IDE
#define NO_REPORT       0
#define SUM             1
#define ERR             2
#define VRB             3
#define DBG             4

void msg_printf(int msglevel, char *str)
/* for non ide, simulate msg printing */
{
    if (msglevel <= report_level) {
	if (msglevel == ERR)
	    printf("\t\t\t\t\t\t- ERROR -\n");
	printf(str);
    }
}

void busy(int on)
/* for non ide, simulate busy signal */
{
#ifdef _STANDALONE
    static int spinchar = 1;

    if (on) {
	spinchar ^= 1;
	if (spinchar) {
	    msg_printf(NO_REPORT,"\r*");  /* print "busy" character */
	} else {
	    msg_printf(NO_REPORT,"\r+");  /* print "busy" character */
	}
    } else {
	    msg_printf(NO_REPORT,"\r");  /* clear "busy" character */
    }
    __scandevs();
#endif
}

#endif

#ifdef IDE
int audio(int argc, char *argv[])
#else
int main(int argc, char *argv[])
#endif
/*
 * Main program -- manage parameters and tests
 */
{
    int writeflag, loopflag, base_gain, imqal, cov;
    int base_swi, base_mic_gain, base_line_gain, base_vol;
    int base_txsrf, base_rxsrf, sdivflg;
    int first_test, last_test, first_swi, last_swi, first_gain, last_gain;
    int first_vol, last_vol, nomvol_flag;
    int first_txsrf, last_txsrf, ilast_txsrf, txsrf_change, itxsrf;
    char sd1, sd2, rev_level;

    report_level = DBG;
#ifdef IDE
    if(is_v50()) { /* V50 vme-based IP20 has no audio */
	goto end_prog;
    }
#endif
    if (mapthedsp() < 0)
	goto end_prog;
    download();
    bufout = NULL;
    writeflag = 0;
    first_test = 0;
    last_test = ENDTEST;
    first_swi = 0;
    last_swi = ENDSWI;
    base_swi = NOMSWI;
    first_gain = 0;
    last_gain = ENDGAIN;
    base_line_gain = NOMLGAIN;
    base_mic_gain = NOMMGAIN;
    first_vol = 0;
    last_vol = ENDVOL;
    base_vol = NOMVOL;
    sdivflg = 1;
    first_txsrf = 0;
    last_txsrf = ENDSRFREQ;
    base_txsrf = NOMSRFREQ;
    txsrf_change = 0;
    base_rxsrf = NOMSRFREQ;
    left_channel = -1;
    noise = HINOISE;
    right_channel = -1;
    loopflag = 1;
    errcount = 0;
    errorflag = 1;
    while (argc > 1 && argv[1][0] == '-' && argv[1][1] != '\0') {
	switch (argv[1][1]) {
	    case 'e':		/* set error loop flag */
		errorflag = atoi(&argv[1][2]);
		break;
	    case 'f':		/* set sin wave frequency to next argument */
		first_swi = atoi(&argv[1][2]);
		last_swi = first_swi + 1;
		base_swi = first_swi;
		if (first_swi > ENDSWI-1)
		    errcount++;
		break;
	    case 'h':		/* set noise to hi temp value */
		noise = HITEMPNOISE;
		break;
	    case 'g':		/* set input gain to next argument */
		first_gain = atoi(&argv[1][2]);
		last_gain = first_gain + 1;
		base_mic_gain = first_gain;
		base_line_gain = first_gain;
		if (first_gain > ENDGAIN-1)
		    errcount++;
		break;
	    case 'j':		/* set tx sr freq to next argument */
		first_txsrf = atoi(&argv[1][2]);
		last_txsrf = first_txsrf + 1;
		base_txsrf = first_txsrf;
		if (first_txsrf > BIGSRFREQ-1)
		    errcount++;
		txsrf_change = 1;
		break;
	    case 'k':		/* set rx sr freq to next argument */
		base_rxsrf = atoi(&argv[1][2]);
		if (base_rxsrf > BIGSRFREQ-1)
		    errcount++;
		break;
	    case 'p':		/* set loop flag */
		loopflag = atoi(&argv[1][2]);
		break;
	    case 'r':		/* set report level */
		report_level = atoi(&argv[1][2]);
		break;
	    case 's':		/* set sin divide flag */
		sdivflg = atoi(&argv[1][2]);
		break;
	    case 't':		/* set test to test arg */
		first_test = atoi(&argv[1][2]);
		last_test = first_test+1;
		if (first_test >= LASTEST)
		    errcount++;
		break;
	    case 'v':		/* set output volume to next argument */
		first_vol = atoi(&argv[1][2]);
		last_vol = first_vol + 1;
		base_vol = first_vol;
		if (first_vol > ENDVOL-1)
		    errcount++;
		break;
	    case 'w':	/* run tests, and write results to files */
		writeflag = 1;
		break;
	    case 'x':		/* set left channel to next argument */
		left_channel = atoi(&argv[1][2]);
		break;
	    case 'y':		/* set right channel to next argument */
		right_channel = atoi(&argv[1][2]);
		break;
	    default:
		errcount++;
		break;
	}
	argc--;
	argv++;
    }
    if (argc > 1 || errcount > 0) {
	sprintf(mbuf, "Usage: audio [-en] [-f0-%d] [-g0-%d] [-j0-%d] [-k0-%d] [-pn] [-r0-4] [-t0-%d] [-v0-%d] [-w] [-xn] [-yn]\n",
    ENDSWI-1,ENDGAIN-1,BIGSRFREQ-1,BIGSRFREQ-1,LASTEST-1,ENDVOL-1);
	msg_printf(ERR,mbuf); 
	goto end_prog;
    }
    testnum = 0;
    /* see if dsp is alive */
    if (block() < 0) {
	msg_printf(ERR,"Pretest--DSP broken, can't continue\n");
	errcount = 1;
	goto end_prog;		/* no reason to continue */
    }
    /* see if actel correct revision */
    rev_level = dsp[DSP_REV_LEVEL] & 0xff;
    if (rev_level < 2) {
	sprintf(mbuf, "WRONG ACTEL revision, should be >1, got %d\n",rev_level);
	msg_printf(ERR,mbuf);
	errcount = 1;
	goto end_prog;		/* no reason to continue */
    }
/* Run tests with each frequency and gain and volume */
 do {
  errcount = 0;
  do {
    for (testnum = first_test; testnum < last_test; testnum++) {
	busy(1);
/* establish default values */
	gain = base_line_gain;
	volume = base_vol;
	sindiv = (gain < NOMLGAIN || volume > NOMVOL) && (sdivflg);
	txsrf = base_txsrf;
	rxsrf = base_rxsrf;
	swfind = base_swi;
	comp_swf();
	mean = HIMEAN;
	loval_left = loline[gain];
	hival_left = hiline[gain];
	loval_right = loline[gain];
	hival_right = hiline[gain];
/*
 * General tests and play loops
 * send command and simply wait for response or leave in case of play
 * tests which never return
 */
	if (testnum == DIGTXRX || (testnum == ACTLTST && rev_level > 2) ||
		    testnum == ANLTXRX || testnum > ENDTEST) {
	    if (testnum == MICPLAY) {
		gain = base_mic_gain;
		sindiv = (gain < NOMMGAIN || volume > NOMVOL) && (sdivflg);
	    }
	    if (testnum == DIGPLAY)
		txsrf = AESSRFREQ;
	    sd1 = (sindiv) ? '/' : ' ';
	    sd2 = (sindiv) ? '8' : ' ';
	    if (testnum == SINPLAY)
	      sprintf(mbuf,"#%d-%s:sin=%d%c%c tx-rate=%d vol=%d\n",
		   testnum,tname[testnum],swfreq,sd1,sd2,txsrfval,volume);
	    else if (testnum == ANLPLAY || testnum == MICPLAY)
	      sprintf(mbuf,"#%d-%s: tx-rate=%d rx-rate=%d gain=%d vol=%d\n",
	       testnum,tname[testnum],txsrfval,rxsrfval,gain,volume);
	    else	/* DIGPLAY, DIGTXRX, ANLTXRX */
	      sprintf(mbuf,"#%d-%s: tx-rate=%d\n",
		    testnum,tname[testnum],txsrfval);
	    msg_printf(VRB,mbuf); 
	    csend(1);
	    if (testnum > ENDTEST) /* these tests never return, so leave */
		goto end_prog;
	    if (block() < 0)	/* check return for other tests */
		goto end_error;		/* no reason to continue */
	    if (testnum == ACTLTST) /* test non-audio data */
		if (test_nonaud() < 0)
		    goto end_error;
/*
 * Tests that use sin wave frequency loop. Send various sin waves
 * through and test the returned results against known values.
 */
	} else if (testnum == DIGTST || testnum == ANLTST ||
		   testnum == MICTST || testnum == DIGTSTP) {
	    if ((fname[testnum] != NULL) && writeflag)
		write_open(fname[testnum]);
	    /* change established values */
	    if (testnum == MICTST) {
		gain = base_mic_gain;
		sindiv = (gain < NOMMGAIN || volume > NOMVOL) && (sdivflg);
		micgain();
	    } else if (testnum == DIGTSTP) {
		loval_left = LODIG;
		hival_left = HIDIG;
		loval_right = LODIG;
		hival_right = HIDIG;
	    }
	    swfind = first_swi;
	    /* send first to get it started in right mode */
	    csend(4);
	    for (swfind = first_swi; swfind < last_swi; swfind++) {
		comp_swf();
		printitle();
		csend(1);
		if (block() < 0)
		    goto end_error;
		if (bufout != NULL)
		    write_buf();
		if (testnum == DIGTST) {
		    if (test_digital() < 0)
			goto end_error;
		} else {
		    compute();
		    if (test_mnsv() < 0)
			goto end_error;
		    if (test_first() < 0)
			goto end_error;
		}
	    }
	    if (bufout != NULL)
		write_close();
/*
 * Tests that use sample rate frequency loop. Send a sin wave with
 * various sample rates through and test the returned results
 * against known values.
 */
	} else if (testnum == ANLSRTST || testnum == DIGSRTST) {
	    if ((fname[testnum] != NULL) && writeflag) 
		write_open(fname[testnum]);
	    csend(4);	/* send first through to start it off */
	    if (testnum == DIGSRTST)
		ilast_txsrf = (last_txsrf+1)/2; /* digital uses half range */
	    else
		ilast_txsrf = last_txsrf;
	    for (itxsrf = first_txsrf; itxsrf < ilast_txsrf; itxsrf++) {
		if (txsrf_change) {	/* if arg, use it */
		    txsrf = itxsrf;
		} else	{		/* else, use table look up */
		    txsrf = srftst[itxsrf];
		}
		rxsrf = txsrf;		/* loop assumes same freq */
		comp_swf();
		printitle();
		csend(1);
		if (block() < 0)
		    goto end_error;
		if (bufout != NULL)
		    write_buf();
		if (test_srf() < 0)
		    goto end_error;
		if (testnum == DIGSRTST) {
		    if (test_digital() < 0)
			goto end_error;
		} else {			/* ANLSRTST */
		    compute();
		    if (test_mnsv() < 0)
			goto end_error;
		    if (test_first() < 0)
			goto end_error;
		}
	    }
	    if (bufout != NULL)
		write_close();
/*
 * Tests that use input gain loop. Send a sin wave with various input gains
 * through and test the returned results against known values.
 */
	} else if (testnum == ANLIGTST || testnum == MICIGTST) {
	    if ((fname[testnum] != NULL) && writeflag) 
		write_open(fname[testnum]);
	    gain = first_gain;
	    base_gain = (testnum == MICIGTST) ? NOMMGAIN : NOMLGAIN;
	    sindiv = (gain < base_gain || volume > NOMVOL) && (sdivflg);
	    csend(4);
	    for (gain = first_gain; gain < last_gain; gain++) {
		if ((gain & 0x7) == 0)
		    busy(1);
		sindiv = (gain < base_gain || volume > NOMVOL) && (sdivflg);
		printitle();
		csend(1);
		if (block() < 0)
		    goto end_error;
		if (bufout != NULL)
		    write_buf();
		compute();
		if (test_mnsv() < 0)
		    goto end_error;
		if (gain == first_gain || gain == SMALLGAIN+1 ||
			 gain == base_gain) {
		    loval_left = loline[gain];
		    hival_left = hiline[gain];
		    loval_right = loline[gain];
		    hival_right = hiline[gain];
		    if (testnum == MICIGTST) {
			micgain();
		    }
		    if (test_first() < 0)
			goto end_error;
		} else {
		    /* ratios should be about 1.5db=(1.18^2)=139 */
		    loval_left = (gain > 33) ? 80 : 118;
		    hival_left = (gain > 33) ? 250 : 170;
		    loval_right = (gain > 33) ? 80 : 118;
		    hival_right = (gain > 33) ? 250 : 170;
		    if (test_amp() < 0)
			goto end_error;
		}
	    }
	    if (bufout != NULL)
		write_close();
/*
 * Tests that use output gain loop. Send a sin wave with various output gains
 * through and test the returned results against known values.
 */
	} else if (testnum == MICVOLTST) {
	    if ((fname[testnum] != NULL) && writeflag) 
		write_open(fname[testnum]);
	    gain = base_mic_gain;
	    volume = last_vol - 1;
	    sindiv = (gain < NOMMGAIN || volume > NOMVOL) && (sdivflg);
	    csend(4);
	    nomvol_flag = 1;
	    for (volume = last_vol-1; volume >= first_vol; volume -= 5) {
		if ((volume & 0x7) == 0)
		    busy(1);
		sindiv = (gain < NOMMGAIN || volume > NOMVOL) && (sdivflg);
		printitle();
		csend(1);
		if (block() < 0)
		    goto end_error;
		if (bufout != NULL)
		    write_buf();
		compute();
		if (test_mnsv() < 0)
		    goto end_error;
		if (volume == last_vol-1) {
		    micgain();
		    if (test_first() < 0)
			goto end_error;
		    imqal = (sindiv) ? mqal*64 : mqal;
		    cov = (imqal*100)/3200000;
		    lovolume = ((50*cov)/100) - 10;
		    hivolume = ((50*cov)/100) + 10;
		} else if (nomvol_flag && volume <= NOMVOL) {
		    micgain();
		    if (test_first() < 0)
			goto end_error;
		    nomvol_flag = 0;
		} else if (volume == 0) {
		    micgain();
		    if (test_first() < 0)
			goto end_error;
		} else {
		    loval_right = 90;
		    hival_right = 110;
		    if (test_vol() < 0)
			goto end_error;
		}
	    }
	    if (bufout != NULL)
		write_close();
	}
    }
    loopflag--;
    if (loopflag == 0)		/* end if at zero and wasn't before */
	loopflag--;
    else if (loopflag < 0)	/* keep zero at zero forever */
	loopflag = 0;
  } while(loopflag >= 0);
  goto end_prog;
end_error:
    errorflag--;
    if (errorflag == 0)		/* end if at zero and wasn't before */
	errorflag--;
    else if (errorflag < 0)		/* keep zero at zero forever */
	errorflag = 0;
	/* scandevs();	will halt if key hit */
 } while (errorflag >= 0);
end_prog:
    busy(0);
    sprintf(mbuf, "Audio diagnostics complete -- %d errors \n",errcount);
    msg_printf(SUM,mbuf);

#ifdef _STANDALONE
#ifndef IDE
    printf("Type a return to exit\n");
    getchar();
#endif
#endif
    return errcount;
}

void printitle(void)
/* print a useful test title with all parameters mentioned */
{
    char sd1,sd2;

    sd1 = (sindiv) ? '/' : ' ';
    sd2 = (sindiv) ? '8' : ' ';
  sprintf(mbuf,"#%d-%s:sin=%d%c%c tx-rate=%d rx-rate=%d gain=%d vol=%d\n",
       testnum,tname[testnum],swfreq,sd1,sd2,txsrfval,rxsrfval,gain,volume);
    msg_printf(VRB,mbuf); 
}

int print_error(void)
{
    msg_printf(ERR,mbuf);
    errcount++;
    if (errorflag >= 0 && errcount >= errorflag)
	return -1;
    return 0;
}

void csend(int lps)
/* send a command to get dsp running with test testnum
 * lps = loop count for dsp
 */
/* DSP memory locations 
 * starts at 0x20	(really y:$8020)
DSP_HS:
hshake:	ds	1			; hand shake location
frqinc:	ds	1			; frequency increment
ingain:	ds	1			; input gain
volume:	ds	1			; output gain
sindiv:	ds	1			; sin divisor
txsrf:	ds	1			; transmit sample rate frequency
rxsrf:	ds	1			; receiver sample rate frequency
loops:	ds	1			; number of internal loops
leftc:	ds	1			; left channel value
rightc:	ds	1			; right channel value
 */
{
    dsp[DSP_HS+9] = right_channel;	/* tell dsp right channel */
    dsp[DSP_HS+8] = left_channel;	/* tell dsp left channel */
    dsp[DSP_HS+7] = lps;		/* tell dsp loop count */
    dsp[DSP_HS+6] = rxsrf;		/* tell dsp sample rate freq */
    dsp[DSP_HS+5] = txsrf;		/* tell dsp sample rate freq */
    dsp[DSP_HS+4] = sindiv;		/* tell dsp sin divisor */
    dsp[DSP_HS+3] = volume;		/* tell dsp volume */
    dsp[DSP_HS+2] = gain;		/* tell dsp input gain */
    dsp[DSP_HS+1] = swctab[swfind];	/* tell dsp frequency increment */
    dsp[DSP_HS] = testnum;		/* tell dsp were ready */
}

static int block(void)
/* wait for dsp to finish running a test. memory location will become
 * 0xffffff on dsp completion. If dsp fails to change location or if
 * dsp reports error on completion, declare error return.
 */ 
{
    int m, count;
    volatile int i;

    count = 0;
    while(1) {		/* wait for dsp to be done */
	for (i = 0; i < 100000; i++)	/* don't read every time */
	    ;
	m = dsp[DSP_HS];
	m = m & 0xffffff;
	if(m == 0xffffff)
	    break;
	count++;
	if (count > 3000) {
	    sprintf(mbuf,"#%d--DSP not responding\n",testnum);
	    if (print_error() < 0)
		return -1;
	    break;
	}
    }
    m = dsp[DSP_ERRNUM];		/* get error code */
    m = m & 0xffffff;
    if (m != 0xffffff) {	/* report error */
	sprintf(mbuf,"#%d--%s\n",testnum,errmsg[m]);
	if (print_error() < 0)
	    return -1;
    }
    return 0;
}

void comp_swf()
/* compute srfval's and swfreq based on txsrf and actel knowledge */
{
    int bdiv;

    txsrfval = (txsrf & 2) ? 44100 : 48000;
    if (txsrf & 1)		/* scale bit 2 */
	txsrfval = (txsrfval * 2) / 3;
    bdiv = (txsrf >> 3) & 7;	/* get scale bits 4-6 */
    txsrfval /= (bdiv + 1);
    swfreq = txsrfval/(256/(1<<swfind));

    rxsrfval = (rxsrf & 2) ? 44100 : 48000;
    if (rxsrf & 1)		/* scale bit 2 */
	rxsrfval = (rxsrfval * 2) / 3;
    bdiv = (rxsrf >> 3) & 7;	/* get scale bits 4-6 */
    rxsrfval /= (bdiv + 1);
}

int get_dsp16(int addr)
/* get a memory location from the dsp and throw away low eight bits
 * and sign extend the resultant 16 bits of audio data.
 */
{
    int n;

    n = (dsp[addr] >> 8) & 0xffff;
    if ((n & 0x8000) == 0x8000)
	n |= 0xffff0000;
    return n;
}

void compute(void)
/* compute values to be used in all other tests. Values include
 * mean of amplitudes, squared amplitudes, and noise.
 * for gains > SMALLGAIN, use full 32 bits of squares, else
 * throw away low 8 bits to keep sum in 32 bits.
 */
{
    int i;
    int temp1, temp2, sampsz;

    meanl = 0;
    meanr = 0;
    /* calculate mean of samples for mean removal (dc offset) */
    for (i = (BUFSTART+SAMPST); i < (BUFSTART+SAMPST+2*SAMPSIZE); i += 2) {
	meanl += get_dsp16(i);		/* left first */
	meanr += get_dsp16(i+1);		/* right second */
    }
    meanl /= SAMPSIZE;
    meanr /= SAMPSIZE;
    mqal = 0;
    mqar = 0;
    msvl = 0;
    msvr = 0;
/* calculate mean square sum for samples sin and cos
 * mqa = mean quad amplitude, mean of sum of squares of sin + cos
 *	if sin is at buf[x], then cos is buf[x+1/4 wave] (90 degrees)
 *	so if wave is SAMPSIZE/swctab[swfind] words, then cos is at 
 *	buf[(SAMPSIZE/swctab[swfind])/4]
 *	but if right and left both there, then double numbers
 *	so if sin is buf[x], then cos is buf[x+(SAMPSIZE/swctab[swfind])/2]
 * msv = mean signal variance, mean of sum of squares of sin + -sin
 *	if wave is SAMPSIZE/swctab[swfind] words,
 *	and right and left both in buffer,
 *	then sin is at buf[x], then -sin is at buf[x+(SAMPSIZE/swctab[swfind])]
 */
    sampsz = SAMPSIZE/swctab[swfind];
    for (i = (BUFSTART+SAMPST); i < (BUFSTART+SAMPST+2*SAMPSIZE); i += 2) {
	temp1 = get_dsp16(i) - meanl;
	temp2 = get_dsp16(i + (sampsz/2)) - meanl;
	if (gain > SMALLGAIN)
	    mqal += (((temp1*temp1)) + ((temp2*temp2)));
	else
	    mqal += (((temp1*temp1) >> 8) + ((temp2*temp2) >> 8));
	temp2 = get_dsp16(i + sampsz) - meanl;
	msvl += (((temp1 + temp2)*(temp1 + temp2)));
	temp1 = get_dsp16(i + 1) - meanr;
	temp2 = get_dsp16(i + 1 + (sampsz/2)) - meanr;
	if (gain > SMALLGAIN)
	    mqar += (((temp1*temp1)) + ((temp2*temp2)));
	else
	    mqar += (((temp1*temp1) >> 8) + ((temp2*temp2) >> 8));
	temp2 = get_dsp16(i + 1 + sampsz) - meanr;
	msvr += (((temp1 + temp2)*(temp1 + temp2)));
    }	
    mqal /= SAMPSIZE;
    mqar /= SAMPSIZE;
    msvl /= SAMPSIZE;
    msvr /= SAMPSIZE;
    sprintf(mbuf,"left  mean,noise,amp = %9d %9d %9d\n",meanl,msvl,mqal);
    msg_printf(DBG,mbuf);
    sprintf(mbuf,"right mean,noise,amp = %9d %9d %9d\n",meanr,msvr,mqar);
    msg_printf(DBG,mbuf);
}

int test_digital(void)
/* test audio data for exact digital match. A buffer of audio data
 * is checked for the unique full sine wave value 0x7fffff. Upon
 * detecting value, the other values are matched against the actual
 * dsp sin table which was put in readable dsp memory by the dsp code.
 * If no 0x7fffff is found, or any wrong value is detected, an error
 * is returned.
 */
{
    int i, j, gotone;

/* look for first 0x7ffff after BUFSTART + SAMPSIZE */
    gotone = -1;
    for (i = (BUFSTART+SAMPST); i < (BUFSTART+BUFSIZE); i += 2) {
	if ((dsp[i] & 0xfffff0) == 0x7ffff0) {
	    gotone = i;
	    break;
	}
    }
    if (gotone < 0) {
	sprintf(mbuf,"#%d--Can't find value 0x7ffff0 in buffer\n",testnum);
	if (print_error() < 0)
	    return -1;
	for (i = (BUFSTART+SAMPST); i < (BUFSTART+SAMPST+128); i += 8) {
	    sprintf(mbuf,"%x: %x %x %x %x %x %x %x %x\n",i,dsp[i],dsp[i+1],
		 dsp[i+2],dsp[i+3],dsp[i+4],dsp[i+5],dsp[i+6],dsp[i+7]);
	    msg_printf(DBG,mbuf);
	}
    } else {
    /* compare values for exact matches */
	j = SINTAB + 64;	/* SINTAB + 64 = position of 0x7ffff */
	for (i = 0; i < SAMPSIZE*2; i += 2) {
	    if (dsp[gotone+i] != (dsp[j] & 0xfffff0)) {
		sprintf(mbuf,"#%d--left expected %x, got %x, addr %x\n",
			 testnum,dsp[j],dsp[gotone+i],gotone+i);
		if (print_error() < 0)
		    return -1;
	    }
	    if (dsp[gotone+i+1] != (dsp[j] & 0xfffff0)) {
		sprintf(mbuf,"#%d--right expected %x, got %x, addr %x\n",
			 testnum,dsp[j],dsp[gotone+i+1],gotone+i+1);
		if (print_error() < 0)
		    return -1;
	    }
	    j += swctab[swfind];
	    if (j > (SINTAB + 256))
		j -= 256;
	}
    }
    return 0;
}

int test_mnsv(void)
/* test computed means and noise values to ensure they fall within
 * a given range. Return error if either falls outside range.
 */
{
    int stnl, stnr;

    if (left_channel < 0 && (meanl < -mean || meanl > mean)) {
      sprintf(mbuf,"#%d--sin=%d gain=%d vol=%d left mean -%d-%d, got %d\n",
	    testnum,swfreq,gain,volume,mean,mean,meanl);
	if (print_error() < 0)
	    return -1;
    }
    if (right_channel < 0 && (meanr < -mean || meanr > mean)) {
     sprintf(mbuf,"#%d--sin=%d gain=%d vol=%d right mean -%d-%d, got %d\n",
	    testnum,swfreq,gain,volume,mean,mean,meanr);
	if (print_error() < 0)
	    return -1;
    }

    stnl = (mqal > 0) ? (msvl*10000)/mqal : 0;
    stnr = (mqar > 0) ? (msvr*10000)/mqar : 0;
    sprintf(mbuf,"noise/amp left,right = %9d %9d\n",stnl,stnr);
    msg_printf(DBG,mbuf);

    if (left_channel < 0 && (msvl < 0 || msvl > HUGENOISE)) {
      sprintf(mbuf,"#%d--sin=%d gain=%d vol=%d left noise 0-%d, got %d\n",
		testnum,swfreq,gain,volume,HUGENOISE,msvl);
	if (print_error() < 0)
	    return -1;
    }
    if (right_channel < 0 && (msvr < 0 || msvr > HUGENOISE)) {
      sprintf(mbuf,"#%d--sin=%d gain=%d vol=%d right noise 0-%d, got %d\n",
		testnum,swfreq,gain,volume,HUGENOISE,msvr);
	if (print_error() < 0)
	    return -1;
    }
    if (left_channel < 0 && (stnl > noise && msvl > noise)) {
      sprintf(mbuf,"#%d--sin=%d gain=%d vol=%d left noise/amp <%d, got %d\n",
		testnum,swfreq,gain,volume,noise,stnl);
	if (print_error() < 0)
	    return -1;
    }
    if (right_channel < 0 && (stnr > noise && msvr > noise)) {
     sprintf(mbuf,"#%d--sin=%d gain=%d vol=%d right noise/amp <%d, got %d\n",
		testnum,swfreq,gain,volume,noise,stnr);
	if (print_error() < 0)
	    return -1;
    }
    return 0;
}

int test_amp(void)
/* test mean of squared amplitudes to ensure they fall within a given
 * range. If they don't, return an error.
 */
{
    int atal, atar;

    atal = (mqal > 0) ? (mqal_save*100)/mqal : 0;
    atar = (mqar > 0) ? (mqar_save*100)/mqar : 0;
    sprintf(mbuf,"amp/amp left,right   = %9d %9d\n",atal,atar);
    msg_printf(DBG,mbuf);
    mqal_save = mqal;
    mqar_save = mqar;

    if (left_channel < 0 && (atal < loval_left || atal > hival_left)) {
      sprintf(mbuf,"#%d--sin=%d gain=%d vol=%d left amp/amp %d-%d, got %d\n",
	    testnum,swfreq,gain,volume,
	    loval_left,hival_left,atal);
	if (print_error() < 0)
	    return -1;
    }
    if (right_channel < 0 && (atar < loval_right || atar > hival_right)) {
     sprintf(mbuf,"#%d--sin=%d gain=%d vol=%d right amp/amp %d-%d, got %d\n",
	    testnum,swfreq,gain,volume,
	    loval_right,hival_right,atar);
	if (print_error() < 0)
	    return -1;
    }
    return 0;
}

void micgain(void)
/* compute gains for microphone based on volume and input gain values */
{
    int fac;

    if (volume > NOMVOL) {	/* volume (NOMVOL+1)-255 */
	/* fac will be between 10 and 20 if NOMVOL=128 */
	/* volume=129, fac=10; volume=255, fac=20 */
	fac = (volume*10/NOMVOL); 
	/* multiply values by factor squared */
	loval_left = (lomic[gain]/100)*(fac*fac);
	hival_left = (himic[gain]/100)*(fac*fac);
    } else if (volume > 0) {	/* volume 1 - NOMVOL */
	/* fac will be between 1 and 1280 if NOMVOL = 128 */
	/* volume=1, fac=1280; volume=128, fac=10 */
	fac = (NOMVOL*10)/volume;
	loval_left = (lomic[gain]/(fac*fac))*100;
	hival_left = (himic[gain]/(fac*fac))*100;
    } else {
	loval_left = 0;
	hival_left = 10;
    }
    loval_right = loline[gain];
    hival_right = hiline[gain];
}

int test_first(void)
/* test first mean of squared amplitudes to ensure it falls within a given
 * range. If it doesn't, return an error.
 */
{
    int imqal, imqar;

    imqal = (sindiv) ? mqal*64 : mqal;
    imqar = (sindiv) ? mqar*64 : mqar;
    mqal_save = mqal;
    mqar_save = mqar;

    if (left_channel < 0 && (imqal < loval_left || imqal > hival_left)) {
      sprintf(mbuf,"#%d--sin=%d gain=%d vol=%d left amp %d-%d, got %d\n",
	    testnum,swfreq,gain,volume,
	    loval_left,hival_left,imqal);
	if (print_error() < 0)
	    return -1;
    }
    if (right_channel < 0 && (imqar < loval_right || imqar > hival_right)) {
     sprintf(mbuf,"#%d--sin=%d gain=%d vol=%d right amp %d-%d, got %d\n",
	    testnum,swfreq,gain,volume,
	    loval_right,hival_right,imqar);
	if (print_error() < 0)
	    return -1;
    }
    return 0;
}

int test_srf(void)
/* test sample rate clock to ensure fall within range. If not, return error.
 */
{
    int clk1,clk2,clk, dsp_txclk, dsp_rxclk;
    int i, rxclk, txclk, srfval;

    srfval = dsp[DSP_CP10MS]*10/2;

    dsp_txclk = dsp[DSP_TXCLK_VAL] - 0x8000;
    clk1 = dsp[dsp_txclk] & 0xffffff;
    dsp_txclk += (dsp_txclk == DSP_TXCLK) ? 511 : -1;
    clk2 = dsp[dsp_txclk] & 0xffffff;
    clk = (clk2 - clk1) & 0xffffff;
    txclk = ((srfval*511)/clk)*10;

    dsp_rxclk = dsp[DSP_RXCLK_VAL] - 0x8000;
    clk1 = dsp[dsp_rxclk] & 0xffffff;
    dsp_rxclk += (dsp_rxclk == DSP_RXCLK) ? 511 : -1;
    clk2 = dsp[dsp_rxclk] & 0xffffff;
    clk = (clk2 - clk1) & 0xffffff;
    rxclk = ((srfval*511)/clk)*10;
    sprintf(mbuf,"tx,rx sample rate    = %9d Hz, %9d Hz\n",txclk,rxclk);
    msg_printf(DBG,mbuf);

    if (txclk < (txsrfval-100) || txclk > (txsrfval+100)) {
     sprintf(mbuf,"#%d--sin=%d gain=%d vol=%d transmit clock %d-%d, got %d\n",
		testnum,swfreq,gain,volume,
		txsrfval-100,txsrfval+100,txclk);
	if (print_error() < 0)
	    return -1;
    }
    if (rxclk < (rxsrfval-100) || rxclk > (rxsrfval+100)) {
     sprintf(mbuf,"#%d--sin=%d gain=%d vol=%d receive clock %d-%d, got %d\n",
		testnum,swfreq,gain,volume,
		(rxsrfval-100),(rxsrfval+100),rxclk);
	if (print_error() < 0)
	    return -1;
    }
    return 0;
}

int test_vol(void)
/* test mean square amplitude to volume ratios to ensure they fall
 * within a given range. If not, return error.
 */
{
    int atvl, imqal, atar;

    imqal = (sindiv) ? mqal*64 : mqal;
    atar = (mqar > 0) ? (mqar_save*100)/mqar : 0;
    atvl = (volume > 0) ? imqal / (volume * volume) : 0;
    sprintf(mbuf,"amp/vol left         = %9d\n",atvl);
    msg_printf(DBG,mbuf);
    sprintf(mbuf,"amp/amp right        = %9d\n",atar);
    msg_printf(DBG,mbuf);
    mqar_save = mqar;

    if (left_channel < 0 && (atvl < lovolume || atvl > hivolume)) {
      sprintf(mbuf,"#%d--sin=%d gain=%d vol=%d left amp/vol %d-%d, got %d\n",
		testnum,swfreq,gain,volume,
		lovolume,hivolume,atvl);
	if (print_error() < 0)
	    return -1;
    }
    if (right_channel < 0 && (atar < loval_right || atar > hival_right)) {
     sprintf(mbuf,"#%d--sin=%d gain=%d vol=%d right amp/amp %d-%d, got %d\n",
	    testnum,swfreq,gain,volume,
	    loval_right,hival_right,atar);
	if (print_error() < 0)
	    return -1;
    }
    return 0;
}

int test_nonaud(void)
/* test mean square amplitude to volume ratios to ensure they fall
 * within a given range. If not, return error.
 */
{
    int i, j, data, indata;

    for (i = (BUFSTART+0x40); i < (BUFSTART+0x400); i += 0x100) {
	for (j = 0; j < 16; j++) {
	    indata = dsp[i+j] & 0xffffff;
	    data = condata[j];
	    if (data != indata) {
		sprintf(mbuf,"#%d--consumer byte %d expected %x, got %x\n",
			testnum,j,data,indata);
		if (print_error() < 0)
		    return -1;
	    }
	}
	for (j = 0; j < 16; j++) {
	    indata = dsp[i+j+0x80] & 0xffffff;
	    if (j == 8)
		indata &= 0xffffe0;	/* ignore low 5 bits (see 3436 doc) */
	    data = brodata[j];
	    /* ignore low bytes of clock */
	    if (data != indata && j != 4 && j != 5) {
		sprintf(mbuf,"#%d--broadcast byte %d expected %x, got %x\n",
			 testnum,j,data,indata);
		if (print_error() < 0)
		    return -1;
	    }
	}
    }
    return 0;
}

void download(void)
/* halt dsp, download dsp code, and start dsp running.
 */
{
    static int nwords,i;
    static int *p;

    *miscsr = HPC1MISC_RESET | HPC1MISC_32K;
#ifdef DSP_TEXT_ORG
    nwords=sizeof(dsp_text)/sizeof(int);
    p = (int *)(dsp+(DSP_TEXT_ORG-0xc000));
    for(i=0;i<nwords;i++) {
	*p = dsp_text[i];
	p++;
    }
#endif
#ifdef DSP_YDATA_ORG
    nwords=sizeof(dsp_ydata)/sizeof(int);
    p = (int *)(dsp+(DSP_YDATA_ORG-0x8000));
    for(i=0;i<nwords;i++) {
	*p = dsp_ydata[i];
	p++;
    }
#endif
#ifdef DSP_XDATA_ORG
    nwords=sizeof(dsp_xdata)/sizeof(int);
    p = (int *)(dsp+(DSP_XDATA_ORG-0xc000));
    for(i=0;i<nwords;i++) {
	*p = dsp_xdata[i];
	p++;
    }
#endif
    dsp[DSP_ERRNUM] = 0xffffff;
    dsp[DSP_HS] = 0xdecade; /* wait for microcode to get started */
    *miscsr =  HPC1MISC_32K;
}

#ifdef _STANDALONE
int mapthedsp()
/* map the dsp memory into mips space for read/write access */
{
    dsp = (int *)PHYS_TO_K1(HPC1MEMORY);
    miscsr = (int *)PHYS_TO_K1(HPC1MISCSR);
    return 0;
}

void write_open(char *name)
/* standalone stub for file activity */
{}
void write_close(void)
/* standalone stub for file activity */
{}
void write_buf(void)
/* standalone stub for file activity */
{}
#else	/* not _STANDALONE */
int mapthedsp(void)
/* map the dsp memory into mips space for read/write access */
{
    int fd;
    int *p1, *p2;

    if ((fd = open("/dev/mmem",O_RDWR)) < 0) {
	perror("couldn't open /dev/mmem");
	return -1;
    }
    if ((int)(p1 = mmap(0,4*32768,PROT_READ|PROT_WRITE,MAP_SHARED,fd,
		  PHYS_TO_K1(HPC1MEMORY))) < 0) {
	perror("couldn't map HPC1 memory");
	return -1;
    }
    dsp = p1;	/* points to start of dsp memory on host side */
    if ((int)(p2 = mmap(0,4096,PROT_READ|PROT_WRITE,MAP_SHARED,fd,
		  PHYS_TO_K1(HPC_0_ID_ADDR))) < 0) {
	perror("couldn't map HPC_0_ID_ADDR memory");
	return -1;
    }
/* points to start of hpc memory on host side */
    miscsr = p2+((HPC1MISCSR-HPC_0_ID_ADDR)/sizeof(long));
    return 0;
}

void write_open(char *name)
/* open a data file for writing */
{
    if ((bufout = fopen(name,"w")) == NULL) {
	sprintf(mbuf, "Can't open %s for writing\n",name);
	msg_printf(ERR,mbuf); 
    }
}

void write_close(void)
/* close a data file for writing */
{
    fclose(bufout);
}

void write_buf(void)
/* write sample dsp buffer out to a file */
{
    int i, j;

    for (i = (BUFSTART+SAMPST); i < (BUFSTART+SAMPST+2*SAMPSIZE); i += 2) {
	j = get_dsp16(i);		/* left first */
	putw(j, bufout);
	j = get_dsp16(i+1);		/* right second */
	putw(j, bufout);
    }
}

#endif
