#include "IMon.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/imon.h>
#include <sys/sysmacros.h>
#include <unistd.h>

#include "Interest.h"
#include "Log.h"
#include "Scheduler.h"

const intmask_t INTEREST_MASK = (IMON_CONTENT | IMON_ATTRIBUTE | IMON_DELETE |
				 IMON_EXEC | IMON_EXIT | IMON_RENAME);

int		   IMon::imonfd = -2;
unsigned	   IMon::count;
IMon::EventHandler IMon::ehandler;

Boolean
IMon::is_active()
{
    if (imonfd == -2)
    {   imonfd = open("/dev/imon", O_RDONLY, 0);
	if (imonfd < 0)
	    Log::critical("can't open /dev/imon: %m");
	else
	{   Log::debug("opened /dev/imon");
	    (void) Scheduler::install_read_handler(imonfd, read_handler, NULL);
	}
    }
    return imonfd >= 0;
}

IMon::~IMon()
{
    if (!--count && imonfd >= 0)
    {
	//  Tell the scheduler.

	(void) Scheduler::remove_read_handler(imonfd);

	//  Close the inode monitor device.

	if (close(imonfd) < 0)
	    Log::perror("can't close /dev/imon");
	else
	    Log::debug("closed /dev/imon");
	imonfd = -1;
    }
}

IMon::Status
IMon::express(const char *name, struct stat *status)
{
    if (!is_active())
	return BAD;

    struct stat statbuf;
    if (!status)
	status = &statbuf;

    struct interest interest = { (char *) name, status, INTEREST_MASK };
    int rc = ioctl(imonfd, IMONIOC_EXPRESS, &interest);
    if (rc < 0)
    {   Log::info("IMONIOC_EXPRESS on \"%s\" failed: %m", name);
	return BAD;
    }
    else
    {   Log::debug("told imon to monitor \"%s\" = dev %d/%d, ino %d", name,
		   major(status->st_dev), minor(status->st_dev),
		   status->st_ino);
	return OK;
    }
}

IMon::Status
IMon::revoke(const char *name, dev_t dev, ino_t ino)
{
    if (!is_active())
	return BAD;

    static Boolean can_revokdi = true;
    Boolean revokdi_failed;

    if (can_revokdi)
    {
	struct revokdi revokdi = { dev, ino, INTEREST_MASK };
	int rc = ioctl(imonfd, IMONIOC_REVOKDI, &revokdi);
	if (rc < 0)
	{   Log::perror("IMONIOC_REVOKDI on \"%s\" failed", name);
	    revokdi_failed = true;
	    if (errno == EINVAL)
		can_revokdi = false;
	}
	else
	    revokdi_failed = false;
    }
    if (!can_revokdi || revokdi_failed)
    {
	// Try the old revoke ioctl.

	struct interest interest = { (char *) name, NULL, INTEREST_MASK };
	int rc = ioctl(imonfd, IMONIOC_REVOKE, &interest);
	if (rc < 0)
	{   //  Log error at LOG_DEBUG.  IMONIOC_REVOKE fails all the time.
	    Log::debug("IMONIOC_REVOKE on \"%s\" failed: %m", name);
	    return BAD;
	}
    }

    Log::debug("told imon to forget \"%s\"", name);
    return OK;
}

void
IMon::read_handler(int fd, void *)
{
    assert(count == 1);
    qelem_t readbuf[20];
    int rc = read(fd, readbuf, sizeof readbuf);
    if (rc < 0)
        Log::perror("/dev/imon read");
    else
    {   assert(rc % sizeof (qelem_t) == 0);
	rc /= sizeof (qelem_t);
	for (int i = 0; i < rc; i++)
	    if (readbuf[i].qe_what == IMON_OVER)
		Log::error("imon event queue overflow");
	    else
	    {	dev_t dev = readbuf[i].qe_dev;
		ino_t ino = readbuf[i].qe_inode;
		intmask_t what = readbuf[i].qe_what;
		Log::debug("imon said dev %d/%d, ino %ld changed%s%s%s%s%s%s",
			   major(dev), minor(dev), ino,
			   what & IMON_CONTENT   ? " CONTENT"   : "",
			   what & IMON_ATTRIBUTE ? " ATTRIBUTE" : "",
			   what & IMON_DELETE    ? " DELETE"    : "",
			   what & IMON_EXEC      ? " EXEC"      : "",
			   what & IMON_EXIT      ? " EXIT"      : "",
			   what & IMON_RENAME    ? " RENAME"    : "");
		if (what & IMON_EXEC)
		    (*ehandler)(dev, ino, EXEC);
		if (what & IMON_EXIT)
		    (*ehandler)(dev, ino, EXIT);
		if (what & (IMON_CONTENT | IMON_ATTRIBUTE |
			    IMON_DELETE | IMON_RENAME))
		    (*ehandler)(dev, ino, CHANGE);
	    }
    }
}
