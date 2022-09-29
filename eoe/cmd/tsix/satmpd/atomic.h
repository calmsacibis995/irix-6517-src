#ifndef ATOMIC_HEADER
#define ATOMIC_HEADER

#ident "$Revision: 1.2 $"

typedef unsigned long atomic;

int atomic_initialize (void);

void iniatomic (atomic *, atomic);
atomic getatomic (atomic *);
void setatomic (atomic *, atomic);
atomic incatomic (atomic *);
atomic decatomic (atomic *);

#endif /* ATOMIC_HEADER */
