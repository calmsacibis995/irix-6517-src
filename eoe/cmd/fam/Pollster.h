#ifndef Pollster_included
#define Pollster_included

#include <sys/time.h>

#include "Boolean.h"
#include "Set.h"

class Interest;
class ServerHost;

//  The Pollster remembers what needs to be polled, and wakes up every
//  few seconds to poll it.  The Pollster polls Interests and Hosts.
//  Each Host, if it can't connect to remote fam, polls its own Interests.
//
//  The Pollster's polling interval can be set or interrogated.
//  Also, remote polling can be enabled or disabled using enable()
//  or disable() (corresponds to "fam -l").
//
//  When there's nothing to poll, the Pollster turns itself off, so
//  fam can sleep for a long time.
//
//  Pollster is not instantiated; instead, a bunch of static methods
//  implement its interface.

class Pollster {

public:

    enum { DEFAULT_INTERVAL = 6 };

    static void watch(Interest *);
    static void forget(Interest *);

    static void watch(ServerHost *);
    static void forget(ServerHost *);

    static void interval(unsigned secs)	{ pintvl.tv_sec = secs; }
    static unsigned interval()		{ return pintvl.tv_sec; }
    static void enable()		{ remote_polling_enabled = true; }
    static void disable()		{ remote_polling_enabled = false; }

private:

    // Class Variables

    static Boolean remote_polling_enabled;
    static timeval pintvl;		// polling interval
    static Set<Interest *> polled_interests;
    static Set<ServerHost *> polled_hosts;
    static Boolean polling;

    // Private Class Methods

    static void polling_task(void *closure);
    static void start_polling();
    static void stop_polling();

    Pollster();				// Do not instantiate.

};

#endif /* !Pollster_included */
