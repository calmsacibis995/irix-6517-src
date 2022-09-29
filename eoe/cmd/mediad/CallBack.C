#include "CallBack.H"

#include <stdlib.h>

unsigned int    CallBack::ncallbacks;
unsigned int    CallBack::ncall_alloc;
CallBack::Pair *CallBack::callbacks;

void
CallBack::add(CallbackProc proc, void *closure)
{
    if (ncallbacks >= ncall_alloc)
    {
	Pair *new_callbacks = new Pair[++ncall_alloc];
	for (unsigned int i = 0; i < ncallbacks; i++)
	    new_callbacks[i] = callbacks[i];
	delete [] callbacks;
	callbacks = new_callbacks;
    }
    callbacks[ncallbacks].proc = proc;
    callbacks[ncallbacks].closure = closure;
    ncallbacks++;
}

void
CallBack::remove(CallbackProc proc, void *closure)
{
    for (int i = 0; i < ncallbacks; i++)
	if (callbacks[i].proc == proc && callbacks[i].closure == closure)
	{   callbacks[i] = callbacks[--ncallbacks];
	    break;
	}
}

void
CallBack::activate(const Event& event)
{
    for (unsigned int i = 0; i < ncallbacks; i++)
	(*callbacks[i].proc)(event, callbacks[i].closure);
}
