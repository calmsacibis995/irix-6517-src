#ident "$Revision: 1.2 $"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "init.h"
#include "malloc.h"
#include "output.h"
#include "versions.h"

static void
badmalloc(void)
{
	dbprintf("%s: out of memory", progname);
#if (VERS >= V_653) && (_MIPS_SIM != _ABI64)
	if (sysconf(_SC_XBS5_LP64_OFF64) == 1)
		dbprintf(": try running %s64 instead", progname);
#endif
	dbprintf("\n");
	exit(4);
}

void *
xcalloc(
	size_t	nelem,
	size_t	elsize)
{
	void	*ptr;

	ptr = calloc(nelem, elsize);
	if (ptr)
		return ptr;
	badmalloc();
	/* NOTREACHED */
}

void
xfree(
	void	*ptr)
{
	free(ptr);
}

void *
xmalloc(
	size_t	size)
{
	void	*ptr;

	ptr = malloc(size);
	if (ptr)
		return ptr;
	badmalloc();
	/* NOTREACHED */
}

void *
xrealloc(
	void	*ptr,
	size_t	size)
{
	ptr = realloc(ptr, size);
	if (ptr)
		return ptr;
	badmalloc();
	/* NOTREACHED */
}

char *
xstrdup(
	const char	*s1)
{
	char		*s;

	s = strdup(s1);
	if (s)
		return s;
	badmalloc();
	/* NOTREACHED */
}
