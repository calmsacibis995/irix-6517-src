/*
 * AIM Suite III v3.1.1
 * (C) Copyright AIM Technology Inc. 1986,87,88,89,90,91,92.
 * All Rights Reserved.
 */

#ifndef	testerr_h
#	define testerr_h " @(#) testerr.h:3.2 5/30/92 20:18:51"
#endif

/*
**
** testerr.h
**
*/

#define BADTTY -1
#define BADMEM -2
#define BADTAPE -3
#define BADIOCTL -4
#define BADPIPE -5
#define BADPROC -6

#ifdef DEBUGON
extern long debug;
#define TTFIELD 	0x2
#define TPFIELD		0x4
#define VMFIELD		0x8
#define DEBUG(x)	if(debug) {{x;}fflush(stdout);fflush(stderr);}
#define TTDEBUG(x)	if(debug&TTFIELD) {{x;}fflush(stdout);fflush(stderr);}
#define TPDEBUG(x)	if(debug&TPFIELD) {{x;}fflush(stdout);fflush(stderr);}
#define VMDEBUG(x)	if(debug&VMFIELD) {{x;}fflush(stdout);fflush(stderr);}
#else
#define DEBUG(x)
#define DEBUG2(x)
#define TTDEBUG(x)
#define TPDEBUG(x)
#define VMDEBUG(x)
#endif
