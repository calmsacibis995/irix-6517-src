#ifndef ServerHost_included
#define ServerHost_included

#include "Bag.h"
#include "Boolean.h"
#include "ClientInterest.h"
#include "RequestMap.h"
#include "RPC_TCP_Connector.h"
#include "SlidingArray.h"
struct hostent;
class ServerConnection;

//  ServerHost represents an NFS-server host.  A ServerHost tries to
//  talk to remote fam.  If it succeeds, it relies on remote fam to
//  monitor files that reside on that host.  If it can't talk to fam,
//  it polls the files of interest.
//
//  send_monitor() et al are used to tell the ServerHost what files
//  are being monitored.  If we have a connection to a remote fam,
//  we send the appropriate message.
//
//  When a file is monitored, a request number is allocated for it,
//  and it is added to a RequestMap.  When it's cancelled, it's
//  removed from the RequestMap.
//
//  A ServerHost is "active" if we're monitoring at least one file on
//  that host.  The ServerHost starts trying to connect to remote fam
//  when it becomes active.  When it becomes inactive, it starts a
//  timer; if it stays inactive for three seconds, it disconnects from
//  remote fam.
//
//  So a ServerHost can be "active" but not "connected": there is no
//  remote fam or its connection hasn't come up yet.  A ServerHost can
//  also be "connected" but not "active": its connection is timing
//  out.
//
//  Unfortunately, the kernel's NFS attribute cache gets in fam's way.
//  If we scan a file within n seconds of someone else scanning the
//  same file (n defaults to 3), then we get stale information from
//  the cache.  So we scan the file immediately, just in case the
//  cache won't bite us, then we put the file into a queue of files to
//  rescan n seconds later, the DeferredScanQueue.

class ServerHost {

public:

    Boolean is_connected() const	{ return connection != 0; }
    const char *name() const		{ return myname; }

    Request send_monitor(ClientInterest *, ClientInterest::Type,
			 const char *remote_path);
    void send_cancel(Request);
    void send_suspend(Request);
    void send_resume(Request);

    void poll();

private:

    //  A DeferredScan is the info needed to rescan an Interest after
    //  several seconds' delay.  The delay is needed to let the NFS
    //  attribute cache go stale.  A DeferredScanCohort is a group of
    //  deferred scans that will be ready at the same time.  A
    //  DeferredScanQueue is an array of cohorts, indexed by absolute
    //  time (in seconds).

    class DeferredScan {

    public:

	inline DeferredScan(Request = 0, const char * = 0);
	inline DeferredScan(const DeferredScan& that);

	DeferredScan& operator = (const DeferredScan& that);
	operator int ()			{ return myrequest != 0; }

	Request request() const		{ return myrequest; }
	const char *path() const	{ return mypath[0] ? mypath : NULL; }

    private:

	Request myrequest;
	char mypath[MAXNAMELEN];

    };
    typedef Bag<DeferredScan> DeferredScanCohort;
    typedef SlidingArray<DeferredScanCohort> DeferredScanQueue;

    //  Instance Variables

    unsigned refcount;
    char *myname;
    RPC_TCP_Connector connector;
    ServerConnection *connection;
    Request unique_request;
    RequestMap requests;
    DeferredScanQueue deferrals;
    int min_time;

    //  Private Instance Methods

    ServerHost(const hostent&);
    ~ServerHost();
    Boolean active()			{ return requests.size() != 0; }
    void activate();
    void deactivate();
    void defer_scan(int timeout, Request, const char *);

    //  Class Methods

    static void connect_handler(int fd, void *);
    static void disconnect_handler(void *);

    static void event_handler(const Event&, Request, const char *, void *);
    static void deferred_scan_task(void *);
    static void timeout_task(void *);

    ServerHost(const ServerHost&);	// Do not copy
    operator = (const ServerHost&);	//  or assign.

friend class ServerHostRef;

};

#endif /* !ServerHost_included */
