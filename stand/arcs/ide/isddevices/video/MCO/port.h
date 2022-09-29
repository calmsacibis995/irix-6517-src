#ifndef	__GL_PORT_H__
#define	__GL_PORT_H__
/*
 *	Defines for colors used by window manager tools
 *
 *			Paul Haeberli - 1984
 *
 */
#include <stdio.h>
#include <gl/device.h>

#define MIN(a, b)	(((a) < (b)) ? (a) : (b))
#define MAX(a, b)	(((a) > (b)) ? (a) : (b))
#define ABS(a)		(((a) > 0) ? (a) : -(a))

#define GREY(x)         (16+(x))                    /* shades of grey */
#define DESKTOP(x)      (8+(x))                     /* desktop colors */

#define COLORSYS_RGB	1
#define COLORSYS_CMY	2
#define COLORSYS_HSV	3
#define COLORSYS_HLS	4
#define COLORSYS_YIQ	5

float getgamma();

#define PORTDEF		/* for backwards compatibility */
#endif	/* !__GL_PORT_H__ */
