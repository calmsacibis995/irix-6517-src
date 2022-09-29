#ident "$Id: sysmp.c,v 3.4 1994/10/18 04:16:52 jwag Exp $"

#ifdef __STDC__
	#pragma weak sysmp = _sysmp
#endif
#include "synonyms.h"
#include <sys.s>
#include "sys_extern.h"
#include <stddef.h>		/* For ptrdiff_t */

ptrdiff_t
sysmp(int op, ptrdiff_t arg1, ptrdiff_t arg2, ptrdiff_t arg3, ptrdiff_t arg4)
{
	return(syscall(SYS_sysmp, op, arg1, arg2, arg3, arg4));
}
