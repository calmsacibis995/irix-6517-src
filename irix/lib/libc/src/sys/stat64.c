#ifdef __STDC__
	#pragma weak stat64 = _stat64
#endif
#include "synonyms.h"
#include "sys/types.h"
#include "sys/stat.h"

int
stat64(const char *p, struct stat64 *sb)
{
	return(_xstat(_STAT64_VER, p, (struct stat *)sb));
}
