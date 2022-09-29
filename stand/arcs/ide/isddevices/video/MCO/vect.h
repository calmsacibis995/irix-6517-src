#ifndef VECTDEF
#define VECTDEF

#include "math.h"

typedef struct vect {
    float x, y, z, w;
} vect;

float vlength();
float vdot();
float flerp();
vect *vnew();
vect *vclone();

#endif
