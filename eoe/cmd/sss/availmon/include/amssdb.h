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

#ifndef __AVAILMON_SSDB_H
#define __AVAILMON_SSDB_H

/*--------------------------------------------------------------------*/
/* Database and Connect Parameters definition                         */
/*--------------------------------------------------------------------*/

#define  SSDB                                    "ssdb"
#define  SSDBHOST                                NULL
#define  SSDBUSER                                NULL
#define  SSDBPASSWD                              NULL

/*--------------------------------------------------------------------*/
/* SYS_ID Table Definitions                                           */
/*--------------------------------------------------------------------*/

#define  SYSTEMTABLE                             "system"
#define  SYSTEM_SYSID                            "sys_id"
#define  SYSTEM_HOST                             "hostname"
#define  SYSTEM_SERIAL                           "sys_serial"

/*--------------------------------------------------------------------*/
/* AVAILMON Table definitions                                         */
/*--------------------------------------------------------------------*/

#define  AVAILMONTABLE                           "availdata"
#define  AVAILMON_ID                             "avail_id"
#define  AVAILMON_SYSID                          "sys_id"
#define  AVAILMON_EVENTID                        "event_id"
#define  AVAILMON_EVENTTIME                      "event_time"
#define  AVAILMON_LASTTICK                       "lasttick"
#define  AVAILMON_PREVSTART                      "prev_start"
#define  AVAILMON_START                          "start"
#define  AVAILMON_STATUSINT                      "status_int"
#define  AVAILMON_BOUNDS                         "bounds"
#define  AVAILMON_DIAGTYPE                       "diag_type"
#define  AVAILMON_DIAGFILE                       "diag_file"
#define  AVAILMON_SYSLOG                         "syslog"
#define  AVAILMON_HINVCHANGE                     "hinvchange"
#define  AVAILMON_VERCHANGE                      "verchange"
#define  AVAILMON_METRICS                        "metrics"
#define  AVAILMON_FLAG                           "flag"
#define  AVAILMON_SCOMMENT                       "shutdowncomment"
#define  AVAILMON_SUMMARY                        "summary"

/*--------------------------------------------------------------------*/
/* Event Table                                                        */
/*--------------------------------------------------------------------*/

#define  EVENTTABLE                              "event"
#define  EVENT_TYPEID                            "type_id"
#define  EVENT_EVENTID                           "event_id"

/*--------------------------------------------------------------------*/
/* Tool Configuration Table                                           */
/*--------------------------------------------------------------------*/

#define  TOOLTABLE				 "tool"
#define  TOOLNAME				 "tool_name"
#define  TOOLOPTION     			 "tool_option"
#define  OPTIONDEFAULT	        		 "option_default"
#define  TOOLCLASS				 "AVAILMON"

#endif /* __AVAILMON_SSDB_H */

