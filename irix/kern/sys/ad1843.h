/**************************************************************************
 *                                                                        *
 *          Copyright (C) 1996, Silicon Graphics, Inc.                    *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident "$Revision"

/*
 * ad1843.h - IP32 (Moosehead) baseline audio defines
 */

typedef unsigned long long reg_t;

/* Moosehead audio register definitions - MACE registers */
typedef struct _mace_audio_regs_t {
  reg_t cntrl_stat;           /* reset and codec present bit */
  reg_t codec_reg;            /* codec register address to read or write */
  reg_t codec_intr_mask;  /* mask for read value */
  reg_t codec_read;           /* codec status word setup in codec_reg */
  /* stereo analog input channel */
  reg_t ch1_cntrl;             /* channel 1 ring control */
  reg_t ch1_read;              /* channel 1 ring buffer read pointer */
  reg_t ch1_write;             /* RO -controlled by hardware */
  reg_t ch1_depth;              /* channel 1 number of 32 bit samps in ring */
  /* stereo analog output channel */
  reg_t ch2_cntrl;           /* channel 2 ring control */
  reg_t ch2_read;            /* RO- controlled by hardware */
  reg_t ch2_write;           /* channel 2 ring buffer write pointer */
  reg_t ch2_depth;            /* channel 2 number of 32 bit samps in ring */
  /* stereo output channel */
  reg_t ch3_cntrl;           /* channel 3 ring control */
  reg_t ch3_read;            /* RO- controlled by hardware */
  reg_t ch3_write;           /* channel 3 ring buffer write pointer */
  reg_t ch3_depth;             /* channel 3 number of 32 bit samps in ring */
} mace_audio_regs_t;

/* MACE audio register offsets */
#define AUD_CNTRL_STAT       0
#define AUD_CODEC_REG        0x8
#define AUD_CODEC_INTR_MASK  0x10
#define AUD_CODEC_READ       0x18
#define AUD_CH1_CNTRL        0x20
#define AUD_CH1_READ         0x28
#define AUD_CH1_WRITE        0x30
#define AUD_CH1_DEPTH        0x38
#define AUD_CH2_CNTRL        0x40
#define AUD_CH2_READ         0x48
#define AUD_CH2_WRITE        0x50
#define AUD_CH2_DEPTH        0x58
#define AUD_CH3_CNTRL        0x60
#define AUD_CH3_READ         0x68
#define AUD_CH3_WRITE        0x70
#define AUD_CH3_DEPTH        0x78

#define AD1843_ADC_DMA_CHAN   0  /* data stream from codec */
#define AD1843_DAC1_DMA_CHAN  1  /* data stream to codec */
#define AD1843_DAC2_DMA_CHAN  2  /* data stream to codec */

/**************** MACE REGISTER BITS **************************/
/* cntrl_stat */
#define CODEC_RESET             (1<<0)
#define CODEC_PRESENT           (1<<1)
#define GET_CH1_RING_WRITE_ALIAS(x)  (((x) & 0xfc) << 3)   /* bits 8:2 */
#define GET_CH2_RING_READ_ALIAS(x)   (((x) & 0x7f00) >> 4)  /* bits 15:9 */
#define GET_CH3_RING_READ_ALIAS(x)   (((x) & 0x3f8000) >> 11)  /* bits 22:16 */
#define VOLUME_CNTRL_SHIFT      23    /* positive edge latched inputs */
#define VOLUME_CNTRL             (3<<VOLUME_CNTRL_SHIFT) 
#define VOLUME_UP              (1<<23)
#define VOLUME_DOWN            (1<<24)
#define VOLUME_DEBOUNCE_USECS   (10 * 1000) /* debounce for 10 ms */

/* ch?_cntrl - same for all three channel control registers */
#define CHAN_THRESHOLD_INTR_MASK          ((reg_t)(7<<5))
#define CHAN_THRESHOLD_INTR_OFF           (0<<5)
#define CHAN_THRESHOLD_INTR_25            (1<<5)
#define CHAN_THRESHOLD_INTR_50            (2<<5)  
#define CHAN_THRESHOLD_INTR_75            (3<<5)
#define CHAN_THRESHOLD_INTR_EMPTY         (4<<5)
#define CHAN_THRESHOLD_INTR_NOT_EMPTY     (5<<5)
#define CHAN_THRESHOLD_INTR_FULL          (6<<5)
#define CHAN_THRESHOLD_INTR_NOT_FULL      (7<<5)
#define CHAN_DMA_ENABLE                   (1<<9)
     /* reset chan (inactive), all regs reset, intr disable, fifo flush */
#define CHAN_RESET                        (1<<10)
#define CHAN_ENABLE                        0
#define CHAN_ALIVE CHAN_ENABLE

/* separate desired aliases out of control status register */
#define GET_CH1_WRITE_ALIAS(x) (((x) << 3) & 0xfe0)
#define GET_CH2_READ_ALIAS(x) (((x) >> 4) & 0xfe0)
#define GET_CH3_READ_ALIAS(x) (((x) >> 11) & 0xfe0)


/* number of microseconds for a complete TDM phase */
#ifdef US_DELAY_BUSTED
#define TDM_RATE_16MHZ_32_SLOTS 180
#define TDM_RATE_12MHZ_32_SLOTS 270
#define MILLISECOND 18000
#else
#define MILLISECOND 1000
#define TDM_RATE_16MHZ_32_SLOTS 32    /* 16bits * 32slots * 1/SCLK= 31.25 us */
#define TDM_RATE_12MHZ_32_SLOTS 42    /* 16bits * 32slots * 1/SCLK= 41.67 us */
#endif

/**************************************************************************
 *
 * ad1843.h - AD1843 Analog Devices A/D chip definitions
 *
 **************************************************************************/

/* register addresses */
#define AD1843_STAT_REV           0
#define AD1843_CH_STAT            1
#define AD1843_ADC_SRC_GATTN      2  /* select adc source, gain&attn */
#define AD1843_DAC2_MIX           3  /* mix controls more main mix to DAC2 */
#define AD1843_AUX1_MIX           4
#define AD1843_AUX2_MIX           5
#define AD1843_AUX3_MIX           6
#define AD1843_MIC_MIX            7
#define AD1843_MONO_MIX_MISC    8  /* select mono in and misc mixing */
#define AD1843_DAC1_GATTN  9  /* gain/attenuation for DAC1 */
#define AD1843_DAC2_GATTN  10
#define AD1843_DAC1_DIGITAL_ATTEN  11  /* digital only attenuation for DAC1 */
#define AD1843_DAC2_DIGITAL_ATTEN  12  /* digital only attenuation for DAC2 */
#define AD1843_ADC_DAC1_MIX       13 /* ADC to DAC1 digital mixing */
#define AD1843_ADC_DAC2_MIX       14 /* ADC to DAC2 digital mixing */
#define AD1843_CLK_SRC_SELECT     15  /* select rate clock source */
#define AD1843_CLK_GEN1_MODE      16 /* cntrl mode: video lck, PLL gain, etc */
#define AD1843_CLK_GEN1_RATE      17 /* sample rate */
#define AD1843_CLK_GEN1_PHASE     18  /* phase shift sample clk */
#define AD1843_CLK_GEN2_MODE      19 /* cntrl mode: video lck, PLL gain, etc */
#define AD1843_CLK_GEN2_RATE      20 /* sample rate */
#define AD1843_CLK_GEN2_PHASE     21  /* phase shift sample clk */
#define AD1843_CLK_GEN3_MODE      22 /* cntrl mode: video lck, PLL gain, etc */
#define AD1843_CLK_GEN3_RATE      23 /* sample rate */
#define AD1843_CLK_GEN3_PHASE     24  /* phase shift sample clk */
#define AD1843_FILTER_MODE        25  /* digital filter mode */
#define AD1843_SERIAL             26 /* serial interface and sampl format */
#define AD1843_CH_POWERDOWN       27 /* channel power down */
#define AD1843_CONFIG             28 /* converter pwr dwn, clk out enables */
#define AD1843_RESERVED_29        29
#define AD1843_RESERVED_30        30
#define AD1843_RESERVED_31        31

#define NUM_CODEC_REGS 32

/*****************
 * register bits *
 *****************/
/* reg 0- codec status revision identification register-  AD1843_STAT_REV */
#define PDNO       (1<<14)      /* conversion power down flag */
#define INIT (1<<15)      /* clock initialization flag */
#define REV   0xf

/* reg 1- channel status flags- AD1843_CH_STAT */
#define DAC1_UNDERRUN        (1<<8)  /* DAC1 sample underrun */
#define SU1                  DAC1_UNDERRUN  /* spec name */
#define DAC2_UNDERRUN        (1<<9)  /* DAC2 sample underrun */
#define SU2                  DAC2_UNDERRUN   /* spec name */
#define ADCR_OVERRANGE       (3<<2)  /* ADC right overrange detect */
#define OVR1                 (1<<3)  /* spec name */
#define OVR0                 (1<<2)  /* spec name */
#define ADCL_OVERRANGE        3     /* ADC left overrange detect */
#define OVL1                 (1<<1)  /* spec name */
#define OVL0                  1  /* spec name */

/* reg 2- input control = AD1843_ADC_SRC_GATTN */
#define LEFT_ADC_SRC_MASK      (7<<13)
#define LEFT_ADC_SRC_LINE      (0<<13)
#define LEFT_ADC_SRC_MIC       (1<<13)
#define LEFT_ADC_SRC_AUX1      (2<<13)
#define LEFT_ADC_SRC_AUX2      (3<<13)
#define LEFT_ADC_SRC_AUX3      (4<<13)
#define LEFT_ADC_SRC_MONO      (5<<13)
#define LEFT_ADC_SRC_DAC1      (6<<13)
#define LEFT_ADC_SRC_DAC2      (7<<13)
#define LEFT_ADC_SRCS          (7<<13)
#define LEFT_ADC_MIC_GAIN_EN  (1<<12)
#define SET_LEFT_ADC_GATTN(x,g)   (((x) & 0xf0ff)|(((g) & 0xf)<<8))
#define RIGHT_ADC_SRC_MASK      (7<<5)
#define RIGHT_ADC_SRC_LINE      (0<<5)
#define RIGHT_ADC_SRC_MIC       (1<<5)
#define RIGHT_ADC_SRC_AUX1      (2<<5)
#define RIGHT_ADC_SRC_AUX2      (3<<5)
#define RIGHT_ADC_SRC_AUX3      (4<<5)
#define RIGHT_ADC_SRC_MONO      (5<<5)
#define RIGHT_ADC_SRC_DAC1      (6<<5)
#define RIGHT_ADC_SRC_DAC2      (7<<5)
#define RIGHT_ADC_SRCS           (7<<5)
#define RIGHT_ADC_MIC_GAIN_EN  (1<<4)
#define SET_RIGHT_ADC_GATTN(x,g)   (((x) & 0xfff0)|((g) & 0xf))
#define SET_ADC_GATTN(x,g)  (((x) & 0xf0f0)|((g) & 0xf)|(((g) & 0xf)<<8))
#define ADC_SRC_SELECT (LEFT_ADC_SRC_MASK | RIGHT_ADC_SRC_MASK)

/* reg 3- DAC2 to mixer control- AD1843_DAC2_MIX */
#define LEFT_DAC2_MIX_MUTE           (1<<15)
#define LEFT_DAC2_MIX_GAIN_ATTN(x)   ((0x1f & (x)) << 8)  /* 01000 == 0.0db */
#define RIGHT_DAC2_MIX_MUTE          (1<<7)
#define RIGHT_DAC2_MIX_GAIN_ATTN(x)  (0x1f & (x))  /* 01000 == 0.0db */

/* reg 4- aux1 to mixer control- AD1843_AUX1_MIX */
#define LEFT_AUX1_MIX_MUTE           (1<<15)
#define LEFT_AUX1_MIX_GAIN_ATTN(x)   ((0x1f & (x)) << 8)  /* 01000 is 0.0db */
#define RIGHT_AUX1_MIX_MUTE          (1<<7)
#define RIGHT_AUX1_MIX_GAIN_ATTN(x)  (0x1f & (x))  /* 01000 is 0.0db */

/* reg 5- aux2 to mixer control- AD1843_AUX2_MIX */
#define LEFT_AUX2_MIX_MUTE           (1<<15)
#define LEFT_AUX2_MIX_GAIN_ATTN(x)   ((0x1f & (x)) << 8)  /* 01000 is 0.0db */
#define RIGHT_AUX2_MIX_MUTE          (1<<7)
#define RIGHT_AUX2_MIX_GAIN_ATTN(x)  (0x1f & (x))  /* 01000 is 0.0db */

/* reg 6- aux3 to DAC1 mixer control- AD1843_AUX3_MIX */
#define LEFT_AUX3_MIX_MUTE           (1<<15)
#define LEFT_AUX3_MIX_GAIN_ATTN(x)   ((0x1f & (x)) << 8)  /* 01000 is 0.0db */
#define RIGHT_AUX3_MIX_MUTE          (1<<7)
#define RIGHT_AUX3_MIX_GAIN_ATTN(x)  (0x1f & (x))  /* 01000 is 0.0db */

/* reg 7- mic to DAC1 mixer control- AD1843_MIC_MIX */
#define LEFT_MIC_MIX_MUTE           (1<<15)
#define LEFT_MIC_MIX_GAIN_ATTN(x)   ((0x1f & (x)) << 8)  /* 01000 is 0.0db */
#define RIGHT_MIC_MIX_MUTE          (1<<7)
#define RIGHT_MIC_MIX_GAIN_ATTN(x)  (0x1f & (x))  /* 01000 is 0.0db */

/* reg 8- mono in to DAC1 mixer and misc controls -AD1843_MONO_MIX_MISC */
#define MONO_MIX_MUTE               (1 << 15)
#define MONO_MIX_GAIN_ATTN(x)       ((0x1f & (x)) << 8) /* 01000 is 0.0db */
#define ALL_MIX_MUTE                (1<<7)  /* mic, aux? and dac2 mix mute */
#define MONO_OUTPUT_MUTE            (1<<6)
#define HEADPHONE_MUTE              (1<<5)
#define HEADPHONE_4VOLT_SWING       (1<<4)
#define SUM_MIX_MUTE                     (1<<3)
/* this allows AD1843 to pass thru from mon in to mono out when powered down */
#define MONO_PASSTHRU_MUTE               (1<<2)
/* when gain/attn changes take effect.  zero crossing reduces "clicks" */
#define DAC2_GAIN_ATTN_CHANGE_TIMING(x)  ((x) << 1)
#define DAC1_GAIN_ATTN_CHANGE_TIMING(x)  ((x) << 0)
#define CHNG_AT_ZERO_CROSSING 0
#define CHNG_IMMEDIATE        1

/* reg 9- output- DAC1 analog gain/attn- AD1843_DAC1_GATTN */
/* reg 10- output- DAC2 analog gain/attn- AD1843_DAC2_GATTN */
#define LEFT_DAC_ANALOG_MUTE            (1<<15)
#define SET_LEFT_DAC_GATTN(x,g)   (((x) & 0xc0ff)|(((g) & 0x3f)<<8))
#define GET_LEFT_DAC_GATTN(x)     (((x) & 0x3f00) >> 8)
#define RIGHT_DAC_ANALOG_MUTE           (1<<7)
#define SET_RIGHT_DAC_GATTN(x,g)   (((x) & 0xffc0)|((g) & 0x3f))
#define GET_RIGHT_DAC_GATTN(x)   ((x) & 0x3f)
#define SET_DAC_GATTN(x,g)  (((x) & 0xc0c0)|(((g) & 0x3f)<<8)|((g) & 0x3f))
#define MIN_VOLUME  0x3f     /* lower limit */
#define MAX_VOLUME  0       /* upper limit */

/* reg 11- output- DAC1 digital attn- AD1843_DAC1_DIGITAL_ATTN */
/* reg 12- output- DAC1 digital attn- AD1843_DAC2_DIGITAL_ATTN */
#define LEFT_DAC_DIGITAL_MUTE            (1<<15)
#define LEFT_DAC_DIGITAL_ATTN(x)         ((0x3f & (x)) << 8)
#define RIGHT_DAC_DIGITAL_MUTE           (1<<7)
#define RIGHT_DAC_DIGITAL_ATTN(x)        (0x3f & (x))

/* reg 13- digital ADC to DAC1 mix control- AD1843_ADC_DAC1_MIX */
#define LEFT_ADC_DAC1_DIGITAL_MIX_MUTE       (1<<15)
#define LEFT_ADC_DAC1_DIGITAL_MIX_ATTN(x)    ((0x3f & (x)) << 8)
#define RIGHT_ADC_DAC1_DIGITAL_MIX_MUTE       (1<<7)
#define RIGHT_ADC_DAC1_DIGITAL_MIX_ATTN(x)    (0x3f & (x))

/* reg 14- digital ADC to DAC2 mix control- AD1843_ADC_DAC2_MIX */
#define LEFT_ADC_DAC2_DIGITAL_MIX_MUTE       (1<<15)
#define LEFT_ADC_DAC2_DIGITAL_MIX_ATTN(x)    ((0x3f & (x)) << 8)
#define RIGHT_ADC_DAC2_DIGITAL_MIX_MUTE       (1<<7)
#define RIGHT_ADC_DAC2_DIGITAL_MIX_ATTN(x)    (0x3f & (x))

/* reg 15- codec config, channel sample rate select- AD1843_CLK_SRC_SELECT */
#define DAC2_CLKSRC_48000            (0<<10)
#define DAC2_CLKSRC_CLK1           (1<<10)
#define DAC2_CLKSRC_CLK2           (2<<10)
#define DAC2_CLKSRC_CLK3           (3<<10)
#define DAC2_CLKSRC_MASK             (3<<10)  /* mask */
#define DAC1_CLKSRC_48000            (0<<8)
#define DAC1_CLKSRC_CLK1           (1<<8)
#define DAC1_CLKSRC_CLK2           (2<<8)
#define DAC1_CLKSRC_CLK3           (3<<8)
#define DAC1_CLKSRC_MASK             (3<<8)  /* mask */
#define DAC_CLKSRC_MASK            (DAC1_CLKSRC_MASK|DAC2_CLKSRC_MASK)
#define DAC_CLKSRC_48000           (DAC2_CLKSRC_48000|DAC1_CLKSRC_48000)
#define DAC_CLKSRC_CLK1            (DAC2_CLKSRC_CLK1|DAC1_CLKSRC_CLK1)
#define DAC_CLKSRC_CLK2            (DAC2_CLKSRC_CLK2|DAC1_CLKSRC_CLK2)
#define DAC_CLKSRC_CLK3            (DAC2_CLKSRC_CLK3|DAC1_CLKSRC_CLK3)
#define LEFT_ADC_CLKSRC_48000           (0<<2)
#define LEFT_ADC_CLKSRC_CLK1         (1<<2)
#define LEFT_ADC_CLKSRC_CLK2         (2<<2)
#define LEFT_ADC_CLKSRC_CLK3         (3<<2)
#define RIGHT_ADC_CLKSRC_48000           0
#define RIGHT_ADC_CLKSRC_CLK1         1 
#define RIGHT_ADC_CLKSRC_CLK2         2
#define RIGHT_ADC_CLKSRC_CLK3         3
#define ADC_CLKSRC_48000           0
#define ADC_CLKSRC_CLK1         (1<<2|1) 
#define ADC_CLKSRC_CLK2         (2<<2|2) 
#define ADC_CLKSRC_CLK3         (3<<3|3)
#define ADC_CLKSRC_MASK              0xf
#define GET_ADC_CLKSRC(x)       ((x) & 0x3)
#define GET_DAC1_CLKSRC(x)      (((x)>>8) & 0x3)
#define GET_DAC2_CLKSRC(x)      (((x)>>10) & 0x3)

/* reg 16- clock generator 1 control- AD1843_CLK_GEN1_MODE */
/* reg 19- clock generator 2 control- AD1843_CLK_GEN2_MODE */
/* reg 22- clock generator 3 control- AD1843_CLK_GEN3_MODE */
#define CREF_SYNC        (1<<15)  /* reference clk gen1 off SYNC pin */
#define CVID_LOCK        (1<<14)  /* vid lock mode */
#define CPLL_GAIN        (1<<13)  /* 0=finite PLL loop gain, 1=infinite
                                    * better lock to sync abilty, but
                                    * more conversion noise */
#define C_X87_FREQ_MOD   (1<<11) /* conversion clk generated will be 8/7 times
				   * value in AD1843_CLOCK_GEN1_RATE reg.
				   * ignored when in SYNC1 mode */


/*
 * reg 17- clock generator 1 sample rate- AD1843_CLK_GEN1_RATE
 * reg 20- clock generator 2 sample rate- AD1843_CLK_GEN2_RATE
 * reg 23- clock generator 3 sample rate- AD1843_CLK_GEN3_RATE
 *    defines sample rate when not in SYNC mode. LSB==1HZ
 */
#define MIN_SAMPLE_RATE  0x0fa0    /* 4Khz */
#define MAX_SAMPLE_RATE  0xd2f0    /* 54kHz */

/* reg 18- clock generator 1 sample phase shift- AD1843_CLK_GEN1_PHASE */
/* reg 21- clock generator 2 sample phase shift- AD1843_CLK_GEN2_PHASE */
/* reg 24- clock generator 3 sample phase shift- AD1843_CLK_GEN3_PHASE */
#define C_PHASE_RETARD  (1<<8)  /* direction of samp clk phase shift */
#define C_PHASE_SHIFT(x) ((x) & 0xf) /* magnitude of phase shift. 
					 LSB=0.12 degrees or 3.072 MHz */

/* reg25 - digital filer mode select- AD1843_FILTER_MODE */
/* DAC2 is sacrificed so remaining 4 channels have sufficient resources
 * to realize more strigent filter requirements. */
#define DIGITAL_RESAMPLE_FILTER_MODE  (1<<15)
#define DAC_DAC_DMIX        (1<<14)
#define DIG_RT_ADC_A (0<<6)
#define DIG_RT_ADC_DAC1   (2<<6)
#define DIG_RT_ADC_DAC2   (3<<6)
#define DIG_LT_ADC_A (0<<4)
#define DIG_LT_ADC_DAC1   (2<<4)
#define DIG_LT_ADC_DAC2   (3<<4)
#define DIG_ADC_SRC    (0xf<<4)

/* reg 26- codec serial config- AD1843_SERIAL */
#define DAC2_MONO_MODE               (1<<15)  /* (0<<15) is stereo */
#define DAC1_MONO_MODE               (1<<14)  /* (0<<14) is stereo */
#define DAC2_8BIT_UNSIGNED_PCM       (0<<10)  /* 8 bit unsigned linear PCM */
#define DAC2_16BIT_SIGNED_PCM        (1<<10)  /* 16 bit unsigned linear PCM */
#define DAC2_8BIT_ULAW               (2<<10)  /* 8 bit u-law */
#define DAC2_8BIT_ALAW               (3<<10)  /* 8 bit a-law */
#define DAC1_8BIT_UNSIGNED_PCM       (0<<8)  /* 8 bit unsigned linear PCM */
#define DAC1_16BIT_SIGNED_PCM        (1<<8)  /* 16 bit unsigned linear PCM */
#define DAC1_8BIT_ULAW               (2<<8)  /* 8 bit u-law */
#define DAC1_8BIT_ALAW               (3<<8)  /* 8 bit a-law */
#define SCLK_16MHZ                 (1<<7)  /* 16.384 MHZ, default 12.288 */
#define FRAME_SIZE_16_SLOTS          (1<<6)  /* 16 slots per frame. def 32 */
#define FRAME_SIZE_CHANGE_NOW        (1<<5) /* make frame size chng immediate*/
				           /* 0 = wait until cur frame done */
#define ADC_SAMP_LCK            (1<<4) /* lock output of ADC samples */
#define RIGHT_ADC_8BIT_UNSIGNED_PCM (0<<2)  /* 8 bit unsigned linear PCM */
#define RIGHT_ADC_16BIT_SIGNED_PCM  (1<<2)  /* 16 bit unsigned linear PCM */
#define RIGHT_ADC_8BIT_ULAW         (2<<2)  /* 8 bit u-law */
#define RIGHT_ADC_8BIT_ALAW         (3<<2)  /* 8 bit a-law */
#define LEFT_ADC_8BIT_UNSIGNED_PCM   0  /* 8 bit unsigned linear PCM */
#define LEFT_ADC_16BIT_SIGNED_PCM    1  /* 16 bit unsigned linear PCM */
#define LEFT_ADC_8BIT_ULAW           2  /* 8 bit u-law */
#define LEFT_ADC_8BIT_ALAW           3  /* 8 bit a-law */

/* reg 27- channel power down- AD1843_CH_POWERDOWN */
/* free resource to be transferable elsewhere */
#define DIGITAL_RESOURCE_FREE       (1<<15)
#define DAC2_TO_DAC1_MIX_ENABLE     (1<<12)  /* pwr down dac-to-dac mix */
#define DAC2_ENABLE                 (1<<9)
#define DAC1_ENABLE                 (1<<8)
/* r9-10 reset to 0 */
#define ANALOG_ENABLE       (1<<7) /* enable analog circuitry */
#define HEADPHONE_ENABLE            (1<<6)
/* analog input to analog output powerdown r4-7 and half r8== 0 */
#define ANALOG_MIX_ENABLE (1<<4)
#define ADC_R_ENABLE               (1<<1)
#define ADC_L_ENABLE                1

/* reg 28- codec configuration, output clk enable- AD1843_CONFIG */
#define PDNI  (1<<15)
#define AUTOCALIBRATE      (1<<14)   /* enable auto calibration */
#define CLK_GEN3_ENABLE     (1<<13)  /* enable clock generator 3 */
#define CLK_GEN2_ENABLE     (1<<12)  /* enable clock generator 2 */
#define CLK_GEN1_ENABLE     (1<<11)  /* enable clock generator 1 */
#define CLK_OUTPUT_ENABLE   (1<<10)
#define XCTL1_LOGIC_HI        (1<<9)  /* xctl1 pin logic1, def=logic0 */
#define XCTL0_LOGIC_HI        (1<<8)  /* xctl0 pin logic1, def=logic0 */
#define CONV3_ENABLE          (1<<7)
#define BIT3_ENABLE           (1<<6)
#define CONV2_ENABLE          (1<<5)
#define BIT2_ENABLE           (1<<4)
#define CONV1_ENABLE          (1<<3)
#define BIT1_ENABLE           (1<<2)
