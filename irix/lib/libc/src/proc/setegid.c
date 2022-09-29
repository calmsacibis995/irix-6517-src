#ifdef __STDC__
	#pragma weak setegid = _setegid
#endif
#include "synonyms.h"
#include <unistd.h>

int
setegid(gid_t egid)
{
    return(setregid((gid_t)-1, egid));
}
