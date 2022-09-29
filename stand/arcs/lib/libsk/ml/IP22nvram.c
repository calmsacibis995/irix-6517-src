/* IP22/IP24 nvram routines */

/* This file produces a NVRAM function switch module
 * for use by standalones which don't use the getenv
 * function directly.
 *
 * In IP22prom/IP22.c the functions are explicit for
 * each version of the PROM.
 */

#ident "$Revision: 1.2 $"

int use_dallas;

#define	NVRAM_FUNC(type,name,args,pargs,cargs,return)	\
type eeprom_ ## name pargs;				\
type dallas_ ## name pargs;				\
type name args { register int d;			\
	if (!(d = use_dallas))				\
		use_dallas = d = is_fullhouse() ? 2 : 1; \
							\
	if (d == 1)					\
		return dallas_ ## name cargs;		\
	else						\
		return eeprom_ ## name cargs;		\
}

#include <sys/types.h>
#include <sys/cpu.h>
#include <stringlist.h>
#include <PROTOnvram.h>
