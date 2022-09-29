#ifndef REF_HEADER
#define REF_HEADER

#ident "$Revision: 1.2 $"

#include "atomic.h"

struct ref {
	atomic refcnt;
	void (*delfunc) (void *);
};

void incref (void *);
void decref (void *);
void iniref (void *, void (*) (void *));

#endif /* REF_HEADER */
