/*
 *
 */
#include <signal.h>
extern void fr_finish__Fv();

void
setsigs(void)
{
	sigset((int)SIGTERM, fr_finish__Fv);
}

#include <gl/gl.h>
Object vobj;
void
setvobj(void)
{
	vobj = genobj();
	makeobj(vobj);
    		perspective(50, 1.0, 400.0, 600.0);
    		lookat(0.0, 0.0, 500., 0.0, 0.0, 0., 0);
	closeobj();
}
