#include "share_devices.H"

#include <stdlib.h>

//  Why do I put something like this in a file by itself?
//  I must be some kind of neatness freak.

static bool been_here = false;
static bool share_state;

bool share_devices()
{
    if (!been_here)
    {   share_state = system("/etc/chkconfig share_devices") ? false : true;
	been_here = true;
    }
    return share_state;
}

bool export_devices()
{
    return share_devices();
}
