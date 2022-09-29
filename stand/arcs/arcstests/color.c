/* Change the colors on the prom text port.
 * Usage: colors [-f foreground] [-b background] -h -u -r
 *	-f sets foreground color
 *	-b sets background color
 *	-h sets highlight mode
 *	-u sets underline mode
 *	-r sets revers video mode
 */

#ident "$Revision: 1.2 $"

#include <arcs/io.h>

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

	scolor(argc-1,&argv[1]);
	return;
}

static char *colors[] = {
	"black",
	"red",
	"green",
	"yellow",
	"blue",
	"magenata",
	"cyan",
	"white",
	0
};

int
matchcolor(char *name)
{
	int i;

	for (i=0; colors[i]; i++)
		if (!strcmp(name,colors[i])) return(i);
	return(-1);
}

int
scolor(int argc, char **argv)
{
	int i;
	int hi, ul, rv;
	char *fg, *bg;

	hi = ul = rv = 0;
	fg = bg = 0;

	for (i = 0; i < argc; i++) {
		if (!strcmp(argv[i],"-f"))
			fg = argv[++i];
		else if (!strcmp(argv[i],"-b"))
			bg = argv[++i];
		else if (!strcmp(argv[i],"-h"))
			hi = 1;
		else if (!strcmp(argv[i],"-u"))
			ul = 1;
		else if (!strcmp(argv[i],"-r"))
			rv = 1;
	}

	attroff();
	if (fg)
		if ((i=matchcolor(fg)) != -1) color(i);
	if (bg)
		if ((i=matchcolor(bg)) != -1) bcolor(i);
	if (hi) attrhi();
	if (ul) attrul();
	if (rv) attrrv();
}
