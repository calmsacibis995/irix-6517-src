#include <stdarg.h>

extern char *numstr();
static putstr(char *);
static putchr(char);

printf(fmt)
    char *fmt;
{
    va_list ap;
    int ival;
    char *sval;

    va_start(ap, fmt);
    while( *fmt != '\0' )
    {
	if( *fmt != '%' )
	{
	    write(2, fmt, 1);
	    fmt++;
	    continue;
	}
	fmt++;
	switch(*fmt)
	{
	case 'c':
	    ival = va_arg(ap, int);
	    putchr(ival);
	    break;
	case 'd':
	    ival = va_arg(ap, int);
	    putstr(numstr(ival, 10));
	    break;
	case 'o':
	    ival = va_arg(ap, int);
	    putstr(numstr(ival, 8));
	    break;
	case 'x':
	    ival = va_arg(ap, int);
	    putstr(numstr(ival, 16));
	    break;
	case 's':
	    sval = va_arg(ap, char *);
	    putstr((char *) sval);
	    break;
	case '%':
	    putchr('%');
	    break;
	}
	fmt++;
    }
    va_end(ap);
}

static
putchr(char c)
{
    write(2, &c, 1);
}

static
putstr(s)
    register char *s;
{
    write(2, s, strlen(s));
}

# define STATIC

STATIC char *
numstr(n, radix)
    register unsigned n;
    register int radix;
{
    static char buf[10];
    register char *cp;
    
    cp = buf + sizeof buf;
    *--cp = '\0';
    do
    {
	*--cp = "0123456789ABCDEF"[n%radix];
	n /= radix;
    }
    while ( n != 0 );
    return cp;
}
