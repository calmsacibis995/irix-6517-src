
#ident	"lib/libsc/lib/spinlock.c:  $Revision: 1.3 $"

#include <sys/types.h>
#include <sys/systm.h>

/* Dummy spinlocks routines for libsc */
/* These routines exist in their true form in libsk/ml/spinlock.c and 
 * they exist here to remove the 'unresolved symbol' error while linking 
 * with fx/sash. These had to be placed in a separate file since attempts
 * to put it in dummyfunc.s and anything other than privstub.c resulted 
 * in Warning for those standalone which link with both libsc/libsk. 
 * strange but to be figured out later.
 */
void
initsplocks()
{
}

/* ARGSUSED */
void
initlock(lock_t *lock)
{
}

/* ARGSUSED */
int
splockspl(lock_t lock, int (*splr)())
{
	return(0);
}

/* ARGSUSED */
void
spunlockspl(lock_t lock, int ospl)
{
}
