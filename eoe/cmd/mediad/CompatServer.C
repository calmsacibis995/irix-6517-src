#include "CompatServer.H"

#include <assert.h>
#include <bstring.h>
#include <errno.h>
#include <mntent.h>
#include <mediad.h>
#include <unistd.h>
#include <syslog.h>

#include "Device.H"
#include "DeviceAddress.H"
#include "DeviceMonitor.H"
#include "Log.H"
#include "Partition.H"
#include "PartitionAddress.H"
#include "Scheduler.H"
#include "Volume.H"
#include "VolumeAddress.H"
#include "strplus.H"

CompatServer::CompatServer(int fd)
: _client_handler(client_read_proc, this),
  _exclusive_filename(NULL),
  _exclusive_progname(NULL)
{
    _client_handler.fd(fd);
    _client_handler.activate();
}

CompatServer::~CompatServer()
{
    (void) close(_client_handler.fd());

    // If client holds exclusive use on a device, release it.

    if (_exclusive_filename != NULL)
    {   DeviceMonitor *mon = path_to_monitor(_exclusive_filename);
	if (mon != NULL)
	{
	    Log::info("client \"%s\" died.  Releasing exclusive use of \"%s\".",
		      _exclusive_progname, mon->device()->name());
	    mon->resume();
	}
	delete [] _exclusive_filename;
	delete [] _exclusive_progname;
    }
}

void
CompatServer::client_read_proc(int sock, void *closure)
{
    CompatServer *client = (CompatServer *) closure;
    assert(sock == client->_client_handler.fd());
    client->read_message(sock);
}

void
CompatServer::read_message(int sock)
{
    _sock = sock;

    emsg msg;
    rmsg rmsg;
    bool do_reply = true;
    bool expecting_more = false;

    bzero(&rmsg, sizeof rmsg);
    rmsg.mtype = MSG_RETURN;

    int readlen = read(sock, &msg, sizeof msg);
    if (readlen != sizeof msg)
    {   if (readlen < 0)
	    Log::perror("read from compat client %d", sock);
	else if (readlen > 0)
	    Log::error("short read from compat client %d: %d bytes",
		       sock, readlen);
	do_reply = false;
    }
    else
    {	
	switch (msg.mtype)
	{
	case MSG_EJECT:
	    do_reply = handle_eject_msg(msg, rmsg);
	    break;

	case MSG_TEST:
	    do_reply = handle_test_msg(msg, rmsg);
	    break;

	case MSG_TERM:
	    do_reply = handle_terminate_msg(msg, rmsg);
	    break;

	case MSG_SHOWMOUNT:
	    do_reply = handle_showmount_msg(msg, rmsg);
	    break;

	case MSG_SUSPENDON:
	    do_reply = handle_suspend_msg(msg, rmsg);
	    expecting_more = true;
	    break;

	case MSG_SUSPENDOFF:
	    do_reply = handle_resume_msg(msg, rmsg);
	    break;

	case MSG_SETLOGLEVEL:
	    do_reply = handle_setloglevel_msg(msg, rmsg);
	    break;

	case MSG_STARTENTRY:
	case MSG_STOPENTRY:
	    break;

	default:
	    Log::error("unknown compat message %d", msg.mtype);
	    do_reply = false;
	    break;
	}
    }
    if (do_reply && write(sock, &rmsg, sizeof rmsg) < 0)
    {   if (errno != EPIPE)
	    Log::perror("write to compat client %d", sock);
    }

    if (!expecting_more)
	delete this;
}

bool
CompatServer::handle_eject_msg(const emsg& emsg, rmsg& rmsg)
{
    DeviceMonitor *devmon = NULL;

    if (emsg.ctrl >= 0)
    {   Log::info("Got Eject msg for ctlr %d id %d lun %d",
		  emsg.ctrl, emsg.scsi, emsg.lun);
	DeviceAddress da(emsg.ctrl, emsg.scsi, emsg.lun);
	Device *d = Device::at(da);
	if (d)
	    devmon = DeviceMonitor::at(d);
    }
    else
    {
	if (emsg.filename[0] == '\0')
	{   Log::info("Got Eject msg for first find");
	    devmon = DeviceMonitor::first();
	}
	else
	{   Log::info("Got Eject msg for device \"%s\"", emsg.filename);

	    //  Navigate the twisty maze of little data structures.
	    //	    filename -> volume address -> partition address ->
	    //	    device address -> device ->device monitor

	    VolumeAddress va(emsg.filename);
	    const PartitionAddress *pa = va.partition(0);
	    if (pa)
	    {   const DeviceAddress& da = pa->device();
		Device *d = Device::at(da);
		if (d)
		    devmon = DeviceMonitor::at(d);
	    }
	    else
	    {
		//  Didn't find a device.  Try a mounted filesystem.

		for (const Volume *v = Volume::first(); v; v = v->next())
		{   const char *dir = v->base_dir();
		    if (dir && !strcmp(emsg.filename, dir))
		    {   const Partition *p = v->partition(0);
			if (p)
			{   const Device *d = p->device();
			    if (d)
				devmon = DeviceMonitor::at(d);
			}
		    }
		}
	    }
	}
    }
    if (devmon)
	rmsg.error = devmon->eject();
    else
	rmsg.error = RMED_ENODEVICE;
    return true;
}

bool
CompatServer::handle_test_msg(const emsg&, rmsg& rmsg)
{
    rmsg.mtype = MSG_TESTOK;
    rmsg.error = RMED_NOERROR;
    if (write(_sock, &rmsg, sizeof rmsg) < 0)
    {   Log::info("compat reply: %m, exiting.");
	Scheduler::exit();
    }
    return false;
}

bool
CompatServer::handle_terminate_msg(const emsg&, rmsg&)
{
    Log::info("Got Terminate message.  Exiting.");
    Scheduler::exit();
    return false;
}

bool
CompatServer::handle_showmount_msg(const emsg& emsg, rmsg& rmsg)
{
    Log::info("Got Show Mount message for \"%s\"", emsg.filename);

    //  On a showmount message, we search the device list for a volume
    //  mounted on emsg.filename, and return that volume's mount point.

    for (Volume *vol = Volume::first(); vol; vol = vol->next())
  	for (unsigned i = 0; i < vol->npartitions(); i++)
	    if (!strcmp(vol->fsname(), emsg.filename))
	    {   strncpy(rmsg.mpoint, vol->dir(), sizeof rmsg.mpoint);
		return true;
	    }

    //  Didn't find it.  Find a corresponding Device and return that
    //  Device's default mount point.

    const VolumeAddress va(emsg.filename);
    const PartitionAddress *pap;
    for (unsigned i = 0; pap = va.partition(i); i++)
    {   const DeviceAddress& da = pap->device();
	Device *dev = Device::at(da);
	if (dev)
	{   rmsg.mpoint[0] = '/';
	    strncpy(rmsg.mpoint + 1, dev->name(), sizeof rmsg.mpoint - 1);
	    return true;
	}
    }

    // We struck out.  Return a reply message with an empty filename field.

    return true;
}

bool
CompatServer::handle_suspend_msg(const emsg& emsg, rmsg& rmsg)
{
    DeviceMonitor *mon = path_to_monitor(emsg.filename);
    if (!mon)
	rmsg.error = RMED_ENODEVICE;
    else if (mon->is_suspended())
	rmsg.error = RMED_EACCESS;	// Somebody else already holds it
    else
    {
	Log::info("client \"%s\" takes exclusive use of device \"%s\".",
		  emsg.progname, mon->device()->name());
	rmsg.error = mon->suspend();
	_exclusive_filename = strdupplus(emsg.filename);
	_exclusive_progname = strdupplus(emsg.progname);
    }
    return true;
}

bool
CompatServer::handle_resume_msg(const emsg& /* emsg */, rmsg& rmsg)
{
    if (!_exclusive_filename)
    {	rmsg.error = RMED_EUSAGE;
	return true;
    }

    DeviceMonitor *mon = path_to_monitor(_exclusive_filename);
    if (mon)
    {
	Log::info("client \"%s\" releases exclusive use of device \"%s\".",
		  _exclusive_progname, mon->device()->name());
	mon->resume();
	delete [] _exclusive_filename; _exclusive_filename = NULL;
	delete [] _exclusive_progname; _exclusive_progname = NULL;
    }
    else
	rmsg.error = RMED_ENODEVICE;

    return true;
}

bool
CompatServer::handle_setloglevel_msg(const emsg& emsg, rmsg& rmsg)
{
    int level = emsg.scsi;
    char *levelname;

    if (level >= LOG_DEBUG)
    {	Log::debug();
	levelname = "high";
    }
    else if (level >= LOG_INFO)
    {   Log::info();
	levelname = "medium";
    }
    else
    {	Log::error();
	levelname = "normal (low)";
    }
    Log::error("Got setloglevel message.  Log verbosity is %s", levelname);
    rmsg.mtype = MSG_RETURN;
    rmsg.error = 0;
    return true;
}

DeviceMonitor *
CompatServer::path_to_monitor(const char *path)
{
    //  Navigate the twisty maze of little data structures.
    //
    //	Either:
    //
    //	    filename ->
    //		volume address ->
    //		    partition address ->
    //			device address ->
    //			    device ->
    //				device monitor
    //
    //	or:
    //
    //	    dir -> volume -> partiton -> device -> device monitor

    VolumeAddress va(path);
    const PartitionAddress *pa = va.partition(0);
    if (pa)
    {   const DeviceAddress& da = pa->device();
	Device *d = Device::at(da);
	if (d)
	    return DeviceMonitor::at(d);
    }

    //  Didn't find a device.  Try a mounted filesystem.

    for (const Volume *v = Volume::first(); v; v = v->next())
    {   const char *dir = v->dir();
	if (dir && !strcmp(dir, path))
	{   const Partition *p = v->partition(0);
	    if (p)
	    {   const Device *d = p->device();
		if (d)
		    return DeviceMonitor::at(d);
	    }
	}
    }

    return NULL;			// admit defeat.
}
