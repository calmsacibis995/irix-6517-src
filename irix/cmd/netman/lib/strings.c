/*
 * Copyright 1989 Silicon Graphics, Inc.  All rights reserved.
 *
 * String(3C) extensions.
 * NB: vnsprintf knows much about the native stdio implementation.
 */
#include <stdarg.h>
#include <stdio.h>
#include "heap.h"
#include "strings.h"

char *
#ifdef sun
strdup(char *s)
#else
strdup(const char *s)
#endif
{
	return strcpy(vnew(strlen(s)+1, char), s);
}

char *
strndup(const char *s, int n)
{
	char *ns;

	ns = strncpy(vnew(n+1, char), s, n);
	ns[n] = '\0';
	return ns;
}

int
nsprintf(char *buf, int bufsize, const char *format, ...)
{
	va_list ap;
	int count;

	va_start(ap, format);
	count = vnsprintf(buf, bufsize, format, ap);
	va_end(ap);
	return count;
}

int
vnsprintf(char *buf, int bufsize, const char *format, va_list ap)
{
	/* XXX - Do it the slow way for now */
#ifdef FAST_STDIO_VNSPRINTF
	FILE stream;
	int count;

	stream._cnt = bufsize - 1;
	stream._base = stream._ptr = (unsigned char *) buf;
	stream._flag = _IOFBF | _IOWRT;
	stream._file = _NFILE;
	count = vfprintf(&stream, format, ap);
	*stream._ptr = '\0';
#else
	char tmp[8192];
	int count = vsprintf(tmp, format, ap);
	if (count > bufsize - 1)
		count = bufsize - 1;
	bcopy(tmp, buf, count);
	buf[count] = '\0';
#endif
	return count;
}
