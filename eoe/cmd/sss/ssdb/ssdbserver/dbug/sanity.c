/* Declarate _sanity() if not declared in main program */

#include <global.h>

#if defined(SAFEMALLOC) && !defined(MASTER)	/* Avoid errors in MySQL */
int _sanity(const char * file __attribute__((unused)),
            uint line __attribute__((unused)))
{
  return 0;
}
#endif
