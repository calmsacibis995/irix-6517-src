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

#ifndef __AVAILMON_EVENT_H
#define __AVAILMON_EVENT_H

/*--------------------------------------------------------------------*/
/* Availmon Class Codes                                               */
/*--------------------------------------------------------------------*/

#define  AVAIL_DATA                        4000

/*--------------------------------------------------------------------*/
/* Availmon Type  Codes                                               */
/*--------------------------------------------------------------------*/

#define  MU_UND_NC                         0x200011 
#define  MU_UND_TOUT                       0x200012
#define  MU_UND_UNK                        0x200013
#define  MU_UND_ADM                        0x20001E
#define  MU_HW_UPGRD                       0x20001F
#define  MU_SW_UPGRD                       0x200020
#define  MU_HW_FIX                         0x200021
#define  MU_SW_PATCH                       0x200022
#define  MU_SW_FIX                         0x200023
#define  SU_UND_TOUT                       0x200026
#define  SU_UND_NC                         0x200027
#define  SU_UND_ADM                        0x200028
#define  SU_HW_UPGRD                       0x200029
#define  SU_SW_UPGRD                       0x20002A
#define  SU_HW_FIX                         0x20002B
#define  SU_SW_PATCH                       0x20002C
#define  SU_SW_FIX                         0x20002D
#define  UND_PANIC                         0x200010
#define  HW_PANIC                          0x20000F
#define  UND_INTR                          0x20000E
#define  UND_SYSOFF                        0x20000D
#define  UND_PWRFAIL                       0x20000C
#define  SW_PANIC                          0x200005
#define  UND_NMI                           0x200004
#define  UND_RESET                         0x200003
#define  UND_PWRCYCLE                      0x200002
#define  DE_REGISTER                       0x20000B
#define  REGISTER                          0x20000A
#define  LIVE_NOERR                        0x200009
#define  LIVE_HWERR                        0x200008
#define  LIVE_SWERR                        0x200007
#define  STATUS                            0x200006
#define  ID_CORR                           0x200001
#define  LIVE_SYSEVNT                      0x200000
#define  NOTANEVENT			   0

/*--------------------------------------------------------------------*/
/* Event Macros                                                       */
/*--------------------------------------------------------------------*/

#define  TOOLD(x)          ((x >= 0x200000 && x < 0x20002F) ? (x-0x200013) : x)
#define  TONEW(x)          ( (x > -30 && x < 30) ? (x+0x200013) : x )

#endif /* __AVAILMON_EVENT_H */
