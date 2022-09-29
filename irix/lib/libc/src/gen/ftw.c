#include <sgidefs.h>	/* for _MIPS_SIM defs */
	#pragma weak ftw = _ftw
#if (_MIPS_SIM == _MIPS_SIM_ABI64 || _MIPS_SIM == _MIPS_SIM_NABI32)
	#pragma weak ftw64 = _ftw
	#pragma weak _ftw64 = _ftw
#endif
#include "synonyms.h"
#define	_FTW_INTERNAL
#include "ftw.h"

int
ftw(const char *path, int (*fn) (), int depth)
{
	return(_xftw(XFTWVER, path, fn, depth));
}
