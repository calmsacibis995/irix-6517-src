#ifdef __STDC__
	#pragma weak mknod = _mknod
#endif
#include "synonyms.h"
#include "sys/types.h"
#include "sys/stat.h"

int
mknod(const char *p, mode_t m, dev_t d)
{
	return(_xmknod(_MKNOD_VER, p, m, d));
}
