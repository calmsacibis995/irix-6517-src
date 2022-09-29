#ifdef __STDC__
	#pragma weak fstat64 = _fstat64
#endif
#include "synonyms.h"
#include "sys/types.h"
#include "sys/stat.h"

int
fstat64(int p, struct stat64 *sb)
{
	return(_fxstat(_STAT64_VER, p, (struct stat *)sb));
}
