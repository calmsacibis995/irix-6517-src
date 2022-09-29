/*
 * Micro-mini User Interface library
 */

#ident "$Revision: 1.17 $"

#include <stdarg.h>
#include <arcs/io.h>
#include <libsc.h>
#include <libsc_internal.h>

static int p_format;
static int p_off;

#define LEFTJUST 0
#define CENTERED 1
#define MAXPPUTC 128

#define MAXCOLS 80

void p_puts(char *);

void
p_offset(int x)
{
	p_off  = x;
	p_format = LEFTJUST;
}

#if 0		/* not need now */
void
p_indent(void)
{
	int i;
	for (i=0;i<p_off;i++) (void)putchar(' ');
}
#endif

void
p_clear(void)
{
	if ( isgraphic(1) )
		(void)puts("\033[2J\033[H");
	p_offset(0);
}

void
p_cursoff(void)
{
	if ( isgraphic(1) ) (void)puts("\033[25l");
}

void
p_curson(void)
{
	if ( isgraphic(1) ) (void)puts("\033[25h");
}

void
p_panelmode(void)
{
	if ( isgraphic(1) )
		(void)puts("\033[33h");

	p_offset(0);
}

void
p_textmode(void)
{
	if ( isgraphic(1) )
		(void)puts("\033[32h");

	p_offset(0);
}

/*VARARGS1*/
void
p_printf(const char *fmt,...)
{
	char buf[MAXPPUTC];
	char *bp, *lp;

	va_list ap;

	/*CONSTCOND*/
	va_start(ap,fmt);
	vsprintf(buf,fmt,ap);
	va_end(ap);

	for ( lp = bp = buf; *bp; bp++ )
		if ( *bp == '\n' ) {
			*bp = '\0';
			p_puts(lp);
			(void)putchar('\n');
			lp = bp+1;
			}
	if ( *lp )
		p_puts(lp);
	
	return;
}

void
p_center(void)
{
	p_format = CENTERED;
}

void
p_ljust(void)
{
	p_format = LEFTJUST;
}

void
p_puts(char *lp)
{
	int len = (int)strlen(lp);
	int i;

	if ( len )
		switch(p_format) {
			case CENTERED:
				for ( i = 0; i < (MAXCOLS-len)/2; i++ )
					(void)putchar(' ');
				(void)puts(lp);
				break;
			case LEFTJUST:
			default:
				for ( i = 0; i < p_off; i++)
					(void)putchar(' ');
				(void)puts(lp);
				break;
			}
}
