#include "sem/psem.c"
#include "sem/sem_syscalls.c"
#include "sem/vsem_mgr.c"
#include "sem/vsem_mgr_lp.c"

void
seminit(void)
{
	vsem_init();
}
