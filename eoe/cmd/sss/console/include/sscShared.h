/* --------------------------------------------------------------------------- */
/* -                             SSCSHARED.H                                 - */
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
#ifndef H_SSCSHARED_H
#define H_SSCSHARED_H

#ifndef SSCAPI
#ifdef _MSC_VER 
#define SSCAPI WINAPI
#endif
#endif

#ifndef SSCAPI
#define SSCAPI
#endif

#define SSC_SHARED_MAXLONGIDX 20
#define SSC_SHARED_MAXSTRIDX  5
#define SSC_SHARED_MAXSTRLEN  128

#ifdef __cplusplus
extern "C" {
#endif

int SSCAPI sscSetSharedLong(int idx,unsigned long value);
unsigned long SSCAPI sscGetSharedLong(int idx);

int SSCAPI sscSetSharedString(int idx,const char *str);
int SSCAPI sscGetSharedString(int idx,char *strbuf,int strbufsize);

#ifdef __cplusplus
}
#endif
#endif
