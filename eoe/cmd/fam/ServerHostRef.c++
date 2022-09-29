#include "ServerHostRef.h"

#include <assert.h>
#include <netdb.h>

unsigned int		   ServerHostRef::count;
StringTable<ServerHost *> *ServerHostRef::hosts_by_name;

///////////////////////////////////////////////////////////////////////////////
//  ServerHostRef methods simply maintain refcounts on ServerHosts.

ServerHostRef&
ServerHostRef::operator = (const ServerHostRef& that)
{
    if (this != &that)
    {
	if (p && !--p->refcount)
	    delete p;
	p = that.p;
	if (p)
	    p->refcount++;
    }
    return *this;
}

ServerHostRef::~ServerHostRef()
{
    if (!--p->refcount)
    {
	// Remove from hosts_by_name.

	const char *name;
	for (unsigned i = 0; name = hosts_by_name->key(i); )
	    if (hosts_by_name->value(i) == p)
		hosts_by_name->remove(name);
	    else
		i++;

	delete p;
    }

    // When the last reference is gone, delete hosts_by_name.

    if (!--count && hosts_by_name)
    {	assert(!hosts_by_name->size());
	delete hosts_by_name;
	hosts_by_name = NULL;
    }
}

ServerHost *
ServerHostRef::find(const char *name)
{
    //  Create hosts_by_name first time.

    if (!hosts_by_name)
	hosts_by_name = new StringTable<ServerHost *>;

    //  Look in list of existing hosts.

    ServerHost *host = hosts_by_name->find(name);
    if (host)
	return host;

    //  Look up hostname via gethostbyname().

    hostent *hp = gethostbyname(name);
    char *fake_haliases[1];
    in_addr fake_haddr;
    in_addr *fake_haddrlist[2];
    hostent fake_hostent;

    if (!hp)
    {
	//  gethostbyname() failed for some reason (e.g., ypbind
	//  is down or NIS map just changed, ethernet came unplugged,
	//  or something equally tragic).
	//
	//  Fake it by building a hostent with an illegal address.

	fake_haliases[0] = NULL;
	fake_haddr.s_addr = 0;
	fake_haddrlist[0] = &fake_haddr;
	fake_haddrlist[1] = NULL;
	fake_hostent.h_name = (char *) name;
	fake_hostent.h_aliases = fake_haliases;
	fake_hostent.h_addrtype = 0;
	fake_hostent.h_length = sizeof fake_haddr;
	fake_hostent.h_addr_list = (char **) fake_haddrlist;
	hp = &fake_hostent;
    }

    assert(hp->h_length == sizeof (struct in_addr));

    //  Create new host.

    host = new ServerHost(*hp);

    //  Add this host to hosts_by_name.

    hosts_by_name->insert(name, host);
    hosts_by_name->insert(hp->h_name, host);
    for (char *const*p = hp->h_aliases; *p; p++)
	hosts_by_name->insert(*p, host);

    return host;
}
