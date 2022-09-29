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

%{

#include <lexyacc.h>
#include <amstructs.h>

extern Event_t  *Event;
static unsigned int fieldno = 3;

__uint64_t populateEventData(Event_t *, char *, FieldNos_t);
static long        convertint(char *);
static char        *convertchar(char *);

%}

%token  <string>	INTNUM  200
%token  <string>	STRING	201
%token  <string>	NOVAL	202

%%

field_list:
	field
	| field_list ',' field 
	;

field:
	INTNUM
	    {
		fieldno++;
		populateEventData(Event, $1, (FieldNos_t) fieldno);
		free($1);
	    }
	| STRING
	    {
		fieldno++;
		populateEventData(Event, $1, (FieldNos_t) fieldno);
	    }
	| NOVAL
	    {
		fieldno++;
		populateEventData(Event, $1, (FieldNos_t) fieldno);
	    }
	;
%%

/*------------------------------------------------------------------*/
/* populateEventData                                                */
/*  Populates parsed event data                                     */
/*------------------------------------------------------------------*/

__uint64_t populateEventData(Event_t *event, char *data, 
			     FieldNos_t fieldno)
{
    if ( event == NULL ) return (-1);
    if ( data  == NULL ) return (-1);
    if ( fieldno == 0  ) return (-1);

    switch(fieldno) {
	case EVENTCODE:
	    event->eventcode = (__uint32_t) convertint(data);
	    break;
	case EVENTTIME:
	    event->eventtime = (time_t) convertint(data);
	    break;
	case LASTTICK:
	    event->lasttick  = (time_t) convertint(data);
	    break;
	case PREVSTART:
	    event->prevstart  = (time_t) convertint(data);
	    break;
	case STARTTIME:
	    event->starttime  = (time_t) convertint(data);
	    break;
	case STATUSINTERVAL:
	    event->statusinterval  = (__uint32_t) convertint(data);
	    break;
	case BOUNDS:
	    event->bounds  = (__uint32_t) convertint(data);
	    break;
	case DIAGTYPE:
	    event->diagtype  = (char *) convertchar(data);
	    break;
	case DIAGFILE:
	    event->diagfile  = (char *) convertchar(data);
	    break;
	case SYSLOG:
	    event->syslog  = (char *) convertchar(data);
	    break;
	case HINVCHANGE:
	    event->hinvchange  = (__uint32_t) convertint(data);
	    break;
	case VERCHANGE:
	    event->verchange  = (__uint32_t) convertint(data);
	    break;
	case METRICS:
	    event->metrics  = (__uint32_t) convertint(data);
	    break;
	case FLAG:
	    event->flag  = (__uint32_t) convertint(data);
	    break;
	case SHUTDOWNCOMMENT:
	    event->shutdowncomment  = (char *) convertchar(data);
	    break;
	case SUMMARY:
	    event->summary  = (char *) convertchar(data);
	    break;
	default:
	    break;
    }

    return(0);
}

/*------------------------------------------------------------------*/
/* convertint                                                       */
/*  Takes a character pointer and converts it to long               */
/*------------------------------------------------------------------*/

long  convertint(char *data) {

    if ( strcasecmp("NULL", data) == 0 ) {
	return( (long) 0);
    } else {
	return( atoi(data) );
    }
}

/*------------------------------------------------------------------*/
/* convertint                                                       */
/*  Takes a character pointer and interprets it                     */
/*------------------------------------------------------------------*/

char *convertchar(char *data) {
    if ( strcasecmp("NULL", data) == 0 ) {
	free(data);
	return( (char *) NULL ) ;
    } else {
	return( data );
    }
}

