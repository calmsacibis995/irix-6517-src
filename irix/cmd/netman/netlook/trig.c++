/*
 * Copyright 1989-1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Trigonometric function table
 *
 *	$Revision: 1.5 $
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

#include <math.h>
#include "trig.h"

class _trigtable
{
	float t_table[901];
public:
	_trigtable();
	friend float l_sin(int angle);
	friend float l_cos(int angle);
	friend float l_sin(float rad);
	friend float l_cos(float rad);
};

#define M_1_PI	0.31830988618379067154
#define DEG(r)	((float) (r) * (180.0 * M_1_PI))
#define RAD(x)	((float) (x) / (180.0 * M_1_PI))
 
_trigtable _ttable;

_trigtable::_trigtable()
{
	register int i;

	for (i = 0; i < 901; i++)
		t_table[i] = sinf((float) RAD(i / 10.0));
}

float
l_sin(int angle)
{
	register int quad = 0;
	register float *f;

	while (angle < 0)
		angle += 3600;

	while (angle > 3600)
		angle -= 3600;

	while (angle > 900)
	{
		quad++;
		angle -= 900;
	}

	if (quad & 1)
		f = &_ttable.t_table[900-angle];
	else
		f = &_ttable.t_table[angle];

	if (quad & 2)
		return(-*f);

	return(*f);
}

float
l_cos(int angle)
{
	register int quad = 0;
	register float *f;

	while (angle < 0)
		angle += 3600;

	while (angle > 3600)
		angle -= 3600;

	while (angle > 900)
	{
		quad++;
		angle -= 900;
	}

	if (quad & 1)
		f = &_ttable.t_table[900-angle];
	else
		f = &_ttable.t_table[angle];

	quad++;

	f = (float *) ((int) &_ttable.t_table[0] +
					(int) &_ttable.t_table[900] - (int) f);

	if (quad & 2)
		return(-*f);

	return(*f);
}

float
l_sin(float rad)
{
	return l_sin((int) (DEG(rad)*10.0));
}

float
l_cos(float rad)
{
	return l_cos((int) (DEG(rad)*10.0));
}
