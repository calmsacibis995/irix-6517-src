/*
 * Copyright 1998, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions 
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */


#if defined(_LICENSED)

#define _HAVE_FLEXLM

#include <sys/types.h>
#include <netinet/in.h>
#include <syslog.h>

#include "license_common.h"

#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <string.h>


#ifdef _HAVE_FLEXLM
#include <lmsgi.h>
#endif




/*
 * Error messages from license routines
 */
static char badInitMessage[] =
"Error initializing the license system; rsvpd-snmpagent not activated.\n";
static char noLicenseMessage[] =
"Could not find a valid rsvpd-snmpagent license; rsvpd-snmpagent not activated.\n";
static char licenseWarningMessage[] =
	"The license for rsvpd-snmpagent expires";


#ifdef _HAVE_FLEXLM
static char base_feature[] = "rsvpd-snmpagent";
#define LICENSE_VERSION "1.000"

LM_CODE(code, ENCRYPTION_CODE_1, ENCRYPTION_CODE_2,
        VENDOR_KEY1, VENDOR_KEY2, VENDOR_KEY3, VENDOR_KEY4,
        VENDOR_KEY5);
#endif

/*
 * Issue a warning if the license will expire 
 * within warnOfExpiration days
 */
static const unsigned int warnOfExpiration = 30;


bool_t getLicense(char *, char **);


/* check the license*/
int
check_License(void)
{

    char *mesg= NULL;
    if (getLicense(base_feature,&mesg) 
	== FALSE) {
/*	TRACE(L_ERR,mesg); */
        syslog(LOG_ERR, mesg);
        return -1;
    }

    /* if mesg is not null, there is 
       some info (probably warning)
    */
    if (mesg) {
/* 	TRACE(L_WARN,mesg); */
	syslog(LOG_WARNING, mesg);
 	free(mesg);
    }

    return 0;

}




#ifdef _HAVE_FLEXLM


#ifdef __not_called
/*
 * Release the license back
 */
static void
releaseLicense(void)
{
    license_chk_in(base_feature, 0);
    return;
}

#endif /* __not called */



/*
 * Get a license from FlexLM
 */
bool_t 
getLicense(char *prog, char **msg)
{
    time_t expDate;
    unsigned int remaining;

    /* shut compiler up */
    prog = prog;

    *msg = 0;
    if (license_init(&code, "sgifd", B_TRUE) 
	< 0) {
	*msg = badInitMessage;
	return FALSE;
    }

    /* Reinitialize to prevent license_chk_out 
       from exiting program
       If the retval is < 0, the return from this
       function is TRUE ???
    */

    if (license_set_attr(LMSGI_NO_SUCH_FEATURE,
        NULL) < 0) {
#ifdef _DEBUG
         printf("license_set_attr error: %s\n", 
	      license_errstr()); 
#endif
	return TRUE;
    }


    /* Get a license */
    if (license_chk_out(&code, 
        base_feature, LICENSE_VERSION)) {
	*msg = noLicenseMessage;
	return FALSE;
    }

    expDate = license_expdate(base_feature);

    /* Warn if expiration date is 
       approaching */
    remaining = (expDate - time(0)) / (60 * 60 * 24);
    if (remaining > warnOfExpiration)
	*msg = 0;
    else {
	int nch;
        nch= strlen(licenseWarningMessage)+32;
	*msg = calloc(nch,sizeof(char));
	if (!*msg) {
/*	    TRACE(L_ERR,"Cannot allocate memory for"
	      "license message"); */
	    return FALSE;
	}

	sprintf(*msg, licenseWarningMessage, 
	        remaining);

	switch (remaining) {
	    case 0:
		sprintf(*msg, "%s today", 
		    licenseWarningMessage);
		break;

	    case 1:
		sprintf(*msg, "%s tomorrow", 
		    licenseWarningMessage);
		break;

	    default:
		sprintf(*msg, "%s in %d days", 
		 licenseWarningMessage, remaining);
		break;

	}
    }
    
    return TRUE;

}
#endif  /* _HAVE_FLEXLM */


#endif /* if defined(_LICENSED)*/
