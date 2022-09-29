/* --------------------------------------------------------------------------- */
/* -                              SSCPAIR.H                                  - */
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
#ifndef H_SSCPAIR_H
#define H_SSCPAIR_H

#ifndef SSCAPI
#ifdef _MSC_VER 
#define SSCAPI WINAPI
#endif
#endif

#ifndef SSCAPI
#define SSCAPI
#endif

typedef struct sss_s_Pair {
 char *keyname;   /* Key name */
 char *value;     /* Key value */
} CMDPAIR;

#ifdef __cplusplus
extern "C" {
#endif

int SSCAPI sscValidateURLString(char *url);
int SSCAPI sscInitPair(char *cmdstr,CMDPAIR *cmdp,int cmdpsize);
int SSCAPI sscFindPairByKey(CMDPAIR *cmdp,int startidx,const char *szKeyName);
int SSCAPI sscFindPairStrictByKey(CMDPAIR *cmdp,int startidx,const char *szKeyName);
int SSCAPI sscGetPairCounterByKey(CMDPAIR *cmdp,const char *szKeyName);
int SSCAPI sscGetPairStrictCounterByKey(CMDPAIR *cmdp,const char *szKeyName);

#ifdef __cplusplus
}
#endif
#endif /* #ifndef H_SSCPAIR_H */
