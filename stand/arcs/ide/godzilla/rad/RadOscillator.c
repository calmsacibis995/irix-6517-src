/*********************************************************************
 *                                                                   *
 *  Title :  RadOscillator   					     *
 *  Author:  Jimmy Acosta					     *
 *  Date  :  03.18.96                                                *
 *                                                                   *
 *********************************************************************
 *                                                                   *
 *  Description:                                                     *
 *  								     *
 *      Routines for generating sine waves for bringup and           *
 *      diagnostics.                                                 *
 *								     *
 *  (C) Copyright 1996 by Silicon Graphics, Inc.                     *	
 *  All Rights Reserved.                                             *
 *********************************************************************/
#include "ide_msg.h"
#include "uif.h"

#include "sys/types.h"
#include <sgidefs.h>
#include "sys/sbd.h"

#include "d_rad_util.h"
/* 
 * const definitions 
 */

#define PI 3.14159265358979323846

#define COS48_2  1.999999982865270100
#define SIN48  0.000130899693525753
#define COS441_2 1.99999997970063
#define SIN441 0.00014247585682
#define COS32_2  1.99999996144686
#define SIN32  0.00019634953959


static double* oscRefTable;
uint_t oscRefTableSize;

/*
 * RadOscillator: Will generate a sine wave with a freq defined by freq, 
 * a sample rate defined by samplefreq, and an amplitude defined by amp.
 * It has the following restrictions: 
 * 1. values for freq will be multiples of one. (to avoid interpolation)
 * 2. values for sampleFreq will match the following list (in kHz):
 * 4,8,11.025,16,22.05,24,32,44.1  and 48
 * 3. The amplitude shall be no larger than 2^24
 * 
 * Memory allocation of table should be done by the calling routine.  The
 * sine wave values have a measured error of 2^-18, or more or less 2 x 10^-6
 */

int RadOscillatorInit(uint_t sf) 
{ 

    uint_t refbufsize;
    double *reftable=NULL, *refsftable=NULL;
    double b1, wosc, out, sinw;
    double z1, z2;
    int factor = 1;
    int i;
    int incr, chunkSize;
    
    msg_printf(DBG,"RadOscillatorInit: sampling frequency %d \n",sf);

    /* 
     * Determine the sampling frequency factor if not 48000,
     * 44100 or 32000
     */

    if(!(48000 % sf)) {
	b1 = COS48_2;  
	z1 = SIN48;  
	refbufsize = 48000;
	factor = 48000/sf;
    } else if(!(44100 %sf)) {
 	b1 = COS441_2; 
 	z1 = SIN441; 
	refbufsize = 44100;
	factor = 44100/sf;
    } else if(sf == 32000) {
 	b1 = COS32_2; 
 	z1 = SIN32; 
	refbufsize = 32000;
	factor = 32000/sf;
    } else {
	msg_printf(SUM,"Unsopported sampling frequency.\n");
	return -1;
    }	      

    /*
     * Get memory to put sample oscillator table
     */

    if(factor != 1) 
	chunkSize = refbufsize + refbufsize/factor;
    else
	chunkSize = refbufsize;
	
    reftable = (double *)get_chunk(chunkSize*sizeof(double));
    if(reftable == NULL) {
	msg_printf(SUM,"Problems allocating memory for reference table \n");
	return -1;
    }

    /*
     * Generate reference 1 Hz sine wave table using a
     * second order IIR filter with poles in the unit circle.
     */

    
    z2 = 0;  

    reftable[0] = 0;
    reftable[1] = z1;

    for(i=2; i < refbufsize; i++) {
 	reftable[i] = z1*b1 - z2; 
	z2 = z1;
	z1 = reftable[i];
    }
    
    
    oscRefTableSize = refbufsize/factor;
    msg_printf(DBG,"oscRefTableSize = %d\n",oscRefTableSize);

    incr = 0;
    if(factor != 1) {
	msg_printf(DBG,"RadOscillatorInit: changing sample rate factor:  %d",factor);

	refsftable = reftable + refbufsize;
	
	if(refsftable == NULL) {
	    msg_printf(SUM,"Problems allocating memory for reference table \n");
	    return -1;
	}
	refsftable[0] = 0;
	for(i=0; i< oscRefTableSize; i++) {

	    refsftable[i] = reftable[incr];
	    incr += factor;
	}
	oscRefTable = refsftable;
    }  
    else 
	oscRefTable = reftable;


    return 0;
}


int RadOscillator(int *table,        /* Space in memory to write wave data */
		  uint_t size,       /* size of memory chunk               */
		  uint_t channels,   /* Number of channels for the sine    */
		  uint_t freq,       /* Sine wave frequency                */
		  uint_t amp         /* Amplitude of the wave              */
		  )
{ 
   
    int i,j;
    uint_t shift, amplitude;
    int incr = 0;
    int tmp = 1 << (amp-1);

    msg_printf(DBG,"RadOScillator: channels: %d, freq: %d amplitude: %d\n",
	       channels,freq,amp);

    if(oscRefTable == NULL || oscRefTableSize <= 0) {
	msg_printf(SUM,"Invalid reference information\n");
	msg_printf(DBG,"oscRefTable = %x oscRefTableSize = %d\n",
		   oscRefTable,oscRefTableSize);
	return -1;
    }

    amplitude = tmp -1;
    shift = 24 - amp;
    
    incr = 0;

    for(i=0; i<(size-channels); i+=channels) {
	for(j=i;j<(i+channels);j++) 
	    table[j] = (int)(((double)amplitude)*oscRefTable[incr])<<shift;
    incr += freq;
    if(incr > oscRefTableSize)
	incr -= oscRefTableSize;
    }
    return 0;
}



