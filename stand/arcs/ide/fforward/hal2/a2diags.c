#if 0
#define PARANOID /* compiles "paraniod" diagnostics */
#endif
/**********************************************************
***     Audio Board Diagnostic Tests                    ***
***                                                     ***
***     Amit Shoham, 1993                               ***
**********************************************************/
#include <sys/types.h>
#include <fault.h>
#include <sys/param.h>
#include <sys/sysmacros.h>

#include "sys/sbd.h"
#include "sys/cpu.h"
#include "sys/immu.h"

#include "sys/hal2.h"
#include "arcs/hinv.h"
#include "libsc.h"
#include "libsk.h"
#include "uif.h"
#include "noise.h"
#include "ctype.h"
#include "pbus.h"

#define PHASE_FIX 1
#define NUM_TESTS 8
#define OK 0
#define MAXSAMPS 48000
#define BUF_MARGIN 1900 /* space for garbage samples at start of DMA */
#define NINTY_DEGREES 12000
#define TOO_BIG 1000000000 /* used for adjusting factors in Compute() */
#define MAX_DESCRIPTORS 400
#define OUT_CHAN 1
#define IN_CHAN 2

/******* THE FOLLOWING define LINES SET THE TEST BRACKETS ********/

#define DMA_WAIT_T 500             /* max DMA wait time in tens of ms */
#define LR_TEST_THRESHHOLD 4666    /* max phase error for chan phase test */
#define DC_MAX_IP22 450            /* max DC offset in all sweeps, IP22 */
#define DC_MAX_IP24 450            /* max DC offset in all sweeps, IP24 */
#define MDAC_OFF 1612              /* max amp reading with mdac at zero */
#define MDAC_OFF_NOISE 40          /* max noise with mdac at zero */
#define AMP_BRACKET_PERCENT_H 25   /* percent error allowed for amplitude^2 */
#define AMP_BRACKET_PERCENT_L 28   /* percent error allowed for amplitude^2 */

#ifndef PARANOID
#define AC_LEN 36                  /* DMA length factor, must be mult of 4 */
#else
#define AC_LEN 8
#endif

#define NUMFR 5                    /* number of frequencies in sweep */

/* Table of frequencies (in Hz) to test and gains of analog loop at different
   frequencies (in percents) */
static int frequencies[NUMFR]={      100,500,1000,2000,6000 }; 

/* fullhouse settings */ 
static int f_analog_line_gain[NUMFR]={  49, 50,  51,  51,  51 };

/* fullhouse Rev 4 settings */ 
static int f_analog_mic_gain_rev4[NUMFR]={   54, 55,  56,  56,  56 };

/* fullhouse Rev 3 settings */ 
static int f_analog_mic_gain_rev3[NUMFR]={   49, 50,  51,  51,  51 };

/* guiness */
static int g_analog_line_gain[NUMFR]={  49, 49,  50,  50,  50 };
static int g_analog_mic_gain[NUMFR]={   44, 45,  46,  46,  46 };

/* During gain/atten.  sweeps, the value DC_MAX serves as */
/* initial condition only and the actual limit increases proportionally */
/* with gain. */

/******* END OF TEST BRACKET defines *******/


#define STEREO  2
#define QUAD    4

#define AES_TX      0
#define AES_RX      1
#define CODEC_A     2
#define CODEC_B     3

#define DMA_TEST           0
#define CLOCK_TEST         1
#define AES_TEST           2
#define LINE1_TEST         3
#define QUAD_TEST          4
#define MIC_HEADPHONE_TEST 5
#define SR8K_TEST          6
#define AES_CLOCK_TEST     7

#define COMPUTE_THD_N   0
#define COMPUTE_NOISE   1

static int req_loopback[NUM_TESTS] = {0, 1, 0, 1, 1, 1, 1, 1};
static char* st_chan[STEREO] = {"(L)","(R)"};
static char* qu_chan[QUAD] = {"(Line1-L)","(Line1-R)","(Line2-L)","(Line2-R)"};

#define CHAN(x) (mode==STEREO?st_chan[x]:qu_chan[x])
#define SPIN while (*isrp & HAL2_ISR_TSTATUS);
#define ABS(x) ((x)>0 ? (x):(-x))
#define NOISE_MAX_LIMIT if (noise_max<23) noise_max=23;

/* pick # of words which is an integer # of sine cycles */
#define NWORDS(f) ((MAXSAMPS/AC_LEN)/(MAXSAMPS/f))*AC_LEN*(MAXSAMPS/f)

static int *sintab;			/* [MAXSAMPS] */
static int *tunetab;			/* [MAXSAMPS] */
static int *buf;			/* [MAXSAMPS+BUF_MARGIN*2] */
scdescr_t *output_desc;
scdescr_t *input_desc;
scdescr_t *aux_output_desc;
scdescr_t *aux_input_desc;
int mean[4],amplitude[4],variance[4],snr[4];
int mean_uv[4],amp_uv[4],var_uv[4];
int mean_uv2[4],amp_uv2[4],var_uv2[4];
static int noise_compute_mode;
static int dump_mode;
static int test_mode;
static int chan3_phase_sel;
static int pause_always,pause_on_error,debug_mode;
static int gain2volt;  /* for converting compute() results to volts */
static int DC_MAX;
static int use_DC_vec;		/* use DC_vec[i] values for DC_MAX */
static int DC_vec [4];		/* four explicit DC_MAX values */
static int *analog_line_gain;
static int *analog_mic_gain;
static int tenXgain = 334;

static int (*noise_lg)[NUMFR];
static int (*noise_la)[NUMFR];
static int (*noise_qg)[NUMFR];
static int (*noise_qa)[NUMFR];
static int (*noise_mg)[NUMFR];
static int *noise_mdac;
static int *noise_gs;
static int noise_8k;

static int setup_audio_memory(void);
static void maketone(int f, int dev, int mode);
static int AES_init(void);
static int DMA_walk(void);
static int Test_AES(void);
static int Test_Clocks(void);
static int Test_Clocks(void);
static int TestLine(int mode);
static int TestMicHP(void);
static int Test_8k(void);
static int Test_Rx_Clock(void);
static int CheckToneFreq(void);
static void gettone(int,int,int);
static printbuf(int,int,int,int);
static int LR_Check(int, int, int, int);
static void Compute(int,int);
static int sroot(int);
static void MakeSinTable(void);
static void SetupHAL2(void);
static void kpause(void);
static void SetupDMAChannel(int, int, int);
static void BuildDescriptors(scdescr_t *, int *, int, int);
static void InitiateDMA(scdescr_t *, int, int);
static void KillDMA(int);
static int WaitForOneDMA(int);

static int Delay_maketone;	/* delay after initiating a tone */
static int Delay_setup;		/* delay for codec coming out of reset */

static int ReportLimits = 0;	/* report sa_lo sa_hi limit numbers */

static int dotest(int);

/* main function call for diagnostic routines */
int
audio(int argc, char **argv)
{
  volatile __uint32_t *revptr = (__uint32_t *)PHYS_TO_K1(HAL2_REV);
  int testnum,errcount=0,firsttest=0,lasttest=NUM_TESTS;
  int i;
  int repeat_cycles=1;
  int t_fail[NUM_TESTS];
  unsigned short Rev;

#ifdef PARANOID
  msg_printf(DBG,"AC-LEN:%d (PARANOID)\n",AC_LEN);
#endif

  if (!setup_audio_memory())
    return(-1);
  hal2_configure_pbus_pio();
  hal2_configure_pbus_dma();
  AES_init();
  MakeSinTable();
  if (hal2_rev()) { /* see if hal2 is alive */
    msg_printf(SUM,"pretest failed: can't continue...\n");
    return(-1); /* no reasone to continue if hal2 is dead */
  }

  argc--; argv++;
  pause_always=0; pause_on_error=0;

  debug_mode=0;    /* this sets 50C temp brackets for t5 */
  dump_mode=0;     /* this disables buffer dumps on errors */
  test_mode=0;     /* this disables any special test/debugging mods */
  chan3_phase_sel=3+PHASE_FIX; /* chan3 may have same phase as chan1 */

  Delay_maketone = is_fullhouse () ? 185000 : 250000;
  Delay_setup = is_fullhouse () ? 0 : 750000;
  tenXgain = is_fullhouse () ? 334 : 280;
  analog_line_gain = is_fullhouse() ? f_analog_line_gain:g_analog_line_gain;

  if (is_fullhouse ()) {
	  DC_vec[0] = DC_MAX = DC_MAX_IP22;
	  DC_vec[1] = 5000;
	  DC_vec[2] = 10000;
	  DC_vec[3] = 20000;
	  use_DC_vec = 1;
  } else {
	  DC_vec [0] = DC_MAX = 450;
	  DC_vec [1] = 2200;
	  DC_vec [2] = 5000;
	  DC_vec [3] = 10000;
	  use_DC_vec = 1;
  }
  Rev = *revptr;

#if 0
 /* Use the same noise margin for both indigo2 and indy for now.
  */
 if (Rev == 0x1010) {
    noise_lg = f4_noise_lg;
    noise_la = f4_noise_la;
    noise_qg = f4_noise_qg;
    noise_qa = f4_noise_qa;
    noise_mg = f4_noise_mg;
    noise_mdac = f4_noise_mdac;
    noise_gs = f4_noise_gs;
    noise_8k = f4_noise_8k;
    analog_mic_gain = is_fullhouse() ? f_analog_mic_gain_rev4:g_analog_mic_gain;
  } else
#endif
  {
    noise_lg = f3_g_noise_lg;
    noise_la = f3_g_noise_la;
    noise_qg = f3_g_noise_qg;
    noise_qa = f3_g_noise_qa;
    noise_mg = f3_g_noise_mg;
    noise_mdac = f3_g_noise_mdac;
    noise_gs = f3_g_noise_gs;
    noise_8k = f3_g_noise_8k;
    analog_mic_gain = is_fullhouse() ? f_analog_mic_gain_rev3:g_analog_mic_gain;
  }
  if (is_fullhouse()) {
    analog_mic_gain = (Rev == 0x1010) ? f_analog_mic_gain_rev4 :
					f_analog_mic_gain_rev3 ;
  }
  else
    analog_mic_gain = g_analog_mic_gain;

  while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
    switch (argv[0][1]) {
      case 't':              /* set test number */
	firsttest=atoi(&argv[0][2]);
	lasttest=1+firsttest;
	if (firsttest>=NUM_TESTS || firsttest<0) errcount++;
	break;
      case 'p':             /* set pause always mode */
	pause_always=1;
	break;
      case 'e':             /* set pause on error mode */
	pause_on_error=1;
	break;
      case 'r':             /* set repeat mode */
	repeat_cycles=atoi(&argv[0][2]);
	break;
      case 'B':             /* set debugging bracket mode */
	debug_mode=1;
	break;
      case 'E':             /* pause and dump buffer on error */
	pause_on_error=1;
	dump_mode=1;
	break;
      case 'S':             /* enable test/debug mods done to diags */
	test_mode=1;
	break;
      case 'T':             /* enable test/debug mods done to diags */
	chan3_phase_sel=4-PHASE_FIX;
	break;
      case 'D':
	switch (argv[0][2]) {
#ifdef notdef
	case 'M':	/* delay after each maketone starts */
		Delay_maketone = atoi (argv [0] + 3);
		break;
	case 'S':	/* relay settling time */
		Delay_setup = atoi (argv [0] + 3);
		break;
#endif
	case 'L':
		if (argv [0][3] == '0' ||
		    argv [0][3] == 'o' || argv [0][3] == 'O') {
			ReportLimits = 0;
		} else {
			ReportLimits = 1;
		}
		break;
#ifdef notdef
	case 'C': {	/* DC_MAX, one value or four (one per gain setting) */

			int i;
			char *p;

			p = argv [0] + 3;
			use_DC_vec = 0;
			DC_vec [i = 0] = 0;;
			while (*p != '\0') {
				if (isdigit (*p)) {
					DC_vec [i] = DC_vec [i] * 10 + *p - '0';
				} else if (*p == ',') {
					++use_DC_vec;
					if (++i >= 4) break;
					DC_vec [i] = 0;
				}
				++p;
			}
			DC_MAX = DC_vec [0];
		}
		break;
#endif
	default:
		msg_printf(SUM, "No such delay or constant\n");
		errcount=-1;
		goto end_prog;
	}
	break;
      default:
	errcount++;
	break;
    }
    argc--; argv++;
  }
  if (argc || errcount){
    msg_printf(SUM,"Usage: audio [-t0-%d][-p][-e][-r#]\n",NUM_TESTS-1);
    errcount=-1;
    goto end_prog;
  }

  for (i=0; i<NUM_TESTS; i++)
    t_fail[i]=0;
  while (repeat_cycles) {
    for (testnum=firsttest; testnum<lasttest; testnum++)
      if (dotest(testnum)) {
	errcount++;
	t_fail[testnum]++;
      }
    msg_printf(SUM,"Audio diagnostics complete -- %d errors\n",errcount);
    if (errcount) {
      msg_printf(SUM,"Tests Failed:");
      for (i=0; i<NUM_TESTS; i++)
	if (t_fail[i]) msg_printf(SUM," #%d",i);
      msg_printf(SUM,"\n");
    }
    repeat_cycles--;
  }
end_prog:
  busy(0);
  return(errcount);
}

/* main function call for audio passthru */
thru(int argc, char **argv)
{
  volatile __uint32_t *idr0p=(__uint32_t *)PHYS_TO_K1(HAL2_IDR0);
  volatile __uint32_t *idr1p=(__uint32_t *)PHYS_TO_K1(HAL2_IDR1);
  volatile __uint32_t *iarp=(__uint32_t *)PHYS_TO_K1(HAL2_IAR);
  volatile __uint32_t *isrp=(__uint32_t *)PHYS_TO_K1(HAL2_ISR);
  volatile __uint32_t *hpl=(__uint32_t *)PHYS_TO_K1(VOLUME_LEFT);
  volatile __uint32_t *hpr=(__uint32_t *)PHYS_TO_K1(VOLUME_RIGHT);
  volatile int ctrlval0,ctrlval1;

  int gain=0;
  int volume=128;
  int atten=0;
  int frequency=1000;
  int mode=STEREO;
  int quad_include_digital=0;
  int aes_to_quad=0;
  char source='1';
  char dest='1';
  int sdev=CODEC_B;
  int ddev=CODEC_A;
  int rate=48000;
  int gain_inc=0;
  register int i,j;
  int *bufp;

  chan3_phase_sel=3+PHASE_FIX;
  if (!setup_audio_memory())
    return(-1);
  hal2_configure_pbus_pio();
  hal2_configure_pbus_dma();
  AES_init();
  MakeSinTable();
  if (hal2_rev()) { /* see if hal2 is alive */
    msg_printf(SUM,"pretest failed: can't continue...\n");
    return(-1); /* no reason to continue if hal2 is dead */
  }
  SetupHAL2();
  argc--; argv++;
  while (argc && argv[0][0]=='-' && argv[0][1]!='\0'){
    switch (argv[0][1]) {
      case 'f':  /* set frequency for tone output */
        frequency=atoi(&argv[0][2]);
	break;
      case 'r':  /* select a bress. clock setting */
	rate=atoi(&argv[0][2]);
      case 'g':  /* set codec input gain */
	gain=atoi(&argv[0][2])&0x0f;
	break;
      case 'v':  /* set headphone volume (mdac) */
	volume=atoi(&argv[0][2])&0xff;
	break;
      case 'a':  /* set codec output attenuation */
	atten=atoi(&argv[0][2])&0x1f;
	break;
      case 's':  /* select input source */
	source=argv[0][2];
	break;
      case 'd':  /* select output destination */
	dest=argv[0][2];
	break;
      case 'i':  /* gain increment */
	gain_inc=atoi(&argv[0][2]);
	break;
      case 'q':  /* quad mode */
	mode=QUAD;
	break;
      case 'Q':  /* quad mode with additional digital loop */
	mode=QUAD;
	quad_include_digital=1;
	break;
      case 'D':  /* digital to dual line mode */
	mode=QUAD;
	quad_include_digital=1;
	aes_to_quad=1;
	rate=0; /* use aes rate */
	break;
      case 'T':  /* toggle the phase mode of channels 2,3 */
	chan3_phase_sel=4-PHASE_FIX;
	break;
      default:
	msg_printf(ERR,"Usage: thru [-sdfgavq]\n");
	return(-1);
      }
    argc--; argv++;
  }
  *hpl=volume; *hpr=volume;
  if (rate) {
    frequency*=48000;
    frequency/=rate;
  }
  switch (rate) {
  case 48000: break; /* this is the default, so do nothing */
  case 44100:
    *idr0p=1;
    *iarp=HAL2_BRES1_CTRL1_W; /* select 441 master clock */
    SPIN;
    break;
  case 32000:
    *idr0p=4;
    *idr1p=0xffff & (4-6-1);
    *iarp=HAL2_BRES1_CTRL2_W;
    SPIN;
    break;
  case 16000:
    *idr0p=4;
    *idr1p=0xffff & (4-12-1);
    *iarp=HAL2_BRES1_CTRL2_W;
    SPIN;
    break;
  case 8000:
    *idr0p=4;
    *idr1p=0xffff & (4-24-1);
    *iarp=HAL2_BRES1_CTRL2_W; /* set bres clock to 1/6 Fs */
    SPIN;
    break;
  case 0:
    *idr0p=0x2;
    *iarp=HAL2_BRES1_CTRL1_W; /* set AES master clock */
    SPIN;
    break;
  default: /* rate unknown */
    msg_printf(ERR,"Unknown sample rate.\n");
  }
  ctrlval1=(atten<<7)+(atten<<2)+0x03;
  if (mode==QUAD) {
    *isrp = HAL2_ISR_CODEC_RESET_N|HAL2_ISR_GLOBAL_RESET_N
      |HAL2_ISR_CODEC_MODE;
    us_delay(50);
    ctrlval0=(gain<<4)+gain;
    *idr0p=ctrlval0; *idr1p=ctrlval1;
    *iarp=HAL2_CODECB_CTRL2_W;
    ctrlval0 += (3<<8);
    SPIN;
    *idr0p=ctrlval0; *idr1p=ctrlval1;
    *iarp=HAL2_CODECA_CTRL2_W;
    SPIN;
  }
  else {
    switch (source){ /* setup input device */
    case '1':  /* line 1 */
      ctrlval0=(gain<<4)+gain;
      break;
    case '2':  /* line 2 */
      ctrlval0=(3<<8)+(gain<<4)+gain;
      break;
    case 'm':  /* mic */
      ctrlval0=(3<<8)+(gain<<4)+gain;
      ctrlval1=(atten<<7)+(atten<<2)+0x01;
      break;
    case 'M':  /* mic w. x10 gain */
      ctrlval0=(3<<8)+(gain<<4)+gain;
      ctrlval1=(atten<<7)+(atten<<2);
      break;
    case 'd':  /* AES in */
      *idr0p=0x2;
      *iarp=HAL2_BRES1_CTRL1_W; /* use AES RX master for clock */
      SPIN;
      ctrlval0=0;
      sdev=AES_RX;
      break;
    case 's':  /* sine wave */
      ctrlval0=0;
      break;
    }
    *idr0p=ctrlval0;
    *idr1p=ctrlval1;
    *iarp=HAL2_CODECB_CTRL2_W;
    SPIN;
    *idr0p=ctrlval0;
    *idr1p=ctrlval1;
    *iarp=HAL2_CODECA_CTRL2_W;
    SPIN;
    if (dest=='d') {
      ddev=AES_TX;
      maketone(1000,CODEC_A,STEREO);
      KillDMA(OUT_CHAN);
    }

    if (dest=='h') {
      *idr0p=0x1;
      *iarp=HAL2_RELAY_CONTROL_W;
      SPIN;
    }
  }
  if (source=='s')  /* generate sine wave */
    maketone(frequency,ddev,mode);
  else {
    BuildDescriptors(input_desc,buf,MAXSAMPS/16,1);
    BuildDescriptors(output_desc,buf,MAXSAMPS/16,1);
    SetupDMAChannel(OUT_CHAN,ddev,mode);
    SetupDMAChannel(IN_CHAN,sdev,mode);
    if (!aes_to_quad)
      InitiateDMA(input_desc,1,IN_CHAN);
    InitiateDMA(output_desc,0,OUT_CHAN);
    if (quad_include_digital) {
      *idr0p=0x2;
      *iarp=HAL2_BRES2_CTRL1_W;
      SPIN;
      *idr0p=4;
      *idr1p=0xffff & (-1);
      *iarp=HAL2_BRES2_CTRL2_W;
      SPIN;
      BuildDescriptors(aux_input_desc,&buf[MAXSAMPS/8],MAXSAMPS/32,1);
      BuildDescriptors(aux_output_desc,&buf[MAXSAMPS/8],MAXSAMPS/32,1);
      SetupDMAChannel(IN_CHAN+2,AES_RX,STEREO);
      SetupDMAChannel(OUT_CHAN+2,AES_TX,STEREO);
      *idr0p=(OUT_CHAN+2)+(0x2<<3)+(0x2<<8);
      *iarp=HAL2_AESTX_CTRL_W;
      SPIN;
      InitiateDMA(aux_input_desc,1,IN_CHAN+2);
      if (!aes_to_quad)
	InitiateDMA(aux_output_desc,0,OUT_CHAN+2);
    }
  }
  if (aes_to_quad) {
    msg_printf(SUM,"Loopthru running...\n");
    bufp=&buf[MAXSAMPS/8];
    while(1)
      for (i=0; i<MAXSAMPS/32; i+=2) {
	j=i<<1;
	buf[j]=bufp[i];
	buf[j+1]=bufp[i+1];
	buf[j+2]=bufp[i];
	buf[j+3]=bufp[i+1];
      }
  }
  else {
    msg_printf(SUM,"Loopthru running... hit any key to quit.\n");
    while (!getchar()) {
      if ((mode==QUAD)&&(gain_inc)) {
	gain=(gain+gain_inc)&(0x0f);
	ctrlval0=(gain<<4)+gain;
	*idr0p=ctrlval0;
	*iarp=HAL2_CODECB_CTRL2_W;
	ctrlval0 += (3<<8);
	SPIN;
	*idr0p=ctrlval0;
	*iarp=HAL2_CODECA_CTRL2_W;
	SPIN;
	us_delay(1000000);
      }
    }
    KillDMA(OUT_CHAN);
    KillDMA(IN_CHAN);
    if (quad_include_digital) {
      KillDMA(OUT_CHAN+2);
      KillDMA(IN_CHAN+2);
    }
  }

	return 0;
}

/* this simply calls the test function identified by testnum. */
/* may add some text printing here - if a test fails print a list */
/* of the major parts on the a2 board that may be at fault */
int
dotest(int testnum)
{
  SetupHAL2(); /* start w. HAL2 in known state */
  noise_compute_mode=COMPUTE_THD_N;
  if ((testnum<0) || (testnum >= NUM_TESTS)) {
    msg_printf(SUM,"bad test number - %d\n",testnum);
    return(OK);
  }
  if (req_loopback[testnum])
    msg_printf(SUM,"\nTest requires loopback fixture.\n");
  switch (testnum){
    case DMA_TEST:
      return DMA_walk();

    case CLOCK_TEST:
      return Test_Clocks();

    case AES_TEST:
      return Test_AES();

    case LINE1_TEST:
      return TestLine(STEREO);

    case QUAD_TEST:
      return TestLine(QUAD);

    case MIC_HEADPHONE_TEST:
      if (!debug_mode)
	noise_compute_mode=COMPUTE_NOISE;
      return TestMicHP();

    case SR8K_TEST:
      return Test_8k();

    case AES_CLOCK_TEST:
      return Test_Rx_Clock();

    default:
      msg_printf(SUM,"bad test number - %d\n",testnum);
      return(OK);

    }
}

/* test all DMA channels */
int
DMA_walk(void)
{
  int i,chan,dir;

  msg_printf(DBG,"DMA test entered...\n");
  for (i=0;i<1024;i++) tunetab[i]=0; /* output silence */
  BuildDescriptors(output_desc,tunetab,1024,0);

  for (dir=0;dir<2;dir++) /* test both input and output directions */
    for (chan=1;chan<5;chan++) {
      SetupDMAChannel(chan,CODEC_A,STEREO);
      InitiateDMA(output_desc,dir,chan);
      if (WaitForOneDMA(chan)) {
	msg_printf(ERR,"DMA test failed!!!\n");
	return(-1);
      }
    }
  return(OK);
}

/* Test Clocks: generate 1k tone and read it w. timestamping,  */
/*              then count zero crossings and divide by total  */
/*              time to make sure frequency, and therefore     */
/*              sampling rate, is correct.                     */
int
Test_Clocks(void)
{
  volatile __uint32_t *idr0p=(__uint32_t *)PHYS_TO_K1(HAL2_IDR0);
  volatile __uint32_t *iarp=(__uint32_t *)PHYS_TO_K1(HAL2_IAR);
  volatile __uint32_t *isrp=(__uint32_t *)PHYS_TO_K1(HAL2_ISR);
  int err=OK;

  msg_printf(VRB,"Clock test entered...\n");
  /* test 48kHz master clock */
  maketone(1000,CODEC_A,STEREO);
  SetupDMAChannel(IN_CHAN,CODEC_B,STEREO);
  *idr0p=0x28+IN_CHAN; /* enable timestamping */
  *iarp=HAL2_CODECB_CTRL1_W;
  SPIN;
  us_delay(150000);
  BuildDescriptors(input_desc,buf,MAXSAMPS/2,0);
  InitiateDMA(input_desc,1,IN_CHAN);
  if (WaitForOneDMA(IN_CHAN)) {
    msg_printf(ERR,"DMA failure: exiting clock test.\n");
    return(-1);
  }
  KillDMA(OUT_CHAN);
  if (CheckToneFreq()) {
    msg_printf(ERR,"48kHz master clock - test failed.\n\n");
    err--;
  }

  /* test 44.1kHz master clock */
  *idr0p=0x1; /* select 44.1kHz master */
  *iarp=HAL2_BRES1_CTRL1_W;
  SPIN;
  /* freq. of 1088 used instead of 1000 to compensate for clock change */
  maketone(1088,CODEC_A,STEREO);
  SetupDMAChannel(IN_CHAN,CODEC_B,STEREO);
  *idr0p=0x28+IN_CHAN; /* enable timestamping */
  *iarp=HAL2_CODECB_CTRL1_W;
  SPIN;
  us_delay(150000);
  BuildDescriptors(input_desc,buf,MAXSAMPS/2,0);
  InitiateDMA(input_desc,1,IN_CHAN);
  if (WaitForOneDMA(IN_CHAN)) {
    msg_printf(ERR,"DMA failure: exiting clock test.\n");
    return(-1);
  }
  KillDMA(OUT_CHAN);
  if (CheckToneFreq()) {
    msg_printf(ERR,"44.1kHz master clock - test failed.\n\n");
    err--;
  }
  return(err);
}

/* Checks a buffer of timestamped data for a 1k frequency by */
/* counting zero crossings and dividing by total time. */
int
CheckToneFreq(void)
{
  int time,freq,zero_cross=0;
  int samp,lastsamp,i,mean=0;

  /* get mean for removing DC offset */
  for (i=BUF_MARGIN; i<MAXSAMPS/2; i+=4)
    mean+=buf[i]>>8;
  mean/=(((MAXSAMPS/2)-BUF_MARGIN)/4);

  /* count zero crossings */
  lastsamp=((buf[BUF_MARGIN]>>8)-mean)>>8;
  for (i=BUF_MARGIN; i<MAXSAMPS/2; i+=4) {
    samp=((buf[i]>>8)-mean)>>8;
    if (((lastsamp>0)&(samp<=0))|((lastsamp<=0)&(samp>0)))
      zero_cross++;
    lastsamp=samp;
  }

  /* get total time in microseconds */
  time=buf[(MAXSAMPS/2)-1]+((buf[(MAXSAMPS/2)-2]-buf[BUF_MARGIN+2])*1000000)
    -buf[BUF_MARGIN+3];
  /* compute */
  freq = (zero_cross*500000)/time;
  msg_printf(DBG,"measured tone frequency of %dHz\n",freq);
  if ((freq>1020)|(freq<980)) {
    msg_printf(ERR,"tone frequency is not within +/-2%% of 1kHz\n");
    return(-1);
  }
  return(OK);
}

/* Tests AES in/out by playing tone on out, reading it back,     */
/* finding the "starting point" by comparing pairs of samples to */
/* first 4 samples in output table, then comparing sent and      */
/* received data for exact digital match */
int
Test_AES(void)
{
  volatile __uint32_t *idr0p=(__uint32_t *)PHYS_TO_K1(HAL2_IDR0);
  volatile __uint32_t *idr1p=(__uint32_t *)PHYS_TO_K1(HAL2_IDR1);
  volatile __uint32_t *iarp=(__uint32_t *)PHYS_TO_K1(HAL2_IAR);
  volatile __uint32_t *isrp=(__uint32_t *)PHYS_TO_K1(HAL2_ISR);
  volatile __uint32_t *aesrx_sr2=(__uint32_t *)PHYS_TO_K1(AESRX_SR2);
  int nwords,i,j,sr,status;

  msg_printf(VRB,"AES test entered...\n");
  nwords=NWORDS(1000)/4;

  for (sr=0; sr<3; sr++) { /* perform test at 3 different sampling rates */
    switch (sr) {
    case 0: /* 48kHz sampling rate */
      msg_printf(DBG," - testing 48kHz rate\n");
      break;
    case 1: /* 44.1kHz sampling rate */
      *idr0p=1;
      *iarp=HAL2_BRES1_CTRL1_W;
      SPIN;
      msg_printf(DBG," - testing 44.1kHz rate\n");
      break;
    case 2: /* 32kHz sampling rate */
      *idr0p=0;
      *iarp=HAL2_BRES1_CTRL1_W;
      SPIN;
      *idr0p=4;
      *idr1p=0xffff & (4-6-1);
      *iarp=HAL2_BRES1_CTRL2_W;
      SPIN;
      msg_printf(DBG," - testing 32kHz rate\n");
      break;
    default: /* 44.1kHz sampling rate */
      *idr0p=1;
      *iarp=HAL2_BRES1_CTRL1_W;
      SPIN;
      msg_printf(DBG," - testing 44.1kHz rate\n");
      break;
    }
    maketone(1000,AES_TX,STEREO);
    us_delay(150000);
    status=*aesrx_sr2;
    us_delay(5000);
    status=(*aesrx_sr2)&0xff;
    if (status&0x1f) {
      msg_printf(ERR,"digital test failed - status:0x%x\n",status);
      if (status&0x10)
	msg_printf(DBG,"AESRX chip receiving weak or noisy signal.\n");
      if (status&0x08)
	msg_printf(DBG,"AESRX chip can't lock onto signal.\n");
      if (status&0x06)
	msg_printf(DBG,"AESRX chip dectecting data errors in signal.\n");
      if (status&0x01)
	msg_printf(DBG,"Validity bit error.\n");
      return(-1);
    }
    for (i=0; i<10000; i++)
      if (status != ((*aesrx_sr2)&0xff)) {
	msg_printf(ERR,"digital test failed - unreliable status register.\n");
	return(-1);
      }
    gettone(nwords,AES_RX,STEREO);
    KillDMA(OUT_CHAN);

    for (i=0; i<nwords-3000; i+=2)
      if ((buf[i]==tunetab[0])&(buf[i+1]==tunetab[1])&
	  (buf[i+2]==tunetab[2])&(buf[i+3]==tunetab[3]))
	break;
    if (i>=(nwords-3000)) {
      msg_printf(ERR,"digital test failed - no starting point.\n");
      return(-1);
    }
    for (j=0; j<1000; j++)
      if (buf[i+j]!=tunetab[j]) {
	msg_printf(ERR,"digital test failed - sample mismatch.\n");
	msg_printf(DBG,"sample #%d = %d (expected %d)\n"
		   ,j,buf[i+j],tunetab[j]);
	return(-1);
      }
  }
  return(OK);
}

/* Report where the measured value falls between the given limits, as a
 * percentage.
 */
ShowLimits (int measured, int low, int high)
{
	int t1 = (measured - low);
	int t2 = (high - low);

	if (t1 > 1000000 || t2 > 1000000) {
		t1 = t1 / 1000;
		t2 = t2 / 1000;
	}

	msg_printf (VRB,
	    "- - - Low: %10d    High %10d   Measured %10d   %3d%%\n",
		low, high, measured, 100 * t1 / t2);

	return 0;
}

/* Test Line in/out by playing sine waves with various frequencies */
/* and gain and attenuation levels, and computing the amplitude    */
/* noise and DC offsets of the returned signal.                    */
static
int
TestLine(int mode)
{
  volatile __uint32_t *idr0p=(__uint32_t *)PHYS_TO_K1(HAL2_IDR0);
  volatile __uint32_t *idr1p=(__uint32_t *)PHYS_TO_K1(HAL2_IDR1);
  volatile __uint32_t *iarp=(__uint32_t *)PHYS_TO_K1(HAL2_IAR);
  volatile __uint32_t *isrp=(__uint32_t *)PHYS_TO_K1(HAL2_ISR);
  int fi,g,a,f,chn;
  int gval,aval;
  int ampl_f,sa_hi,sa_lo,dc_max,noise_max;
  int err_last,err=0;
  int ampl[4];

  if (mode==STEREO) msg_printf(VRB,"line test entered...\n");
  else {
    msg_printf(VRB,"4-chan test entered...\n");
    *isrp = HAL2_ISR_CODEC_RESET_N|HAL2_ISR_GLOBAL_RESET_N
      |HAL2_ISR_CODEC_MODE;
    us_delay(500);
  }
  msg_printf(VRB,"sweeping input gain (line)\n");
  a=15; /* attenuate by 22.5dB while sweeping gain */
  for (chn=0; chn<mode; chn++)
    ampl[chn]=2457; /* 22.5 dB down from 32768 */
  gain2volt=3052;
  dc_max= DC_MAX;
  for (g=0; g<16; g+=5) {
    if (use_DC_vec) dc_max = DC_vec [g / 5];
    gval=g+(g<<4);
    aval=(a<<7)+(a<<2)+0x03;
    *idr0p=gval+(0x0<<10);
    *idr1p=aval;
    *iarp=HAL2_CODECB_CTRL2_W;
    SPIN;
    *idr0p=gval+(3<<8)+(0x0<<10);
    *idr1p=aval;
    *iarp=HAL2_CODECA_CTRL2_W;
    SPIN;
    for (fi=0; fi<NUMFR; fi++) {
      f=frequencies[fi];
      maketone(f,CODEC_A,mode);
      if (fi == 0) {
	us_delay (Delay_setup);	/* first time only */
      }
      gettone(NWORDS(f)*mode/AC_LEN,CODEC_B,mode);
#if (defined(IP26) || defined(IP28))
      KillDMA(OUT_CHAN);
#endif
      msg_printf(DBG,"CONDITIONS: atten=15 gain=%d frequency=%d\n",g,f);
      Compute(f,mode);
      err_last=err;
      for (chn=0; chn<mode; chn++) {
	ampl_f=(ampl[chn]*analog_line_gain[fi])/100;
	sa_hi=(ampl_f*ampl_f/100)*(AMP_BRACKET_PERCENT_H+100);
	sa_lo=(ampl_f*ampl_f/100)*(100-AMP_BRACKET_PERCENT_L);
        if (ABS(mean[chn])>dc_max) {
	  err--;
	  msg_printf(ERR,"Test #%d:High DC offset on channel %d%s: %duV\n"
	             ,(mode/2)+2,chn,CHAN(chn),mean_uv[chn]);
	  msg_printf(VRB,"Max allowed reading: %d  measured: %d\n"
		     ,dc_max,mean[chn]);
	}
	if (mode==QUAD) noise_max=amplitude[chn]/noise_qg[g/5][fi];
	else noise_max=amplitude[chn]/noise_lg[g/5][fi];
	NOISE_MAX_LIMIT;
	if (variance[chn]<0 || variance[chn]>noise_max) {
	  err--;
          msg_printf(ERR,"Test #%d:High noise on channel %d%s: %duV(rms)\n"
		     ,(mode/2)+2,chn,CHAN(chn),var_uv[chn]);
	  msg_printf(VRB,"Max allowed reading: %d  measured: %d\n"
		     ,noise_max,variance[chn]);
	}
	if (ReportLimits) ShowLimits (amplitude [chn], sa_lo, sa_hi);
	if (amplitude[chn]<sa_lo || amplitude[chn]>sa_hi) {
          err--;
          msg_printf(ERR,"Test #%d:Gain error on channel %d%s: %duV(rms)\n"
		     ,(mode/2)+2,chn,CHAN(chn),amp_uv[chn]);
	  msg_printf(VRB,"Allowed range: %d to %d  measured: %d\n"
		     ,sa_lo,sa_hi,amplitude[chn]);
	}
      }
      if (err_last-err) {
	msg_printf(VRB,"Gain:%d Atten:%d Frequency:%d\n",g,a,f);
	if (pause_always | pause_on_error) kpause();
	if (dump_mode) {
	  msg_printf(DBG,"BUFFER START\n");
	  printbuf(BUF_MARGIN,NWORDS(f)*mode/AC_LEN,mode,0);
	  msg_printf(DBG,"BUFFER END\n");
	}
      }
      else if (pause_always) kpause();

    }
    for (chn=0; chn<mode; chn++) {
      /* compute expected ampl based on last ampl and 7.5 dB increase */
      ampl[chn]=(sroot(amplitude[chn])*237)/analog_line_gain[NUMFR-1];
    }
    dc_max*=2376; dc_max/=1000;
    gain2volt*=1000; gain2volt/=2371;
  }

  msg_printf(VRB,"\nsweeping output attenuation (line)\n");
  g=0; /* start w. no gain while sweeping atten. */
  /* expected amplitude is full scale when g=a=0 */
  for (chn=0; chn<mode; chn++)
    ampl[chn]=32768;
  gain2volt=3052;
  for (a=0; a<32; a+=5) {
    if (a==20) a=16; /* make sure last value is 31, not 30 */
    gval=g+(g<<4);
    aval=(a<<7)+(a<<2)+0x03;
    *idr0p=gval; *idr1p=aval;
    *iarp=HAL2_CODECB_CTRL2_W;
    SPIN;
    *idr0p=gval+(3<<8); *idr1p=aval;
    *iarp=HAL2_CODECA_CTRL2_W;
    SPIN;
    for (fi=0; fi<NUMFR; fi++) {
      f=frequencies[fi];
      maketone(f,CODEC_A,mode);
      gettone(NWORDS(f)*mode/AC_LEN,CODEC_B,mode);
#if (defined(IP26) || defined(IP28))
      KillDMA(OUT_CHAN);
#endif
      msg_printf(DBG,"CONDITIONS: atten=%d gain=0 frequency=%d\n",a,f);
      Compute(f,mode);
      err_last=err;
      for (chn=0; chn<mode; chn++) {
	ampl_f=(ampl[chn]*analog_line_gain[fi])/100;
	sa_lo=(ampl_f*ampl_f/100)*(100-AMP_BRACKET_PERCENT_L);
	sa_hi=(ampl_f*ampl_f/100)*(100+AMP_BRACKET_PERCENT_H);
	if (ABS(mean[chn])>DC_MAX) {
	  err--;
	  msg_printf(ERR,"Test #%d:High DC offset on channel %d%s: %duV\n",
		     (mode/2)+2,chn,CHAN(chn),mean_uv[chn]);
	  msg_printf(VRB,"Max allowed reading: %d  measured: %d\n"
		     ,DC_MAX,mean[chn]);
	}
	if (mode==QUAD) noise_max=amplitude[chn]/noise_qa[(a+4)/5][fi];
	else noise_max=amplitude[chn]/noise_la[(a+4)/5][fi];
	NOISE_MAX_LIMIT;
	if (variance[chn]<0 || variance[chn]>noise_max) {
	  err--;
	  msg_printf(ERR,"Test #%d:High noise on channel %d%s: %duV(rms)\n"
		     ,(mode/2)+2,chn,CHAN(chn),var_uv[chn]);
	  msg_printf(VRB,"Max allowed reading: %d  measured: %d\n"
		     ,noise_max,variance[chn]);
	}
	if (ReportLimits) ShowLimits (amplitude [chn], sa_lo, sa_hi);
	if (amplitude[chn]<sa_lo || amplitude[chn]>sa_hi) {
	  err--;
	  msg_printf(ERR,"Test #%d:Gain error on channel %d%s: %duV(rms)\n"
		     ,(mode/2)+2,chn,CHAN(chn),amp_uv[chn]);
	  msg_printf(VRB,"Allowed range: %d to %d  measured: %d\n"
		     ,sa_lo,sa_hi,amplitude[chn]);
	}
      }
      if (err_last-err) {
	msg_printf(VRB,"Gain:%d Atten:%d Frequency:%d\n",g,a,f);
	if (pause_always | pause_on_error) kpause();
      }
      else if (pause_always) kpause();

      if (a==0) {/* when atten & gain both zero, also check L/R & F/R */
	if (mode==QUAD) {
	  if (LR_Check(0,1,f,mode)||LR_Check(1,2,f,mode)) {
	    msg_printf(ERR,
		       "Channel phase failure! Tested at %dHz\n",f);
	    err--;
	    if (pause_on_error) kpause();
	  }
	  else if (((PHASE_FIX) && LR_Check(2,3,f,mode)) ||
		   ((!PHASE_FIX) && LR_Check(3,1,f,mode))) {
	    msg_printf(ERR,
		       "Channel phase failure! Tested at %dHz\n",f);
	    err--;
	    if (pause_on_error) kpause();
	  }
	}
	else {
	  if (LR_Check(0,1,f,mode)) {
	    msg_printf(ERR,"Left/Right phase failure! Tested at %dHz\n",f);
	    err--;
	    if (pause_on_error) kpause();
	  }
	}
      }
    }
    for (chn=0; chn<mode; chn++) {
      ampl[chn]=(sroot(amplitude[chn])*100)/analog_line_gain[NUMFR-1];
      if (a==15) {
	ampl[chn]*=8414; /* atten increases 1.5 dB */
      }
      else {
	ampl[chn]*=4217; /* atten increases 7.5 dB */
      }
      ampl[chn]/=10000;
    }
  }
  KillDMA(OUT_CHAN);
  return(err);
}

/* Test mic in/ hp out by playing sine waves with various frequencies */
/* and gain and attenuation levels, and computing the amplitude    */
/* noise and DC offsets of the returned signal.                    */
int
TestMicHP(void)
{
  volatile __uint32_t *idr0p=(__uint32_t *)PHYS_TO_K1(HAL2_IDR0);
  volatile __uint32_t *idr1p=(__uint32_t *)PHYS_TO_K1(HAL2_IDR1);
  volatile __uint32_t *iarp=(__uint32_t *)PHYS_TO_K1(HAL2_IAR);
  volatile __uint32_t *isrp=(__uint32_t *)PHYS_TO_K1(HAL2_ISR);
  volatile __uint32_t *hpl=(__uint32_t *)PHYS_TO_K1(VOLUME_LEFT);
  volatile __uint32_t *hpr=(__uint32_t *)PHYS_TO_K1(VOLUME_RIGHT);
  int fi,g,a,v,f,chn;
  int gval,aval;
  int ampl_f,sa_hi,sa_lo,dc_max,noise_max;
  int err_last,err=0;
  int ampl[2];

  *idr0p=1; /* put relay in headphone mode */
  *iarp=HAL2_RELAY_CONTROL_W;
  SPIN;
  msg_printf(VRB,"mic/headphone loopback test entered...\n");

  msg_printf(VRB,"sweeping input gain (mic/headphone)\n");
  *hpl=128; /* MDAC output at nominal vol. */
  *hpr=128;
  us_delay(80000);
  for (chn=0; chn<STEREO; chn++)
    ampl[chn]=2457; /* 22.5 dB down from 32768 */
  gain2volt=3052;
  dc_max=DC_MAX;
  for (g=0; g<16; g+=5) {
    if (use_DC_vec) dc_max = DC_vec [g / 5];
    gval=g+(g<<4);
    aval=(15<<7)+(15<<2)+0x01; /* mic power on, atten 22.5dB, gain x1 */
    *idr0p=gval; *idr1p=aval;
    *iarp=HAL2_CODECA_CTRL2_W;
    SPIN;
    *idr0p=gval+(3<<8); *idr1p=aval;
    *iarp=HAL2_CODECB_CTRL2_W;
    SPIN;
    for (fi=0; fi<NUMFR; fi++) {
      f=frequencies[fi];
      maketone(f,CODEC_A,STEREO);
      if (fi == 0) us_delay (Delay_setup);	/* first time only */
      gettone(NWORDS(f)*STEREO/AC_LEN,CODEC_B,STEREO);
#if (defined(IP26) || defined(IP28))
      KillDMA(OUT_CHAN);
#endif
      msg_printf(DBG,
		 "CONDITIONS: MDAC=128 atten=15 gain=%d frequency=%d\n",g,f);
      Compute(f,STEREO);
      err_last=err;
      for (chn=0; chn<2; chn++) {
	ampl_f=(ampl[chn]*analog_mic_gain[fi])/100;
	if (debug_mode)
	  sa_lo=(ampl_f*ampl_f/100)*(100-AMP_BRACKET_PERCENT_L);
	else
	  sa_lo=(ampl_f*ampl_f/100)*(95-AMP_BRACKET_PERCENT_L);
	sa_hi=(ampl_f*ampl_f/100)*(100+AMP_BRACKET_PERCENT_H);
	if (ABS(mean[chn])>dc_max) {
	  err--;
	  msg_printf(ERR,"Test #5:High DC offset on channel %d%s: %duV\n"
		     ,chn,st_chan[chn],mean_uv[chn]);
	  msg_printf(VRB,"Max allowed reading: %d  measured: %d\n"
		     ,dc_max,mean[chn]);
	}
	noise_max=amplitude[chn]/noise_mg[g/5][fi];
	NOISE_MAX_LIMIT;
	if (variance[chn]<0 || variance[chn]>noise_max) {
	  err--;
	  msg_printf(ERR,"Test #5:High noise on channel %d%s: %duV(rms)\n",
		     chn,st_chan[chn],var_uv[chn]);
	  msg_printf(VRB,"Max allowed reading: %d  measured: %d\n"
		     ,noise_max,variance[chn]);
	}
	if (ReportLimits) ShowLimits (amplitude [chn], sa_lo, sa_hi);
	if (amplitude[chn]<sa_lo || amplitude[chn]>sa_hi) {
	  err--;
	  msg_printf(ERR,"Test #5:Gain error on channel %d%s: %duV(rms)\n"
		     ,chn,st_chan[chn],amp_uv[chn]);
	  msg_printf(VRB,"Allowed range: %d to %d  measured: %d\n"
		     ,sa_lo,sa_hi,amplitude[chn]);
	}
      }
      if (err_last-err) {
	msg_printf(VRB,"Gain:(x1)%d Atten:15 MDAC:128 Frequency:%d\n",
		   g,f);
	if (pause_always | pause_on_error) kpause();
	if (dump_mode) {
	  msg_printf(DBG,"BUFFER START\n");
	  printbuf(BUF_MARGIN,NWORDS(f)*STEREO/AC_LEN,STEREO,0);
	  msg_printf(DBG,"BUFFER END\n");
	}
      }
      else if (pause_always) kpause();
    }
    for (chn=0; chn<STEREO; chn++) {
      /* compute new ampl base on current reading and 7.5 dB increase */
      ampl[chn]=(sroot(amplitude[chn])*237)/analog_mic_gain[NUMFR-1];
    }
    dc_max*=2376; dc_max/=1000;
    gain2volt*=1000; gain2volt/=2371;
  }

  msg_printf(VRB,"\nsweeping MDAC volume (mic/headphone)\n");
  gval=0;     /* no gain */
  aval=0x01; a=0; /* mic power on, atten 0dB, gain x1 */
  *idr0p=gval; *idr1p=aval;
  *iarp=HAL2_CODECA_CTRL2_W;
  SPIN;
  *idr0p=gval+(3<<8); *idr1p=aval;
  *iarp=HAL2_CODECB_CTRL2_W;
  SPIN;
  gain2volt=3052;
  fi=NUMFR-1;
  for (v=0; v<256; v+=17) {
    *hpl=v; *hpr=v;
    us_delay(10000);
    if (v==136) {
      /* when MDAC passes 1/2 way pt. we have to attenuate to */
      /* avoid clipping */
      aval=(0x4<<7)+(0x4<<2)+0x01;  /* mic power on, atten 6dB, gain x1 */
      *idr0p=gval; *idr1p=aval;
      *iarp=HAL2_CODECA_CTRL2_W;
      SPIN; a=4;
    }
    f=frequencies[fi];
    maketone(f,CODEC_A,STEREO);
    gettone(NWORDS(f)*STEREO/AC_LEN,CODEC_B,STEREO);
#if (defined(IP26) || defined(IP28))
    KillDMA(OUT_CHAN);
#endif
    if (v<129) {
      msg_printf(DBG,"CONDITIONS: MDAC=%d gain=0 atten=0 frequency=%d\n",v,f);
      ampl_f=(256*v*analog_mic_gain[fi])/100;
    }
    else {
      msg_printf(DBG,"CONDITIONS: MDAC=%d gain=0 atten=4 frequency=%d\n",v,f);
      ampl_f=(128*v*analog_mic_gain[fi])/100;
    }
    Compute(f,STEREO);
    if (v<10) {
      sa_hi=MDAC_OFF;
      sa_lo=0;
    }
    else {
      sa_hi=(ampl_f*ampl_f/100)*(100+AMP_BRACKET_PERCENT_H);
      if (debug_mode)
	sa_lo=(ampl_f*ampl_f/100)*(100-AMP_BRACKET_PERCENT_L);
      else
	sa_lo=(ampl_f*ampl_f/100)*(94-AMP_BRACKET_PERCENT_L);
    }
    err_last=err;
    for (chn=0; chn<2; chn++) {
      if (ABS(mean[chn])>DC_MAX) {
	err--;
	msg_printf(ERR,"Test #5:High DC offset on channel %d%s: %duV\n"
		   ,chn,st_chan[chn],mean_uv[chn]);
	msg_printf(VRB,"Max allowed reading: %d  measured: %d\n"
		   ,DC_MAX,mean[chn]);
      }
      if (v<10) noise_max=MDAC_OFF_NOISE;
      else noise_max=amplitude[chn]/noise_mdac[v/17];
      if (noise_max<MDAC_OFF_NOISE) noise_max=MDAC_OFF_NOISE;
      if (variance[chn]<0 || variance[chn]>noise_max) {
	err--;
	msg_printf(ERR,"Test #5:High noise on channel %d%s: %duV(rms)\n"
		   ,chn,st_chan[chn],var_uv[chn]);
	msg_printf(VRB,"Max allowed reading: %d  measured: %d\n"
		   ,noise_max,variance[chn]);
      }
      if (ReportLimits) ShowLimits (amplitude [chn], sa_lo, sa_hi);
      if (amplitude[chn]<sa_lo || amplitude[chn]>sa_hi) {
	err--;
	msg_printf(ERR,"Test #5:Gain error on channel %d%s: %duV(rms)\n"
		   ,chn,st_chan[chn],amp_uv[chn]);
	msg_printf(VRB,"Allowed range: %d to %d  measured: %d\n"
		   ,sa_lo,sa_hi,amplitude[chn]);
      }
    }
    if (err_last-err) {
      msg_printf(VRB,"Gain:(x1)0 Atten:%d MDAC:%d Frequency:%d\n",
		 a,v,f);
      if (pause_always | pause_on_error) kpause();
    }
    else if (pause_always) kpause();
    fi=(fi+1)%NUMFR;
  }

  msg_printf(VRB,"\ntesting x10 gain (mic/headphone)\n");
  gval=0;     /* no gain */
  aval=0x00;  /* mic power on, atten 0dB, gain x10 */
  *idr0p=gval; *idr1p=aval;
  *iarp=HAL2_CODECA_CTRL2_W;
  SPIN;
  *idr0p=gval+(3<<8); *idr1p=aval;
  *iarp=HAL2_CODECB_CTRL2_W;
  SPIN;
  *hpl=13; *hpr=13;  /* about 1/10 of nominal vol. because of x10 gain */
  us_delay(10000);
  for (fi=0; fi<NUMFR; fi++) {
    f=frequencies[fi];
    maketone(f,CODEC_A,STEREO);
    gettone(NWORDS(f)*STEREO/AC_LEN,CODEC_B,STEREO);
#if (defined(IP26) || defined(IP28))
    KillDMA(OUT_CHAN);
#endif
    msg_printf(DBG,
	       "CONDITIONS: MDAC=13 gain=0(x10) atten=0 frequency=%d\n",f);
    Compute(f,STEREO);
    ampl_f=tenXgain*analog_mic_gain[fi];
    sa_hi=(ampl_f*ampl_f/100)*(103+AMP_BRACKET_PERCENT_H);
    sa_lo=(ampl_f*ampl_f/100)*(47-AMP_BRACKET_PERCENT_L);
    err_last=err;
    for (chn=0; chn<2; chn++) {
      if (ABS(mean[chn])>DC_MAX) {
        err--;
        msg_printf(ERR,"Test #5:High DC offset on channel %d%s: %duV\n"
		   ,chn,st_chan[chn],mean_uv[chn]);
	msg_printf(VRB,"Max allowed reading: %d  measured: %d\n"
		   ,DC_MAX,mean[chn]);
      }
      noise_max=amplitude[chn]/noise_gs[fi];
      if (noise_max<MDAC_OFF_NOISE) noise_max=MDAC_OFF_NOISE;
      if (variance[chn]<0 || variance[chn]>noise_max) {
        err--;
        msg_printf(ERR,"Test #5:High noise on channel %d%s: %duV(rms)\n"
		   ,chn,st_chan[chn],var_uv[chn]);
	msg_printf(VRB,"Max allowed reading: %d  measured: %d\n"
		   ,noise_max,variance[chn]);
      }
      if (ReportLimits) ShowLimits (amplitude [chn], sa_lo, sa_hi);
      if (amplitude[chn]<sa_lo || amplitude[chn]>sa_hi) {
        err--;
        msg_printf(ERR,"Test #5:Gain error on channel %d%s: %duV(rms)\n"
		   ,chn,st_chan[chn],amp_uv[chn]);
	msg_printf(VRB,"Allowed range: %d to %d  measured: %d\n"
		   ,sa_lo,sa_hi,amplitude[chn]);
      }
    }
    if (err_last-err) {
      msg_printf(VRB,"Gain:(x10)0 Atten:0 MDAC:13 Frequency:%d\n",f);
      if (pause_always | pause_on_error) kpause();
    }
    else if (pause_always) kpause();
  }

  KillDMA(OUT_CHAN);
  return(err);
}

/* This tests the CODECs with a sampling rate of 8kHz instead of 48kHz */
/* this test is a single frequency sweep in quad mode, no gain, no atten. */
int
Test_8k(void)
{
  volatile __uint32_t *idr0p=(__uint32_t *)PHYS_TO_K1(HAL2_IDR0);
  volatile __uint32_t *idr1p=(__uint32_t *)PHYS_TO_K1(HAL2_IDR1);
  volatile __uint32_t *iarp=(__uint32_t *)PHYS_TO_K1(HAL2_IAR);
  volatile __uint32_t *isrp=(__uint32_t *)PHYS_TO_K1(HAL2_ISR);
  int f,fi,chn,err,err_last;
  int ampl_f,sa_hi,sa_lo,noise_max;

  msg_printf(VRB,"8kHz test entered...\n");
  err=0;
  *isrp = HAL2_ISR_CODEC_RESET_N|HAL2_ISR_GLOBAL_RESET_N
    |HAL2_ISR_CODEC_MODE;
  us_delay(50);
  *idr0p=0;
  *iarp=HAL2_BRES1_CTRL1_W;  /* select 48kHz master */
  SPIN;
  *idr0p=4;
  *idr1p=0xffff & (4-24-1);
  *iarp=HAL2_BRES1_CTRL2_W;  /* set rate to 8kHz */
  SPIN;
  *idr0p=0; *idr1p=0x03;
  *iarp=HAL2_CODECB_CTRL2_W; /* no gain, no atten CODEC B */
  SPIN;
  *idr0p=(3<<8); *idr1p=0x03;
  *iarp=HAL2_CODECA_CTRL2_W; /* no gain, no atten CODEC A */
  SPIN;
  us_delay(75000);
  gain2volt=3052;
  for (fi=0; fi<(NUMFR/2); fi++) {
    f=frequencies[fi]*6;
    maketone(f,CODEC_A,QUAD);
    if (fi == 0) us_delay(is_fullhouse() ? Delay_setup+850000 : Delay_setup);		/* first time only */
    gettone(NWORDS(f)*QUAD/AC_LEN,CODEC_B,QUAD);
#if (defined(IP26) || defined(IP28))
    KillDMA(OUT_CHAN);
#endif
    msg_printf(DBG,"CONDITIONS: gain=0 atten=0 frequency=%d\n",f);
    Compute(f,QUAD);
    ampl_f=(32768*analog_line_gain[fi])/100;
    sa_lo=(ampl_f*ampl_f/100)*(100-AMP_BRACKET_PERCENT_L);
    sa_hi=(ampl_f*ampl_f/100)*(100+AMP_BRACKET_PERCENT_H);
    err_last=err;
    for (chn=0; chn<4; chn++) {
      if (ABS(mean[chn])>DC_MAX) {
        err--;
        msg_printf(ERR,"Test #6:High DC offset on channel %d%s: %duV\n"
		   ,chn,qu_chan[chn],mean_uv[chn]);
	msg_printf(VRB,"Max allowed reading: %d  measured: %d\n"
		   ,DC_MAX,mean[chn]);
      }
      noise_max=amplitude[chn]/noise_8k;
      NOISE_MAX_LIMIT;
      if (variance[chn]<0 || variance[chn]>noise_max) {
        err--;
        msg_printf(ERR,"Test #6:High noise on channel %d%s: %duV(rms)\n"
		   ,chn,qu_chan[chn],var_uv[chn]);
	msg_printf(VRB,"Max allowed reading: %d  measured: %d\n"
		   ,noise_max,variance[chn]);
      }
      if (ReportLimits) ShowLimits (amplitude [chn], sa_lo, sa_hi);
      if (amplitude[chn]<sa_lo || amplitude[chn]>sa_hi) {
        err--;
        msg_printf(ERR,"Test #6:Gain error on channel %d%s: %duV(rms)\n"
		   ,chn,qu_chan[chn],amp_uv[chn]);
	msg_printf(VRB,"Allowed range: %d to %d  measured: %d\n"
		   ,sa_lo,sa_hi,amplitude[chn]);
      }
    }
    if (err_last-err) {
      msg_printf(VRB,"Sample Rate:8kHz  Frequency:%d\n",f/6);
      if (pause_always | pause_on_error) kpause();
    }
    else if (pause_always) kpause();
  }
  KillDMA(OUT_CHAN);
  return(err);
}

/* This tests the use of the AESRX clock as a master for a bres. clock */
/* by giving AESTX a clock of 48kHz and using the RX clock for a 1ktone */
/* at the analog loop.  As in the clock test, the input frequency is */
/* measured by counting zero crossings and time stamping.  */
int
Test_Rx_Clock(void)
{
  volatile __uint32_t *idr0p=(__uint32_t *)PHYS_TO_K1(HAL2_IDR0);
  volatile __uint32_t *idr1p=(__uint32_t *)PHYS_TO_K1(HAL2_IDR1);
  volatile __uint32_t *iarp=(__uint32_t *)PHYS_TO_K1(HAL2_IAR);
  volatile __uint32_t *isrp=(__uint32_t *)PHYS_TO_K1(HAL2_ISR);
  volatile __uint32_t *aesrx_sr2=(__uint32_t *)PHYS_TO_K1(AESRX_SR2);
  int status;

  msg_printf(VRB,"AESRX clock test entered...\n");
  SetupDMAChannel(OUT_CHAN+2,AES_TX,STEREO);
  *idr0p=0x0; /* select 48kHz master for bres. clock #2 */
  *iarp=HAL2_BRES2_CTRL1_W;
  SPIN;
  *idr0p=4; *idr1p=0xffff&(4-4-1);
  *iarp=HAL2_BRES2_CTRL2_W;
  SPIN;
  *idr0p=0x213; /* setup AESTX w. bres. clock #2 */
  *iarp=HAL2_AESTX_CTRL_W;
  SPIN;
  *idr0p=0x2; /* select AES master for bres. clock #1 */
  *iarp=HAL2_BRES1_CTRL1_W;
  SPIN;
   /* make sure something comes out of the aes tx chip */
  BuildDescriptors(aux_output_desc,sintab,1000,1);
  InitiateDMA(aux_output_desc,0,OUT_CHAN+2);
  us_delay(50000);
  status=*aesrx_sr2;
  us_delay(5000);
  status=(*aesrx_sr2)&0xff;
  msg_printf(DBG,"AES status: %x\n",status);
  maketone(1000,CODEC_A,STEREO);
  SetupDMAChannel(IN_CHAN,CODEC_B,STEREO);
  *idr0p=0x28+IN_CHAN; /* enable timestamping */
  *iarp=HAL2_CODECB_CTRL1_W;
  SPIN;
  BuildDescriptors(input_desc,buf,MAXSAMPS/2,0);
  InitiateDMA(input_desc,1,IN_CHAN);
  if (WaitForOneDMA(IN_CHAN)) {
    msg_printf(ERR,"DMA failure: exiting AESrx clock test.\n");
    if (pause_on_error || pause_always) kpause();
    return(-1);
  }
  KillDMA(OUT_CHAN);
  KillDMA(OUT_CHAN+2);

  if (CheckToneFreq()) {
    msg_printf(ERR,"AESrx master clock - test failed.\n\n");
    if (pause_on_error || pause_always) kpause();
    return(-1);
  }
  if (pause_always) kpause();
  return(OK);
}

/* Make a 48000 element table containing one cycle of 32767*sin(x) */
static void
MakeSinTable(void)
{
    unsigned int theta,t2,t3,t4,t5,t6,t7;
    int *p1,*p2;
    int i;

    /* Compute the first quadrant */
    p1=sintab; p2=p1+12000;
    for(i=0;i<=6000;i++) {
        theta=i*51472;            /* theta=i*.25*pi*65536 */
        theta/=6000;              /* theta=i*.25*pi*65536/6000 */
        t2=(theta*theta)>>16;
        t3=(t2*theta)>>16;
        t4=(t2*t2)>>16;
        t5=(t2*t3)>>16;
        t6=(t3*t3)>>16;
        t7=(t3*t4)>>16;
        /*Use a taylor series for sin x to compute values in the first octant*/
        *p1++=(5040*theta-840*t3+42*t5-t7)/5040;
        /*Use a taylor series for cos x to compute values in the second octant*/
        *p2--= 65535-(t2>>1) + ( 30*t4 -t6)/720;
    }

    /* Scale and limit */
    p1=sintab; p2=p1+12000;
    while(p1<=p2) {
        t7 = (*p1)*4095;
	t7 /= 8192;
        if(t7>=32767)
            t7=32767;
        *p1=t7;
        p1++;
    }

    /* Reflect the first quadrant into the second quadrant */
    p1=sintab; p2=p1+24000;
    while(p1<=p2) {
        *p2 = *p1;
        p1++;
        p2--;
    }

    /* Reflect the first two quadrants int the last two quadrants */
    p1=sintab+1; p2=sintab+47999;
    while(p1<=p2) {
        *p2 = -(*p1);
        p1++;
        p2--;
    }
}

/* generates a tone w. frequency f in quad/stereo mode, on DMA channel 0 */
void
maketone(int f, int dev, int mode)
{
  int chan,i=0,j=0;

  /* pick # of words that is an integer # of cycles of wave */
  int nwords=NWORDS(f);

  /* generate a sine wave at frequecy f, w. number of channels    */
  /* defined by mode.  Channel phases are offset by 90 degs. so   */
  /* that left/right and back/front correspondence may be checked */
  /* right rear is perfectly in phase with left front so that     */
  /* errors such as data shifted by 1 sample in quad mode still   */
  /* show up. */
  while (j<nwords) {
    for (chan=0; chan<mode; chan++)
      tunetab[j++]=sintab[(i+(chan%(chan3_phase_sel))
			   *NINTY_DEGREES)%MAXSAMPS]<<8;
/*
  CORRECT CODE:
      tunetab[j++]=sintab[(i+(chan%3)*NINTY_DEGREES)%MAXSAMPS]<<8;
  TEST CASE ONLY:
      tunetab[j++]=sintab[(i)%MAXSAMPS]<<8;
*/
    i+=f; if (i+f>MAXSAMPS) i=0;
  }

  SetupDMAChannel(OUT_CHAN,dev,mode);
  BuildDescriptors(output_desc,tunetab,nwords,1);
  InitiateDMA(output_desc,0,OUT_CHAN);
  us_delay(Delay_maketone);
}

/* Grab a buffer of samples from input for computation */
static void
gettone(int nwords,int dev,int mode)
{
  SetupDMAChannel(IN_CHAN,dev,mode);
  BuildDescriptors(input_desc,buf,nwords+BUF_MARGIN*2,0);
  InitiateDMA(input_desc,1,IN_CHAN);
  (void)WaitForOneDMA(IN_CHAN);
}

/* print contents of buf[] from buf[start] to buf[start+nwords] */
static
printbuf(int start,int nwords,int mode,int timestamp)
{
  int i,step;

  if (timestamp) step=mode+2;
  else step=mode;

  for (i=start; i<nwords+start; i+=step) {
    if (mode==QUAD)
      printf("%d  %d  %d  %d  ",buf[i],buf[i+1],buf[i+2],buf[i+3]);
    else
      printf("%d  %d  ",buf[i],buf[i+1]);
    if (timestamp)
      printf("sec:%d ms:%d\n",buf[i+mode],buf[i+1+mode]);
    else
      printf("\n");
  }

	return 0;
}

/* create a list of DMA descriptors given # of words and start addr. */
static void
BuildDescriptors(scdescr_t *list, int *startaddr, int nwords, int loop)
/* loop=1 link last desc. to first for infinite repeat */
/* loop=0 do not loop descriptor chain. */
{
      unsigned int nremaining, bytes_till_page_boundary, bytes_this_descriptor;
      char *p;
      scdescr_t *desc;

      desc=list;
      nremaining= (unsigned int)nwords*(int)sizeof(int);
      p=(char *)startaddr;
      while(nremaining) {
	  desc->cbp = KDM_TO_MCPHYS(p);

	  bytes_till_page_boundary = (int)(IO_NBPP
					- ((__psunsigned_t)p & IO_POFFMASK));
	  bytes_this_descriptor = MIN(bytes_till_page_boundary,nremaining);
	  desc->bcnt = (unsigned int) bytes_this_descriptor;

	  desc->eox=0;
	  desc->pad1=0;
	  desc->xie=0;
	  desc->pad0=0;

	  desc->nbp = KDM_TO_MCPHYS(desc+1);
	  desc->word_pad = 0;
	  nremaining -= bytes_this_descriptor;
	  p+= bytes_this_descriptor;
	  desc++;
      }
      desc--;
      if (loop) {
	desc->nbp = KDM_TO_MCPHYS(list);
	desc->eox = 0;
      }
      else {
	desc->nbp = 0;
	desc->eox = 1;
      }
}

  /*
   * InitiateDMA - initiates a DMA with the descriptor
   * 		 chain starting at desc_list.
   */

static char hal2_hpc3fifo_start[8] = {0, 1, 9, 21, 29, 0, 0, 0};
static char hal2_hpc3fifo_end[8] = {0, 8, 20, 28, 32, 0, 0, 0};
static char hal2_hpc3fifo_highwater[8] = {0, 16, 24, 16, 8, 0, 0, 0};

/*direction 0=frommips, 1=tomips*/
static void
InitiateDMA(scdescr_t *desc_list, int direction, int channel)
{
    pbus_control_write_t ctrlval;
    volatile __uint32_t *ctrlptr=
	(unsigned int *)PHYS_TO_K1(HPC3_PBUS_CONTROL(channel));
    volatile __uint32_t *dpptr=
	(__uint32_t *)PHYS_TO_K1(HPC3_PBUS_DP(channel));

    ctrlval.pad0=0;
    ctrlval.fifo_end=hal2_hpc3fifo_end[channel];
    ctrlval.pad1=0;
    ctrlval.fifo_beg=hal2_hpc3fifo_start[channel];
    ctrlval.highwater=hal2_hpc3fifo_highwater[channel];
    ctrlval.pad2=0;
    ctrlval.real_time=1;
    ctrlval.ch_act_ld=1;
    ctrlval.ch_act=1;
    ctrlval.flush=0;
    ctrlval.receive=direction;
    ctrlval.little=0;
    ctrlval.pad3=0;

    *dpptr=KDM_TO_MCPHYS(desc_list);
#ifdef R10000
    flushbus();
#endif
    *ctrlptr=*(__uint32_t *)&ctrlval;	/* T5 did not like the bitfield store */
    flushbus();
}

/* stop an active DMA channel- used to stop a DMA channel whose */
/* descriptor list is looped for infinite repeat.  */
static void
KillDMA(int channel)
{
    pbus_control_write_t ctrlval;
    volatile pbus_control_write_t* ctrlptr=
	(pbus_control_write_t*)PHYS_TO_K1(HPC3_PBUS_CONTROL(channel));
    volatile __uint32_t *idr0p=(__uint32_t *)PHYS_TO_K1(HAL2_IDR0);
    volatile __uint32_t *iarp=(__uint32_t *)PHYS_TO_K1(HAL2_IAR);
    volatile __uint32_t *isrp=(__uint32_t *)PHYS_TO_K1(HAL2_ISR);

    /* Clear DMA drive and enable */
    *idr0p = 0;
    *iarp = HAL2_DMA_ENABLE_W;
    us_delay(50);
    SPIN;
    *idr0p =0;
    *iarp = HAL2_DMA_DRIVE_W;
    us_delay(50);
    SPIN;

    /* clear channel active bit in HPC3 */
    ctrlval.pad0=0;
    ctrlval.fifo_end=hal2_hpc3fifo_end[channel];
    ctrlval.pad1=0;
    ctrlval.fifo_beg=hal2_hpc3fifo_start[channel];
    ctrlval.highwater=hal2_hpc3fifo_highwater[channel];
    ctrlval.pad2=0;
    ctrlval.real_time=1;
    ctrlval.ch_act_ld=1;
    ctrlval.ch_act=0;
    ctrlval.flush=0;
    ctrlval.receive=0;
    ctrlval.little=0;
    ctrlval.pad3=0;

    *ctrlptr=ctrlval;
    flushbus();
}

/* this establishes a known state for HAL2. */
/*  - HAL2 out of reset state */
/*  - all DMA channels & devices disabled */
/*  - all DMA ports big endian */
/*  - CODECS unmuted, no gain, no attenuation */
/*  - bres. clock #1 set to 48kHz */
static void
SetupHAL2(void)
{
    volatile __uint32_t *idr0p=(__uint32_t *)PHYS_TO_K1(HAL2_IDR0);
    volatile __uint32_t *idr1p=(__uint32_t *)PHYS_TO_K1(HAL2_IDR1);
    volatile __uint32_t *iarp=(__uint32_t *)PHYS_TO_K1(HAL2_IAR);
    volatile __uint32_t *isrp=(__uint32_t *)PHYS_TO_K1(HAL2_ISR);
    int inc, mod, modctrl;

    /* Set up the ISR */
    *isrp = 0;
    us_delay(990);
    *isrp = HAL2_ISR_CODEC_RESET_N|HAL2_ISR_GLOBAL_RESET_N;
    us_delay(70);

    /* Set up DMA Endian */
    *idr0p = 0;  /* All ports are big endian */
    *iarp = HAL2_DMA_ENDIAN_W;
    SPIN;

    /* Clear DMA drive and enable */
    *idr0p = 0;
    *iarp = HAL2_DMA_DRIVE_W;
    SPIN;
    *idr0p =0;
    *iarp = HAL2_DMA_ENABLE_W;
    SPIN;

    /* unmute codecs, no gain, no attenuation */
    *idr0p = 0; *idr1p = 0x03;
    *iarp = HAL2_CODECA_CTRL2_W;
    SPIN;
    *idr0p = 0; *idr1p = 0;
    *iarp = HAL2_CODECB_CTRL2_W;
    SPIN;

    /* Set up BRES CLOCK 1 */
    *idr0p = 0;		/* 48khz master */
    *iarp = HAL2_BRES1_CTRL1_W;
    SPIN;
    inc=4;
    mod=4;
    modctrl = 0xffff & (inc-mod-1);
    *idr0p = inc;
    *idr1p = modctrl;
    *iarp = HAL2_BRES1_CTRL2_W;
    SPIN;
}

/* reset and initialize AES chips */
static int
AES_init(void)
{
  __uint32_t *aesrx_cr1 = (__uint32_t *) PHYS_TO_K1(AESRX_CR1);
  __uint32_t *aesrx_cr2 = (__uint32_t *) PHYS_TO_K1(AESRX_CR2);
  __uint32_t *aestx_cr1 = (__uint32_t *) PHYS_TO_K1(AESTX_CR1);
  __uint32_t *aestx_cr2 = (__uint32_t *) PHYS_TO_K1(AESTX_CR2);
  __uint32_t *aestx_cr3 = (__uint32_t *) PHYS_TO_K1(AESTX_CR3);

  *aesrx_cr1=0x0; /* reset rx chip */
  *aestx_cr2=0x0; /* reset tx chip */
  us_delay(100000);
  *aesrx_cr1=0x1;
  *aestx_cr2=0x87;
  us_delay(60);
  *aesrx_cr2=0xab;
  *aestx_cr1=0x0;
  us_delay(60);
  *aestx_cr3=0x9;
  us_delay(60);

  return 0;
}

/* sets DMA drive and enable bits in HAL2 for the appropriate chan. */
/* and device (codec/AES chip) and sets up device w. appropriate */
/* mode (STEREO/QUAD) bres. clock, and DMA chan. */
static void
SetupDMAChannel(int chan, int dev, int mode)
{
    volatile __uint32_t *idr0p=(__uint32_t *)PHYS_TO_K1(HAL2_IDR0);
    volatile __uint32_t *iarp=(__uint32_t *)PHYS_TO_K1(HAL2_IAR);
    volatile __uint32_t *isrp=(__uint32_t *)PHYS_TO_K1(HAL2_ISR);
    __uint32_t tmp,tmp2;
    int mode_bits=0x2;
    if (mode==QUAD) mode_bits=0x3;

    /* Set up DMA DRIVE */
    *iarp = HAL2_DMA_DRIVE_R;
    SPIN;
    tmp = *idr0p;
    *idr0p = (0x1<<chan)|tmp;  /* Activate channel */
    *iarp = HAL2_DMA_DRIVE_W;
    SPIN;

    tmp = chan	/* Physical DMA channel */
	    +(0x1<<3)	/* Bres Clock ID */
	    +(mode_bits<<8);
    if (dev==AES_RX) 
      tmp = chan & 0x0007;
    if (dev==CODEC_B)
      tmp &= 0x1f;
    *idr0p=tmp;
    if ((mode==QUAD)&&(dev==CODEC_A)) {
      *iarp = HAL2_CODECA_CTRL1_W;
      tmp = ((chan+1)& 0x0007) + (0x1<<3);
      SPIN;
      *idr0p = tmp;
      *iarp = HAL2_CODECB_CTRL1_W;
    }
    else {
      switch (dev) {
      case CODEC_A:
        *iarp = HAL2_CODECA_CTRL1_W;
	break;
      case CODEC_B:
	*iarp = HAL2_CODECB_CTRL1_W;
	break;
      case AES_TX:
	*iarp = HAL2_AESTX_CTRL_W;
	break;
      case AES_RX:
	*iarp = HAL2_AESRX_CTRL_W;
	break;
      }
    }
    SPIN;

    /* Set up DMA ENABLE */
    *iarp = HAL2_DMA_ENABLE_R;
    SPIN;
    tmp=*idr0p;
    if (mode==QUAD) tmp2=0x18;
    else {
      switch (dev) {
      case AES_RX:  tmp2=0x02;break;
      case AES_TX:  tmp2=0x04;break;
      case CODEC_A: tmp2=0x08;break;
      case CODEC_B: tmp2=0x10;break;
      default: tmp2=0;
      }
    }
    *idr0p = tmp|tmp2;
    *iarp = HAL2_DMA_ENABLE_W;
    SPIN;
    if (tmp!=(tmp|tmp2)) {
      tmp = HAL2_ISR_GLOBAL_RESET_N;
      if (mode==QUAD) tmp|=HAL2_ISR_CODEC_MODE;
      *isrp = tmp;
      us_delay(5000);
      *isrp = HAL2_ISR_CODEC_RESET_N|tmp;
      us_delay(400000);
    }
}

/* waits for the DMA on selected channel to complete */
static int
WaitForOneDMA(int which)
{
    volatile pbus_control_read_t* ctrl0ptr=
	(pbus_control_read_t*)PHYS_TO_K1(HPC3_PBUS_CONTROL(which));
    pbus_control_read_t ctrl0val;
    int ntimes;

    ctrl0val = *ctrl0ptr;
    if (!ctrl0val.ch_act) {
      msg_printf(ERR,"DMA %d not active\n",which);
      return(-1);
    }
    for(ntimes=0;ntimes<DMA_WAIT_T;ntimes++) {
	ctrl0val = *ctrl0ptr;
	if (!ctrl0val.ch_act)break;
	us_delay(10000);
    }
    if(ntimes==DMA_WAIT_T) {
	msg_printf(ERR,"DMA %d did not complete after %d0 ms\n"\
		   ,which,ntimes);
	return(-1);
    } else {
	msg_printf(DBG,"DMA %d complete after %d0 ms\n",which,ntimes);
	return(OK);
      }
}

/**** calculate mean, amplitude, and noise for each channel *****/
/* AMPLITUDE: calculated by averaging the sums of squares of    */
/*            "sin" and "cos" samples, where "sin" is sample x  */
/*            "cos" is the sample at x + ninty degrees.         */
/* VARIANCE:  calculated by averaging the squares of sums of    */
/*            "sin" and "-sin" samples, where "sin" is sample   */
/*            x and "-sin" is the sample at x + one hundred and */
/*            eighty degrees.                                   */
static void
Compute(int f,int mode)
{
  int samps_per_phase,chan,i,nwords;
  int samp1,samp2,samp3,tmp,tmp2;
  int factor_a[4],factor_v[4];
  int *bufp;

  bufp=&buf[BUF_MARGIN];
  samps_per_phase=MAXSAMPS/f;
  nwords=NWORDS(f)*mode/AC_LEN;

  /* clear result accumulators */
  for (chan=0; chan<mode; chan++)
    {
      mean[chan]=0;
      amplitude[chan]=0;
      variance[chan]=0;
      factor_a[chan]=1;
      factor_v[chan]=1;
    }

  /* compute mean for all channels */
  for (i=0; i<nwords; i+=mode)
    for (chan=0; chan<mode; chan++)
      mean[chan]+=bufp[i+chan]>>8;
  for (chan=0; chan<mode; chan++)
    mean[chan]/=(nwords/mode);

  /* compute amplitude and variance for all chans */
  tmp=mode*(samps_per_phase/4);
  if (noise_compute_mode == COMPUTE_THD_N)
    tmp2=mode*(samps_per_phase/2);
  else tmp2=mode*(samps_per_phase);
  for (i=0; i<nwords-(samps_per_phase*mode); i+=mode)
    for (chan=0; chan<mode; chan++) {
      samp1=(bufp[i+chan]>>8)-mean[chan];
      samp2=(bufp[i+tmp+chan]>>8)-mean[chan];
      samp3=(bufp[i+tmp2+chan]>>8)-mean[chan];
      amplitude[chan]+=((samp1*samp1) + (samp2*samp2))/factor_a[chan];
      if (noise_compute_mode == COMPUTE_THD_N)
	variance[chan]+=((samp1+samp3) * (samp1+samp3))/factor_v[chan];
      else
	variance[chan]+=((samp1-samp3) * (samp1-samp3))/factor_v[chan];
      if (amplitude[chan]>TOO_BIG) {
	/* if numbers are too large, divide by 2 to compensate */
	amplitude[chan]/=2;
	factor_a[chan]*=2;
      }
      if (variance[chan]>TOO_BIG) {
	/* if numbers are too large, divide by 2 to compensate */
	variance[chan]/=2;
	factor_v[chan]*=2;
      }
    }
  tmp=(nwords/mode)-samps_per_phase;
  for (chan=0; chan<mode; chan++) {
    amplitude[chan]/=tmp;
    amplitude[chan]*=factor_a[chan];
    variance[chan]/=tmp;
    variance[chan]*=factor_v[chan];
    snr[chan]=amplitude[chan]/variance[chan];
    mean_uv[chan]=mean[chan]*gain2volt;
    mean_uv[chan]/=36;
    var_uv[chan]=sroot(variance[chan])*gain2volt;
    var_uv[chan]/=100;
    amp_uv[chan]=sroot(amplitude[chan])*gain2volt;
    amp_uv[chan]/=100;
/*
This part added for more info in lab debugging
    mean_uv2[chan]=(mean[chan]*3052)/100;
    mean_uv2[chan]=mean[chan];
    var_uv2[chan]=sroot(variance[chan])*3052;
    var_uv2[chan]/=100;
    amp_uv2[chan]=sroot(amplitude[chan])*3052;
    amp_uv2[chan]/=100;

end of add
*/
    if (test_mode)
      msg_printf(DBG,"snr:%d\n",snr[chan]);
    else {
      msg_printf(DBG,"CHAN%d%s: mean:%duV   noise:%duV(rms)   amp:%duV(rms)\n",
	       chan,CHAN(chan),mean_uv[chan],var_uv[chan],amp_uv[chan]);
/*
This part added for more info in lab debugging

      msg_printf(DBG,"a/d-CHAN%d%s: mean:%duV   noise:%duV(rms)   amp:%duV(rms)\n",
	       chan,CHAN(chan),mean_uv2[chan],var_uv2[chan],amp_uv2[chan]);

end of add
*/
    }
  }
}

/* Check that chan_b is 90 degrees ahead of chan_a.  Used for */
/* verifying left-right and front-back correspondence.        */
/* this will also detect large amounts of crosstalk           */
/* This function assumes Compute() was called previously      */
int
LR_Check(int chan_a, int chan_b, int f, int mode)
{
  int nwords,i,quarter_phase,tmp;
  int acc=0,mean_a=0,mean_b=0;
  int amp_a,amp_b;
  int factor=1;
  int *bufp;

  bufp=&buf[BUF_MARGIN];
  nwords=NWORDS(f)*mode/AC_LEN;
  quarter_phase=(MAXSAMPS/(f*4))*mode;

  for (i=0; i<nwords; i+=mode) {
    mean_a+=bufp[i+chan_a]>>8;
    mean_b+=bufp[i+chan_b]>>8;
  }
  mean_a/=(nwords/mode);
  mean_b/=(nwords/mode);

  amp_a=sroot(amplitude[chan_a])/1000;
  amp_b=sroot(amplitude[chan_b])/1000;

if  ( (amp_a==0) || (amp_b==0) ) {
msg_printf(DBG,"About to do division by zero...exiting LR_Check\n");
msg_printf(DBG,"chan_a=%d\tchan_b=%d\tf=%d\tmode=%d\n",chan_a,chan_b,f,mode);
msg_printf(DBG,"%d\t%d\n",amp_a,amp_b);
return(-1);
}




  for (i=0; i<nwords; i+=mode) {
    tmp=(int)((((bufp[i+chan_a]>>8)-mean_a)/amp_a)
      +(((bufp[i+chan_b+quarter_phase]>>8)-mean_b)/amp_b));
    /* should be zero */
    acc+=(tmp*tmp)/factor;
    if (acc>TOO_BIG) {
      acc/=2;
      factor*=2;
    }
  }
  acc/=(nwords/mode);
  acc*=factor;
  /* acc will be small if b is 90 degs. ahead of a */
  msg_printf(DBG,"channels %d and %d phase test: %d\n",chan_a,chan_b,acc);

  if (acc>LR_TEST_THRESHHOLD) return(-1);
  else return(OK);
}

/* Setup sintab, tunetab and buf pointers in free memory since declaring them
 * as .bss will make ide too big.
 */
static int
setup_audio_memory(void)
{
	MEMORYDESCRIPTOR *m=0;
	__psunsigned_t i;

	/* Find a location with > 1M of free memory and set-up shop.
	 */
	while (m = GetMemoryDescriptor(m)) {
		if ((m->Type == FreeMemory) &&
		    (m->PageCount > 256)) {
			sintab = (int *)PHYS_TO_K1(arcs_ptob(m->BasePage));
			tunetab = sintab + MAXSAMPS;
			buf = tunetab + MAXSAMPS;

			/* DMA descriptors must be 128 bit aligned.
	 		 */
			i=((__psunsigned_t)(buf+MAXSAMPS+BUF_MARGIN*2)+15)&~15;
			output_desc = (scdescr_t *)i;
			input_desc = &output_desc[MAX_DESCRIPTORS+1];
			aux_output_desc = &input_desc[MAX_DESCRIPTORS+1];
			aux_input_desc = &aux_output_desc[MAX_DESCRIPTORS+1];
#if defined(IP26)
			/* ensure no data in DCache (really must be done) */
			flush_cache();
#endif
			return(1);
		}
	}

	msg_printf(SUM,"setup_audio_memory: no free memory for test.\n");
	return(0);
}

/* wait for user to press key */
static void
kpause(void)
{
  msg_printf(SUM,"Hit any key to continue...\n");
  while(!getchar());
}

/* integer square root computation */
static int
sroot(int x2)
{
  int x,dx;

  if (x2<2) return(x2);
  x=x2/100;
  dx=100-x;
  while (ABS(dx)>1) {
    x+=(dx>>1);
    dx=(x2/x)-x;
  }
  return(x);
}
