#ident "$Id: abi_grp.c,v 1.5 1997/10/12 08:19:59 jwag Exp $"

#ifdef __STDC__
        #pragma weak getgrnam = _getgrnam
        #pragma weak getgrgid = _getgrgid
        #pragma weak getgrent = _getgrent
        #pragma weak setgrent = _setgrent
        #pragma weak endgrent = _endgrent
#endif

#include "synonyms.h"
#include <sys/types.h>
#include <grp.h>
#include <unistd.h>

/* ARGSUSED */
struct group *
getgrnam(const char *nam)
{
	return((struct group *)NULL);
}

/* ARGSUSED */
struct group *
getgrgid(gid_t gid)
{
	return((struct group *)NULL);
}


struct group *
getgrent(void)
{
	return NULL;
}

void
setgrent(void)
{
	return;
}

void
endgrent(void)
{
	return;
}
