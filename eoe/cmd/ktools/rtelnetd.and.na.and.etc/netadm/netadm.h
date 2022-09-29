/******************************************************************************
 *
 *        Copyright 1989, Xylogics, Inc.  ALL RIGHTS RESERVED.
 *
 * ALL RIGHTS RESERVED. Licensed Material - Property of Xylogics, Inc.
 * This software is made available solely pursuant to the terms of a
 * software license agreement which governs its use.
 * Unauthorized duplication, distribution or sale are strictly prohibited.
 *
 * Include file description:
 *  %$(description)$%
 *
 * Original Author: %$(author)$%    Created on: %$(created-on)$%
 *
 * Revision Control Information:
 *
 * $Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/netadm/RCS/netadm.h,v 1.3 1996/10/04 12:13:36 cwilson Exp $
 *
 * This file created by RCS from $Source: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/netadm/RCS/netadm.h,v $
 *
 * Revision History:
 *
 * $Log: netadm.h,v $
 * Revision 1.3  1996/10/04 12:13:36  cwilson
 * latest rev
 *
 * Revision 2.8  1991/08/01  18:12:06  emond
 * Don't define *strcpy() for AIX machines, per Griff
 *
 * Revision 2.7  91/01/23  21:52:32  raison
 * added lat_value na parameter and GROUP_CODE parameter type.
 * 
 * Revision 2.6  90/12/04  12:13:41  sasson
 * The image_name was increased to 100 characters on annex3.
 * 
 * Revision 2.5  89/05/18  15:05:38  grant
 * Add defibes for RPC shutdown call.
 * 
 * Revision 2.4  89/04/05  12:44:26  loverso
 * Changed copyright notice
 * 
 * Revision 2.3  88/05/04  23:19:11  harris
 * No more global pep or socket descriptor.
 * 
 * Revision 2.2  87/03/05  15:38:50  parker
 * made explicit extern of Gsocket_decr and Gpep_id. Gould machines
 * didn't like the old way.
 * 
 * Revision 2.1  86/05/07  11:15:55  goodmon
 * Changes for broadcast command.
 * 
 * Revision 2.0  86/02/21  11:35:55  parker
 * First development revision for Release 2
 * 
 * Revision 1.2  86/01/02  11:49:50  goodmon
 * Removed unused error codes.
 * 
 * Revision 1.1  85/11/01  17:52:43  palmer
 * Initial revision
 * 
 *
 * This file is currently under revision by:
 *
 * $Locker:  $
 *
 *  DATE:   $Date: 1996/10/04 12:13:36 $
 *  REVISION:   $Revision: 1.3 $
 *
 *****************************************************************************/


#define DELAY   2
#define TIMEOUT 5

#define RESPONSE_SIZE	1100
#define STRING_MAX	32

#define FILENAME_LENGTH 80
#define MESSAGE_LENGTH 1024
#define WARNING_LENGTH 250
#define USERNAME_LENGTH 32
#define HOSTNAME_LENGTH 32
#define MAX_STRING_100 100 /* also defined in na/na.h and inc/rom/e2rom.h */

/* type definitions */

typedef union
    {
    char     chr;
    COUR_MSG msg;
    CMCALL   call;
    CMREJECT rej;
    CMABORT  ab;
    CMRETURN ret;
    }        COUR_HDR;

typedef struct
    {
    u_short count;
    char    data[MAX_STRING_100 + 4];
    }       STRING;

typedef struct
    {
    u_short     type;
    union
	{
	u_short short_data;
	u_short long_data[2];
	u_char	raw_data[MAX_STRING_100];
	STRING  string;
	}       data;
    }           PARAM;

/* to keep lint from complaining */
#ifndef AIX
char *strcpy();
#endif
