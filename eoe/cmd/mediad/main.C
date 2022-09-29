#include <assert.h>
#include <bstring.h>
#include <ctype.h>
#include <dslib.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <syslog.h>
#include <unistd.h>

#include "CompatClient.H"
#include "CompatListener.H"
#include "Device.H"
#include "DeviceAddress.H"
#include "DeviceDSO.H"
#include "DeviceInfo.H"
#include "DeviceMonitor.H"
#include "DSReq.H"
#include "Fsd.H"
#include "Inventory.H"
#include "Log.H"
#include "MediaDaemon.H"
#include "MonitorClient.H"
#include "PartitionAddress.H"
#include "Scheduler.H"
#include "RPCListener.H"
#include "VolumeAddress.H"
#include "Volume.H"		// XXX Hack for security bug late in
                                // IRIX 6.5 release cyle.

extern int dsdebug;	// when non-zero enables dslib debug messages

static int mediad_main(int argc, char *argv[]);
static int eject_main(int argc, char *argv[]);
//static int mediad_inetd_main(int argc, char *argv[]);
static void mediad_usage();
static void eject_usage();
//static void inetd_usage();

//static void do_default_reply(int loglevel);
static void start_daemon(bool started_by_inetd, bool debugging, int loglevel);
static int do_eject(const char *path, int ctlr, int id, int lun);
static int do_eject_no_mediad(const char *path, int ctlr, int id, int lun);
static int execute(const char *argv[3]);
static void do_kill();
static void do_show_mpoint(const char *fsname);
static void do_add_dev(const char *fsname,
		       const char *dir,
		       const char *mntopts,
		       bool immediate);
static void do_query_dev(const char *fsname);
static void do_remove_dev(const char *fsname, bool immediate);
static void do_set_log_level(int loglevel);

#ifdef MEDIAD_FROM_INETD
static bool sigusr1_seen;
#endif

// Utility functions.  Simple string handling functions, simple
// system call wrappers.

// isnumber() returns true if str is an unsigned decimal number.

static bool
isnumber(const char *str)
{
    for ( ; *str; str++)
	if (!isdigit(*str))
	    return false;
    return true;
}

// basename() returns the last component of a pathname.  Note
// that the component might have trailing '/' characters.

static const char *
basename(const char *path)
{
    const char *base = path;

    for (const char *p = path; *p; p++)
	if (*p == '/' && p[1] != '\0' && p[1] != '/')
	    base = p + 1;
    return base;
}

// suser() returns true if program was invoked by the superuser.

static bool
suser()
{
    return getuid() == 0;		// Check that real uid == 0.
}

#ifdef MEDIAD_FROM_INETD

//  issock() -- Function ripped off from rpc.mountd.c.  Returns true if
//  file descriptor is a socket.

static bool
issock(int fd)
{
    struct stat st;

    if (fstat(fd, &st) < 0)
	return false;

    //  SunOS returns S_IFIFO for sockets, while 4.3 returns 0 and does not
    //  even have an S_IFIFO mode.  Since there is confusion about what the
    //  mode is, we check for what it is not instead of what it is. 

    switch (st.st_mode & S_IFMT)
    {
    case S_IFCHR:
    case S_IFREG:
    case S_IFLNK:
    case S_IFDIR:
    case S_IFBLK:
	return false;
    default:
	return true;
    }
}

#endif

// Send a test message to mediad and return true if successful.

static bool
test_mediad_present()
{
    CompatClient cc;
    cc.verbose(false);
    cc.send_test();
    return cc.status() == RMED_NOERROR;
}

//  mediad is invoked three ways --
//
//	as eject
//	from inetd
//	from a shell
//
//  The decision process is --
//	if argv[0] is "eject" or ".../eject", then this is eject.
//	else if stdin is a socket, then we were invoked from inetd,
//	else we were invoked from a shell.

extern int
main(int argc, char *argv[])
{
    /* Security Bug 530697 - watch for missing argv */

    if (argc == 0 || argv[0] == NULL)
    {
	fprintf(stderr, "mediad: no argv\n"); 
	exit(1);
    }

    Log::foreground();			// Log to stderr, not syslog.

    if (!strncmp(basename(argv[0]), "eject", 5))
    {
	// This is eject.

	Log::name("eject");		// This program is named eject.
	return eject_main(argc, argv);
    }
#ifdef MEDIAD_FROM_INETD
    else if (issock(0))
    {
	// This is mediad invoked from inetd.

	Log::name("mediad");		// This program is named mediad.
	return mediad_inetd_main(argc, argv);
    }
#endif
    else
    {
	// This is mediad.

	Log::name("mediad");		// This program is named mediad.
	return mediad_main(argc, argv);
    }
}

//  Argument processing when mediad is invoked from a shell.
//
//	Verbs: a e k m p q r u  (default: a)
//	Adverbs:	f modifies/implies a
//			i modifies p, r
//			o modifies p
//	Confused: 	l is either a verb or an adverb modifying a.

static int
mediad_main(int argc, char *argv[])
{
    enum action {
	START_DAEMON,
	EJECT,
	KILL,
	SET_LOG_LEVEL,
	SHOW_MPOINT,
	ADD_DEV,
	QUERY_DEV,
	REMOVE_DEV
    };
    enum modifier {
	FOREGROUND = 1 << 0,
	IMMEDIATE  = 1 << 1,
	LOGLEVEL   = 1 << 2,
	OPTIONS    = 1 << 3,
	TYPE       = 1 << 4
    };

    int nverbs = 0;
    int modifiers = 0;
    enum action what_to_do = START_DAEMON;
    int ok_modifiers = FOREGROUND | LOGLEVEL;
    int loglevel = -1;
    bool any_user_ok = false;
    bool debugging = false;
    char *fsname = NULL;
    char *dir = NULL;
#ifdef OBSOLETE
    char *type = NULL;
#endif
    char *mntopts = NULL;
    int eject_ctlr = -1, eject_id = -1, eject_lun = -1;
    bool immediate = false;

    //  Argument processing.  Each option flag is either a verb or a
    //  modifier.  There can only be one verb, which is kept track of
    //  in what_to_do.  (Zero verbs are okay and imply "-a").  The
    //  flag word "modifiers" keeps track of which modifiers have been
    //  seen.  The word "ok_modifiers" keeps track of which modifiers
    //  are okay for the selected verb.
    //
    //	Some flags eat arguments.  Those that eat one arg do it
    //  in the normal way by using getopt.  "-p" takes two
    //  args, though, and "-e" will take one, two or three args.

    for (int opt; (opt = getopt(argc, argv, "ade:fikl:m:no:pqr:t:u")) != -1; )
	switch(opt)
	{
	case 'a':			// mediad [-l N] -a
	    nverbs++;
	    what_to_do = START_DAEMON;
	    ok_modifiers = FOREGROUND | LOGLEVEL;
	    break;

	case 'd':
	    dsdebug = 1;
	    break;

	case 'e':			// mediad [-l N] -e [ctlr] id [lun]
	    				// mediad [-l N] -e device|mountpt
	    nverbs++;
	    what_to_do = EJECT;
	    ok_modifiers = LOGLEVEL;
	    any_user_ok = true;
	    if (isnumber(optarg))
	    {   eject_ctlr = 0;
		eject_id = 0;
		eject_lun = 0;
		if (argv[optind] && isnumber(argv[optind]))
		{   // Two numeric arguments
		    eject_ctlr = atoi(optarg);
		    eject_id = atoi(argv[optind++]);
		    if (argv[optind] && isnumber(argv[optind]))
		    {	// Three numeric arguments
			eject_lun = atoi(argv[optind++]);
		    }
		}
		else
		{   // One numeric argument
		    eject_id = atoi(optarg);
		}
	    }
	    else
	    {
		// One path argument.
		fsname = optarg;
	    }
	    break;

	case 'f':			// mediad -f
					// mediad -f -a
	    modifiers |= FOREGROUND;
	    debugging = true;
	    break;

#ifdef OBSOLETE
	case 'i':			// mediad -i -p dev dir
	    				// mediad -i -r dev
	    modifiers |= IMMEDIATE;
	    immediate = true;
	    break;
#endif

	case 'k':			// mediad -k
	    nverbs++;
	    what_to_do = KILL;
	    ok_modifiers = 0;
	    break;

	case 'l':			// mediad -l N
					// mediad -l N -a
	    loglevel = atoi(optarg);
	    // verb/adverb determination is made below.
	    break;

	case 'm':			// mediad -m device
	    nverbs++;
	    what_to_do = SHOW_MPOINT;
	    ok_modifiers = 0;
	    any_user_ok = true;
	    fsname = optarg;
	    break;

	case 'n':
	    // XXX Security fix late in IRIX 6.5 release.
	    Volume::setGlobalOptions("nosuid");
	    break;

#ifdef OBSOLETE
	case 'o':			// mediad -o options -p ...
	    modifiers |= OPTIONS;
	    mntopts = optarg;
	    break;

	case 'p':			// mediad [-i] [-o opts] -p dev dir
	    nverbs++;
	    what_to_do = ADD_DEV;
	    ok_modifiers = IMMEDIATE | OPTIONS;
	    if (optind != argc - 2)
		mediad_usage();
	    fsname = argv[optind++];
	    dir = argv[optind++];
	    break;
#endif

	case 'q':			// mediad -q device
	    nverbs++;
	    what_to_do = QUERY_DEV;
	    ok_modifiers = 0;
	    fsname = argv[optind++];
	    break;

#ifdef OBSOLETE
	case 'r':			// mediad [-i] -r device
	    nverbs++;
	    what_to_do = REMOVE_DEV;
	    ok_modifiers = IMMEDIATE;
	    fsname = optarg;
	    break;

	case 't':
	    modifiers |= TYPE;
	    type = optarg;
	    break;
#endif

	case 'u':		       // mediad [-l N] -u (eject first device)
	    nverbs++;
	    what_to_do = EJECT;
	    ok_modifiers = LOGLEVEL;
	    any_user_ok = true;
	    break;

	default:
	    mediad_usage();
	}

    //  loglevel hack.  "mediad -l N" means send a message to mediad
    //  setting its log level to N.  "mediad -l N -a" means start
    //  mediad with log level N.

    if (loglevel != -1)
    {   if (nverbs == 0)
	{   what_to_do = SET_LOG_LEVEL;
	    nverbs++;
	    ok_modifiers = 0;
	}
	else
	    modifiers |= LOGLEVEL;
    }

    // Sanity checks.  Crash and burn if two verbs specified or if
    // modifier that doesn't work with this verb specified or if
    // extra arguments specified.

    if (nverbs > 1)
	mediad_usage();
    if (modifiers & ~ok_modifiers)
	mediad_usage();
    if (optind < argc)
	mediad_usage();

    // Check whether this user can do that.

    if (!any_user_ok && !suser())
    {   Log::error("must be superuser");
	return 1;
    }

    // Don't allow -d without -f.

    if (dsdebug && !debugging)
	mediad_usage();

    switch (what_to_do)
    {
    case START_DAEMON:
	start_daemon(false, debugging, loglevel);
	break;

    case EJECT:
	do_eject(fsname, eject_ctlr, eject_id, eject_lun);
	break;

    case KILL:
	do_kill();
	break;

    case SHOW_MPOINT:
	do_show_mpoint(fsname);
	break;

    case ADD_DEV:
	do_add_dev(fsname, dir, mntopts, immediate);
	break;

    case QUERY_DEV:
	do_query_dev(fsname);
	break;

    case REMOVE_DEV:
	do_remove_dev(fsname, immediate);
	break;

    case SET_LOG_LEVEL:
	do_set_log_level(loglevel);
	break;

    default:
	assert(0);			// Can't get here.
    }
    return 0;
}

//  Eject argument parsing

static int
eject_main(int argc, char *argv[])
{
    int ctlr = -1, id = -1, lun = -1;
    char *fsname = NULL;

    switch (argc)
    {
    case 1:				// eject
	break;

    case 2:				// eject fsname
					// eject id
	if (isnumber(argv[1]))
	{   ctlr = 0;
	    id = atoi(argv[1]);
	    lun = 0;
	}
	else
	    fsname = argv[1];
	break;

    case 3:				// eject ctlr id
	if (!isnumber(argv[1]) || !isnumber(argv[2]))
	    eject_usage();
	ctlr = atoi(argv[1]);
	id = atoi(argv[2]);
	lun = 0;
	break;

    case 4:				// eject ctlr id lun
	if (!isnumber(argv[1]) || !isnumber(argv[2]) || !isnumber(argv[3]))
	    eject_usage();
	ctlr = atoi(argv[1]);
	id = atoi(argv[2]);
	lun = atoi(argv[3]);
	break;

    default:
	eject_usage();
    }
    return do_eject(fsname, ctlr, id, lun);
}

#ifdef MEDIAD_FROM_INETD

//  argument processing when mediad is invoked from inetd.
//  Only -a and -l N are allowed.

static int
mediad_inetd_main(int argc, char *argv[])
{
    Log::background();
    if (!suser())
    {   Log::error("must be launched by superuser");
	return 1;
    }

    // Argument checking

    int loglevel = -1;
    for (int opt; (opt = getopt(argc, argv, "al:")) != -1; )
	switch (opt)
	{
	case 'a':
	    // Ignore -a.
	    break;

	case 'l':
	    // Set loglevel 
	    if (isnumber(optarg))
		loglevel = atoi(optarg);
		inetd_usage();
	    
	default:
	    assert(0);			// Can't get here.
	}

    if (optind != argc)
	inetd_usage();

    do_default_reply(loglevel);

    return 0;
}

static void
inetd_usage()
{
    static bool been_here = false;
    if (!been_here)
    {
	been_here = true;
	Log::error("use: mediad [-a] [-l loglevel]");
    }

    // inetd usage messages are nonfatal, because the user may not look
    // in SYSLOG for a long time.  Just return.
}

#endif /* MEDIAD_FROM_INETD */

static void
eject_usage()
{
    fprintf(stderr, "use: eject\n");
    fprintf(stderr, "     eject device_name\n");
    fprintf(stderr, "     eject mount_point\n");
    fprintf(stderr, "     eject scsi_id (numeric)\n");
    fprintf(stderr, "     eject scsi_ctlr scsi_id [scsi_lun] (numeric)\n");
    fprintf(stderr, "\n");
    exit(1);
}

static void
mediad_usage()
{
     fprintf(stderr, "Usage:\n");
     fprintf(stderr, "     mediad\n");
     fprintf(stderr, "     mediad [-l level] [ -n ] "
	     "-e [controller_id] scsi_id [lun] | dir | device\n");
#ifdef OBSOLETE
     fprintf(stderr, "     mediad [-i] [-o options] -p device dir\n");
     fprintf(stderr, "     mediad [-i] -r device\n");
#endif
     fprintf(stderr, "     mediad -m dir\n");
     fprintf(stderr, "     mediad -q device\n");
     fprintf(stderr, "     mediad [-l loglevel] [-f] -a\n");
     fprintf(stderr, "     mediad -k\n");
     fprintf(stderr, "     mediad [-l level] -u\n");
     fprintf(stderr, "     mediad [-d] -f\n");
     fprintf(stderr, "\n");
     exit(1);
}

//////////////////////////////////////////////////////////////////////////////
// Actions.  main() or *_main() calls one of these routines after parsing
// argv.

#ifdef MEDIAD_FROM_INETD

static void
sigusr1_handler()
{
    sigusr1_seen = true;
}

static void
sigalrm_handler()
{ }

static void
do_default_reply(int loglevel)
{
    const int listenfd = 0;		// standard input; passed by inetd.
    int clientfd = accept(listenfd, (sockaddr *) &addr, sizeof addr);
    if (clientfd < 0)
    {	Log::perror("accept failed");
	exit(1);
    }

    //  Set up to receive SIGALRM and SIGUSR1.

    sigusr1_seen = false;
    signal(SIGALRM, (SIG_PF) sigalrm_handler);
    signal(SIGUSR1, (SIG_PF) sigusr1_handler);

    //  Create a MonitorClient which will immediately send a
    //  MCE_NO_DEVICES message, then destroy the MonitorClient,
    //  closing the socket.  (The curly brackets force the
    //  MonitorClient out of scope immediately, so it is destroyed.)

XXX this will not work any more because MonitorClient constructor args changed.
    { MonitorClient mc(clientfd, true); }

    //  Wait two seconds for a SIGUSR1 signal.
    //  XXX this should run the scheduler and accept/flush other clients.

    alarm(2);
    sigpause(0);
    alarm(0);

    // If we got SIGUSR1, start the daemon, otherwise return/exit.

    if (sigusr1_seen)
      	start_daemon(true, false, loglevel);
}

#endif /* MEDIAD_FROM_INETD */

static void
signal_handler()
{
    Scheduler::exit();
}

static void
start_daemon(bool started_by_inetd, bool debugging, int loglevel)
{
    if (test_mediad_present())
    {	Log::error("another mediad is already running.");
	exit(1);
    }

    if (debugging)
	Log::debug();
    else
    {   _daemonize(0, -1, -1, -1);
	Log::background();
    }
    if (loglevel != -1)
    {	if (loglevel >= LOG_DEBUG)
	    Log::debug();
	else if (loglevel >= LOG_INFO)
	    Log::info();
	else
	    Log::error();
    }

    //  Catch common signals.

    signal(SIGHUP, (SIG_PF) signal_handler);
    signal(SIGINT, (SIG_PF) signal_handler);
    signal(SIGTERM, (SIG_PF) signal_handler);
    signal(SIGPIPE, SIG_IGN);

    //  Instantiate a media daemon.

    MediaDaemon cosmo_mediad;		// Everything should be named Cosmo.

    if (Device::count() == 0)
    {	Log::info("No devices to monitor.  Exiting.");
	return;
    }

    //  Instantiate listeners.

    RPCListener left_ear(true, started_by_inetd);
    CompatListener right_ear(CompatListener::MEDIAD_SOCKNAME);

    //  The main loop

    Scheduler::loop();

    Log::debug("exiting");
}

static int
do_eject(const char *path, int ctlr, int id, int lun)
{
    CompatClient cc;
    cc.send_eject(path, ctlr, id, lun);
    int rcode = cc.status();
    if (rcode == RMED_ENOMEDIAD)
    {
	//  Couldn't send message to mediad.  Try to find and eject
	//  the device.

	rcode = do_eject_no_mediad(path, ctlr, id, lun);
    }
    if (rcode != RMED_NOERROR)
	Log::error(CompatClient::message(rcode));
    return rcode;
}

static int
do_eject_no_mediad(const char *path, int ctlr, int id, int lun)
{
    DeviceAddress da;
    Inventory hinv;

    if (path)
    {   
	//  Try to find device.

	VolumeAddress va(path);
	if (!va.valid())
	{
	    // Not a device.  Maybe it's a mount point.

	    Fsd mtab("/etc/mtab");
	    mntent *mnt = mtab.at_dir(path);
	    if (mnt)
		va = VolumeAddress(mnt->mnt_fsname);
	}

	// Extract device address from volume address.

	// XXX in future, should find all distinct device addresses
	// and eject them all.

	if (va.valid())
	    da = va.partition(0)->device();
    }
    else if (ctlr >= 0)
    {
	// eject by SCSI address

	da = DeviceAddress(ctlr, id, lun);
    }
    else
    {
	// eject first find -- take the first recognizable entry in the
	// H/W inventory.

	const inventory_s *invp = hinv[0];
	if (invp)
	    da = DeviceAddress(*invp);
    }
    if (!da.valid())
	return RMED_ENODEVICE;
    const inventory_s *invp = hinv.at(da);
    if (!invp)
	return RMED_ENODEVICE;

    // Check for filesystems mounted on this device.  Iterate through
    // /etc/mtab and compare each filesystem's device to this device.
    // Dismount any that match.

    Fsd mtab("/etc/mtab");
    const mntent *mnt;
    for (int k = 0; mnt = mtab[k]; k++)
    {
	VolumeAddress mva(mnt->mnt_fsname);
	if (mva.valid())
	{
	    const PartitionAddress *mpa;
	    for (int j = 0; mpa = mva.partition(j); j++)
	    {
		if (mpa->device() == da)
		{
		    const char *umount_argv[3] = {
			"/etc/umount", mnt->mnt_dir, NULL
			};
		    int status = execute(umount_argv);
		    if (status != 0)
			return RMED_ECANTUMOUNT;
		    break;
		    // Continue search with next mtab entry.
		}
	    }
	}
    }    

    // We have a device and an inventory record.  Probe for the device
    // and eject it.  This code duplicates MediaDaemon::start_monitor()
    // and I should only have one copy of it.

    DeviceInfo info(da, *invp);
    SCSIAddress *sa = da.as_SCSIAddress();
    if (sa)
    {
	char inquiry[98];
	bzero(inquiry, sizeof inquiry);
	DSReq dsp(*sa);

	if (dsp && inquiry12(dsp, inquiry, sizeof inquiry, 0) == STA_GOOD)
	    info.inquiry(inquiry);
    }
    DeviceLibrary devlib("/usr/lib/devicelib");
    DeviceDSO *dso;
    for (unsigned i = 0; dso = devlib[i]; i++)
    {	info.dso(dso);
	Device *device = dso->instantiate(info);
	int err = RMED_NOERROR;
	if (device) {
            err = device->eject();
            dso->deinstantiate(device);
            return err;
	}
    }

    return RMED_ENODEVICE;
}

static int
execute(const char *argv[3])
{
    int status = 0;
#define STR(p) ((p) ? (p) : "")

    Log::debug("executing command %s %s",
	       STR(argv[0]), STR(argv[1]));
    assert(argv[0][0] == '/');

    int pid = fork();
    if (pid < 0)
    {	Log::perror("fork failed");
	return -1;
    }
    if (pid == 0)
    {
	// Child

	execv(argv[0], (char *const *) argv);
	Log::perror("%s exec failed", argv[0]);
	_exit(1);
    }
    else
    {
	// Parent

	do
	{
	    if (waitpid(pid, &status, NULL) != pid)
	    {   Log::perror("%s wait failed", argv[0]);
		return -1;
	    }
	} while (!WIFEXITED(status));
	if (status != 0)
	    Log::debug("command failed with status %d", status);
    }
    return status;
}
static void
do_kill()
{
    CompatClient cc;
    cc.send_kill();
}

static void
do_show_mpoint(const char *fsname)
{
    CompatClient cc;
    cc.send_show_mpoint(fsname);
    if (cc.status() == RMED_NOERROR)
	     printf("%s\n", cc.reply().mpoint);
    else if (cc.status() == RMED_ENOMEDIAD) 
    {
	//  No mediad.  We'll see if we can find anything in /etc/fsd.auto.

	const Fsd& fsd_auto = Fsd::fsd_auto();
	const mntent *mntp;
	for (unsigned i = 0; mntp = fsd_auto[i]; i++)
	    if (!strcmp(fsname, mntp->mnt_fsname))
		printf("%s\n", mntp->mnt_dir);
    }
}

static void
do_query_dev(const char * /* fsname */ )
{
    int write_me = 0; assert(write_me);
}

static char *
munge_options(const char *oldopts, const char *newopts, bool mon)
{
    char optbuffer[MNTMAXSTR];

    // Build the

    if (newopts)
	strcpy(optbuffer, newopts);
    else if (oldopts)
    {
	mntent mnt = { NULL, NULL, NULL, (char *) oldopts, 0, 0 };
	const char *oldmon = hasmntopt(&mnt, "mon");
	if (oldmon)
	{   
	    if (oldmon != oldopts)
	    {   strcpy(optbuffer, oldopts);
		optbuffer[oldmon - oldopts - 1] = '\0'; // truncate at comma
	    }
	    else
		optbuffer[0] = '\0';
	    const char *p = strchr(oldmon, ',');
	    if (p)
		strcat(optbuffer, p);
	}
	else
	    strcpy(optbuffer, oldopts);
    }
    else
	optbuffer[0] = '\0';

    // Append "mon=on/off".

    if (optbuffer[0])
	strcat(optbuffer, ",");
    strcat(optbuffer, mon ? "mon=on" : "mon=off");

    return optbuffer;
}

static void
do_add_dev(const char *fsname,
	   const char *dir,
	   const char *mntopts,
	   bool immediate)
{
    Fsd& fsd_auto = Fsd::fsd_auto();
    const mntent *mnt;
    bool found = false;
    for (unsigned i = 0; !found && (mnt = fsd_auto[i]); i++)
    {	assert(mnt->mnt_fsname);
	if (!strcmp(mnt->mnt_fsname, fsname) &&
	    !strcmp(mnt->mnt_type, "mediad"))
	{   char *newopts = munge_options(mnt->mnt_opts, mntopts, true);
	    const mntent newmnt = {
		(char *) fsname,
		(char *) dir,
		"mediad",
		newopts, 0, 0
		};
	    fsd_auto.replace(i, &newmnt);
	    found = true;
	}
    }
    if (!found)
    {   mntent newmnt = {
	    (char *) fsname,
	    (char *) dir,
	    "mediad",
	    "mon=on",
	    0, 0
	};
	fsd_auto.append(&newmnt);
    }

    //  Inform mediad that it should monitor the new device.

    if (immediate)
    {   CompatClient cc;
	cc.send_start_entry(fsname);
	if (cc.status() == RMED_ENOMEDIAD)
	{
	    int write_me = 0; assert(write_me);
	    // Start mediad.
	}
    }
}

static void
do_remove_dev(const char *fsname, bool immediate)
{
    Fsd& fsd_auto = Fsd::fsd_auto();
    const mntent *mnt;
    bool found = false;
    for (unsigned i = 0; !found && (mnt = fsd_auto[i]); i++)
    {	assert(mnt->mnt_fsname);
	if (!strcmp(mnt->mnt_fsname, fsname) &&
	    !strcmp(mnt->mnt_type, "mediad"))
	{   char *newopts = munge_options(mnt->mnt_opts, NULL, false);
	    const mntent newmnt = {
		(char *) fsname,
		mnt->mnt_dir,
		"mediad",
		newopts, 0, 0
		};
	    fsd_auto.replace(i, &newmnt);
	    found = true;
	}
    }
    if (!found)
    {   mntent newmnt = {
	    (char *) fsname,
	    ".",
	    "mediad",
	    "mon=off",
	    0, 0
	};
	fsd_auto.append(&newmnt);
    }

    //  Inform mediad that it should monitor the new device.

    if (immediate)
    {   CompatClient cc;
	cc.send_stop_entry(fsname);
    }
}

static void
do_set_log_level(int level)
{
    CompatClient cc;
    cc.send_set_log_level(level);
}
