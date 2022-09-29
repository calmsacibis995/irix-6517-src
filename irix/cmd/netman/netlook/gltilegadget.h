/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	GL Tile Gadget
 *
 *	$Revision: 1.1 $
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

#include <tuGLGadget.h>

class GLTileGadget : public tuGLGadget {
public:
	GLTileGadget(tuGadget *, const char *);

	virtual void bindResources(tuResourceDB * = NULL,
				   tuResourcePath * = NULL);
	virtual void render(void);
	virtual void getLayoutHints(tuLayoutHints *);

	void setColor(unsigned int c);
	void setColor(unsigned int c1, unsigned int c2);
	void setColor(unsigned int c1, unsigned int c2,
		      unsigned int c3, unsigned int c4);

private:
	unsigned int colors;
	unsigned int tileColor[4];
};
