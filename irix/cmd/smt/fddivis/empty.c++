/*
 * Empty View
 */

#include <libc.h>
#include <stdParts.h>
#include "empty.h"

void
emptyView::resize()
{
}


void
emptyView::paint()
{
    pushmatrix();
    pushviewport();
    localTransform();

	Box2 clipBox;
	int ox,oy;
	getClipBox(&clipBox,&ox,&oy);

        // viewport and ortho map entire bounds
        ::viewport(ox - 64, ox + getiExtentX() - 1 + 64,
				oy - 64, oy + getiExtentY() - 1 + 64);
        ::ortho2(-0.5-64.0, getExtentX() - 0.5+64.0,
				-0.5-64.0, getExtentY() - 0.5+64.0);

	withStdColor(BG_ICOLOR) {
	    sboxfi(0, 0, getiExtentX(), getiExtentY());
	}

    popviewport();
    popmatrix();
}
