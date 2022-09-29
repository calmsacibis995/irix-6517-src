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

#ifndef __AMFIELDS_H
#define __AMFIELDS_h

/*--------------------------------------------------------------------*/
/* Types of reports and their definitions                             */
/*--------------------------------------------------------------------*/

#define DEREGISREP		(1 << 0)
#define REGISREP		(1 << 1)
#define IDCORRREP		(1 << 2)
#define STATUSREP		(1 << 3)
#define REBOOTREP		(1 << 4)
#define UNSCHEDREP		(1 << 5)
#define LIVEERRREP		(1 << 6)
#define LIVESYSREP		(1 << 7)

#define LIVEREP			(LIVEERRREP | LIVESYSREP)
#define ALLREP			(DEREGISREP|REGISREP|IDCORRREP| \
				 STATUSREP |REBOOTREP|UNSCHEDREP| \
				 LIVEREP)

/*--------------------------------------------------------------------*/
/* Field Seperator in Availmon Reports                                */
/*--------------------------------------------------------------------*/

#define SEPERATOR               "|"


/*--------------------------------------------------------------------*/
/* Some Defines used for generation of Reports                        */
/*--------------------------------------------------------------------*/

#define	SERIALNUMVAL		0
#define	HOSTNAMEVAL		1
#define AMRVERSIONVAL		2
#define UNAMEVAL		3
#define PREVSTARTVAL		4
#define EVENTVAL		5
#define OLDSERIALNUMVAL		6
#define OLDHOSTNAMEVAL		7
#define LASTTICKVAL		8
#define STATUSINTERVALVAL	9
#define STARTVAL		10
#define SUMMARYVAL		11
#define REGISTRATIONVAL		12
#define DEREGISTRATIONVAL	13
#define SYSLOGVAL		14
#define VERSIONSVAL		15
#define HINVVAL			16
#define GFXINFOVAL		17

/*--------------------------------------------------------------------*/
/* amrFields structure definition                                     */
/*--------------------------------------------------------------------*/

typedef struct amrFields_s {
    char                      *field_name;
    __uint32_t                flag;
} amrFields_t;

amrFields_t repFields [] = {
    {"SERIALNUM",	ALLREP},
    {"HOSTNAME",        ALLREP},
    {"AMRVERSION",	ALLREP},
    {"UNAME",		(ALLREP & ~DEREGISREP)}, 
    {"PREVSTART",	ALLREP & ~IDCORRREP},
    {"EVENT",		ALLREP},
    {"OLDSERIALNUM",	IDCORRREP},
    {"OLDHOSTNAME",	IDCORRREP},
    {"LASTTICK",	(ALLREP & ~(DEREGISREP|IDCORRREP))},
    {"STATUSINTERVAL",	(ALLREP & ~(DEREGISREP|IDCORRREP))},
    {"START",		REBOOTREP|UNSCHEDREP},
    {"SUMMARY",		IDCORRREP|UNSCHEDREP|LIVEREP},
    {"REGISTRATION",	REGISREP},
    {"DEREGISTRATION",	DEREGISREP},
    {"SYSLOG",          UNSCHEDREP|LIVEERRREP},
    {"VERSIONS",	(ALLREP & ~(DEREGISREP|IDCORRREP))},
    {"HINV",		(ALLREP & ~(DEREGISREP|IDCORRREP))},
    {"GFXINFO",		(ALLREP & ~(DEREGISREP|IDCORRREP))}
};

#endif /* __AMFIELDS_H */
