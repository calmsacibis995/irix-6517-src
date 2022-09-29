#ifndef __boxes_h_
#define __boxes_h_

/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	misc data classes
 *
 *	$Revision: 1.1 $
 *	$Date: 1992/09/08 00:33:46 $
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


class fBox {
  public:
    fBox() {
	x = y = width = height = 0;
    }
    fBox(float nx, float ny, float nwidth, float nheight) {
	x = nx;
	y = ny;
	width = nwidth;
	height = nheight;
    }
    fBox(const fBox& r) {
	x = r.x;
	y = r.y;
	width = r.width;
	height = r.height;
    }
    fBox& operator=(const fBox& c) {
	x = c.x;
	y = c.y;
	width = c.width;
	height = c.height;
	return *this;
    }
    int operator==(const fBox& c) {
	return x == c.x && y == c.y && width == c.width && height == c.height;
    }
    float getX() {return x;}
    float getY() {return y;}
    float getWidth() {return width;}
    float getHeight() {return height;}

    void setX(float value) {x = value;}
    void setY(float value) {y = value;}
    void setWidth(float value) {width = value;}
    void setHeight(float value) {height = value;}

    void move(float nx, float ny) {x = nx; y = ny;}
    void resize(float nw, float nh) {width = nw; height = nh;}
	    
  protected:
    float x;
    float y;
    float width;
    float height;
};



class iBox {
  public:
    iBox() {
	x = y = width = height = 0;
    }
    iBox(int nx, int ny, int nwidth, int nheight) {
	x = nx;
	y = ny;
	width = nwidth;
	height = nheight;
    }
    iBox(const iBox& r) {
	x = r.x;
	y = r.y;
	width = r.width;
	height = r.height;
    }
    iBox& operator=(const iBox& c) {
	x = c.x;
	y = c.y;
	width = c.width;
	height = c.height;
	return *this;
    }
    int operator==(const iBox& c) {
	return x == c.x && y == c.y && width == c.width && height == c.height;
    }
    int getX() {return x;}
    int getY() {return y;}
    int getWidth() {return width;}
    int getHeight() {return height;}

    void setX(int value) {x = value;}
    void setY(int value) {y = value;}
    void setWidth(int value) {width = value;}
    void setHeight(int value) {height = value;}

    void move(int nx, int ny) {x = nx; y = ny;}
    void resize(int nw, int nh) {width = nw; height = nh;}
	    
  protected:
    int x;
    int y;
    int width;
    int height;
};


#endif /* __boxes_h_ */
