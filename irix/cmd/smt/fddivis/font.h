#ifndef __FONT_H__
#define __FONT_H__
/*
 * Copyright 1989,1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	$Revision: 1.3 $
 */

#define FVYPHS		128	/* Maximum ASCII value for font fvyph	*/
#define FVYPHCOMMANDS	24	/* Maximum number of commands per fvyph	*/

struct fvyphcommand {
	unsigned short cmd;
	unsigned char point[2];
};

struct fvyph {
	unsigned char csize[2];
	unsigned short ncommands;
	struct fvyphcommand commands[FVYPHCOMMANDS];
};

extern struct fvyph fvyph[FVYPHS];

extern void fontInit(void);
extern void DrawStringLowerLeft(char*);
extern void DrawStringCenterLeft(char*);
extern void DrawStringCenterRight(char*);
extern void DrawStringCenterCenter(char*);
extern void DrawChar(unsigned char);
extern int StringWidth(char*);
extern int StringHeight(char*);

#endif /* __FONT_H__ */
