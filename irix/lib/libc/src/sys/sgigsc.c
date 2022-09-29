#ifdef __STDC__
	#pragma weak sgigsc = _sgigsc
#endif
#include "synonyms.h"

#include	<sys.s>

sgigsc(op, argp)
   int		op;
   char *	argp;
{
	return(syscall(SYS_sgigsc, op, argp));
}
