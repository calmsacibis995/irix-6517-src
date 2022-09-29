/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1994, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Revision: 1.7 $"

/*
 * Check to see if this node is licensed for plexing.
 */
#if sgi && NETLS

#include <stdio.h>
#include <sys/stat.h>
#include <stdlib.h>

#include <lmsgi.h>

#include <sys/types.h>
#include <sys/uuid.h>
#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>
#include <string.h>
#include <xlv_utils.h>

/*
 * code for FLEXlm license calls
 */
LM_CODE(code, ENCRYPTION_CODE_1, ENCRYPTION_CODE_2,
	VENDOR_KEY1, VENDOR_KEY2, VENDOR_KEY3, VENDOR_KEY4, VENDOR_KEY5);

/*
 * FLEXlm Input values: specific to Silicon Graphics
 * vname - The Vendor name for SGI: identifies SGI in a license database
 */
static char *vname = "sgifd";

#define VERSION 	"2.0"
#define PROGNAME 	"XLV"

/*
 * We aren't using the default nodelock file /var/netls/nodelock
 */
static char xlvplex_licpath[] = "/etc/flexlm/license.dat";

/*
 * This is a FLEXlm license check.
 * Returns 1 for success, 0 if not.
 */
int
check_plexing_license(char *root)
{
	char			path[256];
	static char		env[256];
	long			status;
#ifdef DEBUG_NETLS
	char			msg[128];
#endif
	struct stat 		statbuf;
	/*
	 * indicate whether license is already checked out
	 */
	static int initialized = 0;

	if (initialized == 0) {

		/*
		 * Change nodelock file from /var/netls/nodelock (default)
		 */
		if (root && *root)
			strcat (strcpy(path, root), xlvplex_licpath);
		else
			strcpy (path, xlvplex_licpath);

		/*
		 * first see if the nodelock file exists first.
		 */
		if (stat(path, &statbuf) < 0) {
			return (0);
		}

		/*
		 * build environment var and set it
		 */
		strcat (strcpy(env,"LM_LICENSE_FILE="), path);
		status = putenv(env);

		/*
		 * Initialize the FLEXlm licensing system
		 */
		status = license_init(&code, vname, B_TRUE);
		if (status < 0) {
#ifdef DEBUG_NETLS
			sprintf(msg,"error: %s initialization failed\n%s",
				PROGNAME, license_errstr());
			fprintf( stderr, "%s\n", msg );
#endif
			return (0);
		}

		initialized = 1;

		if ((status = license_set_attr(LM_A_USER_RECONNECT,
					       (LM_A_VAL_TYPE) NULL)) ||
		    (status = license_set_attr(LM_A_USER_RECONNECT_DONE,
					       (LM_A_VAL_TYPE) NULL)) ||
		    (status = license_set_attr(LM_A_USER_EXITCALL,
					       (LM_A_VAL_TYPE) NULL)) ||
		    (status = license_set_attr(LMSGI_NO_SUCH_FEATURE,
					       (LM_A_VAL_TYPE) NULL)) ||
		    (status = license_set_attr(LMSGI_30_DAY_WARNING,
					       (LM_A_VAL_TYPE) NULL)) ||
		    (status = license_set_attr(LMSGI_60_DAY_WARNING,
					       (LM_A_VAL_TYPE) NULL)) ||
		    (status = license_set_attr(LMSGI_90_DAY_WARNING,
					       (LM_A_VAL_TYPE) NULL)))
		  return (0);	/* error */
	}

	/*
	 * Request a license from the license server
	 */
	status = license_chk_out(&code, PROGNAME, VERSION);
	if (status < 0)
	{
#ifdef DEBUG_NETLS
		printf(msg,"error: unable to check out license for %s\n%s"
		       PROGNAME, license_errstr());
		fprintf(stderr, "%s\n", msg);
#endif
		return(0);
	}

	/*
	 * check license back in.  we merely want to check we have it.
	 */
	status = license_chk_in(PROGNAME, 0);

	return(1);
}

#else
/* ARGSUSED0 */
int
check_plexing_license(char *root)
{
	return(1);
}
#endif /* sgi && NETLS */
