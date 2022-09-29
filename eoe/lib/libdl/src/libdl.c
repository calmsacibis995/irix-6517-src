#ifdef _LIBDL_ABI
#ifdef __STDC__
        #pragma weak dlopen = _dlopen
        #pragma weak dlsym = _dlsym
        #pragma weak dlclose = _dlclose
        #pragma weak dlerror = _dlerror
#endif

#include <libc_synonyms.h>

#ifndef NULL
#define NULL		0
#endif 

/* ARGSUSED */
void *
dlopen(const char *pathname, int mode)
{
	return(NULL);
}

/* ARGSUSED */
void *
dlsym(void *handle, const char *name)
{
	return(NULL);
}

/* ARGSUSED */
int
dlclose(void *handle)
{
	return(0);
}

char *dlerror(void)
{
	return(NULL);
}

#else /* _LIBDL_ABI */
/* Nothing here! - all routines in libc */

void
__libdl_dummy(void)
{
}
#endif /* LIBDL_ABI */
