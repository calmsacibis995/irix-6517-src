/*
 * Copyright 1989-1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Netlook Font Functions
 *
 *	$Revision: 1.4 $
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#include <stdio.h>
#include <gl/gl.h>
#include "font.h"
#include "position.h"

static Coord fontheight = 15.0;
static Object cobj[GLYPHS];

static void DrawString(char *);

void
fontInit()
{
	for (int i = 0; i < GLYPHS; i++) {
		struct glyph *g = &glyph[i];
		struct glyphcommand *p,*end;

		makeobj(i);

		end = &(g->commands[g->ncommands]);
		for (p = &(g->commands[0]); p < end; p++) {
			if (p->cmd == 'M')
				move2i(p->point[0],p->point[1]);
			else
				draw2i(p->point[0],p->point[1]);
		}

		translate(g->size[X],0.0,0.0);

		closeobj();
	}
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

	while ((c = *s++) != NULL) {
		if (size < glyph[c].size[Y])
			size = glyph[c].size[Y];
	}

	return(size);
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
