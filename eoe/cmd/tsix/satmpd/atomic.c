#ident "$Revision: 1.2 $"

#include <stdlib.h>
#include <mutex.h>
#include "atomic.h"

/* initialize the atomic operations subsystem */
int
atomic_initialize (void)
{
	return (0);
}

/* allocate and initialize atomic variable */
void
iniatomic (atomic *var, atomic value)
{
	*var = value;
}

/* set current value of atomic variable */
void
setatomic (atomic *var, atomic value)
{
	(void) test_and_set (var, value);
}

/* fetch current value of atomic variable */
atomic
getatomic (atomic *var)
{
	return (test_then_add (var, 0));
}

/* decrement atomic variable, returning previous value */
atomic
decatomic (atomic *var)
{
	return (test_then_add (var, -1));
}

/* increment atomic variable, returning previous value */
atomic
incatomic (atomic *var)
{
	return (test_then_add (var, 1));
}
