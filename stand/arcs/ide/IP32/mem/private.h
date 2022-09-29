#ifndef __PRIVATE_H__
#define	__PRIVATE_H__

#ident	"IP22diags/mem/private.h:  $Revision: 1.1 $"

/*
 * Private header file for IP22diags/mem parity test utility functions,
 * intended for use internal to the IP22diags library only.
 */

#include <fault.h>
#include "setjmp.h"
#include "mem.h"

extern unsigned char	_parexpectsig(void *, int);
extern bool_t		_parcheckresult(unsigned long, unsigned long,
				unsigned char, unsigned long);
extern void		_parcleanup(void *);
#endif	/* __PRIVATE_H__ */
