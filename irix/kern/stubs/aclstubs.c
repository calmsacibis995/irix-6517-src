#include <sys/types.h>
#include <sys/systm.h>

/*
 * Access Control List stubs.
 */
#ident "$Revision: 1.9 $"

#ifdef DEBUG
#define DOPANIC(s) panic(s)
#else /* DEBUG */
#define DOPANIC(s)
#endif /* DEBUG */

void acl_init()	{}	/* Do not put a DOPANIC in this stub! */
void acl_confignote()	{ DOPANIC("acl_confignote stub"); }
void acl_vtoacl()	{ DOPANIC("acl_vtoacl stub"); }
int  acl_vaccess()	{ DOPANIC("acl_vaccess stub"); /* NOTREACHED */ }
int  acl_vchmod()	{ DOPANIC("acl_vchmod stub"); /* NOTREACHED */ }
int  acl_access()	{ DOPANIC("acl_access stub"); /* NOTREACHED */ }
int  acl_inherit()	{ DOPANIC("acl_inherit stub"); /* NOTREACHED */ }
int  acl_invalid()	{ DOPANIC("acl_invalid stub"); /* NOTREACHED */ }
int  acl_xfs_iaccess()	{ DOPANIC("acl_xfs_iaccess stub"); /* NOTREACHED */ }
int  acl_get()		{ DOPANIC("acl_get stub"); /* NOTREACHED */ }
int  acl_set()		{ DOPANIC("acl_set stub"); /* NOTREACHED */ }
int  acl_hwg_iaccess()	{ DOPANIC("acl_hwg_iaccess stub"); /* NOTREACHED */ }
int  acl_hwg_get()	{ DOPANIC("acl_hwg_get stub"); /* NOTREACHED */ }
int  acl_hwg_match()	{ DOPANIC("acl_hwg_match stub"); /* NOTREACHED */ }
