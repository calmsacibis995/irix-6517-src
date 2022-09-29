/*
 * Copyright 1989,1990 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Font functions for VisualNet
 *
 *	$Revision: 1.1 $
 */

#include <gl/gl.h>
#include <stdio.h>
#include "font.h"

static Coord fontheight = 15.0;
static Object cobj[GLYPHS];

static void DrawString(char *s);

#define X   0
#define Y   1
#define Z   2

void
fontInit()
{
	for (int i = 0; i < GLYPHS; i++)
	{
		struct glyph *g = &glyph[i];
		struct glyphcommand *p,*end;

		makeobj(i);

		for (p = &(g->commands[0]),end = &(g->commands[g->ncommands]);
								p < end; p++)
		{
			if (p->cmd == 'M')
				move2i(p->point[0],p->point[1]);
			else
				draw2i(p->point[0],p->point[1]);
		}

		translate(g->size[X],0.0,0.0);

		closeobj();
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
		size += glyph[c].size[X];

	return(size);
}

int
StringHeight(char *s)
{
	int size = 0;
	char c;

	while ((c = *s++) != NULL)
	{
		if (size < glyph[c].size[Y])
			size = glyph[c].size[Y];
	}

	return(size);
}
