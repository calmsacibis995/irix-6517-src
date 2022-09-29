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

#ifndef __AMNOTIFY_H
#define __AMNOTIFY_H


/*--------------------------------------------------------------------*/
/* Definitions for the types of reports that availmon generates       */
/*--------------------------------------------------------------------*/

#ifndef MAXTYPES 
#define MAXTYPES                7
#endif

char  *repTypestr[MAXTYPES] = { 
    "AVAILABILITY_CENCR", "AVAILABILITY_COMP", "AVAILABILITY",
    "DIAGNOSTIC_CENCR", "DIAGNOSTIC_COMP", "DIAGNOSTIC",
    "PAGER" 
};

#endif /* __AMNOTIFY_H */









