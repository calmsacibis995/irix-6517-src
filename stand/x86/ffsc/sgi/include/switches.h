/*
 * switches.h
 *	Declarations and prototypes pertaining to the MMSC display switches
 */
#ifndef _SWITCHES_H_
#define _SWITCHES_H_

/* Bits corresponding to the various switches on the display. */
/* IMPORTANT: a switch is being pressed if its bit is *0*     */
#define SWITCH_DOWN	0x02
#define SWITCH_ENTER	0x01
#define SWITCH_ESC	0x20
#define SWITCH_LEFT	0x10
#define SWITCH_RIGHT	0x04
#define SWITCH_UP	0x08

#define SWITCH_ALL	(SWITCH_UP | SWITCH_DOWN | SWITCH_LEFT | SWITCH_RIGHT \
			 | SWITCH_ESC | SWITCH_ENTER)


/* "Switch messages" at boot time */
#define SWITCH_ZAP_NVRAM	(SWITCH_LEFT | SWITCH_RIGHT)


/* Other constants */
#define BOOT_SWITCH_SAMPLES	10000	/* # of samples at boot time */


/* Global variables */
extern int ffscInitSwitches;		/* Switch settings at boot time */


/* Function prototypes */
int sampleswitches(int);

#endif  /* _SWITCHES_H_ */
