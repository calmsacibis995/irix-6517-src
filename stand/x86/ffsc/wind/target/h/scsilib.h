/* scsi2Lib.h - SCSI library header file */

/* Copyright 1984-1994 Wind River Systems, Inc. */

/*
modification history
--------------------
01a,10oct94,jds  merged for SCSI1 and SCSI2 compatability
*/

#ifndef __INCscsiLibh
#define __INCscsiLibh

typedef struct scsifunctbl
    {
    FUNCPTR scsiCtrlInit;
    FUNCPTR scsiBlkDevInit;
    FUNCPTR scsiBlkDevCreate;
    FUNCPTR scsiBlkDevShow;
    FUNCPTR scsiPhaseNameGet;
    FUNCPTR scsiPhysDevCreate;
    FUNCPTR scsiPhysDevDelete;
    FUNCPTR scsiPhysDevIdGet;
    FUNCPTR scsiAutoConfig;
    FUNCPTR scsiShow;
    FUNCPTR scsiBusReset;
    FUNCPTR scsiCmdBuild;
    FUNCPTR scsiTransact;
    FUNCPTR scsiIoctl;
    FUNCPTR scsiFormatUnit;
    FUNCPTR scsiModeSelect;
    FUNCPTR scsiModeSense;
    FUNCPTR scsiReadCapacity;
    FUNCPTR scsiRdSecs;
    FUNCPTR scsiWrtSecs;
    FUNCPTR scsiTestUnitRdy;
    FUNCPTR scsiInquiry;
    FUNCPTR scsiReqSense;
    } SCSI_FUNC_TBL;

/* The default is to include SCSI1 headers */

#ifndef INCLUDE_SCSI2

#include "scsi1lib.h"

#else

#include "scsi2lib.h"

#endif

#endif /* INCscsiLibh */
