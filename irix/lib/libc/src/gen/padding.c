/*
 * This is a temporary to force the alignment path of the initialized data
 * segment until we get the NLD loader.
 */

#include <standards.h>

#if _NO_ABIAPI && !defined(_LIBC_NONSHARED)
char __libc_padding[192] = {0};
#endif
