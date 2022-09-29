
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/errno.h>
/*
 * Capability Set stubs
 */
#ident "$Revision: 1.7 $"

#ifdef DEBUG
#define DOPANIC(s) panic(s)
#else /* DEBUG */
#define DOPANIC(s)
#endif /* DEBUG */

void cap_init()		{}	/* Do not put a DOPANIC in this stub! */
void cap_init_cred()	{ DOPANIC("cap_init_cred stub"); }
void cap_confignote()	{ DOPANIC("cap_confignote stub"); }
int cap_style()		{ return ENOSYS; } /* Do not put a DOPANIC here! */
int cap_recalc()	{ DOPANIC("cap_recalc stub"); /* NOTREACHED */ }
int cap_setpcap()	{ DOPANIC("cap_setpcap stub"); /* NOTREACHED */ }
int cap_request()	{ DOPANIC("cap_request stub"); /* NOTREACHED */ }
int cap_surrender()	{ DOPANIC("cap_surrender stub"); /* NOTREACHED */ }
int cap_get()		{ DOPANIC("cap_get stub"); /* NOTREACHED */ }
int cap_set()		{ DOPANIC("cap_set stub"); /* NOTREACHED */ }
