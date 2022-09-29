
#ifdef __STDC__
	#pragma weak getpgid = _getpgid
#endif
#include "synonyms.h"
#include <unistd.h>
#include <sys/syssgi.h>
#include <sys/types.h>

pid_t
getpgid(pid_t pid)
{
	return((pid_t)syssgi(SGI_GETPGID, pid));
}
