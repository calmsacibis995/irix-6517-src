#include <string.h>
#include <limits.h>
#define NDEBUG
#include <assert.h>
#include "fsdump.h"

static char    pathname[PATH_MAX];
static long    pathoff;

int pathcat (char *s) {
	register long i;

	i = (long) strlen(s);
	if (pathoff != sizeof(pathname) - 1) {
		if (--pathoff < 0)
			return -1;		/* tells path() to quit */
		pathname[pathoff] = '/';
	}
	while (--i >= 0) {
		if (--pathoff < 0)
			return -1;		/* tells path() to quit */
		pathname[pathoff] = s[i];
	}
	return 0;				/* tells path() to continue */
}

char *path (dent_t *ep) {

	pathoff = sizeof (pathname);
	pathname[--pathoff] = '\0';

	while (ep != mh.hp_dent) {
		if (pathcat (PNAM(mh,ep)))
			break;
		ep = PDDE(mh,ep);
	}
	pathcat ("");
	return pathname + pathoff;
}
