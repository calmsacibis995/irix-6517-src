/*--------------------------------------------------------------------*/
/*                                                                    */
/* Copyright 1992-1998 Silicon Graphics, Inc.                         */
/* All Rights Reserved.                                               */
/*                                                                    */
/* This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics    */
/* Inc.; the contents of this file may not be disclosed to third      */
/* parties, copied or duplicated in any form, in whole or in part,    */
/* without the prior written permission of Silicon Graphics, Inc.     */
/*                                                                    */
/* RESTRICTED RIGHTS LEGEND:                                          */
/* Use, duplication or disclosure by the Government is subject to     */
/* restrictions as set forth in subdivision (c)(1)(ii) of the Rights  */
/* in Technical Data and Computer Software clause at                  */
/* DFARS 252.227-7013, and/or in similar or successor clauses in      */
/* the FAR, DOD or NASA FAR Supplement.  Unpublished - rights         */
/* reserved under the Copyright Laws of the United States.            */
/*                                                                    */
/*--------------------------------------------------------------------*/

#ifndef __AVAILMON_DEFS_H
#define __AVAILMON_DEFS_H

/*--------------------------------------------------------------------*/
/* Availmon's Version                                                 */
/*--------------------------------------------------------------------*/

#define  AMRVERSIONNUM                     "2.1"
#define  AMHEADER                          "#@:AVAILMON:@#"

/*--------------------------------------------------------------------*/
/* Availmon's configuration files                                     */
/*--------------------------------------------------------------------*/

#define  PREVSTARTFILE                  "/var/adm/avail/.save/prevstart"
#define  LASTTICKFILE                   "/var/adm/avail/lasttick"

/*--------------------------------------------------------------------*/
/* Commonly used command definitions                                  */
/*--------------------------------------------------------------------*/

#define ISCOMMAND               1

#define HINV                    "/sbin/hinv"
#define HINVCOMMAND             "/sbin/hinv -mvv"
#define VERSIONS                "/usr/sbin/versions"
#define VERSIONSCOMMAND         "/usr/sbin/versions -n eoe.sw.base patchSG*"
#define GFX                     "/usr/gfx/gfxinfo"
#define GFXCOMMAND              "/usr/gfx/gfxinfo -v"
#define UNAME                   "/sbin/uname"
#define UNAMECOMMAND            "/sbin/uname -Ra"
#define MAILCMD                 "/usr/sbin/Mail"
#define ENCOCMD                 "/usr/bsd/uuencode"
#define CATCMD                  "/usr/bin/cat"
#define COMPRESSCMD             "/usr/bsd/compress -f -c"
#define CRYPTCMD                "/usr/bin/crypt"

/*--------------------------------------------------------------------*/
/* Some maximum string lengths                                        */
/*--------------------------------------------------------------------*/

#define  MAX_LINE_LEN                      1024
#define  MAXINPUTLINE                      2048
#define  MAX_TAGS                          4
#define  MAX_STR_LEN                       80
#define  MAXVARCHARSIZE			   256

/*--------------------------------------------------------------------*/
/* File name for temporary encoded file                               */
/*--------------------------------------------------------------------*/

#define  DEFAULTFILELOC                    "/var/adm/crash"
#define  AVAILREPORT                       "availreport"
#define  PAGERREPORT                       "pagerreport"
#define  DIAGREPORT                        "diagreport"
#define  AVAILREPORTSU                     "availreport.su"
#define  DIAGREPORTSU                      "diagreport.su"
#define  PAGERREPORTSU                     "pagerreport.su"
#define  AMR_CODE                          "amc"

#define  errorfp                           stderr

#endif /* __AVAILMON_DEFS_H */
