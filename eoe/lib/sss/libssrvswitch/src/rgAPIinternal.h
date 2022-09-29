/* --------------------------------------------------------------------------- */
/* -                           RGAPIINTERNAL.H                               - */
/* --------------------------------------------------------------------------- */
/*                                                                             */
/* Copyright 1992-1998 Silicon Graphics, Inc.                                  */
/* All Rights Reserved.                                                        */
/*                                                                             */
/* This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;      */
/* the contents of this file may not be disclosed to third parties, copied or  */
/* duplicated in any form, in whole or in part, without the prior written      */
/* permission of Silicon Graphics, Inc.                                        */
/*                                                                             */
/* RESTRICTED RIGHTS LEGEND:                                                   */
/* Use, duplication or disclosure by the Government is subject to restrictions */
/* as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data      */
/* and Computer Software clause at DFARS 252.227-7013, and/or in similar or    */
/* successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -     */
/* rights reserved under the Copyright Laws of the United States.              */
/*                                                                             */
/* --------------------------------------------------------------------------- */
#ifndef H_RGAPIINTERNAL_H
#define H_RGAPIINTERNAL_H

typedef void * hPlugInInfo;

/* --------------------------------------------------------------------------- */
#define PF_TIME_TO_KEEP_PLUGIN  10 /* Timeout to keep RG server code and data in memory
                                      after closing the last report session (RGPLUGIN_UNLOAD_TIMESTEP sec unit) */
#define RGPLUGIN_UNLOAD_TIMESTEP 5 /* Unload PlugIn timer step */

#ifdef __cplusplus
extern "C" {
#endif

void rgiOpenPlugInInfo(void);
hPlugInInfo rgiGetFirstPlugInInfo(void);
hPlugInInfo rgiGetNexPlugInInfo(hPlugInInfo);

const char *rgiGetPlugInPathByPlugInInfo(hPlugInInfo);
const char *rgiGetPlugInTitleByPlugInInfo(hPlugInInfo);
const char *rgiGetPlugInVersionByPlugInInfo(hPlugInInfo);

int rgiGetPlugInUsageCountByPlugInInfo(hPlugInInfo);
int rgiGetPlugInUnloadableFlagByPlugInInfo(hPlugInInfo);
int rgiGetPlugInThreadSafeFlagByPlugInInfo(hPlugInInfo);
int rgiGetPlugInUnloadTimeoutByPlugInInfo(hPlugInInfo);
int rgiGetPlugInCurrentUnloadTimeoutByPlugInInfo(hPlugInInfo);

void rgiClosePlugInInfo(void);

#ifdef __cplusplus
}
#endif
#endif 
