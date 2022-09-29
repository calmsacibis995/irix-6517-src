#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/RCS/fru.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"

#define _KERNEL  1
#define _PAGESZ         16384
#include <sys/types.h>
#include <sys/param.h>
#include <sys/fcntl.h>
#include <sys/immu.h>
#include <sys/mbuf.h>
#include <sys/pcb.h>
#include <sys/utsname.h>
#include <stdio.h>
#include "icrash.h"
#include "extern.h"
#include "eval.h"

#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/IP21addrs.h>
#include <sys/EVEREST/evconfig.h>       /* Whole evconfig structure        */
#include <sys/EVEREST/dang.h>           /* DANG chip                       */
#include <sys/EVEREST/fchip.h>          /* F chip                          */
#include <sys/EVEREST/io4.h>            /* IA chip                         */
#include <sys/EVEREST/mc3.h>            /* MA chip                         */
#include <sys/EVEREST/s1chip.h>         /* S1 chip                         */
#include <sys/EVEREST/vmecc.h>          /* VMECC chip                      */
#include <sys/EVEREST/everror.h>        /* for everror_t                   */
#include <sys/EVEREST/gda.h>            /* for everror_ext determination   */
#include "lib/libfru/icrash_fru.h"

/*
 * dofru()
 */
int
dofru(int flags, FILE *ofp)
{
	everror_t *errbuf;
	evcfginfo_t *ecbuf;
	everror_ext_t *errbuf1 = (everror_ext_t *)NULL;
	gda_t *gdbuf;
	kaddr_t vers, valid;

	/* Get the everror_t structure 
	 */
	if (!(errbuf = get_struct((kaddr_t)EVERROR_ADDR, sizeof(everror_t), 
			NULL, "everror"))) {
		fprintf(ofp,
			"---------------------------------------------------------------\n"
			FRU_INDENT "FRU ANALYZER (%s):  No errors found.\n"
			"---------------------------------------------------------------\n"
			"\n", VERSION);
		return 1;
	}

	/* Get evconfig 
	 */
	if (!(ecbuf = get_struct((kaddr_t)EVERROR_ADDR, sizeof(evcfginfo_t), 
			NULL, "evcfg"))) {
		fprintf(ofp,
			"---------------------------------------------------------------\n"
			FRU_INDENT "FRU ANALYZER (%s):  No errors found.\n"
			"---------------------------------------------------------------\n"
			"\n", VERSION);
		return 1;
	}

	/* Get the GDA structure for everror validity 
 	 */
	if ((gdbuf =  get_struct((kaddr_t)GDA_ADDR, sizeof(gda_t), 
			   NULL, "gda")) && (gdbuf->everror_vers >= 3)) {
		errbuf1 = get_struct((kaddr_t)EVERROR_EXT_ADDR, 
				sizeof(everror_ext_t), NULL, "everror_ext");
	}

	if (!force_flag) {
		if (gdbuf == NULL) {
		  fprintf(ofp,
			"---------------------------------------------------------------\n"
			FRU_INDENT "FRU ANALYZER (%s):  No errors found.\n"
			"---------------------------------------------------------------\n"
			"\n", VERSION);
		  return 1;
		}
		vers = gdbuf->everror_vers;
		valid = gdbuf->everror_valid;


		/* Verify that the everror structure is okay */
		if ((valid != EVERROR_VALID) || (vers != EVERROR_VERSION)) {
		  fprintf(ofp,
			"---------------------------------------------------------------\n"
			FRU_INDENT "FRU ANALYZER (%s):  No errors found.\n"
			"---------------------------------------------------------------\n"
			"\n", VERSION);
		  
		  return 1;
		}
	}

	/* Do it and return the result! 
	 */
	return icrash_fruan(errbuf, errbuf1, ecbuf, flags, ofp);
}

/*
 * print_errorbuf()
 */
void
print_errorbuf(FILE *ofp)
{
	if (S_error_dumpbuf) {
		dump_string(S_error_dumpbuf, 0, ofp);
	}
}
