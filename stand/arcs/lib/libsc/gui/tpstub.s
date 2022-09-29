
#include <sys/regdef.h>
#include <sys/asm.h>

.extern tpfunc;

LEAF(_tpbutton)
	PTR_L	v0,tpfunc
	jr	v0
	END(_tpbutton)
