#ifndef __EMPTY__
#define __EMPTY__
/*
 * Empty View that just paints itself a color
 */

class emptyView : public tkView {
protected:

public:
	void		resize();
	void		paint();
};

#endif
