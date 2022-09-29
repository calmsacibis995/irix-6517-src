#ifdef __STDC__
	#pragma weak lstat64 = _lstat64
#endif
#include "synonyms.h"
#include "sys/types.h"
#include "sys/stat.h"

int
lstat64(const char *p, struct stat64 *sb)
{
	return(_lxstat(_STAT64_VER, p, (struct stat *)sb));
}
