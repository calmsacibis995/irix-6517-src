#ident "$Id: abi_getpwnam.c,v 1.3 1997/10/12 08:19:24 jwag Exp $"

/*
 * stubs since the 'real' files have lots of non-ABI stuff in them
 */
#ifdef __STDC__
        #pragma weak getpwnam = _getpwnam
        #pragma weak getpwuid = _getpwuid
        #pragma weak getpwent = _getpwent
        #pragma weak setpwent = _setpwent
        #pragma weak endpwent = _endpwent
#endif

#include "synonyms.h"
#include <sys/types.h>
#include <pwd.h>

/* ARGSUSED */
struct passwd *
getpwnam(const char *nam)
{
	return NULL;
}

/* ARGSUSED */
struct passwd *
getpwuid(uid_t uid)
{
	return NULL;
}

struct passwd *
getpwent(void)
{
	return NULL;
}

void
setpwent(void)
{
	return;
}

void
endpwent(void)
{
	return;
}
