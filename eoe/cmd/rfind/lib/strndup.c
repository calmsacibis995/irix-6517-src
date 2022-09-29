#include <string.h>
#include <stdlib.h>
#include "externs.h"

char *strndup (const char *s1, const size_t sz) {
	char * s2;

	if ((s2 = malloc((unsigned) sz+1)) == NULL)
		error ("no memory");
	strncpy (s2, s1, sz);
	s2[sz] = '\0';
	return s2;
}
