#include "FormatDSO.H"

#include <assert.h>

#include "Format.H"
#include "Log.H"

void
FormatDSO::inspect(Device& device)
{
    //  Ensure DSO is loaded.

    load();

    //  Find inspection proc.  Call it.

    typedef void InspProc(FormatDSO *, Device *);
    InspProc *inspect = (InspProc *) sym("inspect");
    if (inspect)
	(*inspect)(this, &device);
    else
	Log::error("format DSO \"%s\" has no inspection procedure", name());
}
