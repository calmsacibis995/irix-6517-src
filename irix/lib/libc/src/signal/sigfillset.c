
#include	"synonyms.h"
#include	"signal.h"

void
__sigfillset(sigset_t *sigs)
{
	sigs->__sigbits[0] = 0xffffffff;
	sigs->__sigbits[1] = 0xffffffff;
	sigs->__sigbits[2] = 0;
	sigs->__sigbits[3] = 0;
}
