#ident	"IP12prom/hello_tune.c:  $Revision: 1.9 $"

/*
 * hello_tune.c
 */

#include "sys/cpu.h"
#include "sys/types.h"
#include "sys/sbd.h"
#include "sys/invent.h"
#include "hello_tune.h"

#include "arcs/hinv.h"

static COMPONENT hal1tmpl = {
	ControllerClass,		/* Class */
	AudioController,		/* Type */
	Input|Output,			/* Flags */
	SGI_ARCS_VERS,			/* Version */
	SGI_ARCS_REV,			/* Revision */
	0,				/* Key */
	0x01,				/* Affinity */
	0,				/* ConfigurationDataSize */
	5,				/* IdentifierLength */
	"HAL1"				/* Identifier */
};

void
audio_install(COMPONENT *p)
{
	COMPONENT tmpl = hal1tmpl;
	int vers;
	if ((vers = play_hello_tune(1, 0)) < 0)
	    return;

	tmpl.Key = vers;
	p = AddChild(p,&tmpl,0);
	if (p == (COMPONENT *)NULL) cpu_acpanic("audio");
}	

/*
 * flag		0 for normal, 1 for supress tune & just get rev level
 * tune_type  	0=hello, 1=goodbye, 2=graphics failure
 */
int 
play_hello_tune(int flag, int tune_type)
{
	volatile register u_int *px,i;
	register u_int *s,*end;
	int vol;
	struct hello_comm_area_struct {

	    /* The dsp uses the dsp_state field to inform the MIPS of its
	     * progress.  The MIPS sets it to 0xffffff before starting the
	     * DSP.  The DSP sets it to zero immediately upon being started,
	     * sets it to 1 right after it writes the actel_rev field, and
	     * sets it to 2 right after the boot tune has completed.
	     */
	    long dsp_state; 

	    /* The DSP writes the actel_rev field with the value of actel
	     * register 7, the Revision Level Key.  All official SGI hdsp
	     * actels have zero in the most significant (bit 7) bit of the
	     * revision level key, and the most significant bit will float
	     * to 1 if the audio board (or the actel chip) is not plugged
	     * in.  Hence the actel_rev field is used to test for the
	     * presence of audio hardware.
	    */ 
	    long actel_rev;

	    /* The MIPS sets the audio_volume field before starting the
	     * DSP.  The DSP will use it as the setting of the left and
	     * right headphone&speaker gain dacs.  However, if audio_volume
	     * is set to zero, the tone will not be played at all.
	    */
	    long audio_volume;

	    /* The MIPS sets the tune_type field before starting the
	     * DSP.  The DSP will use it to select the tune to play.
	     * Tune 0 is the boot tune.  Tune 1 is the power-off tune.
	     * Tune 2 is the graphics failure tune.
	    */
	   
	    long tune_type;
        } *comm;

	comm = (struct hello_comm_area_struct *)PHYS_TO_K1(HPC1MEMORY);

	/* Reset the DSP */

	*(u_int *)PHYS_TO_K1(HPC1MISCSR)=1;

	/* Initialize the dsp state to -1 */
	comm->dsp_state = -1;

	/* Initialize the audio volume */
	atob(getenv("volume"),&vol);
	comm->audio_volume = vol;

	/* If flag==1 skip playing the tune. */

	if(flag==1)comm->audio_volume=0;

	/* Setup the tune_type */

	comm->tune_type=tune_type;

	/* Download the program's P memory space. */

	s=dsp_text;
	end=dsp_text+sizeof(dsp_text)/sizeof(u_int);
	px = (u_int *)PHYS_TO_K1(HPC1MEMORY+(DSP_TEXT_ORG-0xc000)*sizeof(long));
	for(;;) {
	    if(s==end)break;
	    *px = *s;
	    px++;
	    s++;
	}

	/* Download the program's L memory space. */

	s=dsp_ldata;
	end=dsp_ldata+sizeof(dsp_ldata)/sizeof(u_int);
	for(;;) {
	    if(s==end)break;
	    *px = s[0];
	    *(px+0x4000) = s[1];
	    px++;
	    s += 2;
	}

	/* Start the DSP */

	*(u_int *)PHYS_TO_K1(HPC1MISCSR)=8;

	/* The dsp_state should at least get to the 1 state within
	 * 50 microseconds of startup.  So we wait for it, then we
	 * check that the dsp actually started up and we check whether
	 * the audio board is plugged in.
	 */
	us_delay(50);
	i = comm->dsp_state & 0xffffff;
	if(i == 1 || i == 2) {
	    if(!(comm->actel_rev&0x80))
		return comm->actel_rev&0xff;
	}
	return -1;
}
