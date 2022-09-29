/*
 * icrash_fru.h-
 *
 *	This file contains things needed by the FRU analyzer _only_
 * in icrash.  The in-kernel version doesn't require it.
 */

#include <stdio.h>

/*
 * Icrash has a DEBUG macro that isn't a simple switch like the one
 * in the kernel.  Get rid of it and define DEBUG as 1.  When we're not in
 * the kernel, it makes sense to compile in the debugging code all the
 * time.
 */
#ifdef DEBUG
#undef DEBUG
#endif

/* Always include the debugging code in the icrash version.
 */
#define DEBUG	1

/*
 * Use this special printf that pritns to the output file pointer when called
 * by icrash.
 */
#define	FRU_PRINTF	icrash_printf

/*
 * icrash likes to indent FRU analyzer output.  The kernel doesn't.
 */
#define FRU_INDENT	"    "

/* Prototypes: */
extern void icrash_print(char *, ...);
extern int icrash_fruan(everror_t *, everror_ext_t *, evcfginfo_t *, 
		    int, FILE *);

