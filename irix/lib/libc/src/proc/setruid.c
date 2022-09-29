#ifdef __STDC__
	#pragma weak setruid = _setruid
#endif
#include "synonyms.h"
#include <unistd.h>

int
setruid(uid_t ruid)
{
    return(setreuid(ruid, (uid_t)-1));
}
