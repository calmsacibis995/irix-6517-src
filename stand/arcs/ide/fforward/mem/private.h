#ifndef __PRIVATE_H__
#define	__PRIVATE_H__

#ident	"$Revision: 1.5 $"

/*
 * Private header file for fforward/mem parity test utility functions,
 * intended for use internal to the IP2[026]diags library only.
 */

#include <fault.h>
#include "setjmp.h"
#include "mem.h"

extern unsigned char	_parexpectsig(void *, int);
extern bool_t		_parcheckresult(unsigned long, unsigned long,
				unsigned char, unsigned long);
extern void		_parcleanup(void *);
#endif	/* __PRIVATE_H__ */
