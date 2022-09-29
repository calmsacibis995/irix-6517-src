#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash/RCS/error.c,v 1.1 1999/05/25 19:21:38 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <klib/klib.h>
#include "icrash.h"

/*
 * print_error()
 */
void
print_error()
{
	k_uint_t base_error, struct_error;

	/* Print out icrash specific error messages. We don't have to 
	 * print out the bad value first as the klib error printing 
	 * routine already printed it out.
	 */
	if (base_error = (KL_ERROR & 0xffffffff)) {

		switch (base_error) {

			case E_NO_LINKS:
				fprintf(KL_ERRORFP, "no links to follow");
				break;

			default:
				fprintf(KL_ERRORFP, "unknown error (%lld)", KL_ERROR);
				break;
		}
	}
}
