/* Front end to make ARCS console esc sequence coding easy.
 */

#ident "$Revision: 1.9 $"

#include <arcs/io.h>
#include <libsc.h>

#ifdef REALCSI
#define CSI	Write(StandardOut,"\233",1,&rc);	/* 0x9b */
#else
#define CSI	Write(StandardOut,"\033[",2,&rc)	/* ^[[ */
#endif

/* Clear the whole screen */
void clear(void)
{
	ULONG rc;

	CSI;
	Write(StandardOut,"2J",2,&rc);
}

/* erase to end of line */
void eraseeos(void)
{
	ULONG rc;

	CSI;
	Write(StandardOut,"J",1,&rc);
}

void erasebos(void)
{
	ULONG rc;

	CSI;
	Write(StandardOut,"1J",2,&rc);
}

/* move the cursor */
void move(int x, int y)
{
	char buf[20];
	ULONG rc;
	ULONG l;

	sprintf(buf,"%d;%dH",y,x);
	l = strlen(buf);

	CSI;
	Write(StandardOut,buf,l,&rc);
}

void eraseeol(void)
{
	ULONG rc;

	CSI;
	Write(StandardOut,"K",1,&rc);
}

void erasebol(void)
{
	ULONG rc;

	CSI;
	Write(StandardOut,"1K",2,&rc);
}

void eraseline(void)
{
	ULONG rc;

	CSI;
	Write(StandardOut,"2K",2,&rc);
}

static void _move(int n, char ch)
{
	char buf[20];
	ULONG rc;
	size_t l;

	sprintf(buf,"%d%c",n,ch);
	l = strlen(buf);

	CSI;
	Write(StandardOut,buf,l,&rc);
}

void moveup(int n) {_move(n,'A');}
void movedown(int n) {_move(n,'B');}
void moveright(int n) {_move(n,'C');}
void moveleft(int n) {_move(n,'D');}

void attroff(void)
{
	ULONG rc;

	CSI;
	Write(StandardOut,"0m",2,&rc);
}

void attrul(void)
{
	ULONG rc;

	CSI;
	Write(StandardOut,"4m",2,&rc);
}

void attrrv(void)
{
	ULONG rc;

	CSI;
	Write(StandardOut,"7m",2,&rc);
}

void attrhi(void)
{
	ULONG rc;

	CSI;
	Write(StandardOut,"1m",2,&rc);
}

void color(int clr)
{
	char buf[20];
	ULONG rc;

	CSI;
	sprintf(buf,"%dm",clr+30);
	Write(StandardOut,buf,strlen(buf),&rc);
}

void bcolor(int clr)
{
	char buf[20];
	ULONG rc;

	CSI;
	sprintf(buf,"%dm",clr+40);
	Write(StandardOut,buf,strlen(buf),&rc);
}
