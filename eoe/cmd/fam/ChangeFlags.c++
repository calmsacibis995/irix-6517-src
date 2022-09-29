#include "ChangeFlags.h"

#include <assert.h>
#include <ctype.h>
#include <stdio.h>

char ChangeFlags::value_buffer[];

ChangeFlags::ChangeFlags(const char *p, const char **pp)
{
    while (isascii(*p) && isspace(*p))
	p++;
    for ( ; ; p++)
	switch (*p)
	{
	case 'd':
	    set(DEV);	break;
	case 'i':
	    set(INO);	break;
	case 'u':
	    set(UID);	break;
	case 'g':
	    set(GID);	break;
	case 's':
	    set(SIZE);	break;
	case 'm':
	    set(MTIME);	break;
	case 'c':
	    set(CTIME);	break;
	default:
	    if (pp)
		*pp = p;
	    return;
	}
}

const char *
ChangeFlags::value() const
{
    register FlagWord f = flags;
    register char *s = value_buffer;

    if (f & DEV)
	*s++ = 'd';
    if (f & INO)
	*s++ = 'i';
    if (f & UID)
	*s++ = 'u';
    if (f & GID)
	*s++ = 'g';
    if (f & SIZE)
	*s++ = 's';
    if (f & MTIME)
	*s++ = 'm';
    if (f & CTIME)
	*s++ = 'c';
    *s = '\0';

    assert(s < value_buffer + sizeof value_buffer);

    return value_buffer;
}
