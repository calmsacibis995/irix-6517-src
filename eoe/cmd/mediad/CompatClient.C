#include "CompatClient.H"

#include <assert.h>
#include <bstring.h>
#include <ctype.h>
#include <errno.h>
#include <mntent.h>
#include <mediad.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "CompatListener.H"		// for MEDIAD_SOCKNAME
#include "Fsd.H"
#include "Log.H"

CompatClient::CompatClient()
: _sent(false), _verbose(false)
{
    bzero(&_msg, sizeof _msg);
    bzero(&_reply, sizeof _reply);
}

void
CompatClient::verbose(bool yesno)
{
    assert(!_sent);
    _verbose = yesno;
}

const rmsg&
CompatClient::reply()
{
    assert(_sent);
    return _reply;
}

const char *
CompatClient::message(int code)
{
    static const char *msg_array[] = {
	" ",						/* no error */
	"Incorrect usage.  Check the man page.",	  
	"The drive is busy.  Make sure no programs are using the drive.",
	"Unable to eject.  Try hardware eject.",
	"There is no medium in the drive.  Make sure you are specifying the right drive.",
	"Software eject not supported.  Use hardware eject.",
	"No such device.  Check your scsi id, fsname, or dir.", 
	"A system error has occured.  See /usr/adm/SYSLOG for more info.",
	"Could not access a device.  Check permissions on the device.",
	"No mediad is running.  Make sure that mediad is running.",
	"Another mediad is running.  Only one is allowed at a time.",
	"There are more than one removable media device on the system.  Please specify one.",
	"Mediad was terminated by a signal.  You must restart mediad.",
	"The device has been unmounted.  Use hardware eject to remove the device now."
    };
    const int nmsgs = sizeof msg_array / sizeof msg_array[0];

    if (code >= 0 && code < nmsgs)
	return msg_array[code];
    else
	return NULL;
}

// Make sure that mediad is not already running on the system before I
// start.  Returns a 1 if there is one
// running already, 0 otherwise.

void
CompatClient::send_test()
{
    _msg.mtype = MSG_TEST;

    send_mediad_sockmsg(DUPMEDIAD_TIMEOUT);
}

void
CompatClient::send_eject(const char *fsname, int ctlr, int id, int lun)
{
    _msg.mtype = MSG_EJECT;
    if (fsname)
    {   strncpy(_msg.filename, fsname, sizeof _msg.filename - 1);
	_msg.ctrl = -1;
	_msg.scsi = -1;
	_msg.lun = -1;
    }
    else
    {	_msg.ctrl = ctlr;
	_msg.scsi = id;
	_msg.lun = lun;
    }
    send_mediad_sockmsg(EJECT_TIMEOUT);
}

void
CompatClient::send_kill()
{
    _msg.mtype = MSG_TERM;
    send_mediad_sockmsg(KILL_TIMEOUT);
}

void
CompatClient::send_show_mpoint(const char *fsname)
{
    _msg.mtype = MSG_SHOWMOUNT;
    strncpy(_msg.filename, fsname, sizeof _msg.filename);
    send_mediad_sockmsg(SOCK_TIMEOUT);
}

void
CompatClient::send_start_entry(const char *fsname)
{
    _msg.mtype = MSG_STARTENTRY;
    strncpy(_msg.filename, fsname, sizeof _msg.filename);
    send_mediad_sockmsg(SOCK_TIMEOUT);
}

void
CompatClient::send_query_dev(const char *fsname)
{
    _msg.mtype = MSG_QUERY;
    strncpy(_msg.filename, fsname, sizeof _msg.filename);
    send_mediad_sockmsg(SOCK_TIMEOUT);
}

void
CompatClient::send_stop_entry(const char *fsname)
{
    _msg.mtype = MSG_STOPENTRY;
    strncpy(_msg.filename, fsname, sizeof _msg.filename);
    send_mediad_sockmsg(SOCK_TIMEOUT);
}

void
CompatClient::send_set_log_level(int level)
{
    _msg.mtype = MSG_SETLOGLEVEL;
    _msg.scsi = level;
    send_mediad_sockmsg(SOCK_TIMEOUT);
}

void
CompatClient::sigalrm_handler()
{ }

/*
 * General purpose send mediad a packet on the socket routine.
 */

void
CompatClient::send_mediad_sockmsg(int wait_time)
{
    assert(!_sent);
    _sent = true;

    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0)
    {
	Log::perror("can't create socket");
	_reply.error = RMED_ENOMEDIAD;
	return;
    }
    struct sockaddr_un addr;
    bzero(&addr, sizeof addr);
    addr.sun_family = AF_UNIX;
    assert(strlen(CompatListener::MEDIAD_SOCKNAME) < sizeof addr.sun_path);
    strcpy(addr.sun_path, CompatListener::MEDIAD_SOCKNAME);
    if (connect(sock, (struct sockaddr *) &addr, sizeof addr) == -1)
    {   if (_verbose)
	    Log::perror("can't connect to mediad");
	(void) close(sock);
	_reply.error = RMED_ENOMEDIAD;
	return;
    }
    (void) signal(SIGALRM, (SIG_PF) sigalrm_handler);
    alarm(wait_time);
    int ret = write(sock, &_msg, sizeof _msg);
    alarm(0);
    if (ret < 0)
    {
	if (errno == EINTR)
	{   Log::error("send to mediad timed out after %d seconds", wait_time);
	    (void) close(sock);
	    _reply.error = RMED_ENOMEDIAD;
	    return;
	}

	Log::perror("failed to send message to mediad");
	(void) close(sock);
	_reply.error = RMED_ENOMEDIAD;
	return;
    }
     
    (void) signal(SIGALRM, (SIG_PF) sigalrm_handler);
    alarm(wait_time);
    ret = read(sock, &_reply, sizeof _reply);
    alarm(0);
    if (ret < 0)
    {
	if (errno == EINTR)
	{   if (_verbose)
		Log::error("mediad did not reply after %d seconds", wait_time);
	    (void) close(sock);
	    _reply.error = RMED_ENOMEDIAD;
	    return;
	}

	Log::perror("failed to receive reply from mediad");
	(void) close(sock);
	_reply.error = RMED_ENOMEDIAD;
	return;
    } 

    Log::debug("received reply code %d", _reply.error);
    (void) close(sock);
}
