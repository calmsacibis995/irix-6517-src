#ifdef __STDC__
	#pragma weak setrgid = _setrgid
#endif
#include "synonyms.h"
#include <unistd.h>

int
setrgid(gid_t rgid)
{
    return(setregid(rgid, (gid_t)-1));
}
