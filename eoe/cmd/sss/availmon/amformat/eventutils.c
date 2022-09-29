/*---------------------------------------------------------------------------*/
/*                                                                           */
/* Copyright 1992-1998 Silicon Graphics, Inc.                                */
/* All Rights Reserved.                                                      */
/*                                                                           */
/* This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics Inc.;     */
/* contents of this file may not be disclosed to third parties, copied or    */
/* duplicated in any form, in whole or in part, without the prior written    */
/* permission of Silicon Graphics, Inc.                                      */
/*                                                                           */
/* RESTRICTED RIGHTS LEGEND:                                                 */
/* Use,duplication or disclosure by the Government is subject to restrictions*/
/* as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data    */
/* and Computer Software clause at DFARS 252.227-7013, and/or in similar or  */
/* successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -   */
/* rights reserved under the Copyright Laws of the United States.            */
/*                                                                           */
/*---------------------------------------------------------------------------*/

#ident           "$Revision: 1.1 $"

/*---------------------------------------------------------------------------*/
/* eventutils.c                                                              */
/* A bunch of utilities related to events                                    */
/*---------------------------------------------------------------------------*/

#include <amstructs.h>
#include <amevent.h>

#if   !defined(TRUE) || ((TRUE) != 1)
#define TRUE                 1
#endif

#if   !defined(FALSE) || ((FALSE) != 0 ) 
#define FALSE                0
#endif

/*---------------------------------------------------------------------------*/
/* Function Prototypes                                                       */
/*---------------------------------------------------------------------------*/

int issuevent(__uint32_t);
int ismuevent(__uint32_t);
int isderegis(__uint32_t);
int isregis(__uint32_t);
int isidcorr(__uint32_t);
int isstatusupd(__uint32_t);
int isliveerrrep(__uint32_t);
int islivesysrep(__uint32_t);
int isunscheduled(__uint32_t);
int iscontrolled(__uint32_t);
int ismetric(__uint32_t);
int isnonmetric(__uint32_t);
int isvalid(__uint32_t);
char *getEventDescription(int);

/*---------------------------------------------------------------------------*/
/* Event Descriptions                                                        */
/*---------------------------------------------------------------------------*/

static amrEvDesc_t EventDescriptions[] = {
    {MU_UND_NC,        "Controlled shutdown (unknown)"},
    {MU_UND_TOUT,      "Controlled shutdown (timeout)"},
    {MU_UND_UNK,       "Controlled shutdown (unknown)"},
    {MU_UND_ADM,       "Controlled shutdown (1)"},
    {MU_HW_UPGRD,      "Controlled shutdown (2)"},
    {MU_SW_UPGRD,      "Controlled shutdown (3)"},
    {MU_HW_FIX,        "Controlled shutdown (4)"},
    {MU_SW_PATCH,      "Controlled shutdown (5)"},
    {MU_SW_FIX,        "Controlled shutdown (6)"},
    {SU_UND_TOUT,      "Singleuser shutdown (unknown)"},
    {SU_UND_NC,        "Singleuser shutdown (unknown)"},
    {SU_UND_ADM,       "Singleuser shutdown (1)"},
    {SU_HW_UPGRD,      "Singleuser shutdown (2)"},
    {SU_SW_UPGRD,      "Singleuser shutdown (3)"},
    {SU_HW_FIX,        "Singleuser shutdown (4)"},
    {SU_SW_PATCH,      "Singleuser shutdown (5)"},
    {SU_SW_FIX,        "Singleuser shutdown (6)"},
    {UND_PANIC,        "Panic"},
    {HW_PANIC,         "Panic (H/W)"},
    {UND_INTR,         "Interrupt"},
    {UND_SYSOFF,       "System off"},
    {UND_PWRFAIL,      "Power failure"},
    {SW_PANIC,         "Panic (S/W)"},
    {UND_NMI,          "NMI"},
    {UND_RESET,        "System reset"},
    {UND_PWRCYCLE,     "Power cycle"},
    {DE_REGISTER,      "Deregistration"},
    {REGISTER,         "Registration"},
    {LIVE_NOERR,       "No error"},
    {LIVE_HWERR,       "Hardware error"},
    {LIVE_SWERR,       "Software error"},
    {STATUS,           "Status report"},
    {ID_CORR,          "System ID change"},
    {LIVE_SYSEVNT,     "Live event"},
    {NOTANEVENT,       "Unknown reasons"}
};

/*---------------------------------------------------------------------------*/
/* Name    : issuevent                                                       */
/* Purpose : Returns 1 if eventcode is a Single user event otherwise 0       */
/* Inputs  : eventcode                                                       */
/* Outputs : 1 or 0                                                          */
/*---------------------------------------------------------------------------*/

int 
issuevent(__uint32_t eventcode)
{
    switch(eventcode) {
	case SU_UND_TOUT:
        case SU_UND_NC :
        case SU_UND_ADM:
        case SU_HW_UPGRD:
        case SU_SW_UPGRD:
        case SU_HW_FIX:
        case SU_SW_PATCH:
        case SU_SW_FIX:
	    return(TRUE);
	default:
	    return(FALSE);
    }
}

/*---------------------------------------------------------------------------*/
/* Name    : ismuevent                                                       */
/* Purpose : Determines whether given eventcode is a multi user reboot event */
/* Inputs  : eventcode                                                       */
/* Outputs : 1 or 0                                                          */
/*---------------------------------------------------------------------------*/

int 
ismuevent (__uint32_t eventcode)
{
    switch(eventcode) {
	case MU_UND_UNK:
	case MU_UND_TOUT:
        case MU_UND_NC :
        case MU_UND_ADM:
        case MU_HW_UPGRD:
        case MU_SW_UPGRD:
        case MU_HW_FIX:
        case MU_SW_PATCH:
        case MU_SW_FIX:
	    return(TRUE);
	default:
	    return(FALSE);
    }
}

/*---------------------------------------------------------------------------*/
/* Name    : isderegis                                                       */
/* Purpose : Determines whether given event code is a deregistration event   */
/* Inputs  : eventcode                                                       */
/* Outputs : 1 or 0                                                          */
/*---------------------------------------------------------------------------*/

int 
isderegis(__uint32_t eventcode)
{
    if ( eventcode == DE_REGISTER ) 
	return(TRUE);
    return(FALSE);
}

/*---------------------------------------------------------------------------*/
/* Name    : isregis                                                         */
/* Purpose : Determines whether given eventcode relates to a registration    */
/* Inputs  : eventcode                                                       */
/* Outputs : 1 or 0                                                          */
/*---------------------------------------------------------------------------*/

int 
isregis(__uint32_t eventcode)
{
    if ( eventcode == REGISTER ) 
	return(TRUE);
    return(FALSE);
}

/*---------------------------------------------------------------------------*/
/* Name    : isidcorr                                                        */
/* Purpose : Determines whether given eventcode relates to an ID Correction  */
/* Inputs  : eventcode                                                       */
/* Outputs : 1 or 0                                                          */
/*---------------------------------------------------------------------------*/

int 
isidcorr(__uint32_t eventcode)
{
    if ( eventcode == ID_CORR ) 
	return(TRUE);
    return(FALSE);
}

/*---------------------------------------------------------------------------*/
/* Name    : isstatusupd                                                     */
/* Purpose : Determines whether given event code is a status update          */
/* Inputs  : eventcode                                                       */
/* Outputs : 1 or 0                                                          */
/*---------------------------------------------------------------------------*/

int 
isstatusupd(__uint32_t eventcode)
{
    if ( eventcode == STATUS )
	return(TRUE);
    return(FALSE);
}

/*---------------------------------------------------------------------------*/
/* Name    : isliveerrrep                                                    */
/* Purpose : Determines whether given event code is a live error report      */
/* Inputs  : eventcode                                                       */
/* Outputs : 1 or 0                                                          */
/*---------------------------------------------------------------------------*/

int 
isliveerrrep(__uint32_t eventcode)
{
    switch(eventcode) {
	case LIVE_NOERR:
	case LIVE_HWERR:
	case LIVE_SWERR:
	    return(TRUE);
        default:
	    return(FALSE);
    }
}

/*---------------------------------------------------------------------------*/
/* Name    : islivesysrep                                                    */
/* Purpose : Determines whether given event code is live system event        */
/* Inputs  : eventcode                                                       */
/* Outputs : 1 or 0                                                          */
/*---------------------------------------------------------------------------*/

int
islivesysrep(__uint32_t eventcode)
{
    if ( eventcode == LIVE_SYSEVNT ) 
	return(TRUE);
    return(FALSE);
}

/*---------------------------------------------------------------------------*/
/* Name    : isunscheduled                                                   */
/* Purpose : Determines whether given event code is an unscheduled reboot    */
/* Inputs  : eventcode                                                       */
/* Outputs : 1 or 0                                                          */
/*---------------------------------------------------------------------------*/

int 
isunscheduled(__uint32_t eventcode)
{

    switch(eventcode) {
	case UND_PWRCYCLE:
	case UND_RESET:
	case UND_NMI:
	case SW_PANIC:
	case UND_PWRFAIL:
	case UND_SYSOFF:
	case UND_INTR:
	case HW_PANIC:
	case UND_PANIC:
	    return(TRUE);
        default:
	    return(FALSE);
    }
}

/*---------------------------------------------------------------------------*/
/* Name    : iscontrolled                                                    */
/* Purpose : Determines whether given event code is a controlled shutdown    */
/* Inputs  : eventcode                                                       */
/* Outputs : 1 or 0                                                          */
/*---------------------------------------------------------------------------*/

int 
iscontrolled(__uint32_t eventcode)
{
    if ( issuevent(eventcode) || ismuevent(eventcode) )
	return(TRUE);
    else
	return(FALSE);
}

/*---------------------------------------------------------------------------*/
/* Name    : ismetric                                                        */
/* Purpose : Determines whether given eventcode can be used as a metric data */
/* Inputs  : eventcode                                                       */
/* Outputs : 1 or 0                                                          */
/*---------------------------------------------------------------------------*/

int 
ismetric(__uint32_t eventcode)
{
    if ( iscontrolled(eventcode) || isunscheduled(eventcode) )
	return(TRUE);
    return(FALSE);
}

/*---------------------------------------------------------------------------*/
/* Name    : isnonmetric                                                     */
/* Purpose : Determines whether given eventcode can't be used as a metric dat*/
/* Inputs  : eventcode                                                       */
/* Outputs : 1 or 0                                                          */
/*---------------------------------------------------------------------------*/

int
isnonmetric(__uint32_t eventcode)
{
    if ( islivesysrep(eventcode) ||
	 isliveerrrep(eventcode) ||
	 isderegis(eventcode)    ||
	 isregis(eventcode)      ||
	 isidcorr(eventcode)     ||
	 isstatusupd(eventcode) ) return (TRUE);
    return(FALSE);
}

/*---------------------------------------------------------------------------*/
/* Name    : isvalid                                                         */
/* Purpose : Determines whether a given eventcode is valid                   */
/* Inputs  : eventcode                                                       */
/* Outputs : 1 or 0                                                          */
/*---------------------------------------------------------------------------*/

int
isvalid(__uint32_t eventcode)
{
    if ( ismetric(eventcode) || isnonmetric(eventcode) )
	return(TRUE);
    return(FALSE);
}


/*---------------------------------------------------------------------------*/
/* Name    : getEventDescription                                             */
/* Purpose : Takes an eventcode and returns text description for that event  */
/* Inputs  : eventcode                                                       */
/* Outputs : Text description of the eventcode                               */
/*---------------------------------------------------------------------------*/

char *getEventDescription(int EventCode)
{
    int   i = 0;

    for ( i = 0; EventDescriptions[i].eventcode != NOTANEVENT; i++)
    {
        if (EventDescriptions[i].eventcode == EventCode ) {
            return (EventDescriptions[i].description);
        }
    }

    /*
       Not reached.  Should only come here in case of a wrong
       event code
    */

    return (EventDescriptions[i].description);
}

