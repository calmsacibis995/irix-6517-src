/*
 * Copyright 1989,1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	$Revision: 1.2 $
 */

#include "fddivis.h"

#define fontheight	15.0
static void DrawString(char *);

void
fontInit(void)
{
	for (int i = 0; i < FVYPHS; i++) {
		struct fvyph *g = &fvyph[i];
		struct fvyphcommand *p,*end;

		::makeobj(i);
		p = &(g->commands[0]);
		for (end = &(g->commands[g->ncommands]); p < end; p++) {
			if (p->cmd == 'M')
				::move2i((Icoord)(p->point[0]),
					 (Icoord)(p->point[1]));
			else
				::draw2i((Icoord)(p->point[0]),
					 (Icoord)(p->point[1]));
		}
		::translate((Coord)(g->csize[0]), (Coord)0., (Coord)0.);
		::closeobj();
	}
}

void
DrawStringLowerLeft(char *s)
{
	DrawString(s);
}

void
DrawStringCenterLeft(char *s)
{
	pushmatrix();

	translate(0.0,-0.5*fontheight,0.0);
	DrawString(s);

	popmatrix();
}

void
DrawStringCenterRight(char *s)
{
	pushmatrix();

	translate(-StringWidth(s),-0.5*fontheight,0.0);
	DrawString(s);

	popmatrix();
}

void
DrawStringCenterCenter(char *s)
{
	pushmatrix();

	translate(-0.5*StringWidth(s),-0.5*fontheight,0.0);
	DrawString(s);

	popmatrix();
}

static void
DrawString(char *s)
{
	char c;

	if (s == NULL)
		return;

	while ((c = *s++) != NULL)
		callobj((unsigned int) c);
}

void
DrawChar(unsigned char c)
{
	callobj((unsigned int) c);
}

int
StringWidth(char *s)
{
	int size = 0;
	char c;

	if (s == NULL)
		return(0);

	while ((c = *s++) != NULL)
		size += fvyph[c].csize[X];

	return(size);
}

int
StringHeight(char *s)
{
	int size = 0;
	char c;

	while ((c = *s++) != NULL) {
		if (size < fvyph[c].csize[Y])
			size = fvyph[c].csize[Y];
	}

	return(size);
}
