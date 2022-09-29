#ident "$Revision: 1.3 $"

#include <stdlib.h>
#include "ref.h"

/* increment reference-count of structure */
void
incref (void *p)
{
	struct ref *r = (struct ref *) p;

	if (r != NULL)
		(void) incatomic (&r->refcnt);
}

/* decrement reference-count and deallocate if there are zero references */
void
decref (void *p)
{
	struct ref *r = (struct ref *) p;

	if (r != NULL && decatomic (&r->refcnt) == 1)
	{
		(*r->delfunc) (p);
	}
}

/* initialize reference-count structure */
void
iniref (void *p, void (*delfunc) (void *))
{
	struct ref *r = (struct ref *) p;

	if (r == NULL || delfunc == NULL)
		return;
	iniatomic (&r->refcnt, 1);
	r->delfunc = delfunc;
}
