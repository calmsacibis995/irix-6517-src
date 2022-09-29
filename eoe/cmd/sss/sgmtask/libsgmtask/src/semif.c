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

#ident           "$Revision: 1.3 $"

#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <eventmonapi.h>
#include "sgmtask.h"

/*---------------------------------------------------------------------------*/
/* Function Prototypes                                                       */
/*---------------------------------------------------------------------------*/

int SGMEXTISendSEMRuleConfig(int, int);
int SGMEXTISendSEMSgmOps(int, char *);
int SGMEXTISendToEventmon(char *, __uint32_t, char *);
void SGMEXTIReloadEventMonConfig(void);
extern int configure_rule(char *); 
extern int configure_sgm(char *); 
extern void errorexit(char *, int, int, char *, ...);

int SGMEXTISendSEMRuleConfig(int action_id, int type)
{
    char  buf[20];

    memset(buf, 0, sizeof(buf));

    switch(type)
    {
	case 1: 
	    snprintf(buf, sizeof(buf), "NEW %d", action_id);
	    break;
	case 2:
	    snprintf(buf, sizeof(buf), "DELETE %d", action_id);
	    break;
	case 3:
	    snprintf(buf, sizeof(buf), "UPDATE %d", action_id);
	    break;
        default:
	    return(0);
    }

    if (configure_rule(buf)) return(0);
    return(1);
}

int SGMEXTISendSEMSgmOps(int proc, char *sysid)
{
    char  buf[30];

    if ( !sysid ) return (0);
    memset(buf, 0, sizeof(buf));

    switch(proc)
    {
	case SUBSCRIBEPROC:
	    snprintf(buf, sizeof(buf), "SUBSCRIBE %s", sysid);
	    break;
	case UNSUBSCRIBEPROC:
	    snprintf(buf, sizeof(buf), "UNSUBSCRIBE %s", sysid);
	    break;
	default:
	    return(0);
    }

    if ( configure_sgm(buf)) return(0);
    return(1);
}

int SGMEXTISendToEventmon(char *host,__uint32_t type, char *data)
{
    
    if ( !data ) return(0);
    if ( !emapiIsDaemonInstalled()) return(0);
    if(host)
        return(emapiSendEvent(host, time(0), type, 0, data));
    else
        return(emapiSendEvent("localhost", time(0), type, 0, data));

}

void SGMEXTIReloadEventMonConfig(void)
{
    emapiDeclareDaemonReloadConfig();
}
