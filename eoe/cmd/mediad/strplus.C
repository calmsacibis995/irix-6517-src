#include "strplus.H"

#include <assert.h>
#include <string.h>

//  strdup that uses new[] instead of malloc.

char *
strdupplus(const char *src)
{
    assert(src);
    char *dest = new char[strlen(src) + 1];
    if (dest)
	(void) strcpy(dest, src);
    return dest;
}

int
strcmpnull(const char *left, const char *right)
{
    if (!left && !right)
	return 0;
    if (!left)
	return 1;
    if (!right)
	return -1;
    return strcmp(left, right);
}
