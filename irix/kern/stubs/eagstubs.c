
#include <sys/types.h>
#include <sys/systm.h>
/*
 * Plan G artifactual stubs.
 */
#ident "$Revision: 1.9 $"

#ifdef DEBUG
#define DOPANIC(s) panic(s)
#else /* DEBUG */
#define DOPANIC(s)
#endif /* DEBUG */


int eag_mount()		{ DOPANIC("eag_mount stub"); /* NOTREACHED */ }
int eag_parseattr()	{ DOPANIC("eag_parseattr stub"); /* NOTREACHED */ }
void eag_confignote()	{ DOPANIC("eag_confignote stub"); }
