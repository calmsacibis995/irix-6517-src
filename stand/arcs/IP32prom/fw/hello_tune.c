#ident	"$Revision: 1.1 $"

/*
 * hello_tune.c
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/invent.h>
#include <sys/pbus.h>
#include <arcs/hinv.h>
#include <libsc.h>
#include <libsk.h>
#include <uif.h>
#include <sys/IP32.h> 
#include "sys/ad1843.h"
#include "tiles.h"

/*
 * AD1843 audio codec reset register defaults
 */
ushort_t codec_reset_default[NUM_CODEC_REGS] = {
   0xc001,        /* 0 CODEC_STAT_REV */
   0,             /* 1 CODEC_CH_STAT */
   0,             /* 2 CODEC_ADC_SRC_GATTN */
   0x8888,        /* 3 CODEC_DAC2_MIX */
   0x8888,        /* 4 CODEC_AUX1_MIX */
   0x8888,        /* 5 CODEC_AUX2_MIX */
   0x8888,        /* 6 CODEC_AUX3_MIX */
   0x8888,        /* 7 CODEC_MIC_MIX */
   0x8860,        /* 8 CODEC_MONOIN_MIX_MISC */
/* POWBUG: set gain to -6dB */
   0x8888,        /* 9  CODEC_DAC1_ANALOG_GATTN */
   0x8888,        /* 10 CODEC_DAC2_ANALOG_GATTN */
   0,             /* 11 CODEC_DAC1_DIGITAL_ATTN */
   0,             /* 12 CODEC_DAC2_DIGITAL_ATTN */
   0x8080,        /* 13 CODEC_ADC_DAC1_MIX */
   0x8080,        /* 14 CODEC_ADC_DAC2_MIX */
   0,             /* 15 CODEC_CLK_SRC_SELECT */
   0x00ff,        /* 16 CODEC_CLK_GEN1_MODE */
   0xbb80,        /* 17 CODEC_CLK_GEN1_RATE */
   0,             /* 18 CODEC_CLK_GEN1_PHASE */
   0x00ff,        /* 19 CODEC_CLK_GEN2_MODE */
   0xbb80,        /* 20 CODEC_CLK_GEN2_RATE */
   0,             /* 21 CODEC_CLK_GEN1_PHASE */
   0x00ff,        /* 22 CODEC_CLK_GEN3_MODE */
   0xbb80,        /* 23 CODEC_CLK_GEN3_RATE */
   0,             /* 24 CODEC_CLK_GEN3_PHASE */
   0,             /* 25 CODEC_FILTER_MODE */
   0,             /* 26 CODEC_SERIAL */
   0x00c0,        /* 27 CODEC_CH_POWERDOWN */
   0xc400,        /* 28 CODEC_CONFIG */
   0,             /* 29 reserved */
   0,             /* 30 reserved */
   0,             /* 31 reserved */
 };

static COMPONENT a3tmpl = {
	ControllerClass,		/* Class */
	AudioController,		/* Type */
	Input|Output,			/* Flags */
	SGI_ARCS_VERS,			/* Version */
	SGI_ARCS_REV,			/* Revision */
	0,				/* Key */
	0x01,				/* Affinity */
	0,				/* ConfigurationDataSize */
	3,				/* IdentifierLength */
	"A3"				/* Identifier */
};

/*
 * parameter structure for the synthesis algorithm (currently the
 * Karplus-Strong plucked string algo.)
 */
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

static void pluck(pluckparams_t *params, int *soundbuf, float *delaybuf);
static void maketune(pluckparams_t *list, int npluck, int *buf, int maxperiod);
static void randfill(float *buf, int n, float amp);

/* Tune data
 *
 * NOTE: max xxx_buflen == 512000 (2MB) for IP26 memory map
 */

/******* s4_1 parameter list *******/

#define s4_1_buflen      125731
#define s4_1_maxperiod   55

pluckparams_t s4_1_plist[] = {
  {44, 88200, 0, 14079.570312, 1.000000, 0.378496, 0.621504, 0.577689},
  {55, 88200, 9382, 12287.625000, 0.998624, 0.500000, 0.500000, 0.142429},
  {44, 88200, 18765, 13055.601562, 1.000000, 0.378496, 0.621504, 0.577689},
  {41, 88200, 28148, 11007.664062, 1.000000, 0.271694, 0.728306, 0.070275},
  {37, 88200, 37531, 12543.617188, 1.000000, 0.168371, 0.831629, 0.455139},
};

/******* s5_1 parameter list *******/

#define s5_1_buflen      139083
#define s5_1_maxperiod   83

pluckparams_t s5_1_plist[] = {
  {83, 88200, 0, 11775.640625, 0.996301, 0.500000, 0.500000, 0.123125},
  {66, 88200, 10176, 9215.718750, 0.997613, 0.500000, 0.500000, 0.435053},
  {55, 88200, 20353, 8191.750000, 0.998624, 0.500000, 0.500000, 0.142429},
  {33, 88200, 30530, 12031.632812, 1.000000, 0.111528, 0.888472, 0.497777},
  {41, 88200, 40706, 11263.656250, 1.000000, 0.271694, 0.728306, 0.070275},
  {33, 88200, 50883, 5631.828125, 1.000000, 0.111528, 0.888472, 0.497777},
};

/******* s6_1 parameter list *******/

#define s6_1_buflen      125999
#define s6_1_maxperiod   83

pluckparams_t s6_1_plist[] = {
  {83, 88200, 0, 7679.765625, 0.996301, 0.500000, 0.500000, 0.123125},
  {66, 88200, 9449, 11007.664062, 0.997613, 0.500000, 0.500000, 0.435053},
  {55, 88200, 18899, 9983.695312, 0.998624, 0.500000, 0.500000, 0.142429},
  {33, 88200, 28349, 10239.687500, 1.000000, 0.111528, 0.888472, 0.497777},
  {41, 88200, 37799, 10239.687500, 1.000000, 0.271694, 0.728306, 0.070275},
};


/******* s6_3 parameter list *******/

#define s6_3_buflen      125999
#define s6_3_maxperiod   83

pluckparams_t s6_3_plist[] = {
  {83, 88200, 0, 14591.554688, 0.996301, 0.500000, 0.500000, 0.123125},
  {66, 88200, 9449, 8191.750000, 0.997613, 0.500000, 0.500000, 0.435053},
  {55, 88200, 18899, 13311.593750, 0.998624, 0.500000, 0.500000, 0.142429},
  {33, 88200, 28349, 9215.718750, 1.000000, 0.111528, 0.888472, 0.497777},
  {41, 88200, 37799, 11775.640625, 1.000000, 0.271694, 0.728306, 0.070275},
};

/******* t1_1 parameter list *******/

#define t1_1_buflen      126787
#define t1_1_maxperiod   99

pluckparams_t t1_1_plist[] = {
  {99, 88200, 0, 13311.593750, 0.995269, 0.500000, 0.500000, 0.157895},
  {83, 88200, 16537, 14079.570312, 0.996301, 0.500000, 0.500000, 0.123125},
  {28, 88200, 22050, 10751.671875, 1.000000, 0.062973, 0.937027, 0.882610},
  {33, 88200, 38587, 8959.726562, 1.000000, 0.111528, 0.888472, 0.497777},
};


/******* t2_2 parameter list *******/

#define t2_2_buflen      141120
#define t2_2_maxperiod   112

pluckparams_t t2_2_plist[] = {
  {99, 88200, 0, 15871.515625, 0.995269, 0.500000, 0.500000, 0.157895},
  {88, 88200, 13230, 14847.546875, 0.995965, 0.500000, 0.500000, 0.115853},
  {112, 88200, 26460, 15871.515625, 0.994531, 0.500000, 0.500000, 0.997379},
  {99, 88200, 39689, 13567.585938, 0.995269, 0.500000, 0.500000, 0.157895},
  {83, 88200, 52920, 13311.593750, 0.996301, 0.500000, 0.500000, 0.123125},
  {66, 88200, 52920, 11775.640625, 0.997613, 0.500000, 0.500000, 0.435053},
};


/******* t3_2 parameter list *******/

#define t3_2_buflen      135449
#define t3_2_maxperiod   168

pluckparams_t t3_2_plist[] = {
  {168, 88200, 0, 8703.734375, 0.991410, 0.500000, 0.500000, 0.884139},
  {149, 88200, 11812, 13823.578125, 0.992407, 0.500000, 0.500000, 0.196720},
  {28, 88200, 23624, 10751.671875, 1.000000, 0.062973, 0.937027, 0.882610},
  {83, 88200, 35436, 6911.789062, 0.996301, 0.500000, 0.500000, 0.123125},
  {33, 88200, 47249, 7679.765625, 1.000000, 0.111528, 0.888472, 0.497777},
};

/******* t4_1 parameter list *******/

#define t4_1_buflen      124278
#define t4_1_maxperiod   168

pluckparams_t t4_1_plist[] = {
  {168, 88200, 0, 6143.812500, 0.991410, 0.500000, 0.500000, 0.884139},
  {112, 88200, 6013, 4863.851562, 0.994531, 0.500000, 0.500000, 0.997379},
  {133, 88200, 12026, 5631.828125, 0.993313, 0.500000, 0.500000, 0.553575},
  {112, 88200, 18039, 4351.867188, 0.994531, 0.500000, 0.500000, 0.997379},
  {125, 88200, 24052, 9983.695312, 0.993736, 0.500000, 0.500000, 0.124575},
  {112, 88200, 30065, 7423.773438, 0.994531, 0.500000, 0.500000, 0.997379},
  {168, 88200, 36078, 4095.875000, 0.991410, 0.500000, 0.500000, 0.884139},
};

/******* t4_2 parameter list *******/

#define t4_2_buflen      124278
#define t4_2_maxperiod   168

pluckparams_t t4_2_plist[] = {
  {168, 88200, 0, 6143.812500, 0.991410, 0.500000, 0.500000, 0.884139},
  {112, 88200, 6013, 4863.851562, 0.994531, 0.500000, 0.500000, 0.997379},
  {133, 88200, 12026, 5631.828125, 0.993313, 0.500000, 0.500000, 0.553575},
  {112, 88200, 18039, 4351.867188, 0.994531, 0.500000, 0.500000, 0.997379},
  {125, 88200, 24052, 9983.695312, 0.993736, 0.500000, 0.500000, 0.124575},
  {112, 88200, 30065, 7423.773438, 0.994531, 0.500000, 0.500000, 0.997379},
  {168, 88200, 36078, 4095.875000, 0.991410, 0.500000, 0.500000, 0.884139},
};

/******* t4_3 parameter list *******/

#define t4_3_buflen      124278
#define t4_3_maxperiod   168

pluckparams_t t4_3_plist[] = {
  {168, 88200, 0, 6655.796875, 0.991410, 0.500000, 0.500000, 0.884139},
  {112, 88200, 6013, 5119.843750, 0.994531, 0.500000, 0.500000, 0.997379},
  {133, 88200, 12026, 6655.796875, 0.993313, 0.500000, 0.500000, 0.553575},
  {112, 88200, 18039, 4607.859375, 0.994531, 0.500000, 0.500000, 0.997379},
  {125, 88200, 24052, 7935.757812, 0.993736, 0.500000, 0.500000, 0.124575},
  {112, 88200, 30065, 8447.742188, 0.994531, 0.500000, 0.500000, 0.997379},
  {83, 88200, 36078, 7167.781250, 0.996301, 0.500000, 0.500000, 0.123125},
};

/******* t4_4 parameter list *******/

#define t4_4_buflen      124278
#define t4_4_maxperiod   168

pluckparams_t t4_4_plist[] = {
  {168, 88200, 0, 7167.781250, 0.991410, 0.500000, 0.500000, 0.884139},
  {112, 88200, 6013, 6655.796875, 0.994531, 0.500000, 0.500000, 0.997379},
  {133, 88200, 12026, 6399.804688, 0.993313, 0.500000, 0.500000, 0.553575},
  {112, 88200, 18039, 5375.835938, 0.994531, 0.500000, 0.500000, 0.997379},
  {125, 88200, 24052, 9215.718750, 0.993736, 0.500000, 0.500000, 0.124575},
  {112, 88200, 30065, 13311.593750, 0.994531, 0.500000, 0.500000, 0.997379},
  {83, 88200, 36078, 9215.718750, 0.996301, 0.500000, 0.500000, 0.123125},
};

struct tunelist {
	pluckparams_t *plist;
	int buflen;
	int maxperiod;
	int playlen;
	int size;
};

#ifndef MIN
#define MIN(a,b) ( (a) < (b) ? (a) : (b) )
#endif

#define MILLISECOND 1000

/*-----------------------------
 * local prototypes
 -----------------------------*/
int audio_probe(void);
int init_codec(int);
void write_codec_reg(char, ushort_t);
ushort_t read_codec_reg(char);

/*------------------------------
 *  global variables
 ------------------------------*/
int tdm_frame_rate;
ushort_t *codec_regs;  /* pointer to codec reg values array */
unsigned int *isa_dma_buf_addr = 0;

int
audio_probe()
{
  int rev;
  rev = READ_REG64(PHYS_TO_K1(MACE_AUDIO|AUD_CNTRL_STAT), reg_t);
  return (rev & CODEC_PRESENT);
}

void
audio_install(COMPONENT *p)
{
    if (audio_probe()) {
      p = AddChild(p,&a3tmpl,0);
      if (p == (COMPONENT *)NULL) cpu_acpanic("audio");
      p->Key = 1;
    }
}

static char *grab_mem(unsigned long);
static char *grab_loc;

/*
 * spin until we finish with the hello tune.  This is
 *  for the PROM cmd: play
 */
void
wait_hello_tune()
{

}

/*
 * tune_type:   0=hello, 1=goodbye, 2=graphics failure
 */

#pragma weak playTune = play_hello_tune
void 
play_hello_tune(int tune_type)
{
  int c;
  int cntrl_stat, ch2_write, ch2_read;
  int filled, fillable;
  int totalsamps;
  pluckparams_t *paramlist;	/* ptr to the parameters */
  int paramlist_size;		/* size of the list */
#ifdef POWBUG
  int *tunebuffer;
  int *dacbuf;
#endif
  uchar_t *tunebuffer;
  uchar_t *dacbuf;
  char *vol;
  int maxperiod;
  int playlen;
  int buflen;
  int volume;
  long tune;
  
  /*
   * Should not play the boottune at all if:
   *   1. there is no audio
   *   2. the volume parameter is equal to 0
   */
  if (!audio_probe()) {
    return;
  }

 
  /*  Check volume right after bad gfx tune.  This is slightly different
   * than Indigo2, but not many people should hear it, and we might save
   * a few people some grief that are sick of the boot tune.
   */
  vol = getenv("volume");
  atob(vol,&volume);

  /* don't play a boot tune */
  if (volume == 0) {
    return;
  }

  /* limit if needed */
  if (volume > 255) {
    volume = 255;
  }
  /*
   * convert the volume parameter into some reasonable register value.  
   * THe goal here was to make 80 == ~12 == ~-6dB.
   */
  volume /= 17;
  volume = 17 - volume;

  /* initialize audio hardware */
  if (init_codec(volume|(volume<<8)) < 0) {
    return;
  }
 
  /* PROM_STACK is a K1 space, convert it to cached K0 */
#ifdef XXX
  grab_loc = (char *)PHYS_TO_K0((PROM_STACK+1024) & 0x1fffffff);
#endif
  grab_loc = (char *)(PROM_STACK+1024);

  /*  Pick which tune to play.  If tune is set to an invalid number
   * use the default tune.
   */
  tune=0;
  switch(tune_type) {
	case 4:
		paramlist = s4_1_plist;
    		paramlist_size = sizeof(s4_1_plist)/sizeof(pluckparams_t);
    		buflen = s4_1_buflen;
    		playlen = s4_1_buflen;
    		maxperiod = s4_1_maxperiod;
    		break;
	case 3:
		paramlist = s5_1_plist;
    		paramlist_size = sizeof(s5_1_plist)/sizeof(pluckparams_t);
    		buflen = s5_1_buflen;
    		playlen = s5_1_buflen;
    		maxperiod = s5_1_maxperiod;
    		break;
	case 1:
		paramlist = s6_1_plist;
    		paramlist_size = sizeof(s6_1_plist)/sizeof(pluckparams_t);
    		buflen = s6_1_buflen;
    		playlen = s6_1_buflen;
    		maxperiod = s6_1_maxperiod;
    		break;
	case 5:
		paramlist = s6_3_plist;
    		paramlist_size = sizeof(s6_3_plist)/sizeof(pluckparams_t);
    		buflen = s6_3_buflen;
    		playlen = s6_3_buflen;
    		maxperiod = s6_3_maxperiod;
    		break;
	case 6:
		paramlist = t1_1_plist;
    		paramlist_size = sizeof(t1_1_plist)/sizeof(pluckparams_t);
    		buflen = t1_1_buflen;
    		playlen = t1_1_buflen;
    		maxperiod = t1_1_maxperiod;
    		break;
	case 2:
		paramlist = t2_2_plist;
    		paramlist_size = sizeof(t2_2_plist)/sizeof(pluckparams_t);
    		buflen = t2_2_buflen;
    		playlen = t2_2_buflen;
    		maxperiod = t2_2_maxperiod;
    		break;
	case 7:
		paramlist = t3_2_plist;
    		paramlist_size = sizeof(t3_2_plist)/sizeof(pluckparams_t);
    		buflen = t3_2_buflen;
    		playlen = t3_2_buflen;
    		maxperiod = t3_2_maxperiod;
    		break;
	case 8:
		paramlist = t4_1_plist;
    		paramlist_size = sizeof(t4_1_plist)/sizeof(pluckparams_t);
    		buflen = t4_1_buflen;
    		playlen = t4_1_buflen;
    		maxperiod = t4_1_maxperiod;
    		break;
	case 9:
		paramlist = t4_2_plist;
    		paramlist_size = sizeof(t4_2_plist)/sizeof(pluckparams_t);
    		buflen = t4_2_buflen;
    		playlen = t4_2_buflen;
    		maxperiod = t4_2_maxperiod;
    		break;
	case 10:
		paramlist = t4_3_plist;
    		paramlist_size = sizeof(t4_3_plist)/sizeof(pluckparams_t);
    		buflen = t4_3_buflen;
    		playlen = t4_3_buflen;
    		maxperiod = t4_3_maxperiod;
    		break;
	case 0:
		paramlist = t4_4_plist;
    		paramlist_size = sizeof(t4_4_plist)/sizeof(pluckparams_t);
    		buflen = t4_4_buflen;
    		playlen = t4_4_buflen;
    		maxperiod = t4_4_maxperiod;
    		break;
  default:
    /* unknown tune type */
    return;
  }
  
  if (isa_dma_buf_addr == 0) {
    isa_dma_buf_addr = (unsigned int *)getTile();
    WRITE_REG64(K1_TO_PHYS(isa_dma_buf_addr), PHYS_TO_K1(ISA_RINGBASE),
                reg_t);
    flushbus();
  } 

  /*
   * calculate how much memory this whole thing will take
   * (audio playback buffer + delay line buffer for pluck
   * algorithm) and allocate it.
   * 
   * buflen should be defined with the boot tune pluck params.
   * buflen is the number of audio FRAMES for the boot tune. need
   * to allocate 1*buflen for mono audio output.
   * 
   * maxperiod is also defined with the pluck params. it is the
   * maximum size of the delay line for the plucked string algorithm.
   */
  buflen = ((buflen * (int)sizeof(int)) + 0x3f) & ~0x3f;/* scale-n-align */
  playlen = ((playlen * (int)sizeof(int)) + 0xf) & ~0xf;/* scale-n-align */

  tunebuffer = (uchar_t *)grab_mem(buflen);
  bzero(tunebuffer,buflen);

  maketune(paramlist, paramlist_size, (int *)tunebuffer, maxperiod);

  flush_cache();

  /* pointer into dac dma ring buffer */
  dacbuf = (uchar_t *)((uint_t)isa_dma_buf_addr|(AD1843_DAC1_DMA_CHAN << 12));

  /* start dma */
  WRITE_REG64(CHAN_RESET, PHYS_TO_K1(MACE_AUDIO|AUD_CH2_CNTRL), reg_t);
  ch2_read = 0; /* reset clears all registers */
  us_delay(MILLISECOND);
#ifdef XX
  ch2_write = 192;  /* pad output buffer with 2 msecs of zeros */
#endif
  ch2_write=0;
  WRITE_REG64(CHAN_ENABLE|CHAN_DMA_ENABLE,
              PHYS_TO_K1(MACE_AUDIO|AUD_CH2_CNTRL), reg_t);
  WRITE_REG64(ch2_write, PHYS_TO_K1(MACE_AUDIO|AUD_CH2_WRITE), reg_t);

  totalsamps = 0;
  while (totalsamps < buflen) {
    cntrl_stat = READ_REG64(PHYS_TO_K1(MACE_AUDIO|AUD_CNTRL_STAT), reg_t);
    ch2_read = GET_CH2_READ_ALIAS(cntrl_stat);
    filled = ch2_write - ch2_read;
    if (filled < 0) {
      /* wrapped */
      filled += 4096;
    }
    fillable = 4096 - filled - 16;  /* must leave 1 block empty */
 
    if (fillable > buflen-totalsamps) fillable=buflen-totalsamps;

    /* now copy samples into dac dma ring buffer */
    if (ch2_write+fillable < 4096) {
      bcopy(tunebuffer, dacbuf+ch2_write, fillable);
    } else {
      /* wrap */
      c = 4096-ch2_write;
      bcopy(tunebuffer, dacbuf+ch2_write, c);
      bcopy(tunebuffer+c, dacbuf, fillable-c);
    }
    tunebuffer += fillable;
    ch2_write += fillable;
    if (ch2_write >= 4096) {
      ch2_write -= 4096;
    }
    
    totalsamps += fillable;

 /*   flush_cache(); */
    WRITE_REG64(ch2_write, PHYS_TO_K1(MACE_AUDIO|AUD_CH2_WRITE), reg_t);

    us_delay(MILLISECOND);
  }

}

static char *
grab_mem(unsigned long nlocs)
{
  char *loc = grab_loc;
  
  /* make sure new location is 8 byte aligned so works with any type */ 
  grab_loc = (char *)((__psunsigned_t)(grab_loc + nlocs + 0x7) & ~0x7L);
  
  return loc;
}

/*
 * do the pluckin'
 */
static void
maketune(pluckparams_t *list, int npluck, int *buf, int maxperiod)
{
    float *delaybuf;
    int i;

    delaybuf = (float *)grab_mem(maxperiod * sizeof(float));

    for (i = 0; i < npluck; i++,list++) {
      pluck(list, buf, delaybuf);
    }
}

/*
 * roll my own linear congruential random number generator.
 * swiped this from some code i wrote ages ago for dither
 * generation for quantization of audio samples.
 */
#define myrand(state) (state = ((state * 1103515245 + 12345) >> 1) - 1)
#define FABS(x)	(x < 0.0f ? -(x):(x))

/*
 * fill buf with n random numbers in the range (-amp,amp)
 */
static void
randfill(float *buf, int n, float amp)
{
    int rstate;			/* get random value to start with no init! */
    float avg, sum;
    float tmp, max;
    float scale;
    int i;

    for (i = 0; i < n; i++) {
	buf[i] = 2.0f*((1.0f/(float)0x7fffffff)*myrand(rstate)-0.5f);
    }

    sum = max = 0;
    for (i = 0; i < n; i++) {
	sum += buf[i];
    }

    avg = sum/n;
    for (i = 0; i < n; i++) {
	buf[i] -= avg;
	tmp = FABS(buf[i]);
	if (tmp > max) max = tmp;
    }

    scale = amp/max;
    for (i = 0; i < n; i++) {
	buf[i] *= scale;
    }
}

static void
pluck(pluckparams_t *params, int *buf, float *delaybuf)
{
    float sample, res1, res2, lastres1, lastsample;
    float rho, s, s1, apcoeff;
    float *bptr, *bend;
    int i, notelen;
    
    notelen = params->duration;
    bptr = delaybuf;
    bend = delaybuf+params->period-1;
    lastsample = res2 = lastres1 = 0.0f;

    randfill(delaybuf, params->period, params->amplitude);

    buf += params->start;

    rho = params->rho;
    s = params->s;
    s1 = params->s1;
    apcoeff = params->apcoeff;
    for (i = 0; i < notelen; i++) {
	sample = *bptr;
	res1 = rho * (s1*sample + s*lastsample);
	res2 = apcoeff*res1 + lastres1 - apcoeff*res2;

	*buf++ += (int)sample << 8;		/* mono sample */
	*buf++ += (int)sample << 8;		/* mono sample */

	lastsample = sample;
	lastres1 = res1;
	*bptr++ = res2;

	if (bptr > bend) bptr = delaybuf;
    }
}



/*----------------------------------------------------------------------------
 *
 * int init_codec(a3_subsys_t *)
 *
 *	- resets and initializes the AD1843 codec chip
 *
 *---------------------------------------------------------------------------*/

int
init_codec(int volume)
{
  ushort_t codec_data;
  char *pass;
  int audiopass;

  /*
   * reset the codec to clear all register and get the chip into
   * a known state
   */

  WRITE_REG64(CODEC_RESET, PHYS_TO_K1(MACE_AUDIO|AUD_CNTRL_STAT), reg_t);

  /* assert reset for 100nsec */
  us_delay(1);
  WRITE_REG64(0, PHYS_TO_K1(MACE_AUDIO|AUD_CNTRL_STAT), reg_t);

  /* default serial buss rate */
  tdm_frame_rate = TDM_RATE_12MHZ_32_SLOTS;
  
  /* bcopy?? */
  codec_regs = codec_reset_default;  /* setup codec register values */

  /*
   * poll the INIT bit to determine when the codec is ready
   * According to the chip spec, this takes between 400-800 usecs
   * after a reset.
   */

  codec_data = read_codec_reg(AD1843_STAT_REV);

  if (codec_data & INIT) {

    us_delay(500);
    codec_data = read_codec_reg(AD1843_STAT_REV);
    if (codec_data & INIT) {
       /* not ready: try on more time before failing */
      us_delay(300);
      codec_data = read_codec_reg(AD1843_STAT_REV);
      if (codec_data & INIT) {
	return -1;  /* FAILED INITIALIZATION */
      }
    }
  }

  /* 
   * POWBUG: re-enable analog loopback for Weather Channel here
   */

  /* 
   * setup serial clock rate, sample format, and set ADC sample lock
   * so left and right samples are transmitted on the TDM buss cycle.
   * The MACE serial interface expects this to happen.
   * If L/R samples are not transmitted in the same TDM cycle (also
   * called frame), garbage is latched into the MACE fifos and the
   * state machine could get screwed...
   */

  write_codec_reg(AD1843_SERIAL,
		     codec_regs[AD1843_SERIAL] |
		     SCLK_16MHZ |
		     ADC_SAMP_LCK |
		     DAC2_16BIT_SIGNED_PCM |
		     DAC1_16BIT_SIGNED_PCM |
		     LEFT_ADC_16BIT_SIGNED_PCM |
		     RIGHT_ADC_16BIT_SIGNED_PCM);

  /* wait 6 slots for change serial clk rate change to take effect */
  us_delay(7 * TDM_RATE_12MHZ_32_SLOTS);
  tdm_frame_rate = TDM_RATE_16MHZ_32_SLOTS;


  /*
   * Put conversion resources into standby by zeroing the PDNI bit.
   * Also, re-enable any clk generators or output bit clks requested.
   * Going from powerdown to standby takes about 474milliseconds.
   */

  write_codec_reg(AD1843_CONFIG, codec_regs[AD1843_CONFIG] & ~PDNI);
  us_delay(480000);
  codec_data = read_codec_reg(AD1843_STAT_REV);
  if (codec_data & PDNO) {
    us_delay(100000);
    codec_data = read_codec_reg(AD1843_STAT_REV);
    if (codec_data & PDNO) {
      printf("audio failed power-up to standby\n");
      return -1;
    }
  }

  /*
   * Conversion resources now in standby.  Enable DAC1 to DAC2 mixing.
   * By mixing the outputs of the dacs, both dacs output the same data 
   * and therefore act as a single device.
   */

  write_codec_reg(AD1843_FILTER_MODE, codec_regs[AD1843_FILTER_MODE]|
		     DAC_DAC_DMIX);

  /*
   * enable conversion resources
   */

  write_codec_reg(AD1843_CH_POWERDOWN,
		  codec_regs[AD1843_CH_POWERDOWN] |
		  DAC2_ENABLE |
		  DAC1_ENABLE |
		  ANALOG_ENABLE |
		  ANALOG_MIX_ENABLE |
		  ADC_R_ENABLE |
		  ADC_L_ENABLE |
		  HEADPHONE_ENABLE);
  
  /*
   * enable analog line in to analog DAC1 loopback for the Weather Channel.
   * Unmute both right and left channels and set gain to 0dB.
   */
  pass = getenv("audiopass");
  if (pass != NULL) {
	atob(pass,&audiopass);
	if (audiopass) {
        	write_codec_reg(AD1843_AUX1_MIX, 0x808);
	}
  }

  /*
   *  enable clock generators 
   */
  
  write_codec_reg(AD1843_CONFIG,
		     codec_regs[AD1843_CONFIG] |
		     CLK_GEN1_ENABLE |
		     CLK_GEN2_ENABLE |
		     CLK_GEN3_ENABLE);
  /* 
   * setup clock generators
   */

  write_codec_reg(AD1843_CLK_SRC_SELECT,
		  DAC1_CLKSRC_CLK1|DAC2_CLKSRC_CLK3|ADC_CLKSRC_CLK2);

  /*
   * set at 44100 sample rate for the boot tune
   */

   write_codec_reg(AD1843_CLK_GEN1_RATE, 34000);

  /*
   * unmute DAC, headpones, and speaker
   */

  write_codec_reg(AD1843_DAC1_GATTN, volume);
  write_codec_reg(AD1843_DAC2_GATTN, volume);
  write_codec_reg(AD1843_MONO_MIX_MISC,
		     codec_regs[AD1843_MONO_MIX_MISC] &
		     ~(HEADPHONE_MUTE|MONO_OUTPUT_MUTE));
  
  return 0;

}

/* reads the codec reg */
ushort_t
read_codec_reg(char codec_addr)
{

  WRITE_REG64(((codec_addr << 17) | 1<<16),
	      PHYS_TO_K1(MACE_AUDIO|AUD_CODEC_REG), reg_t);
  wbflush();
  us_delay(tdm_frame_rate * 4);

  codec_regs[codec_addr] =
    READ_REG64(PHYS_TO_K1(MACE_AUDIO|AUD_CODEC_READ), reg_t);
  return(codec_regs[codec_addr]);
}

/* does the work for codec register writes */
void
write_codec_reg(char codec_addr, ushort_t value)
{
  codec_regs[codec_addr] = value;
  WRITE_REG64(((codec_addr << 17)|(0xffff & value)),
              PHYS_TO_K1(MACE_AUDIO|AUD_CODEC_REG), reg_t);
  wbflush();
  us_delay(tdm_frame_rate * 2);
}
