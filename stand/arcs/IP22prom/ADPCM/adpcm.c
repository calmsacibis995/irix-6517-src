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

#include "adpcm.h"

#ifndef __STDC__
#define signed
#endif

/* Intel ADPCM step variation table */
static int indexTable[16] = {
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8,
};

static int stepsizeTable[89] = {
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
    
void
adpcm_coder(indata, outdata, len, state)
    short indata[];
    char outdata[];
    int len;
    struct adpcm_state *state;
{
    short *inp;			/* Input buffer pointer */
    signed char *outp;		/* output buffer pointer */
    int val;			/* Current input sample value */
    int sign;			/* Current adpcm sign bit */
    int delta;			/* Current adpcm output value */
    int step;			/* Stepsize */
    int valprev;		/* virtual previous output value */
    int vpdiff;			/* Current change to valprev */
    int index;			/* Current step change index */
    int outputbuffer;		/* place to keep previous 4-bit value */
    int bufferstep;		/* toggle between outputbuffer/output */

    outp = (signed char *)outdata;
    inp = indata;

    valprev = state->valprev;
    index = state->index;
    step = stepsizeTable[index];
    
    bufferstep = 1;

    for ( ; len > 0 ; len-- ) {
	val = *inp++;

	/* Step 1 - compute difference with previous value */
	delta = val - valprev;
	sign = (delta < 0) ? 8 : 0;
	if ( sign ) delta = (-delta);

	/* Step 2 - Divide and clamp */
#ifdef NODIVMUL
        {
	    int tmp = 0;
#ifdef STUPID_V1_BUG
	    vpdiff = 0;
#else
	    vpdiff = (step >> 3);
#endif /* STUPID_V1_BUG */
	    if ( delta > step ) {
		tmp = 4;
		delta -= step;
		vpdiff += step;
	    }
	    step >>= 1;
	    if ( delta > step  ) {
		tmp |= 2;
		delta -= step;
		vpdiff += step;
	    }
	    step >>= 1;
	    if ( delta > step ) {
		tmp |= 1;
		vpdiff += step;
	    }
	    delta = tmp;
	}
#else
	delta = (delta<<2) / step;
	if ( delta > 7 ) delta = 7;

#ifdef STUPID_V1_BUG
	vpdiff = (delta*step) >> 2;
#else
	vpdiff = ((delta*step) >> 2) + (step >> 3);
#endif /* STUPID_V1_BUG */
#endif /* NODIVMUL */
	  
	/* Step 3 - Update previous value */
	if ( sign )
	  valprev -= vpdiff;
	else
	  valprev += vpdiff;

	/* Step 4 - Clamp previous value to 16 bits */
	if ( valprev > 32767 )
	  valprev = 32767;
	else if ( valprev < -32768 )
	  valprev = -32768;

	/* Step 5 - Assemble value, update index and step values */
	delta |= sign;
	
	index += indexTable[delta];
	if ( index < 0 ) index = 0;
	if ( index > 88 ) index = 88;
	step = stepsizeTable[index];

	/* Step 6 - Output value */
	if ( bufferstep ) {
	    outputbuffer = (delta << 4) & 0xf0;
	} else {
	    *outp++ = (delta & 0x0f) | outputbuffer;
	}
	bufferstep = !bufferstep;
    }

    /* Output last step, if needed */
    if ( !bufferstep )
      *outp++ = outputbuffer;
    
    state->valprev = valprev;
    state->index = index;
}

void
adpcm_decoder(indata, outdata, len, state)
    char indata[];
    short outdata[];
    int len;
    struct adpcm_state *state;
{
    signed char *inp;		/* Input buffer pointer */
    short *outp;		/* output buffer pointer */
    int sign;			/* Current adpcm sign bit */
    int delta;			/* Current adpcm output value */
    int step;			/* Stepsize */
    int valprev;		/* virtual previous output value */
    int vpdiff;			/* Current change to valprev */
    int index;			/* Current step change index */
    int inputbuffer;		/* place to keep next 4-bit value */
    int bufferstep;		/* toggle between inputbuffer/input */

    outp = outdata;
    inp = (signed char *)indata;

    valprev = state->valprev;
    index = state->index;
    step = stepsizeTable[index];

    bufferstep = 0;
    
    for ( ; len > 0 ; len-- ) {
	
	/* Step 1 - get the delta value and compute next index */
	if ( bufferstep ) {
	    delta = inputbuffer & 0xf;
	} else {
	    inputbuffer = *inp++;
	    delta = (inputbuffer >> 4) & 0xf;
	}
	bufferstep = !bufferstep;

	/* Step 2 - Find new index value (for later) */
	index += indexTable[delta];
	if ( index < 0 ) index = 0;
	if ( index > 88 ) index = 88;

	/* Step 3 - Separate sign and magnitude */
	sign = delta & 8;
	delta = delta & 7;

	/* Step 4 - update output value */
#ifdef NODIVMUL
#ifdef STUPID_V1_BUG
	vpdiff = 0;
#else
	vpdiff = step >> 1;
#endif /* STUPID_V1_BUG */
	if ( delta & 4 ) vpdiff += (step << 2);
	if ( delta & 2 ) vpdiff += (step << 1);
	if ( delta & 1 ) vpdiff += step;
	vpdiff >>= 2;
#else
#ifdef STUPID_V1_BUG
	vpdiff = ((delta*step) >> 2);
#else
	vpdiff = ((delta*step) >> 2) + (step >> 3);
#endif /* STUPID_V1_BUG */
#endif /* ! NODIVMUL */
	if ( sign )
	  valprev -= vpdiff;
	else
	  valprev += vpdiff;

	/* Step 5 - clamp output value */
	if ( valprev > 32767 )
	  valprev = 32767;
	else if ( valprev < -32768 )
	  valprev = -32768;

	/* Step 6 - Update step value */
	step = stepsizeTable[index];

	/* Step 7 - Output value */
	*outp++ = valprev;
    }

    state->valprev = valprev;
    state->index = index;
}
