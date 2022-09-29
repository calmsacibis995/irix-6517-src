#ident	"IP12prom/hello_tune.c:  $Revision: 1.22 $"

/*
 * hello_tune.c
 */

#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "sys/invent.h"
#include "sys/hal2.h"
#include "sys/pbus.h"
#include "arcs/hinv.h"
#include "libsk.h"
#include "libsc.h"

#include "adpcm.h"

static void waitfordma(int, int, int);
static char *grab_mem(int);

#define CODECA_DMA_CHAN 1
#define AESTX_DMA_CHAN  2

static COMPONENT hal2tmpl = {
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
 * Need three tunes. Defined in the *.tune files along with their 
 * accompanying lengths.
 */
#if	IP24
#define	starttune	IP24starttune
#define	starttune_size	IP24starttune_size
#define	stoptune	IP24stoptune
#define	stoptune_size	IP24stoptune_size
#define	badgfxtune	IP24badgfxtune
#define	badgfxtune_size	IP24badgfxtune_size

#else	/* IP22 */
#define	starttune	IP22starttune
#define	starttune_size	IP22starttune_size
#define	stoptune	IP22stoptune
#define	stoptune_size	IP22stoptune_size
#define	badgfxtune	IP22badgfxtune
#define	badgfxtune_size	IP22badgfxtune_size
#endif

#ifndef	DEBUG
extern unsigned char starttune[]; 
extern int           starttune_size;
extern unsigned char stoptune[]; 
extern int           stoptune_size;
extern unsigned char badgfxtune[]; 
extern int           badgfxtune_size;
#endif

struct adpcm_state state;

#ifndef MIN
#define MIN(a,b) ( (a) < (b) ? (a) : (b) )
#endif

void
audio_install(COMPONENT *p)
{
    unsigned short rev;
    int vers;

    rev = *(unsigned int *)PHYS_TO_K1(HAL2_REV);
    if (rev & 0x8000)
	return;

    p = AddChild(p,&hal2tmpl,0);
    if (p == (COMPONENT *)NULL) cpu_acpanic("audio");
    p->Key = rev;
}

#define SPIN while (*isrp & HAL2_ISR_TSTATUS);

#define HAL2_AESTX_DMA_ENABLE    0x4
#define HAL2_CODECA_DMA_ENABLE   0x8

#if ENETBOOT || !defined(NOBOOTTUNE)
static void
SetupHAL2(void)
{
    volatile unsigned int *idr0p=(unsigned int *)PHYS_TO_K1(HAL2_IDR0);
    volatile unsigned int *idr1p=(unsigned int *)PHYS_TO_K1(HAL2_IDR1);
    volatile unsigned int *idr2p=(unsigned int *)PHYS_TO_K1(HAL2_IDR2);
    volatile unsigned int *idr3p=(unsigned int *)PHYS_TO_K1(HAL2_IDR3);
    volatile unsigned int *iarp=(unsigned int *)PHYS_TO_K1(HAL2_IAR);
    volatile unsigned int *isrp=(unsigned int *)PHYS_TO_K1(HAL2_ISR);
    int inc, mod, modctrl;
    
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
    volatile unsigned int *idr0p=(unsigned int *)PHYS_TO_K1(HAL2_IDR0);
    volatile unsigned int *iarp=(unsigned int *)PHYS_TO_K1(HAL2_IAR);
    volatile unsigned int *isrp=(unsigned int *)PHYS_TO_K1(HAL2_ISR);
    
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
    volatile pbus_control_write_t * ctrlptr=
	(pbus_control_write_t*)PHYS_TO_K1(HPC3_PBUS_CONTROL(channel));
    volatile unsigned int *dpptr=
	(unsigned int *)PHYS_TO_K1(HPC3_PBUS_DP(channel));

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

    *dpptr=K1_TO_PHYS(desc_list);
    *ctrlptr=ctrlval;
}

static void
BuildDescriptors(scdescr_t *list,void *startaddr,int nbytes)
{
    int nremaining, bytes_till_page_boundary, bytes_this_descriptor;
    char *p;
    scdescr_t *desc;

    desc=list;
    nremaining=nbytes;
    p=(char *)startaddr;
    while(nremaining) {
	desc->cbp = K1_TO_PHYS(p);

        bytes_till_page_boundary = 4096-((__psunsigned_t)p & 0xfff);
        bytes_this_descriptor = MIN(bytes_till_page_boundary,nremaining);
        desc->bcnt = bytes_this_descriptor;

	desc->eox=0;
	desc->pad1=0;
	desc->xie=0;
	desc->pad0=0;

        desc->nbp = K1_TO_PHYS(desc+1);
        desc->word_pad = 0;
        nremaining -= bytes_this_descriptor;
	p+= bytes_this_descriptor;
	desc++;
    }
    desc--;
    desc->nbp = 0;
    desc->eox = 1;
}

#define MAX_DESCRIPTORS 150
scdescr_t  *tune_desc_list;
static char *grab_loc;

void 
play_hello_tune(int tune_type)
/*
 * tune_type:   0=hello, 1=goodbye, 2=graphics failure
 */
{
    int samples;
    int volume;
    volatile unsigned int *revptr=(unsigned int *)PHYS_TO_K1(HAL2_REV);
    unsigned short rev;
    int *tunebuffer;
    int codelen;
    void end_adpcm();
    void (*decodep)(unsigned char *, int *, int, struct adpcm_state *);
    unsigned char *soundbuf;        /* the compressed sample to play */
    int soundbuf_size;    /* and the number of bytes in soundbuf */
    volatile unsigned int *aestx_cr2 = (unsigned int *)PHYS_TO_K1(AESTX_CR2);
    volatile unsigned int *aestx_cr1 = (unsigned int *)PHYS_TO_K1(AESTX_CR1);
    volatile unsigned int *aestx_cr3 = (unsigned int *)PHYS_TO_K1(AESTX_CR3);
    char *volstr;
    
    /*
     * Should not play the boottune at all if:
     *   1. there is no audio
     *   2. the volume parameter is equal to 0
     */
    grab_loc = (char *)PHYS_TO_K1(PROM_STACK+16);
    tunebuffer = (int *)grab_mem(2000000);
    tune_desc_list = (scdescr_t*)grab_mem(MAX_DESCRIPTORS*sizeof(scdescr_t));
    rev = *revptr;
    if (rev & 0x8000)
	return;
    
    volstr = cpu_get_nvram_offset(NVOFF_VOLUME,NVLEN_VOLUME);
    atob(volstr,&volume);
    if (volume == 0) 
	return;

#ifndef	DEBUG
    switch(tune_type) {
	case 0:
	    soundbuf = starttune;
	    soundbuf_size = starttune_size;
	    break;
	case 1:
	    soundbuf = stoptune;
	    soundbuf_size = stoptune_size;
	    break;
	case 2:
            waitfordma(CODECA_DMA_CHAN, AESTX_DMA_CHAN, 3000);
	    soundbuf = badgfxtune;
	    soundbuf_size = badgfxtune_size;
	    break;
    }
    
    /* Set up the HAL2 part */
    SetupHAL2();

    /* Set the volume of the audio system  */

    *((unsigned int *)(PHYS_TO_K1(VOLUME_LEFT)))  = volume;
    *((unsigned int *)(PHYS_TO_K1(VOLUME_RIGHT))) = volume;

    /*
     * Determine the length of the ADPCM code and then copy this
     * to memory for faster execution. Also set the pointer to this
     * executable code in memory. This pointer is called below to 
     * perform the ADPCM algorithm.
     */
    codelen = (char *)end_adpcm - (char *)adpcm_decoder;
    decodep = (void (*)())malloc(sizeof(char)*codelen);
    bcopy((void *)adpcm_decoder, (void *)decodep, codelen);
    
    samples=soundbuf_size*2;
    decodep(soundbuf,tunebuffer,samples,&state);
    BuildDescriptors(tune_desc_list, tunebuffer, samples*sizeof(int));
    
    /* 
     * Do the last bit of HAL2 work to get samples out
     * (throw the relay and enable the DMA from the HAL2 side).
     */
    EnableOutput();

    InitiateDMA(tune_desc_list, 0, CODECA_DMA_CHAN);
    InitiateDMA(tune_desc_list, 0, AESTX_DMA_CHAN);

    /*
     * if we are playing the bad graphics or the shutdown tune,
     * we need to hang around and wait for it to finish
     */
    if (tune_type == 1 || tune_type == 2) {
        waitfordma(CODECA_DMA_CHAN, AESTX_DMA_CHAN, 3000);
    }
#endif
}

void
wait_hello_tune()
{
	waitfordma(CODECA_DMA_CHAN, AESTX_DMA_CHAN, 3000);
}


/***********************************************************
Copyright 1992 by Stichting Mathematisch Centrum, Amsterdam, The
Netherlands.

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the names of Stichting Mathematisch
Centrum or CWI not be used in advertising or publicity pertaining to
distribution of the software without specific, written prior permission.

STICHTING MATHEMATISCH CENTRUM DISCLAIMS ALL WARRANTIES WITH REGARD TO
THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
FITNESS, IN NO EVENT SHALL STICHTING MATHEMATISCH CENTRUM BE LIABLE
FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

******************************************************************/

/*
** Intel/DVI ADPCM coder/decoder.
**
** The algorithm for this coder was taken from the IMA Compatability Project
** proceedings, Vol 2, Number 2; May 1992.
**
** Version 1.1, 16-Dec-92.
**
** Change log:
** - Fixed a stupid bug, where the delta was computed as
**   stepsize*code/4 in stead of stepsize*(code+0.5)/4. The old behavior can
**   still be gotten by defining STUPID_V1_BUG.
*/

#ifndef __STDC__
#define signed
#endif

/* Intel ADPCM step variation table */
static int prom_indexTable[16] = {
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8,
};

static int prom_stepsizeTable[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17,
    19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
    50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
    130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
    337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
    876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
    2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
    5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
};

int *stepsizeTable, *indexTable;

void
adpcm_decoder(unchar *indata, int *outdata, int len,
	      struct adpcm_state *state)
{
    int delta;		/* Current adpcm output value */
    int code;		/* Current adpcm output value */
    int step;		/* Stepsize */
    int valprev;	/* virtual previous output value */
    int vpdiff;		/* difference between adjacent output samps */
    int index;		/* Current step change index */
    int indexdelta;
    unsigned int inputbuffer;	/* place to keep next 4-bit value */
    void (*bcopyp)(const void *, void *, int);
    void* (*mallocp)(size_t);
    
    /*
     * Create area in memory for the tables used by the ADPCM
     * algorithm. Then copy the values from the tables in the
     * prom to memory. Bcopy cannot be used since we must do a 
     * jump instruction. Thus, we must call bcopy via a pointer
     * to the function.
     */
    bcopyp = bcopy;
    mallocp = malloc;
    stepsizeTable = (int *) mallocp(sizeof(prom_stepsizeTable));
    indexTable = (int *) mallocp(sizeof(prom_indexTable));
    bcopyp(prom_stepsizeTable, stepsizeTable, sizeof(prom_stepsizeTable));
    bcopyp(prom_indexTable, indexTable, sizeof(prom_indexTable));

    valprev = state->valprev;
    index = state->index;
    step = stepsizeTable[index];

    len /= 2;
    
    for ( ; len > 0 ; len-- ) {
	
	/* Step 1 - get the code value */
        inputbuffer = *indata++;
	code = (inputbuffer >> 4);

	/* Step 2 - Find new index value (for later) */
	indexdelta = indexTable[code];
	index += indexdelta;
        if( (unsigned)index> 88) {
	    if ( index < 0 ) index = 0;
	    else if ( index > 88 ) index = 88;
        }

	/* Step 3 - compute the magnitude */
	delta = code & 7;

	/* Step 4 - update and clamp the output value */
	vpdiff = ((delta*step) >> 2) + (step >> 3);
	if ( delta != code ) {
	    valprev -= vpdiff;
	    if ( valprev < -32768 ) valprev = -32768;
	} else {
	    valprev += vpdiff;
	    if ( valprev > 32767 ) valprev = 32767;
        }

	/* Step 6 - Update step value */
	step = stepsizeTable[index];

	/* Step 7 - Output value */
	*outdata++ = valprev<<8;

	/* Step 1 - get the delta value and compute next index */
	code = inputbuffer & 0xf;

	/* Step 2 - Find new index value (for later) */
	indexdelta = indexTable[code];
	index += indexdelta;
        if( (unsigned)index> 88) {
	    if ( index < 0 ) index = 0;
	    else if ( index > 88 ) index = 88;
        }

	/* Step 3 - compute the magnitude */
	delta = code & 7;

	/* Step 4 - update and clamp the output value */
	vpdiff = ((delta*step) >> 2) + (step >> 3);
	if ( delta != code ) {
	    valprev -= vpdiff;
	    if ( valprev < -32768 ) valprev = -32768;
	} else {
	    valprev += vpdiff;
	    if ( valprev > 32767 ) valprev = 32767;
        }

	/* Step 6 - Update step value */
	step = stepsizeTable[index];

	/* Step 7 - Output value */
	*outdata++ = valprev<<8;
    }

    state->valprev = valprev;
    state->index = index;
}

void
end_adpcm()
{
}

static char *
grab_mem(int nlocs)
{
    char *loc = grab_loc;

    grab_loc += nlocs;
    return loc;
}

static void
waitfordma(int chan1, int chan2, int maxtimes)
/*
 * maxtimes = milliseconds to wait for the two DMA streams
 */
{
    volatile pbus_control_read_t* ctrlptr1=
        (pbus_control_read_t*)PHYS_TO_K1(HPC3_PBUS_CONTROL(chan1));
    volatile pbus_control_read_t* ctrlptr2=
        (pbus_control_read_t*)PHYS_TO_K1(HPC3_PBUS_CONTROL(chan2));
    pbus_control_read_t ctrlval1;
    pbus_control_read_t ctrlval2;
    int ntimes;

    for(ntimes=0;ntimes<maxtimes;ntimes++) {
        ctrlval1 = *ctrlptr1;
        ctrlval2 = *ctrlptr2;
        if (!ctrlval1.ch_act && !ctrlval2.ch_act)
		return;
        us_delay(1000);
    }
#if DEBUG
    printf("warning: waitfordma failed!\n");
#endif
}
#else
void wait_hello_tune() {}
void play_hello_tune(int tune_type) {}
#endif
