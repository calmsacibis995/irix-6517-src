#ifndef _FONT_H_
#define _FONT_H_
/*
 * Copyright 1989-1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Font definitions
 *
 *	$Revision: 1.2 $
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
#endif
