
#include "cms_base.h"
#include "cms_kernel.h"
#include <stdarg.h>

int lbolt = 0;

/*
 * File that contains kernel equivalent routines 
 */

void
cmn_err(register int level, char *fmt, ...)
{
        va_list ap;
        va_start(ap, fmt);
	fprintf(stderr,"Cell %d: ",cellid());
        vfprintf(stderr, fmt, ap);
        va_end(ap);
	if (level == CE_PANIC)
		abort();
}

/* ARGSUSED */
void *
kmem_zalloc(int size, int flag)
{
	void *ptr = malloc(size);
	bzero(ptr, size);
	return ptr;
}

/* ARGSUSED */
void
kmem_free(void *ptr, int size)
{
	free(ptr);
}


/* ARGSUSED */
void
spinlock_init(lock_t *lck, char *name)
{
}

/* ARGSUSED */
void
init_sv(sv_t *svp, int flag, char *name, long sequencer)
{
}


/* ARGSUSED */
int
sthread_create(char *name,
                caddr_t stack_addr,
                uint_t stack_size,
                uint_t flags,
                uint_t pri,
                uint_t schedflags,
                st_func_t func,
                void *arg0,
                void *arg1,
                void *arg2,
                void *arg3)
{
	return 1;
}
