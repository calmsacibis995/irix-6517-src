/*
 * icrash_fru.c-
 *
 *	Here we have crash's entrance to the FRU analyzer.  This file
 * provides the means for icrash to call into code that's shared with the
 * kernel.  Anything strange like global file pointers must be kept here.
 * We won't want the kernel code to have any references to FILE in it.
 *
 */

#define _KERNEL  1
#include <sys/types.h>
#undef _KERNEL

#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/evconfig.h>       /* Whole evconfig structure */

#define _K64U64 1
#include <sys/EVEREST/IP19addrs.h>      /* For evconfig, everror addresses */
#undef _K64U64

/*
 * These line must be _before_ we include evfru.h because of a namespace
 * problem (DEBUG).
 */
#include <stdio.h>
#include "../include/icrash.h"

#include "evfru.h"			/* FRU analyzer definitions */

#include <stdarg.h>

static FILE *ofp;
static char icrash_msg[256];

/* Here's where we call the FRU analyzer from icrash. */
int
icrash_fruan(everror_t *errbuf, everror_ext_t *errbuf1, evcfginfo_t *ecbuf,
	   int flags, FILE *icrash_ofp)
{
    if (flags & C_FULL) {
		fru_ignore_sw = 1;
    }

    ofp = icrash_ofp;

    /* Set the FRU analyzer's debug flag to the value in icrash's */

#ifdef DEBUG
    fru_debug = debug;
#endif
    
    /* Call the standard FRU analyzer. */
    return fru_analyzer(errbuf, errbuf1, ecbuf, flags);

}


/*
 * Print messages in printf() format to icrash's stdout.
 */
void
icrash_printf(char *fmt, ...)
{
    va_list	ap;
    unchar	*c, *p, val=0;
    char	*line;
    int		i;

    va_start(ap, fmt);
    vsprintf(icrash_msg, fmt, ap);
    va_end(ap);

    fprintf(ofp, "%s", icrash_msg);
}


