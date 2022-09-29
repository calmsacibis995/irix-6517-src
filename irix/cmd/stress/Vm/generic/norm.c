# include "norm.h"

void srand48();
double drand48();
long time();
double sqrt();
double log();

int inited = 0;

void
norm(rv)
   norm_t *rv;
{
   register double v1;
   register double v2;
   register double s;
   register double ver;

	if (!inited) {
		srand48(time((long *) 0));
		inited++;
	}
    retry:
	v1 = (2 * drand48()) - 1;
	v2 = (2 * drand48()) - 1;
	s = (v1 * v1) + (v2 * v2);
	if (s >= 1)
		goto retry;
	ver = sqrt((-2*log(s))/s);
	rv->x1 = v1 * ver;
	rv->x2 = v2 * ver;
	return;
}
