
/* This stub allows a user to call sprocmonstart() from his code and
   then compile it for non-profiling.  This satisfies the reference
   which would otherwise be picked up from mcrt1.o.

   No weak symbol defined.
*/
#include "sys/regdef.h"

.text


.globl _sprocmonstart
.ent _sprocmonstart
_sprocmonstart:
	j	ra
.end _sprocmonstart

