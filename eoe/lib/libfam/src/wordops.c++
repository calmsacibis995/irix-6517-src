#include "Client.h"

void getword(char *p, __uint32_t *l)
{
	char	*pp;
	pp = (char *) l;
	*pp++ = *p++;
	*pp++ = *p++;
	*pp++ = *p++;
	*pp++ = *p++;
}

void putword(char *p, __uint32_t *l)
{
	char *pp;

	pp = (char *) l;
	*p++ = *pp++;
	*p++ = *pp++;
	*p++ = *pp++;
	*p++ = *pp++;
}
