#ifndef _TRIG_H_
#define _TRIG_H_
/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Fast trigonometric functions
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

#define M_PI	3.14159265358979323846
#define M_1_PI	0.31830988618379067154
#define M_2PI	(2.0 * M_PI)

#define DEG(r)	((float) (r) * (180.0 * M_1_PI))
#define RAD(x)	((float) (x) / (180.0 * M_1_PI))

float l_sin(int);
float l_sin(float);

float l_cos(int);
float l_cos(float);
#endif
