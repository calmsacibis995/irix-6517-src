/*
** adpcm.h - include file for adpcm coder.
**
** Version 1.0, 7-Jul-92.
*/

struct adpcm_state {
    short	valprev;	/* Previous output value */
    char	index;		/* Index into stepsize table */
};

void adpcm_coder(short *, char *, int, struct adpcm_state *);
void adpcm_decoder(unchar *, int *, int, struct adpcm_state *);
