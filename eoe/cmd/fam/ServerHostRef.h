#ifndef ServerHostRef_included
#define ServerHostRef_included

#include "ServerHost.h"
#include "StringTable.h"

//  ServerHostRef is a reference-counted pointer to a ServerHost.
//
//  A ServerHostRef is created by specifying the name of a
//  host.  The pointed-to ServerHost will be shared with other
//  Refs to the same host, even under different names.

class ServerHostRef {

public:

    ServerHostRef()			: p(0) { count++; }
    ServerHostRef(const char *name)	: p(find(name))
					{ p->refcount++; count++; }
    ServerHostRef(const ServerHostRef& that)
					: p(that.p)
					{ if (p) p->refcount++; count++; }
    ServerHostRef& operator = (const ServerHostRef&);
    ~ServerHostRef();

    ServerHost *operator -> ()		{ assert(p); return p; }
    const ServerHost *operator -> () const { assert(p); return p; }

private:

    // Instance Variable

    ServerHost *p;

    //  Class Variables

    static unsigned count;		// number of handles extant
    static StringTable<ServerHost *> *hosts_by_name;

    //  Private Instance Methods

    ServerHostRef(ServerHost& that)	: p(&that) { p->refcount++; count++; }

    //  Private Class Methods

    static ServerHost *find(const char *name);

};

#endif /* !ServerHost_included */
