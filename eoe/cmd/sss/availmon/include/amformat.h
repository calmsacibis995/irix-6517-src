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

#ifndef __AMFORMAT_H
#define __AMFORMAT_H

/*--------------------------------------------------------------------*/
/* Configuration Flags for Availmon                                   */
/*--------------------------------------------------------------------*/

#define AUTOEMAIL               1         /* Signifies autoemail on   */
#define HINVUPDATE              2         /* hinvupdate on            */
#define LIVENOTIFICATION        4         /* livenotification on      */
#define TICKERD                 8         /* tickerd on               */
#define SHUTDOWNREASON          16        /* shutdownreason on        */

/*--------------------------------------------------------------------*/
/* Definitions for flag field of availmon database                    */
/*--------------------------------------------------------------------*/

#define SUMMARYISFILE           (1 << 0)  /* Signifies that the       */
					  /* field summary contains   */
					  /* a filename instead of    */
					  /* summary string           */

#define EVENTISSU               (1 << 1)  /* Signifies that this event*/
					  /* is a single user event   */

#define PREVEVENTISSU           (1 << 2)  /* Signifies that perv event*/
					  /* is a single user event   */

#define DIAGTYPE_OSERL          (1 << 3)  /* Signifies that the field */
					  /* diagtype has the old     */
					  /* serial number needed for */
					  /* IDCORR report            */

#define DIAGFILE_OHOST          (1 << 4)  /* Signifies that the field */
					  /* diagfile has the old     */
					  /* hostname needed for IDCOR*/
					  /* report                   */


/*--------------------------------------------------------------------*/
/* Report Type definitions                                            */
/*--------------------------------------------------------------------*/

#define AVAILABILITY_CENCR      1
#define AVAILABILITY_COMP       2
#define AVAILABILITY            3
#define DIAGNOSTIC_CENCR        4
#define DIAGNOSTIC_COMP         5
#define DIAGNOSTIC              6
#define PAGER                   7



#endif /* __AMFORMAT_H */

