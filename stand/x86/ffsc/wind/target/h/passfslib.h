/* passFsLib.h - pass-through file system library header */

/* Copyright 1984-1993 Wind River Systems, Inc. */

/*
modification history
--------------------
01a,05jun93,gae  written.
*/


#ifndef __INCpassFsLibh
#define __INCpassFsLibh

#ifdef __cplusplus
extern "C" {
#endif


/* Function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern void *passFsDevInit (char *devName);
extern STATUS passFsInit (int nPassfs);
	      
#else	/* __STDC__ */

extern void *passFsDevInit ();
extern STATUS passFsInit ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCpassFsLibh */
