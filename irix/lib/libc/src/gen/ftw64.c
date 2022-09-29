#ifdef __STDC__
	#pragma weak ftw64 = _ftw64

#endif
#include "synonyms.h"
#include "ftw.h"

int
ftw64(const char *path, int (*fn) (), int depth)
{
	return(_xftw64(XFTWVER, path, fn, depth));
}
