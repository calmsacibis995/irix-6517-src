#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/ggd/daemon/RCS/lic_flexlm.c,v 1.2 1997/02/22 04:09:30 kanoj Exp $"

/*
 *	lic_flexlm.c
 *
 *	FLEXlm is used to determine how many streams of guaranteed rate I/O
 *	this system is licensed to use.
 */

#include <assert.h>
#include <stdio.h>
#include <bstring.h>
#include <grio.h>
#include <string.h>
#include <lmsgi.h>
#include <sys/time.h>
#include <sys/fs/xfs_inum.h>
#include <sys/types.h>
#include <sys/uuid.h>

/*
 * FLEXlm Input values: specific to Silicon Graphics
 */
LM_CODE(VENDOR_CODE, ENCRYPTION_CODE_1, ENCRYPTION_CODE_2,
	VENDOR_KEY1, VENDOR_KEY2, VENDOR_KEY3, VENDOR_KEY4, VENDOR_KEY5);
#define PRODID_GRIO_40 "grio_40" /* Guaranteed Rate I/O: 40 streams */
#define PRODID_GRIO_ANY "grio_any" /* Guaranteed Rate I/O: unlimited streams */
#define GRIO_VERSION "2.0"
#define VENDOR_DAEMON "sgifd"

#define NETLS_MAX_ELEMS	(long)40	/* max number of NetLS licenses */

/*
 * Get the total number of currently licensed streams.
 *
 */
int
license_streamcount( void )
{
	int 	count = DEFAULT_GRIO_STREAMS;
	int	status;

	/*
	 * Set up a connection into the FLEXlm (lmsgi) library code
	 */
	status = license_init(&VENDOR_CODE, VENDOR_DAEMON, B_TRUE);
	if (status < 0) {
		fprintf(stderr,"GRIO error: %s\n", license_errstr());
		return(0);
	}

	/*
	 * prevent error messages for some errors
	 */
	if ((status = license_set_attr(LM_A_USER_EXITCALL,
				       (LM_A_VAL_TYPE) NULL)) ||
	    (status = license_set_attr(LMSGI_NO_SUCH_FEATURE,
				       (LM_A_VAL_TYPE) NULL))) {
		fprintf(stderr,"GRIO error: %s\n", license_errstr());
		return(0);
	}

	/*
	 * get licenses for GRIO
	 */
	status = license_chk_out(&VENDOR_CODE, PRODID_GRIO_40, GRIO_VERSION);
	if (status == 0) {
		count = 40;
		status = license_chk_in(PRODID_GRIO_40, 0);
	}

	status = license_chk_out(&VENDOR_CODE, PRODID_GRIO_ANY, GRIO_VERSION);
	if (status == 0) {
		count = UNLIMITED_GRIO_STREAMS;
		status = license_chk_in(PRODID_GRIO_ANY, 0);
	}

	return( count );
}
