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
/* amformat.c                                                                */
/*                                                                           */
/*    Functionality :                                                        */
/*       - called out by decision support module with event data logged      */
/*         either by availmon or amdiag                                      */
/*       - creates event report that needs to be mailed out                  */
/*       - if encryption is needed, encrypts the report                      */
/*       - generates registration report based on command line options       */
/*       - Notifies generated reports to configured/specified e-mail         */
/*         addresses                                                         */
/*---------------------------------------------------------------------------*/

#include  <amformat.h>
#include  <amdefs.h>
#include  <amstructs.h>
#include  <unistd.h>
#include  <syslog.h>

#define   OPT_STR               "AVhrdLaRE:I:n:f:O:"

/*--------------------------------------------------------------------------*/
/* Some Global Variables                                                    */
/*--------------------------------------------------------------------------*/

Event_t        *Event   = NULL;            /* Global Event Structure        */
amConfig_t     amConfig;                   /* Global Configuration Structure*/
char           *sqlstr  = NULL;            /* Input for Parser              */
int            lflag    = 0;               /* Log Notify Event              */
int            aflag    = 0;               /* Auto notify to configured add */
int            Aflag    = 0;               /* Ignore autoemail config value */
char           *cmd     = NULL;            /* My program name               */

/*--------------------------------------------------------------------------*/
/* External Variables                                                       */
/*--------------------------------------------------------------------------*/

extern         int      debug_level;       /* Debug Level for error messages*/
extern         FILE     *yyout;            /* Output for parser             */

/*---------------------------------------------------------------------------*/
/* Function Prototypes                                                       */
/*---------------------------------------------------------------------------*/

int main(int, char **);
void PrintError(char *, int *);
int doreport(int);

/*---------------------------------------------------------------------------*/
/* main function                                                             */
/*   Parses arguments                                                        */
/*   Initializes Data Structures                                             */
/*---------------------------------------------------------------------------*/

int main (int argc, char **argv)
{
    FILE    *fp = NULL;
    char    tmpStr[MAXINPUTLINE];
    int     nobytes = 0;
    char    *filename = NULL;
    char    *tmpSqlStr = NULL;
    char    *hostname = NULL;
    int     eflag = 0, dflag = 0, rflag = 0, fflag = 0;
    int     oflag = 0;
    int     resendflag=0, dataflag=0;
    int     ErrNo=0;
    int     c=0;
    __uint32_t objId = 0;

    cmd = argv[0];
    aflag = lflag = 0;
    initConfig(&amConfig);

    while ( (c = getopt(argc, argv, OPT_STR)) != -1 ) {
	switch(c) {
	    case 'V':
	        printf("%s V%s\n", cmd, AMRVERSIONNUM);
	        exit(0);
	    case 'h':
		errorexit(cmd, 1, -1, 
			  "Usage: %s [-hV] [-v] %s", 
			  cmd,"[<notifyoptions> [-L]] [-R] <dataoptions>");
	    case 'v':
		debug_level = LOG_DEBUG;
		break;
	    case 'A':
		Aflag = 1;
		break;
	    case 'n':
		hostname = strdup(optarg);
		break;
	    case 'O':
		oflag = atoi(optarg);
		break;
	    case 'I':
		objId = atoi(optarg);
		break;
	    case 'L':
		lflag = 1;
		break;
	    case 'R':
		resendflag = 1;
		break;
	    case 'a':
		aflag = 1;
		break;
            case 'r':
		rflag = 1;
		break;
            case 'd':
		dflag = 1;
		break;
	    case 'E':
		if ( readNotifyOptions(&amConfig, optarg) < 0 )  {
		    PrintError("Error reading e-mail addresses for -E", &ErrNo);
		    eflag = 0;
                } else
		    eflag = 1;
		break;
	    case 'f':
		filename = optarg;
		fflag = 1;
		break;
	    case '?':
		errorexit(cmd, 1, -1, " ");
		break;
        }	
    }

    if ( optind < argc ) {
	nobytes = 0;
	for ( ; optind < argc; optind++ ) {
	    nobytes += sprintf(&tmpStr[nobytes], "%s ", argv[optind]);
	}

	if ( nobytes ) dataflag = 1;
    }

    /*----------------------------------------------------------------*/
    /* Check flags                                                    */
    /*----------------------------------------------------------------*/

    if ( dflag ) {
	if ( rflag ) 
	    PrintError("Can't have -r and -d at the same time",&ErrNo);
	if ( resendflag )
	    PrintError("Can't RESEND a DEREGISTRATION report", &ErrNo);
	if ( dataflag || objId || fflag ) 
	    PrintError("-d can't take any other data arguments",&ErrNo);
    } else if ( rflag ) {
	if ( dflag ) 
	    PrintError("Can't have -r and -d at the same time",&ErrNo);
	if ( resendflag )
	    PrintError("Can't RESEND a REGISTRATION report", &ErrNo);
	if ( dataflag || objId || fflag  ) 
	    PrintError("-r can't take any other data arguments",&ErrNo);
    } else if ( objId > 0 ) {
	if ( fflag || dataflag ) {
	    PrintError("Too many data options. (-I & -f/data)", &ErrNo);
	}
	if ( !resendflag ) 
	    PrintError("-I can be used only in conjunction with -R", &ErrNo);
    } else {
	if ( fflag == 0 && dataflag == 0 ) {
	    PrintError("No data to operate upon", &ErrNo);
	} else if ( fflag && dataflag ) {
	    PrintError("Too many data options. (-S & -f)", &ErrNo);
	} else if ( resendflag ) {
	    PrintError("-R can't be used with -f/data", &ErrNo);
	}
    }

    if ( !aflag && !eflag ) PrintError("No recipients for reports", &ErrNo);
    if ( ErrNo ) goto end;

    /*----------------------------------------------------------------*/
    /* Initialize Event Structure                                     */
    /*----------------------------------------------------------------*/

    if ( (Event = (Event_t *) malloc(sizeof(Event_t))) != NULL ) {
	initEvent(Event);
    } else {
	errorexit(cmd, 1, LOG_ERR, "Can't malloc memory for Event Strucutre");
    }

    if ( hostname != NULL ) {
	Event->hostname = hostname;
    }

    /*----------------------------------------------------------------*/
    /* Process Options                                                */
    /*----------------------------------------------------------------*/

    if ( rflag ) {

	getRegistrationData(Event, 1);

	/*-------------------------------------------------------------------*/
	/* If amformat is run with a -A option, then, irrespective of whether*/
        /* autoemail is turned on or off, we need to send a report           */
	/*-------------------------------------------------------------------*/

	if ( Aflag ) amConfig.configFlag |= (AUTOEMAIL|HINVUPDATE);

    } else if ( dflag ) {

	getRegistrationData(Event, 0);
	if ( Aflag ) amConfig.configFlag |= (AUTOEMAIL);

    } else if ( objId > 0 && resendflag ) {
	if ( read_data_from_ssdb(Event, objId) < 0 ) {
	    errorexit(cmd, 1, LOG_ERR, "Error reading data (%d) from Database",
		      objId);
	}
    } else {
        if ( fflag ) {
	    if ( (fp = fopen(filename, "r")) == NULL ) {
		errorexit(cmd, 1, LOG_ERR, "Can't open %s", filename);
			  
	    }

	    if ( (nobytes = fread(tmpStr, 1, MAXINPUTLINE, fp)) == 0 ) {
		errorexit(cmd, 1, LOG_ERR, "No data in input file");
	    } else 
	        fclose(fp);
	}

	sqlstr = (char *) strdup(tmpStr);
	tmpSqlStr = sqlstr;

	if ( (yyout = fopen("/dev/null", "w")) == NULL ) {
	    yyout = stderr;
	}

	if ( yyparse() ) {
	    errorexit(cmd, 0, LOG_INFO, "Parse error in %s", filename);
	}

	if ( yyout != stderr ) fclose(yyout);
    }

    /*----------------------------------------------------------------*/
    /* Send output to /dev/null by default                            */
    /*----------------------------------------------------------------*/

    if ( doreport(resendflag) < 0 ) 
	errorexit(cmd, 1, LOG_WARNING, "Error creating availmon report");

    end:
	if ( hostname) free(hostname);
	if ( sqlstr) free(tmpSqlStr);
        if ( tmpStr) free(tmpStr);
        if ( Event)  freeEvent(&Event);
	if ( ErrNo ) exit(1);
	exit(0);
}

/*---------------------------------------------------------------------------*/
/* PrintError                                                                */
/*    Calls errorexit to print the error messages                            */
/*---------------------------------------------------------------------------*/
 
void PrintError(char *message, int *error)
{
    errorexit(cmd, 0, LOG_DEBUG, "%s", message);
    (*error)++;
}

/*---------------------------------------------------------------------------*/
/* doreport                                                                  */
/*   Generates the actual availmon report and notifies it.                   */
/*---------------------------------------------------------------------------*/

int doreport(int rflag) {

    int   i = 0;
    int   j = 0;
    repparams_t  *tmpRepParams = NULL;

    /*-----------------------------------------------------------------------*/
    /* Get the system information like serial number, hostname etc.          */
    /*-----------------------------------------------------------------------*/

    if ( getConfig(Event, &amConfig) < 0 ) {
	return(-1);
    }

    /*-----------------------------------------------------------------------*/
    /* adjust notification configuration.  Some specific event reports have  */
    /* specific formats only that need to be notified.  For example, PAGER   */
    /* reports need to be generated only for shutdowns. No AVAILABILITY      */
    /* reports are needed for LIVE events.                                   */
    /*                                                                       */
    /* So, what we do here is we remove those nodes from amConfig structure  */
    /* that are not required.  So, amConfig structure will only contain      */
    /* those notification options that are required after this call          */
    /*-----------------------------------------------------------------------*/

    if (adjustNotifyConfig(&amConfig, Event->eventcode) < 0) {
	errorexit(cmd, 1, LOG_ERR, "Can't adjust Notification configuration");
    }

    /*-----------------------------------------------------------------------*/
    /* Now, start generating reports                                         */
    /*-----------------------------------------------------------------------*/

    for ( i = 0; i < PAGER; i++ ) 
    {
	switch(i+1)
	{
	    case AVAILABILITY:
	    case DIAGNOSTIC:
	    case PAGER:
		tmpRepParams = amConfig.notifyParams[i];
		if ( tmpRepParams != NULL ) {
		    tmpRepParams->filename = tempnam(NULL, "amr");
#ifdef DEBUG
                    errorexit(cmd, 0, LOG_DEBUG, "Filename %s", 
			      tmpRepParams->filename);
#endif
		    if ( PrintReport(Event, tmpRepParams,
				     amConfig.configFlag, rflag) < 0 ) {
			errorexit(cmd, 0, LOG_ERR, 
				  "Can't create report (%d:%s)", i,
				  tmpRepParams->filename);
			return(-1);
                    }
		}
		break;
	    default:
		break;
	}
    }

    if ( (amConfig.configFlag & AUTOEMAIL) ) { 
	notifyReports(&amConfig);
    }  

    for ( i = 0; i < PAGER; i++ ) {
	tmpRepParams = amConfig.notifyParams[i];
	if ( tmpRepParams != NULL && tmpRepParams->filename != NULL ) 
	    unlink(tmpRepParams->filename);
    }

    return(0);
}

