#ident "$Revision: 1.5 $"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "strvec.h"
#include "output.h"
#include "malloc.h"

static int	count_strvec(char **vec);

void
add_strvec(
	char	***vecp,
	char	*str)
{
	char	*dup;
	int	i;
	char	**vec;

	dup = xstrdup(str);
	vec = *vecp;
	i = count_strvec(vec);
	vec = xrealloc(vec, sizeof(*vec) * (i + 2));
	vec[i] = dup;
	vec[i + 1] = NULL;
	*vecp = vec;
}

char **
copy_strvec(
	char	**vec)
{
	int	i;
	char	**rval;

	i = count_strvec(vec);
	rval = new_strvec(i);
	for (i = 0; vec[i] != NULL; i++)
		rval[i] = xstrdup(vec[i]);
	return rval;
}

static int
count_strvec(
	char	**vec)
{
	int	i;

	for (i = 0; vec[i] != NULL; i++)
		continue;
	return i;
}

void
free_strvec(
	char	**vec)
{
	int	i;

	for (i = 0; vec[i] != NULL; i++)
		xfree(vec[i]);
	xfree(vec);
}

char **
new_strvec(
	int	count)
{
	char	**rval;

	rval = xmalloc(sizeof(*rval) * (count + 1));
	rval[count] = NULL;
	return rval;
}

void
print_strvec(
	char	**vec)
{
	int	i;

	for (i = 0; vec[i] != NULL; i++)
		dbprintf("%s", vec[i]);
}
