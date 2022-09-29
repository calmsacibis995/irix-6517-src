/* 
   (C) Copyright Digital Instrumentation Technology, Inc., 1990 - 1993 
       All Rights Reserved
*/
/*-- macPort.h ---------
 *
 * This file contains port-specific information
 *
 */

#ifndef MACPORT_H
#define MACPORT_H

/*
 * FREESTANDING is used internally by DIT to distinguish a distributed version
 * of the Mac library from an "in-context" use internally. It is intended
 * to be configured from the Makefile.
 *
 */


#include "macSG.h"


/* declarations for macPort.c */
extern int macBlockSizeDevice();
extern int macLastLogicalBlockDevice(void *);
extern int macVolumeTypeDevice(void *);
extern int macLogicalVolumeDiskDevice();
extern int macReadBlockDevice(void *, char *, int, int);
extern int macWriteBlockDevice(void *, char *, int, int);

/*-- maximum allowable transfer in bytes for single call to
 *   macRead/WriteBlockDevice
 */

#ifndef MAX_TRANSFER
#define MAX_TRANSFER  65536
#endif

/* miscellaneous for local funcs */
extern int straicmp(char *, char *), strnaicmp(char *, char *, int);
extern int Nstricmp(char *, char *), Nstrnicmp(char *, char *, int);

#endif

