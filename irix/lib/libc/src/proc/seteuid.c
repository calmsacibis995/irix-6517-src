#ifdef __STDC__
	#pragma weak seteuid = _seteuid
#endif
#include "synonyms.h"
#include <unistd.h>

int
seteuid(uid_t euid)
{
    return(setreuid((uid_t)-1, euid));
}
