
#ifdef __STDC__
	#pragma weak getsid = _getsid
#endif
#include "synonyms.h"
#include <sys/syssgi.h>
#include <sys/types.h>

pid_t
getsid(pid_t pid)
{
	return((pid_t)syssgi(SGI_GETSID, pid));
}

