#ifndef __FONT__
#define __FONT__
/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Font declarations for NetTop (stolen from netlook) 
 *
 *	$Revision: 1.1 $
 *	$Date: 1992/07/02 18:04:53 $
 */

#define GLYPHS		128	/* Maximum ASCII value for font glyph	*/
#define GLYPHCOMMANDS	24	/* Maximum number of commands per glyph	*/

typedef struct glyphcommand {
	unsigned short cmd;
	unsigned char point[2];
};

struct glyph {
	unsigned char size[2];
	unsigned short ncommands;
	struct glyphcommand commands[GLYPHCOMMANDS];
};

extern struct glyph glyph[GLYPHS];

void    fontInit(void);
int StringWidth(char *);
int StringHeight(char *);
void    DrawStringCenterLeft(char *);
void    DrawStringCenterRight(char *);
void    DrawStringCenterCenter(char *);
void    DrawStringLowerLeft(char *);
void    DrawChar(unsigned char);

#endif
