#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/lib/libutil/RCS/error.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"
#include <sys/types.h>
#include <sys/param.h>
#include <stdio.h>
#include <errno.h>
#include "icrash.h"
#include "extern.h"

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

			case E_NO_RA:
#ifdef XXX
				fprintf(KL_ERRORFP, "ra not saved in %s()",
					get_funcname(KLIB_ERROR.e_nval));
#else
				fprintf(KL_ERRORFP, "ra not saved in function\n");
#endif
				break;

			case E_NO_TRACE:
				fprintf(KL_ERRORFP, "no valid traces");
				break;

			case E_NO_EFRAME:
				fprintf(KL_ERRORFP, "no eframe found");
				break;

			case E_NO_KTHREAD: 
				fprintf(KL_ERRORFP, "no kthread pointer");
				break;

			case E_NO_PDA:
				fprintf(KL_ERRORFP, "no pda_s pointer");
				break;

			case E_NO_LINKS:
				fprintf(KL_ERRORFP, "no links to follow");
				break;

			case E_STACK_NOT_FOUND:
				fprintf(KL_ERRORFP, "no stack found");
				break;

			default:
				fprintf(KL_ERRORFP, "unknown error (%lld)", KL_ERROR);
				break;
		}
	}
}
