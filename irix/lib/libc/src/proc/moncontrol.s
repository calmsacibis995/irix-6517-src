
/* This stub allows a user to call moncontrol() from his code and
   then compile it for non-profiling.  This satisfies the reference
   which would otherwise be picked up from libprof.a

   Use weak symbol version for namespace purity.
*/

#include "sys/regdef.h"

	.weakext moncontrol, _moncontrol

.text


.globl _moncontrol
.ent _moncontrol
_moncontrol:
	j	ra
.end _moncontrol

