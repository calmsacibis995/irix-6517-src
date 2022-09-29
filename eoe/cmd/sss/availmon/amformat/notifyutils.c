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

#ident           "$Revision: 1.2 $"

/*---------------------------------------------------------------------------*/
/* notifyutils.c                                                             */
/* This file contains a bunch of notification utilities                      */
/*---------------------------------------------------------------------------*/

#include <amstructs.h>
#include <amnotify.h>
#include <amformat.h>
#include <amdefs.h>

/*---------------------------------------------------------------------------*/
/* Function Prototypes                                                       */
/*---------------------------------------------------------------------------*/

int readNotifyOptions(amConfig_t *, char *);
__uint32_t convertToFormatCode(char *);
void ClearReportOpts(amConfig_t *, __uint32_t);
int adjustNotifyConfig(amConfig_t *, __uint32_t);
void notifyReports(amConfig_t *);
extern void freerepparams(repparams_t *);
extern void freeConfig(amConfig_t *);
extern repparams_t *newrepparams(int);
extern emailaddrlist_t *newaddrnode(char *);
extern int insertaddr(repparams_t *, emailaddrlist_t *);

/*---------------------------------------------------------------------------*/
/* Name    : readNotifyOptions                                               */
/* Purpose : Reads the notification options from command-line and puts them  */
/*           into appropriate structures.                                    */
/* Inputs  : amConfig_t structure and the command-line string                */
/* Outputs : None                                                            */
/*---------------------------------------------------------------------------*/

int readNotifyOptions (amConfig_t *Config, char *String)
{
    int     Format = 0;
    char    *address = NULL;
    char    *address1 = NULL;
    emailaddrlist_t *node;

    if ( Config == NULL ) return(-1);
    if ( String == NULL ) return(-1);

    /*-----------------------------------------------------------------------*/
    /* OK, email string is of the following format :                         */
    /*    AVAIL_COMP:xx@xx.xx.xxx,yy@yy.yy.yyy, zz@zz.zz.zzz..               */
    /* In order to break the string, we first extract the first portion, that*/
    /* is AVAIL_COMP and check whether it is a valid format we can accept.   */
    /* If it is not valid, then return.  Otherwise, start breaking the email */
    /* addresses and put them in our linked list.                            */
    /*-----------------------------------------------------------------------*/

    address = strtok(String, ":");

    if ( (Format = convertToFormatCode(address)) == 0 ) {
	return(-1);
    } else {
	if ( Config->notifyParams[Format-1] == NULL ) {
	    if ( (Config->notifyParams[Format-1] = newrepparams(Format))
		 == NULL ) return(-1);
	}
    }

    /*-----------------------------------------------------------------------*/
    /* While breaking e-mail string, look for both a space and a comma.  Get */
    /* the e-mail address and insert it into Config->notifyParams[Format-1]  */
    /*-----------------------------------------------------------------------*/


    address1 = strtok(NULL, ":");

    while ( (address = strtok(address1, " ,")) != NULL ) {

	node = newaddrnode(address);
	if ( node != NULL ) {
	    insertaddr(Config->notifyParams[Format-1], node);
	}
        address1 = NULL;
    }

    return(0);
}

/*---------------------------------------------------------------------------*/
/* Name    : convertToFormatCode                                             */
/* Purpose : Converts a Format Description string to a FormatCode            */
/* Inputs  : FormatDescription                                               */
/* Outputs : FormatCode                                                      */
/*---------------------------------------------------------------------------*/

__uint32_t convertToFormatCode(char *FormatDescription)
{
    __uint32_t     Format = 0;
    int            found  = 0;

    if ( FormatDescription != NULL ) {
	for ( Format = 1; Format <= MAXTYPES; Format++ ) {
	    if ( strcmp(repTypestr[Format-1], FormatDescription) == 0 )
	    {
		found = 1;
		break;
	    }
	}
    }

    return ( (found > 0) ? Format : 0 );
}

/*---------------------------------------------------------------------------*/
/* Name    : ClearReportOpts                                                 */
/* Purpose : Cleans out a Report Format                                      */
/* Inputs  : amConfig_t and ReportType                                       */
/* Outputs : None                                                            */
/*---------------------------------------------------------------------------*/

void ClearReportOpts (amConfig_t *config, __uint32_t  ReportType)
{
    if ( config == NULL ) return;

    if ( config->notifyParams[ReportType-1] != NULL ) {
	freerepparams(config->notifyParams[ReportType-1]);
	config->notifyParams[ReportType-1] = NULL;
    }

    return;
}

/*---------------------------------------------------------------------------*/
/* Name    : adjustNotifyConfig                                              */
/* Purpose : Adjusts Notification options depending on the type of report    */
/* Inputs  : amConfig_t and eventcode                                        */
/* Outputs : 1 or 0                                                          */
/*---------------------------------------------------------------------------*/

int adjustNotifyConfig(amConfig_t *config, __uint32_t eventcode)
{
    repparams_t   *tmpRepParams = NULL;
    int    i = 0;

    if ( config== NULL ) return(-1);

    /*------------------------------------------------------------------------*/
    /* If it is a live report, we don't send AVAILABILITY_* reports.  Only    */
    /* DIAGNOSTIC reports are sent out.                                       */
    /* So, we free whatever is not required.                                  */
    /*------------------------------------------------------------------------*/

    if ( isliveerrrep(eventcode) ||
	 islivesysrep(eventcode) )
    {
	ClearReportOpts(config, PAGER);
	ClearReportOpts(config, AVAILABILITY);
	ClearReportOpts(config, AVAILABILITY_COMP);
	ClearReportOpts(config, AVAILABILITY_CENCR);
    } 
    
    /*------------------------------------------------------------------------*/
    /* If the report is not a controlled shutdown or unscheduled shutdown,    */
    /* we don't need to send PAGER reports.  So, clear those notification opts*/
    /*------------------------------------------------------------------------*/

    if ( !isunscheduled(eventcode) && !iscontrolled(eventcode) ) {
	ClearReportOpts(config, PAGER);
    } 

    /*------------------------------------------------------------------------*/
    /* If someone configured e-mail addresses to send a AVAILABILITY_CENCR    */
    /* report but not a AVAILABILITY report, we still need to generate an     */
    /* AVAILABILITY report because AVAILABILITY_CENCR is derived from         */
    /* AVAILABILITY report.  Same applies for other options and DIAGNOSTIC    */
    /*------------------------------------------------------------------------*/

    if ( (config->notifyParams[AVAILABILITY-1] == NULL) &&
	 (config->notifyParams[AVAILABILITY_COMP-1] != NULL ||
	  config->notifyParams[AVAILABILITY_CENCR-1] != NULL ) ) {

        config->notifyParams[AVAILABILITY-1] = newrepparams(AVAILABILITY);
	if ( config->notifyParams[AVAILABILITY-1] == NULL ) return(-1);
    }

    if ( (config->notifyParams[DIAGNOSTIC-1] == NULL) &&
	 (config->notifyParams[DIAGNOSTIC_COMP-1] != NULL ||
	  config->notifyParams[DIAGNOSTIC_CENCR-1] != NULL ) ) {

        config->notifyParams[DIAGNOSTIC-1] = newrepparams(DIAGNOSTIC);
	if ( config->notifyParams[DIAGNOSTIC-1] == NULL ) return(-1);
    }

    return(0);
}

/*---------------------------------------------------------------------------*/
/* Name    : notifyReports                                                   */
/* Purpose : Main notification routine                                       */
/* Inputs  : amConfig structure                                              */
/* Outputs : None                                                            */
/*---------------------------------------------------------------------------*/

void notifyReports(amConfig_t *Config)
{
    char         systemcmd[MAXINPUTLINE];
    int          i = 0, first = 1, index = 0;
    int          nobytes = 0;
    repparams_t *tmpRepParams = NULL;
    emailaddrlist_t *tmpaddrlist = NULL;
    char         *tmpfile = NULL;
    char         rettemplate[24] = "/usr/tmp/amr.en7wXXXXXX\0";

    if ( Config == NULL ) return;

    mktemp(rettemplate);
    if ( rettemplate[0] == '\0' ) {
        strcpy(rettemplate, "/usr/tmp/amr.en345FA785");
    }

    for ( i = 0; i < MAXTYPES; i++ ) {
	
	if ( Config->notifyParams[i] != NULL ) {
	    first  = 1;
	    nobytes = 0;
	    tmpRepParams = Config->notifyParams[i];
	    if ( tmpRepParams->addrlist != NULL ) {
	        tmpaddrlist = tmpRepParams->addrlist;
	        switch(tmpRepParams->reportType)
	        {
		    case PAGER:
		        nobytes = sprintf(systemcmd, 
				       "%s %s | %s ",
				       CATCMD, tmpRepParams->filename,
				       MAILCMD);
		        break;
		    case AVAILABILITY:
		    case DIAGNOSTIC:
		        nobytes = sprintf( systemcmd,
			        "%s %s | %s -s AMR-%s-T ",
				CATCMD,  tmpRepParams->filename, 
				MAILCMD, tmpRepParams->subject);
		        break;
		    case DIAGNOSTIC_COMP:
                    case AVAILABILITY_COMP:
                        index = tmpRepParams->reportType+1;
			tmpfile = Config->notifyParams[index-1]->filename;
			nobytes = sprintf( systemcmd,
				 "%s %s | %s %s | %s -s AMR-%s-Z-U ",
				 COMPRESSCMD, tmpfile,
				 ENCOCMD, rettemplate,
				 MAILCMD, 
				 Config->notifyParams[index-1]->subject);
		        break;
		    case DIAGNOSTIC_CENCR:
		    case AVAILABILITY_CENCR:
                        index = tmpRepParams->reportType+2;
			tmpfile = Config->notifyParams[index-1]->filename;
			nobytes = sprintf(systemcmd, 
				 "%s %s | %s %s | %s %s | %s -s AMR-%s-Z-X-U ",
				 COMPRESSCMD, tmpfile,
				 CRYPTCMD, AMR_CODE,
				 ENCOCMD,  rettemplate,
				 MAILCMD, 
				 Config->notifyParams[index-1]->subject);
		        break;
                    default:
		        break;
	        }


	        while ( tmpaddrlist != NULL ) {
		    nobytes += snprintf( &systemcmd[nobytes], (MAXINPUTLINE-nobytes),
				    "%c%s", ((first == 1) ? ' ': ','),
				    tmpaddrlist->address);
		    tmpaddrlist = tmpaddrlist->next;
		    first = 0;
	        }
	    system(systemcmd);

            /*---------------------------------------------------------------*/
     	    /* If we need to log an event indicating sucessful completion,   */
     	    /* then, we need do some more stuff here.  Lets call SEM API     */
     	    /*---------------------------------------------------------------*/

	    }
	}
    }
}
