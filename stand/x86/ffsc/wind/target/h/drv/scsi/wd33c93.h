/* wd33c93.h - SCSI library header file */

/* Copyright 1984-1994 Wind River Systems, Inc. */

/*
modification history
--------------------
01a,10oct94,jds  written for SCSI1 and SCSI2 compatability
*/

#ifndef __INCwd33c93h
#define __INCwd33c93h

#ifndef INCLUDE_SCSI2

#include "wd33c93_1.h"

#else

#include "wd33c93_2.h"

#endif /* INCLUDE_SCSI2 */

#endif /* __INCwd33c93 */
