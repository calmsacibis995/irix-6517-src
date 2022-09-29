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

/*---------------------------------------------------------------------------*/
/* reportutils.c                                                             */
/*                                                                           */
/*     various utilities used to print AVAILABILITY, DIAGNOSTIC & PAGER      */
/*     reports                                                               */
/*                                                                           */
/*     Todo :                                                                */
/*       - Include NOTMULTIUSER in START & PREVSTART                         */
/*       - Change check[Hinv|Versions|Gfx]Change to read from SSDB for       */
/*         actual changes (integration with ConfigMon)                       */
/*---------------------------------------------------------------------------*/

#include <amstructs.h>
#include <amformat.h>
#include <amfields.h>
#include <amevent.h>
#include <amdefs.h>
#include <time.h>
#include <sys/times.h>
#include <libgen.h>

/*---------------------------------------------------------------------------*/
/* Function Prototypes                                                       */
/*---------------------------------------------------------------------------*/

void getRegistrationData(Event_t *, int);
__uint32_t getReportOpts(__uint32_t);
int PrintReport(Event_t *, repparams_t *, __uint32_t, int);
int checkHinvChange(Event_t *);
int checkGfxChange(Event_t *);
int checkVersionsChange(Event_t *);

/*---------------------------------------------------------------------------*/
/* Macros                                                                    */
/*---------------------------------------------------------------------------*/

#define PrintFieldName(x,y)	( fprintf(x, "%s%s", \
				  repFields[y].field_name, SEPERATOR))

/*---------------------------------------------------------------------------*/
/* Name    : getRegistrationData                                             */
/* Purpose : Generates required data for a Registration Event                */
/* Inputs  : Event Structure and a flag                                      */
/* Outputs : None                                                            */
/*---------------------------------------------------------------------------*/

void getRegistrationData(Event_t *event, int flag)
{
    FILE  *fp;
    char  buffer[20] = "0";

    if (event == NULL ) return;

    if ( flag ) {
        event->eventcode = REGISTER;
        event->hinvchange = 1;
	event->verchange  = 1;
    } else {
	event->eventcode = DE_REGISTER;
        event->hinvchange = 0;
	event->verchange  = 0;
    }

    event->eventtime = time(0);

    /*-----------------------------------------------------------------------*/
    /* Open PREVSTARTFILE and get the value of prevstart                     */
    /*-----------------------------------------------------------------------*/

    if ( (fp = fopen(PREVSTARTFILE, "r")) != NULL ) {
	fread(buffer, 1, 20, fp);
	event->prevstart = atoi(buffer);
	fclose(fp);
    }

    if ( (fp = fopen(LASTTICKFILE, "r")) != NULL ) {
	fread(buffer, 1, 20, fp);
	event->lasttick = atoi(buffer);
	fclose(fp);
    }

    return;
}

/*---------------------------------------------------------------------------*/
/* Name    : getReportOpts                                                   */
/* Purpose : Determines which type of report to generate                     */
/* Inputs  : Eventcode                                                       */
/* Outputs : Type of Report                                                  */
/*---------------------------------------------------------------------------*/

__uint32_t getReportOpts(__uint32_t EventCode) 
{
    if ( !isvalid(EventCode) ) {
        return(NOTANEVENT);
    } else if (iscontrolled(EventCode)) {
        return(REBOOTREP);
    } else if (isderegis(EventCode)) {
        return(DEREGISREP);
    } else if (isregis(EventCode)) {
        return(REGISREP);
    } else if (isidcorr(EventCode)) {
        return(IDCORRREP);
    } else if (isstatusupd(EventCode)) {
        return(STATUSREP);
    } else if (isunscheduled(EventCode)) {
        return(UNSCHEDREP);
    } else if (isliveerrrep(EventCode)) {
        return(LIVEERRREP);
    } else if (islivesysrep(EventCode)) {
        return(LIVESYSREP);
    } else {
        return(NOTANEVENT);
    }
}

/*---------------------------------------------------------------------------*/
/* Name    : PrintReport                                                     */
/* Purpose : Main utility to generate Availmon Report                        */
/* Inputs  : Event Structure, report types, Configuration flag and resendflag*/
/* Outputs : -1 or 0                                                         */
/*---------------------------------------------------------------------------*/

int PrintReport(Event_t *event, repparams_t *nParam, __uint32_t ConfFlag, 
		int resendflag)
{


    FILE        *outFile = NULL;
    __uint32_t  ReportOpts = 0;
    char        tmpBuffer[MAXVARCHARSIZE];
    char        tmpBuffer1[MAXVARCHARSIZE];
    int         noBytes = 0;
    int         iFlag = 0;        /* Flag for interesting Report */

    if ( event == NULL ) return(-1);

    if ( nParam == NULL ) return (-1);
    else if ( nParam->filename == NULL ) {
	outFile = stdout;
    } else {
	if ( (outFile = fopen(nParam->filename, "w")) == NULL ) {
	    return(-1);
	}
    }


    if ( nParam->reportType == PAGER ) {
	fprintf(outFile, "%s%s\n", 
		((event->hostname != NULL) ? event->hostname : "unknown"),
		SEPERATOR);

	fprintf(outFile, "%s\n", 
	        (char *) getEventDescription(event->eventcode));
        
	if ( event->bounds >= 0 ) {
	    fprintf(outFile, "%s%s\n", repFields[SUMMARYVAL].field_name,
		    SEPERATOR);
	    if ( event->summary && !(event->flag & SUMMARYISFILE) ) {
		fprintf(outFile, "%s\n", 
			strccpy(tmpBuffer,event->summary));
	    } else {
		fprintf(outFile, "unknown\n");
	    }
	} 

    } else {

	if ( (ReportOpts = getReportOpts(event->eventcode)) == 0 ) 
	    return(-1);

        /*-------------------------------------------------------------------*/
        /* First, print out the header of Availmon report which contains     */
        /* SERIALNUM, HOSTNAME, AMRVERSION and UNAME.  In case a report is   */
        /* being resent, we print out RESEND also.                           */
        /*-------------------------------------------------------------------*/

	if ( ReportOpts & repFields[SERIALNUMVAL].flag ) {
	    PrintFieldName(outFile, SERIALNUMVAL);
	    fprintf(outFile, "%s\n", 
		    ((event->serialnum != NULL) ? event->serialnum:"unknown"));

	}

	if ( ReportOpts & repFields[HOSTNAMEVAL].flag ) {
	    PrintFieldName(outFile, HOSTNAMEVAL);
	    fprintf(outFile, "%s\n",
		    ((event->hostname != NULL) ? event->hostname:"unknown"));
        }


	if ( ReportOpts & repFields[AMRVERSIONVAL].flag ) {
	    fprintf(outFile, "%s%s%s\n", 
		    repFields[AMRVERSIONVAL].field_name, 
		    SEPERATOR, AMRVERSIONNUM);
        }

	if ( resendflag ) {
	    fprintf(outFile, "RESEND|\n");
	}

        if ( ReportOpts & repFields[UNAMEVAL].flag ) {
	    PrintFieldName(outFile, UNAMEVAL);
	    if (checkCmd(UNAME))
		appendOutput(outFile, UNAMECOMMAND, ISCOMMAND,NULL);
        }

        /*-------------------------------------------------------------------*/
        /* After the header of the report is printed out, we need to output  */
        /* the eventtime and prevstart.  A note on prevstart :  If the mach  */
        /* ne is brought up from a single user mode, then prevstart needs to */
        /* identify the previous event as not a multi user event.  We deter  */
        /* mine this by looking at the value of event->flag.  If this flag   */
        /* (which is set by /etc/init.d/availmon) says that the previous even*/
        /* t is Single User, then we include NOTMULTIUSER in the line that is*/
        /* printed out.                                                      */
        /*-------------------------------------------------------------------*/

	if ( ReportOpts & repFields[PREVSTARTVAL].flag ) {
	    PrintFieldName(outFile, PREVSTARTVAL);
	    fprintf(outFile, "%ld%s%.24s",
		    ((event->prevstart > 0) ? event->prevstart : -1),
		    SEPERATOR,
		    ((event->prevstart>0)?ctime(&event->prevstart):"unknown"));

	    if ( event->flag & PREVEVENTISSU )
		fprintf(outFile, "|NOTMULTIUSER");

	    fprintf(outFile, "\n");
        }

	if ( ReportOpts & repFields[EVENTVAL].flag ) {
	    PrintFieldName(outFile, EVENTVAL);
	    fprintf(outFile, "%d%s", TOOLD(event->eventcode),SEPERATOR);
	    fprintf(outFile, "%ld%s%s",
		    ((event->eventtime > 0) ? event->eventtime : -1),
		    SEPERATOR,
		    ((event->eventtime>0)?ctime(&event->eventtime):"unknown\n"));
        }

	if ( ReportOpts & repFields[OLDSERIALNUMVAL].flag) {
	    if ( event->flag & DIAGTYPE_OSERL ) {
	        PrintFieldName(outFile, OLDSERIALNUMVAL);
	        fprintf(outFile, "%s\n", 
			((event->diagtype != NULL) ? event->diagtype : "unknown"));
                noBytes = snprintf(&tmpBuffer1[noBytes], 
				   MAXVARCHARSIZE,
				   "%s%s%s",
				   repFields[OLDSERIALNUMVAL].field_name,
				   SEPERATOR,
				   ((event->diagtype != NULL) ? event->diagtype : "unknown"));
                event->summary = tmpBuffer1;
	    }
	}


	if ( ReportOpts & repFields[OLDHOSTNAMEVAL].flag) {
	    if ( event->flag & DIAGFILE_OHOST ) {
	        PrintFieldName(outFile, OLDHOSTNAMEVAL);
	        fprintf(outFile, "%s\n", 
			((event->diagfile != NULL) ? event->diagfile : "unknown"));
		if ( noBytes > 0 ) {
		    noBytes += snprintf(&tmpBuffer1[noBytes],
					MAXVARCHARSIZE, "\n");
		}

                noBytes = snprintf(&tmpBuffer1[noBytes], 
				   MAXVARCHARSIZE,
				   "%s%s%s",
				   repFields[OLDHOSTNAMEVAL].field_name,
				   SEPERATOR,
				   ((event->diagfile != NULL) ? event->diagfile : "unknown"));
                event->summary = tmpBuffer1;
	    }
	}

	if ( ReportOpts & repFields[LASTTICKVAL].flag ) {
	    PrintFieldName(outFile, LASTTICKVAL);
	    fprintf(outFile, "%ld%s%s",
		    ((event->lasttick > 0) ? event->lasttick : -1),
		    SEPERATOR,
		    ((event->lasttick>0)?ctime(&event->lasttick):"unknown\n"));
        }

	if ( ReportOpts & repFields[STATUSINTERVALVAL].flag ) {
	    PrintFieldName(outFile, STATUSINTERVALVAL);
	    fprintf(outFile, "%d\n",
		    ((event->statusinterval > 0) ? event->statusinterval:0));
        }

        /*-------------------------------------------------------------------*/
        /* We need to include NOTMULTIUSER in start field if the event is    */
        /* a single user event.  See comments for PREVSTART above.           */
        /*-------------------------------------------------------------------*/

	if ( ReportOpts & repFields[STARTVAL].flag ) {
	    PrintFieldName(outFile, STARTVAL);
	    fprintf(outFile, "%ld%s%.24s",
		    ((event->starttime > 0) ? event->starttime : -1),
		    SEPERATOR,
		    ((event->starttime>0)?ctime(&event->starttime):"unknown"));

	    if ( event->flag & EVENTISSU )
		fprintf(outFile, "|NOTMULTIUSER");

	    fprintf(outFile, "\n");
	}

	if ( ReportOpts & repFields[SUMMARYVAL].flag ) {
	    if ( event->summary ) {
	        if ( event->flag & SUMMARYISFILE ) {
		    appendOutput(outFile, event->summary, !ISCOMMAND, "SUMMARY");
	        } else {
		    fprintf(outFile, "SUMMARY|Begin\n%s\nSUMMARY|End\n",
			    strccpy(tmpBuffer,event->summary));
	        }
	    }
	}

	if ( ReportOpts & repFields[DEREGISTRATIONVAL].flag ) {
	    fprintf(outFile, "%s%s\n", 
		    repFields[DEREGISTRATIONVAL].field_name,
		    SEPERATOR);
	}

	if ( ReportOpts & repFields[REGISTRATIONVAL].flag ) {
	    fprintf(outFile, "%s%s\n", 
		    repFields[REGISTRATIONVAL].field_name,
		    SEPERATOR);
	}

        /*-------------------------------------------------------------------*/
        /* Now, we have printed out all the fields that are common to an     */
        /* availability and diagnostic reports.  We need to do some more work*/
        /* if the report is a diagnostic report.                             */
        /*-------------------------------------------------------------------*/

	if ( nParam->reportType == DIAGNOSTIC && !isidcorr(event->eventcode)){

	    if ( event->diagtype != NULL ) {
		if ( event->diagfile != NULL ) {
		    appendOutput(outFile, event->diagfile, !ISCOMMAND,
				 event->diagtype);
		    iFlag++;
		} else {
		    fprintf(outFile, "%s%sBegin\nunknown\n%s%sEnd\n",
			    event->diagtype, SEPERATOR, event->diagtype,
			    SEPERATOR);
		}
	    }

	    if ( ReportOpts & repFields[SYSLOGVAL].flag ) {
		if ( event->syslog && !resendflag ) {
		    appendOutput(outFile, event->syslog, !ISCOMMAND,
				 repFields[SYSLOGVAL].field_name);
		    iFlag++;
		} else if ( resendflag ) {
		    if ( event->bounds >= 0  && event->syslog ) {
			appendOutput(outFile, event->syslog, !ISCOMMAND,
				     repFields[SYSLOGVAL].field_name);
		    } else {
			fprintf(outFile, "%s%sBegin\n", 
				repFields[SYSLOGVAL].field_name, SEPERATOR);
			fprintf(outFile,"Can't recreate SYSLOG information\n");
			fprintf(outFile, "%s%sEnd\n",
				repFields[SYSLOGVAL].field_name, SEPERATOR);
		    }
		} else {
		    fprintf(outFile, "%s%sBegin\nunknown\n%s%sEnd\n",
			    repFields[SYSLOGVAL].field_name,SEPERATOR,
			    repFields[SYSLOGVAL].field_name,SEPERATOR);
		}
	    }

            /*---------------------------------------------------------------*/
            /* The diagnostic report normally contains VERSIONS, HINV and    */
            /* GFXINFO information.  This information is sent in a report on */
            /* a change being identified.  HINV and GFXINFO are sent only if */
            /* 'hinvupdate' flag is turned on.  VERSIONS is sent whenever a  */
            /* change in versions info. is identified.  The startup script   */
            /* of Availmon identifies whether a change occured in HINV or    */
            /* VERSIONS and sends us the flag to update or not               */
            /*---------------------------------------------------------------*/

	    if ( (ReportOpts & repFields[VERSIONSVAL].flag) && !resendflag ) {
		if ( checkVersionsChange(event) && checkCmd(VERSIONS) ) {
		    appendOutput(outFile, VERSIONSCOMMAND, ISCOMMAND,
				 "VERSIONS");
		    iFlag++;
		}
	    }

	    if ( (ReportOpts & repFields[HINVVAL].flag) && !resendflag ) {
		if ( (ConfFlag & HINVUPDATE) && checkHinvChange(event) && checkCmd(HINV) ) {
		    appendOutput(outFile, HINVCOMMAND, ISCOMMAND,
				 "HINV");
		    iFlag++;
		}
	    }

	    if ( (ReportOpts & repFields[GFXINFOVAL].flag) && !resendflag ) {
		if ( (ConfFlag & HINVUPDATE) && checkGfxChange(event) && checkCmd(GFX) ) {
		    appendOutput(outFile, GFXCOMMAND, ISCOMMAND,
				 "GFXINFO");
		    iFlag++;
		}
	    }

            /*---------------------------------------------------------------*/
            /* Since availmon reports are identified based on the subject by */
            /* amreceive, we need to put proper subject in the report        */
            /*---------------------------------------------------------------*/

	    if ( iFlag ) {
		nParam->subject[0] = 'I';
	    } else {
		nParam->subject[0] = 'D';
	    }
	} else if (nParam->reportType==DIAGNOSTIC &&isidcorr(event->eventcode)){
	    nParam->subject[0] = 'I';
	} else {
	    nParam->subject[0] = 'A';
	}

        /*-------------------------------------------------------------------*/
        /* In case of a REGISTRATION/DEREGISTRATION/IDCORRECTION reports, the*/
        /* subject should contain -R so that amreceive can identify it       */
        /*-------------------------------------------------------------------*/

	if ( isidcorr(event->eventcode) || isregis(event->eventcode)
	     || isderegis(event->eventcode) ) {
	    nParam->subject[1] = '-';
	    nParam->subject[2] = 'R';
        }
    }

    if ( outFile ) fclose(outFile);
    return(iFlag);
}

/*---------------------------------------------------------------------------*/
/* Name    : checkHinvChange                                                 */
/* Purpose : Checks whether HINV data needs to be incorporated in the report */
/* Inputs  : None                                                            */
/* Outputs : 1 or 0                                                          */
/*---------------------------------------------------------------------------*/

int checkHinvChange(Event_t *e) 
{
    if ( e->hinvchange > 0 ) return(1);
    return(0);
}

/*---------------------------------------------------------------------------*/
/* Name    : checkGfxChange                                                  */
/* Purpose : Checks whether GFXINFO data needs to go in a report or not      */
/* Inputs  : None                                                            */
/* Outputs : 1 or 0                                                          */
/*---------------------------------------------------------------------------*/

int checkGfxChange (Event_t *e) 
{
    if ( e->hinvchange > 0 ) return(1);
    return(0);
}

/*---------------------------------------------------------------------------*/
/* Name    : checkVersionsChange                                             */
/* Purpose : Checks whether VERSIONS data needs to go in a report or not     */
/* Inputs  : None                                                            */
/* Outputs : 1 or 0                                                          */
/*---------------------------------------------------------------------------*/

int checkVersionsChange(Event_t *e)
{
    if ( e->verchange > 0 ) return(1);
    return(0);
}

