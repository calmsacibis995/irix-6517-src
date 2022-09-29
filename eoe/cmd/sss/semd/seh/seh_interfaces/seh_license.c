
typedef enum { B_FALSE, B_TRUE } boolean_t;

#include <lmsgi.h>
#include "common.h"

LM_CODE(vendorcode, ENCRYPTION_CODE_1, ENCRYPTION_CODE_2,
	VENDOR_KEY1, VENDOR_KEY2, VENDOR_KEY3,
	VENDOR_KEY4, VENDOR_KEY5);

#define LICENSE_DEBUG     1
extern int verbose;
extern SGMLicense_t  license;
static int already_initialized=0;

void checkin_sgm_license(void)
{
    if ( !already_initialized ) return;

    if ( license_chk_in(SGM_FEATURE, 0) ) {
	seh_syslog_event(NULL, "SGM License Check in error: %s\n", license_errstr());
    }
    
    return;
}

/* Call back for No such feature.  Primiarily intended to let the user
   know that the feature which she configured doesn't exist. */

int nosuchfeature(char *feature)
{
#if 0
    seh_syslog_event(NULL, "License for Feature (%s) not found\n", feature);
#endif
    return(0);
}

/* Callback for warning for 30, 60 and 90 days expiry of license.  This is
   to let the user know that the license is going to expire shortly */

int warning(int days)
{
    seh_syslog_event(NULL, "License for ESP (SGM) expires in %d days\n", 
		     days);
    return(0);
}

/* We enter this routine only when reconnect fails after 5 attempts. At
   this point, we better degrade to SEM as we don't have any license */

int exit_call(char *feature)
{
#if 0
    seh_syslog_event(NULL, 
	"Connect to License Server failed after 5 retries.Degrading to SEM\n");
#endif
    if ( degrade_sgm(&license) )
        return(0);
    else
	return(-1);
}

/* This routine is a for warning purposes that everytime multiple attempts
   are being made to connect to License Server. */

int reconnect_done(char *feature, int tries, int total_attempts, int interval)
{
#if 0
    seh_syslog_event(NULL, "Connection to License Server succeded after %d tries\n", tries);
#endif
    return(0);

}

__uint64_t init_sgm_license()
{
    LM_HANDLE    *lic_handle;

    if(already_initialized)
	return 0;

    if ( license_init(&vendorcode, "sgifd", B_TRUE) < 0 )  {
#if LICENSE_DEBUG
        if ( verbose > VERB_INFO ) {
	    fprintf(stderr, "Error initializing license library\n");
        }
#endif
	return(0);
    }
    
    /* Set the retry count; how many times we want to check for license server */
    
    if ( license_set_attr(LM_A_RETRY_COUNT, (LM_A_VAL_TYPE)LICENSE_RETRY_COUNT)) {
#if LICENSE_DEBUG
        if ( verbose > VERB_INFO ) {
	    fprintf(stderr, "Error setting Retry Count of License Library\n");
        }
#endif
	return(0);
    }
    
    /* Set other attributes that display warnings */
#if LICENSE_DEBUG
    if ( verbose > VERB_INFO ) {
	fprintf(stderr, "Installing callbacks\n");
    }
#endif

    if ( license_set_attr(LMSGI_NO_SUCH_FEATURE, (LM_A_VAL_TYPE) nosuchfeature) ||
	 license_set_attr(LMSGI_30_DAY_WARNING, (LM_A_VAL_TYPE) warning) ||
	 license_set_attr(LMSGI_60_DAY_WARNING, (LM_A_VAL_TYPE) warning) ||
	 license_set_attr(LMSGI_60_DAY_WARNING, (LM_A_VAL_TYPE) warning) ) {
	return(0);
    }
    
    /* Register a NULL callback for LM_A_USER_RECONNECT */
    
    license_set_attr(LM_A_USER_RECONNECT, (LM_A_VAL_TYPE) NULL ) ;
    
    /* Register a SYSLOG Message Callback for LM_A_USER_RECONNECT_DONE */

    license_set_attr(LM_A_USER_RECONNECT_DONE, (LM_A_VAL_TYPE) reconnect_done);
    
    /* Register a Degraded Mode of operation for LM_A_USER_EXITCALL */

    license_set_attr(LM_A_USER_EXITCALL, (LM_A_VAL_TYPE) exit_call);

    already_initialized=1;

    return 0;
}

__uint64_t check_sgm_license(SGMLicense_t *lic)
{
    CONFIG       *conf;
    char         *vdef;

    init_sgm_license();

    /* Check-out License */

    if ( license_chk_out(&vendorcode, SGM_FEATURE, SGM_VERSION) ) {
#if LICENSE_DEBUG
        if ( verbose > VERB_INFO ) {
	    fprintf(stderr, "Can't check-out license for %s,%s\n", SGM_FEATURE,
	    SGM_VERSION);
        }
#endif
	return(0);
    } else {
	/* Get License Expiry date */

	lic->license_expiry_date = (time_t) license_expdate(SGM_FEATURE);
	lic->last_license_check  = (time_t) time(0);

	/* Check VENDOR_STRING */

	if ( (conf = lc_auth_data(get_job(), SGM_FEATURE)) != NULL ) {
	    if ( (conf->users == 0 ) && (conf->lc_vendor_def != NULL) && 
		 (strncmp(conf->lc_vendor_def, "MAXSUBSCR ", 10) == 0)) {
                vdef = conf->lc_vendor_def;
		if ( atoi(vdef+10) != lic->max_subscriptions ) {
		    lic->max_subscriptions = atoi(vdef+10);
		    lic->flag |= SGM_SSDBUPDT_REQD;
		}
            }
	}
    }

    return(1);
}

