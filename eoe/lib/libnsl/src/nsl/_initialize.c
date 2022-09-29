
#include <sys/types.h>
#include <sys/syssgi.h>
#include <stdlib.h>

void
_initializer(void)
{
	char *p;

	p = getenv("_LIBNSL_USE_SVR3_PIPE");

	if ((p == 0) || (atoi(p) == 0)) {
		/* Turn on SVR4 STREAMS pipes unless requested otherwise */
		syssgi(SGI_SPIPE, 1);
	}
}
