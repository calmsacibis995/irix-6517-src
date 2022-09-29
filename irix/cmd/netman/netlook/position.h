#ifndef _POSITION_H_
#define _POSITION_H_
/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Netlook Positioning
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

typedef int     p2i[2];		/* 2D integer point			*/
typedef float   p2f[2];		/* 2D float point			*/
typedef int     p3i[3];		/* 3D integer point			*/
typedef float   p3f[3];		/* 3D float point			*/

#define X	0
#define Y	1
#define Z	2

class tuRect;

class Rectf {
private:
	float x, y, width, height;

public:
	Rectf(void);
	Rectf(float, float, float, float);

	Rectf &operator=(const Rectf &);
	Rectf &operator=(tuRect &);

	void set(float, float, float, float);

	inline const float getX(void);
	inline const float getY(void);
	inline const float getWidth(void);
	inline const float getHeight(void);

	inline void setX(float);
	inline void setY(float);
	inline void setWidth(float);
	inline void setHeight(float);
};

class Position			/* Position and extent of object	*/
{
public:
	p2f	position;	/* Location for line			*/
	Rectf	extent;		/* Extent of object			*/
};

// Rectf inline functions
const float
Rectf::getX(void)
{
    return x;
}

const float
Rectf::getY(void)
{
    return y;
}

const float
Rectf::getWidth(void)
{
    return width;
}

const float
Rectf::getHeight(void)
{
    return height;
}

void
Rectf::setX(float f)
{
    x = f;
}

void
Rectf::setY(float f)
{
    y = f;
}

void
Rectf::setWidth(float f)
{
    width = f;
}

void
Rectf::setHeight(float f)
{
    height = f;
}

#endif
