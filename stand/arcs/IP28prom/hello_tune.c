#ident	"$Revision: 1.5 $"

/*
 * hello_tune.c
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/invent.h>
#include <sys/hal2.h>
#include <sys/pbus.h>
#include <arcs/hinv.h>
#include <libsc.h>
#include <libsk.h>

void wait_hello_tune(void);
void run_chandra(void);

#define CODECA_DMA_CHAN 1
#define AESTX_DMA_CHAN  2

static const COMPONENT hal2tmpl = {
	ControllerClass,		/* Class */
	AudioController,		/* Type */
	Input|Output,			/* Flags */
	SGI_ARCS_VERS,			/* Version */
	SGI_ARCS_REV,			/* Revision */
	0,				/* Key */
	0x01,				/* Affinity */
	0,				/* ConfigurationDataSize */
	5,				/* IdentifierLength */
	"HAL2"				/* Identifier */
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

static void pluck(const pluckparams_t *params, int *soundbuf, float *delaybuf);
static void maketune(const pluckparams_t *list, int npluck, int *buf,
		int maxperiod);
static void randfill(float *buf, int n, float amp);

/* Tune data
 *
 * NOTE: max xxx_buflen == 512000 (2MB) for IP26 memory map
 */

/* A 220, A# 233, B 247, C 262, C# 277, D 294, D# 311, E 330, F 349, F# 370,
 * G 392, G# 415
 *
 * first column is 44100/NOTE.
 */
#define HI(X) ((X)/2)
#define HIHI(X) HI(HI(X))
#define NOTE_A  (44100/220)
#define NOTE_A_ (44100/233)
#define NOTE_B  (44100/247)
#define NOTE_C  (44100/262)
#define NOTE_C_ (44100/277)
#define NOTE_D  (44100/294)
#define NOTE_E  (44100/330)
#define NOTE_F  (44100/349)
#define NOTE_F_ (44100/370)
#define NOTE_G  (44100/392)
#define NOTE_G_ (44100/415)

#define starttune1_buflen      88200+36000
#define starttune1_playlen     85000
#define starttune1_maxperiod   NOTE_A

static pluckparams_t starttune1_plist[] = {
  {    (NOTE_A ), 88200,     0,  9500.0, 1.0, 0.5, 0.5, 0.25},
  {    (NOTE_C_), 88200,  6000, 12500.0, 1.0, 0.5, 0.5, 0.25},
  {    (NOTE_E ), 88200, 12000, 12500.0, 1.0, 0.5, 0.5, 0.25},
  {  HI(NOTE_A ), 88200, 18000, 12500.0, 1.0, 0.5, 0.5, 0.50},
  {  HI(NOTE_C_), 88200, 24000, 12500.0, 1.0, 0.5, 0.5, 0.50},
  {  HI(NOTE_E ), 88200, 30000, 12500.0, 1.0, 0.5, 0.5, 0.50},
  {HIHI(NOTE_A ), 88200, 36000, 15500.0, 1.0, 0.5, 0.5, 0.75},
};

/******* starttune1a parameter list *******/

#define starttune1a_buflen      123055
#define starttune1a_playlen     85000
#define starttune1a_maxperiod   188

static const pluckparams_t starttune1a_plist[] = {
  {188, 88200, 2756, 13567.585938, 0.990306, 0.500000, 0.500000, 0.173776},
  {149, 88200, 8039, 13823.578125, 0.992407, 0.500000, 0.500000, 0.196720},
  {133, 88200, 12173, 11519.648438, 0.993313, 0.500000, 0.500000, 0.553575},
  {99, 88200, 14929, 13823.578125, 0.995269, 0.500000, 0.500000, 0.157895},
  {94, 88200, 19351, 14847.546875, 0.995622, 0.500000, 0.500000, 0.814962},
  {74, 88200, 22968, 13567.585938, 0.996960, 0.500000, 0.500000, 0.261339},
  {66, 88200, 26816, 14335.562500, 0.997613, 0.500000, 0.500000, 0.435053},
  {62, 88200, 30605, 13567.585938, 0.997944, 0.500000, 0.500000, 0.220089},
  {66, 88200, 34855, 14335.562500, 0.997613, 0.500000, 0.500000, 0.435053},
};

/******* starttune2 parameter list *******/

#define starttune2_buflen      143095
#define starttune2_playlen     65000
#define starttune2_maxperiod   188

static const pluckparams_t starttune2_plist[] = {
  {188, 88200, 2796, 6655.796875, 0.990306, 0.500000, 0.500000, 0.173776},
  {133, 88200, 8596, 14335.562500, 0.993313, 0.500000, 0.500000, 0.553575},
  {125, 88200, 14510, 13055.601563, 0.993736, 0.500000, 0.500000, 0.124575},
  {99, 88200, 18357, 12543.617188, 0.995269, 0.500000, 0.500000, 0.157895},
  {94, 88200, 23123, 12031.632813, 0.995622, 0.500000, 0.500000, 0.814962},
  {74, 88200, 27889, 13823.578125, 0.996960, 0.500000, 0.500000, 0.261339},
  {62, 88200, 34895, 13823.578125, 0.997944, 0.500000, 0.500000, 0.220089},
};

/******* starttune3 parameter list *******/

#define starttune3_buflen      134596
#define starttune3_playlen     71000
#define starttune3_maxperiod   188

static const pluckparams_t starttune3_plist[] = {
  {188, 88200, 2930, 13567.585938, 0.990306, 0.500000, 0.500000, 0.173776},
  {125, 88200, 8673, 15871.515625, 0.993736, 0.500000, 0.500000, 0.124575},
  {74, 88200, 13037, 14335.562500, 0.996960, 0.500000, 0.500000, 0.261339},
  {62, 88200, 16310, 14335.562500, 0.997944, 0.500000, 0.500000, 0.220089},
  {46, 88200, 18721, 11263.656250, 0.999738, 0.500000, 0.500000, 0.110509},
  {37, 88200, 22396, 13567.585938, 1.000000, 0.168371, 0.831629, 0.455139},
};

/******* stoptune1 parameter list *******/

#define stoptune1_buflen      88200+32000
#define stoptune1_playlen     80000
#define stoptune1_maxperiod   HI(NOTE_A)

static pluckparams_t stoptune1_plist[] = {
  {HIHI(NOTE_A ), 88200,     0, 12500.0, 1.0, 0.5, 0.5, 0.2},
  {  HI(NOTE_F_), 88200,  8000, 12500.0, 1.0, 0.5, 0.5, 0.2},
  {  HI(NOTE_E ), 88200, 16000, 12500.0, 1.0, 0.5, 0.5, 0.2},
  {  HI(NOTE_C_), 88200, 24000, 12500.0, 1.0, 0.5, 0.5, 0.2},
  {  HI(NOTE_A ), 88200, 32000, 12500.0, 1.0, 0.5, 0.5, 0.2},
};

/******* stoptune1a parameter list *******/

#define stoptune1a_buflen      130979
#define stoptune1a_playlen     80000
#define stoptune1a_maxperiod   99

static const pluckparams_t stoptune1a_plist[] = {
  {62, 88200, 1025, 12543.617188, 0.997944, 0.500000, 0.500000, 0.220089},
  {66, 88200, 3953, 14079.570313, 0.997613, 0.500000, 0.500000, 0.435053},
  {74, 88200, 8719, 13567.585938, 0.996960, 0.500000, 0.500000, 0.261339},
  {62, 88200, 15150, 13823.578125, 0.997944, 0.500000, 0.500000, 0.220089},
  {66, 88200, 19457, 14079.570313, 0.997613, 0.500000, 0.500000, 0.435053},
  {74, 88200, 24166, 13823.578125, 0.996960, 0.500000, 0.500000, 0.261339},
  {94, 88200, 28013, 12287.625000, 0.995622, 0.500000, 0.500000, 0.814962},
  {99, 88200, 32779, 12031.632813, 0.995269, 0.500000, 0.500000, 0.157895},
};

/******* stoptune2 parameter list *******/

#define stoptune2_buflen      142750
#define stoptune2_playlen     90000
#define stoptune2_maxperiod   99

static const pluckparams_t stoptune2_plist[] = {
  {62, 88200, 2239, 16383.500000, 0.997944, 0.500000, 0.500000, 0.220089},
  {66, 88200, 7062, 14591.554688, 0.997613, 0.500000, 0.500000, 0.435053},
  {74, 88200, 12517, 12031.632813, 0.996960, 0.500000, 0.500000, 0.261339},
  {94, 88200, 20327, 14591.554688, 0.995622, 0.500000, 0.500000, 0.814962},
  {74, 88200, 32902, 11775.640625, 0.996960, 0.500000, 0.500000, 0.261339},
  {94, 88200, 42951, 11007.664063, 0.995622, 0.500000, 0.500000, 0.814962},
  {99, 88200, 54550, 10751.671875, 0.995269, 0.500000, 0.500000, 0.157895},
};

/******* stoptune3 parameter list *******/

#define stoptune3_buflen      141372
#define stoptune3_playlen     70000
#define stoptune3_maxperiod   188

static const pluckparams_t stoptune3_plist[] = {
  {83, 88200, 2591, 15871.515625, 0.996301, 0.500000, 0.500000, 0.123125},
  {188, 88200, 3223, 15871.515625, 0.990306, 0.500000, 0.500000, 0.173776},
  {74, 88200, 8448, 13567.585938, 0.996960, 0.500000, 0.500000, 0.261339},
  {83, 88200, 11951, 14335.562500, 0.996301, 0.500000, 0.500000, 0.123125},
  {94, 88200, 17349, 13311.593750, 0.995622, 0.500000, 0.500000, 0.814962},
  {125, 88200, 22172, 13567.585938, 0.993736, 0.500000, 0.500000, 0.124575},
};

/* hand-hacked bad gfx tune */

#define badgfx_buflen		132300
#define badgfx_playlen		132300
#define badgfx_size		(sizeof(badgfx_plist)/sizeof(pluckparams_t))
#define badgfx_maxperiod	282

/* A 220, A# 233, B 247, C 262, C# 277, D 294, D# 311, E 330, F 349, F# 370,
 * G 392, G# 415
 *
 * first column is 44100/NOTE.
 */
static const pluckparams_t badgfx_plist[] = {	/* devil chords (three) */
  {168, 88200, 0, 16383.500000, 0.991410, 0.500000, 0.500000, 0.884139},
  {211, 88200, 0, 16383.500000, 0.989081, 0.500000, 0.500000, 0.067127},
  {158, 88200, 0, 16383.500000, 0.991921, 0.500000, 0.500000, 0.249304},
  {224, 88200, 0, 16383.500000, 0.988417, 0.500000, 0.500000, 0.331005},
  {282, 88200, 0, 16383.500000, 0.985368, 0.500000, 0.500000, 0.007285},
  {252, 88200, 0, 16383.500000, 0.986976, 0.500000, 0.500000, 0.892327},
  {168, 88200, 22050, 16383.500000, 0.991410, 0.500000, 0.500000, 0.884139},
  {211, 88200, 22050, 16383.500000, 0.989081, 0.500000, 0.500000, 0.067127},
  {158, 88200, 22050, 16383.500000, 0.991921, 0.500000, 0.500000, 0.249304},
  {224, 88200, 22050, 16383.500000, 0.988417, 0.500000, 0.500000, 0.331005},
  {282, 88200, 22050, 16383.500000, 0.985368, 0.500000, 0.500000, 0.007285},
  {252, 88200, 22050, 16383.500000, 0.986976, 0.500000, 0.500000, 0.892327},
  {168, 88200, 44100, 16383.500000, 0.991410, 0.500000, 0.500000, 0.884139},
  {211, 88200, 44100, 16383.500000, 0.989081, 0.500000, 0.500000, 0.067127},
  {158, 88200, 44100, 16383.500000, 0.991921, 0.500000, 0.500000, 0.249304},
  {224, 88200, 44100, 16383.500000, 0.988417, 0.500000, 0.500000, 0.331005},
  {282, 88200, 44100, 16383.500000, 0.985368, 0.500000, 0.500000, 0.007285},
  {252, 88200, 44100, 16383.500000, 0.986976, 0.500000, 0.500000, 0.892327},
};

#define MAXTUNES	4L		/* number of tunes supported */
struct tunelist {
	const pluckparams_t *plist;
	int buflen;
	int maxperiod;
	int playlen;
	int size;
};

static const struct tunelist boottunes[MAXTUNES] = {
	{ starttune1_plist, starttune1_buflen, starttune1_maxperiod,
	  starttune1_playlen, sizeof(starttune1_plist)/sizeof(pluckparams_t) },
	{ starttune1a_plist, starttune1a_buflen, starttune1a_maxperiod,
	  starttune1a_playlen, sizeof(starttune1a_plist)/sizeof(pluckparams_t) },
	{ starttune2_plist, starttune2_buflen, starttune2_maxperiod,
	  starttune2_playlen, sizeof(starttune2_plist)/sizeof(pluckparams_t) },
	{ starttune3_plist, starttune3_buflen, starttune3_maxperiod,
	  starttune3_playlen, sizeof(starttune3_plist)/sizeof(pluckparams_t) },
};

static const struct tunelist stoptunes[MAXTUNES] = {
	{ stoptune1_plist, stoptune1_buflen, stoptune1_maxperiod,
	  stoptune1_playlen, sizeof(stoptune1_plist)/sizeof(pluckparams_t) },
	{ stoptune1a_plist, stoptune1a_buflen, stoptune1a_maxperiod,
	  stoptune1a_playlen, sizeof(stoptune1a_plist)/sizeof(pluckparams_t) },
	{ stoptune2_plist, stoptune2_buflen, stoptune2_maxperiod,
	  stoptune2_playlen, sizeof(stoptune2_plist)/sizeof(pluckparams_t) },
	{ stoptune3_plist, stoptune3_buflen, stoptune3_maxperiod,
	  stoptune3_playlen, sizeof(stoptune3_plist)/sizeof(pluckparams_t) },
};

#ifndef MIN
#define MIN(a,b) ( (a) < (b) ? (a) : (b) )
#endif

void
audio_install(COMPONENT *p)
{
    unsigned short rev;

    rev = *(unsigned int *)PHYS_TO_COMPATK1(HAL2_REV);
    if (rev & 0x8000)
	return;

    p = AddChild(p,(COMPONENT *)&hal2tmpl,0);
    if (p == (COMPONENT *)NULL) cpu_acpanic("audio");
    p->Key = rev;
}

#define SPIN	while (*isrp & HAL2_ISR_TSTATUS)

#define HAL2_AESTX_DMA_ENABLE    0x4
#define HAL2_CODECA_DMA_ENABLE   0x8

static void waitfordma(__psunsigned_t, __psunsigned_t, int);
static char *grab_mem(unsigned long);

static void
SetupHAL2(void)
{
    volatile unsigned int *idr0p=(unsigned int *)PHYS_TO_COMPATK1(HAL2_IDR0);
    volatile unsigned int *idr1p=(unsigned int *)PHYS_TO_COMPATK1(HAL2_IDR1);
    volatile unsigned int *iarp=(unsigned int *)PHYS_TO_COMPATK1(HAL2_IAR);
    volatile unsigned int *isrp=(unsigned int *)PHYS_TO_COMPATK1(HAL2_ISR);
    
    /* Set up the ISR */

    *isrp = HAL2_ISR_CODEC_RESET_N|HAL2_ISR_GLOBAL_RESET_N;

    /* Clear DMA ENABLE */

    *idr0p = 0x0;  /* Disable all the devices */
    *iarp = HAL2_DMA_ENABLE_W;
    SPIN;

    /* Set up DMA Endian */

    *idr0p = 0;  /* All ports are big endian */
    *iarp = HAL2_DMA_ENDIAN_W;
    SPIN;

    /* 
     * Set up DMA DRIVE 
     *
     *    Audio is currently slated for HPC3 DMA channels 1-4
     *    and the parallel port chip uses DMA channel 0. 
     */

    *idr0p = 0x2 + 0x4;  /* Drive DMA ports 1 and 2 */
    *iarp = HAL2_DMA_DRIVE_W;
    SPIN;

    /* Set up CODEC A */

    *idr0p = CODECA_DMA_CHAN	/* Physical DMA channel */
	    +(1<<3)		/* Bres Clock ID */
	    +(1<<8);		/* Mono Mode */
    *iarp = HAL2_CODECA_CTRL1_W;
    SPIN;
    *idr0p = (0<<10)+(0x00<<7)+(0x00<<2);	/* unmute, A/D gains == 0 */
    *idr1p = (0x0<<7)	/* Left D/A output atten */
	    +(0x0<<2)    /* Right D/A output atten */
	    +(0<<1)	/* Digital output port data bit 1 */
	    +(0);	/* Digital output port data bit 0 */
    *iarp = HAL2_CODECA_CTRL2_W;
    SPIN;
}

static void
EnableOutput(void)
{
    volatile unsigned int *idr0p=(unsigned int *)PHYS_TO_COMPATK1(HAL2_IDR0);
    volatile unsigned int *iarp=(unsigned int *)PHYS_TO_COMPATK1(HAL2_IAR);
    volatile unsigned int *isrp=(unsigned int *)PHYS_TO_COMPATK1(HAL2_ISR);
    
    /* Set up RELAY ctl. */

    *idr0p = 0x1;        /* Enable headphones n' MDAC for indigo mode */
    *iarp = HAL2_RELAY_CONTROL_W;
    SPIN;
    
    /* Set up DMA ENABLE */

    *idr0p = HAL2_AESTX_DMA_ENABLE + HAL2_CODECA_DMA_ENABLE;  
    *iarp = HAL2_DMA_ENABLE_W;
    SPIN;
}

/*
 * InitiateDMA 
 *
 *    initiates DMA on a particular channel and in a particular 
 *    direction (0 = frommips, 1 = tomips).
 */
static void
InitiateDMA(scdescr_t *desc_list, int direction, int channel)
{
    pbus_control_write_t ctrlval;
    volatile pbus_control_write_t *ctrlptr =
	(pbus_control_write_t *)PHYS_TO_COMPATK1(HPC3_PBUS_CONTROL_ADDR(channel));
    volatile unsigned int *dpptr =
	(unsigned int *)PHYS_TO_COMPATK1(HPC3_PBUS_DP(channel));

    ctrlval.pad0=0;
    ctrlval.fifo_end=4*channel+3;
    ctrlval.pad1=0;
    ctrlval.fifo_beg=4*channel;
    ctrlval.highwater=11;
    ctrlval.pad2=0;
    ctrlval.real_time=1;
    ctrlval.ch_act_ld=1;
    ctrlval.ch_act=1;
    ctrlval.flush=0;
    ctrlval.receive=direction;
    ctrlval.little=0;
    ctrlval.pad3=0;

    *dpptr = KDM_TO_MCPHYS(desc_list);
    *ctrlptr = ctrlval;

    flushbus();
}

#define HPC_NBPP	8192			/* we can DMA in 8K chunks */
#define HPC_POFFMASK	(HPC_NBPP-1)

static void
BuildDescriptors(scdescr_t *list,void *startaddr,int nbytes)
{
    int nremaining, bytes_till_page_boundary, bytes_this_descriptor;
    scdescr_t *desc;
    char *p;

    desc=list;
    nremaining=nbytes;
    p=(char *)startaddr;
    while(nremaining) {
	desc->cbp = KDM_TO_MCPHYS(p);

        bytes_till_page_boundary = HPC_NBPP -
		(int)((__psunsigned_t)p & HPC_POFFMASK);
        bytes_this_descriptor = MIN(bytes_till_page_boundary,nremaining);
        desc->bcnt = bytes_this_descriptor;

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
    desc->nbp = 0;
    desc->eox = 1;
}

scdescr_t  *tune_desc_list;
static char *grab_loc;

void 
play_hello_tune(int tune_type)
/*
 * tune_type:   0=hello, 1=goodbye, 2=graphics failure
 */
{
    volatile unsigned int *revptr = (unsigned int *)PHYS_TO_COMPATK1(HAL2_REV);
    const pluckparams_t *paramlist;	/* ptr to the parameters */
    int paramlist_size;			/* size of the list */
    int *tunebuffer;
    int maxperiod;
    int playlen;
    int buflen;
    int volume;
    long tune;
    char *p;

    /* on old T5's do not play the tune */
    if ((r4k_getprid() & 0x00f0) < 0x0020)
	return;

    /*
     * Should not play the boottune at all if:
     *   1. there is no audio
     *   2. the volume parameter is equal to 0
     */
    if (*revptr & 0x8000)
	return;

    /*  Check volume right after bad gfx tune.  This is slightly different
     * than Indigo2, but not many people should hear it, and we might save
     * a few people some grief that are sick of the boot tune.
     */
    p = cpu_get_nvram_offset(NVOFF_VOLUME,NVLEN_VOLUME);
    atob(p,&volume);
    if (volume == 0) 
	return;

    grab_loc = (char *)PHYS_TO_K0_RAM(0x00c00000);	/* 12MB, cached */

    /*  Pick which tune to play.  If tune is set to an invalid number
     * use the default tune.
     */
    tune = *cpu_get_nvram_offset(NVOFF_BOOTTUNE,NVLEN_BOOTTUNE) - '1';
    if (tune == -1) {
	/* pick a random tune based on the MC RPSS_CTR counter */
	tune = (*(volatile uint *)PHYS_TO_COMPATK1(RPSS_CTR)) % MAXTUNES;
    }
    else if (tune < 0 || tune >= MAXTUNES)
	tune = 0;
    
    switch(tune_type) {
	case 0:
	    paramlist = boottunes[tune].plist;
	    paramlist_size = boottunes[tune].size;
	    buflen = boottunes[tune].buflen;
	    playlen = boottunes[tune].playlen;
	    maxperiod = boottunes[tune].maxperiod;
	    break;
	case 1:
	    paramlist = stoptunes[tune].plist;
	    paramlist_size = stoptunes[tune].size;
	    buflen = stoptunes[tune].buflen;
	    playlen = stoptunes[tune].playlen;
	    maxperiod = stoptunes[tune].maxperiod;
	    break;
	case 2:
	    paramlist = badgfx_plist;
	    paramlist_size = badgfx_size;
	    buflen = badgfx_buflen;
	    playlen = badgfx_playlen;
	    maxperiod = badgfx_maxperiod;
	    break;
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
    buflen = ((buflen * (int)sizeof(int)) + 0xf) & ~0xf;/* scale-n-align */
    playlen = ((playlen * (int)sizeof(int)) + 0xf) & ~0xf;/* scale-n-align */
    tunebuffer = (int *)grab_mem(buflen);
    bzero(tunebuffer,buflen);
    tune_desc_list = (scdescr_t *)grab_mem(((buflen/HPC_NBPP)+1) *
					   sizeof(scdescr_t));

    /* Set up the HAL2 part */
    SetupHAL2();

    /* Set the volume of the audio system */

    *((unsigned int *)(PHYS_TO_COMPATK1(VOLUME_LEFT)))  = volume;
    *((unsigned int *)(PHYS_TO_COMPATK1(VOLUME_RIGHT))) = volume;

    maketune(paramlist, paramlist_size, tunebuffer, maxperiod);

    BuildDescriptors(tune_desc_list, tunebuffer, playlen);
    flush_cache();
    
    /* 
     * Do the last bit of HAL2 work to get samples out
     * (throw the relay and enable the DMA from the HAL2 side).
     */
    EnableOutput();

    InitiateDMA(tune_desc_list, 0, CODECA_DMA_CHAN);
    InitiateDMA(tune_desc_list, 0, AESTX_DMA_CHAN);
}

void
wait_hello_tune(void)
{
	waitfordma(CODECA_DMA_CHAN, AESTX_DMA_CHAN, 5000);
}

static char *
grab_mem(unsigned long nlocs)
{
	char *loc = grab_loc;

	/* make sure new location is 8 byte aligned so works with any type */ 
	grab_loc = (char *)((__psunsigned_t)(grab_loc + nlocs + 0x7) & ~0x7L);

#if DEBUG
	if (KDM_TO_PHYS(grab_loc) >= 0x08e40000)
		printf("grab_mem: overflowed standalone map: 0x%x\n",grab_loc);
#endif

	return loc;
}

/*
 * maxtimes = milliseconds to wait for the two DMA streams
 */
static void
waitfordma(__psunsigned_t chan1, __psunsigned_t chan2, int maxtimes)
{
    volatile pbus_control_read_t *ctrlptr1=
        (pbus_control_read_t *)PHYS_TO_COMPATK1(HPC3_PBUS_CONTROL_ADDR(chan1));
    volatile pbus_control_read_t *ctrlptr2=
        (pbus_control_read_t *)PHYS_TO_COMPATK1(HPC3_PBUS_CONTROL_ADDR(chan2));
    pbus_control_read_t ctrlval1;
    pbus_control_read_t ctrlval2;
    int ntimes;

    for (ntimes=0;ntimes<maxtimes;ntimes++) {
        ctrlval1 = *ctrlptr1;
        ctrlval2 = *ctrlptr2;
        if(!ctrlval1.ch_act && !ctrlval2.ch_act) {
		return;
	}
        us_delay(1000);
    }

#if DEBUG
    printf("warning: waitfordma failed!\n");
#endif
}

/*
 * do the pluckin'
 */
static void
maketune(const pluckparams_t *list, int npluck, int *buf, int maxperiod)
{
    float *delaybuf;
    int i;

    delaybuf = (float *)grab_mem(maxperiod * sizeof(float));

    for (i = 0; i < npluck; i++,list++)
	pluck(list, buf, delaybuf);
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
pluck(const pluckparams_t *params, int *buf, float *delaybuf)
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

    for (i = 0; i < notelen; i++, buf++) {
	sample = *bptr;
	res1 = rho * (s1*sample + s*lastsample);
	res2 = apcoeff*res1 + lastres1 - apcoeff*res2;

	*buf += (int)sample << 8;		/* mono sample */

	lastsample = sample;
	lastres1 = res1;
	*bptr++ = res2;

	if (bptr > bend) bptr = delaybuf;
    }
}
