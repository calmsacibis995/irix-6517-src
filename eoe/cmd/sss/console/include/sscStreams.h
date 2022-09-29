/* --------------------------------------------------------------------------- */
/* -                              SSCSTREAMS.H                               - */
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
#ifndef _STREAMS_H_
#define _STREAMS_H_

typedef void* streamHandle;

#define STREAMLIB_GETMBLOCKALLOC  0x01992001  /* Get Memory blocks "alloc" counter */
#define STREAMLIB_GETMBLOCKFREE   0x01992002  /* Get Memory blocks "free" counter */
#define STREAMLIB_GETSTREAMALLOC  0x01992003  /* Get Stream control blocks "alloc" counter */
#define STREAMLIB_GETSTREAMFREE   0x01992004  /* Get Stream control blocks "free" counter */

#ifdef __cplusplus
extern "C" {
#endif

unsigned long streamGetInfo(int idx);

int isStreamValid(streamHandle str);

streamHandle newMemoryStream(void);

streamHandle newFileInputStream(const char *filename);

streamHandle newFileOutputStream(const char *filename);

/* return memory or file output stream */
streamHandle newOutputStream(void);

int destroyStream(streamHandle str);

int putChar(const int c,   streamHandle str);

int putString(const char *s, streamHandle str);

int putStringFmt(streamHandle str, char *msg,...);

int getChar(streamHandle str);

int putStream(streamHandle from, streamHandle to);

int unputChar(streamHandle str);

int rewriteStream(streamHandle str);

int rewindStream(streamHandle str);

int eofStream(streamHandle str);

int sizeofStream(streamHandle str);

#ifdef __cplusplus
}
#endif
#endif
