#ident "$Revision: 1.7 $"

#include <sys/types.h>
#include <sys/uuid.h>
#include "sim.h"

void
uuid_create(uuid_t *uuid, uint_t *status)
{
	int i;
	long *tp = (long *)uuid;
	long t = gtime();

	for (i = 0; i < sizeof(*tp) / sizeof(long); i++)
		tp[i] = t;
	*status = 0;
}

/* ARGSUSED */
__uint64_t
uuid_hash64(uuid_t *uuid, uint_t *status)
{
	*status = 0;
	return 0ULL;
}
