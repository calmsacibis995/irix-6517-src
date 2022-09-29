#ifndef _RESIZEVIEW_
#define _RESIZEVIEW_
/*
 * Copyright 1989,1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Visual FDDI Managment System
 *
 *	$Revision: 1.2 $
 */


#include <tkView.h>
#include <tkParentView.h>

class ResizeView : public tkView
{
public:
	virtual void resize();
};

class ResizeParentView : public tkParentView
{
public:
	virtual void resize();
};
#endif
