/*
 * Copyright 1989 Silicon Graphics, Inc.  All rights reserved.
 *
 * String(3C) extensions.
 * NB: vnsprintf knows much about the native stdio implementation.
 */
#include <stdarg.h>
#include <stdio.h>
#include <strings.h>
#include <malloc.h>
/* #include "heap.h" */
#include "nstrings.h"

char *
strndup(const char *s, size_t n)
{
	char *ns;

	ns = (char *)malloc(n + 1);
	if (! ns) {
		return (char *)0;
	}
	strncpy(ns, s, n);
	ns[n] = (char)0;

	return ns;
}

int
nsprintf(char *buf, size_t bufsize, const char *format, ...)
{
	va_list ap;
	int count;

	va_start(ap, format);
	count = vnsprintf(buf, bufsize, format, ap);
	va_end(ap);
	return count;
}

int
vnsprintf(char *buf, size_t bufsize, const char *format, va_list ap)
{
	/* XXX - Do it the slow way for now */
	int count;
#ifdef FAST_STDIO_VNSPRINTF
	FILE stream;

	stream._cnt = bufsize - 1;
	stream._base = stream._ptr = (unsigned char *) buf;
	stream._flag = _IOFBF | _IOWRT;
	stream._file = _NFILE;
	count = vfprintf(&stream, format, ap);
	*stream._ptr = '\0';
#else
	char tmp[8192];
	count = vsprintf(tmp, format, ap);
	if (count > (int)bufsize - 1)
		count = (int)bufsize - 1;
	bcopy(tmp, buf, count);
	buf[count] = '\0';
#endif
	return count;
}
