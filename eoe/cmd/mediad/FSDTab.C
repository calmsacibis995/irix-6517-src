#include "FSDTab.H"

#include <errno.h>
#include <mntent.h>
#include <string.h>
#include <unistd.h>

#include "CallBack.H"
#include "Log.H"
#include "Volume.H"

const char FSDTab::fsdtab_path[] = "/etc/fsd.tab";

FSDTab::FSDTab()
{
    update();

    CallBack::add(event_proc, this);
}

FSDTab::~FSDTab()
{
    CallBack::remove(event_proc, this);

    (void) unlink(fsdtab_path);
}

void
FSDTab::event_proc(const Event&, void *closure)
{
    FSDTab *p = (FSDTab *) closure;

    p->update();
}

void
FSDTab::update()
{
    int error = 0;			// assume no error

    const char tmplate[] = "/etc/fsdXXXXXX";
    char tmpname[sizeof tmplate];
    (void) strcpy(tmpname, tmplate);
    mktemp(tmpname);
    FILE *f = setmntent(tmplate, "w");
    if (f)
    {
	for (Volume *v = Volume::first(); v; v = v->next())
	{   const mntent *mnt = v->mnt();
	    if (addmntent(f, mnt))
	    {   error = errno;
		break;
	    }
	}
	(void) endmntent(f);
    }
    else
	error = errno;

    // Rename the file before releasing its lock.
    
    if (error == 0)
    {	if (rename(tmpname, fsdtab_path))
	    error = errno;
    }
    if (error)
    {	(void) unlink(tmpname);
	Log::perror("error updating /etc/fsd.tab");
    }
}
