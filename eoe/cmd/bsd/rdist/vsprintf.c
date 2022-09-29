#include	<stdio.h>
#include	<varargs.h>

extern	int	_doprnt();

int
vsprintf(buf, fmt, args)
	char	*buf;
	char	*fmt;
	va_list	args;
{
	FILE	strbuf;

	strbuf._flag = _IOWRT + _IOSTRG;
	strbuf._ptr = buf;
	strbuf._cnt = 32767;
	_doprnt(fmt, args, &strbuf);
	putc('\0', &strbuf);
	return ferror(&strbuf) ? EOF : 0;
}
