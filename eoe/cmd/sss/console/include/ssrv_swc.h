/* --------------------------------------------------------------------------- */
/* -                             SSRV_SWC.H                                  - */
/* --------------------------------------------------------------------------- */
/*                                                                             */
/* Copyright 1992-1999 Silicon Graphics, Inc.                                  */
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
#ifndef H_SSRV_SWC_H
#define H_SSRV_SWC_H

#ifdef __cplusplus
extern "C" {
#endif

typedef void *hTaskControlBlock; /* Like in RSX-11M :) */
/* --------------------------------------------------------------------------- */
/* Definition for GET/SET functions                                            */
/* GET: Integer */
#define SSRVGETI_HTTPMETHOD      0x00110001    /* Get HTTP request method (see.ssrv_http.h HTTPM_...) */
#define SSRVGETI_URLSIZE         0x00110002    /* Get URL size (without last "0" */
#define SSRVGETI_HTTPVMAJOR      0x00110003    /* Get HTTP protocol major version number */
#define SSRVGETI_HTTPVMINOR      0x00110004    /* Get HTTP protocol major version number */
#define SSRVGETI_KEEPALIVE       0x00110005    /* Get current status of "keep-alive" flag */
#define SSRVGETI_USERARGMAX      0x00110006    /* Get max numbers of "user defined parameters" */
#define SSRVGETI_MAXREALMSIZE    0x00110007    /* Get max "realm" string size */
#define SSRVGETI_MAXWORKBUFSIZE  0x00110008    /* Get max size of work buffer */
#define SSRVGETI_ACTWORKBUFSIZE  0x00110009    /* Get actual data size in work buffer */
#define SSRVGETI_CONTENTLENGTH   0x0011000a    /* Get "Content-length" value from TCB */
#define SSRVGETI_MAXMIMETYPESIZE 0x0011000b    /* Get max size of "mime type" buffer */
#define SSRVGETI_WEBSRVVMAJOR    0x0011000c    /* Get WEB server core module major version number */
#define SSRVGETI_WEBSRVVMINOR    0x0011000d    /* Get WEB server core module minor version number */
#define SSRVGETI_WEBSRVPORT      0x0011000e    /* Get WEB server "listen" port */
#define SSRVGETI_USERNAMEKEY     0x0011000f    /* Get "username" xcode key */
#define SSRVGETI_PASSWORDKEY     0x00110010    /* Get "password" xcode key */

/* GET: Strings */
#define SSRVGETS_USERARGS        0x00210001    /* Get User defined string (void *) value */
#define SSRVGETS_URL             0x00210002    /* Get URL pointer from TCB */
#define SSRVGETS_CLIENTIPADDR    0x00210003    /* Get "Client IP address" string pointer */
#define SSRVGETS_USERNAME        0x00210004    /* Get "UserName" from authentication string */
#define SSRVGETS_PASSWD          0x00210005    /* Get "Password" from authentication string */
#define SSRVGETS_WORKBUFPTR      0x00210006    /* Get work buffer pointer */
#define SSRVGETS_BODYPARAMS      0x00210007    /* Get parameters string from POST body */
#define SSRVGETS_SERVERNAME      0x00210008    /* Get WEB server name (daemon name) */
#define SSRVGETS_SRVLEGALNAME    0x00210009    /* Get WEB server legal name */
#define SSRVGETS_SRVVERTIME      0x0021000a    /* Get WEB server version date */
#define SSRVGETS_SRVRESPATH      0x0021000b    /* Get WEB server resource path */
#define SSRVGETS_SRVISOCKNAME    0x0021000c    /* Get WEB server "communication pipe" name */

/* SET: Integer */
#define SSRVSETI_HTTPRETCODE     0x00120001    /* Set "HTTP" return code (see: ssrv_http.h HTTP_...) */
#define SSRVSETI_ACTWORKBUFSIZE  0x00120002    /* Set actual data size for work buffer */
#define SSRVSETI_CONTENTLENGTH   0x00120003    /* Set "Content-length" value (for hide this parameter use (-1)) */
#define SSRVSETI_NOCACHEFLG      0x00120004    /* Set "No-Cache" flag for response */

/* SET: Strings */
#define SSRVSETS_USERARGS        0x00220001    /* Set User defined string (void *) value */
#define SSRVSETS_REALMSTR        0x00220002    /* Set "Realm" string value for response */
#define SSRVSETS_MIMETYPE        0x00220003    /* Set "mime type" string value for response */
#define SSRVSETS_LOCATION        0x00220004    /* Set "Location" string value for response */
/* --------------------------------------------------------------------------- */
/* Get Integer value by TCB and index                                          */
/* parameter(s):                                                               */
/*  hTaskControlBlock hTcb - must be valid Task Control Block                  */
/*  valueIdx - request index (SSRVGETI_...)                                    */
/*  valueIdx2 - additional parameter (depend from valueIdx value)              */
/* --------------------------------------------------------------------------- */
int ssrvGetIntegerByTcb(hTaskControlBlock hTcb, int valueIdx, int valueIdx2);
typedef int FPssrvGetIntegerByTcb(hTaskControlBlock hTcb,int valueIdx, int valueIdx2);

/* --------------------------------------------------------------------------- */
/* Get string pointer by TCB and index                                         */
/* parameter(s):                                                               */
/*  hTaskControlBlock hTcb - must be valid Task Control Block                  */
/*  valueIdx - request index (SSRVGETS_...)                                    */
/*  valueIdx2 - additional parameter (depend from valueIdx value)              */
/* --------------------------------------------------------------------------- */
const char *ssrvGetStringByTcb(hTaskControlBlock hTcb,int valueIdx,int valueIdx2);
typedef const char *FPssrvGetStringByTcb(hTaskControlBlock hTcb,int valueIdx,int valueIdx2);

/* --------------------------------------------------------------------------- */
/* Set Integer value by TCB and index                                          */
int ssrvSetIntegerByTcb(hTaskControlBlock hTcb,int valueIdx,int valueIdx2,int value);
typedef int FPssrvSetIntegerByTcb(hTaskControlBlock hTcb,int valueIdx,int valueIdx2,int value);

/* --------------------------------------------------------------------------- */
/* Set string pointer by TCB and index                                         */
int ssrvSetStringByTcb(hTaskControlBlock hTcb,int valueIdx,int valueIdx2,const char *value);
typedef int FPssrvSetStringByTcb(hTaskControlBlock hTcb,int valueIdx,int valueIdx2,const char *value);


/* --------------------------------------------------------------------------- */
/* Declare "reload server IP filters configurations" event                     */
void ssrvDeclareReloadIPFilterInfo(void);
typedef void FPssrvDeclareReloadIPFilterInfo(void);
/* --------------------------------------------------------------------------- */
/* server functions export structure                                           */
typedef struct sss_s_ssrvExportFunctions {
 unsigned long struct_size;              /* sizeof(SSRVEXPORT) */
 FPssrvGetIntegerByTcb           *fp_ssrvGetIntegerByTcb;
 FPssrvGetStringByTcb            *fp_ssrvGetStringByTcb;
 FPssrvSetIntegerByTcb           *fp_ssrvSetIntegerByTcb;
 FPssrvSetStringByTcb            *fp_ssrvSetStringByTcb;
 FPssrvDeclareReloadIPFilterInfo *fp_ssrvDeclareReloadIPFilterInfo;
} SSRVEXPORT;

/* --------------------------------------------------------------------------- */
/* Switcher API                                                                */

/* --------------------------------------------------------------------------- */
/* Initialize "Switcher" API                                                   */
int swcInitSwitcher(SSRVEXPORT *exf);
typedef int FPswcInitSwitcher(SSRVEXPORT *exf);

/* --------------------------------------------------------------------------- */
/* Done "Switcher" API                                                         */
void swcDoneSwitcher(void);
typedef void FPswcDoneSwitcher(void);

/* --------------------------------------------------------------------------- */
/* Start request processing                                                    */
int swcBeginProc(hTaskControlBlock hTcb);
typedef int FPswcBeginProc(hTaskControlBlock hTcb);

/* --------------------------------------------------------------------------- */
/* Continue request processing                                                 */
int swcContProc(hTaskControlBlock hTcb);
typedef int FPswcContProc(hTaskControlBlock hTcb);

/* --------------------------------------------------------------------------- */
/* Finalize request processing                                                 */
int swcEndProc(hTaskControlBlock hTcb);
typedef int FPswcEndProc(hTaskControlBlock hTcb);

/* --------------------------------------------------------------------------- */
/* "Switcher" export functions                                                 */
typedef struct sss_s_ExportFunctions {
 unsigned long struct_size;          /* sizeof(SWCEXPORT) */
 FPswcInitSwitcher *fp_swcInitSwitcher;
 FPswcDoneSwitcher *fp_swcDoneSwitcher;
 FPswcBeginProc    *fp_swcBeginProc;
 FPswcContProc     *fp_swcContProc;
 FPswcEndProc      *fp_swcEndProc;
} SWCEXPORT;

#ifdef __cplusplus
}
#endif
#endif /* #ifndef H_SSRV_SWC_H */
