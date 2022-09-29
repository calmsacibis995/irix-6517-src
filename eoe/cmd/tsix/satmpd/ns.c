#ident "$Revision: 1.1 $"

#include <sys/types.h>
#include <stdio.h>
#include <ns_api.h>

/*
 * This is a hack.  All the getXbyY() calls call ns_lookup() which turns
 * around and calls us, so we end up hanging on ourselves.  By including
 * this ns_lookup() it allows us to use all the getXbyY calls to read
 * the local files.
 */
/* ARGSUSED */
int
_ns_lookup(ns_map_t *map, char *domain, char *table, const char *key,
	   char *library, char *buf, size_t len)
{
	return NS_FATAL;
}

/*
 * Same thing as the above.  All the getXent() calls will result in a
 * recursive call which hangs, so this routine exists in case protocol
 * libraries try to use one of the name service API routines.
 */
/* ARGSUSED */
FILE *
_ns_list(char *domain, char *table, char *library)
{
	return (FILE *)0;
}
