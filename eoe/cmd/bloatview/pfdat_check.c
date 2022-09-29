/*
 * pfdat_check.c - checks to make sure local versions of pfdat32 and
 *                 pfdat64 are correct; breaks the build if they
 *                 are not correct.
 */

#define _KMEMUSER
#include <sys/types.h>
#include <sys/pfdat.h>
#include <stdlib.h>
#include <stdio.h>

#include "pfdat3264.h"

main()
{

#if (_MIPS_SZLONG == 32)
	if (sizeof(pfd_t) != sizeof(pfd32_t))
#else
	if (sizeof(pfd_t) != sizeof(pfd64_t))
#endif
	{
		fprintf(stderr, "\nERROR:\tLocal version of pfd_t %s %s\n",
			"(in pfdat3264.h) doesn't match the\n\tone in",
			"sys/pfdat.h; please update.\n");
		exit(1);
	}
	else
		exit(0);
}
