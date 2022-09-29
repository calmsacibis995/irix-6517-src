#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/syssgi.h>
#include <unistd.h>

#include "Activity.h"
#include "Listener.h"
#include "Log.h"
#include "Pollster.h"
#include "Scheduler.h"

const char *program_name;

//
//  issock() -- Function ripped off from rpc.mountd.c.  Returns true if
//  file descriptor is a socket.
//

static Boolean issock(int fd)
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

static const char *basename(const char *p)
{
    const char *tail = p;
    while (*p)
	if (*p++ == '/' && *p && *p != '/')
	    tail = p;
    return tail;
}

void usage()
{   fprintf(stderr,
	    "Use: %s [ -f | -d | -v ] [ -l | -t seconds ] [ -T seconds ] \\\n"
	    "\t\t\t\t[ -p prog.vers ]\n",
	    program_name);
    fprintf(stderr, "\t-f\t\tstay in foreground\n");
    fprintf(stderr, "\t-d\t\tdebug\n");
    fprintf(stderr, "\t-v\t\tverbose\n");
    fprintf(stderr, "\t-l\t\tno polling\n");
    fprintf(stderr, "\t-t seconds\tset polling interval (default 3 sec)\n");
    fprintf(stderr, "\t-T seconds\tset inactive timeout (default 5 sec)\n");
    fprintf(stderr, "\t-p prog.vers\tset RPC program number and version\n");
    fprintf(stderr, "\n");
    exit(1);
}

int main(int argc, char *argv[])
{
    // This keeps down nfs mounts from hanging fam.  System calls on
    // down nfs mounts fail with errno == ETIMEDOUT.  We don't do this
    // if we're running diskless because we don't want fam to crash
    // because a page fault timed out.
    char buf[20];
    if (sgikopt("diskless", buf, sizeof buf) == 0
	&& strcmp(buf, "0") == 0) {
	syssgi(SGI_NOHANG, 1);
    }

    Boolean debugging = false;
    Boolean started_by_inetd = issock(0); 
    unsigned long program = Listener::FAMPROG, version = Listener::FAMVERS;

    program_name = basename(argv[0]);
    Log::name(program_name);

    if (!started_by_inetd)
	Log::foreground();

    for (int i = 1; i < argc; i++)
    {   if (argv[i][0] != '-' || !argv[i][1] || argv[i][2])
	    usage();
	switch (argv[i][1])
	{
	    char *p;
	    unsigned secs;

	case 'f':
	    debugging = true;
	    break;

	case 'd':
	    Log::debug();
	    debugging = true;
	    break;

	case 'v':
	    Log::info();
	    debugging = true;
	    break;

	case 'l':
	    Pollster::disable();
	    break;

	case 'p':
	    if (++i >= argc)
		usage();
	    p = strchr(argv[i], '.');
	    if (p)
	    {	*p++ = '\0';
		version = atoi(p);
	    }
	    program = atoi(argv[i]);
	    break;

	case 't':
	    if (i + 1 >= argc)
		usage();
	    secs = strtoul(argv[++i], &p, 10);
	    if (*p)
		usage();
	    if (secs == 0)
		Log::error("illegal poll interval 0");
	    else
		Pollster::interval(secs);
	    break;

	case 'T':
	    if (i + 1 >= argc)
		usage();
	    secs = strtoul(argv[++i], &p, 10);
	    if (*p)
		usage();
	    Activity::timeout(secs);
	    break;

	default:
	    usage();
	}
    }
    if (!started_by_inetd)
    {
	if (getuid() != 0)
	{   Log::error("must be superuser");
	    exit(1);
	}
	if (!debugging)
	{   _daemonize(0, -1, -1, -1);
	    Log::background();
	}
    }
    (void) signal(SIGPIPE, SIG_IGN);

    // Ignore SIGCHLD because we run nfsunhang to unhang down nfs
    // mounts, and we don't care about the exit status of nfsunhang
    // (since we poll anyway) and we don't want to create zombies.
    (void) signal(SIGCHLD, SIG_IGN);
    Listener the_big_ear(started_by_inetd, program, version);
    Scheduler::loop();
    return 0;
}
