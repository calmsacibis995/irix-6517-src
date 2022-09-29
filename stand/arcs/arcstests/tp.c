/* Test of ARCS textport capability.
 * Usage: tp [command]
 */

#ident "$Revision: 1.14 $"

#include <arcs/io.h>
#include <sys/ht_gl.h>

void tpclr(void), tptabs(void), tpautow(void), tpereos(void), tpereos2(void);
void tperbos(void), tpereol(void), tpereol2(void), tperbol(void);
void tperline(void), tpmove(void), tpmover(void), tpfont(void), tpattr(void);
void tpcs(void), tpcolor(void), tpac(void), tpdl(void), tpinsl(void);
void tpcr(void);

struct cmd {
	char *name;
	void (*func)();
	char *desc;
};

struct cmd cmds[] = {
	"allcolors",	tpac,		"display all color combinations",
	"attr",		tpattr,		"display attributes",
	"autowrap",	tpautow,	"tests for no-automatic autowrap",
	"color",	tpcolor,	"color attributes",
	"clear",	tpclr,		"clear screen and move to 0,0",
	"cr",		tpcr,		"test carriage return",
	"cs",		tpcs,		"single char control sequences",
	"delline",	tpdl,		"delete a line",
	"erasebol",	tperbol,	"clear to beginning of line",
	"erasebos",	tperbos,	"clear to beginning of screen",
	"eraseeol",	tpereol,	"clear to end of line",
	"eraseeol2",	tpereol2,	"clear to end of line (seq 2)",
	"eraseeos",	tpereos,	"clear to end of screen",
	"eraseeos2",	tpereos2,	"clear to end of screen (seq 2)",
	"eraseline",	tperline,	"clear line",
	"font",		tpfont,		"change font (current a nop)",
	"insline",	tpinsl,		"insert line",
	"move",		tpmove,		"move cursor",
	"mover",	tpmover,	"move cursor relative",
	"tabs",		tptabs,		"print at tabstops",
	0,0,0
};

void
main(int argc, char **argv)
{
	int i;

	/* fix argc and argv to nuke extra boot parameters */
	for (i=0; i < argc ; i++) {
		if (index(argv[i],'=')) {
			argc = i;
			argv[i] = 0;
			break;
		}
	}

	if (argc == 1) {
		for (i=0; cmds[i].name; i++)
			printf("%s\t%s\n",cmds[i].name, cmds[i].desc);
	}
	else {
		for (i=0; cmds[i].name ; i++)
			if (!strcmp(argv[1],cmds[i].name)) {
				cmds[i].func(argc-2,&argv[2]);
				return;
			}

		printf("%s: invalid command\n",argv[1]);
	}

	return;
}

static void sleep(int n)
{
	volatile int j;

	for (;n != 0; n--)
		for (j=0; j < 0x2ffff ; j++) ;
}

void tpclr(void)
{
	int i;

	for (i=0; i < 7; i++) {
		bcolor(i);
		clear();
		sleep(30);
	}
	attroff();
	clear();
}

void tptabs(void)
{
	ULONG rc;
	int i;

	clear();

	move(1,5);
	attroff(); color(CYAN); bcolor(BLACK);

	for (i=0; i < 15 ; i++) {
		Write(StandardOut,"\t",1,&rc);
		sleep(1);
	}

	printf("\n");
	for (i=0; i < 5 ; i++) {
		Write(StandardOut,"\t",1,&rc);
		sleep(1);
	}
	printf("XXXX\n");

	attroff();
	move(1,5);
}

void tpautow(void)
{
	ULONG rc;
	int i,y;

	clear();

	move(0,5);
	for (y=2; y<22; y++) {
		move(1,y);
		for (i=0 ; i < 15; i++)
			Write(StandardOut,"1234567890",10,&rc);
	}

	move(0,25);
}

void tpereos(void)
{
	ULONG rc;

	bcolor(BLACK);
	clear();

	move(5,5);
	color(RED);
	Write(StandardOut,"LEAVEME",7,&rc);
	color(YELLOW);
	Write(StandardOut,"--JUNK--",8,&rc);

	move(5,6);
	Write(StandardOut,"--JUNK--",8,&rc);

	move(12,5);
	attroff();color(BLACK);bcolor(CYAN);

	sleep(10);
	eraseeos();

	attroff();
	move(1,6);
}

void tpereos2(void)
{
	ULONG rc;

	bcolor(BLACK);
	clear();

	move(5,5);
	color(RED);
	Write(StandardOut,"LEAVEME",7,&rc);
	color(YELLOW);
	Write(StandardOut,"--JUNK--",8,&rc);

	move(5,6);
	Write(StandardOut,"--JUNK--",8,&rc);

	move(12,5);
	attroff();color(BLACK);bcolor(CYAN);

	sleep(10);
	Write(StandardOut,"\033[0J",4,&rc);	/* erase end of screen */

	attroff();
	move(1,6);
}

void tperbos(void)
{
	ULONG rc;


	bcolor(BLACK);
	clear();

	move(5,4);
	color(YELLOW);
	Write(StandardOut,"--JUNK--",8,&rc);

	move(5,5);
	Write(StandardOut,"--JUNK--",8,&rc);
	color(RED);
	Write(StandardOut,"LEAVEME",7,&rc);

	move(12,5);
	attroff();bcolor(CYAN);color(BLACK);
	sleep(15);
	erasebos();

	attroff();
	move(1,6);
}

void tpereol(void)
{
	ULONG rc;

	bcolor(BLACK);
	clear();

	move(1,4);
	color(RED);
	Write(StandardOut,"before",6,&rc);

	move(5,5);
	Write(StandardOut,"LEAVEME",7,&rc);
	color(YELLOW);
	Write(StandardOut,"--JUNK--",8,&rc);

	move(1,6);
	color(RED);
	Write(StandardOut,"after",5,&rc);

	attroff();color(BLACK);bcolor(CYAN);
	move(12,5);

	sleep(10);
	eraseeol();

	attroff();
	move(1,7);
}

void tpereol2(void)
{
	ULONG rc;

	bcolor(BLACK);
	clear();

	move(1,4);
	color(RED);
	Write(StandardOut,"before",6,&rc);

	move(5,5);
	Write(StandardOut,"LEAVEME",7,&rc);
	color(YELLOW);
	Write(StandardOut,"-JUNK--",8,&rc);

	move(0,6);
	color(RED);
	Write(StandardOut,"after",5,&rc);

	attroff();color(BLACK);bcolor(CYAN);
	move(12,5);

	sleep(10);
	Write(StandardOut,"\033[0K",4,&rc);	/* erase end of line */

	attroff();
	move(1,7);
}

void tperbol(void)
{
	ULONG rc;

	color(RED);
	bcolor(BLACK);
	clear();

	move(1,4);
	Write(StandardOut,"before",6,&rc);

	move(5,5);
	color(YELLOW);
	Write(StandardOut,"--JUNK--",8,&rc);
	color(RED);
	Write(StandardOut,"LEAVEME",7,&rc);

	move(1,6);
	Write(StandardOut,"after",5,&rc);

	attroff();color(BLACK);bcolor(CYAN);
	move(12,5);

	sleep(10);
	erasebol();

	attroff();
	move(1,7);
}

void tperline(void)
{
	ULONG rc;

	bcolor(BLACK);
	clear();

	move(1,4);
	color(RED);
	Write(StandardOut,"The next line should be empty",29,&rc);

	color(YELLOW);
	move(5,5);
	Write(StandardOut,"I WILL BE ERASED SOON",21,&rc);

	move(1,6);
	color(RED);
	Write(StandardOut,"The prompt should be above this",31,&rc);

	bcolor(CYAN); color(BLACK);
	move(10,5);
	sleep(10);

	attroff();
	eraseline();
}

void tpmove(void)
{
	ULONG rc;
	int i;
	char c;

	clear();

	for (i=0 ; i < 25 ; i++) {
		move(1,i); Write(StandardOut,"X",1,&rc);
		move(80,i); Write(StandardOut,"X",1,&rc);
		move(25-i,25-i);Write(StandardOut,"X",1,&rc);
		move(80-i,25-i);Write(StandardOut,"X",1,&rc);
		c = 'a' + i;
		move(40,i);Write(StandardOut,&c,1,&rc);
	}
	move(1,1);
}

void tpmover(void)
{
	ULONG rc;

	attroff(); bcolor(BLACK);
	clear();

	color(RED);
	move(40,10); Write(StandardOut,"start",5,&rc);
	color(GREEN);
	moveleft(10); Write(StandardOut,"*****",5,&rc);
	moveright(5); Write(StandardOut,"*****",5,&rc);
	moveup(1);moveleft(10); Write(StandardOut,"*****",5,&rc);
	movedown(2);moveleft(5); Write(StandardOut,"*****",5,&rc);

	/* test border condidtions */
	color(BLUE);attrhi();attrrv();
	move(40,2);Write(StandardOut,"2",1,&rc);	/* top */
	moveup(2);Write(StandardOut,"1",1,&rc);

	move(2,15);Write(StandardOut,"2",1,&rc);	/* left */
	moveleft(2);Write(StandardOut,"1",1,&rc);

	move(40,39);Write(StandardOut,"2",1,&rc);	/* bottom */
	movedown(2);Write(StandardOut,"1",1,&rc);

	move(79,15);Write(StandardOut,"2",1,&rc);	/* right */
	moveright(2);Write(StandardOut,"1",1,&rc);

	/* no digit */
	attroff();attrhi();color(BLUE);bcolor(WHITE);
	move(5,5);
	Write(StandardOut,"+",1,&rc);
	Write(StandardOut,"\033[D\033[A1",7,&rc);
	Write(StandardOut,"\033[B2",4,&rc);
	Write(StandardOut,"\033[D\033[D\033[B3",10,&rc);
	Write(StandardOut,"\033[D\033[D\033[A4",10,&rc);

	attroff();
	move(1,1);
}

void
tpfont(void)
{
	char buf[20];
	ULONG rc;
	int i,j;
	

	clear();

	for (j=0; j<=1 ; j++) {
		move(1,5+j);
		for (i=1;i<=9;i++) {
			sprintf(buf,"\033[%d;%d D%d",j,i,i);
			Write(StandardOut,buf,strlen(buf),&rc);
		}
	}

	move(1,8);
	for (j=0; j<=9; j++) {
		sprintf(buf,"\033[1%dm%d",j,j);
		Write(StandardOut,buf,strlen(buf),&rc);
	}

	move(1,9);
}

void tpattr(void)
{
	ULONG rc;

	clear();

	attroff();

	attrul();
	printf("underlined\n");

	attroff();
	attrrv();
	printf("reverse video\n");

	attrul();
	printf("reverse and underlined\n");

	attroff();
	printf("\033[4;7msame as above, but multi-parametered\n");

	attroff();
}

void tpcs(void)
{
	char buf[20];
	ULONG rc;
	int i;

	clear();

	/* first test nulls -- should be ignored */
	Write(StandardOut,"nulls[",6,&rc);
	bzero(buf,20);
	Write(StandardOut,buf,20,&rc);
	Write(StandardOut,"] -- brakets should be empty\n",29,&rc);

	/* test Bel */
	Write(StandardOut,"bell?\007\n",7,&rc);

	/* horizontal tab tested in "tabs" testcase */

	/* LF, VT, and FF -- should print ABCD in a vertical column */
	Write(StandardOut,"A\012B\013C\014D\n",8,&rc);

	/* CR */
	Write(StandardOut,"test cr",7,&rc);
	Write(StandardOut,"\015",1,&rc);
	sleep(1);
	Write(StandardOut,"\n",1,&rc);

	/* backspace */
	color(RED);
	bcolor(WHITE);
	Write(StandardOut,"backspace",9,&rc);
	attroff();
	for (i=0 ;i < 20; i++) {
		sleep(1);
		Write(StandardOut,"\010",1,&rc);
	}
	Write(StandardOut,"\n",1,&rc);
}

void tpcolor(void)
{
	ULONG rc;

	clear();

	printf("default\n");

	color(BLACK); Write(StandardOut,"black\t",6,&rc);
	attrhi(); Write(StandardOut,"black\t",6,&rc);
	attroff();
	color(RED); Write(StandardOut,"red\t",4,&rc);
	attrhi(); Write(StandardOut,"red\t",4,&rc);
	attroff();
	color(GREEN); Write(StandardOut,"green\t",6,&rc);
	attrhi(); Write(StandardOut,"green\t",6,&rc);
	attroff();
	color(YELLOW); Write(StandardOut,"yellow\t",7,&rc);
	attrhi(); Write(StandardOut,"yellow\n",7,&rc);
	attroff();
	color(BLUE); Write(StandardOut,"blue\t",5,&rc);
	attrhi(); Write(StandardOut,"blue\t",5,&rc);
	attroff();
	color(MAGENTA); Write(StandardOut,"magenta\t",8,&rc);
	attrhi(); Write(StandardOut,"magenta\t",8,&rc);
	attroff();
	color(CYAN); Write(StandardOut,"cyan\t",5,&rc);
	attrhi(); Write(StandardOut,"cyan\t",5,&rc);
	attroff();
	color(WHITE); Write(StandardOut,"white\t",6,&rc);
	attrhi(); Write(StandardOut,"white\n\n",6,&rc);
	attroff();

	bcolor(BLACK); printf("black\n");
	bcolor(RED); printf("red\n");
	bcolor(GREEN); printf("green\n");
	bcolor(YELLOW); printf("yellow\n");
	bcolor(BLUE); printf("blue\n");
	bcolor(MAGENTA); printf("magenta\n");
	bcolor(CYAN); printf("cyan\n");
	bcolor(WHITE); printf("white\n\n");

	attroff();
}

void
tpac(void)
{
	int i,c;
	ULONG rc;

	attroff(); clear();

	/* normal */
	for (c=0; c < 8; c++) {
		attroff();
		for(i=0 ; i < 16; i++) {
			if (i == 8) attrhi();
			color(c); bcolor(i%8);
			Write(StandardOut,"XXXX",4,&rc);
		}
		color(c);
		Write(StandardOut,"                    \n",21,&rc);
	}

	/* reverse video */
	for (c=0; c < 8; c++) {
		attroff(); attrrv();
		for(i=0 ; i < 16; i++) {
			if (i == 8) attrhi();
			color(c); bcolor(i%8);
			Write(StandardOut,"XXXX",4,&rc);
		}
		color(c);
		Write(StandardOut,"                    \n",21,&rc);
	}

	/* underline */
	for (c=0; c < 8; c++) {
		attroff(); attrul();
		for(i=0 ; i < 16; i++) {
			if (i == 8) attrhi();
			color(c); bcolor(i%8);
			Write(StandardOut,"XXXX",4,&rc);
		}
		color(c);
		Write(StandardOut,"                    \n",21,&rc);
	}

	/* reverse video underline */
	for (c=0; c < 8; c++) {
		attroff(); attrul(); attrrv();
		for(i=0 ; i < 16; i++) {
			if (i == 8) attrhi();
			color(c); bcolor(i%8);
			Write(StandardOut,"XXXX",4,&rc);
		}
		color(c);
		Write(StandardOut,"                    \n",21,&rc);
	}

	attroff();
}

void
tpdl()
{
	ULONG rc;

	color(RED);
	bcolor(BLACK);
	clear();

	move(0,5);
	Write(StandardOut,"before",6,&rc);

	move(0,7);
	Write(StandardOut,"after",5,&rc);

	move(0,6);
	color(YELLOW);
	Write(StandardOut,"target",6,&rc);

	sleep(10);

	Write(StandardOut,"\033[M",3,&rc);

	move(0,8);
	attroff();
}

void tpinsl()
{
	ULONG rc;

	color(RED);
	bcolor(BLACK);
	clear();

	move(0,5);
	Write(StandardOut,"one\nthree\nfour\nfive\n",20,&rc);

	move(0,6);
	color(YELLOW);
	Write(StandardOut,"\033[L",3,&rc);
	Write(StandardOut,"two (inserted line)",19,&rc);

	attroff();
}

void tpcr()
{
	ULONG rc;

	clear();
	move(0,5);
	Write(StandardOut,"hello world, here comes a CR!\015",30,&rc);
	sleep(5);
	move(0,8);
}
