/* This module is added here in libgen for VSU:XPG4.
 * We want to keep it out of libc for vendor configure scripts
 * which look for BSD vfork which we do not support.
 */
#include <sys/types.h>

extern pid_t _vfork(void);

pid_t
vfork(void)
{
	return(_vfork());
}
