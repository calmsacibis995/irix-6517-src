#include "MxClient.h"

#include <assert.h>

#include "DirEntry.h"
#include "Directory.h"
#include "Event.h"
#include "File.h"
#include "Log.h"

MxClient::~MxClient()
{
    while (requests.size())
	cancel(requests.first());	// Destroy all interests.
}

//  These functions exist so that Acknowledge events posted from the
//  destructor will be ignored.

void
MxClient::dequeue_from_scan(Interest *)
{ }

void
MxClient::post_event(const Event&, Request, const char *)
{ }

//  MxClient::interest() maps a request number to a ClientInterest ptr.

ClientInterest *
MxClient::interest(Request r)
{
    ClientInterest *ip = requests.find(r);
    if (!ip)
	Log::error("%s invalid request number %d", name(), r);
    return ip;
}

Boolean
MxClient::check_new(Request request, const char *path)
{
    if (requests.find(request))
    {   Log::error("%s nonunique request number %d rejected", name(), request);
	return false;
    }

    if (path[0] != '/')
    {   Log::info("relative path \"%s\" rejected", path);
	post_event(Event::Acknowledge, request, path);
	return false;
    }

    return true;
}

void
MxClient::monitor_file(Request request, const char *path, const Cred& cred)
{
    if (check_new(request, path))
    {
	ClientInterest *ip = new File(path, this, request, cred);
	requests.insert(request, ip);
    }
}

void
MxClient::monitor_dir(Request request, const char *path, const Cred& cred)
{
    static Boolean been_here = false;
    if (!been_here)
    {
	been_here = true;
	Log::info("sizeof        (Boolean) = %d", sizeof (Boolean));
	Log::info("sizeof       (Interest) = %d", sizeof (Interest));
	Log::info("sizeof (ClientInterest) = %d", sizeof (ClientInterest));
	Log::info("sizeof           (File) = %d", sizeof (File));
	Log::info("sizeof      (Directory) = %d", sizeof (Directory));
	Log::info("sizeof       (DirEntry) = %d", sizeof (DirEntry));
    }

    if (check_new(request, path))
    {
	ClientInterest *ip = new Directory(path, this, request, cred);
	requests.insert(request, ip);
    }
}

void
MxClient::suspend(Request r)
{
    ClientInterest *ip = interest(r);
    if (ip)
	ip->suspend();
}

void
MxClient::resume(Request r)
{
    ClientInterest *ip = interest(r);
    if (ip)
	ip->resume();
}

void
MxClient::cancel(Request r)
{
    ClientInterest *ip = interest(r);
    requests.remove(r);
    delete ip;
}
