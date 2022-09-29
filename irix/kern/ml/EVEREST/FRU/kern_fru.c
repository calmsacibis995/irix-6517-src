/*
 * kern_fru.c
 *
 *	This is the kernel's entrance to the FRU analyzer.  It deals
 * with logging and multiple printf functions before calling the FRU analyzer.
 *
 */


#if 0
#include <sys/types.h>
#include <sys/reg.h>
#include <sys/systm.h>
#include <sys/cpu.h>
#include <stdarg.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/everror.h>
#include <sys/EVEREST/evconfig.h>
#endif /* 0 */

#include "evfru.h"			/* FRU analyzer definitions */

static char fru_msg[256];
extern char fru_nvbuff[];
extern int fru_buffindx ; 
static void (*fru_print_func)(char *, ...);

/* Here's where we call the FRU analyzer from the kernel. */
int
everest_fru_analyzer(everror_t *errbuf, everror_ext_t *errbuf1,
		 evcfginfo_t *ecbuf, int flags,
	void (*print_func)(char *, ...))
{

    fru_print_func = print_func;

    /* Call the standard FRU analyzer. */
    return fru_analyzer(errbuf, errbuf1, ecbuf, flags);

}


/*
 * Print messages in printf() format to icrash's stdout.
 */
void
fru_printf(char *fmt, ...)
{
    va_list	ap;

    va_start(ap, fmt);
    vsprintf(fru_msg, fmt, ap);
    va_end(ap);
    /* NVRAM FRU Logging:
     * Concatenate information to logging buffer which gets copied to NVRAM.
     */
    fru_buffindx += strlen(fru_msg);
    strcat(fru_nvbuff, fru_msg);

    fru_print_func("%s", fru_msg);
}


