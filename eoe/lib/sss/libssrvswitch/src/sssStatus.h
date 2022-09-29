/* --------------------------------------------------------------------------- */
/* -                              SSSSTATUS.H                                - */
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
#ifndef H_SSSSTATUS_H
#define H_SSSSTATUS_H

typedef struct sss_s_GLBINFO {
unsigned long volatile statTotalHTMLProcessorCall;      /* Total sscHTMLProcessor calls */
unsigned long volatile statTotalHTMLPreProcessorCall;   /* Total sscHTMLPreProcessor calls */
unsigned long volatile statTotalrgPluginHeadAlloced;    /* Total rgPluginHead alloced in sssSession.c */
unsigned long volatile statTotalrgPluginHeadInFreeList; /* Total rgPluginHead in free list */
unsigned long volatile statTotalsssSessionAlloced;      /* Total sssSession alloced in sssSession.c */
unsigned long volatile statTotalsssSessionInFreeList;   /* Total sssSession in free list */
unsigned long volatile statTotalPluginFaceAlloced;      /* Total PluginFace in free list (file: rgAPI.c) */
unsigned long volatile statTotalPluginFaceInFreeList;   /* Total PluginFace in free list */
unsigned long volatile statTotalLoadNewPlugIn;          /* Total "Load plugin" */
unsigned long volatile statTotalUnloadPlugin;           /* Total "Unload plugin" */
unsigned long volatile statTotalReportSessionAlloced;   /* Total ReportSession alloced */
unsigned long volatile statTotalReportSessionInFreeList;/* Total ReportSession in free list */

} SSSGLBINFO;
#endif
