/* --------------------------------------------------------------------------- */
/* -                               SSC.H                                     - */
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
#ifndef H_SSC_CONSOLE_H
#define H_SSC_CONSOLE_H

#define SSC_VMAJOR     1   /* SSC major version number */
#define SSC_VMINOR     0   /* SSC minor version number */

#include "sscStreams.h" 
#include "sscErrors.h" 
#include "sscShared.h"

#ifndef SSS_ROOT
#define SSS_ROOT "/$sss/"
#endif

#define SSC_NAME "System Support Console"


#ifdef __cplusplus
extern "C" {
#endif

int sscHTMLProcessorInit(void);
int sscHTMLProcessorDone(void);
int sscHTMLPreProcessor(sscErrorHandle hError, streamHandle source, streamHandle result,const char *paramstr);
int sscHTMLProcessor(sscErrorHandle hError, const char *url, streamHandle result);

#ifdef __cplusplus
}
#endif
#endif /* #ifndef H_SSC_CONSOLE_H */

